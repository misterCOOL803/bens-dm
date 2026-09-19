#include "bensdm/config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace bensdm {

namespace {

std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return s;
}

} // namespace

Config Config::LoadDefaultOrFile() {
    const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME");
    std::string path;
    if (xdg_config_home && xdg_config_home[0] != '\0') {
        path = std::string(xdg_config_home) + "/bens-dm/config";
    } else {
        const char* home = std::getenv("HOME");
        if (!home) {
            // Geen HOME bekend: gewoon met defaults verdergaan.
            return Config{};
        }
        path = std::string(home) + "/.config/bens-dm/config";
    }
    return LoadFromFile(path);
}

Config Config::LoadFromFile(const std::string& path) {
    Config config; // begint met de defaults

    std::ifstream file(path);
    if (!file.is_open()) {
        // Geen configbestand is prima: bens-dm draait dan met de defaults.
        return config;
    }

    std::string line;
    int line_number = 0;
    while (std::getline(file, line)) {
        ++line_number;
        std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            std::fprintf(stderr,
                "bens-dm: config regel %d ongeldig (geen '='): %s\n",
                line_number, trimmed.c_str());
            continue;
        }

        std::string key = ToLower(Trim(trimmed.substr(0, eq_pos)));
        std::string value = Trim(trimmed.substr(eq_pos + 1));

        if (key == "gap") {
            config.gap = std::atoi(value.c_str());
        } else if (key == "master_ratio") {
            double v = std::atof(value.c_str());
            if (v > 0.1 && v < 0.9) {
                config.master_ratio = v;
            } else {
                std::fprintf(stderr,
                    "bens-dm: master_ratio moet tussen 0.1 en 0.9 liggen, "
                    "'%s' genegeerd\n", value.c_str());
            }
        } else if (key == "mod_key") {
            std::string v = ToLower(value);
            if (v == "super" || v == "mod4") {
                config.mod_mask = Mod4Mask;
            } else if (v == "alt" || v == "mod1") {
                config.mod_mask = Mod1Mask;
            } else {
                std::fprintf(stderr,
                    "bens-dm: onbekende mod_key '%s' (gebruik 'super' of "
                    "'alt'), default gebruikt\n", value.c_str());
            }
        } else if (key == "terminal") {
            config.terminal_command = value;
        } else {
            std::fprintf(stderr,
                "bens-dm: onbekende config-key op regel %d: '%s'\n",
                line_number, key.c_str());
        }
    }

    return config;
}

} // namespace bensdm
