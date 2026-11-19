package session

import (
	"context"
	"errors"
)

var (
	ErrNotFound     = errors.New("session not found")
	ErrEnded        = errors.New("session has ended")
	ErrInvalidEvent = errors.New("unsupported event_type")
	ErrDuplicateID  = errors.New("session ID already exists")
)

// SessionStore commits changes only when the callback succeeds. UpdateWithEvent
// must commit the session change and event together, or neither of them.
type SessionStore interface {
	Create(context.Context, Session) error
	Update(context.Context, string, func(*Session) error) (Session, error)
	UpdateWithEvent(context.Context, Event, func(*Session) error) (Session, error)
}
