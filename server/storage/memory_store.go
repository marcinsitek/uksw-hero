package storage

import (
	"context"
	"sync"

	domain "arena/server/session"
)

type MemoryStore struct {
	mu       sync.Mutex
	sessions map[string]domain.Session
	events   []domain.Event
}

func NewMemoryStore() *MemoryStore {
	return &MemoryStore{sessions: make(map[string]domain.Session)}
}

func (s *MemoryStore) Create(_ context.Context, session domain.Session) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, exists := s.sessions[session.ID]; exists {
		return domain.ErrDuplicateID
	}
	s.sessions[session.ID] = cloneSession(session)
	return nil
}

func (s *MemoryStore) Update(_ context.Context, id string, change func(*domain.Session) error) (domain.Session, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.update(id, change)
}

func (s *MemoryStore) UpdateWithEvent(_ context.Context, event domain.Event, change func(*domain.Session) error) (domain.Session, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	session, err := s.update(event.SessionID, change)
	if err != nil {
		return domain.Session{}, err
	}
	event.Data = append(event.Data[:0:0], event.Data...)
	s.events = append(s.events, event)
	return session, nil
}

// update requires the caller to hold mu.
func (s *MemoryStore) update(id string, change func(*domain.Session) error) (domain.Session, error) {
	session, exists := s.sessions[id]
	if !exists {
		return domain.Session{}, domain.ErrNotFound
	}
	session = cloneSession(session)
	if err := change(&session); err != nil {
		return domain.Session{}, err
	}
	s.sessions[id] = cloneSession(session)
	return session, nil
}

func cloneSession(session domain.Session) domain.Session {
	if session.EndedAt != nil {
		end := *session.EndedAt
		session.EndedAt = &end
	}
	return session
}
