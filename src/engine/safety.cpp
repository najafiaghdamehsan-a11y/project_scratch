#include "engine/safety.h"
#include <chrono>

uint64_t time_now_ms(void) {
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()
    ).count();
}

double clampd(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

double wrap_angle_deg(double deg) {
    while (deg >= 360.0) deg -= 360.0;
    while (deg < 0.0) deg += 360.0;
    return deg;
}

int watchdog_allow_step(int* budget) {
    if (!budget) return 0;
    if (*budget <= 0) return 0;
    (*budget)--;
    return 1;
}