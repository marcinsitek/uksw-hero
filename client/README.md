# UKSW Hero client

Minimal native C++17/raylib client, separate from the Go server. This stage
includes WASD movement, a following camera, rectangle collisions, and a collision
overlay while TAB is held. ESC or the window close button exits.

## Build

Requires a C/C++ compiler, CMake 3.24 or newer, and a build tool such as Make.
On Ubuntu, install the desktop build dependencies:

```sh
sudo apt install build-essential cmake libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev libcurl4-openssl-dev nlohmann-json3-dev
```

From the repository root:

```sh
cmake -S client -B client/build -DCMAKE_BUILD_TYPE=Release
cmake --build client/build --parallel
./client/build/uksw_arena
```

CMake uses an installed raylib 5.5-compatible package when available; otherwise
it downloads and builds [raylib 5.5](https://github.com/raysan5/raylib/tree/5.5)
through FetchContent. The first such build requires internet access. For an
offline build, provide a local raylib source checkout using
`-DFETCHCONTENT_SOURCE_DIR_RAYLIB=/absolute/path/to/raylib` at configure time.

## Arena asset

Add `assets/maps/uksw_arena.png`, then build again to copy it beside the
executable. Assets are resolved relative to the executable, so the game can be
started from another working directory. Without the image, a plain fallback
arena displays the example obstacles. Their coordinates are placeholders and
must be measured against the final map.

## Session creation

Starting a game queues `POST /session` with the entered `player_name`. The client
reads only `session_id` from a successful JSON response and displays it below the
player name. It ignores the other response fields. While waiting, or if the server
is unavailable or returns an invalid response, the session ID stays empty and
local gameplay continues normally. Each new game clears the previous ID.

The default API URL is `http://127.0.0.1:8080`. Override it before launching:

```sh
UKSW_HERO_API_URL=http://127.0.0.1:8081 ./client/build/uksw_arena
```

One background worker processes requests in FIFO order, with at most eight
pending requests in addition to one in flight. HTTP requests use a one-second
connection timeout and a three-second total timeout. A libcurl build with
asynchronous DNS support is required to enforce hostname resolution timeouts;
otherwise networking is disabled and the game runs without a session.
Failures are logged to stderr, with no automatic retries. On exit, queued requests
are canceled and an in-flight request finishes within its timeout.

## Gameplay events

Characters emit local notifications, and `main.cpp` queues them as `POST /event`
requests with `session_id` and `event_type`:

| Event | Trigger |
| --- | --- |
| `player_attacked` | The student takes positive attack damage, including a lethal hit. |
| `player_died` | Student health first reaches zero, before its death animation finishes. |
| `goblin_killed` | A goblin first receives lethal damage, before its death animation finishes. |

A lethal attack reports `player_attacked` before `player_died`. The debug death
key reports only `player_died`. Repeated damage to dead characters reports nothing.
Local health and the HUD kill count continue to be calculated by the client.

Events share the bounded FIFO queue with session creation and retain the session
they were queued for, including when another game starts. Events produced while
creation is pending wait behind it. If creation fails, their requests are skipped.
Events are also dropped when the queue is full or the client shuts down. Event
response bodies and HTTP statuses are ignored; only transport failures are logged.
There are no retries, acknowledgments, or separate session-end requests yet.

Run the headless combat and mocked-HTTP checks (no running API or display needed):

```sh
ctest --test-dir client/build --output-on-failure
```
