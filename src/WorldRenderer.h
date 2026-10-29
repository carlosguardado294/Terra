#pragma once
#include "WorldGenerator.h"
#include "raylib.h"
enum class MapView {
    Terrain,
    Elevation,
    Temperature,
    Moisture
};
const char* mapViewName(MapView view);
RenderTexture2D createWorldTexture(
    const World& world,
    const WorldSettings& settings,
    MapView view = MapView::Terrain
);
RenderTexture2D createGridTexture(
    int width,
    int height
);
void drawCachedTexture(
    const RenderTexture2D& texture,
    int width,
    int height
);
