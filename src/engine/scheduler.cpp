#include "engine/scheduler.h"
#include "engine/safety.h"
#include <cmath>

// ---- tiny RNG (no SDL, deterministic-ish) ----
static uint32_t g_rng = 0x12345678u;
static uint32_t xorshift32(void) {
    uint32_t x = g_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng = x;
    return x;
}
static double rand01(void) {
    return (double)(xorshift32() & 0xFFFFFF) / (double)0x1000000; // [0,1)
}

// ---- helpers ----
static double deg2rad(double deg) {
    return deg * 3.14159265358979323846 / 180.0;
}

// Scratch-like convention: 0=up, 90=right
static void sprite_move_steps(Sprite* s, double steps) {
    double d = wrap_angle_deg(s->dir);
    double r = deg2rad(d);
    s->x += steps * std::sin(r);
    s->y += steps * std::cos(r);
}

static int eval_cond(CondCode c, double a, const Project* p) {
    const Sprite* spr = (p && p->sprite_count > 0) ? &p->sprites[0] : nullptr;

    switch (c) {
        case COND_TRUE: return 1;
        case COND_SPRITE_X_GT: return spr ? (spr->x > a) : 0;
        case COND_SPRITE_X_LT: return spr ? (spr->x < a) : 0;
        case COND_SPRITE_Y_GT: return spr ? (spr->y > a) : 0;
        case COND_SPRITE_Y_LT: return spr ? (spr->y < a) : 0;
        case COND_RANDOM_LT:   return rand01() < a;
        default: return 0;
    }
}

static void thread_start(Thread* t, const Instr* code, int len) {
    t->active = 1;
    t->pc = 0;
    t->wake_ms = 0;
    t->code = code;
    t->code_len = len;
    t->rep_top = 0;
}

// ---- Demo scripts ----
//
// Script A: repeat 120 {
//   move 6
//   if (x > 200) then turn 180 else turn 15
//   wait 20ms
// }
static const Instr SCRIPT_A[] = {
    // idx:0
    {101, OP_REPEAT_BEGIN, 0.0, 9, 120, COND_TRUE},
    {102, OP_MOVE_STEPS,   6.0, 0,   0, COND_TRUE},

    // if x > 200
    // if false jump -> idx 6 (false branch start)
    {103, OP_IF_BEGIN,   200.0, 6,   0, COND_SPRITE_X_GT},

    // true branch
    {104, OP_TURN_DEG,   180.0, 0,   0, COND_TRUE},

    // else: jump to idx 8 (after ENDIF)
    {105, OP_ELSE,         0.0, 8,   0, COND_TRUE},

    // false branch
    {106, OP_TURN_DEG,    15.0, 0,   0, COND_TRUE},

    {107, OP_ENDIF,        0.0, 0,   0, COND_TRUE},

    {108, OP_WAIT_MS,     20.0, 0,   0, COND_TRUE},
    {109, OP_REPEAT_END,   0.0, 0,   0, COND_TRUE},
    {110, OP_END,          0.0, 0,   0, COND_TRUE},
};

// Script B: repeat 200 {
//   if random < 0.5 then move 3 else move -3
//   wait 15ms
// }
static const Instr SCRIPT_B[] = {
    {201, OP_REPEAT_BEGIN, 0.0, 8, 200, COND_TRUE},

    // if random < 0.5
    // if false jump -> idx 5 (false branch)
    {202, OP_IF_BEGIN,     0.5, 5,   0, COND_RANDOM_LT},

    // true branch
    {203, OP_MOVE_STEPS,   3.0, 0,   0, COND_TRUE},

    // else jump -> idx 7 (after ENDIF)
    {204, OP_ELSE,         0.0, 7,   0, COND_TRUE},

    // false branch
    {205, OP_MOVE_STEPS,  -3.0, 0,   0, COND_TRUE},

    {206, OP_ENDIF,        0.0, 0,   0, COND_TRUE},

    {207, OP_WAIT_MS,     15.0, 0,   0, COND_TRUE},
    {208, OP_REPEAT_END,   0.0, 0,   0, COND_TRUE},
    {209, OP_END,          0.0, 0,   0, COND_TRUE},
};

void scheduler_init(Scheduler* s) {
    for (int i = 0; i < 16; i++) {
        s->threads[i].active = 0;
        s->threads[i].pc = 0;
        s->threads[i].wake_ms = 0;
        s->threads[i].code = nullptr;
        s->threads[i].code_len = 0;
        s->threads[i].rep_top = 0;
    }
    s->rr_index = 0;
}

void scheduler_stop_all(Scheduler* s) {
    for (int i = 0; i < 16; i++) {
        s->threads[i].active = 0;
        s->threads[i].pc = 0;
        s->threads[i].wake_ms = 0;
        s->threads[i].code = nullptr;
        s->threads[i].code_len = 0;
        s->threads[i].rep_top = 0;
    }
}

void scheduler_start_demo(Scheduler* s) {
    scheduler_stop_all(s);

    thread_start(&s->threads[0], SCRIPT_A, (int)(sizeof(SCRIPT_A) / sizeof(SCRIPT_A[0])));
    thread_start(&s->threads[1], SCRIPT_B, (int)(sizeof(SCRIPT_B) / sizeof(SCRIPT_B[0])));

    s->rr_index = 0;
}

static int step_thread(Thread* t, Project* p, uint64_t now_ms, uint64_t* out_block_id, int* budget) {
    if (!t->active || !t->code || t->code_len <= 0) return 0;

    // waiting?
    if (t->wake_ms != 0 && now_ms < t->wake_ms) return 0;
    t->wake_ms = 0;

    // watchdog budget
    if (!watchdog_allow_step(budget)) return 0;

    if (t->pc < 0 || t->pc >= t->code_len) {
        t->active = 0;
        return 0;
    }

    const Instr in = t->code[t->pc++];
    if (out_block_id) *out_block_id = in.id;

    Sprite* spr = (p && p->sprite_count > 0) ? &p->sprites[0] : nullptr;

    switch (in.op) {
        case OP_MOVE_STEPS:
            if (spr) sprite_move_steps(spr, in.a);
            return 1;

        case OP_TURN_DEG:
            if (spr) spr->dir = wrap_angle_deg(spr->dir + in.a);
            return 1;

        case OP_WAIT_MS:
            t->wake_ms = now_ms + (uint64_t)in.a;
            return 1;

        case OP_REPEAT_BEGIN: {
            int n = in.count;

            if (n <= 0) {
                t->pc = in.jump; // skip loop
                return 1;
            }

            if (t->rep_top >= 32) { // nesting limit
                t->active = 0;
                return 1;
            }

            t->rep_left[t->rep_top] = n;
            t->rep_begin_pc[t->rep_top] = t->pc;
            t->rep_top++;
            return 1;
        }

        case OP_REPEAT_END: {
            if (t->rep_top <= 0) return 1;
            int top = t->rep_top - 1;
            t->rep_left[top]--;
            if (t->rep_left[top] > 0) {
                t->pc = t->rep_begin_pc[top];
            } else {
                t->rep_top--;
            }
            return 1;
        }

        case OP_IF_BEGIN: {
            int ok = eval_cond(in.cond, in.a, p);
            if (!ok) {
                // jump to false branch start (ELSE block or direct false body)
                t->pc = in.jump;
            }
            return 1;
        }

        case OP_ELSE:
            // skip false branch
            t->pc = in.jump;
            return 1;

        case OP_ENDIF:
            return 1;

        case OP_END:
            t->active = 0;
            return 1;

        default:
            return 1;
    }
}

int scheduler_step_one(Scheduler* s, Project* p, uint64_t now_ms, uint64_t* out_block_id, int* budget) {
    for (int attempts = 0; attempts < 16; attempts++) {
        int idx = (s->rr_index + attempts) % 16;
        Thread* t = &s->threads[idx];

        uint64_t bid = 0;
        int did = step_thread(t, p, now_ms, &bid, budget);
        if (did) {
            if (out_block_id) *out_block_id = bid;
            s->rr_index = (idx + 1) % 16;
            return 1;
        }
    }
    return 0;
}