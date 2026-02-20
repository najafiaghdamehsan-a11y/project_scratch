#include "engine/runtime.h"
#include "core/log.h"

void runtime_init(Runtime* r) {
    r->cycle = 0;
    r->paused = 0;
    r->step_mode = 0;
    r->do_step = 0;
}

void runtime_set_paused(Runtime* r, int paused) { r->paused = paused; }
void runtime_set_step_mode(Runtime* r, int step_mode) { r->step_mode = step_mode; }
void runtime_request_step(Runtime* r) { r->do_step = 1; }

void runtime_tick(Runtime* r, Project* p) {
    // Pause logic + step-by-step hook (big points)
    if (r->paused) return;
    if (r->step_mode && !r->do_step) return;
    r->do_step = 0;

    r->cycle++;

    // Placeholder: demonstrate “engine changes model”
    // (later, interpreter executes blocks here)
    if (p->sprite_count > 0) {
        p->sprites[0].x += 1.0;
    }

    log_write(LogRecord{
        r->cycle,
        0,
        "TICK",
        "Sprite1.x",
        "x += 1",
        LOG_INFO
    });
}