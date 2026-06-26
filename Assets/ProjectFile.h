#pragma once
#include <string>
#include <vector>

// Project file (.fcproj) + the recent-projects list. Standalone so both the
// launcher window and the editor can use it.
namespace projects {
    struct Info {
        std::string name;
        std::string assetSub = "Assets";
        std::string startupScene;
    };

    struct RecentEntry {
        std::string path;
        long long   opened = 0;   // epoch seconds of last open (0 = unknown)
    };

    bool Parse(const std::string& fcprojPath, Info& out);                 // read fields
    bool Create(const std::string& fcprojPath, const std::string& name);  // folders + .fcproj

    std::vector<RecentEntry> LoadRecent();
    void AddRecent(const std::string& fcprojPath);
    void RemoveRecent(const std::string& fcprojPath);
}
