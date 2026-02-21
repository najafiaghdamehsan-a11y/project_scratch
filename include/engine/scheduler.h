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
        OP_TURN_DEG,       // a = degrees (positive = turn right)

        // Control
        OP_WAIT_MS,        // a = ms
        OP_REPEAT_BEGIN,   // count = repeat count, jump = index of matching END
        OP_REPEAT_END,     // jump = index of matching BEGIN
        OP_FOREVER_BEGIN,  // jump = index of matching END
        OP_FOREVER_END,    // jump = index of matching BEGIN

        OP_END
    } OpCode;

    typedef struct Instr {
        uint64_t id;   // debug highlight id
        OpCode op;
        double a;      // numeric parameter
        int jump;      // for loops: where to jump
        int count;     // for repeat begin: how many times
    } Instr;

    typedef struct Thread {
        int active;
        int pc;
        uint64_t wake_ms;

        // Repeat stack (supports nested repeats)
        int rep_left[32];
        int rep_begin_pc[32];
        int rep_top;
    } Thread;

    typedef struct Scheduler {
        Thread t;             // 1 thread for now
        const Instr* code;
        int code_len;
    } Scheduler;

    void scheduler_init(Scheduler* s);
    void scheduler_start_demo(Scheduler* s);
    void scheduler_stop_all(Scheduler* s);

    // Executes at most ONE instruction per call.
    // Returns 1 if executed, 0 if nothing executed (waiting/inactive).
    int scheduler_step_one(Scheduler* s, Project* p, uint64_t now_ms, uint64_t* out_block_id, int* budget);

#ifdef __cplusplus
}
#endif