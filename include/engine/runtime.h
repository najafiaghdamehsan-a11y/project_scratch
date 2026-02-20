#pragma once
#include "model/model.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct Runtime {
        uint64_t cycle;
        int paused;
        int step_mode;   // 1 => step-by-step
        int do_step;     // UI sets this to 1 to execute exactly one step
    } Runtime;

    void runtime_init(Runtime* r);
    void runtime_set_paused(Runtime* r, int paused);
    void runtime_set_step_mode(Runtime* r, int step_mode);
    void runtime_request_step(Runtime* r);

    // “One tick” of execution (later you’ll run scripts here)
    void runtime_tick(Runtime* r, Project* p);

#ifdef __cplusplus
}
#endif