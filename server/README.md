# Arena game API

Requires Go 1.27. From this directory:

```sh
go mod download
go run .
```

The server listens on `:8080`. Set `ADDR=127.0.0.1:8081` to override it.
Sessions live only in this process and disappear when it restarts. There is no database.

## API

All responses are JSON. Send JSON objects to the create and event endpoints; bodies
are limited to 1 MiB. `player_name` must be a nonblank string (outer whitespace is
trimmed). `device_info` is an optional string. Event `data` may contain any valid
JSON value and is logged without affecting the game rules.

| Endpoint | Success | Behavior |
| --- | --- | --- |
| `POST /session` | `201 Created` | Create an independent session with health 100, score 0, and status `active`. |
| `POST /event` | `200 OK` | Apply one event to the given `session_id`. |
| `POST /session/:id/end` | `200 OK` | End the session; no request body required. Repeating this preserves the original end time and final state. |

Responses contain `session_id`, `player_name`, optional `device_info`, `start_time`,
`health`, `score`, and `status`. Ended sessions also include `end_time`. Times are UTC
RFC 3339 timestamps. Session IDs are opaque, cryptographically random strings.

| Event type | Effect |
| --- | --- |
| `player_attacked` | Subtract 10 health, stopping at zero. |
| `potion_consumed` | Add 20 health, capped at 100. |
| `enemy_killed` | Add 10 score. |
| `player_died` | End the session and record its end time, preserving health and score. |

Zero health alone does not end a session: send `player_died` or call the end
endpoint. Every event against an ended session, including repeated `player_died`,
returns `409 Conflict`. Every accepted event is logged to stdout with its session
ID, event type, and optional data; rejected events are not logged as accepted.

Errors use this shape:

```json
{"error":{"code":"session_not_found","message":"session not found"}}
```

Malformed JSON, wrong field types, missing required fields, and unsupported event
types return `400`. Unknown sessions return `404`, ended-session events return
`409`, oversized JSON bodies return `413`, and unsupported HTTP methods return
`405`. Unexpected storage errors return `500` without exposing internal details.
Clients should inspect both the HTTP status and `error.code`.

## Try it

```sh
curl -i http://localhost:8080/session \
  -H 'Content-Type: application/json' \
  -d '{"player_name":"Knight","device_info":"Linux"}'

# Copy session_id from the response:
SESSION_ID='replace-with-session-id'

curl -i http://localhost:8080/event \
  -H 'Content-Type: application/json' \
  -d "{\"session_id\":\"$SESSION_ID\",\"event_type\":\"player_attacked\"}"

curl -i http://localhost:8080/event \
  -H 'Content-Type: application/json' \
  -d "{\"session_id\":\"$SESSION_ID\",\"event_type\":\"potion_consumed\"}"

curl -i http://localhost:8080/event \
  -H 'Content-Type: application/json' \
  -d "{\"session_id\":\"$SESSION_ID\",\"event_type\":\"enemy_killed\",\"data\":{\"enemy\":\"slime\"}}"

curl -i -X POST "http://localhost:8080/session/$SESSION_ID/end"
```

## Verify

```sh
go test ./...
go test -race ./...
go build -o arena-server .
```

## Organization

Go code is grouped into three packages, with the executable at the server root:

```text
server/
├── main.go
├── api/
│   ├── handlers.go
│   └── handlers_test.go
├── session/
│   ├── model.go
│   ├── service.go
│   └── store.go
└── storage/
    ├── database.go
    ├── database_test.go
    ├── memory_store.go
    ├── memory_store_test.go
    ├── postgres_store.go
    └── postgres_store_test.go
```

`main.go` reads configuration, opens PostgreSQL using `DATABASE_URL`, wires the
store, service, router, and logger together, and manages HTTP startup and shutdown.
The existing root build command and Dockerfile still apply.

`api` handles HTTP parsing, validation, and consistent JSON responses. It receives
the session service and its own logger through the router constructor.

`session` defines the session/event models, game rules, storage interface, and
domain errors. It does not import either the HTTP or storage packages.

`storage` implements the interface with a PostgreSQL store and a mutex-protected
in-memory store. Database connection configuration also belongs here. PostgreSQL
updates use transactions and row locks to commit session changes and events
together; tests can inject the in-memory store instead.

Tests stay beside their implementations. Run checks from `/server` using the
commands above. PostgreSQL integration tests require `TEST_DATABASE_URL` and use
isolated schemas initialized from `../../db/init.sql`, relative to `/server/storage`.
