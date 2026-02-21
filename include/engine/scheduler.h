#pragma once
#include <stdint.h>
#include "model/model.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum OpCode {
        OP_NOP = 0,

        // Motion
        OP_MOVE_STEPS,    // a = steps
        OP_TURN_DEG,      // a = degrees (positive = turn right)

        // Control
        OP_WAIT_MS,       // a = milliseconds
        OP_REPEAT_BEGIN,  // count = repeat count, jump = index after matching END
        OP_REPEAT_END,    // (uses repeat stack)

        // IF / ELSE
        OP_IF_BEGIN,      // cond + a used, jump = index of ELSE/FALSE branch start
        OP_ELSE,          // jump = index after ENDIF (skip false branch)
        OP_ENDIF,

        OP_END
    } OpCode;

    typedef enum CondCode {
        COND_TRUE = 0,
        COND_SPRITE_X_GT,   // sprite.x > a
        COND_SPRITE_X_LT,   // sprite.x < a
        COND_SPRITE_Y_GT,   // sprite.y > a
        COND_SPRITE_Y_LT,   // sprite.y < a
        COND_RANDOM_LT      // random(0..1) < a
    } CondCode;

    typedef struct Instr {
        uint64_t id;
        OpCode op;

        double a;     // steps/deg/ms OR condition threshold
        int jump;     // for loops/if/else jumps
        int count;    // for repeat begin
        CondCode cond;// for OP_IF_BEGIN (otherwise can be COND_TRUE)
    } Instr;

    typedef struct Thread {
        int active;
        int pc;
        uint64_t wake_ms;

        const Instr* code;
        int code_len;

        // Repeat stack (nested repeats)
        int rep_left[32];
        int rep_begin_pc[32];
        int rep_top;
    } Thread;

    typedef struct Scheduler {
        Thread threads[16];
        int rr_index; // round-robin pointer
    } Scheduler;

    void scheduler_init(Scheduler* s);
    void scheduler_start_demo(Scheduler* s);
    void scheduler_stop_all(Scheduler* s);

    // Executes at most ONE instruction across all threads (round-robin).
    // Returns 1 if executed something, 0 if nothing executed (all waiting/inactive).
    int scheduler_step_one(Scheduler* s, Project* p, uint64_t now_ms, uint64_t* out_block_id, int* budget);

#ifdef __cplusplus
}
#endif