#pragma once
#include <stdint.h>
#include "model/model.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum OpCode {
        OP_NOP = 0,

        // Motion
        OP_MOVE_STEPS,     // a = steps
        OP_TURN_DEG,       // a = degrees
        OP_SET_X,          // a = x
        OP_SET_Y,          // a = y
        OP_CHANGE_X,       // a = dx
        OP_CHANGE_Y,       // a = dy
        OP_GOTO_RANDOM,    // no params
        OP_IF_ON_EDGE_BOUNCE, // no params

        // Control
        OP_WAIT_MS,        // a = ms
        OP_REPEAT_BEGIN,   // count = repeat count, jump = index after matching END
        OP_REPEAT_END,

        // Forever
        OP_FOREVER_BEGIN,  // no params (optional marker)
        OP_FOREVER_END,    // jump = index to jump back to

        // IF / ELSE
        OP_IF_BEGIN,       // cond + a used, jump = index of ELSE/FALSE branch start
        OP_ELSE,           // jump = index after ENDIF
        OP_ENDIF,

        OP_END
    } OpCode;

    typedef enum CondCode {
        COND_TRUE = 0,
        COND_SPRITE_X_GT,
        COND_SPRITE_X_LT,
        COND_SPRITE_Y_GT,
        COND_SPRITE_Y_LT,
        COND_RANDOM_LT
    } CondCode;

    typedef struct Instr {
        uint64_t id;
        OpCode op;

        double a;      // primary parameter (steps/deg/ms/threshold/etc.)
        double b;      // secondary parameter (unused for most ops; kept for future)
        int jump;      // loops/if/else jumps
        int count;     // repeat count
        CondCode cond; // for OP_IF_BEGIN
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
        int rr_index;
    } Scheduler;

    void scheduler_init(Scheduler* s);
    void scheduler_start_demo(Scheduler* s);
    void scheduler_stop_all(Scheduler* s);

    int scheduler_step_one(Scheduler* s, Project* p, uint64_t now_ms, uint64_t* out_block_id, int* budget);

#ifdef __cplusplus
}
#endif