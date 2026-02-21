#include "engine/scheduler.h"
#include "engine/safety.h"
#include <cmath>
#include <cstdint>

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
    return (double)(xorshift32() & 0xFFFFFFu) / (double)0x1000000u; // [0,1)
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

// Scratch direction: 0=up, 90=right
static void sprite_move_steps(Sprite* s, double steps) {
    double d = wrap_angle_deg(s->dir);
    double r = deg2rad(d);
    s->x += steps * std::sin(r);
    s->y += steps * std::cos(r);
}

static void sprite_bounce_if_needed(Sprite* s) {
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

// ---- Value stack helpers (per thread) ----
static void stack_reset(Thread* t) {
    t->sp = 0;
}

static int stack_push(Thread* t, Value v) {
    if (t->sp >= 64) return 0;
    t->stack[t->sp++] = v;
    return 1;
}

static int stack_pop(Thread* t, Value* out) {
    if (t->sp <= 0) return 0;
    if (out) *out = t->stack[--t->sp];
    else (void)t->stack[--t->sp];
    return 1;
}

static Value stack_pop_or_num(Thread* t, double fallback) {
    Value v;
    if (!stack_pop(t, &v)) return value_num(fallback);
    return v;
}

static Value stack_pop_or_bool(Thread* t, int fallback) {
    Value v;
    if (!stack_pop(t, &v)) return value_bool(fallback);
    return v;
}

// ---- thread start ----
static void thread_start(Thread* t, const Instr* code, int len) {
    t->active = 1;
    t->pc = 0;
    t->wake_ms = 0;
    t->code = code;
    t->code_len = len;
    t->rep_top = 0;
    stack_reset(t);
}

// ============================================================
// DEMO SCRIPTS (Green Flag)


// Script A: forever { move 8; bounce; wait 20 }
static const Instr SCRIPT_A[] = {
    {101, OP_FOREVER_BEGIN, 0,0, 0,0, COND_TRUE}, // idx 0
    {102, OP_MOVE_STEPS,    8,0, 0,0, COND_TRUE}, // idx 1
    {103, OP_IF_ON_EDGE_BOUNCE, 0,0, 0,0, COND_TRUE}, // idx 2
    {104, OP_WAIT_MS,      20,0, 0,0, COND_TRUE}, // idx 3
    {105, OP_FOREVER_END,   0,0, 0,0, COND_TRUE}, // idx 4 (jump=0 => loop to start)
};

// Script B: repeat 20 { goto random; set y 0; wait 150 } end
static const Instr SCRIPT_B[] = {
    {201, OP_REPEAT_BEGIN, 0,0, 6, 20, COND_TRUE}, // idx 0, jump->6
    {202, OP_GOTO_RANDOM,  0,0, 0,  0, COND_TRUE}, // idx 1
    {203, OP_SET_Y,        0,0, 0,  0, COND_TRUE}, // idx 2
    {204, OP_WAIT_MS,    150,0, 0,  0, COND_TRUE}, // idx 3
    {205, OP_REPEAT_END,   0,0, 0,  0, COND_TRUE}, // idx 4
    {206, OP_END,          0,0, 0,  0, COND_TRUE}, // idx 5
};


// KEY SCRIPTS (when key pressed)
// ============================================================

// Space (32): forever bounce mover
static const Instr KEY_SPACE[] = {
    {301, OP_FOREVER_BEGIN, 0,0, 0,0, COND_TRUE}, // idx 0
    {302, OP_MOVE_STEPS,    6,0, 0,0, COND_TRUE}, // idx 1
    {303, OP_IF_ON_EDGE_BOUNCE, 0,0, 0,0, COND_TRUE}, // idx 2
    {304, OP_WAIT_MS,      15,0, 0,0, COND_TRUE}, // idx 3
    {305, OP_FOREVER_END,   0,0, 0,0, COND_TRUE}, // idx 4
};

// 'a' (97): repeat turn+move
static const Instr KEY_A[] = {
    {401, OP_REPEAT_BEGIN, 0,0, 6, 60, COND_TRUE}, // idx 0, jump->6
    {402, OP_TURN_DEG,    10,0, 0,  0, COND_TRUE}, // idx 1
    {403, OP_MOVE_STEPS,   4,0, 0,  0, COND_TRUE}, // idx 2
    {404, OP_WAIT_MS,     20,0, 0,  0, COND_TRUE}, // idx 3
    {405, OP_REPEAT_END,   0,0, 0,  0, COND_TRUE}, // idx 4
    {406, OP_END,          0,0, 0,  0, COND_TRUE}, // idx 5
};

// 'c' (99): Operators demo (random range + change x pop)
static const Instr KEY_C[] = {
    {601, OP_REPEAT_BEGIN,  0,0, 6, 120, COND_TRUE},   // idx 0, jump->6
    {602, OP_RANDOM_RANGE, -8,8, 0,   0,  COND_TRUE},  // idx 1 push rand[-8,8]
    {603, OP_CHANGE_X_POP,  0,0, 0,   0,  COND_TRUE},  // idx 2 pop -> change x
    {604, OP_IF_ON_EDGE_BOUNCE, 0,0,0,0, COND_TRUE},   // idx 3
    {605, OP_WAIT_MS,      15,0, 0,   0,  COND_TRUE},  // idx 4
    {606, OP_REPEAT_END,    0,0, 0,   0,  COND_TRUE},  // idx 5
    {607, OP_END,           0,0, 0,   0,  COND_TRUE},  // idx 6
};

// BROADCAST RECEIVERS (when I receive msg)

// msg 1: Operators demo receiver (random jitter)
static const Instr RECV_MSG1[] = {
    {501, OP_REPEAT_BEGIN,  0,0, 6, 160, COND_TRUE},   // idx 0, jump->6
    {502, OP_RANDOM_RANGE, -12,12, 0,   0,  COND_TRUE},// idx 1 push rand[-12,12]
    {503, OP_CHANGE_X_POP,  0,0, 0,   0,  COND_TRUE},  // idx 2 pop -> change x
    {504, OP_IF_ON_EDGE_BOUNCE, 0,0,0,0, COND_TRUE},   // idx 3 bounce
    {505, OP_WAIT_MS,      10,0, 0,   0,  COND_TRUE},  // idx 4
    {506, OP_REPEAT_END,    0,0, 0,   0,  COND_TRUE},  // idx 5
    {507, OP_END,           0,0, 0,   0,  COND_TRUE},  // idx 6
};

// Scheduler lifecycle

void scheduler_init(Scheduler* s) {
    for (int i = 0; i < 16; i++) {
        s->threads[i].active = 0;
        s->threads[i].pc = 0;
        s->threads[i].wake_ms = 0;
        s->threads[i].code = nullptr;
        s->threads[i].code_len = 0;
        s->threads[i].rep_top = 0;
        stack_reset(&s->threads[i]);
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
        stack_reset(&s->threads[i]);
    }
}

void scheduler_start_demo(Scheduler* s) {
    scheduler_stop_all(s);

    thread_start(&s->threads[0], SCRIPT_A, (int)(sizeof(SCRIPT_A) / sizeof(SCRIPT_A[0])));
    thread_start(&s->threads[1], SCRIPT_B, (int)(sizeof(SCRIPT_B) / sizeof(SCRIPT_B[0])));

    s->rr_index = 0;
}

static int find_free_thread(Scheduler* s) {
    for (int i = 0; i < 16; i++) {
        if (!s->threads[i].active) return i;
    }
    return -1;
}

void scheduler_start_on_key(Scheduler* s, int keycode) {
    int idx = find_free_thread(s);
    if (idx < 0) return;

    // SDL_Keycode values for letters/spaces match ASCII:
    // space=32, 'a'=97, 'c'=99
    if (keycode == 32) { // SPACE
        thread_start(&s->threads[idx], KEY_SPACE, (int)(sizeof(KEY_SPACE) / sizeof(KEY_SPACE[0])));
        return;
    }
    if (keycode == 97) { // 'a'
        thread_start(&s->threads[idx], KEY_A, (int)(sizeof(KEY_A) / sizeof(KEY_A[0])));
        return;
    }
    if (keycode == 99) { // 'c'
        thread_start(&s->threads[idx], KEY_C, (int)(sizeof(KEY_C) / sizeof(KEY_C[0])));
        return;
    }
}

void scheduler_broadcast(Scheduler* s, int msg_id) {
    int idx = find_free_thread(s);
    if (idx < 0) return;

    if (msg_id == 1) {
        thread_start(&s->threads[idx], RECV_MSG1, (int)(sizeof(RECV_MSG1) / sizeof(RECV_MSG1[0])));
        return;
    }
    // unknown msg -> ignore
}

// Interpreter (one instruction per step)

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
        // -------------------------
        // Motion
        // -------------------------
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

        // -------------------------
        // Control
        // -------------------------
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

        case OP_FOREVER_BEGIN:
            return 1;

        case OP_FOREVER_END: {
            int j = in.jump;
            if (j < 0 || j >= t->code_len) j = 0;
            t->pc = j;
            return 1;
        }

        // -------------------------
        // IF / ELSE (non-stack)
        // -------------------------
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

        // -------------------------
        // Operators / Stack VM
        // -------------------------
        case OP_PUSH_NUM:
            stack_push(t, value_num(in.a));
            return 1;

        case OP_RANDOM_RANGE: {
            double lo = in.a;
            double hi = in.b;
            double r = rand_range(lo, hi);
            stack_push(t, value_num(r));
            return 1;
        }

        case OP_ADD: {
            double b = value_as_num(stack_pop_or_num(t, 0.0));
            double a = value_as_num(stack_pop_or_num(t, 0.0));
            stack_push(t, value_num(a + b));
            return 1;
        }

        case OP_SUB: {
            double b = value_as_num(stack_pop_or_num(t, 0.0));
            double a = value_as_num(stack_pop_or_num(t, 0.0));
            stack_push(t, value_num(a - b));
            return 1;
        }

        case OP_MUL: {
            double b = value_as_num(stack_pop_or_num(t, 0.0));
            double a = value_as_num(stack_pop_or_num(t, 0.0));
            stack_push(t, value_num(a * b));
            return 1;
        }

        case OP_DIV: {
            double b = value_as_num(stack_pop_or_num(t, 1.0));
            double a = value_as_num(stack_pop_or_num(t, 0.0));
            if (b == 0.0) stack_push(t, value_num(0.0));
            else stack_push(t, value_num(a / b));
            return 1;
        }

        case OP_GT: {
            double b = value_as_num(stack_pop_or_num(t, 0.0));
            double a = value_as_num(stack_pop_or_num(t, 0.0));
            stack_push(t, value_bool(a > b));
            return 1;
        }

        case OP_LT: {
            double b = value_as_num(stack_pop_or_num(t, 0.0));
            double a = value_as_num(stack_pop_or_num(t, 0.0));
            stack_push(t, value_bool(a < b));
            return 1;
        }

        case OP_EQ: {
            double b = value_as_num(stack_pop_or_num(t, 0.0));
            double a = value_as_num(stack_pop_or_num(t, 0.0));
            stack_push(t, value_bool(a == b));
            return 1;
        }

        case OP_AND: {
            int b = value_as_bool(stack_pop_or_bool(t, 0));
            int a = value_as_bool(stack_pop_or_bool(t, 0));
            stack_push(t, value_bool(a && b));
            return 1;
        }

        case OP_OR: {
            int b = value_as_bool(stack_pop_or_bool(t, 0));
            int a = value_as_bool(stack_pop_or_bool(t, 0));
            stack_push(t, value_bool(a || b));
            return 1;
        }

        case OP_NOT: {
            int a = value_as_bool(stack_pop_or_bool(t, 0));
            stack_push(t, value_bool(!a));
            return 1;
        }

        case OP_IF_POP: {
            int cond = value_as_bool(stack_pop_or_bool(t, 0));
            if (!cond) t->pc = in.jump;
            return 1;
        }

        case OP_CHANGE_X_POP: {
            double dx = value_as_num(stack_pop_or_num(t, 0.0));
            if (spr) spr->x = clampd(spr->x + dx, -STAGE_HALF_W, STAGE_HALF_W);
            return 1;
        }

        case OP_CHANGE_Y_POP: {
            double dy = value_as_num(stack_pop_or_num(t, 0.0));
            if (spr) spr->y = clampd(spr->y + dy, -STAGE_HALF_H, STAGE_HALF_H);
            return 1;
        }

        // -------------------------
        // End
        // -------------------------
        case OP_END:
            t->active = 0;
            return 1;

        default:
            return 1;
    }
}

int scheduler_step_one(Scheduler* s, Project* p, uint64_t now_ms, uint64_t* out_block_id, int* budget) {
    // round-robin: find one runnable thread
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