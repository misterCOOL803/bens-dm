Bens DM

A fully custom X11 desktop environment / window manager for Bens OS.

Written from scratch in C++ with Xlib.

No XFCE.
No KDE.
No GNOME.
No Openbox.
No Wayland.

Just X11, C++, and a steadily increasing amount of code that now controls your entire desktop.

Current status: Step 2 of the development strategy
Base OS: Debian Trixie
Display server: X11
Window manager: Bens DM
Launcher: The terminal
Panel: Coming soon™
Regret: Not yet measured

What is Bens DM?

Bens DM is the window manager for Bens OS.

It owns the X11 root window, manages windows, handles input, calculates tiling layouts, launches terminals, supports floating windows, and generally decides where things are allowed to exist on the screen.

There is no desktop environment underneath it.

There is no existing window manager being wrapped.

There is no GNOME/KDE/XFCE doing the difficult parts in the background.

Bens DM is the difficult part.

Current features
Window management

Bens DM claims SubstructureRedirectMask on the X11 root window.

This is the part where it officially becomes the window manager instead of merely being a program that happens to draw things.

It:

opens the X display

checks whether another WM is already running

exits cleanly if one is detected

handles new windows through MapRequest

manages focus

supports focus-follows-mouse

supports click-to-focus

moves windows

resizes windows

closes windows

If another window manager already owns the root window, Bens DM refuses to start instead of having an argument with it.

Moving and resizing

There are no titlebars.

There are no visible resize handles.

This is intentional.

Window manipulation is handled directly through mouse + modifier combinations:

Alt + Left Drag    Move window
Alt + Right Drag   Resize window


Closing the focused window:

Super + Q


Bens DM first attempts to send WM_DELETE_WINDOW.

If the application doesn't support that, it falls back to XKillClient.

Because sometimes the application needs to be informed politely.

And sometimes it doesn't.

Tiling

The current layout engine uses a master-stack layout.

The first managed window becomes the master.

Everything else goes into the stack.

Conceptually:
```text
┌──────────────────────┬──────────────────┐
│                      │                  │
│                      │     Window 2     │
│                      │                  │
│       MASTER         ├──────────────────┤
│                      │                  │
│                      │     Window 3     │
│                      │                  │
│                      ├──────────────────┤
│                      │                  │
│                      │     Window 4     │
└──────────────────────┴──────────────────┘
```


The layout implementation lives in:

src/layouts/tiling_layout.cpp


The layout math is tested independently from X11, so the actual geometry calculations can be verified without starting a graphical session.

Which is useful, because debugging an X11 window manager by repeatedly destroying your own desktop gets old fairly quickly.

Tiling and Stacking

mod + Space switches between two modes.

Tiling

Bens DM controls the geometry.

Windows are automatically positioned using the master-stack layout whenever windows are opened, closed, or the layout is explicitly refreshed.

Stacking

Bens DM stops imposing the tiling geometry.

Windows return to their own stored positions and sizes.

If you moved a window somewhere manually, that position is preserved.

Switching back to Tiling later does not permanently overwrite the window's free geometry.

This means Tiling is a layout mode rather than a permanent relocation of every window.

Floating

mod + F toggles floating mode for the focused window.

A floating window:

is removed from the tiling grid

keeps its own position and size

remains above tiled windows

can still be moved

can still be resized

can coexist with tiled windows while Tiling mode is active

Press mod + F again and the window returns to the tiling system.

At this point Bens DM has both a tiling system and a floating system, which is starting to look suspiciously like an actual window manager.

Swapping windows

In Tiling mode, Alt + Left Drag changes meaning.

For normal floating or stacking windows:

Alt + Left Drag


means move.

For tiled windows:

Alt + Left Drag


means swap.

Drag one tiled window over another and release.

The two windows exchange positions in the tiling order.

There are also keyboard controls:

mod + Shift + J
mod + Shift + K


These swap the focused window with the next or previous window in the tiling order.

The list wraps around.

Resizing tiles

Tile boundaries can be dragged directly.

No modifier is required.

No titlebar is required.

No special decoration is required.

Click and drag the boundary between:

the master and stack

two stack windows

and the corresponding ratio changes.

The interaction is handled through the root background in the gap between windows.

The desktop background is therefore, technically, part of the user interface.

This was not originally on the roadmap.

Configuration

Bens DM reads:

~/.config/bens-dm/config


or:

$XDG_CONFIG_HOME/bens-dm/config


Configuration uses a simple:

key = value


syntax.

Comments use #.

If the configuration file doesn't exist, Bens DM uses built-in defaults.

Invalid lines are ignored with a warning rather than preventing the WM from starting.

Current options:

gap
master_ratio
mod_key
terminal


mod_key can be:

super
alt


The terminal command is started using fork() and execvp().

SIGCHLD is set to SIG_IGN so terminal processes don't leave zombie processes behind.

The launcher

There isn't one.

There is a terminal.

mod + Enter


starts it.

That's currently the entire application-launching strategy.

This is not necessarily a limitation.

It's also not necessarily a good idea.

It is, however, extremely functional.

Session integration

Bens DM can be installed as a real X11 desktop session.

The session file:

assets/bens-dm.desktop


is installed to:

/usr/share/xsessions/


using make install.

This allows display managers such as LightDM and SDDM to present:

Bens DM


as an available session.

For startx / xinit, an example configuration is provided:

assets/xinitrc.example

Development strategy
Step 1 — Minimal WM

The original goal was simply to make a functioning window manager.

Implemented:

X display initialization

root window ownership

existing-WM detection

MapRequest handling

focus management

focus-follows-mouse

click-to-focus

window moving

window resizing

window closing

WM_DELETE_WINDOW

XKillClient fallback

At this point, Bens DM could manage windows.

That was the first major milestone.

Step 2 — Tiling layout engine

Current development stage.

Implemented:

master-stack layout

Tiling ↔ Stacking switching

persistent free window geometry

automatic relayout

dependency-free layout tests

floating windows

tiled window swapping

keyboard-based swapping

draggable tile boundaries

The project has now moved beyond “minimal WM” territory.

This is where things started getting interesting.

Step 3 — Configuration layer

Implemented:

configuration file parsing

configurable gaps

configurable master ratio

configurable modifier key

configurable terminal

example configuration

terminal spawning

zombie-process prevention

Configuration example:

assets/config.example

Step 4 — Session integration and advanced window handling

Implemented:

display-manager session file

X11 session integration

floating windows

tiling swaps

keyboard swaps

draggable tile boundaries

ratio reset

The roadmap is gradually becoming a list of things that already work.

Keybindings
Shortcut	Action
mod + Space	Switch Tiling ↔ Stacking
mod + Enter	Start terminal
mod + Q	Close focused window
mod + F	Toggle floating
mod + R	Reset tiling ratios
mod + Shift + J	Swap with next window
mod + Shift + K	Swap with previous window
Alt + Left Drag	Move / swap window
Alt + Right Drag	Resize window
Drag tile boundary	Adjust tile ratio

mod is configurable through mod_key and can be either Super or Alt.

Things that don't exist yet

These are intentionally left for later:

multiple master windows

workspace management

panel

launcher

settings application

more advanced keybinding handling

There is also a known limitation with XGrabKey and modifier masks.

Numlock and Capslock are currently not handled independently from the grabbed modifier combinations.

This will be fixed later.

Probably after something else breaks.

Build info
Requirements

CMake >= 3.16

C++17 compiler

X11 development headers

Debian/Ubuntu: libx11-dev

Debug build
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make


This produces the Bens DM binary in the build directory.

Testing

Do not immediately replace your existing window manager with Bens DM.

Test it in a nested X server first.

For example:

Xephyr -screen 1280x800 :1 &
DISPLAY=:1 ./build/bens-dm


This gives Bens DM its own X11 environment to break without taking the rest of your desktop with it.

The intended testing progression is:

Nested Xephyr session

Separate TTY/X11 session

QEMU/KVM with the complete Bens OS ISO

Physical hardware

In that order.

The layout engine can also be tested without X11:

cd build
ctest --output-on-failure

Installing as a real session
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr
sudo make install


This installs:

/usr/bin/bens-dm
/usr/share/xsessions/bens-dm.desktop


After installation, the display manager should show:

Bens DM


as a selectable X11 session.

At that point, Bens DM is no longer something you're launching manually from a terminal.

It is now responsible for your actual desktop.

and its alive. 

hunting for dumb engineers.

Good luck.
# Project structure

```text
bens-dm/
├── src/
│   ├── main.cpp
│   ├── wm/             # Window manager core
│   ├── layouts/        # Master-stack tiling engine
│   ├── input/          # Keybinding configuration
│   ├── x11/            # X11 helpers / wrappers
│   └── config/         # Configuration parser
├── include/bensdm/
├── assets/
├── tests/
├── CMakeLists.txt
└── README.md
```

# Current state

```text
Window management       DONE
Tiling                  DONE
Stacking                DONE
Floating                DONE
Window swapping         DONE
Tile resizing           DONE
Configuration           DONE
Terminal launching      DONE
Session integration     DONE

Multiple masters        NOT YET
Workspaces              NOT YET
Panel                   COMING SOON™
Launcher                NO
Settings app            NO
Wayland                 NO
```


And, most importantly:

Unauthorized audio      SHIPPED


Yes, that is a real category.

No, it was not planned.

Bens DM

A window manager written from scratch for Bens OS.

It started as:

“I should probably make a minimal WM.”

It is now:

“Why does this thing have floating windows, tiling swaps, session integration and a configuration system?”

At some point this stopped being a prototype.

Nobody informed the prototype.
