#pragma once
#include <stdint.h>
#include "model/model.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum OpCode {
        OP_NOP = 0,
        OP_MOVE_X,
        OP_WAIT_MS,
        OP_END
    } OpCode;

    typedef struct Instr {
        uint64_t id;
        OpCode op;
        double a;
    } Instr;

    typedef struct Thread {
        int active;
        int pc;
        uint64_t wake_ms;

        // which script this thread is running
        const Instr* code;
        int code_len;
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