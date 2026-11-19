package storage

import (
	"context"
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/jackc/pgx/v5/pgxpool"
)

const databaseTimeout = 5 * time.Second

func OpenDatabase(ctx context.Context, databaseURL string) (*pgxpool.Pool, error) {
	if strings.TrimSpace(databaseURL) == "" {
		return nil, errors.New("DATABASE_URL is required")
	}
	config, err := pgxpool.ParseConfig(databaseURL)
	if err != nil {
		// Connection-string parse errors may contain credentials.
		return nil, errors.New("invalid DATABASE_URL")
	}
	config.MaxConns = 10
	config.MinConns = 0
	config.ConnConfig.ConnectTimeout = databaseTimeout

	ctx, cancel := context.WithTimeout(ctx, databaseTimeout)
	defer cancel()
	pool, err := pgxpool.NewWithConfig(ctx, config)
	if err != nil {
		return nil, fmt.Errorf("create database pool: %w", err)
	}
	if err := pool.Ping(ctx); err != nil {
		pool.Close()
		return nil, fmt.Errorf("connect to database: %w", err)
	}
	return pool, nil
}
