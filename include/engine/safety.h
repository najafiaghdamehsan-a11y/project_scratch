#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    // monotonic time in milliseconds (engine-safe; no SDL)
    uint64_t time_now_ms(void);

    // helpers
    double clampd(double v, double lo, double hi);
    double wrap_angle_deg(double deg); // keep angle in [0, 360)

    // watchdog: returns 1 if you still can execute a step, 0 if budget exhausted
    int watchdog_allow_step(int* budget);

#ifdef __cplusplus
}
#endif