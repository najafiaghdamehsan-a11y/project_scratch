#include "engine/scheduler.h"
#include "engine/safety.h"
#include <cmath>

// ---- helpers (no classes) ----
static double deg2rad(double deg) {
    return deg * 3.14159265358979323846 / 180.0;
}

// Scratch-like: direction 90 = right, 0 = up.
// We'll interpret:
// 0 deg = up, 90 deg = right, 180 = down, 270 = left
static void move_steps(Sprite* s, double steps) {
    double d = wrap_angle_deg(s->dir);
    double r = deg2rad(d);

    // if 0 is up, then x uses sin, y uses cos
    s->x += steps * std::sin(r);
    s->y += steps * std::cos(r);
}

// ---- demo script ----
// repeat 20: move 10, wait 100ms, turn 15
static const Instr DEMO[] = {
    { 1, OP_REPEAT_BEGIN, 0.0, 7, 20 }, // jump to index 7 when done
    { 2, OP_MOVE_STEPS,  10.0, 0, 0 },
    { 3, OP_WAIT_MS,    100.0, 0, 0 },
    { 4, OP_TURN_DEG,    15.0, 0, 0 },
    { 5, OP_WAIT_MS,    100.0, 0, 0 },
    { 6, OP_REPEAT_END,   0.0, 0, 0 },  // jump back to begin (we’ll use rep stack)
    { 7, OP_END,          0.0, 0, 0 },
};

void scheduler_init(Scheduler* s) {
    s->t.active = 0;
    s->t.pc = 0;
    s->t.wake_ms = 0;
    s->t.rep_top = 0;
    s->code = nullptr;
    s->code_len = 0;
}

void scheduler_start_demo(Scheduler* s) {
    s->code = DEMO;
    s->code_len = (int)(sizeof(DEMO) / sizeof(DEMO[0]));
    s->t.active = 1;
    s->t.pc = 0;
    s->t.wake_ms = 0;
    s->t.rep_top = 0;
}

void scheduler_stop_all(Scheduler* s) {
    s->t.active = 0;
    s->t.pc = 0;
    s->t.wake_ms = 0;
    s->t.rep_top = 0;
}

int scheduler_step_one(Scheduler* s, Project* p, uint64_t now_ms, uint64_t* out_block_id, int* budget) {
    if (!s->t.active || !s->code || s->code_len <= 0) return 0;

    // waiting?
    if (s->t.wake_ms != 0 && now_ms < s->t.wake_ms) return 0;
    s->t.wake_ms = 0;

    // watchdog budget
    if (!watchdog_allow_step(budget)) return 0;

    if (s->t.pc < 0 || s->t.pc >= s->code_len) {
        s->t.active = 0;
        return 0;
    }

    const Instr in = s->code[s->t.pc++];
    if (out_block_id) *out_block_id = in.id;

    Sprite* spr = (p->sprite_count > 0) ? &p->sprites[0] : nullptr;

    switch (in.op) {
        case OP_MOVE_STEPS:
            if (spr) move_steps(spr, in.a);
            return 1;

        case OP_TURN_DEG:
            if (spr) spr->dir = wrap_angle_deg(spr->dir + in.a);
            return 1;

        case OP_WAIT_MS:
            s->t.wake_ms = now_ms + (uint64_t)in.a;
            return 1;

        case OP_REPEAT_BEGIN: {
            // push repeat state
            if (s->t.rep_top >= 32) { s->t.active = 0; return 1; } // simple safety
            int n = in.count;
            if (n <= 0) {
                // skip loop entirely: jump to after loop end
                s->t.pc = in.jump;
                return 1;
            }
            s->t.rep_left[s->t.rep_top] = n;
            s->t.rep_begin_pc[s->t.rep_top] = s->t.pc; // next instruction after BEGIN
            s->t.rep_top++;
            return 1;
        }

        case OP_REPEAT_END: {
            if (s->t.rep_top <= 0) return 1; // malformed script, ignore
            int top = s->t.rep_top - 1;
            s->t.rep_left[top]--;
            if (s->t.rep_left[top] > 0) {
                // loop back to begin body
                s->t.pc = s->t.rep_begin_pc[top];
            } else {
                // pop and continue forward
                s->t.rep_top--;
            }
            return 1;
        }

        case OP_FOREVER_BEGIN:
            // nothing special; body runs until FOREVER_END jumps back
            return 1;

        case OP_FOREVER_END:
            // jump back to matching begin (stored in jump)
            s->t.pc = in.jump;
            return 1;

        case OP_END:
            s->t.active = 0;
            return 1;

        default:
            return 1;
    }
}