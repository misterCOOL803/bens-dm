#pragma once

#include <X11/Xlib.h>
#include <unordered_map>
#include <vector>

namespace bensdm {

// Simpele geometrie-struct; los van X11-types zodat layout-code testbaar
// blijft zonder een echte display nodig te hebben.
struct Rect {
    int x;
    int y;
    unsigned int width;
    unsigned int height;
};

namespace layouts {

// Master-stack tiling: het eerste venster in `windows` wordt de "master"
// en krijgt het linkerdeel van het scherm (breedte bepaald door
// master_ratio). De overige vensters worden rechts verticaal gestapeld.
//
// `stack_weights` bepaalt de relatieve hoogte van elk stack-venster
// (index 0 hoort bij windows[1], index 1 bij windows[2], enz.). Is de
// grootte verkeerd (leeg, of niet gelijk aan windows.size()-1), dan wordt
// stilzwijgend teruggevallen op gelijke verdeling — zo kan de aanroeper
// altijd veilig een net-aangepast of nog-niet-geïnitialiseerd gewichten-
// vector doorgeven.
//
// `gap` is zowel de buitenrand als de ruimte tussen vensters, in pixels.
std::unordered_map<Window, Rect> ComputeTiling(
    const std::vector<Window>& windows,
    int screen_width,
    int screen_height,
    double master_ratio = 0.55,
    int gap = 8,
    const std::vector<double>& stack_weights = {});

} // namespace layouts
} // namespace bensdm
