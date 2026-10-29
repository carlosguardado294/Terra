#include "WorldSave.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif
namespace fs = std::filesystem;
namespace {
const char* SAVE_HEADER = "TERRA_SAVE_V2";
void flushStorage() {
#if defined(__EMSCRIPTEN__)
    EM_ASM({
        FS.syncfs(false, function(err) {});
    });
#endif
}
fs::path userDataRoot() {
#if defined(__EMSCRIPTEN__)
    return fs::path("/terra");
#elif defined(_WIN32)
    const char* base = std::getenv("APPDATA");
    if (base && *base) return fs::path(base) / "TERRA";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home && *home) return fs::path(home) / "Library" / "Application Support" / "TERRA";
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg) return fs::path(xdg) / "TERRA";
    const char* home = std::getenv("HOME");
    if (home && *home) return fs::path(home) / ".local" / "share" / "TERRA";
#endif
    return fs::path("TERRA_data");
}
const fs::path& saveDirectory() {
    static const fs::path directory = userDataRoot() / "saves";
    return directory;
}
bool validFilename(const std::string& filename) {
    if (filename.empty() ||
        filename.find('/') != std::string::npos ||
        filename.find('\\') != std::string::npos ||
        filename.find("..") != std::string::npos) {
        return false;
    }
    return fs::path(filename).extension() == ".pworld";
}
std::string makeFilename(const std::string& name) {
    std::string result;
    for (unsigned char ch : name) {
        if (std::isalnum(ch)) {
            result += static_cast<char>(ch);
        } else if (ch == ' ' || ch == '-' || ch == '_') {
            if (!result.empty() && result.back() != '_') {
                result += '_';
            }
        }
    }
    while (!result.empty() && result.back() == '_') {
        result.pop_back();
    }
    if (result.empty()) {
        result = "World";
    }
    return result + ".pworld";
}
void migrateOldSaves() {
    std::error_code error;
    const fs::path old = "saves";
    if (!fs::is_directory(old, error)) {
        return;
    }
    for (const auto& entry : fs::directory_iterator(old, error)) {
        if (error) {
            break;
        }
        if (entry.is_regular_file() && entry.path().extension() == ".pworld") {
            fs::copy_file(
                entry.path(),
                saveDirectory() / entry.path().filename(),
                fs::copy_options::skip_existing,
                error
            );
        }
    }
}
bool ensureSaveDirectory() {
    std::error_code error;
    const bool existed = fs::is_directory(saveDirectory(), error);
    fs::create_directories(saveDirectory(), error);
    if (error || !fs::is_directory(saveDirectory())) {
        return false;
    }
    if (!existed) {
        migrateOldSaves();
    }
    return true;
}
bool readSettings(
    std::istream& file,
    WorldSettings& settings
) {
    unsigned int seed = 0;
    if (!(file >> settings.width
               >> settings.height
               >> seed
               >> settings.waterLevel
               >> settings.mountainIntensity
               >> settings.forestDensity
               >> settings.temperature
               >> settings.rainfall
               >> settings.biomeVariation)) {
        return false;
    }
    if (settings.width < 1 || settings.height < 1 ||
        settings.width > 1024 || settings.height > 1024 ||
        seed == 0) {
        return false;
    }
    settings.seed = seed;
    return true;
}
void writeSettings(
    std::ostream& file,
    const WorldSettings& settings
) {
    file << settings.width << '\n';
    file << settings.height << '\n';
    file << settings.seed << '\n';
    file << settings.waterLevel << '\n';
    file << settings.mountainIntensity << '\n';
    file << settings.forestDensity << '\n';
    file << settings.temperature << '\n';
    file << settings.rainfall << '\n';
    file << settings.biomeVariation << '\n';
}
}
void initPersistentStorage() {
#if defined(__EMSCRIPTEN__)
    EM_ASM({
        try { FS.mkdir('/terra'); } catch (e) {}
        FS.mount(IDBFS, {}, '/terra');
        FS.syncfs(true, function(err) {});
    });
#endif
}
void offerDownload(const std::string& path) {
#if defined(__EMSCRIPTEN__)
    const std::string script =
        "var d=FS.readFile('" + path + "');"
        "var a=document.createElement('a');"
        "a.href=URL.createObjectURL(new Blob([d],{type:'image/png'}));"
        "a.download='" + path + "';"
        "document.body.appendChild(a);a.click();a.remove();";
    emscripten_run_script(script.c_str());
#else
    (void)path;
#endif
}
std::string appDataDirectory() {
    std::error_code error;
    fs::create_directories(userDataRoot(), error);
    return userDataRoot().string();
}
bool saveNamedWorld(
    const std::string& name,
    const WorldSettings& settings
) {
    if (name.empty() || !ensureSaveDirectory()) {
        return false;
    }
    const std::string filename = makeFilename(name);
    const fs::path path = saveDirectory() / filename;
    std::ofstream file(path);
    if (!file) {
        return false;
    }
    file << SAVE_HEADER << '\n';
    file << name << '\n';
    writeSettings(file, settings);
    file.close();
    if (file.fail()) {
        return false;
    }
    flushStorage();
    return true;
}
bool loadNamedWorld(
    const std::string& filename,
    WorldSettings& settings
) {
    if (!validFilename(filename) || !ensureSaveDirectory()) {
        return false;
    }
    std::ifstream file(saveDirectory() / filename);
    if (!file) {
        return false;
    }
    std::string header;
    std::string name;
    if (!std::getline(file, header) ||
        header != SAVE_HEADER ||
        !std::getline(file, name)) {
        return false;
    }
    WorldSettings loaded = settings;
    if (!readSettings(file, loaded)) {
        return false;
    }
    settings = loaded;
    return true;
}
std::vector<SavedWorld> listSavedWorlds() {
    std::vector<SavedWorld> result;
    if (!ensureSaveDirectory()) {
        return result;
    }
    std::error_code error;
    for (const auto& entry :
         fs::directory_iterator(saveDirectory(), error)) {
        if (error) {
            break;
        }
        if (!entry.is_regular_file() ||
            entry.path().extension() != ".pworld") {
            continue;
        }
        std::ifstream file(entry.path());
        std::string header;
        std::string name;
        if (!std::getline(file, header) ||
            header != SAVE_HEADER ||
            !std::getline(file, name)) {
            continue;
        }
        WorldSettings settings;
        if (!readSettings(file, settings)) {
            continue;
        }
        result.push_back({
            name,
            entry.path().filename().string(),
            settings.seed
        });
    }
    std::sort(
        result.begin(), result.end(),
        [](const SavedWorld& a, const SavedWorld& b) {
            return a.name < b.name;
        }
    );
    return result;
}
bool deleteSavedWorld(const std::string& filename) {
    if (!validFilename(filename) || !ensureSaveDirectory()) {
        return false;
    }
    std::error_code error;
    const bool removed = fs::remove(
        saveDirectory() / filename,
        error
    );
    if (removed && !error) {
        flushStorage();
        return true;
    }
    return false;
}
