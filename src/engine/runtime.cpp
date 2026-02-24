#include "engine/runtime.h"
#include "engine/safety.h"
#include "engine/scheduler.h"
#include "core/log.h"
#include <cstdio>
#include <cstring> // memcpy

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

    // key event plumbing
    r->key_pending = 0;
    r->last_key = 0;

    // broadcast plumbing
    r->msg_pending = 0;
    r->msg_id = 0;

    // variables
    varstore_init(&r->vars);

    r->key_script_count = 0;
    r->recv_script_count = 0;

    // main script buffer
    r->main_len = 0;
}

void runtime_set_paused(Runtime* r, int paused) { r->paused = paused; }
void runtime_set_step_mode(Runtime* r, int step_mode) { r->step_mode = step_mode; }
void runtime_request_step(Runtime* r) { r->do_step = 1; }

void runtime_set_main_script(Runtime* r, const Instr* code, int len) {
    if (!r) return;

    if (!code || len <= 0) {
        r->main_len = 0;
        return;
    }

    if (len > RUNTIME_MAX_MAIN_CODE) len = RUNTIME_MAX_MAIN_CODE;
    std::memcpy(r->main_code, code, sizeof(Instr) * (size_t)len);
    r->main_len = len;
}

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

    // Scratch-like: reset variables each run
    varstore_clear(&r->vars);

    // IMPORTANT: run what UI compiled. If none, run nothing.
    scheduler_stop_all(&r->sched);

    if (r->main_len > 0) {
        ScriptDef one;
        one.code = r->main_code;
        one.len  = r->main_len;

        scheduler_start_many(&r->sched, &one, 1);

        log_write(LogRecord{0,0,"EVENT","GreenFlag","start main script", LOG_INFO});
    } else {
        r->running = 0;
        log_write(LogRecord{0,0,"EVENT","GreenFlag","no scripts", LOG_INFO});
    }
}

void runtime_clear_event_scripts(Runtime* r) {
    if (!r) return;
    r->key_script_count = 0;
    r->recv_script_count = 0;
}

static void copy_instr(Instr* dst, const Instr* src, int len) {
    if (!dst || !src || len <= 0) return;
    std::memcpy(dst, src, sizeof(Instr) * (size_t)len);
}

void runtime_set_key_script(Runtime* r, int keycode, const Instr* code, int len) {
    if (!r) return;

    if (!code || len <= 0) {
        // remove if exists
        for (int i = 0; i < r->key_script_count; i++) {
            if (r->key_scripts[i].keycode == keycode) {
                for (int j = i; j < r->key_script_count - 1; j++) r->key_scripts[j] = r->key_scripts[j+1];
                r->key_script_count--;
                return;
            }
        }
        return;
    }

    if (len > RUNTIME_MAX_EVENT_CODE) len = RUNTIME_MAX_EVENT_CODE;

    // replace if exists
    for (int i = 0; i < r->key_script_count; i++) {
        if (r->key_scripts[i].keycode == keycode) {
            copy_instr(r->key_scripts[i].code, code, len);
            r->key_scripts[i].len = len;
            return;
        }
    }

    // add new
    if (r->key_script_count >= RUNTIME_MAX_KEY_SCRIPTS) return;
    Runtime::KeyScriptEntry* e = &r->key_scripts[r->key_script_count++];
    e->keycode = keycode;
    copy_instr(e->code, code, len);
    e->len = len;
}

void runtime_set_recv_script(Runtime* r, int msg_id, const Instr* code, int len) {
    if (!r) return;

    if (!code || len <= 0) {
        for (int i = 0; i < r->recv_script_count; i++) {
            if (r->recv_scripts[i].msg_id == msg_id) {
                for (int j = i; j < r->recv_script_count - 1; j++) r->recv_scripts[j] = r->recv_scripts[j+1];
                r->recv_script_count--;
                return;
            }
        }
        return;
    }

    if (len > RUNTIME_MAX_EVENT_CODE) len = RUNTIME_MAX_EVENT_CODE;

    for (int i = 0; i < r->recv_script_count; i++) {
        if (r->recv_scripts[i].msg_id == msg_id) {
            copy_instr(r->recv_scripts[i].code, code, len);
            r->recv_scripts[i].len = len;
            return;
        }
    }

    if (r->recv_script_count >= RUNTIME_MAX_RECV_SCRIPTS) return;
    Runtime::RecvScriptEntry* e = &r->recv_scripts[r->recv_script_count++];
    e->msg_id = msg_id;
    copy_instr(e->code, code, len);
    e->len = len;
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

void runtime_post_broadcast(Runtime* r, int msg_id) {
    r->msg_id = msg_id;
    r->msg_pending = 1;
}

void runtime_tick(Runtime* r, Project* p) {
    // 0) key event handling
    if (r->key_pending) {
        int k = r->last_key;

        // log keycode
        char keybuf[32];
        std::snprintf(keybuf, sizeof(keybuf), "%d", k);
        log_write(LogRecord{r->cycle, 0, "EVENT", "KeyDown", keybuf, LOG_INFO});

        // Demo trigger: press 'b' (98) -> broadcast msg 1
        if (k == 98) {
            runtime_post_broadcast(r, 1);
            log_write(LogRecord{r->cycle, 0, "EVENT", "BroadcastRequest", "msg=1", LOG_INFO});
        }

        // Start key scripts only while running (Scratch-like)
        int started = 0;
        for (int i = 0; i < r->key_script_count; i++) {
            if (r->key_scripts[i].keycode == k && r->key_scripts[i].len > 0) {
                scheduler_start_custom(&r->sched, r->key_scripts[i].code, r->key_scripts[i].len);
                started = 1;
            }
        }
        if (started) r->running = 1;

        r->key_pending = 0;
    }

    // 0.5) broadcast processing
    if (r->msg_pending) {
        char msgbuf[32];
        std::snprintf(msgbuf, sizeof(msgbuf), "msg=%d", r->msg_id);
        log_write(LogRecord{r->cycle, 0, "EVENT", "Broadcast", msgbuf, LOG_INFO});

        int started = 0;
        for (int i = 0; i < r->recv_script_count; i++) {
            if (r->recv_scripts[i].msg_id == r->msg_id && r->recv_scripts[i].len > 0) {
                scheduler_start_custom(&r->sched, r->recv_scripts[i].code, r->recv_scripts[i].len);
                started = 1;
            }
        }
        if (started) r->running = 1;

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

    // 3) execute up to max_steps with watchdog budget
    const uint64_t now = time_now_ms();
    int budget = 2000;
    uint64_t bid = 0;

    int executed_any = 0;

    for (int i = 0; i < max_steps; i++) {
        bid = 0;

        // pass &r->vars
        int did = scheduler_step_one(&r->sched, p, &r->vars, now, &bid, &budget);
        if (!did) break;

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