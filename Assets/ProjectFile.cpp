#include "ProjectFile.h"

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <ctime>

namespace fs = std::filesystem;

namespace {
    constexpr const char* kRecentFile = "recent_projects.txt";
}

namespace projects {

bool Parse(const std::string& fcprojPath, Info& out) {
    std::ifstream in(fcprojPath);
    if (!in.is_open()) return false;

    out.name = fs::path(fcprojPath).stem().string();
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), val = line.substr(eq + 1);
        if      (key == "name")    out.name = val;
        else if (key == "assets")  out.assetSub = val;
        else if (key == "startup") out.startupScene = val;
    }
    return true;
}

bool Create(const std::string& fcprojPath, const std::string& name) {
    fs::path p(fcprojPath);
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    fs::create_directories(p.parent_path() / "Assets", ec);

    std::ofstream out(fcprojPath);
    if (!out.is_open()) return false;
    out << "fcproj 1\n";
    out << "name="   << (name.empty() ? p.stem().string() : name) << "\n";
    out << "assets=Assets\n";
    out << "startup=\n";
    return true;
}

namespace {
    void WriteRecent(const std::vector<projects::RecentEntry>& v) {
        std::ofstream out(kRecentFile);
        for (const auto& e : v) out << e.opened << '\t' << e.path << "\n";   // "epoch<TAB>path"
    }
}

std::vector<RecentEntry> LoadRecent() {
    std::vector<RecentEntry> recent;
    std::ifstream in(kRecentFile);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        RecentEntry e;
        auto tab = line.find('\t');
        if (tab != std::string::npos) {                 // "epoch<TAB>path"
            try { e.opened = std::stoll(line.substr(0, tab)); } catch (...) { e.opened = 0; }
            e.path = line.substr(tab + 1);
        } else {
            e.path = line;                              // legacy: just a path
        }
        if (!e.path.empty() && fs::exists(e.path)) recent.push_back(e);
    }
    return recent;
}

void AddRecent(const std::string& fcprojPath) {
    auto recent = LoadRecent();
    recent.erase(std::remove_if(recent.begin(), recent.end(),
                 [&](const RecentEntry& e) { return e.path == fcprojPath; }), recent.end());
    RecentEntry e; e.path = fcprojPath; e.opened = (long long)std::time(nullptr);
    recent.insert(recent.begin(), e);
    if (recent.size() > 10) recent.resize(10);
    WriteRecent(recent);
}

void RemoveRecent(const std::string& fcprojPath) {
    auto recent = LoadRecent();
    recent.erase(std::remove_if(recent.begin(), recent.end(),
                 [&](const RecentEntry& e) { return e.path == fcprojPath; }), recent.end());
    WriteRecent(recent);
}

} // namespace projects
