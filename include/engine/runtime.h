#pragma once
#include "model/model.h"
#include "engine/scheduler.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct Runtime {
        uint64_t cycle;
        int paused;
        int step_mode;   // 1 => step-by-step
        int do_step;     // UI sets this to 1 to execute exactly one step

        // Scratch-like control
        int running;              // 1 => scripts are running (after green flag)
        int stop_all;             // 1 => request stop all (one-shot)
        uint64_t current_block_id;// for debug highlight

        // NEW: scheduler state
        Scheduler sched;
    } Runtime;

    void runtime_init(Runtime* r);
    void runtime_set_paused(Runtime* r, int paused);
    void runtime_set_step_mode(Runtime* r, int step_mode);
    void runtime_request_step(Runtime* r);

    void runtime_green_flag(Runtime* r);   // start running
    void runtime_stop_all(Runtime* r);     // stop everything
    int  runtime_is_running(const Runtime* r);
    uint64_t runtime_current_block(const Runtime* r);

    void runtime_tick(Runtime* r, Project* p);

#ifdef __cplusplus
}
#endif