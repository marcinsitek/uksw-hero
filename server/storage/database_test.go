package storage

import (
	"context"
	"testing"
)

func TestOpenDatabaseRequiresURL(t *testing.T) {
	for _, url := range []string{"", "   "} {
		pool, err := OpenDatabase(context.Background(), url)
		if pool != nil || err == nil {
			t.Fatal("empty DATABASE_URL must fail before connecting")
		}
	}
}
