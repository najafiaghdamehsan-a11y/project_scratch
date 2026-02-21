#include "engine/scheduler.h"
#include "engine/safety.h"
#include <cmath>

// ---- helpers ----
static double deg2rad(double deg) {
    return deg * 3.14159265358979323846 / 180.0;
}

// Scratch-like convention:
// 0 deg = up, 90 = right, 180 = down, 270 = left
static void sprite_move_steps(Sprite* s, double steps) {
    double d = wrap_angle_deg(s->dir);
    double r = deg2rad(d);

    // if 0 is up:
    s->x += steps * std::sin(r);
    s->y += steps * std::cos(r);
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
// Script A: repeat 24 { move 8; wait 60ms; turn 15; wait 60ms } end
static const Instr SCRIPT_A[] = {
    {101, OP_REPEAT_BEGIN, 0.0, 7, 24},
    {102, OP_MOVE_STEPS,   8.0, 0, 0},
    {103, OP_WAIT_MS,     60.0, 0, 0},
    {104, OP_TURN_DEG,    15.0, 0, 0},
    {105, OP_WAIT_MS,     60.0, 0, 0},
    {106, OP_REPEAT_END,   0.0, 0, 0},
    {107, OP_END,          0.0, 0, 0},
};

// Script B: repeat 30 { turn -10; move 4; wait 40ms } end
static const Instr SCRIPT_B[] = {
    {201, OP_REPEAT_BEGIN, 0.0, 6, 30},
    {202, OP_TURN_DEG,   -10.0, 0, 0},
    {203, OP_MOVE_STEPS,   4.0, 0, 0},
    {204, OP_WAIT_MS,     40.0, 0, 0},
    {205, OP_REPEAT_END,   0.0, 0, 0},
    {206, OP_END,          0.0, 0, 0},
};

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

    Sprite* spr = (p->sprite_count > 0) ? &p->sprites[0] : nullptr;

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

            // If repeat count <= 0: skip loop body
            if (n <= 0) {
                t->pc = in.jump; // jump to after loop end
                return 1;
            }

            // Push repeat state
            if (t->rep_top >= 32) {
                // Too deep nesting: stop thread safely
                t->active = 0;
                return 1;
            }

            t->rep_left[t->rep_top] = n;
            t->rep_begin_pc[t->rep_top] = t->pc; // first instruction after BEGIN
            t->rep_top++;
            return 1;
        }

        case OP_REPEAT_END: {
            if (t->rep_top <= 0) return 1; // malformed script; ignore

            int top = t->rep_top - 1;
            t->rep_left[top]--;

            if (t->rep_left[top] > 0) {
                // loop back to begin body
                t->pc = t->rep_begin_pc[top];
            } else {
                // pop and continue
                t->rep_top--;
            }
            return 1;
        }

        case OP_END:
            t->active = 0;
            return 1;

        default:
            return 1;
    }
}

int scheduler_step_one(Scheduler* s, Project* p, uint64_t now_ms, uint64_t* out_block_id, int* budget) {
    // Try up to 16 threads to find one runnable (round-robin)
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
    return 0; // none runnable (all waiting/inactive)
}