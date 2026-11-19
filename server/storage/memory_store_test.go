package storage

import (
	"context"
	"encoding/json"
	"errors"
	"testing"

	domain "arena/server/session"
)

func TestMemoryStoreEventAtomicity(t *testing.T) {
	ctx := context.Background()
	store := NewMemoryStore()
	if err := store.Create(ctx, domain.Session{ID: "test", Health: 100}); err != nil {
		t.Fatal(err)
	}
	event := domain.Event{SessionID: "test", Type: "goblin_killed", Data: json.RawMessage(`{"enemy":"goblin"}`)}
	rejected := errors.New("reject change")
	_, err := store.UpdateWithEvent(ctx, event, func(session *domain.Session) error {
		session.GoblinsKilled = 1
		return rejected
	})
	if !errors.Is(err, rejected) || store.sessions["test"].GoblinsKilled != 0 || len(store.events) != 0 {
		t.Fatal("rejected change persisted a session update or event")
	}
	_, err = store.UpdateWithEvent(ctx, event, func(session *domain.Session) error {
		session.GoblinsKilled++
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
	event.Data[0] = 'x'
	if store.sessions["test"].GoblinsKilled != 1 || len(store.events) != 1 || !json.Valid(store.events[0].Data) {
		t.Fatal("accepted event was not saved independently of the caller's payload")
	}
}
