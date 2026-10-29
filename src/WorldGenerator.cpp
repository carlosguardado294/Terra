#include "WorldGenerator.h"
#include <array>
#include <cmath>
#include <functional>
#include <queue>
#include <utility>
Color terrainColor(Terrain terrain) {
    switch (terrain) {
        case Terrain::DeepWater:    return {22, 65, 125, 255};
        case Terrain::ShallowWater: return {45, 120, 175, 255};
        case Terrain::Sand:         return {225, 205, 145, 255};
        case Terrain::Grass:        return {100, 170, 80, 255};
        case Terrain::Forest:       return {40, 110, 55, 255};
        case Terrain::Mountain:     return {125, 120, 120, 255};
        case Terrain::Snow:         return {235, 240, 245, 255};
        case Terrain::Desert:       return {215, 175, 95, 255};
        case Terrain::Plains:       return {150, 185, 95, 255};
        case Terrain::Wetland:      return {85, 135, 105, 255};
        case Terrain::Hill:         return {130, 155, 90, 255};
        case Terrain::Tundra:       return {150, 160, 135, 255};
        case Terrain::Taiga:        return {55, 105, 75, 255};
        case Terrain::Savanna:      return {185, 175, 85, 255};
        case Terrain::Jungle:       return {25, 95, 45, 255};
        case Terrain::Ice:          return {200, 225, 240, 255};
    }
    return MAGENTA;
}
const char* terrainName(Terrain terrain) {
    switch (terrain) {
        case Terrain::DeepWater:    return "Deep ocean";
        case Terrain::ShallowWater: return "Shallow sea";
        case Terrain::Sand:         return "Beach";
        case Terrain::Grass:        return "Grassland";
        case Terrain::Forest:       return "Temperate forest";
        case Terrain::Mountain:     return "Mountain";
        case Terrain::Snow:         return "Snow / ice cap";
        case Terrain::Desert:       return "Desert";
        case Terrain::Plains:       return "Steppe";
        case Terrain::Wetland:      return "Wetland";
        case Terrain::Hill:         return "Hills";
        case Terrain::Tundra:       return "Tundra";
        case Terrain::Taiga:        return "Taiga";
        case Terrain::Savanna:      return "Savanna";
        case Terrain::Jungle:       return "Rainforest";
        case Terrain::Ice:          return "Sea ice";
    }
    return "?";
}
void resetWorldSettings(WorldSettings& settings) {
    settings.waterLevel = 0.50f;
    settings.mountainIntensity = 0.50f;
    settings.forestDensity = 0.70f;
    settings.temperature = 0.50f;
    settings.rainfall = 0.55f;
    settings.biomeVariation = 0.65f;
}
unsigned int hashPosition(int x, int y, unsigned int seed) {
    auto mix = [](unsigned int h) {
        h = (h ^ (h >> 16)) * 0x7FEB352Du;
        h = (h ^ (h >> 15)) * 0x846CA68Bu;
        return h ^ (h >> 16);
    };
    unsigned int h = seed * 0x9E3779B1u;
    h = mix(h ^ (static_cast<unsigned int>(x) * 0x85EBCA6Bu));
    h = mix(h ^ (static_cast<unsigned int>(y) * 0xC2B2AE35u));
    return h;
}
float randomValue(int x, int y, unsigned int seed) {
    return static_cast<float>(hashPosition(x, y, seed)) / 4294967295.0f;
}
std::size_t tileIndex(int x, int y, int width) {
    return static_cast<std::size_t>(y) * width + x;
}
namespace {
constexpr float TERRA_PI = 3.14159265f;
constexpr float ICE_AIR_TEMP = -14.0f;
constexpr float FREEZING_SEA = -1.8f;
float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
float lerpf(float a, float b, float t) { return a + (b - a) * t; }
float smooth01(float t) {
    t = clamp01(t);
    return t * t * (3.0f - 2.0f * t);
}
float remap(float v, float lo, float hi) {
    return smooth01((v - lo) / (hi - lo));
}
float fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}
struct Grad { float x, y; };
const std::array<Grad, 16>& gradients() {
    static const std::array<Grad, 16> table = [] {
        std::array<Grad, 16> g{};
        for (int i = 0; i < 16; ++i) {
            const float a = static_cast<float>(i) * 2.0f * TERRA_PI / 16.0f;
            g[i] = {std::cos(a), std::sin(a)};
        }
        return g;
    }();
    return table;
}
float gradDot(int ix, int iy, float dx, float dy, unsigned int seed) {
    const Grad& g = gradients()[hashPosition(ix, iy, seed) & 15u];
    return g.x * dx + g.y * dy;
}
float perlin(float x, float y, unsigned int seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    const float u = fade(fx);
    const float v = fade(fy);
    const float n00 = gradDot(x0,     y0,     fx,        fy,        seed);
    const float n10 = gradDot(x0 + 1, y0,     fx - 1.0f, fy,        seed);
    const float n01 = gradDot(x0,     y0 + 1, fx,        fy - 1.0f, seed);
    const float n11 = gradDot(x0 + 1, y0 + 1, fx - 1.0f, fy - 1.0f, seed);
    return lerpf(lerpf(n00, n10, u), lerpf(n01, n11, u), v);
}
float fbm(float x, float y, unsigned int seed, int octaves, float gain = 0.5f) {
    float sum = 0.0f, amp = 1.0f, norm = 0.0f, freq = 1.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * perlin(
            x * freq + 37.1f * static_cast<float>(i),
            y * freq - 19.7f * static_cast<float>(i),
            seed + static_cast<unsigned int>(i) * 977u
        );
        norm += amp;
        amp *= gain;
        freq *= 2.0f;
    }
    return clamp01(0.5f + 1.15f * sum / norm);
}
float quantileOf(std::vector<float> values, float q) {
    if (values.empty()) return 0.0f;
    const std::size_t k = static_cast<std::size_t>(
        clamp01(q) * static_cast<float>(values.size() - 1)
    );
    std::nth_element(values.begin(), values.begin() + k, values.end());
    return values[k];
}
bool isWaterTile(const Tile& t) { return t.height <= 0.0f; }
struct Thresholds {
    float hillStart = 0.5f;
    float mountainStart = 0.7f;
    std::vector<float> volcanoField;
};
Thresholds computeElevation(World& world, const WorldSettings& s) {
    const int w = s.width, h = s.height;
    const std::size_t count = world.size();
    const float mountain = clamp01(s.mountainIntensity);
    std::vector<float> raw(count);
    std::vector<float> site(count);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float u = (x + 0.5f) / w;
            const float v = (y + 0.5f) / h;
            const float wx = fbm(u * 3.0f, v * 3.0f, s.seed + 11, 3) - 0.5f;
            const float wy = fbm(u * 3.0f + 5.2f, v * 3.0f + 1.3f, s.seed + 12, 3) - 0.5f;
            const float pu = u + wx * 0.30f;
            const float pv = v + wy * 0.30f;
            const float continent = fbm(pu * 3.3f, pv * 3.3f, s.seed + 1, 6);
            const float ex = std::abs(2.0f * u - 1.0f);
            const float ey = std::abs(2.0f * v - 1.0f);
            const float edge = std::pow(
                std::pow(ex, 3.0f) + std::pow(ey, 3.0f), 1.0f / 3.0f
            );
            const float falloff = remap(edge, 0.62f, 1.0f);
            const float base = continent - falloff * 0.42f;
            const float ridgeRaw = fbm(pu * 4.0f, pv * 4.0f, s.seed + 2, 5);
            float ridge = 1.0f - std::abs(2.0f * ridgeRaw - 1.0f);
            ridge *= ridge;
            const float rangeMask = remap(
                fbm(pu * 1.6f + 3.0f, pv * 1.6f + 7.0f, s.seed + 3, 2),
                0.42f, 0.66f
            );
            const float inland = remap(base, 0.40f, 0.62f);
            const float mountains =
                ridge * rangeMask * inland * (0.10f + 0.40f * mountain);
            const float detail =
                (fbm(u * 24.0f, v * 24.0f, s.seed + 4, 3) - 0.5f)
                * 0.05f * (0.3f + inland);
            raw[tileIndex(x, y, w)] = base + mountains + detail;
            site[tileIndex(x, y, w)] =
                base + ridge * rangeMask * inland * (0.10f + 0.40f * 0.5f) + detail;
        }
    }
    const float waterFrac = std::clamp(s.waterLevel, 0.10f, 0.90f);
    const float sea = quantileOf(raw, waterFrac);
    float minRaw = raw[0];
    std::vector<float> landRaw;
    landRaw.reserve(count);
    for (float r : raw) {
        minRaw = std::min(minRaw, r);
        if (r > sea) landRaw.push_back(r);
    }
    const float maxRef = landRaw.empty() ? sea + 1.0f : quantileOf(landRaw, 0.998f);
    const float landRange = std::max(maxRef - sea, 1e-4f);
    const float seaRange = std::max(sea - minRaw, 1e-4f);
    std::vector<float> landElevation;
    landElevation.reserve(landRaw.size());
    for (std::size_t i = 0; i < count; ++i) {
        const float r = raw[i];
        float e = r > sea ? (r - sea) / landRange : (r - sea) / seaRange;
        e = std::clamp(e, -1.0f, 1.0f);
        if (e == 0.0f && r <= sea) e = -1e-4f;
        world[i].height = e;
        if (e > 0.0f) landElevation.push_back(e);
    }
    auto elev = [&](int x, int y) {
        x = std::clamp(x, 0, w - 1);
        y = std::clamp(y, 0, h - 1);
        return std::max(0.0f, world[tileIndex(x, y, w)].height);
    };
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float dzdx = elev(x + 1, y) - elev(x - 1, y);
            const float dzdy = elev(x, y + 1) - elev(x, y - 1);
            world[tileIndex(x, y, w)].shade =
                clamp01(0.5f + (dzdx + dzdy) * 5.0f);
        }
    }
    const float mountainFraction = 0.03f + 0.14f * mountain;
    const float hillFraction = 0.16f;
    Thresholds th;
    th.mountainStart = quantileOf(landElevation, 1.0f - mountainFraction);
    th.hillStart = quantileOf(landElevation, 1.0f - mountainFraction - hillFraction);
    th.volcanoField = std::move(site);
    return th;
}
void blurMasked(
    std::vector<float>& values,
    const std::vector<unsigned char>& mask,
    int w, int h, int radius
) {
    std::vector<float> tmp(values.size(), 0.0f);
    for (int pass = 0; pass < 2; ++pass) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const std::size_t i = tileIndex(x, y, w);
                if (!mask[i]) { tmp[i] = 0.0f; continue; }
                float sum = 0.0f;
                int n = 0;
                for (int d = -radius; d <= radius; ++d) {
                    const int nx = pass == 0 ? x + d : x;
                    const int ny = pass == 0 ? y : y + d;
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    const std::size_t j = tileIndex(nx, ny, w);
                    if (!mask[j]) continue;
                    sum += values[j];
                    ++n;
                }
                tmp[i] = n > 0 ? sum / n : 0.0f;
            }
        }
        values.swap(tmp);
    }
}
float rainBand(float lat) {
    auto sq = [](float v) { return v * v; };
    return 0.22f
        + 1.90f * std::exp(-sq(lat / 0.18f))
        + 0.90f * std::exp(-sq((lat - 0.55f) / 0.15f));
}
void computeClimate(World& world, const WorldSettings& s) {
    const int w = s.width, h = s.height;
    const std::size_t count = world.size();
    const float noiseAmp = 0.06f + 0.30f * clamp01(s.biomeVariation);
    const float tempShift = (clamp01(s.temperature) - 0.5f) * 28.0f;
    const float rainFactor = std::exp((clamp01(s.rainfall) - 0.55f) * 1.8f);
    for (int y = 0; y < h; ++y) {
        const float v = (y + 0.5f) / h;
        const float lat = std::abs(2.0f * v - 1.0f);
        const float cosLat = std::cos(lat * TERRA_PI * 0.5f);
        for (int x = 0; x < w; ++x) {
            const float u = (x + 0.5f) / w;
            Tile& t = world[tileIndex(x, y, w)];
            const float n = fbm(u * 5.0f, v * 5.0f, s.seed + 30, 4);
            const float air = -25.0f + 52.0f * cosLat
                - std::pow(std::max(t.height, 0.0f), 1.8f) * 35.75f
                + (n - 0.5f) * noiseAmp * 30.0f
                + tempShift;
            t.airTemperature = air;
            if (t.height > 0.0f) {
                t.surfaceTemperature = air;
                t.temperature = air;
                continue;
            }
            const float sst = std::clamp(
                29.8f * std::pow(cosLat, 1.6f) - 1.8f
                + (n - 0.5f) * noiseAmp * 20.0f
                + tempShift * 0.75f,
                FREEZING_SEA, 35.0f);
            t.surfaceTemperature = sst;
            if (air < ICE_AIR_TEMP) {
                t.temperature = air;
                continue;
            }
            const float deep = std::min(0.7f + 2.6f * (1.0f - lat) + tempShift * 0.4f, sst);
            t.temperature = std::max(
                lerpf(sst, deep, remap(-t.height, 0.03f, 0.30f)), FREEZING_SEA);
        }
    }
    std::vector<float> precip(count, 0.0f);
    std::vector<unsigned char> landMask(count, 0);
    for (std::size_t i = 0; i < count; ++i) {
        landMask[i] = world[i].height > 0.0f ? 1 : 0;
    }
    for (int y = 0; y < h; ++y) {
        const float lat = std::abs(2.0f * ((y + 0.5f) / h) - 1.0f);
        const int dir = (lat < 0.30f || lat >= 0.68f) ? -1 : 1;
        float air = 0.6f;
        float prevElevation = 0.0f;
        for (int k = 0; k < w; ++k) {
            const int x = dir > 0 ? k : w - 1 - k;
            const std::size_t i = tileIndex(x, y, w);
            const Tile& t = world[i];
            if (t.height <= 0.0f) {
                const float warm = clamp01((t.surfaceTemperature + 2.0f) / 32.0f);
                air += (0.06f + 0.10f * warm) * (1.0f - air);
                prevElevation = 0.0f;
            } else {
                const float rise = std::max(0.0f, t.height - prevElevation);
                const float orographic = std::min(rise * 2.5f, 0.10f);
                const float rain = air * (0.020f + orographic);
                air -= rain;
                air += 0.014f * (1.0f - air);
                precip[i] = rain;
                prevElevation = t.height;
            }
        }
    }
    blurMasked(precip, landMask, w, h, 2);
    for (int y = 0; y < h; ++y) {
        const float v = (y + 0.5f) / h;
        const float lat = std::abs(2.0f * v - 1.0f);
        const float band = rainBand(lat);
        for (int x = 0; x < w; ++x) {
            const float u = (x + 0.5f) / w;
            const std::size_t i = tileIndex(x, y, w);
            Tile& t = world[i];
            if (t.height <= 0.0f) {
                t.moisture = 1.0f;
                continue;
            }
            const float n = fbm(u * 6.0f, v * 6.0f, s.seed + 31, 4);
            const float variation = std::exp((n - 0.5f) * noiseAmp * 4.0f);
            const float coldDry = std::clamp(std::exp(0.045f * (t.airTemperature - 12.0f)), 0.12f, 1.8f);
            const float wetness =
                (0.03f + precip[i] * 22.0f * (0.30f + 0.85f * band))
                * variation * rainFactor * coldDry;
            const float floorMm = 20.0f + 100.0f * clamp01(-t.airTemperature / 8.0f);
            t.rainfallMm = std::clamp(wetness * 1500.0f, floorMm, 6000.0f);
            t.moisture = std::sqrt(std::min(t.rainfallMm / 6000.0f, 0.9f));
        }
    }
}
Terrain pickBiome(float c, float mm) {
    if (c < -15.0f) return Terrain::Snow;
    if (c < -5.0f) return Terrain::Tundra;
    if (c < 5.0f) return mm >= 300.0f ? Terrain::Taiga : Terrain::Plains;
    if (c < 21.0f) {
        if (mm < 250.0f) return c > 10.0f ? Terrain::Desert : Terrain::Plains;
        if (mm < 480.0f) return Terrain::Plains;
        if (mm < 825.0f) return Terrain::Grass;
        return Terrain::Forest;
    }
    if (mm < 250.0f) return Terrain::Desert;
    if (mm < 1600.0f) return Terrain::Savanna;
    return Terrain::Jungle;
}
void classifyBiomes(
    World& world,
    const WorldSettings& s,
    const Thresholds& th
) {
    const int w = s.width, h = s.height;
    auto waterAt = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= h) return true;
        return isWaterTile(world[tileIndex(x, y, w)]);
    };
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            Tile& t = world[tileIndex(x, y, w)];
            const float e = t.height;
            if (e <= 0.0f) {
                t.terrain = t.airTemperature < ICE_AIR_TEMP ? Terrain::Ice
                          : e < -0.30f ? Terrain::DeepWater
                                       : Terrain::ShallowWater;
                if (t.terrain != Terrain::Ice) {
                    t.temperature = std::clamp(t.temperature, FREEZING_SEA, 40.0f);
                }
                continue;
            }
            const bool coastal =
                waterAt(x - 1, y) || waterAt(x + 1, y) ||
                waterAt(x, y - 1) || waterAt(x, y + 1);
            Terrain biome = pickBiome(t.temperature, t.rainfallMm);
            if (e >= th.mountainStart) {
                biome = t.temperature < -4.0f ? Terrain::Snow : Terrain::Mountain;
            } else if (coastal && e < 0.08f && t.temperature > -5.0f) {
                biome = Terrain::Sand;
            } else if (t.rainfallMm > 1200.0f && e < 0.12f && t.temperature > -2.0f) {
                biome = Terrain::Wetland;
            } else if (e >= th.hillStart &&
                       (biome == Terrain::Grass || biome == Terrain::Plains ||
                        biome == Terrain::Savanna || biome == Terrain::Tundra)) {
                biome = Terrain::Hill;
            }
            t.terrain = biome;
        }
    }
}
void generateWater(World& world, const WorldSettings& s) {
    const int w = s.width, h = s.height;
    const std::size_t count = world.size();
    using Node = std::pair<float, std::size_t>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> frontier;
    std::vector<float> filled(count, 0.0f);
    std::vector<unsigned char> visited(count, 0);
    std::vector<std::size_t> down(count, count);
    std::vector<std::size_t> orderOf(count, 0);
    std::size_t order = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t i = tileIndex(x, y, w);
            const bool outlet = x == 0 || y == 0 || x == w - 1 || y == h - 1 ||
                                isWaterTile(world[i]);
            if (!outlet) continue;
            visited[i] = 1;
            filled[i] = world[i].height;
            orderOf[i] = order++;
            frontier.push({filled[i], i});
        }
    }
    while (!frontier.empty()) {
        const std::size_t cur = frontier.top().second;
        frontier.pop();
        const int cx = static_cast<int>(cur % w);
        const int cy = static_cast<int>(cur / w);
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                const int nx = cx + dx, ny = cy + dy;
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                const std::size_t next = tileIndex(nx, ny, w);
                if (visited[next]) continue;
                visited[next] = 1;
                filled[next] = std::max(world[next].height, filled[cur] + 1e-5f);
                down[next] = cur;
                orderOf[next] = order++;
                frontier.push({filled[next], next});
            }
        }
    }
    constexpr float MIN_LAKE_DEPTH = 0.045f;
    constexpr int MIN_LAKE_SIZE = 5;
    std::vector<unsigned char> candidate(count, 0);
    for (std::size_t i = 0; i < count; ++i) {
        if (world[i].height > 0.0f && filled[i] - world[i].height > MIN_LAKE_DEPTH) {
            candidate[i] = 1;
        }
    }
    std::vector<unsigned char> seen(count, 0);
    for (std::size_t start = 0; start < count; ++start) {
        if (!candidate[start] || seen[start]) continue;
        std::vector<std::size_t> component{start};
        seen[start] = 1;
        for (std::size_t k = 0; k < component.size(); ++k) {
            const std::size_t cur = component[k];
            const int cx = static_cast<int>(cur % w);
            const int cy = static_cast<int>(cur / w);
            const int dx[4] = {1, -1, 0, 0};
            const int dy[4] = {0, 0, 1, -1};
            for (int d = 0; d < 4; ++d) {
                const int nx = cx + dx[d], ny = cy + dy[d];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                const std::size_t j = tileIndex(nx, ny, w);
                if (candidate[j] && !seen[j]) {
                    seen[j] = 1;
                    component.push_back(j);
                }
            }
        }
        if (static_cast<int>(component.size()) >= MIN_LAKE_SIZE) {
            for (std::size_t i : component) world[i].isLake = true;
        }
    }
    std::vector<std::size_t> byOrder(count, 0);
    for (std::size_t i = 0; i < count; ++i) byOrder[orderOf[i]] = i;
    std::vector<float> flow(count, 0.0f);
    float totalWater = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        if (world[i].height > 0.0f) {
            flow[i] = 0.35f + std::min(world[i].rainfallMm / 1500.0f, 1.0f);
            totalWater += flow[i];
        }
    }
    for (std::size_t k = count; k-- > 0;) {
        const std::size_t i = byOrder[k];
        if (down[i] != count) flow[down[i]] += flow[i];
    }
    const float roll =
        static_cast<float>(hashPosition(17, 43, s.seed) % 1000) / 999.0f;
    const float threshold = totalWater * 0.0030f * (0.7f + 0.6f * roll);
    std::vector<std::size_t> extra;
    for (std::size_t i = 0; i < count; ++i) {
        Tile& t = world[i];
        if (t.height <= 0.0f || t.isLake || flow[i] < threshold) continue;
        t.isRiver = true;
        t.riverWidth = flow[i] >= threshold * 5.0f ? 2 : 1;
        const std::size_t d = down[i];
        if (d == count) continue;
        const int x = static_cast<int>(i % w), y = static_cast<int>(i / w);
        const int dx = static_cast<int>(d % w) - x;
        const int dy = static_cast<int>(d / w) - y;
        if (dx != 0 && dy != 0) {
            const std::size_t a = tileIndex(x + dx, y, w);
            const std::size_t b = tileIndex(x, y + dy, w);
            extra.push_back(world[a].height <= world[b].height ? a : b);
        }
    }
    for (std::size_t i : extra) {
        Tile& t = world[i];
        if (t.height > 0.0f && !t.isLake && !t.isRiver) {
            t.isRiver = true;
            t.riverWidth = 1;
        }
    }
}
void placeDecorations(World& world, const WorldSettings& s) {
    const int w = s.width, h = s.height;
    const float forest = clamp01(s.forestDensity);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            Tile& t = world[tileIndex(x, y, w)];
            t.decoration = Decoration::None;
            if (t.height <= 0.0f || t.isLake || t.isRiver) continue;
            const unsigned int v = t.variation;
            const float roll = randomValue(x, y, s.seed + 10);
            switch (t.terrain) {
                case Terrain::Forest:
                    if (roll < forest)
                        t.decoration = t.temperature < 8.0f
                            ? Decoration::PineTree : Decoration::Tree;
                    break;
                case Terrain::Jungle:
                    if (roll < std::min(1.0f, forest * 1.15f))
                        t.decoration = Decoration::Tree;
                    break;
                case Terrain::Taiga:
                    if (roll < forest * 0.9f)
                        t.decoration = Decoration::PineTree;
                    break;
                case Terrain::Savanna:
                    if (v % 17 == 0) t.decoration = Decoration::Tree;
                    else if (v % 53 == 0) t.decoration = Decoration::Rock;
                    break;
                case Terrain::Grass:
                    if (v % 31 == 0) t.decoration = Decoration::Tree;
                    else if (v % 37 == 0) t.decoration = Decoration::Flower;
                    else if (v % 53 == 0) t.decoration = Decoration::Rock;
                    break;
                case Terrain::Plains:
                    if (v % 37 == 0) t.decoration = Decoration::Flower;
                    else if (v % 53 == 0) t.decoration = Decoration::Rock;
                    break;
                case Terrain::Hill:
                    if (v % 9 == 0) t.decoration = Decoration::Rock;
                    else if (t.rainfallMm > 750.0f && v % 6 == 0)
                        t.decoration = Decoration::Tree;
                    break;
                case Terrain::Mountain:
                    if (v % 5 == 0) t.decoration = Decoration::Rock;
                    break;
                case Terrain::Tundra:
                    if (v % 47 == 0) t.decoration = Decoration::Rock;
                    break;
                case Terrain::Desert:
                    if (v % 41 == 0) t.decoration = Decoration::Cactus;
                    break;
                default:
                    break;
            }
        }
    }
}
struct VolcanoSite {
    std::size_t index;
    bool active;
};
std::vector<VolcanoSite> chooseVolcanoes(
    const World& world,
    const WorldSettings& s,
    const Thresholds& th
) {
    const int w = s.width, h = s.height;
    const std::size_t count = world.size();
    const std::vector<float>& field = th.volcanoField;
    const float refSea = quantileOf(field, 0.5f);
    const float peakCut = quantileOf(field, 0.94f);
    const float maxField = *std::max_element(field.begin(), field.end());
    const float fieldRange = std::max(maxField - refSea, 1e-4f);
    std::vector<int> dist(count, -1);
    std::vector<std::size_t> queue;
    for (std::size_t i = 0; i < count; ++i) {
        if (field[i] <= refSea) {
            dist[i] = 0;
            queue.push_back(i);
        }
    }
    for (std::size_t k = 0; k < queue.size(); ++k) {
        const std::size_t cur = queue[k];
        const int cx = static_cast<int>(cur % w);
        const int cy = static_cast<int>(cur / w);
        const int dx[4] = {1, -1, 0, 0};
        const int dy[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4; ++d) {
            const int nx = cx + dx[d], ny = cy + dy[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            const std::size_t j = tileIndex(nx, ny, w);
            if (dist[j] >= 0) continue;
            dist[j] = dist[cur] + 1;
            queue.push_back(j);
        }
    }
    struct Candidate {
        float score;
        std::size_t index;
    };
    std::vector<Candidate> candidates;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t i = tileIndex(x, y, w);
            if (field[i] < peakCut) continue;
            const float coast = static_cast<float>(std::min(dist[i], 60));
            const float height = clamp01((field[i] - refSea) / fieldRange);
            candidates.push_back({
                height * 0.5f
                    + randomValue(x, y, s.seed + 90) * 0.5f
                    - coast * 0.006f,
                i
            });
        }
    }
    std::sort(
        candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) { return a.score > b.score; }
    );
    const std::size_t target = 3 + hashPosition(5, 9, s.seed + 90) % 5;
    constexpr int MIN_SPACING = 20;
    std::vector<std::size_t> chosen;
    for (const Candidate& c : candidates) {
        if (chosen.size() >= target) break;
        const int x = static_cast<int>(c.index % w);
        const int y = static_cast<int>(c.index / w);
        bool tooClose = false;
        for (std::size_t other : chosen) {
            const int dx = static_cast<int>(other % w) - x;
            const int dy = static_cast<int>(other / w) - y;
            if (dx * dx + dy * dy < MIN_SPACING * MIN_SPACING) {
                tooClose = true;
                break;
            }
        }
        if (!tooClose) chosen.push_back(c.index);
    }
    std::vector<VolcanoSite> sites;
    for (std::size_t k = 0; k < chosen.size(); ++k) {
        const std::size_t i = chosen[k];
        if (world[i].height <= 0.0f) continue;
        const int x = static_cast<int>(i % w);
        const int y = static_cast<int>(i / w);
        const bool active =
            k == 0 || hashPosition(x, y, s.seed + 95) % 100 < 40;
        sites.push_back({i, active});
    }
    return sites;
}
void applyVolcanicHeat(
    World& world,
    const WorldSettings& s,
    const std::vector<VolcanoSite>& volcanoes
) {
    const int w = s.width, h = s.height;
    constexpr int RADIUS = 8;
    constexpr float WATER_HEAT = 8.0f;
    constexpr float LAND_HEAT = 8.0f;
    std::vector<float> heat(world.size(), 0.0f);
    for (const VolcanoSite& site : volcanoes) {
        if (!site.active) continue;
        const int x = static_cast<int>(site.index % w);
        const int y = static_cast<int>(site.index / w);
        for (int dy = -RADIUS; dy <= RADIUS; ++dy) {
            for (int dx = -RADIUS; dx <= RADIUS; ++dx) {
                const int nx = x + dx, ny = y + dy;
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                const float distance =
                    std::sqrt(static_cast<float>(dx * dx + dy * dy));
                if (distance > RADIUS) continue;
                const float falloff = 1.0f - distance / RADIUS;
                const std::size_t j = tileIndex(nx, ny, w);
                heat[j] = std::max(heat[j], falloff * falloff);
            }
        }
    }
    for (std::size_t i = 0; i < world.size(); ++i) {
        if (heat[i] <= 0.0f) continue;
        Tile& t = world[i];
        const float strength = t.height <= 0.0f ? WATER_HEAT : LAND_HEAT;
        t.temperature += heat[i] * strength;
        t.surfaceTemperature += heat[i] * strength;
        t.airTemperature += heat[i] * strength;
    }
}
void placeVolcanoes(World& world, const std::vector<VolcanoSite>& volcanoes) {
    for (const VolcanoSite& site : volcanoes) {
        Tile& t = world[site.index];
        t.decoration = site.active ? Decoration::Volcano : Decoration::ExtinctVolcano;
        t.isRiver = false;
        t.isLake = false;
    }
}
}
World generateWorld(const WorldSettings& settings) {
    const int w = settings.width;
    const int h = settings.height;
    World world(static_cast<std::size_t>(w) * h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            world[tileIndex(x, y, w)].variation =
                hashPosition(x, y, settings.seed);
        }
    }
    const Thresholds th = computeElevation(world, settings);
    const std::vector<VolcanoSite> volcanoes =
        chooseVolcanoes(world, settings, th);
    computeClimate(world, settings);
    applyVolcanicHeat(world, settings, volcanoes);
    classifyBiomes(world, settings, th);
    generateWater(world, settings);
    for (Tile& t : world) {
        if (t.isLake || t.isRiver) {
            t.temperature = 8.0f + (t.temperature - 8.0f) * 0.85f - 1.0f;
            t.moisture = 1.0f;
        }
    }
    placeDecorations(world, settings);
    placeVolcanoes(world, volcanoes);
    return world;
}
