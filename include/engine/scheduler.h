#pragma once
#include <stdint.h>
#include "model/model.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum OpCode {
        OP_NOP = 0,

        // Motion
        OP_MOVE_STEPS,   // a = steps
        OP_TURN_DEG,     // a = degrees (positive = turn right)

        // Control
        OP_WAIT_MS,      // a = milliseconds
        OP_REPEAT_BEGIN, // count = repeat count, jump = index after matching END
        OP_REPEAT_END,   // (uses repeat stack)

        OP_END
    } OpCode;

    typedef struct Instr {
        uint64_t id;
        OpCode op;
        double a;   // numeric parameter (steps/deg/ms)
        int jump;   // for REPEAT_BEGIN: where to jump when count <= 0 (index after loop)
        int count;  // for REPEAT_BEGIN: repeat count
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