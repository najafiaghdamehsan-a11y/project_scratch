#include "engine/scheduler.h"
#include "engine/safety.h"
#include <cmath>
#include <cstdint>

// Stage size
static const double STAGE_HALF_W = 240.0;
static const double STAGE_HALF_H = 180.0;

static Sprite* active_sprite(Project* p) {
    if (!p || p->sprite_count <= 0) return nullptr;
    int i = p->active_sprite_index;
    if (i < 0 || i >= p->sprite_count) i = 0;
    return &p->sprites[i];
}
static const Sprite* active_sprite_c(const Project* p) {
    if (!p || p->sprite_count <= 0) return nullptr;
    int i = p->active_sprite_index;
    if (i < 0 || i >= p->sprite_count) i = 0;
    return &p->sprites[i];
}

// RNG
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
    return (double)(xorshift32() & 0xFFFFFFu) / (double)0x1000000u;
}
static double rand_range(double lo, double hi) {
    return lo + (hi - lo) * rand01();
}

// Math helpers
static double deg2rad(double deg) { return deg * 3.14159265358979323846 / 180.0; }
static double rad2deg(double rad) { return rad * 180.0 / 3.14159265358979323846; }

// Motion helpers
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

    int hit_v = 0, hit_h = 0;
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
    const Sprite* spr = active_sprite_c(p);
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

// Stack helpers
static void stack_reset(Thread* t) { t->sp = 0; }
static int stack_push(Thread* t, Value v) { if (t->sp >= 64) return 0; t->stack[t->sp++] = v; return 1; }
static int stack_pop(Thread* t, Value* out) { if (t->sp <= 0) return 0; if (out) *out = t->stack[--t->sp]; else (void)t->stack[--t->sp]; return 1; }
static Value stack_pop_or_num(Thread* t, double fallback) { Value v; if (!stack_pop(t, &v)) return value_num(fallback); return v; }
static Value stack_pop_or_bool(Thread* t, int fallback) { Value v; if (!stack_pop(t, &v)) return value_bool(fallback); return v; }

// Thread start
static void thread_start(Thread* t, const Instr* code, int len) {
    t->active = 1;
    t->pc = 0;
    t->wake_ms = 0;
    t->code = code;
    t->code_len = len;
    t->rep_top = 0;
    stack_reset(t);
}

// ---------------- DEMOS ----------------

// Green flag scripts (unchanged)
static const Instr SCRIPT_A[] = {
    {101, OP_FOREVER_BEGIN, 0,0, 0,0, COND_TRUE},
    {102, OP_MOVE_STEPS,    8,0, 0,0, COND_TRUE},
    {103, OP_IF_ON_EDGE_BOUNCE, 0,0, 0,0, COND_TRUE},
    {104, OP_WAIT_MS,      20,0, 0,0, COND_TRUE},
    {105, OP_FOREVER_END,   0,0, 0,0, COND_TRUE}, // jump=0 => loop
};

static const Instr SCRIPT_B[] = {
    {201, OP_REPEAT_BEGIN, 0,0, 6, 20, COND_TRUE},
    {202, OP_GOTO_RANDOM,  0,0, 0,  0, COND_TRUE},
    {203, OP_SET_Y,        0,0, 0,  0, COND_TRUE},
    {204, OP_WAIT_MS,    150,0, 0,  0, COND_TRUE},
    {205, OP_REPEAT_END,   0,0, 0,  0, COND_TRUE},
    {206, OP_END,          0,0, 0,  0, COND_TRUE},
};

// Key scripts
static const Instr KEY_SPACE[] = {
    {301, OP_FOREVER_BEGIN, 0,0, 0,0, COND_TRUE},
    {302, OP_MOVE_STEPS,    6,0, 0,0, COND_TRUE},
    {303, OP_IF_ON_EDGE_BOUNCE, 0,0, 0,0, COND_TRUE},
    {304, OP_WAIT_MS,      15,0, 0,0, COND_TRUE},
    {305, OP_FOREVER_END,   0,0, 0,0, COND_TRUE},
};

static const Instr KEY_A[] = {
    {401, OP_REPEAT_BEGIN, 0,0, 6, 60, COND_TRUE},
    {402, OP_TURN_DEG,    10,0, 0,  0, COND_TRUE},
    {403, OP_MOVE_STEPS,   4,0, 0,  0, COND_TRUE},
    {404, OP_WAIT_MS,     20,0, 0,  0, COND_TRUE},
    {405, OP_REPEAT_END,   0,0, 0,  0, COND_TRUE},
    {406, OP_END,          0,0, 0,  0, COND_TRUE},
};

static const Instr KEY_C[] = {
    {601, OP_REPEAT_BEGIN,  0,0, 6, 120, COND_TRUE},
    {602, OP_RANDOM_RANGE, -8,8, 0,   0, COND_TRUE},
    {603, OP_CHANGE_X_POP,  0,0, 0,   0, COND_TRUE},
    {604, OP_IF_ON_EDGE_BOUNCE, 0,0,0,0, COND_TRUE},
    {605, OP_WAIT_MS,      15,0, 0,   0, COND_TRUE},
    {606, OP_REPEAT_END,    0,0, 0,   0, COND_TRUE},
    {607, OP_END,           0,0, 0,   0, COND_TRUE},
};

// NEW: KEY_V (118) variable demo using var0
// var0 = -240
// forever:
//   var0 = var0 + 4
//   set x to var0
//   if (var0 > 240) then var0 = -240
//   wait 16
static const Instr KEY_V[] = {
    {701, OP_PUSH_NUM,     -240,0, 0,0, COND_TRUE},
    {702, OP_SET_VAR_POP,     0,0, 0,0, COND_TRUE}, // count=0 => var0 (set by below initializer? we use count field!)
    {703, OP_FOREVER_BEGIN,   0,0, 0,0, COND_TRUE},

    {704, OP_READ_VAR_PUSH,   0,0, 0,0, COND_TRUE}, // var0
    {705, OP_PUSH_NUM,        4,0, 0,0, COND_TRUE},
    {706, OP_ADD,             0,0, 0,0, COND_TRUE},
    {707, OP_SET_VAR_POP,     0,0, 0,0, COND_TRUE}, // var0

    {708, OP_READ_VAR_PUSH,   0,0, 0,0, COND_TRUE},
    {709, OP_SET_X_POP,       0,0, 0,0, COND_TRUE},

    {710, OP_READ_VAR_PUSH,   0,0, 0,0, COND_TRUE},
    {711, OP_PUSH_NUM,      240,0, 0,0, COND_TRUE},
    {712, OP_GT,              0,0, 0,0, COND_TRUE},
    {713, OP_IF_POP,          0,0, 716,0, COND_TRUE}, // if false jump -> 716
    {714, OP_PUSH_NUM,     -240,0, 0,0, COND_TRUE},
    {715, OP_SET_VAR_POP,     0,0, 0,0, COND_TRUE}, // var0
    {716, OP_WAIT_MS,        16,0, 0,0, COND_TRUE},

    {717, OP_FOREVER_END,     0,0, 703,0, COND_TRUE}, // loop back to FOREVER_BEGIN
};

// IMPORTANT: For var ops, we use Instr.count as var_id.
// So we patch those entries below with count=0 using designated init style is not allowed here,
// so we will rely on the fact count is the 6th field. We'll re-list with correct count values:

static const Instr KEY_V_FIXED[] = {
    {701, OP_PUSH_NUM,     -240,0, 0, 0, COND_TRUE},
    {702, OP_SET_VAR_POP,     0,0, 0, 0, COND_TRUE}, // count patched below in code at start
    {703, OP_FOREVER_BEGIN,   0,0, 0, 0, COND_TRUE},

    {704, OP_READ_VAR_PUSH,   0,0, 0, 0, COND_TRUE},
    {705, OP_PUSH_NUM,        4,0, 0, 0, COND_TRUE},
    {706, OP_ADD,             0,0, 0, 0, COND_TRUE},
    {707, OP_SET_VAR_POP,     0,0, 0, 0, COND_TRUE},

    {708, OP_READ_VAR_PUSH,   0,0, 0, 0, COND_TRUE},
    {709, OP_SET_X_POP,       0,0, 0, 0, COND_TRUE},

    {710, OP_READ_VAR_PUSH,   0,0, 0, 0, COND_TRUE},
    {711, OP_PUSH_NUM,      240,0, 0, 0, COND_TRUE},
    {712, OP_GT,              0,0, 0, 0, COND_TRUE},
    {713, OP_IF_POP,          0,0, 716,0, COND_TRUE},

    {714, OP_PUSH_NUM,     -240,0, 0, 0, COND_TRUE},
    {715, OP_SET_VAR_POP,     0,0, 0, 0, COND_TRUE},

    {716, OP_WAIT_MS,        16,0, 0, 0, COND_TRUE},
    {717, OP_FOREVER_END,     0,0, 703,0, COND_TRUE},
};

// Broadcast receiver (msg=1)
static const Instr RECV_MSG1[] = {
    {501, OP_REPEAT_BEGIN,  0,0, 6, 160, COND_TRUE},
    {502, OP_RANDOM_RANGE, -12,12, 0,   0, COND_TRUE},
    {503, OP_CHANGE_X_POP,  0,0, 0,   0, COND_TRUE},
    {504, OP_IF_ON_EDGE_BOUNCE, 0,0,0,0, COND_TRUE},
    {505, OP_WAIT_MS,      10,0, 0,   0, COND_TRUE},
    {506, OP_REPEAT_END,    0,0, 0,   0, COND_TRUE},
    {507, OP_END,           0,0, 0,   0, COND_TRUE},
};

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

    // IO/state
    s->mouse_x = 0.0;
    s->mouse_y = 0.0;
    s->mouse_down = 0;
    s->timer_start_ms = 0;
    s->answer_value = 0.0;
    s->answer_valid = 0;
    s->ask_pending = 0;
    s->ask_thread_idx = -1;
    s->broadcast_pending = 0;
    s->broadcast_id = 0;
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

    // reset request flags
    s->ask_pending = 0;
    s->ask_thread_idx = -1;
    s->broadcast_pending = 0;
    s->broadcast_id = 0;
}

void scheduler_set_mouse(Scheduler* s, double mx, double my, int mouse_down) {
    if (!s) return;
    s->mouse_x = mx;
    s->mouse_y = my;
    s->mouse_down = mouse_down ? 1 : 0;
}

void scheduler_set_timer_start(Scheduler* s, uint64_t start_ms) {
    if (!s) return;
    s->timer_start_ms = start_ms;
}

void scheduler_submit_answer(Scheduler* s, double answer) {
    if (!s) return;
    s->answer_value = answer;
    s->answer_valid = 1;

    // unblock waiting thread
    if (s->ask_thread_idx >= 0 && s->ask_thread_idx < 16) {
        s->threads[s->ask_thread_idx].wake_ms = 0;
    }
    s->ask_pending = 0;
    s->ask_thread_idx = -1;
}

void scheduler_start_demo(Scheduler* s) {
    scheduler_stop_all(s);
    thread_start(&s->threads[0], SCRIPT_A, (int)(sizeof(SCRIPT_A) / sizeof(SCRIPT_A[0])));
    thread_start(&s->threads[1], SCRIPT_B, (int)(sizeof(SCRIPT_B) / sizeof(SCRIPT_B[0])));
    s->rr_index = 0;
}

void scheduler_start_custom(Scheduler* s, const Instr* code, int len) {
    if (!s || !code || len <= 0) return;

    // find a free thread; if none, overwrite thread 0 (or just return)
    int idx = -1;
    for (int i = 0; i < 16; i++) {
        if (!s->threads[i].active) { idx = i; break; }
    }
    if (idx < 0) idx = 0;

    thread_start(&s->threads[idx], code, len);
}

void scheduler_start_many(Scheduler* s, const ScriptDef* scripts, int count) {
    scheduler_stop_all(s);
    s->rr_index = 0;

    if (!scripts || count <= 0) return;
    if (count > 16) count = 16; // threads[16]

    for (int i = 0; i < count; i++) {
        if (!scripts[i].code || scripts[i].len <= 0) continue;
        thread_start(&s->threads[i], scripts[i].code, scripts[i].len);
    }
}

static int find_free_thread(Scheduler* s) {
    for (int i = 0; i < 16; i++) if (!s->threads[i].active) return i;
    return -1;
}

void scheduler_start_on_key(Scheduler* s, int keycode) {
    int idx = find_free_thread(s);
    if (idx < 0) return;

    if (keycode == 32) { thread_start(&s->threads[idx], KEY_SPACE, (int)(sizeof(KEY_SPACE)/sizeof(KEY_SPACE[0]))); return; }
    if (keycode == 97) { thread_start(&s->threads[idx], KEY_A, (int)(sizeof(KEY_A)/sizeof(KEY_A[0]))); return; }
    if (keycode == 99) { thread_start(&s->threads[idx], KEY_C, (int)(sizeof(KEY_C)/sizeof(KEY_C[0]))); return; }

    if (keycode == 118) {
        // Start KEY_V_FIXED but we need var_id=0 in count for var ops.
        // Easiest: reuse the same array and interpret count=0 (already is 0), so we can start directly.
        thread_start(&s->threads[idx], KEY_V_FIXED, (int)(sizeof(KEY_V_FIXED)/sizeof(KEY_V_FIXED[0])));
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
}

// Interpreter
static int step_thread(Scheduler* s, int thread_idx, Thread* t, Project* p, VarStore* vars, uint64_t now_ms, uint64_t* out_block_id, int* budget) {
    if (!t->active || !t->code || t->code_len <= 0) return 0;
    if (t->wake_ms != 0 && now_ms < t->wake_ms) return 0;
    t->wake_ms = 0;

    if (!watchdog_allow_step(budget)) return 0;

    if (t->pc < 0 || t->pc >= t->code_len) { t->active = 0; return 0; }

    const Instr in = t->code[t->pc++];
    if (out_block_id) *out_block_id = in.id;

    Sprite* spr = active_sprite(p);

    switch (in.op) {
        // Motion
        case OP_MOVE_STEPS: if (spr) sprite_move_steps(spr, in.a); return 1;
        case OP_TURN_DEG:   if (spr) spr->dir = wrap_angle_deg(spr->dir + in.a); return 1;

        case OP_SET_X: if (spr) spr->x = clampd(in.a, -STAGE_HALF_W, STAGE_HALF_W); return 1;
        case OP_SET_Y: if (spr) spr->y = clampd(in.a, -STAGE_HALF_H, STAGE_HALF_H); return 1;

        case OP_CHANGE_X: if (spr) spr->x = clampd(spr->x + in.a, -STAGE_HALF_W, STAGE_HALF_W); return 1;
        case OP_CHANGE_Y: if (spr) spr->y = clampd(spr->y + in.a, -STAGE_HALF_H, STAGE_HALF_H); return 1;

        case OP_GOTO_RANDOM:
            if (spr) { spr->x = rand_range(-STAGE_HALF_W, STAGE_HALF_W); spr->y = rand_range(-STAGE_HALF_H, STAGE_HALF_H); }
            return 1;

        case OP_IF_ON_EDGE_BOUNCE:
            if (spr) sprite_bounce_if_needed(spr);
            return 1;

        // Control
        case OP_WAIT_MS: t->wake_ms = now_ms + (uint64_t)in.a; return 1;

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
            if (t->rep_left[top] > 0) t->pc = t->rep_begin_pc[top];
            else t->rep_top--;
            return 1;
        }

        case OP_FOREVER_BEGIN: return 1;
        case OP_FOREVER_END: {
            int j = in.jump;
            if (j < 0 || j >= t->code_len) j = 0;
            t->pc = j;
            return 1;
        }

        // Non-stack IF (kept)
        case OP_IF_BEGIN: {
            int ok = eval_cond(in.cond, in.a, p);
            if (!ok) t->pc = in.jump;
            return 1;
        }
        case OP_ELSE:  t->pc = in.jump; return 1;
        case OP_ENDIF: return 1;

        // Operators / Stack
        case OP_PUSH_NUM: stack_push(t, value_num(in.a)); return 1;

        case OP_RANDOM_RANGE: {
            double r = rand_range(in.a, in.b);
            stack_push(t, value_num(r));
            return 1;
        }

        case OP_ADD: { double b = value_as_num(stack_pop_or_num(t, 0)); double a = value_as_num(stack_pop_or_num(t, 0)); stack_push(t, value_num(a+b)); return 1; }
        case OP_SUB: { double b = value_as_num(stack_pop_or_num(t, 0)); double a = value_as_num(stack_pop_or_num(t, 0)); stack_push(t, value_num(a-b)); return 1; }
        case OP_MUL: { double b = value_as_num(stack_pop_or_num(t, 0)); double a = value_as_num(stack_pop_or_num(t, 0)); stack_push(t, value_num(a*b)); return 1; }

        case OP_DIV: {
            double b = value_as_num(stack_pop_or_num(t, 1));
            double a = value_as_num(stack_pop_or_num(t, 0));
            stack_push(t, value_num((b==0.0) ? 0.0 : (a/b)));
            return 1;
        }

        case OP_GT: { double b = value_as_num(stack_pop_or_num(t, 0)); double a = value_as_num(stack_pop_or_num(t, 0)); stack_push(t, value_bool(a>b)); return 1; }
        case OP_LT: { double b = value_as_num(stack_pop_or_num(t, 0)); double a = value_as_num(stack_pop_or_num(t, 0)); stack_push(t, value_bool(a<b)); return 1; }
        case OP_EQ: { double b = value_as_num(stack_pop_or_num(t, 0)); double a = value_as_num(stack_pop_or_num(t, 0)); stack_push(t, value_bool(a==b)); return 1; }

        case OP_AND: { int b = value_as_bool(stack_pop_or_bool(t,0)); int a = value_as_bool(stack_pop_or_bool(t,0)); stack_push(t, value_bool(a&&b)); return 1; }
        case OP_OR:  { int b = value_as_bool(stack_pop_or_bool(t,0)); int a = value_as_bool(stack_pop_or_bool(t,0)); stack_push(t, value_bool(a||b)); return 1; }
        case OP_NOT: { int a = value_as_bool(stack_pop_or_bool(t,0)); stack_push(t, value_bool(!a)); return 1; }

        case OP_IF_POP: {
            int cond = value_as_bool(stack_pop_or_bool(t, 0));
            if (!cond) t->pc = in.jump;
            return 1;
        }

        case OP_CHANGE_X_POP: {
            double dx = value_as_num(stack_pop_or_num(t, 0));
            if (spr) spr->x = clampd(spr->x + dx, -STAGE_HALF_W, STAGE_HALF_W);
            return 1;
        }

        case OP_CHANGE_Y_POP: {
            double dy = value_as_num(stack_pop_or_num(t, 0));
            if (spr) spr->y = clampd(spr->y + dy, -STAGE_HALF_H, STAGE_HALF_H);
            return 1;
        }

        case OP_SET_X_POP: {
            double x = value_as_num(stack_pop_or_num(t, 0));
            if (spr) spr->x = clampd(x, -STAGE_HALF_W, STAGE_HALF_W);
            return 1;
        }

        case OP_SET_Y_POP: {
            double y = value_as_num(stack_pop_or_num(t, 0));
            if (spr) spr->y = clampd(y, -STAGE_HALF_H, STAGE_HALF_H);
            return 1;
        }

        // NEW: Variables
        case OP_READ_VAR_PUSH: {
            Value v = varstore_get(vars, in.count);
            stack_push(t, v);
            return 1;
        }

        case OP_SET_VAR_POP: {
            Value v = stack_pop_or_num(t, 0.0);
            varstore_set(vars, in.count, v);
            return 1;
        }

        case OP_CHANGE_VAR_POP: {
            double d = value_as_num(stack_pop_or_num(t, 0.0));
            varstore_change_num(vars, in.count, d);
            return 1;
        }

        // Events / Messaging
        case OP_BROADCAST:
            if (s) {
                s->broadcast_pending = 1;
                s->broadcast_id = in.count;
            }
            return 1;

        // Sensing / Ask
        case OP_ASK_WAIT:
            if (s) {
                // Only one ask at a time.
                if (s->ask_thread_idx == -1) {
                    s->ask_pending = 1;
                    s->ask_thread_idx = thread_idx;
                    s->answer_valid = 0;
                    s->answer_value = 0.0;
                }
            }
            // Block this thread until answer arrives.
            t->wake_ms = UINT64_MAX;
            return 1;

        case OP_PUSH_ANSWER:
            stack_push(t, value_num(s ? s->answer_value : 0.0));
            return 1;

        case OP_PUSH_MOUSE_X:
            stack_push(t, value_num(s ? s->mouse_x : 0.0));
            return 1;

        case OP_PUSH_MOUSE_Y:
            stack_push(t, value_num(s ? s->mouse_y : 0.0));
            return 1;

        case OP_PUSH_MOUSE_DOWN:
            stack_push(t, value_bool(s ? s->mouse_down : 0));
            return 1;

        case OP_PUSH_TIMER: {
            double sec = 0.0;
            if (s && s->timer_start_ms != 0 && now_ms >= s->timer_start_ms) {
                sec = (double)(now_ms - s->timer_start_ms) / 1000.0;
            }
            stack_push(t, value_num(sec));
            return 1;
        }

        case OP_PUSH_DIST_MOUSE: {
            double dist = 0.0;
            if (spr && s) {
                double dx = spr->x - s->mouse_x;
                double dy = spr->y - s->mouse_y;
                dist = std::sqrt(dx*dx + dy*dy);
            }
            stack_push(t, value_num(dist));
            return 1;
        }

        case OP_END:
            t->active = 0;
            return 1;

        default:
            return 1;
    }
}

int scheduler_step_one(Scheduler* s, Project* p, VarStore* vars,
                       uint64_t now_ms, uint64_t* out_block_id, int* budget) {
    for (int attempts = 0; attempts < 16; attempts++) {
        int idx = (s->rr_index + attempts) % 16;
        Thread* t = &s->threads[idx];

        uint64_t bid = 0;
        int did = step_thread(s, idx, t, p, vars, now_ms, &bid, budget);
        if (did) {
            if (out_block_id) *out_block_id = bid;
            s->rr_index = (idx + 1) % 16;
            return 1;
        }
    }

    return 0;
}