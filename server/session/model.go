package session

import (
	"encoding/json"
	"time"
)

const (
	StatusActive = "active"
	StatusEnded  = "ended"
)

type Session struct {
	ID            string     `json:"session_id"`
	PlayerName    string     `json:"player_name"`
	DeviceInfo    string     `json:"device_info,omitempty"`
	StartedAt     time.Time  `json:"start_time"`
	EndedAt       *time.Time `json:"end_time,omitempty"`
	Health        int        `json:"health"`
	GoblinsKilled int        `json:"goblins_killed"`
}

type Event struct {
	SessionID string          `json:"session_id" binding:"required"`
	Type      string          `json:"event_type" binding:"required"`
	Data      json.RawMessage `json:"data,omitempty"`
}
