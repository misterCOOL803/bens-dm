// Simpele, dependency-vrije test voor layouts::ComputeTiling.
// Draait zonder X-display: Window is gewoon een unsigned long (XID),
// we hebben er geen echte X-server voor nodig.

#include "bensdm/layout.hpp"

#include <cassert>
#include <cstdio>

using bensdm::Rect;
using bensdm::layouts::ComputeTiling;

namespace {

void TestSingleWindowFillsScreen() {
    auto rects = ComputeTiling({1}, 1920, 1080, 0.55, 10);
    assert(rects.size() == 1);
    const Rect& r = rects.at(1);
    assert(r.x == 10 && r.y == 10);
    assert(r.width == 1920 - 20);
    assert(r.height == 1080 - 20);
    std::printf("OK: TestSingleWindowFillsScreen\n");
}

void TestTwoWindowsNoOverlapNoGapLeft() {
    auto rects = ComputeTiling({1, 2}, 1920, 1080, 0.5, 10);
    assert(rects.size() == 2);
    const Rect& master = rects.at(1);
    const Rect& stack = rects.at(2);

    // Master begint aan de linkerkant met de buitenrand-gap.
    assert(master.x == 10);
    // De stack begint rechts van de master plus de tussenruimte.
    assert(stack.x == static_cast<int>(master.x + master.width) + 10);
    // Samen (inclusief gaps) mogen ze niet buiten het scherm vallen.
    int right_edge = stack.x + static_cast<int>(stack.width);
    assert(right_edge == 1920 - 10);
    // Beide vullen de volledige bruikbare hoogte bij één stack-venster.
    assert(master.height == 1080 - 20);
    assert(stack.height == 1080 - 20);
    std::printf("OK: TestTwoWindowsNoOverlapNoGapLeft\n");
}

void TestThreeWindowsStackFillsHeightExactly() {
    auto rects = ComputeTiling({1, 2, 3}, 1920, 1080, 0.55, 8);
    assert(rects.size() == 3);
    const Rect& s1 = rects.at(2);
    const Rect& s2 = rects.at(3);

    // s1 boven, s2 eronder met een gap ertussen.
    assert(s2.y == s1.y + static_cast<int>(s1.height) + 8);
    // De onderste (laatste) stack-venster moet exact tot de onderrand lopen,
    // ondanks integer-afronding — dit is precies waarom ComputeTiling de
    // rest van de deling aan het laatste venster geeft.
    assert(s2.y + static_cast<int>(s2.height) == 1080 - 8);
    std::printf("OK: TestThreeWindowsStackFillsHeightExactly\n");
}

void TestEmptyInputReturnsEmpty() {
    auto rects = ComputeTiling({}, 1920, 1080);
    assert(rects.empty());
    std::printf("OK: TestEmptyInputReturnsEmpty\n");
}

void TestStackWeightsControlHeightRatio() {
    // Twee stack-vensters, verhouding 3:1 -> het eerste moet duidelijk
    // hoger zijn dan het tweede.
    auto rects = ComputeTiling({1, 2, 3}, 1920, 1080, 0.5, 8, {3.0, 1.0});
    assert(rects.size() == 3);
    const Rect& s1 = rects.at(2);
    const Rect& s2 = rects.at(3);
    assert(s1.height > s2.height);
    // Grove sanity-check op de verhouding (met marge voor het afrondings-
    // gedrag van het laatste venster).
    double ratio = static_cast<double>(s1.height) / static_cast<double>(s2.height);
    assert(ratio > 2.0 && ratio < 4.0);
    std::printf("OK: TestStackWeightsControlHeightRatio\n");
}

void TestMismatchedWeightsFallBackToEqual() {
    // Verkeerd aantal gewichten (2 in plaats van 1 voor 2 stack-vensters)
    // moet stilzwijgend terugvallen op gelijke verdeling i.p.v. crashen.
    auto rects = ComputeTiling({1, 2}, 1920, 1080, 0.5, 8, {3.0, 1.0, 5.0});
    assert(rects.size() == 2);
    std::printf("OK: TestMismatchedWeightsFallBackToEqual\n");
}

} // namespace

int main() {
    TestSingleWindowFillsScreen();
    TestTwoWindowsNoOverlapNoGapLeft();
    TestThreeWindowsStackFillsHeightExactly();
    TestEmptyInputReturnsEmpty();
    TestStackWeightsControlHeightRatio();
    TestMismatchedWeightsFallBackToEqual();
    std::printf("Alle layout-tests geslaagd.\n");
    return 0;
}
