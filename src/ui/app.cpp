#include "ui/app.h"
#include "ui/block_editor.h"
#include "core/log.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>

// -------------------- small drawing helpers --------------------
static int pt_in_rect(int x, int y, const SDL_Rect* r) {
    return x >= r->x && y >= r->y && x < (r->x + r->w) && y < (r->y + r->h);
}

static void draw_filled_rect(SDL_Renderer* ren, SDL_Rect r, uint8_t rr, uint8_t gg, uint8_t bb, uint8_t aa) {
    SDL_SetRenderDrawColor(ren, rr, gg, bb, aa);
    SDL_RenderFillRect(ren, &r);
}

static void draw_rect(SDL_Renderer* ren, SDL_Rect r, uint8_t rr, uint8_t gg, uint8_t bb, uint8_t aa) {
    SDL_SetRenderDrawColor(ren, rr, gg, bb, aa);
    SDL_RenderDrawRect(ren, &r);
}

static void draw_line(SDL_Renderer* ren, int x1, int y1, int x2, int y2, uint8_t rr, uint8_t gg, uint8_t bb, uint8_t aa) {
    SDL_SetRenderDrawColor(ren, rr, gg, bb, aa);
    SDL_RenderDrawLine(ren, x1, y1, x2, y2);
}

static void mouse_to_stage(const SDL_Rect* rect_stage, int mx, int my, double* out_x, double* out_y) {
    if (!rect_stage || !out_x || !out_y) return;
    double sx0 = (double)rect_stage->x + (double)rect_stage->w / 2.0;
    double sy0 = (double)rect_stage->y + (double)rect_stage->h / 2.0;
    *out_x = (double)mx - sx0;
    *out_y = sy0 - (double)my;
}

static void draw_text(SDL_Renderer* ren, TTF_Font* font, int x, int y, const char* text, SDL_Color c) {
    if (!font || !text) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text, c);
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
    if (!t) { SDL_FreeSurface(s); return; }
    SDL_Rect dst{ x, y, s->w, s->h };
    SDL_FreeSurface(s);
    SDL_RenderCopy(ren, t, NULL, &dst);
    SDL_DestroyTexture(t);
}

// -------------------- stage title helpers --------------------
static void update_window_title(SDL_Window* win, const Project* p) {
    const char* name = "None";
    if (p && p->sprite_count > 0) {
        int a = p->active_sprite_index;
        if (a < 0) a = 0;
        if (a >= p->sprite_count) a = 0;
        name = p->sprites[a].name;
    }
    char title[256];
    snprintf(title, sizeof(title), "project_scratch | Active: %s", name);
    SDL_SetWindowTitle(win, title);
}

// -------------------- sprite panel actions --------------------
static void project_add_sprite(Project* p) {
    if (!p) return;
    if (p->sprite_count >= MAX_SPRITES) return;

    int idx = p->sprite_count++;
    Sprite* s = &p->sprites[idx];

    static uint64_t next_id = 1;
    s->id = next_id++;

    snprintf(s->name, MAX_NAME, "Sprite%d", idx + 1);
    s->x = 0;
    s->y = 0;
    s->dir = 90;
    s->size = 100;
    s->visible = 1;

    p->active_sprite_index = idx;
}

static void project_delete_active(Project* p) {
    if (!p) return;
    if (p->sprite_count <= 0) return;

    int a = p->active_sprite_index;
    if (a < 0) a = 0;
    if (a >= p->sprite_count) a = p->sprite_count - 1;

    for (int i = a; i < p->sprite_count - 1; i++) {
        p->sprites[i] = p->sprites[i + 1];
    }
    p->sprite_count--;

    if (p->sprite_count <= 0) {
        p->active_sprite_index = 0;
        return;
    }
    if (a >= p->sprite_count) a = p->sprite_count - 1;
    p->active_sprite_index = a;
}

static void project_toggle_visible(Project* p) {
    if (!p) return;
    if (p->sprite_count <= 0) return;
    int a = p->active_sprite_index;
    if (a < 0) a = 0;
    if (a >= p->sprite_count) a = 0;
    p->sprites[a].visible = !p->sprites[a].visible;
}

static void draw_circle_button(SDL_Renderer* ren, int cx, int cy, int r, SDL_Color fill, SDL_Color border) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r) {
                SDL_SetRenderDrawColor(ren, fill.r, fill.g, fill.b, fill.a);
                SDL_RenderDrawPoint(ren, cx + x, cy + y);
            }
        }
    }
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            int d = x*x + y*y;
            if (d <= r*r && d >= (r-1)*(r-1)) {
                SDL_SetRenderDrawColor(ren, border.r, border.g, border.b, border.a);
                SDL_RenderDrawPoint(ren, cx + x, cy + y);
            }
        }
    }
}

// ============================================================
//  NEW: Compile workspace stack -> Instr[] with control flow
// ============================================================
static int iabs_int(int v) { return v < 0 ? -v : v; }

static int be_find_below(const BlockEditor* be, int idx) {
    const int GAP = 8;
    const int TOL_X = 6;
    const int TOL_Y = 12;

    SDL_Rect r = be->blocks[idx].r;
    int target_x = r.x;
    int target_y = r.y + r.h + GAP;

    int best = -1;
    int best_dy = 999999;

    for (int j = 0; j < be->block_count; j++) {
        if (j == idx) continue;
        SDL_Rect o = be->blocks[j].r;

        int dx = iabs_int(o.x - target_x);
        int dy = iabs_int(o.y - target_y);

        if (dx <= TOL_X && dy <= TOL_Y) {
            if (dy < best_dy) { best_dy = dy; best = j; }
        }
    }
    return best;
}

static int be_has_above(const BlockEditor* be, int idx) {
    for (int i = 0; i < be->block_count; i++) {
        if (i == idx) continue;
        if (be_find_below(be, i) == idx) return 1;
    }
    return 0;
}

static int be_find_top_head(const BlockEditor* be) {
    int head = -1;
    for (int i = 0; i < be->block_count; i++) {
        if (be_has_above(be, i)) continue;

        if (head == -1) { head = i; continue; }

        SDL_Rect a = be->blocks[i].r;
        SDL_Rect b = be->blocks[head].r;
        if (a.y < b.y || (a.y == b.y && a.x < b.x)) head = i;
    }
    return head;
}

static int be_find_top_greenflag_head(const BlockEditor* be) {
    int best = -1;
    for (int i = 0; i < be->block_count; i++) {
        if (be->blocks[i].type != BLK_EVENT_GREEN_FLAG) continue;
        if (be_has_above(be, i)) continue; // must be a head

        if (best == -1) { best = i; continue; }
        SDL_Rect a = be->blocks[i].r;
        SDL_Rect b = be->blocks[best].r;
        if (a.y < b.y || (a.y == b.y && a.x < b.x)) best = i;
    }
    return best;
}

static int be_find_top_hat_head(const BlockEditor* be, BlockType hat_type) {
    int best = -1;
    for (int i = 0; i < be->block_count; i++) {
        if (be->blocks[i].type != hat_type) continue;
        if (be_has_above(be, i)) continue; // must be a head

        if (best == -1) { best = i; continue; }

        SDL_Rect a = be->blocks[i].r;
        SDL_Rect b = be->blocks[best].r;
        if (a.y < b.y || (a.y == b.y && a.x < b.x)) best = i;
    }
    return best;
}

// Like be_find_top_hat_head, but ignores hats that have no block snapped under them.
// This fixes the common case where a user drags an extra empty hat above the real script.
static int be_find_top_hat_head_with_body(const BlockEditor* be, BlockType hat_type) {
    int best = -1;
    for (int i = 0; i < be->block_count; i++) {
        if (be->blocks[i].type != hat_type) continue;
        if (be_has_above(be, i)) continue; // must be a head
        if (be_find_below(be, i) == -1) continue; // must have body

        if (best == -1) { best = i; continue; }
        SDL_Rect a = be->blocks[i].r;
        SDL_Rect b = be->blocks[best].r;
        if (a.y < b.y || (a.y == b.y && a.x < b.x)) best = i;
    }
    return best;
}

static int blk_is_if(BlockType t) {
    return t == BLK_IF_X_GT || t == BLK_IF_X_LT || t == BLK_IF_Y_GT || t == BLK_IF_Y_LT || t == BLK_IF_RANDOM_LT;
}

static CondCode blk_to_cond(BlockType t) {
    switch (t) {
        case BLK_IF_X_GT:      return COND_SPRITE_X_GT;
        case BLK_IF_X_LT:      return COND_SPRITE_X_LT;
        case BLK_IF_Y_GT:      return COND_SPRITE_Y_GT;
        case BLK_IF_Y_LT:      return COND_SPRITE_Y_LT;
        case BLK_IF_RANDOM_LT: return COND_RANDOM_LT;
        default:               return COND_TRUE;
    }
}

static int compile_workspace_script_from_start(const BlockEditor* be, int start_idx, Instr* out, int cap) {
    if (!be || !out || cap <= 0) return 0;
    if (start_idx < 0 || start_idx >= be->block_count) return 0;

    // Collect linear order from start_idx following snap-below links
    int seq[MAX_WORKSPACE_BLOCKS];
    int seq_n = 0;

    int idx = start_idx;
    int guard = 0;
    while (idx >= 0 && idx < be->block_count && guard < MAX_WORKSPACE_BLOCKS) {
        seq[seq_n++] = idx;
        idx = be_find_below(be, idx);
        guard++;
    }

    int len = 0;

    // Patch stacks
    int rep_stack[64]; int rep_top = 0;
    int for_stack[64]; int for_top = 0;

    struct IfItem { int if_ip; int else_ip; };
    IfItem if_stack[64]; int if_top = 0;

    auto emit = [&](const Instr& in) -> int {
        if (len >= cap) return 0;
        out[len++] = in;
        return 1;
    };
    auto after_current = [&]() -> int { return len; };

    for (int si = 0; si < seq_n; si++) {
        const BlockInstance* b = &be->blocks[seq[si]];

        switch (b->type) {
            // ---- Motion ----
            case BLK_MOVE_STEPS:
                if (!emit(Instr{ b->id, OP_MOVE_STEPS, (double)b->a, 0.0, 0, 0, COND_TRUE })) return len;
                break;

            case BLK_TURN_DEG:
                if (!emit(Instr{ b->id, OP_TURN_DEG, (double)b->a, 0.0, 0, 0, COND_TRUE })) return len;
                break;

            case BLK_GOTO_XY:
                if (len + 2 > cap) return len;
                out[len++] = Instr{ b->id, OP_SET_X, (double)b->a, 0.0, 0, 0, COND_TRUE };
                out[len++] = Instr{ b->id, OP_SET_Y, (double)b->b, 0.0, 0, 0, COND_TRUE };
                break;

            case BLK_CHANGE_X_BY:
                if (!emit(Instr{ b->id, OP_CHANGE_X, (double)b->a, 0.0, 0, 0, COND_TRUE })) return len;
                break;

            case BLK_CHANGE_Y_BY:
                if (!emit(Instr{ b->id, OP_CHANGE_Y, (double)b->a, 0.0, 0, 0, COND_TRUE })) return len;
                break;

            case BLK_GOTO_RANDOM:
                if (!emit(Instr{ b->id, OP_GOTO_RANDOM, 0.0, 0.0, 0, 0, COND_TRUE })) return len;
                break;

            case BLK_BOUNCE_EDGE:
                if (!emit(Instr{ b->id, OP_IF_ON_EDGE_BOUNCE, 0.0, 0.0, 0, 0, COND_TRUE })) return len;
                break;

            case BLK_CHANGE_X_BY_POP:
                if (!emit(Instr{ b->id, OP_CHANGE_X_POP, 0.0, 0.0, 0, 0, COND_TRUE })) return len;
                break;

            case BLK_CHANGE_Y_BY_POP:
                if (!emit(Instr{ b->id, OP_CHANGE_Y_POP, 0.0, 0.0, 0, 0, COND_TRUE })) return len;
                break;

            case BLK_SET_X_POP:
                if (!emit(Instr{ b->id, OP_SET_X_POP, 0.0, 0.0, 0, 0, COND_TRUE })) return len;
                break;

            case BLK_SET_Y_POP:
                if (!emit(Instr{ b->id, OP_SET_Y_POP, 0.0, 0.0, 0, 0, COND_TRUE })) return len;
                break;

            // ---------------------- Operators (stack-based) ----------------------
            case BLK_PUSH_NUM:
                if (!emit(Instr{ b->id, OP_PUSH_NUM, (double)b->a, 0.0, 0, 0, COND_TRUE })) return len;
                break;
            case BLK_RANDOM_RANGE:
                if (!emit(Instr{ b->id, OP_RANDOM_RANGE, (double)b->a, (double)b->b, 0, 0, COND_TRUE })) return len;
                break;
            case BLK_ADD: if (!emit(Instr{ b->id, OP_ADD, 0.0,0.0, 0,0, COND_TRUE })) return len; break;
            case BLK_SUB: if (!emit(Instr{ b->id, OP_SUB, 0.0,0.0, 0,0, COND_TRUE })) return len; break;
            case BLK_MUL: if (!emit(Instr{ b->id, OP_MUL, 0.0,0.0, 0,0, COND_TRUE })) return len; break;
            case BLK_DIV: if (!emit(Instr{ b->id, OP_DIV, 0.0,0.0, 0,0, COND_TRUE })) return len; break;
            case BLK_GT:  if (!emit(Instr{ b->id, OP_GT,  0.0,0.0, 0,0, COND_TRUE })) return len; break;
            case BLK_LT:  if (!emit(Instr{ b->id, OP_LT,  0.0,0.0, 0,0, COND_TRUE })) return len; break;
            case BLK_EQ:  if (!emit(Instr{ b->id, OP_EQ,  0.0,0.0, 0,0, COND_TRUE })) return len; break;
            case BLK_AND: if (!emit(Instr{ b->id, OP_AND, 0.0,0.0, 0,0, COND_TRUE })) return len; break;
            case BLK_OR:  if (!emit(Instr{ b->id, OP_OR,  0.0,0.0, 0,0, COND_TRUE })) return len; break;
            case BLK_NOT: if (!emit(Instr{ b->id, OP_NOT, 0.0,0.0, 0,0, COND_TRUE })) return len; break;

            // ---------------------- Variables (var0, stack-based) ----------------------
            case BLK_VAR0_READ:
                if (!emit(Instr{ b->id, OP_READ_VAR_PUSH, 0.0,0.0, 0, 0, COND_TRUE })) return len;
                // reuse `count` field as var id
                out[len-1].count = 0;
                break;
            case BLK_VAR0_SET:
                if (!emit(Instr{ b->id, OP_SET_VAR_POP, 0.0,0.0, 0, 0, COND_TRUE })) return len;
                out[len-1].count = 0;
                break;
            case BLK_VAR0_CHANGE:
                if (!emit(Instr{ b->id, OP_CHANGE_VAR_POP, 0.0,0.0, 0, 0, COND_TRUE })) return len;
                out[len-1].count = 0;
                break;

            // ---------------------- Sensing (stack-based) ----------------------
            case BLK_SENSE_MOUSE_X:
                if (!emit(Instr{ b->id, OP_PUSH_MOUSE_X, 0.0,0.0, 0,0, COND_TRUE })) return len;
                break;
            case BLK_SENSE_MOUSE_Y:
                if (!emit(Instr{ b->id, OP_PUSH_MOUSE_Y, 0.0,0.0, 0,0, COND_TRUE })) return len;
                break;
            case BLK_SENSE_MOUSE_DOWN:
                if (!emit(Instr{ b->id, OP_PUSH_MOUSE_DOWN, 0.0,0.0, 0,0, COND_TRUE })) return len;
                break;
            case BLK_SENSE_TIMER:
                if (!emit(Instr{ b->id, OP_PUSH_TIMER, 0.0,0.0, 0,0, COND_TRUE })) return len;
                break;
            case BLK_SENSE_DISTANCE_MOUSE:
                if (!emit(Instr{ b->id, OP_PUSH_DIST_MOUSE, 0.0,0.0, 0,0, COND_TRUE })) return len;
                break;
            case BLK_SENSE_ASK_WAIT:
                if (!emit(Instr{ b->id, OP_ASK_WAIT, 0.0,0.0, 0,0, COND_TRUE })) return len;
                break;
            case BLK_SENSE_ANSWER:
                if (!emit(Instr{ b->id, OP_PUSH_ANSWER, 0.0,0.0, 0,0, COND_TRUE })) return len;
                break;

            // Events (statement)
            case BLK_BROADCAST_MSG1:
                if (!emit(Instr{ b->id, OP_BROADCAST, 0.0,0.0, 0, 1, COND_TRUE })) return len;
                // reuse count field as message id
                out[len-1].count = 1;
                break;

            // ---- Hats inside a script (ignore safely) ----
            case BLK_EVENT_GREEN_FLAG:
            case BLK_EVENT_KEY_SPACE:
            case BLK_EVENT_RECV_MSG1:
                break;

            // ---- Control ----
            case BLK_WAIT_MS:
                if (!emit(Instr{ b->id, OP_WAIT_MS, (double)b->a, 0.0, 0, 0, COND_TRUE })) return len;
                break;

            case BLK_REPEAT_BEGIN: {
                int ip = len;
                if (!emit(Instr{ b->id, OP_REPEAT_BEGIN, 0.0, 0.0, 0, b->a, COND_TRUE })) return len;
                if (rep_top < 64) rep_stack[rep_top++] = ip;
                break;
            }

            case BLK_REPEAT_END: {
                if (!emit(Instr{ b->id, OP_REPEAT_END, 0.0, 0.0, 0, 0, COND_TRUE })) return len;
                if (rep_top > 0) {
                    int beg_ip = rep_stack[--rep_top];
                    out[beg_ip].jump = after_current(); // skip loop when count<=0
                }
                break;
            }

            case BLK_FOREVER_BEGIN: {
                int ip = len;
                if (!emit(Instr{ b->id, OP_FOREVER_BEGIN, 0.0, 0.0, 0, 0, COND_TRUE })) return len;
                if (for_top < 64) for_stack[for_top++] = ip;
                break;
            }

            case BLK_FOREVER_END: {
                int end_ip = len;
                if (!emit(Instr{ b->id, OP_FOREVER_END, 0.0, 0.0, 0, 0, COND_TRUE })) return len;
                int target = 0;
                if (for_top > 0) target = for_stack[--for_top];
                out[end_ip].jump = target;
                break;
            }

            // ---- IF / ELSE / ENDIF ----
            default:
                if (blk_is_if(b->type)) {
                    CondCode cc = blk_to_cond(b->type);
                    double a = (double)b->a;
                    if (cc == COND_RANDOM_LT) a = a / 100.0;

                    int ip = len;
                    if (!emit(Instr{ b->id, OP_IF_BEGIN, a, 0.0, 0, 0, cc })) return len;

                    if (if_top < 64) {
                        if_stack[if_top].if_ip = ip;
                        if_stack[if_top].else_ip = -1;
                        if_top++;
                    }
                }
                else if (b->type == BLK_ELSE) {
                    int ip = len;
                    if (!emit(Instr{ b->id, OP_ELSE, 0.0, 0.0, 0, 0, COND_TRUE })) return len;

                    if (if_top > 0) {
                        auto& top = if_stack[if_top - 1];
                        out[top.if_ip].jump = ip + 1; // if-false -> else body
                        top.else_ip = ip;
                    }
                }
                else if (b->type == BLK_ENDIF) {
                    if (!emit(Instr{ b->id, OP_ENDIF, 0.0, 0.0, 0, 0, COND_TRUE })) return len;

                    if (if_top > 0) {
                        auto top = if_stack[--if_top];
                        int after_endif = after_current();
                        if (top.else_ip != -1) out[top.else_ip].jump = after_endif;
                        else out[top.if_ip].jump = after_endif;
                    }
                }
                break;
        }
    }

    int end_target = len;

    while (if_top > 0) {
        auto top = if_stack[--if_top];
        out[top.if_ip].jump = end_target;
        if (top.else_ip != -1) out[top.else_ip].jump = end_target;
    }
    while (rep_top > 0) {
        int beg_ip = rep_stack[--rep_top];
        out[beg_ip].jump = end_target;
    }

    if (len < cap) out[len++] = Instr{ 0, OP_END, 0.0, 0.0, 0, 0, COND_TRUE };
    return len;
}

static void start_from_workspace(Runtime* runtime, const BlockEditor* be) {
    // clear old scripts
    runtime_set_main_script(runtime, NULL, 0);
    runtime_set_key_script(runtime, SDLK_SPACE, NULL, 0);
    runtime_set_recv_script(runtime, 1, NULL, 0);

    // ---------- Green flag ----------
    int gf_hat = be_find_top_hat_head_with_body(be, BLK_EVENT_GREEN_FLAG);
    if (gf_hat != -1) {
        int start = be_find_below(be, gf_hat);
        if (start != -1) {
            Instr code[RUNTIME_MAX_MAIN_CODE];
            int len = compile_workspace_script_from_start(be, start, code, RUNTIME_MAX_MAIN_CODE);
            if (len > 0) runtime_set_main_script(runtime, code, len);
            log_write(LogRecord{0,0,"COMPILE","GreenFlag","main script compiled", LOG_INFO});
        }
    }

    // ---------- Space key ----------
    int sp_hat = be_find_top_hat_head_with_body(be, BLK_EVENT_KEY_SPACE);
    if (sp_hat != -1) {
        int start = be_find_below(be, sp_hat);
        if (start != -1) {
            Instr code[RUNTIME_MAX_MAIN_CODE];
            int len = compile_workspace_script_from_start(be, start, code, RUNTIME_MAX_MAIN_CODE);
            if (len > 0) runtime_set_key_script(runtime, SDLK_SPACE, code, len);
            log_write(LogRecord{0,0,"COMPILE","KeySpace","space script compiled", LOG_INFO});
        }
    }

    // ---------- Receive msg1 ----------
    int m1_hat = be_find_top_hat_head_with_body(be, BLK_EVENT_RECV_MSG1);
    if (m1_hat != -1) {
        int start = be_find_below(be, m1_hat);
        if (start != -1) {
            Instr code[RUNTIME_MAX_MAIN_CODE];
            int len = compile_workspace_script_from_start(be, start, code, RUNTIME_MAX_MAIN_CODE);
            if (len > 0) runtime_set_recv_script(runtime, 1, code, len);
            log_write(LogRecord{0,0,"COMPILE","RecvMsg1","recv script compiled", LOG_INFO});
        }
    }

    // run main
    runtime_green_flag(runtime);
}

// ============================================================
// app_run
// ============================================================
int app_run(Project* project, Runtime* runtime) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        log_write(LogRecord{0,0,"SDL","Init failed", SDL_GetError(), LOG_ERROR});
        return 0;
    }

    if (TTF_Init() != 0) {
        log_write(LogRecord{0,0,"TTF","TTF_Init failed", TTF_GetError(), LOG_ERROR});
    }

    SDL_Window* win = SDL_CreateWindow("project_scratch",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!win) {
        log_write(LogRecord{0,0,"SDL","CreateWindow failed", SDL_GetError(), LOG_ERROR});
        TTF_Quit();
        SDL_Quit();
        return 0;
    }

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        log_write(LogRecord{0,0,"SDL","CreateRenderer failed", SDL_GetError(), LOG_ERROR});
        SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return 0;
    }

    TTF_Font* font = TTF_OpenFont("assets/fonts/Arial.ttf", 18);
    if (!font) {
        log_write(LogRecord{0,0,"TTF","OpenFont failed", TTF_GetError(), LOG_ERROR});
    }

    if (project && project->sprite_count == 0) {
        project_add_sprite(project);
    }

    BlockEditor be;
    block_editor_init(&be);

    // Layout constants
    const int TOP = 50;
    const int RIGHT = 360;
    const int CAT_W = 140;
    const int PAL_W = 260;

    // Right panel sub-layout
    const int RP_PAD = 14;
    const int STAGE_H = 330;
    const int BTN_H = 40;

    int running = 1;

    // Sensing / Ask UI state
    int ask_active = 0;
    std::string ask_text;

    // Stage sprite dragging
    int dragging_sprite = 0;
    int drag_sprite_idx = -1;

    while (running) {
        int W, H;
        SDL_GetWindowSize(win, &W, &H);

        // --- SAFE LAYOUT CLAMPING ---
        const int min_left  = CAT_W + PAL_W + 220;
        const int min_right = 240;

        int left_w = W - RIGHT;
        if (left_w < min_left) left_w = min_left;
        if (left_w > W - min_right) left_w = W - min_right;
        if (left_w < 0) left_w = 0;

        SDL_Rect rect_cat = {0, TOP, CAT_W, H - TOP};
        SDL_Rect rect_palette = {CAT_W, TOP, PAL_W, H - TOP};
        SDL_Rect rect_workspace = {CAT_W + PAL_W, TOP, left_w - (CAT_W + PAL_W), H - TOP};
        if (rect_workspace.w < 50) rect_workspace.w = 50;

        block_editor_set_layout(&be, rect_cat, rect_palette, rect_workspace);

        SDL_Rect rect_right = {left_w, TOP, W - left_w, H - TOP};
        if (rect_right.w < min_right) rect_right.w = min_right;

        SDL_Rect rect_stage = {
            rect_right.x + RP_PAD,
            rect_right.y + RP_PAD,
            rect_right.w - 2*RP_PAD,
            STAGE_H
        };

        SDL_Rect rect_sprite_panel = {
            rect_right.x + RP_PAD,
            rect_stage.y + rect_stage.h + RP_PAD,
            rect_right.w - 2*RP_PAD,
            rect_right.y + rect_right.h - (rect_stage.y + rect_stage.h + RP_PAD) - RP_PAD
        };

        SDL_Rect btn_add = { rect_sprite_panel.x, rect_sprite_panel.y, rect_sprite_panel.w/3 - 6, BTN_H };
        SDL_Rect btn_del = { btn_add.x + btn_add.w + 9, rect_sprite_panel.y, rect_sprite_panel.w/3 - 6, BTN_H };
        SDL_Rect btn_vis = { btn_del.x + btn_del.w + 9, rect_sprite_panel.y,
                             rect_sprite_panel.w - (btn_del.x + btn_del.w + 9 - rect_sprite_panel.x), BTN_H };

        SDL_Rect rect_sprite_list = {
            rect_sprite_panel.x,
            rect_sprite_panel.y + BTN_H + 10,
            rect_sprite_panel.w,
            rect_sprite_panel.h - (BTN_H + 10)
        };

        // Green flag + stop buttons
        int gf_r = 10;
        int gf_cx = rect_stage.x + 18;
        int gf_cy = rect_stage.y - 18;
        int st_cx = rect_stage.x + 45;
        int st_cy = rect_stage.y - 18;
        int pa_cx = rect_stage.x + 72;
        int pa_cy = rect_stage.y - 18;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;

            block_editor_handle_event(&be, &e);

            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) {
                running = 0;
            }

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = e.button.x, my = e.button.y;

                int dxg = mx - gf_cx, dyg = my - gf_cy;
                if (dxg*dxg + dyg*dyg <= gf_r*gf_r) {
                    start_from_workspace(runtime, &be);
                }
                int dxs = mx - st_cx, dys = my - st_cy;
                if (dxs*dxs + dys*dys <= gf_r*gf_r) {
                    runtime_stop_all(runtime);
                }

                int dxp = mx - pa_cx, dyp = my - pa_cy;
                if (dxp*dxp + dyp*dyp <= gf_r*gf_r) {
                    runtime_set_paused(runtime, !runtime->paused);
                }

                // Stage sprite drag (simple)
                if (!ask_active && project && pt_in_rect(mx, my, &rect_stage)) {
                    double sx, sy;
                    mouse_to_stage(&rect_stage, mx, my, &sx, &sy);

                    int hit = -1;
                    for (int i = project->sprite_count - 1; i >= 0; --i) {
                        const Sprite* sp = &project->sprites[i];
                        if (!sp->visible) continue;

                        int base = 30;
                        int sz = (int)(base * (sp->size / 100.0));
                        if (sz < 6) sz = 6;

                        double cx = sp->x;
                        double cy = sp->y;

                        if (sx >= cx - sz/2 && sx <= cx + sz/2 && sy >= cy - sz/2 && sy <= cy + sz/2) {
                            hit = i;
                            break;
                        }
                    }

                    if (hit != -1) {
                        project->active_sprite_index = hit;
                        dragging_sprite = 1;
                        drag_sprite_idx = hit;

                        // move immediately
                        Sprite* sp = &project->sprites[hit];
                        sp->x = (int)sx;
                        sp->y = (int)sy;
                    }
                }

                if (pt_in_rect(mx, my, &btn_add)) project_add_sprite(project);
                if (pt_in_rect(mx, my, &btn_del)) project_delete_active(project);
                if (pt_in_rect(mx, my, &btn_vis)) project_toggle_visible(project);

                if (pt_in_rect(mx, my, &rect_sprite_list) && project && project->sprite_count > 0) {
                    int row_h = 34;
                    int idx2 = (my - rect_sprite_list.y) / row_h;
                    if (idx2 >= 0 && idx2 < project->sprite_count) {
                        project->active_sprite_index = idx2;
                    }
                }
            }

            if (e.type == SDL_MOUSEMOTION) {
                if (!ask_active && dragging_sprite && project && drag_sprite_idx >= 0 && drag_sprite_idx < project->sprite_count) {
                    int mx = e.motion.x, my = e.motion.y;
                    double sx, sy;
                    mouse_to_stage(&rect_stage, mx, my, &sx, &sy);

                    Sprite* sp = &project->sprites[drag_sprite_idx];
                    sp->x = (int)sx;
                    sp->y = (int)sy;
                }
            }

            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                dragging_sprite = 0;
                drag_sprite_idx = -1;
            }

            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;

                // Ask modal input (numeric)
                if (ask_active) {
                    if ((k == SDLK_BACKSPACE || k == SDLK_DELETE) && !ask_text.empty()) {
                        ask_text.pop_back();
                    }
                    if (k == SDLK_ESCAPE) {
                        scheduler_submit_answer(&runtime->sched, 0.0);
                        ask_active = 0;
                        ask_text.clear();
                        SDL_StopTextInput();
                    }
                    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
                        double v = 0.0;
                        if (!ask_text.empty()) v = atof(ask_text.c_str());
                        scheduler_submit_answer(&runtime->sched, v);
                        ask_active = 0;
                        ask_text.clear();
                        SDL_StopTextInput();
                    }
                    continue;
                }

                // Feed key events to runtime (for key-event scripts)
                runtime_post_key(runtime, (int)k);

                // Debug controls
                if (k == SDLK_g) start_from_workspace(runtime, &be);
                if (k == SDLK_x) runtime_stop_all(runtime);
                if (k == SDLK_p) runtime_set_paused(runtime, !runtime->paused);
                if (k == SDLK_s) runtime_set_step_mode(runtime, !runtime->step_mode);
                if (k == SDLK_n) runtime_request_step(runtime);

                // Sprite controls (active sprite)
                if (project && project->sprite_count > 0) {
                    int a = project->active_sprite_index;
                    if (a < 0) a = 0;
                    if (a >= project->sprite_count) a = 0;
                    Sprite* s = &project->sprites[a];

                    if (k == SDLK_LEFT)  s->x -= 5;
                    if (k == SDLK_RIGHT) s->x += 5;
                    if (k == SDLK_UP)    s->y += 5;
                    if (k == SDLK_DOWN)  s->y -= 5;

                    if (k == SDLK_LEFTBRACKET)  s->dir -= 5;
                    if (k == SDLK_RIGHTBRACKET) s->dir += 5;

                    if (k == SDLK_MINUS) s->size -= 5;
                    if (k == SDLK_EQUALS) s->size += 5;
                    if (s->size < 10) s->size = 10;
                    if (s->size > 300) s->size = 300;
                }
            }

            if (e.type == SDL_TEXTINPUT) {
                if (ask_active) {
                    // Keep it simple: allow up to 32 chars.
                    if (ask_text.size() < 32) ask_text += e.text.text;
                }
            }
        }

        // Update sensing mouse state every frame
        {
            int mx, my;
            Uint32 st = SDL_GetMouseState(&mx, &my);
            int down = (st & SDL_BUTTON(SDL_BUTTON_LEFT)) ? 1 : 0;
            double sx, sy;
            mouse_to_stage(&rect_stage, mx, my, &sx, &sy);
            scheduler_set_mouse(&runtime->sched, sx, sy, down);
        }

        // Open ask modal if requested by the scheduler
        if (runtime->sched.ask_pending && !ask_active) {
            ask_active = 1;
            ask_text.clear();
            SDL_StartTextInput();
        }

        runtime_tick(runtime, project);
        update_window_title(win, project);

        SDL_SetRenderDrawColor(ren, 230, 230, 235, 255);
        SDL_RenderClear(ren);

        SDL_Rect topbar{0, 0, W, TOP};
        draw_filled_rect(ren, topbar, 133, 94, 205, 255);
        draw_text(ren, font, 14, 14, "project_scratch      Code   Costumes   Sounds", SDL_Color{255,255,255,255});

        // small run-state indicator (helps debugging "green flag not working")
        {
            const char* run = runtime_is_running(runtime) ? "RUN" : "STOP";
            const char* paused = runtime->paused ? "PAUSED" : "";
            const char* step = runtime->step_mode ? "STEP" : "";

            char buf[64];
            if (paused[0] && step[0]) snprintf(buf, sizeof(buf), "%s | %s", paused, step);
            else if (paused[0]) snprintf(buf, sizeof(buf), "%s", paused);
            else if (step[0]) snprintf(buf, sizeof(buf), "%s", step);
            else snprintf(buf, sizeof(buf), "%s", run);

            draw_text(ren, font, W - 160, 14, buf, SDL_Color{255,255,255,255});
        }

        block_editor_render(&be, ren, font, runtime_current_block(runtime));

        draw_rect(ren, rect_stage, 120, 120, 120, 255);
        draw_filled_rect(ren, rect_stage, 255, 255, 255, 255);

        int sx0 = rect_stage.x + rect_stage.w/2;
        int sy0 = rect_stage.y + rect_stage.h/2;
        draw_line(ren, rect_stage.x, sy0, rect_stage.x + rect_stage.w, sy0, 220,220,220,255);
        draw_line(ren, sx0, rect_stage.y, sx0, rect_stage.y + rect_stage.h, 220,220,220,255);

        // variable monitor (var0)
        {
            Value v = varstore_get(&runtime->vars, 0);
            char buf[64];
            if (v.type == VAL_BOOL) snprintf(buf, sizeof(buf), "var0: %s", v.boolean ? "true" : "false");
            else snprintf(buf, sizeof(buf), "var0: %.2f", v.num);
            draw_text(ren, font, rect_stage.x + 8, rect_stage.y + 8, buf, SDL_Color{40,40,40,255});
        }

        draw_circle_button(ren, gf_cx, gf_cy, gf_r, SDL_Color{70, 200, 70, 255}, SDL_Color{30, 120, 30, 255});
        draw_circle_button(ren, st_cx, st_cy, gf_r, SDL_Color{230, 80, 80, 255}, SDL_Color{140, 30, 30, 255});
        draw_circle_button(ren, pa_cx, pa_cy, gf_r, SDL_Color{250, 210, 70, 255}, SDL_Color{170, 120, 20, 255});

        // labels on run controls
        draw_text(ren, font, gf_cx - 4, gf_cy - 10, ">", SDL_Color{20,60,20,255});
        draw_text(ren, font, st_cx - 4, st_cy - 10, "x", SDL_Color{80,20,20,255});
        draw_text(ren, font, pa_cx - 7, pa_cy - 10, runtime->paused ? ">" : "||", SDL_Color{70,45,0,255});

        if (project && project->sprite_count > 0) {
            for (int i = 0; i < project->sprite_count; i++) {
                const Sprite* s = &project->sprites[i];
                if (!s->visible) continue;

                int cx = sx0 + (int)s->x;
                int cy = sy0 - (int)s->y;

                int base = 30;
                int sz = (int)(base * (s->size / 100.0));
                if (sz < 6) sz = 6;

                SDL_Rect rr{ cx - sz/2, cy - sz/2, sz, sz };

                if (i == project->active_sprite_index) {
                    draw_rect(ren, SDL_Rect{rr.x-2, rr.y-2, rr.w+4, rr.h+4}, 150, 100, 220, 255);
                }

                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                SDL_RenderFillRect(ren, &rr);
                draw_rect(ren, rr, 120, 120, 120, 255);
            }
        }

        // Ask overlay (numeric)
        if (ask_active) {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

            SDL_Rect ov{ rect_stage.x + 20, rect_stage.y + 60, rect_stage.w - 40, 120 };
            draw_filled_rect(ren, ov, 0, 0, 0, 140);
            draw_rect(ren, ov, 240, 240, 240, 255);

            draw_text(ren, font, ov.x + 14, ov.y + 12, "Ask: enter a number and press Enter", SDL_Color{255,255,255,255});

            SDL_Rect box{ ov.x + 14, ov.y + 54, ov.w - 28, 44 };
            draw_filled_rect(ren, box, 255,255,255,255);
            draw_rect(ren, box, 40,40,40,255);

            std::string line = ask_text;
            if (((SDL_GetTicks()/350) % 2) == 0) line += "_";
            draw_text(ren, font, box.x + 10, box.y + 9, line.c_str(), SDL_Color{20,20,20,255});

            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
        }

        draw_filled_rect(ren, rect_sprite_panel, 245, 245, 248, 255);
        draw_rect(ren, rect_sprite_panel, 200, 200, 210, 255);

        draw_filled_rect(ren, btn_add,  60, 170,  80, 255);
        draw_filled_rect(ren, btn_del, 200,  80,  80, 255);
        draw_filled_rect(ren, btn_vis,  80,  80, 190, 255);
        draw_text(ren, font, btn_add.x + 18, btn_add.y + 9, "Add", SDL_Color{255,255,255,255});
        draw_text(ren, font, btn_del.x + 18, btn_del.y + 9, "Del", SDL_Color{255,255,255,255});
        draw_text(ren, font, btn_vis.x + 18, btn_vis.y + 9, "Vis", SDL_Color{255,255,255,255});

        draw_text(ren, font, rect_sprite_list.x, rect_sprite_list.y - 24, "Sprites", SDL_Color{60,60,60,255});
        if (project && project->sprite_count > 0) {
            int row_h = 34;
            for (int i = 0; i < project->sprite_count; i++) {
                SDL_Rect row{ rect_sprite_list.x, rect_sprite_list.y + i*row_h, rect_sprite_list.w, row_h };
                if (i == project->active_sprite_index) {
                    draw_rect(ren, row, 150, 100, 220, 255);
                } else {
                    draw_rect(ren, row, 210, 210, 220, 255);
                }

                SDL_Rect box{ row.x + 8, row.y + 9, 16, 16 };
                draw_rect(ren, box, 120,120,120,255);
                if (project->sprites[i].visible) {
                    draw_filled_rect(ren, SDL_Rect{box.x+3, box.y+3, 10, 10}, 120,120,120,255);
                }

                draw_text(ren, font, row.x + 32, row.y + 7, project->sprites[i].name, SDL_Color{50,50,50,255});
            }

            int a = project->active_sprite_index;
            if (a < 0) a = 0;
            if (a >= project->sprite_count) a = 0;
            const Sprite* s = &project->sprites[a];

            int info_y = rect_sprite_list.y + project->sprite_count * 34 + 16;
            if (info_y < rect_sprite_panel.y + rect_sprite_panel.h - 120) {
                char buf[128];

                draw_text(ren, font, rect_sprite_panel.x, info_y, "Active Sprite", SDL_Color{60,60,60,255});
                info_y += 22;

                snprintf(buf, sizeof(buf), "Name: %s", s->name);
                draw_text(ren, font, rect_sprite_panel.x, info_y, buf, SDL_Color{60,60,60,255});
                info_y += 22;

                snprintf(buf, sizeof(buf), "X: %d   Y: %d", (int)s->x, (int)s->y);
                draw_text(ren, font, rect_sprite_panel.x, info_y, buf, SDL_Color{60,60,60,255});
                info_y += 22;

                snprintf(buf, sizeof(buf), "Dir: %d   Size: %d", (int)s->dir, (int)s->size);
                draw_text(ren, font, rect_sprite_panel.x, info_y, buf, SDL_Color{60,60,60,255});
            }
        }

        draw_text(ren, font, rect_right.x + 10, H - 64, "Keys:", SDL_Color{60,60,60,255});
        draw_text(ren, font, rect_right.x + 10, H - 42, "Arrows move | [ ] rotate | -/= size", SDL_Color{60,60,60,255});
        draw_text(ren, font, rect_right.x + 10, H - 20, "G start | X stop | P pause | S step | N next", SDL_Color{60,60,60,255});

        SDL_RenderPresent(ren);
    }

    if (font) TTF_CloseFont(font);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 1;
}