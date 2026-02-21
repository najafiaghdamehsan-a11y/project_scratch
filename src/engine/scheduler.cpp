#include "engine/scheduler.h"
#include "engine/safety.h"

// Script A: move + wait + move + wait + end
static const Instr SCRIPT_A[] = {
    {101, OP_MOVE_X,  5.0},
    {102, OP_WAIT_MS, 300.0},
    {103, OP_MOVE_X,  5.0},
    {104, OP_WAIT_MS, 300.0},
    {105, OP_END,     0.0},
};

// Script B: wait + move + wait + move + end (different timing)
static const Instr SCRIPT_B[] = {
    {201, OP_WAIT_MS, 150.0},
    {202, OP_MOVE_X,  -3.0},
    {203, OP_WAIT_MS, 150.0},
    {204, OP_MOVE_X,  -3.0},
    {205, OP_END,     0.0},
};

static void thread_start(Thread* t, const Instr* code, int len) {
    t->active = 1;
    t->pc = 0;
    t->wake_ms = 0;
    t->code = code;
    t->code_len = len;
}

void scheduler_init(Scheduler* s) {
    for (int i = 0; i < 16; i++) {
        s->threads[i].active = 0;
        s->threads[i].pc = 0;
        s->threads[i].wake_ms = 0;
        s->threads[i].code = nullptr;
        s->threads[i].code_len = 0;
    }
    s->rr_index = 0;
}

void scheduler_start_demo(Scheduler* s) {
    scheduler_stop_all(s);

    thread_start(&s->threads[0], SCRIPT_A, (int)(sizeof(SCRIPT_A) / sizeof(SCRIPT_A[0])));
    thread_start(&s->threads[1], SCRIPT_B, (int)(sizeof(SCRIPT_B) / sizeof(SCRIPT_B[0])));

    s->rr_index = 0;
}

void scheduler_stop_all(Scheduler* s) {
    for (int i = 0; i < 16; i++) {
        s->threads[i].active = 0;
        s->threads[i].pc = 0;
        s->threads[i].wake_ms = 0;
        s->threads[i].code = nullptr;
        s->threads[i].code_len = 0;
    }
}

static int step_thread(Thread* t, Project* p, uint64_t now_ms, uint64_t* out_block_id, int* budget) {
    if (!t->active || !t->code || t->code_len <= 0) return 0;

    // waiting?
    if (t->wake_ms != 0 && now_ms < t->wake_ms) return 0;
    t->wake_ms = 0;

    if (!watchdog_allow_step(budget)) return 0;

    if (t->pc < 0 || t->pc >= t->code_len) {
        t->active = 0;
        return 0;
    }

    const Instr in = t->code[t->pc++];
    if (out_block_id) *out_block_id = in.id;

    switch (in.op) {
        case OP_MOVE_X:
            if (p->sprite_count > 0) p->sprites[0].x += in.a;
            return 1;

        case OP_WAIT_MS:
            t->wake_ms = now_ms + (uint64_t)in.a;
            return 1;

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