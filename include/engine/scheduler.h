#pragma once
#include <stdint.h>
#include "model/model.h"
#include "engine/value.h"   // NEW (Value + conversions)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OpCode {
    OP_NOP = 0,

    // -------------------------
    // Motion
    // -------------------------
    OP_MOVE_STEPS,           // a = steps
    OP_TURN_DEG,             // a = degrees
    OP_SET_X,                // a = x
    OP_SET_Y,                // a = y
    OP_CHANGE_X,             // a = dx
    OP_CHANGE_Y,             // a = dy
    OP_GOTO_RANDOM,          // no params
    OP_IF_ON_EDGE_BOUNCE,    // no params

    // -------------------------
    // Control
    // -------------------------
    OP_WAIT_MS,              // a = ms
    OP_REPEAT_BEGIN,         // count = repeat count, jump = index after matching END
    OP_REPEAT_END,

    // Forever
    OP_FOREVER_BEGIN,
    OP_FOREVER_END,          // jump = index to jump back to (use 0 for "loop to start")

    // IF / ELSE (non-stack, uses cond + a)
    OP_IF_BEGIN,             // if cond(a) false => pc = jump
    OP_ELSE,                 // pc = jump (skip false branch)
    OP_ENDIF,

    // -------------------------
    // Operators / Stack VM (NEW)
    // -------------------------
    OP_PUSH_NUM,             // a = number (push)
    OP_RANDOM_RANGE,         // a=lo, b=hi (push random)
    OP_ADD,                  // pop2 -> push (a+b)
    OP_SUB,                  // pop2 -> push (a-b)
    OP_MUL,                  // pop2 -> push (a*b)
    OP_DIV,                  // pop2 -> push (a/b) (handle div0 in impl)
    OP_GT,                   // pop2 -> push bool (a>b)
    OP_LT,                   // pop2 -> push bool (a<b)
    OP_EQ,                   // pop2 -> push bool (a==b)
    OP_AND,                  // pop2 -> push bool
    OP_OR,                   // pop2 -> push bool
    OP_NOT,                  // pop1 -> push bool

    // Stack-based IF (pops bool)
    OP_IF_POP,               // pop bool; if false => pc = jump

    // Stack-based motion (pops number)
    OP_CHANGE_X_POP,         // pop num; sprite.x += num
    OP_CHANGE_Y_POP,         // pop num; sprite.y += num

    // End of script
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

    // Generic params (used differently depending on op)
    double a;
    double b;

    // Jumps / counts for control-flow
    int jump;       // loops/if/else/if_pop jump target
    int count;      // repeat count

    // For OP_IF_BEGIN (non-stack if)
    CondCode cond;
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

    // NEW: Value stack for Operators
    Value stack[64];
    int sp; // stack pointer (0..64)
} Thread;

typedef struct Scheduler {
    Thread threads[16];
    int rr_index; // round-robin pointer
} Scheduler;

void scheduler_init(Scheduler* s);
void scheduler_start_demo(Scheduler* s);
void scheduler_stop_all(Scheduler* s);

// Start scripts on key press (SDL_Keycode as int)
void scheduler_start_on_key(Scheduler* s, int keycode);

// Broadcast -> start receiver threads
void scheduler_broadcast(Scheduler* s, int msg_id);

// Executes at most ONE instruction across all threads (round-robin).
// Returns 1 if executed something, 0 if nothing executed (all waiting/inactive).
int scheduler_step_one(Scheduler* s, Project* p, uint64_t now_ms, uint64_t* out_block_id, int* budget);

#ifdef __cplusplus
}
#endif