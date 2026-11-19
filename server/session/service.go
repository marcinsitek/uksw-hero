package session

import (
	"context"
	"crypto/rand"
	"log/slog"
	"time"
)

type SessionService struct {
	store  SessionStore
	logger *slog.Logger
}

func NewSessionService(store SessionStore, logger *slog.Logger) *SessionService {
	return &SessionService{store: store, logger: logger}
}

func (s *SessionService) Create(ctx context.Context, name, device string) (Session, error) {
	session := Session{
		ID:            rand.Text(),
		PlayerName:    name,
		DeviceInfo:    device,
		StartedAt:     time.Now().UTC().Truncate(time.Microsecond),
		Health:        100,
		GoblinsKilled: 0,
	}
	if err := s.store.Create(ctx, session); err != nil {
		return Session{}, err
	}
	return session, nil
}

func (s *SessionService) RecordEvent(ctx context.Context, event Event) (Session, error) {
	switch event.Type {
	case "player_attacked", "goblin_killed", "player_died":
	default:
		return Session{}, ErrInvalidEvent
	}
	session, err := s.store.UpdateWithEvent(ctx, event, func(session *Session) error {
		switch event.Type {
		case "player_attacked":
			session.Health = max(0, session.Health-10)
		case "goblin_killed":
			session.GoblinsKilled++
		case "player_died":
			endSession(session)
		}
		return nil
	})
	if err != nil {
		return Session{}, err
	}
	s.logger.InfoContext(ctx, "game event accepted", "session_id", event.SessionID,
		"event_type", event.Type, "data", string(event.Data))
	return session, nil
}

func (s *SessionService) End(ctx context.Context, id string) (Session, error) {
	return s.store.Update(ctx, id, func(session *Session) error {
		endSession(session)
		return nil
	})
}

func endSession(session *Session) {
	// Match PostgreSQL timestamp precision so repeated end responses stay equal.
	now := time.Now().UTC().Truncate(time.Microsecond)
	session.EndedAt = &now
}
