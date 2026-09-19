#pragma once

#include "bensdm/config.hpp"
#include "bensdm/layout.hpp"

#include <X11/Xlib.h>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace bensdm {

// De twee hoofd-layouts uit de spec. Meer volgt later (bv. WORKSPACE-varianten),
// maar de kern van Bens DM begint met deze twee.
enum class LayoutMode {
    Tiling,
    Stacking
};

// Minimale info die we per beheerd venster bijhouden.
// Dit wordt in latere stappen uitgebreid (workspace-id, etc.)
struct ManagedWindow {
    Window x_window;

    // Huidige, daadwerkelijke geometrie op het scherm (wordt overschreven
    // zodra Tiling actief is en de layout-engine het venster plaatst).
    int x = 0;
    int y = 0;
    unsigned int width = 100;
    unsigned int height = 100;

    // "Vrije" geometrie: waar/hoe groot dit venster stond de laatste keer
    // dat het niet in de tiling-grid zat (Stacking-mode, of terwijl het
    // floating is). Hierop wordt teruggevallen zodra dat weer relevant is.
    int free_x = 0;
    int free_y = 0;
    unsigned int free_width = 100;
    unsigned int free_height = 100;

    // Floating vensters doen niet mee aan de tiling-grid: ze houden altijd
    // hun eigen (vrije) positie/grootte aan en blijven bovenop getilede
    // vensters, ook terwijl Tiling actief is.
    bool floating = false;
};

// Wat een muis-drag op dit moment betekent. Eén duidelijke toestand i.p.v.
// losse booleans, omdat er nu meerdere soorten sleepacties zijn.
enum class DragMode {
    Idle,               // Geen actieve sleepactie
    MoveFloating,       // Alt+linkersleep op een floating venster / in Stacking
    ResizeFloating,     // Alt+rechtssleep op een floating venster / in Stacking
    SwapTiling,         // Alt+linkersleep op een getild venster: wissel bij loslaten
    BorderMasterStack,  // Slepen aan de grens tussen master- en stack-kolom
    BorderStackGap      // Slepen aan de grens tussen twee stack-vensters
};

class WindowManager {
public:
    // Maakt een WindowManager voor de opgegeven X-display naam.
    // Lege string ("") betekent: gebruik $DISPLAY.
    static WindowManager* Create(const Config& config, const char* display_name = "");

    ~WindowManager();

    // Start de event loop. Blokkerend; keert pas terug bij afsluiten.
    void Run();

private:
    WindowManager(Display* display, const Config& config);

    // --- Setup ---
    void SelectRootEvents();
    void GrabKeybindings();

    // --- X11 event handlers ---
    void OnMapRequest(const XMapRequestEvent& e);
    void OnDestroyNotify(const XDestroyWindowEvent& e);
    void OnUnmapNotify(const XUnmapEvent& e);
    void OnConfigureRequest(const XConfigureRequestEvent& e);
    void OnKeyPress(const XKeyEvent& e);
    void OnButtonPress(const XButtonEvent& e);
    void OnButtonRelease(const XButtonEvent& e);
    void OnMotionNotify(const XMotionEvent& e);
    void OnEnterNotify(const XCrossingEvent& e);

    // --- Window management primitieven ---
    void FrameOrTrackWindow(Window w);
    void ForgetWindow(Window w);
    void FocusWindow(Window w);
    void CloseWindow(Window w);
    void MoveResizeWindow(Window w, int x, int y, unsigned int width, unsigned int height);
    void SpawnTerminal();
    void SyncFreeGeometry(Window w); // Kopieert huidige x/y/w/h naar free_*

    // --- Layout ---
    void ToggleLayoutMode();
    void ApplyLayout(); // Herberekent/herplaatst alle windows voor de huidige mode
    void ApplyTilingLayout();
    void ApplyStackingLayout(); // Herstelt elk venster naar zijn "vrije" geometrie
    void ApplyFloatingWindows(); // Plaatst floating vensters en zet ze bovenop
    void ToggleFloating();       // Floating aan/uit voor het gefocuste venster
    void ResetLayout();          // Zet master-ratio + stack-verhoudingen terug naar config-defaults

    // Getilede (niet-floating) windows, in tiling-volgorde.
    std::vector<Window> TiledWindowOrder() const;

    // Wisselt het gefocuste venster met zijn buur in de tiling-volgorde.
    // direction: +1 = volgende, -1 = vorige (wrap-around).
    void SwapFocusedWithNeighbor(int direction);
    void SwapWindowsInOrder(Window a, Window b);

    // Kijkt of (root_x, root_y) op een sleepbare grens tussen twee getilede
    // vensters ligt. Zet bij een BorderStackGap-hit de betrokken stack-index
    // (0-based, tussen tiled[i+1] en tiled[i+2]) in out_stack_index.
    DragMode DetectBorderHit(int root_x, int root_y, int* out_stack_index);

    // Zoekt het getilede venster (uit de laatst berekende layout) dat de
    // opgegeven rootcoördinaat bevat, of 0 als er geen is (bv. in een gap).
    Window FindTiledWindowAt(int root_x, int root_y) const;

    // Error handler die voorkomt dat de WM crasht op "BadWindow"-races
    // (venster is bv. al weg tegen de tijd dat we het willen manipuleren).
    static int OnXError(Display* display, XErrorEvent* e);

    Display* display_;
    Window root_;
    Config config_;

    std::unordered_map<Window, ManagedWindow> windows_;

    // Aparte volgorde-lijst omdat unordered_map geen invoegvolgorde
    // garandeert, terwijl de tiling-layout wél een vaste "wie is master"
    // volgorde nodig heeft.
    std::vector<Window> window_order_;

    LayoutMode layout_mode_ = LayoutMode::Tiling;

    int screen_width_ = 0;
    int screen_height_ = 0;

    // Venster dat op dit moment de X-inputfocus heeft, expliciet bijgehouden
    // zodat sneltoetsen (sluiten, floaten, swappen) altijd het juiste
    // venster raken, ongeacht waar de muis toevallig staat.
    Window focused_window_ = 0;

    // Instelbare tiling-verhoudingen, runtime aanpasbaar via grens-slepen
    // (zie BorderMasterStack / BorderStackGap) en terug te zetten met
    // ResetLayout(). Beginnen bij de config-defaults.
    double master_ratio_;
    std::vector<double> stack_weights_; // relatief gewicht per stack-venster

    // Laatst berekende tiling-rechthoeken, gebruikt om grens-hitboxen en
    // swap-drop-doelen te bepalen zonder de layout opnieuw te berekenen.
    std::unordered_map<Window, Rect> last_tiling_rects_;

    // State voor interactieve muis-acties (verplaatsen/resizen/swappen/
    // grenzen-slepen — zie DragMode hierboven).
    DragMode drag_mode_ = DragMode::Idle;
    Window drag_target_ = 0;
    int drag_start_x_ = 0;
    int drag_start_y_ = 0;
    int drag_orig_x_ = 0;
    int drag_orig_y_ = 0;
    unsigned int drag_orig_w_ = 0;
    unsigned int drag_orig_h_ = 0;
    int border_stack_index_ = -1;
    double drag_ratio_start_a_ = 0.0;
    double drag_ratio_start_b_ = 0.0;

    static bool wm_detected_; // true als een andere WM al SubstructureRedirect heeft
};
} // namespace bensdm
