#include "bensdm/config.hpp"
#include "bensdm/window_manager.hpp"

#include <cstdio>

int main() {
    bensdm::Config config = bensdm::Config::LoadDefaultOrFile();

    // "" laat WindowManager::Create de $DISPLAY env var gebruiken, zoals
    // gangbaar is voor X11-clients/WMs.
    bensdm::WindowManager* wm = bensdm::WindowManager::Create(config);
    if (wm == nullptr) {
        std::fprintf(stderr, "bens-dm: initialisatie mislukt.\n");
        return 1;
    }
    wm->Run();
    delete wm; // wordt in de praktijk nooit bereikt (Run() is blokkerend)
    return 0;
}
