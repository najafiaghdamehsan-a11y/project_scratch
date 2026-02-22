#pragma once
#include "model/model.h"
#include "engine/scheduler.h"
#include "engine/vars.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    // Main compiled script buffer (UI compiles blocks into Instr[])
#define RUNTIME_MAX_MAIN_CODE 2048

    typedef struct Runtime {
        // Debug / stepping
        uint64_t cycle;
        int paused;
        int step_mode;   // 1 => step-by-step
        int do_step;     // UI sets to 1 to execute exactly one step

        // Scratch-like control
        int running;              // 1 => scripts running after green flag
        int stop_all;             // request stop all (one-shot)
        uint64_t current_block_id;// last executed block (for debug highlight)

        // Scheduler/interpreter
        Scheduler sched;

        // Key events
        int key_pending;
        int last_key;

        // Broadcast plumbing
        int msg_pending;
        int msg_id;

        // Variables store (engine-side)
        VarStore vars;

        // NEW: workspace compiled code (single top-stack for now)
        Instr main_code[RUNTIME_MAX_MAIN_CODE];
        int   main_code_len;
    } Runtime;

    void runtime_init(Runtime* r);
    void runtime_set_paused(Runtime* r, int paused);
    void runtime_set_step_mode(Runtime* r, int step_mode);
    void runtime_request_step(Runtime* r);

    void runtime_green_flag(Runtime* r);
    void runtime_stop_all(Runtime* r);
    int  runtime_is_running(const Runtime* r);
    uint64_t runtime_current_block(const Runtime* r);

    void runtime_post_key(Runtime* r, int keycode);
    void runtime_post_broadcast(Runtime* r, int msg_id);

    // NEW: set the script that green flag should run
    // returns 1 on success, 0 on failure (e.g., too large)
    int runtime_set_main_script(Runtime* r, const Instr* code, int len);

    void runtime_tick(Runtime* r, Project* p);

#ifdef __cplusplus
}
#endif