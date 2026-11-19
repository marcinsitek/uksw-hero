package storage

import (
	"context"
	"crypto/rand"
	"encoding/json"
	"errors"
	"io"
	"log/slog"
	"os"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	domain "arena/server/session"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
)

// TEST_DATABASE_URL must point to a test database where schemas can be created.
// Each test uses its own schema and never modifies the application's tables.
func postgresTestPool(t *testing.T) *pgxpool.Pool {
	t.Helper()
	url := os.Getenv("TEST_DATABASE_URL")
	if url == "" {
		t.Skip("set TEST_DATABASE_URL to run PostgreSQL integration tests")
	}
	admin, err := OpenDatabase(context.Background(), url)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(admin.Close)
	schema := "test_" + strings.ToLower(rand.Text())
	identifier := pgx.Identifier{schema}.Sanitize()
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	if _, err := admin.Exec(ctx, "CREATE SCHEMA "+identifier); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() {
		ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		defer cancel()
		if _, err := admin.Exec(ctx, "DROP SCHEMA "+identifier+" CASCADE"); err != nil {
			t.Error(err)
		}
	})
	config := admin.Config()
	config.ConnConfig.RuntimeParams["search_path"] = schema
	pool, err := pgxpool.NewWithConfig(ctx, config)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(pool.Close)
	schemaSQL, err := os.ReadFile("../../db/init.sql")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := pool.Exec(ctx, string(schemaSQL)); err != nil {
		t.Fatal(err)
	}
	return pool
}

func postgresTestService(pool *pgxpool.Pool) *domain.SessionService {
	return domain.NewSessionService(NewPostgresStore(pool), slog.New(slog.NewTextHandler(io.Discard, nil)))
}

func TestPostgresSessionAndEventPersistence(t *testing.T) {
	pool := postgresTestPool(t)
	ctx := context.Background()
	service := postgresTestService(pool)
	session, err := service.Create(ctx, "Student", "")
	if err != nil {
		t.Fatal(err)
	}
	store := NewPostgresStore(pool)
	if err := store.Create(ctx, session); !errors.Is(err, domain.ErrDuplicateID) {
		t.Fatalf("duplicate create: %v", err)
	}
	for _, data := range []json.RawMessage{nil, json.RawMessage(`null`), json.RawMessage(`{"enemy":"goblin"}`)} {
		_, err = service.RecordEvent(ctx, domain.Event{SessionID: session.ID, Type: "goblin_killed", Data: data})
		if err != nil {
			t.Fatal(err)
		}
	}
	var count int
	if err := pool.QueryRow(ctx, "SELECT COUNT(*) FROM event WHERE session_id = $1", session.ID).Scan(&count); err != nil || count != 3 {
		t.Fatalf("event count = %d, error = %v", count, err)
	}
	var payload string
	if err := pool.QueryRow(ctx, `SELECT data->>'enemy' FROM event WHERE session_id = $1 AND data ? 'enemy'`, session.ID).Scan(&payload); err != nil || payload != "goblin" {
		t.Fatalf("payload = %q, error = %v", payload, err)
	}
	// A separate pool must load the persisted state without any in-memory session.
	reopened, err := pgxpool.NewWithConfig(ctx, pool.Config())
	if err != nil {
		t.Fatal(err)
	}
	defer reopened.Close()
	loaded, err := NewPostgresStore(reopened).Update(ctx, session.ID, func(*domain.Session) error { return nil })
	if err != nil || loaded.GoblinsKilled != 3 || loaded.Health != 100 || loaded.DeviceInfo != "" || loaded.PlayerName != "Student" || loaded.StartedAt.IsZero() {
		t.Fatalf("persisted session = %+v, error = %v", loaded, err)
	}
	ended, err := service.RecordEvent(ctx, domain.Event{SessionID: session.ID, Type: "player_died"})
	if err != nil || ended.EndedAt == nil || ended.Health != 100 || ended.GoblinsKilled != 3 {
		t.Fatalf("death = %+v, error = %v", ended, err)
	}
	repeated, err := postgresTestService(reopened).End(ctx, session.ID)
	if err != nil || repeated.EndedAt == nil || repeated.EndedAt.Before(*ended.EndedAt) || repeated.Health != 100 || repeated.GoblinsKilled != 3 {
		t.Fatalf("repeated end = %+v, error = %v", repeated, err)
	}
	updated, err := service.RecordEvent(ctx, domain.Event{SessionID: session.ID, Type: "goblin_killed"})
	if err != nil || updated.GoblinsKilled != 4 || updated.Health != 100 || updated.EndedAt == nil || !updated.EndedAt.Equal(*repeated.EndedAt) {
		t.Fatalf("event after end: session=%+v, error=%v", updated, err)
	}
	_, err = service.RecordEvent(ctx, domain.Event{SessionID: "missing", Type: "goblin_killed"})
	if !errors.Is(err, domain.ErrNotFound) {
		t.Fatalf("missing session: %v", err)
	}
	if err := pool.QueryRow(ctx, "SELECT COUNT(*) FROM event WHERE session_id = $1", session.ID).Scan(&count); err != nil || count != 5 {
		t.Fatalf("expected five accepted events and no rows from missing session or explicit end: count=%d, error=%v", count, err)
	}
}

func TestPostgresEventInsertFailureRollsBackSession(t *testing.T) {
	pool := postgresTestPool(t)
	ctx := context.Background()
	service := postgresTestService(pool)
	session, err := service.Create(ctx, "Student", "Linux")
	if err != nil {
		t.Fatal(err)
	}
	// Force an INSERT failure after the session UPDATE has executed.
	if _, err := pool.Exec(ctx, `ALTER TABLE event ADD CONSTRAINT reject_events CHECK (event_type = 'rejected')`); err != nil {
		t.Fatal(err)
	}
	_, err = service.RecordEvent(ctx, domain.Event{SessionID: session.ID, Type: "goblin_killed"})
	if err == nil {
		t.Fatal("event insert should fail")
	}
	var goblinsKilled, count int
	if err := pool.QueryRow(ctx, "SELECT goblins_killed FROM session WHERE id = $1", session.ID).Scan(&goblinsKilled); err != nil || goblinsKilled != 0 {
		t.Fatalf("session was not rolled back: goblins_killed=%d, error=%v", goblinsKilled, err)
	}
	if err := pool.QueryRow(ctx, "SELECT COUNT(*) FROM event").Scan(&count); err != nil || count != 0 {
		t.Fatalf("failed event persisted: count=%d, error=%v", count, err)
	}
}

func TestPostgresConcurrentEvents(t *testing.T) {
	pool := postgresTestPool(t)
	ctx := context.Background()
	service := postgresTestService(pool)
	session, err := service.Create(ctx, "Student", "Linux")
	if err != nil {
		t.Fatal(err)
	}
	var wg sync.WaitGroup
	for range 30 {
		wg.Go(func() {
			if _, err := service.RecordEvent(ctx, domain.Event{SessionID: session.ID, Type: "goblin_killed"}); err != nil {
				t.Errorf("record event: %v", err)
			}
		})
	}
	wg.Wait()
	final, err := service.End(ctx, session.ID)
	if err != nil || final.GoblinsKilled != 30 {
		t.Fatalf("lost concurrent updates: session=%+v, error=%v", final, err)
	}
	var count int
	if err := pool.QueryRow(ctx, "SELECT COUNT(*) FROM event WHERE session_id = $1", session.ID).Scan(&count); err != nil || count != 30 {
		t.Fatalf("event count=%d, error=%v", count, err)
	}
}

func TestPostgresConcurrentEventsAndEnd(t *testing.T) {
	pool := postgresTestPool(t)
	ctx := context.Background()
	service := postgresTestService(pool)
	session, err := service.Create(ctx, "Student", "Linux")
	if err != nil {
		t.Fatal(err)
	}
	var accepted atomic.Int64
	var wg sync.WaitGroup
	start := make(chan struct{})
	for range 30 {
		wg.Go(func() {
			<-start
			_, err := service.RecordEvent(ctx, domain.Event{SessionID: session.ID, Type: "goblin_killed"})
			if err == nil {
				accepted.Add(1)
			} else {
				t.Errorf("record event: %v", err)
			}
		})
	}
	wg.Go(func() {
		<-start
		if _, err := service.End(ctx, session.ID); err != nil {
			t.Errorf("end session: %v", err)
		}
	})
	close(start)
	wg.Wait()
	final, err := service.End(ctx, session.ID)
	if err != nil || accepted.Load() != 30 || final.GoblinsKilled != 30 || final.EndedAt == nil || final.Health != 100 {
		t.Fatalf("final session = %+v, accepted=%d, error=%v", final, accepted.Load(), err)
	}
	var count int64
	if err := pool.QueryRow(ctx, "SELECT COUNT(*) FROM event WHERE session_id = $1", session.ID).Scan(&count); err != nil || count != accepted.Load() {
		t.Fatalf("event count=%d, accepted=%d, error=%v", count, accepted.Load(), err)
	}
}
