#include "ui/block_editor.h"
#include <string.h>
#include <stdio.h>

static int pt_in_rect(int x, int y, SDL_Rect r) {
    return (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h);
}

static void draw_rect(SDL_Renderer* ren, SDL_Rect r, SDL_Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    SDL_RenderDrawRect(ren, &r);
}

static void fill_rect(SDL_Renderer* ren, SDL_Rect r, SDL_Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(ren, &r);
}

static void draw_text(SDL_Renderer* ren, TTF_Font* font, int x, int y, const char* txt, SDL_Color c) {
    if (!font || !txt || !txt[0]) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, txt, c);
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
    if (!t) { SDL_FreeSurface(s); return; }
    SDL_Rect dst{ x, y, s->w, s->h };
    SDL_RenderCopy(ren, t, NULL, &dst);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
}

static const char* cat_name(BlockCategory c) {
    switch (c) {
        case CAT_MOTION:    return "Motion";
        case CAT_LOOKS:     return "Looks";
        case CAT_SOUND:     return "Sound";
        case CAT_EVENTS:    return "Events";
        case CAT_CONTROL:   return "Control";
        case CAT_SENSING:   return "Sensing";
        case CAT_OPERATORS: return "Operators";
        case CAT_VARIABLES: return "Variables";
        default: return "???";
    }
}

static SDL_Color cat_color(BlockCategory c) {
    switch (c) {
        case CAT_MOTION:    return SDL_Color{  70, 130, 255, 255 };
        case CAT_LOOKS:     return SDL_Color{ 160,  90, 255, 255 };
        case CAT_SOUND:     return SDL_Color{ 255,  90, 200, 255 };
        case CAT_EVENTS:    return SDL_Color{ 255, 200,  60, 255 };
        case CAT_CONTROL:   return SDL_Color{ 255, 160,  60, 255 };
        case CAT_SENSING:   return SDL_Color{  80, 210, 210, 255 };
        case CAT_OPERATORS: return SDL_Color{  60, 200,  90, 255 };
        case CAT_VARIABLES: return SDL_Color{ 255, 140,  60, 255 };
        default: return SDL_Color{ 200, 200, 200, 255 };
    }
}

static SDL_Color border_from_fill(SDL_Color f) {
    int r = (int)f.r * 45 / 100;
    int g = (int)f.g * 45 / 100;
    int b = (int)f.b * 45 / 100;
    if (r < 30) r = 30;
    if (g < 30) g = 30;
    if (b < 30) b = 30;
    return SDL_Color{ (uint8_t)r, (uint8_t)g, (uint8_t)b, 255 };
}

static SDL_Color text_for_fill(SDL_Color f) {
    int sum = (int)f.r + (int)f.g + (int)f.b;
    if (sum >= 520) return SDL_Color{ 25, 25, 25, 255 };
    return SDL_Color{ 255, 255, 255, 255 };
}

static BlockCategory type_category(BlockType t) {
    switch (t) {
        case BLK_MOVE_STEPS:
        case BLK_TURN_DEG:
        case BLK_GOTO_XY:
            return CAT_MOTION;

        case BLK_EVENT_GREEN_FLAG:
        case BLK_EVENT_KEY_SPACE:
        case BLK_EVENT_RECV_MSG1:
            return CAT_EVENTS;

        case BLK_WAIT_MS:
        case BLK_REPEAT_BEGIN:
        case BLK_REPEAT_END:
        case BLK_FOREVER_BEGIN:
        case BLK_FOREVER_END:
        case BLK_IF_X_GT:
        case BLK_IF_X_LT:
        case BLK_IF_Y_GT:
        case BLK_IF_Y_LT:
        case BLK_IF_RANDOM_LT:
        case BLK_ELSE:
        case BLK_ENDIF:
            return CAT_CONTROL;

        default:
            return CAT_MOTION;
    }
}

static const char* block_label(BlockType t, int a, int b) {
    static char buf[128];
    switch (t) {
        case BLK_MOVE_STEPS: snprintf(buf, sizeof(buf), "move %d steps", a); break;
        case BLK_TURN_DEG:   snprintf(buf, sizeof(buf), "turn %d degrees", a); break;
        case BLK_GOTO_XY:    snprintf(buf, sizeof(buf), "go to x:%d y:%d", a, b); break;

        case BLK_EVENT_GREEN_FLAG: snprintf(buf, sizeof(buf), "when green flag clicked"); break;
        case BLK_EVENT_KEY_SPACE:  snprintf(buf, sizeof(buf), "when space key pressed"); break;
        case BLK_EVENT_RECV_MSG1:  snprintf(buf, sizeof(buf), "when I receive msg1"); break;

        case BLK_WAIT_MS:          snprintf(buf, sizeof(buf), "wait %d ms", a); break;
        case BLK_REPEAT_BEGIN:     snprintf(buf, sizeof(buf), "repeat %d", a); break;
        case BLK_REPEAT_END:       snprintf(buf, sizeof(buf), "end repeat"); break;
        case BLK_FOREVER_BEGIN:    snprintf(buf, sizeof(buf), "forever"); break;
        case BLK_FOREVER_END:      snprintf(buf, sizeof(buf), "end forever"); break;

        case BLK_IF_X_GT:          snprintf(buf, sizeof(buf), "if x > %d", a); break;
        case BLK_IF_X_LT:          snprintf(buf, sizeof(buf), "if x < %d", a); break;
        case BLK_IF_Y_GT:          snprintf(buf, sizeof(buf), "if y > %d", a); break;
        case BLK_IF_Y_LT:          snprintf(buf, sizeof(buf), "if y < %d", a); break;
        case BLK_IF_RANDOM_LT:     snprintf(buf, sizeof(buf), "if random < %d%%", a); break;
        case BLK_ELSE:             snprintf(buf, sizeof(buf), "else"); break;
        case BLK_ENDIF:            snprintf(buf, sizeof(buf), "end if"); break;

        default: snprintf(buf, sizeof(buf), "block"); break;
    }
    return buf;
}

static void clamp_into(SDL_Rect* r, SDL_Rect area) {
    if (r->x < area.x) r->x = area.x;
    if (r->y < area.y) r->y = area.y;
    if (r->x + r->w > area.x + area.w) r->x = (area.x + area.w) - r->w;
    if (r->y + r->h > area.y + area.h) r->y = (area.y + area.h) - r->h;
}

static int hit_workspace_block(BlockEditor* be, int mx, int my) {
    for (int i = be->block_count - 1; i >= 0; --i) {
        if (pt_in_rect(mx, my, be->blocks[i].r)) return i;
    }
    return -1;
}

static void remove_block(BlockEditor* be, int idx) {
    if (idx < 0 || idx >= be->block_count) return;
    for (int i = idx; i < be->block_count - 1; ++i) be->blocks[i] = be->blocks[i + 1];
    be->block_count--;
    if (be->selected_index == idx) be->selected_index = -1;
    else if (be->selected_index > idx) be->selected_index--;
}

static void snap_under(BlockEditor* be, int idx) {
    const int SNAP_X = 18;
    const int SNAP_Y = 18;
    const int GAP = 8;

    SDL_Rect r = be->blocks[idx].r;

    int best = -1;
    int best_dy = 999999;

    for (int j = 0; j < be->block_count; ++j) {
        if (j == idx) continue;

        SDL_Rect o = be->blocks[j].r;
        int target_x = o.x;
        int target_y = o.y + o.h + GAP;

        int dx = (r.x > target_x) ? (r.x - target_x) : (target_x - r.x);
        int dy = (r.y > target_y) ? (r.y - target_y) : (target_y - r.y);

        if (dx <= SNAP_X && dy <= SNAP_Y) {
            if (dy < best_dy) { best_dy = dy; best = j; }
        }
    }

    if (best != -1) {
        SDL_Rect o = be->blocks[best].r;
        be->blocks[idx].r.x = o.x;
        be->blocks[idx].r.y = o.y + o.h + GAP;
        clamp_into(&be->blocks[idx].r, be->work_r);
    }
}

void block_editor_init(BlockEditor* be) {
    memset(be, 0, sizeof(*be));
    be->cat = CAT_MOTION;
    be->block_count = 0;
    be->selected_index = -1;
    be->dragging = 0;
    be->next_id = 1;
}

void block_editor_set_layout(BlockEditor* be, SDL_Rect cat_r, SDL_Rect palette_r, SDL_Rect work_r) {
    be->cat_r = cat_r;
    be->palette_r = palette_r;
    be->work_r = work_r;
}

int block_editor_block_count(const BlockEditor* be) { return be->block_count; }
const BlockInstance* block_editor_blocks(const BlockEditor* be) { return be->blocks; }

void block_editor_handle_event(BlockEditor* be, const SDL_Event* e) {
    if (!be || !e) return;

    if (e->type == SDL_KEYDOWN) {
        SDL_Keycode k = e->key.keysym.sym;
        if ((k == SDLK_BACKSPACE || k == SDLK_DELETE) && be->selected_index != -1) {
            remove_block(be, be->selected_index);
        }
        return;
    }

    int mx = 0, my = 0;
    if (e->type == SDL_MOUSEMOTION) {
        mx = e->motion.x; my = e->motion.y;
    } else if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) {
        mx = e->button.x; my = e->button.y;
    } else {
        return;
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        if (pt_in_rect(mx, my, be->cat_r)) {
            int item_h = 42;
            int rel_y = my - be->cat_r.y;
            int idx = rel_y / item_h;
            if (idx >= 0 && idx < (int)CAT_COUNT) be->cat = (BlockCategory)idx;
            return;
        }

        if (pt_in_rect(mx, my, be->palette_r)) {
            const int pad = 16;
            const int bh = 44;
            const int gap = 10;

            BlockType type = BLK_COUNT;
            int a = 0, b = 0;

            SDL_Rect m0{ be->palette_r.x + pad, be->palette_r.y + pad, be->palette_r.w - pad*2, bh };
            SDL_Rect m1{ m0.x, m0.y + bh + gap, m0.w, bh };
            SDL_Rect m2{ m0.x, m1.y + bh + gap, m0.w, bh };

            SDL_Rect e0{ be->palette_r.x + pad, be->palette_r.y + pad, be->palette_r.w - pad*2, bh };

            SDL_Rect c0{ be->palette_r.x + pad, be->palette_r.y + pad, be->palette_r.w - pad*2, bh };
            SDL_Rect c1{ c0.x, c0.y + bh + gap, c0.w, bh };
            SDL_Rect c2{ c1.x, c1.y + bh + gap, c0.w, bh };
            SDL_Rect c3{ c2.x, c2.y + bh + gap, c0.w, bh };
            SDL_Rect c4{ c3.x, c3.y + bh + gap, c0.w, bh };
            SDL_Rect c5{ c4.x, c4.y + bh + gap, c0.w, bh };
            SDL_Rect c6{ c5.x, c5.y + bh + gap, c0.w, bh };
            SDL_Rect c7{ c6.x, c6.y + bh + gap, c0.w, bh };

            if (be->cat == CAT_MOTION) {
                if (pt_in_rect(mx, my, m0)) { type = BLK_MOVE_STEPS; a = 10; }
                else if (pt_in_rect(mx, my, m1)) { type = BLK_TURN_DEG; a = 15; }
                else if (pt_in_rect(mx, my, m2)) { type = BLK_GOTO_XY; a = 0; b = 0; }
            } else if (be->cat == CAT_EVENTS) {
                SDL_Rect e0{ be->palette_r.x + pad, be->palette_r.y + pad,                be->palette_r.w - pad*2, bh };
                SDL_Rect e1{ e0.x,                 e0.y + (bh+gap)*1,                     e0.w,                   bh };
                SDL_Rect e2{ e0.x,                 e0.y + (bh+gap)*2,                     e0.w,                   bh };

                if (pt_in_rect(mx, my, e0)) { type = BLK_EVENT_GREEN_FLAG; }
                else if (pt_in_rect(mx, my, e1)) { type = BLK_EVENT_KEY_SPACE; }
                else if (pt_in_rect(mx, my, e2)) { type = BLK_EVENT_RECV_MSG1; }
            } else if (be->cat == CAT_CONTROL) {
                if (pt_in_rect(mx, my, c0)) { type = BLK_WAIT_MS; a = 100; }
                else if (pt_in_rect(mx, my, c1)) { type = BLK_REPEAT_BEGIN; a = 10; }
                else if (pt_in_rect(mx, my, c2)) { type = BLK_REPEAT_END; }
                else if (pt_in_rect(mx, my, c3)) { type = BLK_FOREVER_BEGIN; }
                else if (pt_in_rect(mx, my, c4)) { type = BLK_FOREVER_END; }
                else if (pt_in_rect(mx, my, c5)) { type = BLK_IF_X_GT; a = 0; }
                else if (pt_in_rect(mx, my, c6)) { type = BLK_ELSE; }
                else if (pt_in_rect(mx, my, c7)) { type = BLK_ENDIF; }
            }

            if (type != BLK_COUNT && be->block_count < MAX_WORKSPACE_BLOCKS) {
                int idx = be->block_count++;
                be->blocks[idx].id = be->next_id++;
                be->blocks[idx].type = type;
                be->blocks[idx].a = a;
                be->blocks[idx].b = b;
                be->blocks[idx].r = SDL_Rect{ mx - 120, my - 20, 240, 40 };
                clamp_into(&be->blocks[idx].r, be->work_r);

                be->selected_index = idx;
                be->dragging = 1;
                be->drag_from_palette = 1;
                be->drag_index = idx;
                be->drag_off_x = mx - be->blocks[idx].r.x;
                be->drag_off_y = my - be->blocks[idx].r.y;
                return;
            }
        }

        if (pt_in_rect(mx, my, be->work_r)) {
            int hit = hit_workspace_block(be, mx, my);
            if (hit != -1) {
                be->selected_index = hit;
                be->dragging = 1;
                be->drag_from_palette = 0;
                be->drag_index = hit;
                be->drag_off_x = mx - be->blocks[hit].r.x;
                be->drag_off_y = my - be->blocks[hit].r.y;
            } else {
                be->selected_index = -1;
            }
            return;
        }

        be->selected_index = -1;
        return;
    }

    if (e->type == SDL_MOUSEMOTION && be->dragging) {
        int idx = be->drag_index;
        if (idx >= 0 && idx < be->block_count) {
            be->blocks[idx].r.x = mx - be->drag_off_x;
            be->blocks[idx].r.y = my - be->drag_off_y;
        }
        return;
    }

    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        if (!be->dragging) return;

        int idx = be->drag_index;
        be->dragging = 0;

        if (idx < 0 || idx >= be->block_count) return;

        if (!pt_in_rect(mx, my, be->work_r) && be->drag_from_palette) {
            remove_block(be, idx);
            be->drag_from_palette = 0;
            return;
        }

        clamp_into(&be->blocks[idx].r, be->work_r);
        snap_under(be, idx);

        be->drag_from_palette = 0;
        return;
    }
}

void block_editor_render(BlockEditor* be, SDL_Renderer* ren, TTF_Font* font, uint64_t highlight_id) {
    if (!be || !ren) return;

    fill_rect(ren, be->cat_r,     SDL_Color{ 245,245,245,255 });
    fill_rect(ren, be->palette_r, SDL_Color{ 250,250,250,255 });
    fill_rect(ren, be->work_r,    SDL_Color{ 252,252,252,255 });

    SDL_SetRenderDrawColor(ren, 230, 230, 230, 255);
    for (int y = be->work_r.y + 10; y < be->work_r.y + be->work_r.h; y += 22) {
        for (int x = be->work_r.x + 10; x < be->work_r.x + be->work_r.w; x += 22) {
            SDL_RenderDrawPoint(ren, x, y);
        }
    }

    draw_rect(ren, be->cat_r, SDL_Color{210,210,210,255});
    draw_rect(ren, be->palette_r, SDL_Color{210,210,210,255});
    draw_rect(ren, be->work_r, SDL_Color{210,210,210,255});

    int item_h = 42;
    for (int i = 0; i < (int)CAT_COUNT; ++i) {
        SDL_Rect row{ be->cat_r.x, be->cat_r.y + i*item_h, be->cat_r.w, item_h };
        if ((int)be->cat == i) fill_rect(ren, row, SDL_Color{ 235,235,235,255 });

        SDL_Color cc = cat_color((BlockCategory)i);
        SDL_Rect sw{ row.x + 10, row.y + 12, 16, 16 };
        fill_rect(ren, sw, cc);
        draw_rect(ren, sw, SDL_Color{0,0,0,80});
        draw_text(ren, font, row.x + 34, row.y + 10, cat_name((BlockCategory)i), SDL_Color{60,60,60,255});
    }

// palette blocks
{
    const int pad = 16;
    const int bh  = 44;
    const int gap = 10;

    SDL_Color base   = cat_color(be->cat);
    SDL_Color border = SDL_Color{ 40, 70, 140, 255 };
    SDL_Color text   = SDL_Color{ 255,255,255,255 };

    // Motion
    if (be->cat == CAT_MOTION) {
        SDL_Rect b0{ be->palette_r.x + pad, be->palette_r.y + pad, be->palette_r.w - pad*2, bh };
        SDL_Rect b1{ b0.x, b0.y + bh + gap, b0.w, bh };
        SDL_Rect b2{ b0.x, b1.y + bh + gap, b0.w, bh };

        fill_rect(ren, b0, base); draw_rect(ren, b0, border);
        fill_rect(ren, b1, base); draw_rect(ren, b1, border);
        fill_rect(ren, b2, base); draw_rect(ren, b2, border);

        draw_text(ren, font, b0.x + 12, b0.y + 10, "move 10 steps", text);
        draw_text(ren, font, b1.x + 12, b1.y + 10, "turn 15 degrees", text);
        draw_text(ren, font, b2.x + 12, b2.y + 10, "go to x:0 y:0", text);
    }

    // Events
    else if (be->cat == CAT_EVENTS) {
        SDL_Color ev_border = SDL_Color{ 160, 120, 20, 255 };

        SDL_Rect b0{ be->palette_r.x + pad, be->palette_r.y + pad,                be->palette_r.w - pad*2, bh };
        SDL_Rect b1{ b0.x,                 b0.y + (bh+gap)*1,                     b0.w,                   bh };
        SDL_Rect b2{ b0.x,                 b0.y + (bh+gap)*2,                     b0.w,                   bh };

        fill_rect(ren, b0, base); draw_rect(ren, b0, ev_border);
        fill_rect(ren, b1, base); draw_rect(ren, b1, ev_border);
        fill_rect(ren, b2, base); draw_rect(ren, b2, ev_border);

        draw_text(ren, font, b0.x + 12, b0.y + 10, "when green flag clicked", SDL_Color{40,40,40,255});
        draw_text(ren, font, b1.x + 12, b1.y + 10, "when space key pressed",  SDL_Color{40,40,40,255});
        draw_text(ren, font, b2.x + 12, b2.y + 10, "when I receive msg1",     SDL_Color{40,40,40,255});
    }

    // Control
    else if (be->cat == CAT_CONTROL) {
        SDL_Color ctrl_border = SDL_Color{ 170, 90, 20, 255 };

        SDL_Rect b0{ be->palette_r.x + pad, be->palette_r.y + pad,                be->palette_r.w - pad*2, bh };
        SDL_Rect b1{ b0.x, b0.y + (bh+gap)*1, b0.w, bh };
        SDL_Rect b2{ b0.x, b0.y + (bh+gap)*2, b0.w, bh };
        SDL_Rect b3{ b0.x, b0.y + (bh+gap)*3, b0.w, bh };
        SDL_Rect b4{ b0.x, b0.y + (bh+gap)*4, b0.w, bh };
        SDL_Rect b5{ b0.x, b0.y + (bh+gap)*5, b0.w, bh };
        SDL_Rect b6{ b0.x, b0.y + (bh+gap)*6, b0.w, bh };
        SDL_Rect b7{ b0.x, b0.y + (bh+gap)*7, b0.w, bh };

        fill_rect(ren, b0, base); draw_rect(ren, b0, ctrl_border);
        fill_rect(ren, b1, base); draw_rect(ren, b1, ctrl_border);
        fill_rect(ren, b2, base); draw_rect(ren, b2, ctrl_border);
        fill_rect(ren, b3, base); draw_rect(ren, b3, ctrl_border);
        fill_rect(ren, b4, base); draw_rect(ren, b4, ctrl_border);
        fill_rect(ren, b5, base); draw_rect(ren, b5, ctrl_border);
        fill_rect(ren, b6, base); draw_rect(ren, b6, ctrl_border);
        fill_rect(ren, b7, base); draw_rect(ren, b7, ctrl_border);

        draw_text(ren, font, b0.x + 12, b0.y + 10, "wait 100 ms", text);
        draw_text(ren, font, b1.x + 12, b1.y + 10, "repeat 10", text);
        draw_text(ren, font, b2.x + 12, b2.y + 10, "end", text);
        draw_text(ren, font, b3.x + 12, b3.y + 10, "forever", text);
        draw_text(ren, font, b4.x + 12, b4.y + 10, "end", text);
        draw_text(ren, font, b5.x + 12, b5.y + 10, "if x > 0", text);
        draw_text(ren, font, b6.x + 12, b6.y + 10, "else", text);
        draw_text(ren, font, b7.x + 12, b7.y + 10, "end", text);
    }
}

    for (int i = 0; i < be->block_count; ++i) {
        BlockInstance* bi = &be->blocks[i];

        BlockCategory c = type_category(bi->type);
        SDL_Color fill = cat_color(c);
        SDL_Color border = border_from_fill(fill);
        SDL_Color txt = text_for_fill(fill);

        fill_rect(ren, bi->r, fill);
        draw_rect(ren, bi->r, border);

        // draw highlight for currently executing block (runtime debugger)
        if (highlight_id != 0 && bi->id == highlight_id) {
            SDL_Rect h = bi->r;
            h.x -= 4; h.y -= 4; h.w += 8; h.h += 8;
            draw_rect(ren, h, SDL_Color{255, 215, 0, 255});   // gold-ish
            h.x -= 1; h.y -= 1; h.w += 2; h.h += 2;
            draw_rect(ren, h, SDL_Color{255, 215, 0, 255});
        }

        const char* label = block_label(bi->type, bi->a, bi->b);
        draw_text(ren, font, bi->r.x + 12, bi->r.y + 9, label, txt);

        if (be->selected_index == i) {
            SDL_Rect sel = bi->r;
            sel.x -= 2; sel.y -= 2; sel.w += 4; sel.h += 4;
            draw_rect(ren, sel, SDL_Color{ 170, 90, 255, 255 });
        }
    }
}