#pragma once
#include <stdint.h>
#include "model/model.h"

#ifdef __cplusplus
extern "C" {
#endif

    // Very small "instruction" set for demo
    typedef enum OpCode {
        OP_NOP = 0,
        OP_MOVE_X,      // a = delta x
        OP_WAIT_MS,     // a = milliseconds
        OP_END
    } OpCode;

    typedef struct Instr {
        uint64_t id;   // for debug highlight
        OpCode op;
        double a;
    } Instr;

    typedef struct Thread {
        int active;
        int pc;
        uint64_t wake_ms;
    } Thread;

    typedef struct Scheduler {
        Thread t;             // 1 thread for now (super simple)
        const Instr* code;
        int code_len;
    } Scheduler;

    void scheduler_init(Scheduler* s);
    void scheduler_start_demo(Scheduler* s);
    void scheduler_stop_all(Scheduler* s);

    // Executes at most ONE instruction per call.
    // Returns:
    //  1 => executed an instruction
    //  0 => nothing executed (waiting or inactive)
    int scheduler_step_one(Scheduler* s, Project* p, uint64_t now_ms, uint64_t* out_block_id, int* budget);

#ifdef __cplusplus
}
#endiff