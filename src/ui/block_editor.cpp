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
    // close-ish to Scratch colors (simple)
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

static const char* block_label(BlockType t, int a, int b) {
    // NOTE: returns pointer to static buffer. OK for immediate draw in same call.
    static char buf[128];
    switch (t) {
        case BLK_MOVE_STEPS: snprintf(buf, sizeof(buf), "move %d steps", a); break;
        case BLK_TURN_DEG:   snprintf(buf, sizeof(buf), "turn %d degrees", a); break;
        case BLK_GOTO_XY:    snprintf(buf, sizeof(buf), "go to x:%d y:%d", a, b); break;
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

    // Delete selected block
    if (e->type == SDL_KEYDOWN) {
        SDL_Keycode k = e->key.keysym.sym;
        if ((k == SDLK_BACKSPACE || k == SDLK_DELETE) && be->selected_index != -1) {
            remove_block(be, be->selected_index);
        }
        return;
    }

    // Mouse coords
    int mx = 0, my = 0;
    if (e->type == SDL_MOUSEMOTION) {
        mx = e->motion.x; my = e->motion.y;
    } else if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) {
        mx = e->button.x; my = e->button.y;
    } else {
        return;
    }

    // Mouse down
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        // Category click
        if (pt_in_rect(mx, my, be->cat_r)) {
            int item_h = 42;
            int rel_y = my - be->cat_r.y;
            int idx = rel_y / item_h;
            if (idx >= 0 && idx < (int)CAT_COUNT) be->cat = (BlockCategory)idx;
            return;
        }

        // Palette blocks (only Motion implemented right now)
        if (pt_in_rect(mx, my, be->palette_r)) {
            // Define palette block rects
            const int pad = 16;
            const int bh = 44;
            const int gap = 10;
            SDL_Rect b0{ be->palette_r.x + pad, be->palette_r.y + pad, be->palette_r.w - pad*2, bh };
            SDL_Rect b1{ b0.x, b0.y + bh + gap, b0.w, bh };
            SDL_Rect b2{ b0.x, b1.y + bh + gap, b0.w, bh };

            BlockType type = BLK_COUNT;
            int a = 0, b = 0;

            if (be->cat == CAT_MOTION) {
                if (pt_in_rect(mx, my, b0)) { type = BLK_MOVE_STEPS; a = 10; }
                else if (pt_in_rect(mx, my, b1)) { type = BLK_TURN_DEG; a = 15; }
                else if (pt_in_rect(mx, my, b2)) { type = BLK_GOTO_XY; a = 0; b = 0; }
            }

            if (type != BLK_COUNT && be->block_count < MAX_WORKSPACE_BLOCKS) {
                // create new workspace block (starts under mouse)
                int idx = be->block_count++;
                be->blocks[idx].id = be->next_id++;
                be->blocks[idx].type = type;
                be->blocks[idx].a = a;
                be->blocks[idx].b = b;
                be->blocks[idx].r = SDL_Rect{ mx - 120, my - 22, 240, 44 };
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

        // Workspace block click
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
                be->selected_index = -1; // click empty = deselect
            }
            return;
        }

        // click elsewhere deselect
        be->selected_index = -1;
        return;
    }

    // Mouse move while dragging
    if (e->type == SDL_MOUSEMOTION && be->dragging) {
        int idx = be->drag_index;
        if (idx >= 0 && idx < be->block_count) {
            be->blocks[idx].r.x = mx - be->drag_off_x;
            be->blocks[idx].r.y = my - be->drag_off_y;
            // allow outside while dragging, but keep somewhat near workspace
        }
        return;
    }

    // Mouse up: drop
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        if (!be->dragging) return;

        int idx = be->drag_index;
        be->dragging = 0;

        if (idx < 0 || idx >= be->block_count) return;

        // If dropped outside workspace and it was a fresh palette block => cancel it
        if (!pt_in_rect(mx, my, be->work_r) && be->drag_from_palette) {
            remove_block(be, idx);
            be->drag_from_palette = 0;
            return;
        }

        // Clamp into workspace and snap
        clamp_into(&be->blocks[idx].r, be->work_r);
        snap_under(be, idx);

        be->drag_from_palette = 0;
        return;
    }
}

void block_editor_render(BlockEditor* be, SDL_Renderer* ren, TTF_Font* font) {
    if (!be || !ren) return;

    // backgrounds
    fill_rect(ren, be->cat_r,     SDL_Color{ 245,245,245,255 });
    fill_rect(ren, be->palette_r, SDL_Color{ 250,250,250,255 });
    fill_rect(ren, be->work_r,    SDL_Color{ 252,252,252,255 });

    // dotted grid in workspace
    SDL_SetRenderDrawColor(ren, 230, 230, 230, 255);
    for (int y = be->work_r.y + 10; y < be->work_r.y + be->work_r.h; y += 22) {
        for (int x = be->work_r.x + 10; x < be->work_r.x + be->work_r.w; x += 22) {
            SDL_RenderDrawPoint(ren, x, y);
        }
    }

    // separators
    draw_rect(ren, be->cat_r, SDL_Color{210,210,210,255});
    draw_rect(ren, be->palette_r, SDL_Color{210,210,210,255});
    draw_rect(ren, be->work_r, SDL_Color{210,210,210,255});

    // category list
    int item_h = 42;
    for (int i = 0; i < (int)CAT_COUNT; ++i) {
        SDL_Rect row{ be->cat_r.x, be->cat_r.y + i*item_h, be->cat_r.w, item_h };
        if ((int)be->cat == i) {
            fill_rect(ren, row, SDL_Color{ 235,235,235,255 });
        }
        SDL_Color cc = cat_color((BlockCategory)i);
        SDL_Rect sw{ row.x + 10, row.y + 12, 16, 16 };
        fill_rect(ren, sw, cc);
        draw_rect(ren, sw, SDL_Color{0,0,0,80});

        draw_text(ren, font, row.x + 34, row.y + 10, cat_name((BlockCategory)i), SDL_Color{60,60,60,255});
    }

    // palette blocks (only Motion right now)
    if (be->cat == CAT_MOTION) {
        const int pad = 16;
        const int bh = 44;
        const int gap = 10;

        SDL_Color base = cat_color(CAT_MOTION);
        SDL_Color border{ 40, 70, 140, 255 };
        SDL_Color text{ 255,255,255,255 };

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

    // workspace blocks
    for (int i = 0; i < be->block_count; ++i) {
        BlockInstance* bi = &be->blocks[i];
        // color by category (for now, Motion for all three types)
        SDL_Color fill = cat_color(CAT_MOTION);
        SDL_Color border{ 40, 70, 140, 255 };
        SDL_Color txt{ 255,255,255,255 };

        fill_rect(ren, bi->r, fill);
        draw_rect(ren, bi->r, border);

        const char* label = block_label(bi->type, bi->a, bi->b);
        draw_text(ren, font, bi->r.x + 12, bi->r.y + 10, label, txt);

        if (be->selected_index == i) {
            SDL_Rect sel = bi->r;
            sel.x -= 2; sel.y -= 2; sel.w += 4; sel.h += 4;
            draw_rect(ren, sel, SDL_Color{ 170, 90, 255, 255 });
        }
    }
}