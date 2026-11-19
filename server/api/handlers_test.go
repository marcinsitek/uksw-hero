package api

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"os"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	domain "arena/server/session"
	"arena/server/storage"

	"github.com/gin-gonic/gin"
)

func TestMain(m *testing.M) {
	gin.SetMode(gin.TestMode)
	gin.DefaultWriter = io.Discard
	os.Exit(m.Run())
}

func testRouter(output io.Writer) http.Handler {
	logger := slog.New(slog.NewJSONHandler(output, nil))
	return NewRouter(domain.NewSessionService(storage.NewMemoryStore(), logger), logger)
}

func request(router http.Handler, method, path, body string) *httptest.ResponseRecorder {
	req := httptest.NewRequest(method, path, strings.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	response := httptest.NewRecorder()
	router.ServeHTTP(response, req)
	return response
}

func decodeSession(t *testing.T, response *httptest.ResponseRecorder, status int) domain.Session {
	t.Helper()
	if response.Code != status {
		t.Fatalf("status = %d, want %d: %s", response.Code, status, response.Body)
	}
	var session domain.Session
	if err := json.Unmarshal(response.Body.Bytes(), &session); err != nil {
		t.Fatal(err)
	}
	return session
}

func createSession(t *testing.T, router http.Handler) domain.Session {
	t.Helper()
	return decodeSession(t, request(router, "POST", "/session", `{"player_name":" Knight ","device_info":"Linux"}`), http.StatusCreated)
}

func submitEvent(router http.Handler, id, eventType string) *httptest.ResponseRecorder {
	return request(router, "POST", "/event", fmt.Sprintf(`{"session_id":%q,"event_type":%q}`, id, eventType))
}

func TestSessionJSONFields(t *testing.T) {
	router := testRouter(io.Discard)
	response := request(router, "POST", "/session", `{"player_name":"Student"}`)
	session := decodeSession(t, response, http.StatusCreated)
	assertFields := func(response *httptest.ResponseRecorder, expected []string) {
		t.Helper()
		var fields map[string]json.RawMessage
		if err := json.Unmarshal(response.Body.Bytes(), &fields); err != nil {
			t.Fatal(err)
		}
		if len(fields) != len(expected) {
			t.Fatalf("unexpected session fields: %s", response.Body)
		}
		for _, field := range expected {
			if _, ok := fields[field]; !ok {
				t.Fatalf("missing %s in %s", field, response.Body)
			}
		}
	}
	fields := []string{"session_id", "player_name", "start_time", "health", "goblins_killed"}
	assertFields(response, fields)
	response = request(router, "POST", "/session/"+session.ID+"/end", "")
	decodeSession(t, response, http.StatusOK)
	assertFields(response, append(fields, "end_time"))
	response = request(router, "POST", "/session", `{"player_name":"Student","device_info":"Linux"}`)
	withDevice := decodeSession(t, response, http.StatusCreated)
	if withDevice.DeviceInfo != "Linux" {
		t.Fatalf("unexpected device info: %+v", withDevice)
	}
	assertFields(response, append(fields, "device_info"))
}

func TestEndRefreshesTimestamp(t *testing.T) {
	for _, operation := range []string{"endpoint", "player_died"} {
		t.Run(operation, func(t *testing.T) {
			previousEnd := time.Date(2000, time.January, 1, 0, 0, 0, 0, time.UTC)
			store := storage.NewMemoryStore()
			initial := domain.Session{
				ID: "already-ended", PlayerName: "Student",
				StartedAt: previousEnd.Add(-time.Hour), EndedAt: &previousEnd,
				Health: 70, GoblinsKilled: 2,
			}
			if err := store.Create(context.Background(), initial); err != nil {
				t.Fatal(err)
			}
			logger := slog.New(slog.NewJSONHandler(io.Discard, nil))
			router := NewRouter(domain.NewSessionService(store, logger), logger)
			var response *httptest.ResponseRecorder
			if operation == "endpoint" {
				response = request(router, "POST", "/session/"+initial.ID+"/end", "")
			} else {
				response = submitEvent(router, initial.ID, "player_died")
			}
			ended := decodeSession(t, response, http.StatusOK)
			if ended.EndedAt == nil || !ended.EndedAt.After(previousEnd) ||
				ended.ID != initial.ID || !ended.StartedAt.Equal(initial.StartedAt) ||
				ended.Health != initial.Health || ended.GoblinsKilled != initial.GoblinsKilled {
				t.Fatalf("ending should refresh the timestamp and preserve game state: %+v", ended)
			}
		})
	}
}

func TestSessionLifecycle(t *testing.T) {
	var logs bytes.Buffer
	router := testRouter(&logs)
	initial := createSession(t, router)
	if initial.ID == "" || initial.StartedAt.IsZero() || initial.EndedAt != nil ||
		initial.Health != 100 || initial.GoblinsKilled != 0 ||
		initial.PlayerName != "Knight" || initial.DeviceInfo != "Linux" {
		t.Fatalf("unexpected session: %+v", initial)
	}
	other := createSession(t, router)
	if initial.ID == other.ID {
		t.Fatal("session IDs must be unique")
	}
	for _, step := range []struct {
		event                 string
		health, goblinsKilled int
	}{
		{"player_attacked", 90, 0},
		{"player_attacked", 80, 0},
		{"player_attacked", 70, 0},
		{"goblin_killed", 70, 1},
	} {
		session := decodeSession(t, submitEvent(router, initial.ID, step.event), http.StatusOK)
		if session.ID != initial.ID || session.Health != step.health || session.GoblinsKilled != step.goblinsKilled {
			t.Fatalf("after %s: %+v", step.event, session)
		}
	}
	response := request(router, "POST", "/event", fmt.Sprintf(`{"session_id":%q,"event_type":"goblin_killed","data":{"enemy":"slime"}}`, initial.ID))
	decodeSession(t, response, http.StatusOK)
	if !strings.Contains(logs.String(), initial.ID) || !strings.Contains(logs.String(), "slime") || !strings.Contains(logs.String(), "goblin_killed") {
		t.Fatalf("missing event log data: %s", &logs)
	}
	ended := decodeSession(t, request(router, "POST", "/session/"+initial.ID+"/end", ""), http.StatusOK)
	if ended.Health != 70 || ended.GoblinsKilled != 2 || ended.EndedAt == nil || ended.EndedAt.Before(initial.StartedAt) {
		t.Fatalf("unexpected ended session: %+v", ended)
	}
	repeated := decodeSession(t, request(router, "POST", "/session/"+initial.ID+"/end", ""), http.StatusOK)
	if repeated.EndedAt == nil || repeated.EndedAt.Before(*ended.EndedAt) || repeated.GoblinsKilled != ended.GoblinsKilled || repeated.Health != ended.Health {
		t.Fatalf("unexpected repeated end: %+v", repeated)
	}
	// The current model accepts supported events even after the session ends.
	for _, step := range []struct {
		event                 string
		health, goblinsKilled int
	}{
		{"player_attacked", 60, 2},
		{"goblin_killed", 60, 3},
		{"player_died", 60, 3},
	} {
		updated := decodeSession(t, submitEvent(router, initial.ID, step.event), http.StatusOK)
		if updated.Health != step.health || updated.GoblinsKilled != step.goblinsKilled || updated.EndedAt == nil {
			t.Fatalf("after end and %s: %+v", step.event, updated)
		}
		if step.event != "player_died" && !updated.EndedAt.Equal(*repeated.EndedAt) {
			t.Fatal("non-death event changed end time")
		}
	}
	other = decodeSession(t, submitEvent(router, other.ID, "goblin_killed"), http.StatusOK)
	if other.Health != 100 || other.GoblinsKilled != 1 || other.EndedAt != nil {
		t.Fatalf("sessions are not isolated: %+v", other)
	}
}

func TestHealthFloorAndDeath(t *testing.T) {
	router := testRouter(io.Discard)
	session := createSession(t, router)
	for range 12 {
		session = decodeSession(t, submitEvent(router, session.ID, "player_attacked"), http.StatusOK)
	}
	if session.Health != 0 || session.EndedAt != nil {
		t.Fatalf("health must stop at zero without ending the session: %+v", session)
	}
	session = decodeSession(t, submitEvent(router, session.ID, "player_died"), http.StatusOK)
	if session.EndedAt == nil || session.Health != 0 {
		t.Fatalf("death did not end session: %+v", session)
	}
	assertError(t, submitEvent(router, session.ID, "potion_consumed"), http.StatusBadRequest, "invalid_event_type")
	ended := decodeSession(t, request(router, "POST", "/session/"+session.ID+"/end", ""), http.StatusOK)
	if ended.EndedAt == nil || ended.EndedAt.Before(*session.EndedAt) || ended.Health != 0 || ended.GoblinsKilled != 0 {
		t.Fatalf("unexpected end after death: %+v", ended)
	}
}

func assertError(t *testing.T, response *httptest.ResponseRecorder, status int, code string) {
	t.Helper()
	if response.Code != status || !strings.HasPrefix(response.Header().Get("Content-Type"), "application/json") {
		t.Fatalf("expected JSON status %d, got %d: %s", status, response.Code, response.Body)
	}
	var result struct {
		Error struct{ Code, Message string } `json:"error"`
	}
	if err := json.Unmarshal(response.Body.Bytes(), &result); err != nil {
		t.Fatal(err)
	}
	if result.Error.Code != code || result.Error.Message == "" {
		t.Fatalf("unexpected error: %s", response.Body)
	}
}

func TestInvalidRequests(t *testing.T) {
	var logs bytes.Buffer
	router := testRouter(&logs)
	session := createSession(t, router)
	for _, body := range []string{"", "{", "null", "[]", `{}`, `{"player_name":42}`, `{"player_name":"  "}`, `{"player_name":"Knight","device_info":{}}`, `{"player_name":"Knight"} {}`, `{"player_name":"Knight"} garbage`} {
		t.Run("session/"+body, func(t *testing.T) {
			assertError(t, request(router, "POST", "/session", body), http.StatusBadRequest, "invalid_request")
		})
	}
	for _, body := range []string{"", "{", "null", "[]", `{}`, `{"session_id":"x"}`, `{"event_type":"goblin_killed"}`, `{"session_id":42,"event_type":"goblin_killed"}`, `{"session_id":" ","event_type":"goblin_killed"}`, `{"session_id":"x","event_type":1}`, `{"session_id":"x","event_type":"goblin_killed"} {}`} {
		t.Run("event/"+body, func(t *testing.T) {
			assertError(t, request(router, "POST", "/event", body), http.StatusBadRequest, "invalid_request")
		})
	}
	for _, eventType := range []string{"unknown", "enemy_killed", "potion_consumed"} {
		assertError(t, submitEvent(router, session.ID, eventType), http.StatusBadRequest, "invalid_event_type")
	}
	assertError(t, submitEvent(router, "missing", "goblin_killed"), http.StatusNotFound, "session_not_found")
	assertError(t, request(router, "POST", "/session/missing/end", ""), http.StatusNotFound, "session_not_found")
	assertError(t, request(router, "GET", "/session", ""), http.StatusMethodNotAllowed, "method_not_allowed")
	assertError(t, request(router, "POST", "/missing", ""), http.StatusNotFound, "not_found")
	assertError(t, request(router, "POST", "/session", `{"player_name":"`+strings.Repeat("x", 1<<20)+`"}`), http.StatusRequestEntityTooLarge, "request_too_large")
	if logs.Len() != 0 {
		t.Fatalf("invalid events logged as accepted: %s", &logs)
	}
	final := decodeSession(t, request(router, "POST", "/session/"+session.ID+"/end", ""), http.StatusOK)
	if final.Health != 100 || final.GoblinsKilled != 0 {
		t.Fatalf("invalid requests changed state: %+v", final)
	}
}

func TestConcurrentEvents(t *testing.T) {
	router := testRouter(io.Discard)
	session := createSession(t, router)
	var wg sync.WaitGroup
	for range 100 {
		wg.Go(func() {
			response := submitEvent(router, session.ID, "goblin_killed")
			if response.Code != http.StatusOK {
				t.Errorf("concurrent event status: %d", response.Code)
			}
		})
	}
	wg.Wait()
	final := decodeSession(t, request(router, "POST", "/session/"+session.ID+"/end", ""), http.StatusOK)
	if final.GoblinsKilled != 100 {
		t.Fatalf("lost goblins killed updates: %d", final.GoblinsKilled)
	}
	for range 50 {
		wg.Go(func() {
			if response := submitEvent(router, session.ID, "goblin_killed"); response.Code != http.StatusOK {
				t.Errorf("event after end failed: %d", response.Code)
			}
		})
		wg.Go(func() {
			if response := request(router, "POST", "/session/"+session.ID+"/end", ""); response.Code != http.StatusOK {
				t.Errorf("concurrent end failed: %d", response.Code)
			}
		})
	}
	wg.Wait()
	updated := decodeSession(t, request(router, "POST", "/session/"+session.ID+"/end", ""), http.StatusOK)
	if updated.GoblinsKilled != 150 || updated.Health != 100 || updated.EndedAt == nil || updated.EndedAt.Before(*final.EndedAt) {
		t.Fatalf("lost concurrent updates after end: %+v", updated)
	}
}

func TestEndRacingWithEvents(t *testing.T) {
	router := testRouter(io.Discard)
	session := createSession(t, router)
	start := make(chan struct{})
	var accepted atomic.Int64
	var wg sync.WaitGroup
	for range 100 {
		wg.Go(func() {
			<-start
			response := submitEvent(router, session.ID, "goblin_killed")
			switch response.Code {
			case http.StatusOK:
				accepted.Add(1)
			default:
				t.Errorf("unexpected event response: %d", response.Code)
			}
		})
	}
	wg.Go(func() {
		<-start
		if response := request(router, "POST", "/session/"+session.ID+"/end", ""); response.Code != http.StatusOK {
			t.Errorf("end failed: %d", response.Code)
		}
	})
	close(start)
	wg.Wait()
	final := decodeSession(t, request(router, "POST", "/session/"+session.ID+"/end", ""), http.StatusOK)
	if accepted.Load() != 100 || final.GoblinsKilled != 100 || final.EndedAt == nil || final.Health != 100 {
		t.Fatalf("end/event race lost accepted updates: %+v, accepted %d", final, accepted.Load())
	}
}
