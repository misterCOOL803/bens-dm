# Bens DM

Een volledig zelfgeschreven X11-desktopomgeving/window manager voor Bens OS
(Debian Trixie-gebaseerd). Geen Wayland, geen wrapper rond XFCE/KDE/GNOME/
Openbox — een eigen C++/Xlib-kern.

## Status: stap 2 van de ontwikkelstrategie

**Stap 1 (minimale WM):**

- Opent de X-display en claimt `SubstructureRedirectMask` op de root window
  (de kern van "zijn" van een window manager).
- Detecteert of er al een andere WM draait en stopt dan netjes i.p.v. te
  crashen.
- Detecteert nieuwe windows (`MapRequest`) en beheert ze.
- Focus: focus-follows-mouse via `EnterNotify` + click-to-focus bij
  interactie.
- Verplaatsen: `Alt + linkermuisknop` slepen.
- Resizen: `Alt + rechtermuisknop` slepen.
- Sluiten: `Super + Q` (probeert eerst `WM_DELETE_WINDOW`, valt anders terug
  op `XKillClient`).

**Stap 2 (tiling-layout-engine, nieuw):**

- `src/layouts/tiling_layout.cpp` implementeert een master-stack layout:
  het eerst geopende venster wordt "master" (linkerdeel van het scherm),
  overige vensters worden rechts verticaal gestapeld met gelijke hoogte.
- `Super + Space` wisselt nu écht tussen Tiling en Stacking:
  - **Tiling**: alle beheerde vensters worden automatisch herplaatst volgens
    de master-stack-berekening, elke keer als een venster opent/sluit of
    als je expliciet wisselt.
  - **Stacking**: elk venster valt terug op zijn eigen "vrije" geometrie
    (waar het zelf werd geopend, of waar jij het laatst met Alt+slepen hebt
    neergezet) — dus geen tiling-posities die blijven "kleven" na het
    terugschakelen.
- Losse, dependency-vrije unit tests in `tests/layout_test.cpp` die de
  tiling-wiskunde controleren zonder dat er een X-server nodig is
  (`ctest` in de build-map).

**Stap 3 (configuratielaag):**

- `src/config/config.cpp` leest `~/.config/bens-dm/config` (of
  `$XDG_CONFIG_HOME/bens-dm/config`) in — simpele `key = value`-syntax,
  `#` voor commentaar. Ontbreekt het bestand, dan draait bens-dm gewoon met
  ingebouwde defaults; foutieve regels worden overgeslagen met een
  waarschuwing i.p.v. dat bens-dm weigert te starten.
- Instelbaar: `gap`, `master_ratio`, `mod_key` (`super` of `alt`) en
  `terminal` (commando voor de sneltoets hieronder).
- **mod + Enter** start een terminal (`fork`+`execvp`, met `SIGCHLD` op
  `SIG_IGN` zodat er geen zombie-processen ontstaan).
- Voorbeeldconfig: `assets/config.example` — kopieer naar
  `~/.config/bens-dm/config` om aan te passen.

**Stap 4 (session launcher, floating, tiling-swap, grens-slepen, nieuw):**

- **Session launcher**: `assets/bens-dm.desktop` wordt via `make install`
  naar `share/xsessions/` geïnstalleerd, zodat display managers (LightDM,
  SDDM, ...) "Bens DM" tonen als kiesbare sessie. Gebruik je liever
  `startx`/`xinit`, zie `assets/xinitrc.example`.
- **Floating windows**: `mod + F` maakt het gefocuste venster los van de
  tiling-grid. Het houdt daarna zijn eigen positie/grootte aan, blijft
  altijd bovenop getilede vensters, en is te verplaatsen (`Alt` +
  linkersleep) en te resizen (`Alt` + rechtersleep) — ook terwijl Tiling
  actief is. Nogmaals `mod + F` maakt het weer een normaal getild venster.
- **Vensters wisselen van plek**: in Tiling-mode is `Alt` + linkersleep op
  een getild (niet-floating) venster geen verplaatsing, maar een *swap* —
  laat los boven een ander getild venster om van plaats te wisselen.
  Ook via toetsenbord: **mod + Shift + J / K** wisselt het gefocuste
  venster met de volgende/vorige in de tiling-volgorde (met wrap-around).
- **Grenzen tussen tiles slepen**: klik-en-sleep (géén modifier nodig) op
  de rand tussen master en stack, of tussen twee stack-vensters onderling,
  om de verhouding aan te passen. Dit werkt via een klik direct op de
  root-achtergrond in de gap tussen vensters — er is dus geen aparte
  titelbalk/rand-decoratie voor nodig.
- **Reset**: **mod + R** zet de master-ratio en alle stack-verhoudingen
  terug naar de config-defaults (de "reset-knop").

## Sneltoetsentabel (huidige stand)

| Sneltoets              | Actie                                          |
|-------------------------|------------------------------------------------|
| `mod + Space`           | Wissel Tiling ↔ Stacking                        |
| `mod + Enter`           | Terminal starten                                |
| `mod + Q`               | Gefocust venster sluiten                        |
| `mod + F`               | Floating aan/uit voor gefocust venster          |
| `mod + R`               | Tiling-verhoudingen resetten naar defaults      |
| `mod + Shift + J / K`   | Gefocust venster wisselen met volgende/vorige   |
| `Alt` + linkersleep     | Verplaatsen (floating/Stacking) of swap (Tiling)|
| `Alt` + rechtersleep    | Resizen (alleen floating/Stacking)              |
| Slepen op tile-grens    | Verhouding tussen tiles aanpassen (geen mod nodig) |

(`mod` = `Super` of `Alt`, instelbaar via `mod_key` in de config.)

## Ontbreekt nog (bewust, voor latere stappen)

- Meerdere master-vensters
- Workspace-management
- Panel/launcher/settings-app
- Numlock/Capslock-onafhankelijke keybindings (een bekende beperking van
  `XGrabKey` met een vaste modifier-mask, nog niet opgelost)

## Bouwen

Vereist: CMake ≥ 3.16, een C++17-compiler, en X11-development headers
(`libx11-dev` op Debian/Ubuntu).

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
```

## Testen (belangrijk: NIET als je hoofd-WM!)

Test bens-dm eerst in een geneste X-server, zodat je desktop niet crasht
als er een bug in zit:

```sh
Xephyr -screen 1280x800 :1 &
DISPLAY=:1 ./build/bens-dm
```

Zodra dit stabiel is, testen we via een aparte TTY/X11-sessie op een echt
systeem, daarna in QEMU/KVM met de volledige Bens OS ISO, en pas als
laatste op fysieke hardware — conform de teststrategie.

De layout-wiskunde zelf kun je los testen zonder X-server:

```sh
cd build
ctest --output-on-failure
```

## Installeren als echte sessie

```sh
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr
sudo make install
```

Dit zet de binary op `/usr/bin/bens-dm` en het sessiebestand op
`/usr/share/xsessions/bens-dm.desktop`, waarna "Bens DM" verschijnt als
kiesbare sessie in je display manager (LightDM, SDDM, ...). Gebruik je
`startx`/`xinit` in plaats van een display manager, zie
`assets/xinitrc.example`.

## Architectuur

```
bens-dm/
├── src/
│   ├── main.cpp
│   ├── wm/            # WindowManager-kern (stap 1)
│   ├── layouts/        # Master-stack tiling-engine (stap 2)
│   ├── input/           # (volgt) losgetrokken keybinding-config
│   ├── x11/              # (volgt) X11-hulpfuncties/wrappers
│   └── config/          # (volgt) configuratiebestand-parsing
├── include/bensdm/
├── assets/
├── tests/
├── CMakeLists.txt
└── README.md
```
