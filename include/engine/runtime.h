#pragma once
#include "model/model.h"
#include "engine/scheduler.h"
#include "engine/vars.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RUNTIME_MAX_MAIN_CODE 2048

    typedef struct Runtime {
        uint64_t cycle;
        int paused;
        int step_mode;
        int do_step;

        int running;
        int stop_all;
        uint64_t current_block_id;

        Scheduler sched;

        // Key events
        int key_pending;
        int last_key;

        // Broadcast plumbing
        int msg_pending;
        int msg_id;

        // Variables store
        VarStore vars;

        // NEW: UI-compiled “main stack” (copied here, because UI builds it on stack)
        Instr main_code[RUNTIME_MAX_MAIN_CODE];
        int   main_len;
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

    // NEW: called by UI when user presses green flag (compiled blocks)
    void runtime_set_main_script(Runtime* r, const Instr* code, int len);

    void runtime_tick(Runtime* r, Project* p);

#ifdef __cplusplus
}
#endif