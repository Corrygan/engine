#include "Material.h"
#include <fstream>
#include <sstream>

bool Material::Save(const std::string& path) const {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "color="     << color[0] << " " << color[1] << " " << color[2] << " " << color[3] << "\n";
    f << "metallic="  << metallic  << "\n";
    f << "roughness=" << roughness << "\n";
    f << "emissive="  << emissiveColor[0] << " " << emissiveColor[1] << " " << emissiveColor[2] << " " << emissiveIntensity << "\n";
    if (!albedoTexture.empty())            f << "albedo="    << albedoTexture            << "\n";
    if (!normalTexture.empty())            f << "normal="    << normalTexture            << "\n";
    if (!ormTexture.empty()) f << "orm="<< ormTexture << "\n";
    return true;
}

bool Material::Load(const std::string& path, Material& out) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        std::istringstream ss(val);
        if      (key == "color")      { ss >> out.color[0] >> out.color[1] >> out.color[2] >> out.color[3]; }
        else if (key == "metallic")   { ss >> out.metallic;  }
        else if (key == "roughness")  { ss >> out.roughness; }
        else if (key == "emissive")   { ss >> out.emissiveColor[0] >> out.emissiveColor[1] >> out.emissiveColor[2] >> out.emissiveIntensity; }
        else if (key == "albedo")     { out.albedoTexture            = val; }
        else if (key == "normal")     { out.normalTexture            = val; }
        else if (key == "orm") { out.ormTexture = val; }
    }
    return true;
}
