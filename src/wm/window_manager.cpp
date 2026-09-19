#include "bensdm/window_manager.hpp"

#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>

namespace bensdm {

bool WindowManager::wm_detected_ = false;

// Tijdelijke error handler, alleen gebruikt tijdens het claimen van
// SubstructureRedirect op de root window. Als er al een WM draait,
// geeft X een BadAccess-fout: dat vangen we hier op i.p.v. te crashen.
static bool g_wm_detected = false;

namespace {
int OnWmDetectedReal(Display*, XErrorEvent* e) {
    if (e->error_code == BadAccess) {
        g_wm_detected = true;
    }
    return 0;
}
} // namespace

WindowManager* WindowManager::Create(const Config& config, const char* display_name) {
    Display* display = XOpenDisplay(
        (display_name && display_name[0] != '\0') ? display_name : nullptr);
    if (display == nullptr) {
        std::fprintf(stderr,
            "bens-dm: kan geen verbinding maken met X-display '%s'\n",
            display_name);
        return nullptr;
    }
    return new WindowManager(display, config);
}

WindowManager::WindowManager(Display* display, const Config& config)
    : display_(display), root_(DefaultRootWindow(display)), config_(config),
      master_ratio_(config.master_ratio) {
    const int screen = DefaultScreen(display_);
    screen_width_ = DisplayWidth(display_, screen);
    screen_height_ = DisplayHeight(display_, screen);

    // Voorkom zombie-processen van gespawnde applicaties (bv. de terminal
    // via SpawnTerminal()) zonder dat we zelf moeten wachten/pollen.
    signal(SIGCHLD, SIG_IGN);
}

WindowManager::~WindowManager() {
    XCloseDisplay(display_);
}

void WindowManager::SelectRootEvents() {
    g_wm_detected = false;
    XErrorHandler old_handler = XSetErrorHandler(&OnWmDetectedReal);

    // Dit is de kernclaim van een window manager: SubstructureRedirectMask
    // zorgt dat MapRequest/ConfigureRequest naar ONS gestuurd worden i.p.v.
    // direct uitgevoerd. ButtonPress/Release/PointerMotion op de root zelf
    // hebben we nodig om klikken op de "kale" achtergrond te zien — dat is
    // precies waar de tiling-gaps zitten, dus zo detecteren we grens-sleep-
    // acties zonder een aparte grab nodig te hebben.
    XSelectInput(display_, root_,
        SubstructureRedirectMask | SubstructureNotifyMask |
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XSync(display_, False);

    if (g_wm_detected) {
        std::fprintf(stderr,
            "bens-dm: er draait al een andere window manager op deze display.\n");
        std::exit(EXIT_FAILURE);
    }

    XSetErrorHandler(&WindowManager::OnXError);
    (void)old_handler;
}

void WindowManager::GrabKeybindings() {
    // MOD + SPACE: wissel Tiling <-> Stacking (kernfeature uit de spec).
    XGrabKey(display_, XKeysymToKeycode(display_, XK_space), config_.mod_mask,
        root_, True, GrabModeAsync, GrabModeAsync);

    // MOD + Q: actief venster sluiten.
    XGrabKey(display_, XKeysymToKeycode(display_, XK_q), config_.mod_mask,
        root_, True, GrabModeAsync, GrabModeAsync);

    // MOD + Enter: terminal starten (config_.terminal_command).
    XGrabKey(display_, XKeysymToKeycode(display_, XK_Return), config_.mod_mask,
        root_, True, GrabModeAsync, GrabModeAsync);

    // MOD + F: floating aan/uit voor het gefocuste venster.
    XGrabKey(display_, XKeysymToKeycode(display_, XK_f), config_.mod_mask,
        root_, True, GrabModeAsync, GrabModeAsync);

    // MOD + R: reset de tiling-verhoudingen (master-ratio + grens-posities)
    // terug naar de config-defaults — de "reset-knop".
    XGrabKey(display_, XKeysymToKeycode(display_, XK_r), config_.mod_mask,
        root_, True, GrabModeAsync, GrabModeAsync);

    // MOD + SHIFT + J/K: wissel het gefocuste venster met de volgende/vorige
    // in de tiling-volgorde.
    XGrabKey(display_, XKeysymToKeycode(display_, XK_j), config_.mod_mask | ShiftMask,
        root_, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display_, XKeysymToKeycode(display_, XK_k), config_.mod_mask | ShiftMask,
        root_, True, GrabModeAsync, GrabModeAsync);

    // ALT + linkermuisknop op een venster: verplaatsen (floating/Stacking)
    // of wisselen van plaats (getild venster in Tiling-mode) — zie
    // OnButtonPress voor welke van de twee het wordt.
    XGrabButton(display_, Button1, Mod1Mask, root_, True,
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
        GrabModeAsync, GrabModeAsync, None, None);

    // ALT + rechtermuisknop: resizen (alleen zinvol voor floating vensters;
    // getilede vensters worden via grens-slepen aangepast, zie hierboven).
    XGrabButton(display_, Button3, Mod1Mask, root_, True,
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
        GrabModeAsync, GrabModeAsync, None, None);
}

void WindowManager::Run() {
    SelectRootEvents();
    GrabKeybindings();

    // Bestaande windows die al zichtbaar waren vóór onze start (bv. na een
    // herstart van de WM) alsnog gaan beheren.
    XGrabServer(display_);
    Window returned_root, returned_parent;
    Window* top_level_windows;
    unsigned int num_top_level_windows;
    XQueryTree(display_, root_, &returned_root, &returned_parent,
        &top_level_windows, &num_top_level_windows);
    for (unsigned int i = 0; i < num_top_level_windows; ++i) {
        FrameOrTrackWindow(top_level_windows[i]);
    }
    XFree(top_level_windows);
    XUngrabServer(display_);
    ApplyLayout();

    std::printf("bens-dm: gestart, layout-mode = Tiling\n");

    for (;;) {
        XEvent e;
        XNextEvent(display_, &e);

        switch (e.type) {
            case MapRequest:
                OnMapRequest(e.xmaprequest);
                break;
            case DestroyNotify:
                OnDestroyNotify(e.xdestroywindow);
                break;
            case UnmapNotify:
                OnUnmapNotify(e.xunmap);
                break;
            case ConfigureRequest:
                OnConfigureRequest(e.xconfigurerequest);
                break;
            case KeyPress:
                OnKeyPress(e.xkey);
                break;
            case ButtonPress:
                OnButtonPress(e.xbutton);
                break;
            case ButtonRelease:
                OnButtonRelease(e.xbutton);
                break;
            case MotionNotify:
                // Alleen de laatste motion event in de queue verwerken,
                // zodat drags soepel blijven i.p.v. te stotteren.
                while (XCheckTypedEvent(display_, MotionNotify, &e)) {}
                OnMotionNotify(e.xmotion);
                break;
            case EnterNotify:
                OnEnterNotify(e.xcrossing);
                break;
            default:
                break;
        }
    }
}

void WindowManager::FrameOrTrackWindow(Window w) {
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(display_, w, &attrs)) return;
    // Override-redirect windows (bv. tooltips, menu's) beheren we niet zelf.
    if (attrs.override_redirect) return;
    if (windows_.count(w)) return;

    ManagedWindow mw;
    mw.x_window = w;
    mw.x = attrs.x;
    mw.y = attrs.y;
    mw.width = attrs.width;
    mw.height = attrs.height;
    // De positie waarmee de applicatie zelf opende is ook meteen de eerste
    // "vrije" geometrie, tot de gebruiker het venster verplaatst/resized.
    mw.free_x = attrs.x;
    mw.free_y = attrs.y;
    mw.free_width = attrs.width;
    mw.free_height = attrs.height;
    windows_[w] = mw;
    window_order_.push_back(w);

    // We willen weten wanneer de muis een venster binnenkomt (focus-follows-
    // mouse, simpel en zonder extra klik nodig) en of het venster verdwijnt.
    XSelectInput(display_, w, EnterWindowMask | StructureNotifyMask);
}

void WindowManager::OnMapRequest(const XMapRequestEvent& e) {
    FrameOrTrackWindow(e.window);
    XMapWindow(display_, e.window);
    FocusWindow(e.window);
    ApplyLayout();
}

void WindowManager::OnDestroyNotify(const XDestroyWindowEvent& e) {
    ForgetWindow(e.window);
}

void WindowManager::OnUnmapNotify(const XUnmapEvent& e) {
    ForgetWindow(e.window);
}

void WindowManager::ForgetWindow(Window w) {
    if (windows_.erase(w) > 0) {
        window_order_.erase(
            std::remove(window_order_.begin(), window_order_.end(), w),
            window_order_.end());
        if (drag_target_ == w) {
            drag_mode_ = DragMode::Idle;
            drag_target_ = 0;
        }
        if (focused_window_ == w) {
            focused_window_ = 0;
        }
        ApplyLayout();
    }
}

void WindowManager::OnConfigureRequest(const XConfigureRequestEvent& e) {
    // We staan configure-requests toe zoals de applicatie ze vraagt; voor
    // getilede (niet-floating) vensters overrulen we x/y/width/height
    // meteen weer via ApplyLayout() na elke MapRequest/venster-wijziging,
    // dus tijdelijk "verkeerd" staan is onschadelijk.
    XWindowChanges changes;
    changes.x = e.x;
    changes.y = e.y;
    changes.width = e.width;
    changes.height = e.height;
    changes.border_width = e.border_width;
    changes.sibling = e.above;
    changes.stack_mode = e.detail;
    XConfigureWindow(display_, e.window, e.value_mask, &changes);
}

void WindowManager::OnKeyPress(const XKeyEvent& e) {
    KeySym keysym = XkbKeycodeToKeysym(display_, e.keycode, 0, 0);
    const bool mod = (e.state & config_.mod_mask) != 0;
    const bool shift = (e.state & ShiftMask) != 0;

    if (mod && !shift && keysym == XK_space) {
        ToggleLayoutMode();
        return;
    }
    if (mod && !shift && keysym == XK_q) {
        Window target = (focused_window_ != 0) ? focused_window_ : e.subwindow;
        if (target != 0) CloseWindow(target);
        return;
    }
    if (mod && !shift && keysym == XK_Return) {
        SpawnTerminal();
        return;
    }
    if (mod && !shift && keysym == XK_f) {
        ToggleFloating();
        return;
    }
    if (mod && !shift && keysym == XK_r) {
        ResetLayout();
        return;
    }
    if (mod && shift && keysym == XK_j) {
        SwapFocusedWithNeighbor(+1);
        return;
    }
    if (mod && shift && keysym == XK_k) {
        SwapFocusedWithNeighbor(-1);
        return;
    }
}

void WindowManager::OnButtonPress(const XButtonEvent& e) {
    // Klik direct op de rootwindow-achtergrond (dus in een tiling-gap,
    // zonder venster eronder): mogelijk een grens tussen twee tiles.
    if (e.window == root_ && e.subwindow == None) {
        int stack_index = -1;
        DragMode hit = DetectBorderHit(e.x_root, e.y_root, &stack_index);
        if (hit == DragMode::Idle) return;

        drag_mode_ = hit;
        border_stack_index_ = stack_index;
        drag_start_x_ = e.x_root;
        drag_start_y_ = e.y_root;
        drag_ratio_start_a_ = master_ratio_;
        if (hit == DragMode::BorderStackGap) {
            drag_ratio_start_a_ = stack_weights_[stack_index];
            drag_ratio_start_b_ = stack_weights_[stack_index + 1];
        }
        return;
    }

    if (e.subwindow == None) return;

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(display_, e.subwindow, &attrs)) return;

    drag_target_ = e.subwindow;
    drag_start_x_ = e.x_root;
    drag_start_y_ = e.y_root;
    drag_orig_x_ = attrs.x;
    drag_orig_y_ = attrs.y;
    drag_orig_w_ = attrs.width;
    drag_orig_h_ = attrs.height;

    FocusWindow(e.subwindow);

    auto it = windows_.find(e.subwindow);
    const bool is_floating = (it != windows_.end() && it->second.floating);
    const bool free_positioning = is_floating || (layout_mode_ == LayoutMode::Stacking);

    if (e.button == Button1) {
        // Getild venster in Tiling-mode: Alt+slepen betekent "wissel van
        // plaats" i.p.v. vrij verplaatsen (de grid bepaalt de positie).
        drag_mode_ = free_positioning ? DragMode::MoveFloating : DragMode::SwapTiling;
    } else if (e.button == Button3) {
        if (free_positioning) {
            drag_mode_ = DragMode::ResizeFloating;
        } else {
            // Resizen van een getild venster gaat via grens-slepen
            // (BorderMasterStack/BorderStackGap), niet via Alt+rechtsklik.
            drag_mode_ = DragMode::Idle;
            drag_target_ = 0;
        }
    }
}

void WindowManager::OnButtonRelease(const XButtonEvent& e) {
    if (drag_mode_ == DragMode::SwapTiling && drag_target_ != 0) {
        Window drop_target = FindTiledWindowAt(e.x_root, e.y_root);
        if (drop_target != 0 && drop_target != drag_target_) {
            SwapWindowsInOrder(drag_target_, drop_target);
            ApplyLayout();
        }
    }
    drag_mode_ = DragMode::Idle;
    drag_target_ = 0;
    border_stack_index_ = -1;
}

void WindowManager::OnMotionNotify(const XMotionEvent& e) {
    switch (drag_mode_) {
        case DragMode::Idle:
        case DragMode::SwapTiling:
            // Bij SwapTiling gebeurt er bewust niets tijdens het slepen
            // (geen live preview zonder compositing); het wisselen gebeurt
            // pas bij het loslaten, zie OnButtonRelease.
            return;

        case DragMode::MoveFloating: {
            const int dx = e.x_root - drag_start_x_;
            const int dy = e.y_root - drag_start_y_;
            MoveResizeWindow(drag_target_, drag_orig_x_ + dx, drag_orig_y_ + dy,
                drag_orig_w_, drag_orig_h_);
            SyncFreeGeometry(drag_target_);
            return;
        }

        case DragMode::ResizeFloating: {
            const int dx = e.x_root - drag_start_x_;
            const int dy = e.y_root - drag_start_y_;
            const int new_w = std::max(20, static_cast<int>(drag_orig_w_) + dx);
            const int new_h = std::max(20, static_cast<int>(drag_orig_h_) + dy);
            MoveResizeWindow(drag_target_, drag_orig_x_, drag_orig_y_,
                static_cast<unsigned int>(new_w), static_cast<unsigned int>(new_h));
            SyncFreeGeometry(drag_target_);
            return;
        }

        case DragMode::BorderMasterStack: {
            const int dx = e.x_root - drag_start_x_;
            const double usable_w = static_cast<double>(screen_width_ - 2 * config_.gap);
            const double delta = static_cast<double>(dx) / usable_w;
            master_ratio_ = std::clamp(drag_ratio_start_a_ + delta, 0.15, 0.85);
            ApplyLayout();
            return;
        }

        case DragMode::BorderStackGap: {
            if (border_stack_index_ < 0 ||
                border_stack_index_ + 1 >= static_cast<int>(stack_weights_.size())) {
                return;
            }
            const int dy = e.y_root - drag_start_y_;
            const double total = drag_ratio_start_a_ + drag_ratio_start_b_;
            const double stack_h = static_cast<double>(screen_height_ - 2 * config_.gap);
            const double delta = (static_cast<double>(dy) / stack_h) * total;
            const double min_share = total * 0.1;
            const double new_a = std::clamp(drag_ratio_start_a_ + delta, min_share, total - min_share);
            stack_weights_[border_stack_index_] = new_a;
            stack_weights_[border_stack_index_ + 1] = total - new_a;
            ApplyLayout();
            return;
        }
    }
}

void WindowManager::OnEnterNotify(const XCrossingEvent& e) {
    FocusWindow(e.window);
}

void WindowManager::FocusWindow(Window w) {
    if (!windows_.count(w)) return;
    focused_window_ = w;
    XSetInputFocus(display_, w, RevertToPointerRoot, CurrentTime);
    XRaiseWindow(display_, w); // in Stacking-mode bepaalt dit de volgorde/diepte
}

void WindowManager::CloseWindow(Window w) {
    if (!windows_.count(w)) return;

    // Eerst netjes proberen via het WM_DELETE_WINDOW protocol; alleen als
    // de applicatie dat niet ondersteunt vallen we terug op XKillClient.
    Atom* protocols;
    int count;
    bool supports_delete = false;
    if (XGetWMProtocols(display_, w, &protocols, &count)) {
        Atom wm_delete = XInternAtom(display_, "WM_DELETE_WINDOW", False);
        for (int i = 0; i < count; ++i) {
            if (protocols[i] == wm_delete) supports_delete = true;
        }
        XFree(protocols);
    }

    if (supports_delete) {
        Atom wm_protocols = XInternAtom(display_, "WM_PROTOCOLS", False);
        Atom wm_delete = XInternAtom(display_, "WM_DELETE_WINDOW", False);
        XEvent msg = {};
        msg.xclient.type = ClientMessage;
        msg.xclient.window = w;
        msg.xclient.message_type = wm_protocols;
        msg.xclient.format = 32;
        msg.xclient.data.l[0] = static_cast<long>(wm_delete);
        msg.xclient.data.l[1] = CurrentTime;
        XSendEvent(display_, w, False, NoEventMask, &msg);
    } else {
        XKillClient(display_, w);
    }
}

void WindowManager::MoveResizeWindow(Window w, int x, int y,
        unsigned int width, unsigned int height) {
    XMoveResizeWindow(display_, w, x, y, width, height);
    auto it = windows_.find(w);
    if (it != windows_.end()) {
        it->second.x = x;
        it->second.y = y;
        it->second.width = width;
        it->second.height = height;
    }
}

void WindowManager::SyncFreeGeometry(Window w) {
    auto it = windows_.find(w);
    if (it == windows_.end()) return;
    it->second.free_x = it->second.x;
    it->second.free_y = it->second.y;
    it->second.free_width = it->second.width;
    it->second.free_height = it->second.height;
}

void WindowManager::SpawnTerminal() {
    // Fork + exec is de standaardmanier waarop X11 window managers
    // applicaties starten. We forken zodat bens-dm zelf niet vervangen
    // wordt door het gespawnde process (dat doet execvp namelijk wél in
    // het kind).
    pid_t pid = fork();
    if (pid < 0) {
        std::perror("bens-dm: fork mislukt");
        return;
    }
    if (pid == 0) {
        // Simpele split op spaties; geen ondersteuning voor quoted
        // argumenten. Voldoende voor "xterm" of "xterm -e tmux" e.d.; een
        // volwaardige shell-parser is bewuste over-engineering voor nu.
        std::istringstream iss(config_.terminal_command);
        std::vector<std::string> parts;
        std::string token;
        while (iss >> token) parts.push_back(token);

        if (parts.empty()) {
            std::fprintf(stderr, "bens-dm: leeg terminal-commando in config\n");
            std::_Exit(1);
        }

        std::vector<char*> argv;
        argv.reserve(parts.size() + 1);
        for (auto& p : parts) argv.push_back(p.data());
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        // Alleen bereikt als execvp faalt (bv. commando niet gevonden).
        std::perror("bens-dm: kan terminal niet starten");
        std::_Exit(1);
    }
    // Ouderproces (bens-dm zelf) gaat gewoon door; SIGCHLD staat op SIG_IGN
    // dus het kind wordt automatisch opgeruimd zonder dat wij hoeven te
    // wachten.
}

void WindowManager::ToggleLayoutMode() {
    layout_mode_ = (layout_mode_ == LayoutMode::Tiling)
        ? LayoutMode::Stacking
        : LayoutMode::Tiling;
    std::printf("bens-dm: layout-mode = %s\n",
        layout_mode_ == LayoutMode::Tiling ? "Tiling" : "Stacking");
    ApplyLayout();
}

std::vector<Window> WindowManager::TiledWindowOrder() const {
    std::vector<Window> result;
    result.reserve(window_order_.size());
    for (Window w : window_order_) {
        auto it = windows_.find(w);
        if (it != windows_.end() && !it->second.floating) {
            result.push_back(w);
        }
    }
    return result;
}

void WindowManager::ApplyLayout() {
    if (layout_mode_ == LayoutMode::Tiling) {
        ApplyTilingLayout();
    } else {
        ApplyStackingLayout();
    }
    ApplyFloatingWindows();
}

void WindowManager::ApplyTilingLayout() {
    std::vector<Window> tiled = TiledWindowOrder();

    // Zorg dat stack_weights_ bij het huidige aantal stack-vensters past
    // vóórdat we ermee rekenen; bij een mismatch (venster toegevoegd/
    // verwijderd/net floating gezet) resetten we naar gelijke verdeling
    // i.p.v. oude gewichten op de verkeerde vensters te laten "kleven".
    const int stack_count = std::max(0, static_cast<int>(tiled.size()) - 1);
    if (static_cast<int>(stack_weights_.size()) != stack_count) {
        stack_weights_.assign(static_cast<size_t>(stack_count), 1.0);
    }

    last_tiling_rects_ = layouts::ComputeTiling(
        tiled, screen_width_, screen_height_, master_ratio_, config_.gap, stack_weights_);

    for (Window w : tiled) {
        auto rect_it = last_tiling_rects_.find(w);
        if (rect_it == last_tiling_rects_.end()) continue;
        const Rect& r = rect_it->second;
        MoveResizeWindow(w, r.x, r.y, r.width, r.height);
    }
}

void WindowManager::ApplyStackingLayout() {
    // Stacking heeft geen algoritme nodig: elk venster gaat gewoon terug
    // naar de plek/grootte die het had toen we voor het laatst in
    // Stacking-mode zaten (zie ManagedWindow::free_*).
    for (auto& [w, mw] : windows_) {
        MoveResizeWindow(w, mw.free_x, mw.free_y, mw.free_width, mw.free_height);
    }
}

void WindowManager::ApplyFloatingWindows() {
    // Floating vensters staan los van Tiling/Stacking: altijd hun eigen
    // vrije geometrie, en altijd bovenop de getilede/gestackte vensters.
    for (Window w : window_order_) {
        auto it = windows_.find(w);
        if (it == windows_.end() || !it->second.floating) continue;
        MoveResizeWindow(w, it->second.free_x, it->second.free_y,
            it->second.free_width, it->second.free_height);
        XRaiseWindow(display_, w);
    }
}

void WindowManager::ToggleFloating() {
    if (focused_window_ == 0) return;
    auto it = windows_.find(focused_window_);
    if (it == windows_.end()) return;

    it->second.floating = !it->second.floating;
    if (it->second.floating) {
        // Bevries de huidige (vaak getilede) grootte/positie als startpunt
        // voor het floaten, zodat het venster niet plotseling ergens
        // anders verschijnt.
        it->second.free_x = it->second.x;
        it->second.free_y = it->second.y;
        it->second.free_width = it->second.width;
        it->second.free_height = it->second.height;
    }
    std::printf("bens-dm: venster is %s floating\n",
        it->second.floating ? "nu" : "niet meer");
    ApplyLayout();
}

void WindowManager::ResetLayout() {
    master_ratio_ = config_.master_ratio;
    stack_weights_.clear(); // ApplyTilingLayout vult 'm opnieuw met gelijke gewichten
    std::printf("bens-dm: tiling-verhoudingen gereset naar defaults\n");
    ApplyLayout();
}

void WindowManager::SwapWindowsInOrder(Window a, Window b) {
    if (a == b) return;
    auto a_it = std::find(window_order_.begin(), window_order_.end(), a);
    auto b_it = std::find(window_order_.begin(), window_order_.end(), b);
    if (a_it == window_order_.end() || b_it == window_order_.end()) return;
    std::iter_swap(a_it, b_it);
}

void WindowManager::SwapFocusedWithNeighbor(int direction) {
    if (layout_mode_ != LayoutMode::Tiling) return; // swappen heeft alleen zin in de grid
    if (focused_window_ == 0) return;

    std::vector<Window> tiled = TiledWindowOrder();
    if (tiled.size() < 2) return;

    auto pos_it = std::find(tiled.begin(), tiled.end(), focused_window_);
    if (pos_it == tiled.end()) return; // gefocuste venster is floating

    const int n = static_cast<int>(tiled.size());
    const int idx = static_cast<int>(pos_it - tiled.begin());
    const int other_idx = ((idx + direction) % n + n) % n;

    SwapWindowsInOrder(tiled[static_cast<size_t>(idx)], tiled[static_cast<size_t>(other_idx)]);
    ApplyLayout();
}

DragMode WindowManager::DetectBorderHit(int root_x, int root_y, int* out_stack_index) {
    if (layout_mode_ != LayoutMode::Tiling) return DragMode::Idle;

    std::vector<Window> tiled = TiledWindowOrder();
    if (tiled.size() < 2) return DragMode::Idle;

    constexpr int kBorderThreshold = 6;

    auto master_it = last_tiling_rects_.find(tiled[0]);
    if (master_it == last_tiling_rects_.end()) return DragMode::Idle;
    const Rect& master = master_it->second;

    // Verticale grens tussen master- en stack-kolom.
    const int master_stack_border_x = master.x + static_cast<int>(master.width) + config_.gap / 2;
    if (std::abs(root_x - master_stack_border_x) <= kBorderThreshold &&
        root_y >= master.y && root_y <= master.y + static_cast<int>(master.height)) {
        return DragMode::BorderMasterStack;
    }

    // Horizontale grenzen tussen stack-vensters onderling.
    for (size_t i = 1; i + 1 < tiled.size(); ++i) {
        auto rect_it = last_tiling_rects_.find(tiled[i]);
        if (rect_it == last_tiling_rects_.end()) continue;
        const Rect& r = rect_it->second;
        const int border_y = r.y + static_cast<int>(r.height) + config_.gap / 2;
        if (std::abs(root_y - border_y) <= kBorderThreshold &&
            root_x >= r.x && root_x <= r.x + static_cast<int>(r.width)) {
            if (out_stack_index) *out_stack_index = static_cast<int>(i) - 1;
            return DragMode::BorderStackGap;
        }
    }

    return DragMode::Idle;
}

Window WindowManager::FindTiledWindowAt(int root_x, int root_y) const {
    for (const auto& [w, r] : last_tiling_rects_) {
        if (root_x >= r.x && root_x <= r.x + static_cast<int>(r.width) &&
            root_y >= r.y && root_y <= r.y + static_cast<int>(r.height)) {
            return w;
        }
    }
    return 0;
}

int WindowManager::OnXError(Display* display, XErrorEvent* e) {
    char buf[1024];
    XGetErrorText(display, e->error_code, buf, sizeof(buf));
    std::fprintf(stderr, "bens-dm: X-fout: %s (request %d.%d)\n",
        buf, e->request_code, e->minor_code);
    return 0; // niet fataal; WM blijft draaien
}

} // namespace bensdm
