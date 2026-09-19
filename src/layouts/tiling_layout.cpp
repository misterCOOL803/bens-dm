#include "bensdm/layout.hpp"

#include <algorithm>

namespace bensdm {
namespace layouts {

std::unordered_map<Window, Rect> ComputeTiling(
    const std::vector<Window>& windows,
    int screen_width,
    int screen_height,
    double master_ratio,
    int gap,
    const std::vector<double>& stack_weights) {
    std::unordered_map<Window, Rect> result;
    if (windows.empty()) return result;

    const int usable_w = screen_width - 2 * gap;
    const int usable_h = screen_height - 2 * gap;

    // Eén venster: gewoon het hele (bruikbare) scherm.
    if (windows.size() == 1) {
        result[windows[0]] = Rect{
            gap, gap,
            static_cast<unsigned int>(usable_w),
            static_cast<unsigned int>(usable_h)};
        return result;
    }

    // Master: linkerdeel, volledige hoogte.
    const int master_w = static_cast<int>(usable_w * master_ratio) - gap / 2;
    result[windows[0]] = Rect{
        gap, gap,
        static_cast<unsigned int>(master_w),
        static_cast<unsigned int>(usable_h)};

    // Stack: rechterdeel, verticaal verdeeld over de overige vensters,
    // met een instelbaar gewicht per venster (default: gelijk verdeeld).
    const int stack_x = gap + master_w + gap;
    const int stack_w = usable_w - master_w - gap;
    const int stack_count = static_cast<int>(windows.size()) - 1;

    std::vector<double> weights = stack_weights;
    if (static_cast<int>(weights.size()) != stack_count) {
        weights.assign(static_cast<size_t>(stack_count), 1.0);
    }
    double weight_sum = 0.0;
    for (double w : weights) weight_sum += w;
    if (weight_sum <= 0.0) weight_sum = 1.0;

    const int usable_stack_h = usable_h - gap * (stack_count - 1);

    int y = gap;
    for (int i = 0; i < stack_count; ++i) {
        const bool is_last = (i == stack_count - 1);
        // De laatste krijgt de eventuele afrondingsrest, zodat het onderste
        // venster altijd exact tot de onderrand (min gap) doorloopt i.p.v.
        // een paar pixels tekort/over te houden door integer-deling of
        // opgetelde afrondingsfouten in de gewogen verdeling.
        int h;
        if (is_last) {
            h = screen_height - gap - y;
        } else {
            h = static_cast<int>(usable_stack_h * (weights[i] / weight_sum));
            h = std::max(h, 20); // voorkom onzichtbare/negatieve vensters bij extreme sleepacties
        }

        result[windows[i + 1]] = Rect{
            stack_x, y,
            static_cast<unsigned int>(stack_w),
            static_cast<unsigned int>(h)};

        y += h + gap;
    }

    return result;
}

} // namespace layouts
} // namespace bensdm
