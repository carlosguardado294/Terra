#pragma once
#include "raylib.h"
#include <algorithm>
#include <cstddef>
#include <vector>
constexpr int TILE_SIZE = 8;
constexpr unsigned int MAX_SEED = 4294967295u;
enum class Terrain {
    DeepWater,
    ShallowWater,
    Sand,
    Grass,
    Forest,
    Wetland,
    Plains,
    Desert,
    Hill,
    Mountain,
    Snow,
    Tundra,
    Taiga,
    Savanna,
    Jungle,
    Ice
};
enum class Decoration {
    None,
    Tree,
    PineTree,
    Flower,
    Rock,
    Cactus,
    Volcano,
    ExtinctVolcano
};
struct Tile {
    Terrain terrain = Terrain::DeepWater;
    float height = 0.0f;
    float temperature = 10.0f;
    float airTemperature = 10.0f;
    float surfaceTemperature = 10.0f;
    float rainfallMm = 0.0f;
    float moisture = 0.5f;
    float shade = 0.5f;
    Decoration decoration = Decoration::None;
    unsigned int variation = 0;
    bool isRiver = false;
    bool isLake = false;
    unsigned char riverWidth = 0;
};
using World = std::vector<Tile>;
struct WorldSettings {
    int width = 256;
    int height = 256;
    unsigned int seed = 1;
    float waterLevel = 0.50f;
    float mountainIntensity = 0.50f;
    float forestDensity = 0.70f;
    float temperature = 0.50f;
    float rainfall = 0.55f;
    float biomeVariation = 0.65f;
};
std::size_t tileIndex(int x, int y, int width);
unsigned int hashPosition(
    int x,
    int y,
    unsigned int seed
);
float randomValue(
    int x,
    int y,
    unsigned int seed
);
Color terrainColor(Terrain terrain);
const char* terrainName(Terrain terrain);
World generateWorld(const WorldSettings& settings);
void resetWorldSettings(WorldSettings& settings);
