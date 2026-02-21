#include "ui/block_editor.h"
#include <string.h> // memset
#include <stdio.h>  // snprintf

typedef struct BlockDef {
    int category;      // BE_Category
    int opcode;        // BE_Opcode
    const char* label; // e.g. "move %d steps"
    int a_default;
    int b_default;
} BlockDef;

// Minimal starter palette (Motion)
static const BlockDef kPaletteDefs[] = {
        {BE_CAT_MOTION, BE_OP_MOVE_STEPS, "move %d steps", 10, 0},
        {BE_CAT_MOTION, BE_OP_TURN_DEG,   "turn %d degrees", 15, 0},
        {BE_CAT_MOTION, BE_OP_GOTO_XY,    "go to x:%d y:%d", 0, 0},
};
static const int kPaletteDefCount = (int)(sizeof(kPaletteDefs) / sizeof(kPaletteDefs[0]));

static SDL_Color cat_color(int cat) {
    // Scratch-ish colors (simple)
    switch (cat) {
        case BE_CAT_MOTION:    return SDL_Color{  74, 134, 232, 255};
        case BE_CAT_LOOKS:     return SDL_Color{ 153, 102, 255, 255};
        case BE_CAT_SOUND:     return SDL_Color{ 207,  74, 217, 255};
        case BE_CAT_EVENTS:    return SDL_Color{ 255, 191,   0, 255};
        case BE_CAT_CONTROL:   return SDL_Color{ 255, 140,   0, 255};
        case BE_CAT_SENSING:   return SDL_Color{  64, 196, 196, 255};
        case BE_CAT_OPERATORS: return SDL_Color{  64, 200,  64, 255};
        case BE_CAT_VARIABLES: return SDL_Color{ 255, 120,  64, 255};
        default:               return SDL_Color{ 180, 180, 180, 255};
    }
}

static void draw_filled_rect(SDL_Renderer* ren, SDL_Rect r, SDL_Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(ren, &r);
}

static void draw_rect(SDL_Renderer* ren, SDL_Rect r, SDL_Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    SDL_RenderDrawRect(ren, &r);
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

static int pt_in_rect(int x, int y, const SDL_Rect* r) {
    return x >= r->x && y >= r->y && x < (r->x + r->w) && y < (r->y + r->h);
}

static int hit_workspace_block(const BlockEditor* be, int mx, int my) {
    // topmost = last drawn = last in array
    for (int i = be->block_count - 1; i >= 0; --i) {
        SDL_Rect r{be->blocks[i].x, be->blocks[i].y, be->blocks[i].w, be->blocks[i].h};
        if (pt_in_rect(mx, my, &r)) return i;
    }
    return -1;
}

static int palette_visible_index(const BlockEditor* be, int mx, int my) {
    if (!pt_in_rect(mx, my, &be->rect_palette)) return -1;

    const int item_h = 44;
    const int pad = 12;
    int y0 = be->rect_palette.y + pad;

    int local_y = my - y0 + be->palette_scroll_y;
    if (local_y < 0) return -1;

    // Only show blocks of selected category; index in "filtered list"
    int filtered = 0;
    for (int i = 0; i < kPaletteDefCount; i++) {
        if (kPaletteDefs[i].category != be->selected_category) continue;
        int top = filtered * item_h;
        SDL_Rect rr{ be->rect_palette.x + pad, y0 + top - be->palette_scroll_y, be->rect_palette.w - 2*pad, 36 };
        if (pt_in_rect(mx, my, &rr)) return i; // return real def index
        filtered++;
    }
    return -1;
}

static const char* category_name(int c) {
    static const char* names[BE_CAT_COUNT] = {
            "Motion","Looks","Sound","Events","Control","Sensing","Operators","Variables"
    };
    if (c < 0 || c >= BE_CAT_COUNT) return "Unknown";
    return names[c];
}

void block_editor_init(BlockEditor* be) {
    memset(be, 0, sizeof(*be));
    be->selected_category = BE_CAT_MOTION;
    be->palette_scroll_y = 0;
    be->block_count = 0;
    be->next_id = 1;
    be->dragging_index = -1;
    be->hover_palette_index = -1;
    be->hover_block_index = -1;
}

void block_editor_set_layout(BlockEditor* be, SDL_Rect cat, SDL_Rect palette, SDL_Rect workspace) {
    be->rect_cat = cat;
    be->rect_palette = palette;
    be->rect_workspace = workspace;
}

static void spawn_block_from_def(BlockEditor* be, const BlockDef* d, int mx, int my) {
    if (be->block_count >= BE_MAX_BLOCKS) return;

    BE_BlockInstance* bi = &be->blocks[be->block_count++];
    bi->id = be->next_id++;
    bi->opcode = d->opcode;
    bi->a = d->a_default;
    bi->b = d->b_default;
    bi->x = mx;
    bi->y = my;
    bi->w = 190;
    bi->h = 36;
    bi->category = d->category;

    be->dragging_index = be->block_count - 1;
    be->drag_off_x = bi->w / 2;
    be->drag_off_y = bi->h / 2;
}

void block_editor_handle_event(BlockEditor* be, const SDL_Event* e) {
    if (e->type == SDL_MOUSEMOTION) {
        int mx = e->motion.x, my = e->motion.y;

        be->hover_block_index = hit_workspace_block(be, mx, my);
        be->hover_palette_index = palette_visible_index(be, mx, my);

        if (be->dragging_index != -1) {
            BE_BlockInstance* b = &be->blocks[be->dragging_index];
            b->x = mx - be->drag_off_x;
            b->y = my - be->drag_off_y;
        }
        return;
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;

        // 1) click category list
        if (pt_in_rect(mx, my, &be->rect_cat)) {
            const int item_h = 44;
            int idx = (my - be->rect_cat.y) / item_h;
            if (idx >= 0 && idx < BE_CAT_COUNT) {
                be->selected_category = idx;
            }
            return;
        }

        // 2) click palette block => spawn new block into workspace and start dragging
        int def_i = palette_visible_index(be, mx, my);
        if (def_i != -1) {
            // Spawn at mouse position but clamp into workspace a bit
            int sx = mx;
            int sy = my;
            if (sx < be->rect_workspace.x) sx = be->rect_workspace.x + 10;
            if (sy < be->rect_workspace.y) sy = be->rect_workspace.y + 10;
            spawn_block_from_def(be, &kPaletteDefs[def_i], sx, sy);
            return;
        }

        // 3) click existing workspace block => start dragging it (bring to front)
        if (pt_in_rect(mx, my, &be->rect_workspace)) {
            int hit = hit_workspace_block(be, mx, my);
            if (hit != -1) {
                // bring to front
                BE_BlockInstance temp = be->blocks[hit];
                for (int i = hit; i < be->block_count - 1; i++) be->blocks[i] = be->blocks[i + 1];
                be->blocks[be->block_count - 1] = temp;
                be->dragging_index = be->block_count - 1;

                BE_BlockInstance* b = &be->blocks[be->dragging_index];
                be->drag_off_x = mx - b->x;
                be->drag_off_y = my - b->y;
            }
            return;
        }
    }

    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        be->dragging_index = -1;
        return;
    }

    if (e->type == SDL_MOUSEWHEEL) {
        // optional: palette scroll (only when mouse is over palette)
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        if (pt_in_rect(mx, my, &be->rect_palette)) {
            be->palette_scroll_y -= e->wheel.y * 24;
            if (be->palette_scroll_y < 0) be->palette_scroll_y = 0;
        }
    }
}

static void draw_dots(SDL_Renderer* ren, SDL_Rect r) {
    SDL_SetRenderDrawColor(ren, 220, 220, 220, 255);
    for (int y = r.y + 8; y < r.y + r.h; y += 18) {
        for (int x = r.x + 8; x < r.x + r.w; x += 18) {
            SDL_RenderDrawPoint(ren, x, y);
        }
    }
}

void block_editor_draw(BlockEditor* be, SDL_Renderer* ren, TTF_Font* font) {
    // backgrounds
    draw_filled_rect(ren, be->rect_cat, SDL_Color{245,245,245,255});
    draw_filled_rect(ren, be->rect_palette, SDL_Color{250,250,250,255});
    draw_filled_rect(ren, be->rect_workspace, SDL_Color{255,255,255,255});
    draw_dots(ren, be->rect_workspace);

    // separators
    draw_rect(ren, be->rect_cat, SDL_Color{200,200,200,255});
    draw_rect(ren, be->rect_palette, SDL_Color{200,200,200,255});
    draw_rect(ren, be->rect_workspace, SDL_Color{200,200,200,255});

    // category list
    const int item_h = 44;
    for (int i = 0; i < BE_CAT_COUNT; i++) {
        SDL_Rect row{be->rect_cat.x, be->rect_cat.y + i*item_h, be->rect_cat.w, item_h};
        if (i == be->selected_category) {
            draw_filled_rect(ren, row, SDL_Color{230,230,230,255});
        }

        SDL_Color cc = cat_color(i);
        SDL_Rect dot{row.x + 10, row.y + 14, 14, 14};
        draw_filled_rect(ren, dot, cc);

        draw_text(ren, font, row.x + 30, row.y + 10, category_name(i), SDL_Color{40,40,40,255});
    }

    // palette blocks (filtered by category)
    const int pad = 12;
    int y = be->rect_palette.y + pad - be->palette_scroll_y;

    for (int i = 0; i < kPaletteDefCount; i++) {
        if (kPaletteDefs[i].category != be->selected_category) continue;

        SDL_Rect r{be->rect_palette.x + pad, y, be->rect_palette.w - 2*pad, 36};
        SDL_Color col = cat_color(kPaletteDefs[i].category);

        // hover highlight
        if (be->hover_palette_index == i) {
            draw_filled_rect(ren, r, SDL_Color{235,235,235,255});
        }

        draw_filled_rect(ren, r, col);
        draw_rect(ren, r, SDL_Color{20,20,20,60});

        char buf[128];
        if (kPaletteDefs[i].opcode == BE_OP_GOTO_XY) {
            snprintf(buf, sizeof(buf), kPaletteDefs[i].label, kPaletteDefs[i].a_default, kPaletteDefs[i].b_default);
        } else {
            snprintf(buf, sizeof(buf), kPaletteDefs[i].label, kPaletteDefs[i].a_default);
        }
        draw_text(ren, font, r.x + 10, r.y + 8, buf, SDL_Color{255,255,255,255});

        y += 44;
    }

    // workspace blocks
    for (int i = 0; i < be->block_count; i++) {
        BE_BlockInstance* b = &be->blocks[i];
        SDL_Rect r{b->x, b->y, b->w, b->h};
        SDL_Color col = cat_color(b->category);

        // clamp draw inside workspace (optional)
        // (we still allow dragging outside a bit — Scratch does)
        draw_filled_rect(ren, r, col);

        // outline (stronger if hovered)
        if (be->hover_block_index == i) draw_rect(ren, r, SDL_Color{0,0,0,255});
        else draw_rect(ren, r, SDL_Color{0,0,0,80});

        char buf[128];
        if (b->opcode == BE_OP_MOVE_STEPS) snprintf(buf, sizeof(buf), "move %d steps", b->a);
        else if (b->opcode == BE_OP_TURN_DEG) snprintf(buf, sizeof(buf), "turn %d degrees", b->a);
        else if (b->opcode == BE_OP_GOTO_XY) snprintf(buf, sizeof(buf), "go to x:%d y:%d", b->a, b->b);
        else snprintf(buf, sizeof(buf), "unknown");

        draw_text(ren, font, r.x + 10, r.y + 8, buf, SDL_Color{255,255,255,255});
    }
}