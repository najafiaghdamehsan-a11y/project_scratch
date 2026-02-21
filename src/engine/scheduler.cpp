#include "engine/scheduler.h"
#include "engine/safety.h"

// Demo script: move + wait + move + wait + end
static const Instr DEMO[] = {
    {1, OP_MOVE_X,  5.0},
    {2, OP_WAIT_MS, 300.0},
    {3, OP_MOVE_X,  5.0},
    {4, OP_WAIT_MS, 300.0},
    {5, OP_END,     0.0},
};

void scheduler_init(Scheduler* s) {
    s->t.active = 0;
    s->t.pc = 0;
    s->t.wake_ms = 0;
    s->code = nullptr;
    s->code_len = 0;
}

void scheduler_start_demo(Scheduler* s) {
    s->code = DEMO;
    s->code_len = (int)(sizeof(DEMO) / sizeof(DEMO[0]));
    s->t.active = 1;
    s->t.pc = 0;
    s->t.wake_ms = 0;
}

void scheduler_stop_all(Scheduler* s) {
    s->t.active = 0;
    s->t.pc = 0;
    s->t.wake_ms = 0;
}

int scheduler_step_one(Scheduler* s, Project* p, uint64_t now_ms, uint64_t* out_block_id, int* budget) {
    if (!s->t.active || !s->code || s->code_len <= 0) return 0;

    // waiting?
    if (s->t.wake_ms != 0 && now_ms < s->t.wake_ms) {
        return 0;
    }
    s->t.wake_ms = 0;

    // watchdog budget
    if (!watchdog_allow_step(budget)) return 0;

    if (s->t.pc < 0 || s->t.pc >= s->code_len) {
        s->t.active = 0;
        return 0;
    }

    const Instr in = s->code[s->t.pc++];
    if (out_block_id) *out_block_id = in.id;

    switch (in.op) {
        case OP_MOVE_X:
            if (p->sprite_count > 0) p->sprites[0].x += in.a;
            return 1;

        case OP_WAIT_MS:
            s->t.wake_ms = now_ms + (uint64_t)in.a;
            return 1;

        case OP_END:
            s->t.active = 0;
            return 1;

        default:
            return 1;
    }
}