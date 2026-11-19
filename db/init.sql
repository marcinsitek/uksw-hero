BEGIN;

CREATE TABLE session (
    id TEXT PRIMARY KEY,
    player_name TEXT NOT NULL,
    device_info TEXT,
    started_at TIMESTAMPTZ NOT NULL,
    ended_at TIMESTAMPTZ,
    health INTEGER NOT NULL,
    goblins_killed INTEGER NOT NULL
);

CREATE TABLE event (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    session_id TEXT NOT NULL REFERENCES session(id),
    event_type TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    data JSONB
);

COMMIT;
