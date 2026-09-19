#pragma once

#include <X11/Xlib.h>
#include <string>

namespace bensdm {

// Alle instelbare waarden van bens-dm. Bewust een platte struct met
// defaults — geen aparte "config manager"-klasse nodig voor dit aantal
// velden (vermijd onnodige abstractie, zoals de spec vraagt).
struct Config {
    // Ruimte tussen/rond getilede vensters, in pixels.
    int gap = 8;

    // Aandeel van het scherm voor het master-venster in Tiling-mode.
    double master_ratio = 0.55;

    // Modifier-toets voor alle bens-dm-sneltoetsen (Super+Space, Super+Q, ...).
    // Mod4Mask = Super/Windows-toets, Mod1Mask = Alt.
    unsigned int mod_mask = Mod4Mask;

    // Commando dat gestart wordt bij mod_mask + Enter.
    std::string terminal_command = "xterm";

    // Zoekt naar $XDG_CONFIG_HOME/bens-dm/config, anders
    // ~/.config/bens-dm/config. Bestaat het bestand niet, dan worden gewoon
    // de bovenstaande defaults gebruikt (bens-dm start altijd, ook zonder
    // configbestand).
    static Config LoadDefaultOrFile();

    // Leest een specifiek configbestand. Onbekende of foutieve regels
    // worden overgeslagen (met een waarschuwing op stderr) i.p.v. dat
    // bens-dm weigert te starten.
    static Config LoadFromFile(const std::string& path);
};

} // namespace bensdm
