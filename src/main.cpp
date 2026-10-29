#include "WorldGenerator.h"
#include "WorldRenderer.h"
#include "WorldSave.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <functional>
#include <vector>
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif
constexpr int SCREEN_WIDTH = 1100;
constexpr int SCREEN_HEIGHT = 760;
constexpr int HUD_HEIGHT = 100;
constexpr int VIEW_HEIGHT = SCREEN_HEIGHT - HUD_HEIGHT;
enum class Overlay {
    None,
    Save,
    Load,
    SeedSearch
};
namespace {
void clampCamera(Camera2D& camera, const WorldSettings& settings) {
    const float worldWidth = settings.width * TILE_SIZE;
    const float worldHeight = settings.height * TILE_SIZE;
    const float minZoom = std::max(
        static_cast<float>(SCREEN_WIDTH) / worldWidth,
        static_cast<float>(VIEW_HEIGHT) / worldHeight
    );
    camera.zoom = std::clamp(camera.zoom, minZoom, 3.0f);
    const float halfWidth = SCREEN_WIDTH / (2.0f * camera.zoom);
    const float halfHeight = VIEW_HEIGHT / (2.0f * camera.zoom);
    camera.target.x = halfWidth * 2.0f >= worldWidth
        ? worldWidth / 2.0f
        : std::clamp(camera.target.x, halfWidth, worldWidth - halfWidth);
    camera.target.y = halfHeight * 2.0f >= worldHeight
        ? worldHeight / 2.0f
        : std::clamp(camera.target.y, halfHeight, worldHeight - halfHeight);
}
void centerCamera(Camera2D& camera, const WorldSettings& settings) {
    camera.target = {
        settings.width * TILE_SIZE / 2.0f,
        settings.height * TILE_SIZE / 2.0f
    };
    camera.zoom = 1.0f;
    clampCamera(camera, settings);
}
bool parseSeed(const std::string& input, unsigned int& seed) {
    if (input.empty() || input.size() > 10) {
        return false;
    }
    unsigned long long value = 0;
    for (char ch : input) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        const unsigned int digit = static_cast<unsigned int>(ch - '0');
        if (value > (4294967295ULL - digit) / 10ULL) {
            return false;
        }
        value = value * 10ULL + digit;
    }
    if (value < 1 || value > MAX_SEED) {
        return false;
    }
    seed = static_cast<unsigned int>(value);
    return true;
}
void drainCharQueue() {
    while (GetCharPressed() > 0) {
    }
}
void adjust(float& value, float delta, float lo, float hi) {
    value = std::clamp(value + delta, lo, hi);
}
}
int main() {
    WorldSettings settings;
    settings.width = 256;
    settings.height = 256;
    settings.seed = 1;
    resetWorldSettings(settings);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "TERRA - Procedural World Generator");
    SetExitKey(KEY_NULL);
    initPersistentStorage();
    SetTargetFPS(60);
    MapView view = MapView::Terrain;
    World world = generateWorld(settings);
    RenderTexture2D worldTexture = createWorldTexture(world, settings, view);
    RenderTexture2D gridTexture = createGridTexture(settings.width, settings.height);
    Camera2D camera{};
    camera.offset = {SCREEN_WIDTH / 2.0f, VIEW_HEIGHT / 2.0f};
    camera.rotation = 0.0f;
    centerCamera(camera, settings);
    bool showGrid = false;
    bool quit = false;
    Overlay overlay = Overlay::None;
    std::string saveName;
    std::string seedInput;
    std::string status = "Ready";
    std::string worldName;
    bool worldModified = false;
    std::vector<SavedWorld> savedWorlds;
    int selectedSave = 0;
    int deleteArmed = -1;
    auto redrawTexture = [&]() {
        UnloadRenderTexture(worldTexture);
        worldTexture = createWorldTexture(world, settings, view);
    };
    auto regenerateWorld = [&](bool recenter = false) {
        world = generateWorld(settings);
        redrawTexture();
        if (recenter) {
            centerCamera(camera, settings);
        } else {
            clampCamera(camera, settings);
        }
    };
    auto changeSeed = [&](unsigned int seed, const char* message, bool recenter) {
        settings.seed = seed;
        resetWorldSettings(settings);
        worldName.clear();
        worldModified = false;
        regenerateWorld(recenter);
        status = message;
    };
    auto openLoadBrowser = [&]() {
        savedWorlds = listSavedWorlds();
        selectedSave = 0;
        deleteArmed = -1;
        overlay = Overlay::Load;
        status = savedWorlds.empty() ? "No saved worlds yet" : "Choose a world";
    };
    auto frame = [&]() {
        const float dt = GetFrameTime();
        if (overlay == Overlay::Save) {
            int ch = GetCharPressed();
            while (ch > 0) {
                if (ch >= 32 && ch <= 126 && saveName.size() < 60) {
                    saveName += static_cast<char>(ch);
                }
                ch = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && !saveName.empty()) {
                saveName.pop_back();
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                overlay = Overlay::None;
                status = "Save cancelled";
            } else if (IsKeyPressed(KEY_ENTER)) {
                if (saveName.empty()) {
                    status = "Enter a name first";
                } else if (saveNamedWorld(saveName, settings)) {
                    status = "World saved";
                    overlay = Overlay::None;
                    worldName = saveName;
                    worldModified = false;
                    saveName.clear();
                } else {
                    status = "Save failed";
                }
            }
        } else if (overlay == Overlay::SeedSearch) {
            int ch = GetCharPressed();
            while (ch > 0) {
                if (ch >= '0' && ch <= '9' && seedInput.size() < 10) {
                    seedInput += static_cast<char>(ch);
                }
                ch = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && !seedInput.empty()) {
                seedInput.pop_back();
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                overlay = Overlay::None;
                seedInput.clear();
                status = "Seed search cancelled";
            } else if (IsKeyPressed(KEY_ENTER)) {
                unsigned int seed = 0;
                if (parseSeed(seedInput, seed)) {
                    changeSeed(seed, "Seed loaded", true);
                    overlay = Overlay::None;
                    seedInput.clear();
                } else {
                    status = "Invalid seed";
                }
            }
        } else if (overlay == Overlay::Load) {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_L)) {
                overlay = Overlay::None;
                status = "Load cancelled";
            } else if (!savedWorlds.empty()) {
                if (IsKeyPressed(KEY_UP)) {
                    selectedSave = std::max(0, selectedSave - 1);
                    deleteArmed = -1;
                }
                if (IsKeyPressed(KEY_DOWN)) {
                    selectedSave = std::min(
                        static_cast<int>(savedWorlds.size()) - 1,
                        selectedSave + 1
                    );
                    deleteArmed = -1;
                }
                if (IsKeyPressed(KEY_ENTER)) {
                    const SavedWorld chosen = savedWorlds[selectedSave];
                    WorldSettings loaded = settings;
                    if (loadNamedWorld(chosen.filename, loaded)) {
                        loaded.width = 256;
                        loaded.height = 256;
                        settings = loaded;
                        worldName = chosen.name;
                        worldModified = false;
                        regenerateWorld(true);
                        status = "World loaded";
                        overlay = Overlay::None;
                    } else {
                        status = "Could not load world";
                        savedWorlds = listSavedWorlds();
                        selectedSave = 0;
                    }
                }
                if (!savedWorlds.empty() &&
                    (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE))) {
                    if (deleteArmed != selectedSave) {
                        deleteArmed = selectedSave;
                        status = "Press DELETE again to confirm";
                    } else if (deleteSavedWorld(savedWorlds[selectedSave].filename)) {
                        status = "Saved world deleted";
                        savedWorlds = listSavedWorlds();
                        deleteArmed = -1;
                        selectedSave = std::min(
                            selectedSave,
                            std::max(0, static_cast<int>(savedWorlds.size()) - 1)
                        );
                    } else {
                        status = "Delete failed";
                    }
                }
            }
        } else {
#if !defined(__EMSCRIPTEN__)
            if (IsKeyPressed(KEY_ESCAPE)) {
                quit = true;
            }
#endif
            if (IsKeyPressed(KEY_K)) {
                saveName.clear();
                overlay = Overlay::Save;
                drainCharQueue();
            }
            if (IsKeyPressed(KEY_L)) {
                openLoadBrowser();
            }
            if (IsKeyPressed(KEY_F)) {
                seedInput.clear();
                overlay = Overlay::SeedSearch;
                drainCharQueue();
            }
            if (IsKeyPressed(KEY_G)) {
                showGrid = !showGrid;
            }
            if (IsKeyPressed(KEY_M)) {
                view = static_cast<MapView>((static_cast<int>(view) + 1) % 4);
                redrawTexture();
                status = TextFormat("View: %s", mapViewName(view));
            }
            if (IsKeyPressed(KEY_P)) {
                Image image = LoadImageFromTexture(worldTexture.texture);
                ImageFlipVertical(&image);
                const std::string file = TextFormat("terra_seed_%u.png", settings.seed);
                const bool exported = ExportImage(image, file.c_str());
                status = exported ? "Saved " + file : "PNG export failed";
                if (exported) {
                    offerDownload(file);
                }
                UnloadImage(image);
            }
            const float minZoom = std::max(
                static_cast<float>(SCREEN_WIDTH) / (settings.width * TILE_SIZE),
                static_cast<float>(VIEW_HEIGHT) / (settings.height * TILE_SIZE)
            );
            if (IsKeyDown(KEY_Q)) camera.zoom += 1.5f * dt;
            if (IsKeyDown(KEY_E)) camera.zoom -= 1.5f * dt;
            const float wheel = GetMouseWheelMove();
            if (wheel != 0.0f) {
                camera.zoom *= 1.0f + wheel * 0.10f;
            }
            camera.zoom = std::clamp(camera.zoom, minZoom, 3.0f);
            const float speed = 400.0f * dt / camera.zoom;
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    camera.target.y -= speed;
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  camera.target.y += speed;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  camera.target.x -= speed;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) camera.target.x += speed;
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
                GetMouseY() < VIEW_HEIGHT) {
                const Vector2 delta = GetMouseDelta();
                camera.target.x -= delta.x / camera.zoom;
                camera.target.y -= delta.y / camera.zoom;
            }
            clampCamera(camera, settings);
            if (IsKeyPressed(KEY_Z)) {
                changeSeed(settings.seed <= 1 ? MAX_SEED : settings.seed - 1,
                           "Previous seed", false);
            }
            if (IsKeyPressed(KEY_C)) {
                changeSeed(settings.seed >= MAX_SEED ? 1 : settings.seed + 1,
                           "Next seed", false);
            }
            if (IsKeyPressed(KEY_R)) {
                changeSeed(static_cast<unsigned int>(GetRandomValue(1, 1000000)),
                           "Random world generated", false);
            }
            bool changed = false;
            auto slider = [&](int downKey, int upKey, float& value,
                              float step, float lo, float hi) {
                if (IsKeyPressed(downKey)) { adjust(value, -step, lo, hi); changed = true; }
                if (IsKeyPressed(upKey))   { adjust(value,  step, lo, hi); changed = true; }
            };
            slider(KEY_ONE,   KEY_TWO,    settings.waterLevel,        0.05f, 0.10f, 0.90f);
            slider(KEY_THREE, KEY_FOUR,   settings.mountainIntensity, 0.10f, 0.0f,  1.0f);
            slider(KEY_FIVE,  KEY_SIX,    settings.forestDensity,     0.10f, 0.0f,  1.0f);
            slider(KEY_SEVEN, KEY_EIGHT,  settings.temperature,       0.10f, 0.0f,  1.0f);
            slider(KEY_NINE,  KEY_ZERO,   settings.rainfall,          0.10f, 0.0f,  1.0f);
            slider(KEY_COMMA, KEY_PERIOD, settings.biomeVariation,    0.10f, 0.0f,  1.0f);
            if (changed) {
                worldModified = true;
                regenerateWorld();
            }
        }
        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode2D(camera);
        drawCachedTexture(
            worldTexture,
            settings.width * TILE_SIZE,
            settings.height * TILE_SIZE
        );
        if (showGrid) {
            drawCachedTexture(
                gridTexture,
                settings.width * TILE_SIZE,
                settings.height * TILE_SIZE
            );
        }
        EndMode2D();
        DrawRectangle(0, VIEW_HEIGHT, SCREEN_WIDTH, HUD_HEIGHT, {25, 28, 35, 255});
        DrawLine(0, VIEW_HEIGHT, SCREEN_WIDTH, VIEW_HEIGHT, {75, 85, 100, 255});
        std::string title = TextFormat(
            "TERRA | Seed: %u | World: %d x %d | View: %s",
            settings.seed, settings.width, settings.height,
            mapViewName(view)
        );
        if (!worldName.empty()) {
            title += " | \"" + worldName + "\"";
            if (worldModified) title += " (modified)";
        }
        DrawText(title.c_str(), 12, VIEW_HEIGHT + 4, 15, RAYWHITE);
        DrawText(
            TextFormat("1/2 Water -/+ %.0f%%   3/4 Mountains -/+ %.0f%%",
                       settings.waterLevel * 100.0f,
                       settings.mountainIntensity * 100.0f),
            12, VIEW_HEIGHT + 21, 12, LIGHTGRAY
        );
        DrawText(
            TextFormat("5/6 Forest -/+ %.0f%%   7/8 Temperature -/+ %.0f%%",
                       settings.forestDensity * 100.0f,
                       settings.temperature * 100.0f),
            12, VIEW_HEIGHT + 38, 12, LIGHTGRAY
        );
        DrawText(
            TextFormat("9/0 Rainfall -/+ %.0f%%   ,/. Biome variation -/+ %.0f%%",
                       settings.rainfall * 100.0f,
                       settings.biomeVariation * 100.0f),
            12, VIEW_HEIGHT + 55, 12, LIGHTGRAY
        );
        DrawText(
            "WASD/Drag Move | Q/E/Wheel Zoom | Z/C Seeds | R Random | F Find | G Grid | M View | P PNG",
            12, VIEW_HEIGHT + 72, 12, LIGHTGRAY
        );
#if defined(__EMSCRIPTEN__)
        DrawText("K Save | L Load | ESC Close Menu", 12, VIEW_HEIGHT + 86, 11, GREEN);
#else
        DrawText("K Save | L Load | ESC Close Menu / Quit", 12, VIEW_HEIGHT + 86, 11, GREEN);
#endif
        if (overlay == Overlay::None && GetMouseY() < VIEW_HEIGHT) {
            const Vector2 p = GetScreenToWorld2D(GetMousePosition(), camera);
            const int tx = static_cast<int>(std::floor(p.x / TILE_SIZE));
            const int ty = static_cast<int>(std::floor(p.y / TILE_SIZE));
            if (tx >= 0 && ty >= 0 && tx < settings.width && ty < settings.height) {
                const Tile& t = world[tileIndex(tx, ty, settings.width)];
                const char* name = t.isLake ? "Lake"
                                 : t.isRiver ? "River"
                                 : t.decoration == Decoration::Volcano ? "Volcano (active)"
                                 : t.decoration == Decoration::ExtinctVolcano ? "Volcano (extinct)"
                                 : terrainName(t.terrain);
                DrawText(TextFormat("Tile %d, %d: %s", tx, ty, name),
                         700, VIEW_HEIGHT + 21, 12, RAYWHITE);
                const int tempC = static_cast<int>(std::lround(t.temperature));
                const float shownHeight = std::fabs(t.height) < 0.005f ? 0.0f : t.height;
                DrawText(TextFormat("Elevation %+.2f   Temp %d C   Moisture %.0f%%",
                                    shownHeight,
                                    tempC,
                                    t.moisture * 100.0f),
                         700, VIEW_HEIGHT + 38, 12, LIGHTGRAY);
                if (t.height > 0.0f) {
                    DrawText(TextFormat("Rainfall ~%d mm/yr",
                                        static_cast<int>(std::lround(t.rainfallMm / 10.0f)) * 10),
                             700, VIEW_HEIGHT + 55, 12, LIGHTGRAY);
                }
            }
        }
        if (overlay == Overlay::None && !status.empty()) {
            DrawText(status.c_str(), 700, VIEW_HEIGHT + 86, 11, YELLOW);
        }
        if (overlay != Overlay::None) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, {0, 0, 0, 175});
            DrawRectangle(290, 180, 520, 390, {32, 38, 48, 255});
            DrawRectangleLines(290, 180, 520, 390, {110, 135, 160, 255});
            if (overlay == Overlay::Save) {
                DrawText("SAVE WORLD", 315, 205, 24, RAYWHITE);
                DrawText("Give your world a name:", 315, 250, 16, LIGHTGRAY);
                DrawRectangle(315, 290, 470, 42, {18, 22, 30, 255});
                DrawRectangleLines(315, 290, 470, 42, SKYBLUE);
                DrawText(TextFormat("%s_", saveName.c_str()), 327, 302, 18, YELLOW);
                DrawText(TextFormat("Seed: %u", settings.seed), 315, 355, 15, LIGHTGRAY);
                DrawText("ENTER Save | BACKSPACE Delete character", 315, 420, 14, RAYWHITE);
                DrawText("ESC Cancel", 315, 450, 14, LIGHTGRAY);
                if (!status.empty() && status == "Enter a name first") {
                    DrawText(status.c_str(), 315, 380, 14, YELLOW);
                }
            } else if (overlay == Overlay::SeedSearch) {
                DrawText("FIND SEED", 315, 205, 24, RAYWHITE);
                DrawText("Enter a seed between 1 and 4294967295:", 315, 250, 15, LIGHTGRAY);
                DrawRectangle(315, 290, 470, 42, {18, 22, 30, 255});
                DrawRectangleLines(315, 290, 470, 42, SKYBLUE);
                DrawText(TextFormat("%s_", seedInput.c_str()), 327, 302, 18, YELLOW);
                DrawText("ENTER Load seed | ESC Cancel", 315, 420, 14, RAYWHITE);
                if (status == "Invalid seed") {
                    DrawText(status.c_str(), 315, 380, 14, YELLOW);
                }
            } else {
                DrawText("LOAD WORLD", 315, 205, 24, RAYWHITE);
                DrawText("UP/DOWN Select | ENTER Load", 315, 245, 14, LIGHTGRAY);
                DrawText("DELETE Delete (press twice) | ESC Close", 315, 266, 13, LIGHTGRAY);
                if (savedWorlds.empty()) {
                    DrawText("No saved worlds. Press K to save one.", 315, 325, 15, YELLOW);
                } else {
                    const int visible = std::min(8, static_cast<int>(savedWorlds.size()));
                    int first = std::max(0, selectedSave - visible + 1);
                    first = std::min(
                        first,
                        std::max(0, static_cast<int>(savedWorlds.size()) - visible)
                    );
                    for (int row = 0; row < visible; ++row) {
                        const int index = first + row;
                        const int y = 300 + row * 29;
                        if (index == selectedSave) {
                            DrawRectangle(310, y - 3, 480, 26, {65, 90, 115, 255});
                        }
                        DrawText(
                            TextFormat("%s | Seed %u",
                                       savedWorlds[index].name.c_str(),
                                       savedWorlds[index].seed),
                            320, y, 14,
                            index == selectedSave ? YELLOW : RAYWHITE
                        );
                    }
                    if (deleteArmed >= 0) {
                        DrawText("Press DELETE again to confirm", 315, 540, 13, YELLOW);
                    }
                }
            }
        }
        EndDrawing();
    };
#if defined(__EMSCRIPTEN__)
    static std::function<void()> webFrame;
    webFrame = frame;
    emscripten_set_main_loop([]() { webFrame(); }, 0, 1);
#else
    while (!WindowShouldClose() && !quit) {
        frame();
    }
#endif
    UnloadRenderTexture(worldTexture);
    UnloadRenderTexture(gridTexture);
    CloseWindow();
    return 0;
}
