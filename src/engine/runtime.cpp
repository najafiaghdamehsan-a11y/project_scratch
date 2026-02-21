#include "engine/runtime.h"
#include "core/log.h"
#include "engine/safety.h"

void runtime_init(Runtime* r) {
    r->cycle = 0;
    r->paused = 0;
    r->step_mode = 0;
    r->do_step = 0;

    r->running = 0;            // start NOT running (Scratch-like)
    r->stop_all = 0;
    r->current_block_id = 0;
}

void runtime_set_paused(Runtime* r, int paused) { r->paused = paused; }
void runtime_set_step_mode(Runtime* r, int step_mode) { r->step_mode = step_mode; }
void runtime_request_step(Runtime* r) { r->do_step = 1; }

void runtime_green_flag(Runtime* r) {
    r->cycle = 0;
    r->paused = 0;
    r->running = 1;
    r->stop_all = 0;
    r->do_step = 0;
    r->current_block_id = 0;

    log_write(LogRecord{0, 0, "EVENT", "GreenFlag", "start", LOG_INFO});
}

void runtime_stop_all(Runtime* r) {
    r->stop_all = 1; // processed in tick (one-shot)
}

int runtime_is_running(const Runtime* r) { return r->running; }
uint64_t runtime_current_block(const Runtime* r) { return r->current_block_id; }

void runtime_tick(Runtime* r, Project* p) {
    // 1) handle stop request first (no watchdog here)
    if (r->stop_all) {
        r->running = 0;
        r->stop_all = 0;
        r->current_block_id = 0;
        log_write(LogRecord{r->cycle, 0, "CTRL", "StopAll", "stop", LOG_INFO});
        return;
    }

    // 2) normal gates
    if (r->paused) return;
    if (!r->running) return;
    if (r->step_mode && !r->do_step) return;
    r->do_step = 0;

    // 3) watchdog budget for this tick (later: used inside loops)
    int budget = 2000;
    if (!watchdog_allow_step(&budget)) {
        r->running = 0;
        r->current_block_id = 0;
        log_write(LogRecord{r->cycle, 0, "WATCHDOG", "Stop", "too many steps", LOG_ERROR});
        return;
    }

    // 4) one engine step
    r->cycle++;

    if (p->sprite_count > 0) {
        p->sprites[0].x += 1.0;
    }

    log_write(LogRecord{r->cycle, 0, "TICK", "Sprite1.x", "x += 1", LOG_INFO});
}