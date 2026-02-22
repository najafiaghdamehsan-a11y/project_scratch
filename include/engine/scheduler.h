#pragma once
#include <stdint.h>
#include "model/model.h"
#include "engine/value.h"
#include "engine/vars.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OpCode {
    OP_NOP = 0,

    // Motion
    OP_MOVE_STEPS,
    OP_TURN_DEG,
    OP_SET_X,
    OP_SET_Y,
    OP_CHANGE_X,
    OP_CHANGE_Y,
    OP_GOTO_RANDOM,
    OP_IF_ON_EDGE_BOUNCE,

    // Control
    OP_WAIT_MS,
    OP_REPEAT_BEGIN,
    OP_REPEAT_END,

    OP_FOREVER_BEGIN,
    OP_FOREVER_END,

    OP_IF_BEGIN,
    OP_ELSE,
    OP_ENDIF,

    // Operators / Stack
    OP_PUSH_NUM,
    OP_RANDOM_RANGE,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_GT, OP_LT, OP_EQ,
    OP_AND, OP_OR, OP_NOT,
    OP_IF_POP,
    OP_CHANGE_X_POP,
    OP_CHANGE_Y_POP,

    // Variables
    OP_READ_VAR_PUSH,
    OP_SET_VAR_POP,
    OP_CHANGE_VAR_POP,

    // stack-based setters
    OP_SET_X_POP,
    OP_SET_Y_POP,

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

    double a;
    double b;
    int jump;
    int count;
    CondCode cond;
} Instr;

typedef struct Thread {
    int active;
    int pc;
    uint64_t wake_ms;

    const Instr* code;
    int code_len;

    int rep_left[32];
    int rep_begin_pc[32];
    int rep_top;

    Value stack[64];
    int sp;
} Thread;

typedef struct Scheduler {
    Thread threads[16];
    int rr_index;
} Scheduler;

// NEW: script descriptor for starting many stacks
typedef struct ScriptDef {
    const Instr* code;
    int len;
} ScriptDef;

void scheduler_init(Scheduler* s);
void scheduler_start_demo(Scheduler* s);
void scheduler_stop_all(Scheduler* s);

void scheduler_start_on_key(Scheduler* s, int keycode);
void scheduler_broadcast(Scheduler* s, int msg_id);
void scheduler_start_custom(Scheduler* s, const Instr* code, int len);

// NEW: start N scripts concurrently on threads[0..N-1]
void scheduler_start_many(Scheduler* s, const ScriptDef* scripts, int count);

// takes VarStore*
int scheduler_step_one(Scheduler* s, Project* p, VarStore* vars,
                       uint64_t now_ms, uint64_t* out_block_id, int* budget);

#ifdef __cplusplus
}
#endif