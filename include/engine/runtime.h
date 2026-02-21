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

        int running;              // 1 => scripts are running (after green flag)
        int stop_all;             // 1 => request stop all (one-shot)
        uint64_t current_block_id;// for debug highlight

        Scheduler sched;          // scheduler state

        // Key event plumbing
        int key_pending;          // 1 => a key event waiting
        int last_key;             // last keycode (SDL_Keycode as int)

        // NEW: broadcast plumbing
        int msg_pending;          // 1 => broadcast waiting
        int msg_id;               // message id
    } Runtime;

    void runtime_init(Runtime* r);
    void runtime_set_paused(Runtime* r, int paused);
    void runtime_set_step_mode(Runtime* r, int step_mode);
    void runtime_request_step(Runtime* r);

    void runtime_green_flag(Runtime* r);
    void runtime_stop_all(Runtime* r);
    int  runtime_is_running(const Runtime* r);
    uint64_t runtime_current_block(const Runtime* r);

    // UI -> Engine key event
    void runtime_post_key(Runtime* r, int keycode);

    // NEW: Engine broadcast event (UI/model later)
    void runtime_post_broadcast(Runtime* r, int msg_id);

    void runtime_tick(Runtime* r, Project* p);

#ifdef __cplusplus
}
#endif