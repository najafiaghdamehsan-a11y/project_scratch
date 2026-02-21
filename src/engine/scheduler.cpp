#include "engine/scheduler.h"
#include "engine/safety.h"
#include <cmath>

// Scratch stage size (logical coords)
static const double STAGE_HALF_W = 240.0;
static const double STAGE_HALF_H = 180.0;

// ---- tiny RNG (no SDL) ----
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
static double rand_range(double lo, double hi) {
    return lo + (hi - lo) * rand01();
}

// ---- math helpers ----
static double deg2rad(double deg) {
    return deg * 3.14159265358979323846 / 180.0;
}
static double rad2deg(double rad) {
    return rad * 180.0 / 3.14159265358979323846;
}

// Scratch-like convention: 0=up, 90=right
static void sprite_move_steps(Sprite* s, double steps) {
    double d = wrap_angle_deg(s->dir);
    double r = deg2rad(d);
    s->x += steps * std::sin(r);
    s->y += steps * std::cos(r);
}

static void sprite_bounce_if_needed(Sprite* s) {
    // direction vector (dx,dy) where 0=up => dx=sin, dy=cos
    double d = wrap_angle_deg(s->dir);
    double r = deg2rad(d);
    double dx = std::sin(r);
    double dy = std::cos(r);

    int hit_v = 0;
    int hit_h = 0;

    if (s->x > STAGE_HALF_W) { s->x = STAGE_HALF_W; hit_v = 1; }
    if (s->x < -STAGE_HALF_W){ s->x = -STAGE_HALF_W; hit_v = 1; }
    if (s->y > STAGE_HALF_H) { s->y = STAGE_HALF_H; hit_h = 1; }
    if (s->y < -STAGE_HALF_H){ s->y = -STAGE_HALF_H; hit_h = 1; }

    if (!hit_v && !hit_h) return;

    if (hit_v) dx = -dx;
    if (hit_h) dy = -dy;

    // convert back to Scratch direction
    // we used dx=sin, dy=cos => direction = atan2(dx, dy)
    double new_deg = rad2deg(std::atan2(dx, dy));
    s->dir = wrap_angle_deg(new_deg);
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

// ---- Demo scripts ----
//
// Script A: forever {
//   move 8
//   if on edge bounce
//   wait 20ms
// }
static const Instr SCRIPT_A[] = {
    {101, OP_FOREVER_BEGIN, 0,0, 0,0, COND_TRUE},
    {102, OP_MOVE_STEPS,    8,0, 0,0, COND_TRUE},
    {103, OP_IF_ON_EDGE_BOUNCE, 0,0, 0,0, COND_TRUE},
    {104, OP_WAIT_MS,      20,0, 0,0, COND_TRUE},
    // forever end jumps back to index 0
    {105, OP_FOREVER_END,   0,0, 0,0, COND_TRUE}, // jump set below in code (we’ll hardcode in handler)
};

// Script B: repeat 20 {
//   go to random position
//   set y 0
//   wait 150ms
// }
static const Instr SCRIPT_B[] = {
    {201, OP_REPEAT_BEGIN, 0,0, 5, 20, COND_TRUE},
    {202, OP_GOTO_RANDOM,  0,0, 0,  0, COND_TRUE},
    {203, OP_SET_Y,        0,0, 0,  0, COND_TRUE},
    {204, OP_WAIT_MS,    150,0, 0,  0, COND_TRUE},
    {205, OP_REPEAT_END,   0,0, 0,  0, COND_TRUE},
    {206, OP_END,          0,0, 0,  0, COND_TRUE},
};

void scheduler_start_demo(Scheduler* s) {
    scheduler_stop_all(s);

    thread_start(&s->threads[0], SCRIPT_A, (int)(sizeof(SCRIPT_A) / sizeof(SCRIPT_A[0])));
    thread_start(&s->threads[1], SCRIPT_B, (int)(sizeof(SCRIPT_B) / sizeof(SCRIPT_B[0])));

    s->rr_index = 0;
}

static int step_thread(Thread* t, Project* p, uint64_t now_ms, uint64_t* out_block_id, int* budget) {
    if (!t->active || !t->code || t->code_len <= 0) return 0;

    if (t->wake_ms != 0 && now_ms < t->wake_ms) return 0;
    t->wake_ms = 0;

    if (!watchdog_allow_step(budget)) return 0;

    if (t->pc < 0 || t->pc >= t->code_len) {
        t->active = 0;
        return 0;
    }

    const Instr in = t->code[t->pc++];
    if (out_block_id) *out_block_id = in.id;

    Sprite* spr = (p && p->sprite_count > 0) ? &p->sprites[0] : nullptr;

    switch (in.op) {
        // Motion
        case OP_MOVE_STEPS:
            if (spr) sprite_move_steps(spr, in.a);
            return 1;

        case OP_TURN_DEG:
            if (spr) spr->dir = wrap_angle_deg(spr->dir + in.a);
            return 1;

        case OP_SET_X:
            if (spr) spr->x = clampd(in.a, -STAGE_HALF_W, STAGE_HALF_W);
            return 1;

        case OP_SET_Y:
            if (spr) spr->y = clampd(in.a, -STAGE_HALF_H, STAGE_HALF_H);
            return 1;

        case OP_CHANGE_X:
            if (spr) spr->x = clampd(spr->x + in.a, -STAGE_HALF_W, STAGE_HALF_W);
            return 1;

        case OP_CHANGE_Y:
            if (spr) spr->y = clampd(spr->y + in.a, -STAGE_HALF_H, STAGE_HALF_H);
            return 1;

        case OP_GOTO_RANDOM:
            if (spr) {
                spr->x = rand_range(-STAGE_HALF_W, STAGE_HALF_W);
                spr->y = rand_range(-STAGE_HALF_H, STAGE_HALF_H);
            }
            return 1;

        case OP_IF_ON_EDGE_BOUNCE:
            if (spr) sprite_bounce_if_needed(spr);
            return 1;

        // Control
        case OP_WAIT_MS:
            t->wake_ms = now_ms + (uint64_t)in.a;
            return 1;

        case OP_REPEAT_BEGIN: {
            int n = in.count;
            if (n <= 0) { t->pc = in.jump; return 1; }

            if (t->rep_top >= 32) { t->active = 0; return 1; }
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

        // Forever
        case OP_FOREVER_BEGIN:
            return 1;

        case OP_FOREVER_END:
            // For demo, jump back to 0 (start of script)
            t->pc = 0;
            return 1;

        // IF/ELSE
        case OP_IF_BEGIN: {
            int ok = eval_cond(in.cond, in.a, p);
            if (!ok) t->pc = in.jump;
            return 1;
        }

        case OP_ELSE:
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