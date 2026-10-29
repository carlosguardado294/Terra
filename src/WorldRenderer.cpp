#include "WorldRenderer.h"
#include <algorithm>
#include <cmath>
namespace {
float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
unsigned char toByte(float v) {
    return static_cast<unsigned char>(std::clamp(v, 0.0f, 255.0f));
}
Color scaleColor(Color c, float f) {
    return {toByte(c.r * f), toByte(c.g * f), toByte(c.b * f), 255};
}
Color mixColor(Color a, Color b, float t) {
    t = clamp01(t);
    return {
        toByte(a.r + (b.r - a.r) * t),
        toByte(a.g + (b.g - a.g) * t),
        toByte(a.b + (b.b - a.b) * t),
        255
    };
}
template <std::size_t N>
Color gradientColor(const Color (&stops)[N], float t) {
    t = clamp01(t) * static_cast<float>(N - 1);
    const std::size_t i = std::min(static_cast<std::size_t>(t), N - 2);
    return mixColor(stops[i], stops[i + 1], t - static_cast<float>(i));
}
Color riverColor(unsigned char width) {
    if (width >= 2) return {40, 120, 195, 255};
    return {55, 145, 210, 255};
}
Color lakeColor() { return {50, 140, 205, 255}; }
bool isOpenWater(const Tile& t) {
    return t.height <= 0.0f || t.isLake || t.isRiver;
}
Color oceanColor(const Tile& tile) {
    if (tile.terrain == Terrain::Ice) return terrainColor(Terrain::Ice);
    const float depth = clamp01(-tile.height);
    return mixColor({55, 135, 190, 255}, {16, 52, 112, 255}, depth * 1.7f);
}
Color baseColor(const Tile& tile, MapView view) {
    if (view == MapView::Terrain) {
        if (tile.isLake) return lakeColor();
        if (tile.isRiver) return riverColor(tile.riverWidth);
        if (tile.height <= 0.0f) return oceanColor(tile);
        Color c = terrainColor(tile.terrain);
        const float jitter =
            (static_cast<float>((tile.variation >> 3) & 15u) - 7.5f) / 7.5f * 0.03f;
        const float shade = 0.74f + 0.52f * tile.shade;
        return scaleColor(c, (1.0f + jitter) * shade);
    }
    if (tile.height <= 0.0f) {
        if (view == MapView::Temperature) {
            static const Color seaStops[] = {
                {50, 70, 190, 255}, {70, 190, 215, 255}, {100, 195, 95, 255},
                {240, 220, 70, 255}, {225, 70, 45, 255}
            };
            return scaleColor(gradientColor(seaStops, (tile.temperature + 30.0f) / 65.0f), 0.80f);
        }
        Color c = oceanColor(tile);
        return scaleColor(c, 0.65f);
    }
    if (view == MapView::Elevation) {
        static const Color stops[] = {
            {70, 140, 70, 255}, {150, 175, 90, 255}, {185, 150, 90, 255},
            {140, 115, 105, 255}, {250, 250, 250, 255}
        };
        return scaleColor(gradientColor(stops, tile.height),
                          0.80f + 0.40f * tile.shade);
    }
    if (view == MapView::Temperature) {
        static const Color stops[] = {
            {50, 70, 190, 255}, {70, 190, 215, 255}, {100, 195, 95, 255},
            {240, 220, 70, 255}, {225, 70, 45, 255}
        };
        return gradientColor(stops, (tile.temperature + 30.0f) / 65.0f);
    }
    static const Color stops[] = {
        {195, 155, 90, 255}, {205, 200, 105, 255}, {85, 175, 85, 255},
        {30, 120, 150, 255}, {20, 60, 140, 255}
    };
    return gradientColor(stops, tile.moisture / 0.75f);
}
void drawTerrainDetail(int x, int y, const Tile& tile) {
    const int px = x * TILE_SIZE;
    const int py = y * TILE_SIZE;
    const unsigned int v = tile.variation;
    if (tile.terrain == Terrain::Grass && v % 5 == 0) {
        DrawPixel(px + 2 + v % 5, py + 2 + (v >> 8) % 5, {70, 145, 65, 255});
    }
    if ((tile.terrain == Terrain::Savanna || tile.terrain == Terrain::Plains) &&
        v % 4 == 0) {
        DrawPixel(px + 2 + v % 5, py + 2 + (v >> 8) % 5, {150, 140, 60, 255});
    }
    if (tile.terrain == Terrain::Tundra && v % 5 == 0) {
        DrawPixel(px + 2 + v % 5, py + 2 + (v >> 8) % 5, {115, 125, 105, 255});
    }
    if (tile.terrain == Terrain::Sand && v % 7 == 0) {
        DrawPixel(px + 2 + v % 5, py + 2 + (v >> 8) % 5, {245, 225, 170, 255});
    }
    if (tile.terrain == Terrain::Desert && v % 6 == 0) {
        DrawLine(px + 1, py + 4, px + 6, py + 4, {225, 190, 115, 255});
    }
    if (tile.terrain == Terrain::Wetland && v % 3 == 0) {
        DrawPixel(px + 2 + v % 5, py + 2 + (v >> 8) % 5, {110, 155, 115, 255});
        if (v % 7 == 0) {
            DrawPixel(px + 5, py + 5, {65, 115, 100, 255});
        }
    }
    if (tile.terrain == Terrain::Hill && v % 4 == 0) {
        DrawLine(px + 2, py + 5, px + 5, py + 2, {125, 150, 90, 255});
    }
    if (tile.terrain == Terrain::Mountain && v % 3 == 0) {
        DrawLine(px + 2, py + 5, px + 5, py + 2, {155, 155, 150, 255});
    }
    if (tile.terrain == Terrain::Snow && v % 4 == 0) {
        DrawPixel(px + 2 + v % 5, py + 2 + (v >> 8) % 5, {255, 255, 255, 255});
    }
    if ((tile.terrain == Terrain::DeepWater ||
         tile.terrain == Terrain::ShallowWater) && v % 6 == 0) {
        DrawLine(
            px + 1, py + TILE_SIZE / 2,
            px + TILE_SIZE - 2, py + TILE_SIZE / 2,
            tile.terrain == Terrain::DeepWater
                ? Color{40, 95, 165, 255}
                : Color{90, 165, 205, 255}
        );
    }
    const float cx = px + TILE_SIZE / 2.0f;
    const float cy = py + TILE_SIZE / 2.0f;
    switch (tile.decoration) {
        case Decoration::Tree:
            DrawCircle(static_cast<int>(cx + 1), static_cast<int>(cy + 2), 3.2f, {35, 75, 35, 255});
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy), 2.9f, {30, 105, 45, 255});
            DrawCircle(static_cast<int>(cx - 1), static_cast<int>(cy - 1), 1.8f, {55, 145, 60, 255});
            break;
        case Decoration::PineTree:
            DrawTriangle(
                {cx, py + 1.0f},
                {px + 1.0f, py + TILE_SIZE - 1.0f},
                {px + TILE_SIZE - 1.0f, py + TILE_SIZE - 1.0f},
                {25, 85, 45, 255}
            );
            DrawTriangle(
                {cx, py + 2.0f},
                {px + 2.0f, py + TILE_SIZE * 0.65f},
                {px + TILE_SIZE - 2.0f, py + TILE_SIZE * 0.65f},
                {45, 125, 55, 255}
            );
            break;
        case Decoration::Flower: {
            const Color colors[] = {
                {245, 100, 115, 255}, {245, 220, 100, 255}, {235, 235, 245, 255}
            };
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy), 1.8f, colors[v % 3]);
            break;
        }
        case Decoration::Rock:
            DrawCircle(static_cast<int>(cx + 1), static_cast<int>(cy + 1), 2.4f, {85, 85, 95, 255});
            DrawCircle(static_cast<int>(cx - 1), static_cast<int>(cy - 1), 1.7f, {165, 165, 175, 255});
            break;
        case Decoration::Cactus:
            DrawRectangle(static_cast<int>(cx - 1), static_cast<int>(cy - 3), 3, 6, {40, 125, 65, 255});
            DrawRectangle(static_cast<int>(cx - 3), static_cast<int>(cy - 1), 2, 3, {40, 125, 65, 255});
            DrawRectangle(static_cast<int>(cx + 1), static_cast<int>(cy - 2), 2, 3, {40, 125, 65, 255});
            break;
        case Decoration::Volcano:
            DrawTriangle(
                {cx, py - 4.0f},
                {px + 0.0f, py + TILE_SIZE - 0.0f},
                {px + TILE_SIZE - 0.0f, py + TILE_SIZE - 0.0f},
                {58, 44, 40, 255}
            );
            DrawTriangle(
                {cx, py - 3.0f},
                {px + 1.0f, py + TILE_SIZE - 0.0f},
                {cx + 1.0f, py + TILE_SIZE - 0.0f},
                {125, 98, 82, 255}
            );
            DrawRectangle(static_cast<int>(cx - 2), py - 3, 4, 1, {28, 20, 20, 255});
            DrawPixel(static_cast<int>(cx), py - 3, {205, 95, 40, 255});
            DrawCircle(static_cast<int>(cx), py - 5, 1.2f, {215, 215, 220, 150});
            DrawCircle(static_cast<int>(cx + 1), py - 7, 1.0f, {215, 215, 220, 90});
            break;
        case Decoration::ExtinctVolcano:
            DrawTriangle(
                {cx, py - 4.0f},
                {px + 0.0f, py + TILE_SIZE - 0.0f},
                {px + TILE_SIZE - 0.0f, py + TILE_SIZE - 0.0f},
                {66, 64, 60, 255}
            );
            DrawTriangle(
                {cx, py - 3.0f},
                {px + 1.0f, py + TILE_SIZE - 0.0f},
                {cx + 1.0f, py + TILE_SIZE - 0.0f},
                {138, 135, 126, 255}
            );
            DrawRectangle(static_cast<int>(cx - 2), py - 3, 4, 1, {45, 42, 42, 255});
            DrawPixel(static_cast<int>(cx), py - 3, {60, 145, 200, 255});
            break;
        case Decoration::None:
            break;
    }
}
}
const char* mapViewName(MapView view) {
    switch (view) {
        case MapView::Terrain:     return "Terrain";
        case MapView::Elevation:   return "Elevation";
        case MapView::Temperature: return "Temperature";
        case MapView::Moisture:    return "Moisture";
    }
    return "?";
}
RenderTexture2D createWorldTexture(
    const World& world,
    const WorldSettings& settings,
    MapView view
) {
    const int w = settings.width;
    const int h = settings.height;
    std::vector<Color> colors(world.size());
    for (std::size_t i = 0; i < world.size(); ++i) {
        colors[i] = baseColor(world[i], view);
    }
    if (view == MapView::Terrain) {
        std::vector<Color> smoothed = colors;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const std::size_t i = tileIndex(x, y, w);
                if (isOpenWater(world[i])) continue;
                const int dx[4] = {1, -1, 0, 0};
                const int dy[4] = {0, 0, 1, -1};
                float r = 0, g = 0, b = 0;
                int n = 0;
                for (int d = 0; d < 4; ++d) {
                    const int nx = x + dx[d], ny = y + dy[d];
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    const std::size_t j = tileIndex(nx, ny, w);
                    if (isOpenWater(world[j])) continue;
                    r += colors[j].r; g += colors[j].g; b += colors[j].b;
                    ++n;
                }
                if (n > 0) {
                    const Color avg = {toByte(r / n), toByte(g / n), toByte(b / n), 255};
                    smoothed[i] = mixColor(colors[i], avg, 0.40f);
                }
            }
        }
        colors.swap(smoothed);
    }
    RenderTexture2D texture =
        LoadRenderTexture(w * TILE_SIZE, h * TILE_SIZE);
    BeginTextureMode(texture);
    ClearBackground(BLACK);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t i = tileIndex(x, y, w);
            DrawRectangle(
                x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, colors[i]
            );
            if (view == MapView::Terrain && !world[i].isLake && !world[i].isRiver) {
                drawTerrainDetail(x, y, world[i]);
            }
        }
    }
    EndTextureMode();
    SetTextureFilter(texture.texture, TEXTURE_FILTER_POINT);
    return texture;
}
RenderTexture2D createGridTexture(int width, int height) {
    const int pixelWidth = width * TILE_SIZE;
    const int pixelHeight = height * TILE_SIZE;
    RenderTexture2D texture = LoadRenderTexture(pixelWidth, pixelHeight);
    BeginTextureMode(texture);
    ClearBackground({0, 0, 0, 0});
    const Color gridColor = {0, 0, 0, 65};
    for (int x = 0; x <= width; ++x) {
        DrawLine(x * TILE_SIZE, 0, x * TILE_SIZE, pixelHeight, gridColor);
    }
    for (int y = 0; y <= height; ++y) {
        DrawLine(0, y * TILE_SIZE, pixelWidth, y * TILE_SIZE, gridColor);
    }
    EndTextureMode();
    SetTextureFilter(texture.texture, TEXTURE_FILTER_POINT);
    return texture;
}
void drawCachedTexture(
    const RenderTexture2D& texture,
    int width,
    int height
) {
    const Rectangle source = {
        0.0f, 0.0f,
        static_cast<float>(width), -static_cast<float>(height)
    };
    const Rectangle destination = {
        0.0f, 0.0f,
        static_cast<float>(width), static_cast<float>(height)
    };
    DrawTexturePro(texture.texture, source, destination, {0.0f, 0.0f}, 0.0f, WHITE);
}
