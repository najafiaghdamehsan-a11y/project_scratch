#include "engine/runtime.h"
#include "engine/safety.h"
#include "core/log.h"
#include <cstdio>

// For multi-thread scheduler (threads[16])
static int sched_any_active(const Scheduler* s) {
    for (int i = 0; i < 16; i++) {
        if (s->threads[i].active) return 1;
    }
    return 0;
}

void runtime_init(Runtime* r) {
    r->cycle = 0;
    r->paused = 0;
    r->step_mode = 0;
    r->do_step = 0;

    r->running = 0;
    r->stop_all = 0;
    r->current_block_id = 0;

    scheduler_init(&r->sched);

    r->key_pending = 0;
    r->last_key = 0;

    // NEW
    r->msg_pending = 0;
    r->msg_id = 0;
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

    r->key_pending = 0;
    r->last_key = 0;

    r->msg_pending = 0;
    r->msg_id = 0;

    scheduler_start_demo(&r->sched);

    log_write(LogRecord{0, 0, "EVENT", "GreenFlag", "start", LOG_INFO});
}

void runtime_stop_all(Runtime* r) {
    r->stop_all = 1; // processed in tick (one-shot)
}

int runtime_is_running(const Runtime* r) { return r->running; }
uint64_t runtime_current_block(const Runtime* r) { return r->current_block_id; }

void runtime_post_key(Runtime* r, int keycode) {
    r->last_key = keycode;
    r->key_pending = 1;
}

// NEW
void runtime_post_broadcast(Runtime* r, int msg_id) {
    r->msg_id = msg_id;
    r->msg_pending = 1;
}

void runtime_tick(Runtime* r, Project* p) {
    // 0) key event handling (log + optional triggers)
    if (r->key_pending) {
        int k = r->last_key;

        // log keycode
        char keybuf[32];
        std::snprintf(keybuf, sizeof(keybuf), "%d", k);
        log_write(LogRecord{r->cycle, 0, "EVENT", "KeyDown", keybuf, LOG_INFO});

        // Demo trigger: press 'b' to broadcast message 1
        // SDL_Keycode for 'b' is 98 (same as ASCII)
        if (k == 98) {
            runtime_post_broadcast(r, 1);
            log_write(LogRecord{r->cycle, 0, "EVENT", "BroadcastRequest", "msg=1", LOG_INFO});
        }

        // Start key scripts only while running (Scratch-like)
        if (r->running) {
            scheduler_start_on_key(&r->sched, k);
        }

        r->key_pending = 0;
    }

    // 0.5) broadcast processing (log + trigger receivers)
    if (r->msg_pending) {
        char msgbuf[32];
        std::snprintf(msgbuf, sizeof(msgbuf), "msg=%d", r->msg_id);
        log_write(LogRecord{r->cycle, 0, "EVENT", "Broadcast", msgbuf, LOG_INFO});

        if (r->running) {
            scheduler_broadcast(&r->sched, r->msg_id);
        }

        r->msg_pending = 0;
    }

    // 1) stop all first
    if (r->stop_all) {
        r->running = 0;
        r->stop_all = 0;
        r->current_block_id = 0;

        r->msg_pending = 0;
        r->key_pending = 0;

        scheduler_stop_all(&r->sched);
        log_write(LogRecord{r->cycle, 0, "CTRL", "StopAll", "stop", LOG_INFO});
        return;
    }

    // 2) gates
    if (r->paused) return;
    if (!r->running) return;

    // step-by-step gate
    if (r->step_mode && !r->do_step) return;
    const int max_steps = (r->step_mode ? 1 : 64);
    r->do_step = 0;

    // 3) execute up to max_steps, with watchdog budget
    const uint64_t now = time_now_ms();
    int budget = 2000;
    uint64_t bid = 0;

    int executed_any = 0;

    for (int i = 0; i < max_steps; i++) {
        bid = 0;
        int did = scheduler_step_one(&r->sched, p, now, &bid, &budget);
        if (!did) break; // all waiting/inactive

        executed_any = 1;
        r->current_block_id = bid;
        r->cycle++;
    }

    // 4) watchdog exhausted
    if (budget <= 0) {
        r->running = 0;
        r->current_block_id = 0;
        scheduler_stop_all(&r->sched);
        log_write(LogRecord{r->cycle, 0, "WATCHDOG", "Stop", "too many steps", LOG_ERROR});
        return;
    }

    // 5) if all threads finished, stop running
    if (!sched_any_active(&r->sched)) {
        r->running = 0;
        r->current_block_id = 0;
        log_write(LogRecord{r->cycle, 0, "CTRL", "ScriptEnd", "stop", LOG_INFO});
        return;
    }

    if (executed_any) {
        log_write(LogRecord{r->cycle, 0, "EXEC", "Block", "executed", LOG_INFO});
    }
}