#pragma once
#include "model/model.h"
#include "engine/scheduler.h"
#include "engine/vars.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RUNTIME_MAX_SCRIPTS 16
#define RUNTIME_MAX_CODE_PER_SCRIPT 512

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

        // NEW: workspace scripts (compiled from multiple stacks)
        Instr scripts[RUNTIME_MAX_SCRIPTS][RUNTIME_MAX_CODE_PER_SCRIPT];
        int   script_len[RUNTIME_MAX_SCRIPTS];
        int   script_count;
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

    // NEW: set the scripts that green flag will run.
    // Copies into runtime storage (safe).
    // Returns 1 on success, 0 on failure (too many / too big).
    int runtime_set_scripts(Runtime* r, const ScriptDef* scripts, int count);

    void runtime_tick(Runtime* r, Project* p);

#ifdef __cplusplus
}
#endif