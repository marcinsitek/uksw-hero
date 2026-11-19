package storage

import (
	"context"
	"errors"
	"fmt"

	domain "arena/server/session"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
	"github.com/jackc/pgx/v5/pgxpool"
)

type PostgresStore struct {
	pool *pgxpool.Pool
}

var _ domain.SessionStore = (*PostgresStore)(nil)

func NewPostgresStore(pool *pgxpool.Pool) *PostgresStore {
	return &PostgresStore{pool: pool}
}

func (s *PostgresStore) Create(ctx context.Context, session domain.Session) error {
	ctx, cancel := context.WithTimeout(ctx, databaseTimeout)
	defer cancel()
	_, err := s.pool.Exec(ctx, `
		INSERT INTO session (id, player_name, device_info, started_at, ended_at, health, goblins_killed)
		VALUES ($1, $2, NULLIF($3, ''), $4, $5, $6, $7)`,
		session.ID, session.PlayerName, session.DeviceInfo, session.StartedAt,
		session.EndedAt, session.Health, session.GoblinsKilled)
	var pgErr *pgconn.PgError
	if errors.As(err, &pgErr) && pgErr.Code == "23505" {
		return domain.ErrDuplicateID
	}
	if err != nil {
		return fmt.Errorf("insert session: %w", err)
	}
	return nil
}

func (s *PostgresStore) Update(ctx context.Context, id string, change func(*domain.Session) error) (domain.Session, error) {
	return s.update(ctx, id, nil, change)
}

func (s *PostgresStore) UpdateWithEvent(ctx context.Context, event domain.Event, change func(*domain.Session) error) (domain.Session, error) {
	return s.update(ctx, event.SessionID, &event, change)
}

func (s *PostgresStore) update(ctx context.Context, id string, event *domain.Event, change func(*domain.Session) error) (domain.Session, error) {
	ctx, cancel := context.WithTimeout(ctx, databaseTimeout)
	defer cancel()
	tx, err := s.pool.BeginTx(ctx, pgx.TxOptions{IsoLevel: pgx.ReadCommitted})
	if err != nil {
		return domain.Session{}, fmt.Errorf("begin session update: %w", err)
	}
	defer func() {
		// Rollback still needs a live context after a canceled request or timeout.
		rollbackCtx, rollbackCancel := context.WithTimeout(context.Background(), databaseTimeout)
		defer rollbackCancel()
		_ = tx.Rollback(rollbackCtx)
	}()

	var session domain.Session
	err = tx.QueryRow(ctx, `
		SELECT id, player_name, COALESCE(device_info, ''), started_at, ended_at, health, goblins_killed
		FROM session WHERE id = $1 FOR UPDATE`, id).Scan(
		&session.ID, &session.PlayerName, &session.DeviceInfo, &session.StartedAt,
		&session.EndedAt, &session.Health, &session.GoblinsKilled)
	if errors.Is(err, pgx.ErrNoRows) {
		return domain.Session{}, domain.ErrNotFound
	}
	if err != nil {
		return domain.Session{}, fmt.Errorf("load session: %w", err)
	}
	session.StartedAt = session.StartedAt.UTC()
	if session.EndedAt != nil {
		endedAt := session.EndedAt.UTC()
		session.EndedAt = &endedAt
	}
	if err := change(&session); err != nil {
		return domain.Session{}, err
	}
	_, err = tx.Exec(ctx, `
		UPDATE session SET health = $2, goblins_killed = $3, ended_at = $4 WHERE id = $1`,
		id, session.Health, session.GoblinsKilled, session.EndedAt)
	if err != nil {
		return domain.Session{}, fmt.Errorf("update session: %w", err)
	}
	if event != nil {
		var data any
		if len(event.Data) != 0 {
			data = string(event.Data)
		}
		_, err = tx.Exec(ctx, `
			INSERT INTO event (session_id, event_type, data) VALUES ($1, $2, $3::jsonb)`,
			id, event.Type, data)
		if err != nil {
			return domain.Session{}, fmt.Errorf("insert event: %w", err)
		}
	}
	if err := tx.Commit(ctx); err != nil {
		return domain.Session{}, fmt.Errorf("commit session update: %w", err)
	}
	return session, nil
}
