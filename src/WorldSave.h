#pragma once
#include "WorldGenerator.h"
#include <string>
#include <vector>
struct SavedWorld {
    std::string name;
    std::string filename;
    unsigned int seed = 1;
};
std::string appDataDirectory();
void initPersistentStorage();
void offerDownload(const std::string& path);
bool saveNamedWorld(
    const std::string& name,
    const WorldSettings& settings
);
bool loadNamedWorld(
    const std::string& filename,
    WorldSettings& settings
);
std::vector<SavedWorld> listSavedWorlds();
bool deleteSavedWorld(const std::string& filename);
