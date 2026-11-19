# Project Instructions

## Project overview
This repository contains UKSW Hero, a native C++/raylib arena survival game, and a Go REST API server. PostgreSQL will be used for persistent game data in a later development stage.

## General
- Keep the game client and server separate.
- Server code belongs in `/server`.
- Do not modify files outside the scope of the current task.
- Prefer simple, readable solutions suitable for a small university proof-of-concept project.
- Avoid unnecessary dependencies and abstractions.
- Explain significant architectural decisions in the final summary.

## Game
UKSW Hero is an arena survival game set on the UKSW campus. The player controls a student navigating around campus obstacles while trying to survive waves of goblins pursuing them.

## Client
- Client code belongs in `/client`.
- Use C++17 and raylib for the native desktop application.
- Use CMake to configure and build the client.
- Keep character behavior and game systems in their respective directories under `/client/src`.
- Handle player input, camera movement, rendering, and gameplay updates in the client.
- Keep runtime assets under `/client/assets` and load them relative to the executable.
- Keep client gameplay code separate from server communication and persistence concerns.

## Server
- Use Go 1.27
- Use the Gin framework for the REST API.
- Keep HTTP handlers, business logic, and data models reasonably separated.
- Validate requests and return consistent JSON responses.
- Handle concurrent requests safely.
- Keep the API structured so persistent storage can be introduced later.

## Deployment
- Build the native client using CMake and run the resulting executable locally.
- Run the server and PostgreSQL in separate Docker containers as services managed by Docker Compose.
- Define these services in a root-level `docker-compose.yaml`, which is to be created in a later task.

## Testing
- Add tests for new server functionality.
- Run `go test ./...` from `/server` after server changes.
- Ensure the server builds successfully before completing a task.
