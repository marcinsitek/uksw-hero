
#include "raylib.h"

#include "characters/Student.h"
#include "characters/Goblin.h"
#include "systems/GoblinSpawner.h"
#include "systems/NavigationGrid.h"
#include "network/ApiClient.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <future>
#include <string>
#include <vector>

constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 720;

constexpr int TILE_SIZE = 32;
constexpr int GOBLINS_PER_WAVE = 3;

// Fallback dimensions if the arena texture is missing.
// Must cover all obstacle and spawn coordinates.
constexpr int FALLBACK_MAP_WIDTH = 1600;
constexpr int FALLBACK_MAP_HEIGHT = 900;

constexpr Vector2 STUDENT_START = {480.0f, 470.0f};

typedef enum GameScreen {
    TITLE = 0,
    GAMEPLAY,
    ENDING
} GameScreen;

void DrawCenteredText(Font font, const char* text, float y, float fontSize, Color color)
{
    const float width = MeasureTextEx(font, text, fontSize, 1.0f).x;
    DrawTextEx(font, text, {(SCREEN_WIDTH - width) / 2.0f, y}, fontSize, 1.0f, color);
}

int main()
{
    InitWindow(
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        "UKSW Hero"
    );

    if (!IsWindowReady()) {
        return 1;
    }

    SetTargetFPS(60);

    GameScreen currentScreen = TITLE;
    int goblinsKilled = 0;

    const char* apiUrl = std::getenv("UKSW_HERO_API_URL");
    ApiClient apiClient(apiUrl && *apiUrl ? apiUrl : "http://127.0.0.1:8080");
    std::future<std::string> sessionRequest;
    std::string sessionId;

    const GameplayEventHandler reportEvent = [&apiClient](GameplayEvent event) {
        switch (event) {
            case GameplayEvent::PlayerAttacked: apiClient.QueueEvent("player_attacked"); break;
            case GameplayEvent::PlayerDied: apiClient.QueueEvent("player_died"); break;
            case GameplayEvent::GoblinKilled: apiClient.QueueEvent("goblin_killed"); break;
        }
    };

    // --------------------------------------------------
    // Player name input
    // --------------------------------------------------

    constexpr int MAX_NAME_LENGTH = 24;

    char playerName[MAX_NAME_LENGTH + 1] = {};
    int playerNameLength = 0;
    bool textBoxEditMode = true;

    const Rectangle nameTextBox = {
        (SCREEN_WIDTH - 520) / 2.0f,
        400,
        520,
        44
    };

    // --------------------------------------------------
    // Load assets
    // --------------------------------------------------

    ChangeDirectory(GetApplicationDirectory());

    // Include Latin Extended-A so the Polish description keeps its diacritics.
    int codepoints[352];
    for (int i = 0; i < 352; ++i) codepoints[i] = i + 32;
    Font uiFont = LoadFontEx("assets/fonts/DejaVuSans.ttf", 48, codepoints, 352);
    SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);

    Texture2D arena =
        LoadTexture("assets/maps/uksw_arena.png");

    Texture2D studentTexture =
        LoadTexture("assets/sprites/student.png");

    Texture2D goblinTexture =
        LoadTexture("assets/sprites/goblin.png");

    // --------------------------------------------------
    // Arena obstacles
    // --------------------------------------------------

    // Add buildings, cars, trees, fences, etc. here.
    // Coordinates should match the arena image.

    const std::vector<Rectangle> obstacles = {
        // Building 21: stepped rectangles follow the curved western wing.
        {624, 66, 284, 164},
        {576, 78, 48, 158},
        {528, 92, 48, 148},
        {480, 96, 48, 160},
        {432, 128, 48, 192},
        {384, 184, 48, 152},
        {352, 240, 32, 92},

        // Building 3 ends at y=544; the sports hall starts at y=576.
        // The intervening 32-pixel row is one complete navigation tile.
        {174, 330, 150, 74},   // Building 3: rear wing
        {154, 404, 178, 140},  // Building 3: front wing
        {140, 576, 228, 220},  // Sports hall
        {220, 796, 64, 12},    // Sports hall entrance steps

        {544, 564, 336, 140},  // Building 12
        {680, 704, 64, 16},    // Building 12 entrance steps
        {1336, 92, 160, 708},  // Building 23
        {1400, 800, 64, 20},   // Building 23 entrance steps
        {1030, 156, 108, 130}, // Church

        // Five parked cars (including the horizontal car on the east side).
        {1160, 474, 24, 40},
        {1006, 564, 25, 42},
        {1005, 628, 27, 50},
        {1047, 630, 24, 47},
        {1190, 624, 35, 26},

        // Western fence follows the diagonal road beside building 3 and the hall.
        {140, 64, 20, 160},
        {128, 224, 20, 160},
        {116, 384, 20, 160},
        {104, 544, 20, 128},
        {92, 672, 20, 144},

        // Campus tree boundaries; keep the north/south path openings clear.
        {140, 0, 1108, 60},    // Upper boundary, west of the central path
        {1296, 0, 208, 60},    // Upper boundary, east of the central path
        {48, 832, 1200, 109},  // Bottom boundary, west of the central path
        {1312, 872, 192, 69},  // Bottom boundary, east of the central path
        {1520, 0, 44, 941},    // Trees along the campus side of the eastern road
        {1632, 0, 39, 941}     // Trees at the right-hand edge of the arena
    };

    // --------------------------------------------------
    // Navigation grid
    // --------------------------------------------------

    const int mapWidth =
        arena.id != 0 ? arena.width : FALLBACK_MAP_WIDTH;

    const int mapHeight =
        arena.id != 0 ? arena.height : FALLBACK_MAP_HEIGHT;

    NavigationGrid navigationGrid(
        mapWidth,
        mapHeight,
        TILE_SIZE,
        obstacles
    );

    // --------------------------------------------------
    // Student
    // --------------------------------------------------

    Student student(
        STUDENT_START,
        studentTexture,
        reportEvent
    );

    // --------------------------------------------------
    // Goblin spawning
    // --------------------------------------------------

    // Example positions in front of building entrances.
    // Replace with measured coordinates from the map.
    //
    // Each position represents the top-left corner of
    // the goblin's 24x24 collision rectangle.

    const std::vector<Vector2> spawnPoints = {
        {420.0f, 280.0f},   // Building 21 entrance
        {230.0f, 550.0f},   // Building 3 entrance
        {700.0f, 520.0f}    // Building 12 entrance
    };

    std::vector<Goblin> goblins;

    GoblinSpawner spawner(
        spawnPoints,
        GOBLINS_PER_WAVE,
        goblinTexture,
        reportEvent
    );

    // --------------------------------------------------
    // Camera
    // --------------------------------------------------

    Camera2D camera = {};

    camera.offset = {
        SCREEN_WIDTH / 2.0f,
        SCREEN_HEIGHT / 2.0f
    };

    camera.zoom = 1.0f;

    const Rectangle initialBounds = student.GetBounds();

    camera.target = {
        initialBounds.x + initialBounds.width / 2.0f,
        initialBounds.y + initialBounds.height / 2.0f
    };

    // --------------------------------------------------
    // Main game loop
    // --------------------------------------------------

    while (!WindowShouldClose()) {

        const float dt = GetFrameTime();

        if (sessionRequest.valid() &&
            sessionRequest.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            sessionId = sessionRequest.get();
        }

        // ----------------------------------------------
        // Update
        // ----------------------------------------------

        switch (currentScreen)
        {
            case TITLE:
            {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    textBoxEditMode = CheckCollisionPointRec(
                        GetMousePosition(),
                        nameTextBox
                    );
                }

                int key = GetCharPressed();

                while (key > 0) {
                    if (textBoxEditMode &&
                        key >= 32 &&
                        key <= 126 &&
                        playerNameLength < MAX_NAME_LENGTH) {

                        playerName[playerNameLength++] =
                            static_cast<char>(key);

                        playerName[playerNameLength] = '\0';
                    }

                    key = GetCharPressed();
                }

                if (textBoxEditMode &&
                    IsKeyPressed(KEY_BACKSPACE) &&
                    playerNameLength > 0) {

                    playerName[--playerNameLength] = '\0';
                }

                // Start a new game.
                if (IsKeyPressed(KEY_ENTER) &&
                    playerName[0] != '\0') {

                    // Reset student.
                    student = Student(
                        STUDENT_START,
                        studentTexture,
                        reportEvent
                    );

                    // Remove goblins from the previous game.
                    goblins.clear();
                    goblinsKilled = 0;

                    // Clear the previous ID immediately. Replacing the future
                    // prevents a late response from an older game appearing here.
                    sessionId.clear();
                    sessionRequest = apiClient.QueueCreateSession(playerName);

                    // Reset the spawning timer.
                    spawner = GoblinSpawner(
                        spawnPoints,
                        GOBLINS_PER_WAVE,
                        goblinTexture,
                        reportEvent
                    );

                    currentScreen = GAMEPLAY;
                }

            } break;

            case GAMEPLAY:
            {
                // --------------------------------------
                // Update student
                // --------------------------------------

                const auto livingBeforeAttack = std::count_if(
                    goblins.begin(), goblins.end(),
                    [](const Goblin& goblin) { return goblin.IsAlive(); }
                );

                student.Update(dt, obstacles, goblins);

                // Count lethal hits immediately, before spawning or death animations.
                const auto livingAfterAttack = std::count_if(
                    goblins.begin(), goblins.end(),
                    [](const Goblin& goblin) { return goblin.IsAlive(); }
                );
                goblinsKilled += static_cast<int>(livingBeforeAttack - livingAfterAttack);

                // --------------------------------------
                // Spawn goblins
                // --------------------------------------

                if (!student.IsDead()) {
                    spawner.Update(dt, goblins);
                }

                // --------------------------------------
                // Update goblins
                // --------------------------------------

                for (Goblin& goblin : goblins) {
                    goblin.Update(
                        dt,
                        student,
                        navigationGrid,
                        obstacles
                    );
                }

                // --------------------------------------
                // Remove dead goblins
                // --------------------------------------

                goblins.erase(
                    std::remove_if(
                        goblins.begin(),
                        goblins.end(),
                        [](const Goblin& goblin) {
                            return goblin.IsDead();
                        }
                    ),
                    goblins.end()
                );

                // --------------------------------------
                // Update camera
                // --------------------------------------

                const Rectangle playerBounds =
                    student.GetBounds();

                camera.target = {
                    playerBounds.x + playerBounds.width / 2.0f,
                    playerBounds.y + playerBounds.height / 2.0f
                };

                // --------------------------------------
                // Game over
                // --------------------------------------

                if (student.HasDeathAnimationFinished()) {
                    currentScreen = ENDING;
                }

                // Manual ending (existing behavior).
                if (!student.IsDead() &&
                    (IsKeyPressed(KEY_ENTER) ||
                     IsGestureDetected(GESTURE_TAP))) {

                    currentScreen = ENDING;
                }

            } break;

            case ENDING:
            {
                if (IsKeyPressed(KEY_ENTER) ||
                    IsGestureDetected(GESTURE_TAP)) {

                    currentScreen = TITLE;
                    textBoxEditMode = true;
                }

            } break;

            default:
                break;
        }

        // ----------------------------------------------
        // Draw
        // ----------------------------------------------

        BeginDrawing();

        ClearBackground(BLACK);

        switch (currentScreen)
        {
            case TITLE:
            {
                DrawCenteredText(uiFont, "UKSW Hero", 100, 60, MAGENTA);

                const char* description[] = {
                    "Kampus UKSW na Wójcickigo został opanowany przez gobliny z pobliskiego Kampinosu.",
                    "Jesteś studentem III roku informatyki i jedyną nadzieją na uratowanie uczelni.",
                    "Chwyć swojego MacBook-a i zniszcz gobliny!"
                };
                for (int i = 0; i < 3; ++i) {
                    DrawCenteredText(uiFont, description[i], 190 + i * 30, 20, WHITE);
                }

                DrawCenteredText(uiFont, "START GAME", 310, 28, WHITE);
                DrawCenteredText(uiFont, "Name:", 365, 20, WHITE);
                DrawCenteredText(
                    uiFont, "TYPE YOUR NAME AND PRESS ENTER TO START", 480, 20, WHITE
                );

                DrawRectangleRec(
                    nameTextBox,
                    LIGHTGRAY
                );

                DrawRectangleLinesEx(
                    nameTextBox,
                    2,
                    textBoxEditMode ? BLUE : GRAY
                );

                const float playerNameWidth = MeasureTextEx(uiFont, playerName, 20, 1.0f).x;
                DrawCenteredText(uiFont, playerName, nameTextBox.y + 12, 20, BLACK);

                if (textBoxEditMode &&
                    playerNameLength < MAX_NAME_LENGTH &&
                    static_cast<int>(GetTime() * 2) % 2 == 0) {

                    DrawTextEx(
                        uiFont, "_",
                        {(SCREEN_WIDTH + playerNameWidth) / 2.0f + 1, nameTextBox.y + 12},
                        20, 1.0f, BLACK
                    );
                }

            } break;

            case GAMEPLAY:
            {
                BeginMode2D(camera);

                // --------------------------------------
                // Draw arena
                // --------------------------------------

                if (arena.id != 0) {
                    DrawTexture(
                        arena,
                        0, 0,
                        WHITE
                    );
                }
                else {
                    DrawRectangle(
                        0, 0,
                        mapWidth,
                        mapHeight,
                        DARKGREEN
                    );

                    for (const Rectangle& obstacle : obstacles) {
                        DrawRectangleRec(
                            obstacle,
                            DARKGRAY
                        );
                    }
                }

                // --------------------------------------
                // Draw student
                // --------------------------------------

                student.Draw();

                // --------------------------------------
                // Draw goblins
                // --------------------------------------

                for (const Goblin& goblin : goblins) {
                    goblin.Draw();
                }

                // --------------------------------------
                // Debug: obstacle rectangles (TAB)
                // --------------------------------------

                if (IsKeyDown(KEY_TAB)) {
                    for (const Rectangle& obstacle : obstacles) {
                        DrawRectangleLinesEx(
                            obstacle,
                            2,
                            RED
                        );
                    }
                }

                // --------------------------------------
                // Debug: navigation grid (G)
                // --------------------------------------

                if (IsKeyDown(KEY_G)) {

                    const int tileSize =
                        navigationGrid.GetTileSize();

                    for (int y = 0;
                         y < navigationGrid.GetRows();
                         y++) {

                        for (int x = 0;
                             x < navigationGrid.GetColumns();
                             x++) {

                            Rectangle tile = {
                                x * static_cast<float>(tileSize),
                                y * static_cast<float>(tileSize),
                                static_cast<float>(tileSize),
                                static_cast<float>(tileSize)
                            };

                            if (!navigationGrid.IsWalkable(x, y)) {
                                DrawRectangleRec(
                                    tile,
                                    Fade(RED, 0.35f)
                                );
                            }

                            DrawRectangleLinesEx(
                                tile,
                                1.0f,
                                Fade(WHITE, 0.2f)
                            );
                        }
                    }
                }

                EndMode2D();

                // --------------------------------------
                // HUD
                // --------------------------------------

                DrawText(
                    "UKSW Hero",
                    20, 20, 24, WHITE
                );

                DrawText(
                    TextFormat("Player: %s", playerName),
                    20, 50, 20, WHITE
                );

                DrawText(
                    TextFormat("Session ID: %s", sessionId.c_str()),
                    20, 80, 20, WHITE
                );

                DrawText(
                    TextFormat("Health: %d", student.GetHealth()),
                    SCREEN_WIDTH - 280, 20, 20, WHITE
                );

                DrawText(
                    TextFormat("Goblins killed: %d", goblinsKilled),
                    SCREEN_WIDTH - 280, 50, 20, WHITE
                );

                DrawText(
                    "WASD - Move | SPACE - Attack | P - Die",
                    20, 110, 16, MAGENTA
                );

                DrawText(
                    "TAB - Collisions | G - Navigation grid",
                    20, 135, 16, WHITE
                );

                // --------------------------------------
                // Missing asset warnings
                // --------------------------------------

                int warningY = 165;

                if (arena.id == 0) {
                    DrawText(
                        "Map missing: assets/maps/uksw_arena.png",
                        20, warningY, 16, YELLOW
                    );
                    warningY += 25;
                }

                if (studentTexture.id == 0) {
                    DrawText(
                        "Sprite missing: assets/sprites/student.png",
                        20, warningY, 16, YELLOW
                    );
                    warningY += 25;
                }

                if (goblinTexture.id == 0) {
                    DrawText(
                        "Sprite missing: assets/sprites/goblin.png",
                        20, warningY, 16, YELLOW
                    );
                }

            } break;

            case ENDING:
            {
                DrawCenteredText(uiFont, "GAME OVER", 280, 40, WHITE);

                DrawCenteredText(
                    uiFont, "PRESS ENTER OR TAP TO RETURN TO TITLE SCREEN", 360, 20, WHITE
                );

            } break;

            default:
                break;
        }

        EndDrawing();
    }

    // --------------------------------------------------
    // Cleanup
    // --------------------------------------------------

    // Goblins and Student do not own their textures.
    goblins.clear();

    if (goblinTexture.id != 0) {
        UnloadTexture(goblinTexture);
    }

    if (studentTexture.id != 0) {
        UnloadTexture(studentTexture);
    }

    if (arena.id != 0) {
        UnloadTexture(arena);
    }

    UnloadFont(uiFont);
    CloseWindow();

    return 0;
}
