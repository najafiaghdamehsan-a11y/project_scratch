#include "ui/app.h"
#include "core/log.h"
#include "model/model_ops.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdio>
#include <cstring>
#include <cmath>

#ifndef PROJECT_ROOT
#define PROJECT_ROOT "."
#endif

static const char* FONT_PATH = PROJECT_ROOT "/assets/fonts/Arial.ttf";

static int pt_in_rect(int x, int y, const SDL_Rect& r) {
    return x >= r.x && x < (r.x + r.w) && y >= r.y && y < (r.y + r.h);
}

static void draw_rect(SDL_Renderer* ren, const SDL_Rect& r, uint8_t rr, uint8_t gg, uint8_t bb) {
    SDL_SetRenderDrawColor(ren, rr, gg, bb, 255);
    SDL_RenderFillRect(ren, &r);
}

static void draw_rect_outline(SDL_Renderer* ren, const SDL_Rect& r, uint8_t rr, uint8_t gg, uint8_t bb) {
    SDL_SetRenderDrawColor(ren, rr, gg, bb, 255);
    SDL_RenderDrawRect(ren, &r);
}

static void draw_text(SDL_Renderer* ren, TTF_Font* font, int x, int y, const char* text,
                      uint8_t rr, uint8_t gg, uint8_t bb) {
    if (!font || !text || !text[0]) return;

    SDL_Color col{rr, gg, bb, 255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, col);
    if (!surf) return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }

    SDL_Rect dst{x, y, surf->w, surf->h};
    SDL_FreeSurface(surf);

    SDL_RenderCopy(ren, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

// Simple filled circle (for category dots / flag / stop)
static void draw_filled_circle(SDL_Renderer* ren, int cx, int cy, int radius, uint8_t rr, uint8_t gg, uint8_t bb) {
    SDL_SetRenderDrawColor(ren, rr, gg, bb, 255);
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)std::sqrt((double)radius * radius - (double)dy * dy);
        SDL_RenderDrawLine(ren, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void update_window_title(SDL_Window* win, const Project* p) {
    const char* name = "None";
    if (p && p->sprite_count > 0) {
        int a = p->active_sprite_index;
        if (a < 0) a = 0;
        if (a >= p->sprite_count) a = 0;
        name = p->sprites[a].name;
    }
    char title[256];
    std::snprintf(title, sizeof(title),
                  "project_scratch | Active: %s%s",
                  name,
                  project_is_dirty(p) ? " *" : "");
    SDL_SetWindowTitle(win, title);
}

enum Category {
    CAT_MOTION = 0,
    CAT_LOOKS,
    CAT_SOUND,
    CAT_EVENTS,
    CAT_CONTROL,
    CAT_SENSING,
    CAT_OPERATORS,
    CAT_VARIABLES,
    CAT_COUNT
};

static const char* CAT_NAMES[CAT_COUNT] = {
    "Motion", "Looks", "Sound", "Events", "Control", "Sensing", "Operators", "Variables"
};

// Scratch-ish colors per category (simple)
static void cat_color(int cat, uint8_t& r, uint8_t& g, uint8_t& b) {
    switch (cat) {
        case CAT_MOTION:    r = 60;  g = 120; b = 255; break;
        case CAT_LOOKS:     r = 160; g = 80;  b = 255; break;
        case CAT_SOUND:     r = 255; g = 90;  b = 140; break;
        case CAT_EVENTS:    r = 255; g = 190; b = 60;  break;
        case CAT_CONTROL:   r = 255; g = 140; b = 60;  break;
        case CAT_SENSING:   r = 70;  g = 200; b = 200; break;
        case CAT_OPERATORS: r = 70;  g = 200; b = 120; break;
        case CAT_VARIABLES: r = 255; g = 120; b = 60;  break;
        default:            r = 200; g = 200; b = 200; break;
    }
}

static void draw_workspace_grid(SDL_Renderer* ren, const SDL_Rect& r) {
    // Very light grid dots like Scratch
    SDL_SetRenderDrawColor(ren, 210, 210, 210, 255);
    for (int y = r.y + 10; y < r.y + r.h; y += 20) {
        for (int x = r.x + 10; x < r.x + r.w; x += 20) {
            SDL_RenderDrawPoint(ren, x, y);
        }
    }
}

static void draw_fake_block(SDL_Renderer* ren, TTF_Font* font, int x, int y, int w, int h,
                            uint8_t rr, uint8_t gg, uint8_t bb, const char* label) {
    SDL_Rect r{x, y, w, h};
    draw_rect(ren, r, rr, gg, bb);
    draw_rect_outline(ren, r, 30, 30, 30);
    draw_text(ren, font, x + 10, y + 8, label, 255, 255, 255);
}

int app_run(Project* project, Runtime* runtime) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        log_write(LogRecord{0,0,"SDL","Init failed", SDL_GetError(), LOG_ERROR});
        return 0;
    }

    int ttf_ok = 1;
    if (TTF_Init() != 0) {
        log_write(LogRecord{0,0,"TTF","Init failed", TTF_GetError(), LOG_ERROR});
        ttf_ok = 0;
    }

    const int WIN_W = 1200;
    const int WIN_H = 700;

    SDL_Window* win = SDL_CreateWindow("project_scratch",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    if (!win) {
        log_write(LogRecord{0,0,"SDL","CreateWindow failed", SDL_GetError(), LOG_ERROR});
        if (ttf_ok) TTF_Quit();
        SDL_Quit();
        return 0;
    }

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        log_write(LogRecord{0,0,"SDL","CreateRenderer failed", SDL_GetError(), LOG_ERROR});
        SDL_DestroyWindow(win);
        if (ttf_ok) TTF_Quit();
        SDL_Quit();
        return 0;
    }

    TTF_Font* font = nullptr;
    if (ttf_ok) {
        font = TTF_OpenFont(FONT_PATH, 16);
        if (!font) log_write(LogRecord{0,0,"TTF","OpenFont failed", TTF_GetError(), LOG_ERROR});
    }

    update_window_title(win, project);

    // Layout (Scratch-like)
    const int TOPBAR_H = 44;
    const int LEFT_W   = 270;
    const int RIGHT_W  = 360;

    SDL_Rect topbar   {0, 0, WIN_W, TOPBAR_H};
    SDL_Rect left     {0, TOPBAR_H, LEFT_W, WIN_H - TOPBAR_H};
    SDL_Rect right    {WIN_W - RIGHT_W, TOPBAR_H, RIGHT_W, WIN_H - TOPBAR_H};
    SDL_Rect workspace{LEFT_W, TOPBAR_H, WIN_W - LEFT_W - RIGHT_W, WIN_H - TOPBAR_H};

    // Left sub-layout
    const int CAT_COL_W = 70;
    SDL_Rect catCol     {left.x, left.y, CAT_COL_W, left.h};
    SDL_Rect palette    {left.x + CAT_COL_W, left.y, left.w - CAT_COL_W, left.h};

    // Right sub-layout
    SDL_Rect stage      {right.x + 10, right.y + 10, right.w - 20, 340};
    SDL_Rect spritePane {right.x + 10, stage.y + stage.h + 10, right.w - 20,
                         right.h - stage.h - 20};

    // Green flag / stop
    SDL_Rect btnFlag{stage.x + 10, stage.y - 28, 22, 22};
    SDL_Rect btnStop{stage.x + 40, stage.y - 28, 22, 22};

    // Sprite pane buttons (your old ones, moved)
    SDL_Rect btnAdd { spritePane.x + 10, spritePane.y + 10, (spritePane.w - 40) / 3, 32 };
    SDL_Rect btnDel { spritePane.x + 20 + btnAdd.w, spritePane.y + 10, (spritePane.w - 40) / 3, 32 };
    SDL_Rect btnVis { spritePane.x + 30 + btnAdd.w + btnDel.w, spritePane.y + 10, (spritePane.w - 40) / 3, 32 };

    const int listTop = spritePane.y + 52;
    const int itemH = 34;

    int selected_cat = CAT_MOTION;

    int running = 1;
    while (running) {
        SDL_Event e;
        int titleNeedsUpdate = 0;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = e.button.x;
                int my = e.button.y;

                // Top stage controls
                if (pt_in_rect(mx, my, btnFlag)) runtime_green_flag(runtime);
                if (pt_in_rect(mx, my, btnStop)) runtime_stop_all(runtime);

                // Category clicks
                if (pt_in_rect(mx, my, catCol)) {
                    int item = (my - catCol.y - 10) / 46;
                    if (item >= 0 && item < CAT_COUNT) {
                        selected_cat = item;
                    }
                }

                // Sprite panel clicks
                if (pt_in_rect(mx, my, spritePane)) {
                    if (pt_in_rect(mx, my, btnAdd)) {
                        char nm[64];
                        std::snprintf(nm, sizeof(nm), "Sprite%d", project->sprite_count + 1);
                        int idx = project_add_sprite(project, nm);
                        if (idx >= 0) {
                            project_set_active_sprite(project, idx);
                            titleNeedsUpdate = 1;
                        }
                    } else if (pt_in_rect(mx, my, btnDel)) {
                        if (project->sprite_count > 0) {
                            project_delete_sprite(project, project->active_sprite_index);
                            titleNeedsUpdate = 1;
                        }
                    } else if (pt_in_rect(mx, my, btnVis)) {
                        if (project->sprite_count > 0) {
                            int a = project->active_sprite_index;
                            if (a < 0) a = 0;
                            if (a >= project->sprite_count) a = 0;
                            sprite_set_visible(project, a, !project->sprites[a].visible);
                            titleNeedsUpdate = 1;
                        }
                    } else {
                        int idx = (my - listTop) / itemH;
                        if (idx >= 0 && idx < project->sprite_count) {
                            if (project_set_active_sprite(project, idx)) {
                                titleNeedsUpdate = 1;
                            }
                        }
                    }
                }
            }

            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode key = e.key.keysym.sym;

                // Engine debug keys (still useful)
                if (key == SDLK_p) runtime_set_paused(runtime, !runtime->paused);
                if (key == SDLK_s) runtime_set_step_mode(runtime, !runtime->step_mode);
                if (key == SDLK_n) runtime_request_step(runtime);
                if (key == SDLK_g) runtime_green_flag(runtime);
                if (key == SDLK_x) runtime_stop_all(runtime);

                // Sprite edit keys
                if (project->sprite_count > 0) {
                    int a = project->active_sprite_index;
                    if (a < 0) a = 0;
                    if (a >= project->sprite_count) a = 0;

                    double x = project->sprites[a].x;
                    double y = project->sprites[a].y;

                    const double step = 5.0;
                    if (key == SDLK_LEFT)  { x -= step; sprite_set_pos(project, a, x, y); titleNeedsUpdate = 1; }
                    if (key == SDLK_RIGHT) { x += step; sprite_set_pos(project, a, x, y); titleNeedsUpdate = 1; }
                    if (key == SDLK_UP)    { y += step; sprite_set_pos(project, a, x, y); titleNeedsUpdate = 1; }
                    if (key == SDLK_DOWN)  { y -= step; sprite_set_pos(project, a, x, y); titleNeedsUpdate = 1; }

                    if (key == SDLK_LEFTBRACKET)  { sprite_set_dir(project, a, project->sprites[a].dir - 10.0); titleNeedsUpdate = 1; }
                    if (key == SDLK_RIGHTBRACKET) { sprite_set_dir(project, a, project->sprites[a].dir + 10.0); titleNeedsUpdate = 1; }
                    if (key == SDLK_MINUS)        { sprite_set_size(project, a, project->sprites[a].size - 5.0); titleNeedsUpdate = 1; }
                    if (key == SDLK_EQUALS)       { sprite_set_size(project, a, project->sprites[a].size + 5.0); titleNeedsUpdate = 1; }
                }
            }
        }

        if (titleNeedsUpdate) update_window_title(win, project);

        runtime_tick(runtime, project);

        // ======= DRAW =======
        SDL_SetRenderDrawColor(ren, 235, 235, 240, 255);
        SDL_RenderClear(ren);

        // Topbar (Scratch purple-ish)
        draw_rect(ren, topbar, 140, 90, 210);
        draw_text(ren, font, 14, 12, "project_scratch", 255, 255, 255);
        draw_text(ren, font, 160, 12, "Code", 255, 255, 255);
        draw_text(ren, font, 220, 12, "Costumes", 230, 230, 230);
        draw_text(ren, font, 320, 12, "Sounds", 230, 230, 230);

        // Left panels background
        draw_rect(ren, left, 245, 245, 248);
        draw_rect_outline(ren, left, 200, 200, 205);

        // Category column
        draw_rect(ren, catCol, 240, 240, 244);
        draw_rect_outline(ren, catCol, 210, 210, 215);

        for (int i = 0; i < CAT_COUNT; i++) {
            SDL_Rect item{catCol.x + 6, catCol.y + 10 + i * 46, catCol.w - 12, 40};

            if (i == selected_cat) {
                draw_rect(ren, item, 255, 255, 255);
                draw_rect_outline(ren, item, 120, 120, 130);
            } else {
                draw_rect(ren, item, 240, 240, 244);
            }

            uint8_t cr, cg, cb;
            cat_color(i, cr, cg, cb);
            draw_filled_circle(ren, item.x + 16, item.y + 20, 8, cr, cg, cb);
            draw_text(ren, font, item.x + 30, item.y + 11, CAT_NAMES[i], 40, 40, 40);
        }

        // Palette panel
        draw_rect(ren, palette, 250, 250, 252);
        draw_rect_outline(ren, palette, 210, 210, 215);

        // Title
        char palTitle[64];
        std::snprintf(palTitle, sizeof(palTitle), "%s", CAT_NAMES[selected_cat]);
        draw_text(ren, font, palette.x + 12, palette.y + 12, palTitle, 40, 40, 40);

        // Fake block list (just to look Scratch-like)
        uint8_t br, bg, bb;
        cat_color(selected_cat, br, bg, bb);
        int bx = palette.x + 12;
        int by = palette.y + 40;

        if (selected_cat == CAT_MOTION) {
            draw_fake_block(ren, font, bx, by,  palette.w - 24, 34, br, bg, bb, "move 10 steps"); by += 44;
            draw_fake_block(ren, font, bx, by,  palette.w - 24, 34, br, bg, bb, "turn 15 degrees"); by += 44;
            draw_fake_block(ren, font, bx, by,  palette.w - 24, 34, br, bg, bb, "go to x: 0 y: 0"); by += 44;
        } else if (selected_cat == CAT_CONTROL) {
            draw_fake_block(ren, font, bx, by,  palette.w - 24, 34, br, bg, bb, "wait 1 seconds"); by += 44;
            draw_fake_block(ren, font, bx, by,  palette.w - 24, 34, br, bg, bb, "repeat 10"); by += 44;
            draw_fake_block(ren, font, bx, by,  palette.w - 24, 34, br, bg, bb, "if < > then"); by += 44;
        } else {
            draw_fake_block(ren, font, bx, by,  palette.w - 24, 34, br, bg, bb, "(blocks placeholder)"); by += 44;
        }

        // Workspace (scripts area)
        draw_rect(ren, workspace, 245, 245, 245);
        draw_rect_outline(ren, workspace, 200, 200, 205);
        draw_workspace_grid(ren, workspace);
        draw_text(ren, font, workspace.x + 14, workspace.y + 12, "Scripts Workspace (next: drag & snap)", 60, 60, 60);

        // Right background
        draw_rect(ren, right, 240, 240, 244);
        draw_rect_outline(ren, right, 200, 200, 205);

        // Stage (white like Scratch)
        draw_rect(ren, stage, 255, 255, 255);
        draw_rect_outline(ren, stage, 180, 180, 185);

        // Green flag / stop
        draw_filled_circle(ren, btnFlag.x + 11, btnFlag.y + 11, 10, 60, 200, 90);
        draw_filled_circle(ren, btnStop.x + 11, btnStop.y + 11, 10, 230, 90, 90);

        // Draw sprites inside stage
        int stage_cx = stage.x + stage.w / 2;
        int stage_cy = stage.y + stage.h / 2;

        for (int i = 0; i < project->sprite_count; i++) {
            Sprite* s = &project->sprites[i];
            if (!s->visible) continue;

            int cx = stage_cx + (int)s->x;
            int cy = stage_cy - (int)s->y;

            double sc = s->size / 100.0;
            if (sc < 0.2) sc = 0.2;
            if (sc > 3.0) sc = 3.0;

            int half = (int)(18 * sc);
            SDL_Rect r{cx - half, cy - half, half * 2, half * 2};

            if (i == project->active_sprite_index) {
                draw_rect(ren, r, 255, 255, 255);
                SDL_Rect outline{r.x - 2, r.y - 2, r.w + 4, r.h + 4};
                draw_rect_outline(ren, outline, 150, 90, 210);
            } else {
                draw_rect(ren, r, 200, 200, 200);
            }
            draw_rect_outline(ren, r, 120, 120, 120);
        }

        // Sprite pane
        draw_rect(ren, spritePane, 235, 235, 240);
        draw_rect_outline(ren, spritePane, 200, 200, 205);

        // Buttons
        draw_rect(ren, btnAdd,  60, 170, 80);
        draw_rect(ren, btnDel,  210, 80, 80);
        draw_rect(ren, btnVis,  80, 80, 200);
        draw_text(ren, font, btnAdd.x + 12, btnAdd.y + 7, "Add", 255, 255, 255);
        draw_text(ren, font, btnDel.x + 12, btnDel.y + 7, "Del", 255, 255, 255);
        draw_text(ren, font, btnVis.x + 12, btnVis.y + 7, "Vis", 255, 255, 255);

        // Sprite list
        draw_text(ren, font, spritePane.x + 10, spritePane.y + 46, "Sprites", 60, 60, 60);

        for (int i = 0; i < project->sprite_count; i++) {
            SDL_Rect item{spritePane.x + 10, listTop + i * itemH, spritePane.w - 20, itemH - 4};

            if (i == project->active_sprite_index) {
                draw_rect(ren, item, 255, 255, 255);
                draw_rect_outline(ren, item, 150, 90, 210);
            } else {
                draw_rect(ren, item, 245, 245, 248);
                draw_rect_outline(ren, item, 210, 210, 215);
            }

            SDL_Rect dot{item.x + 6, item.y + 7, 14, 14};
            draw_rect(ren, dot, project->sprites[i].visible ? 200 : 120,
                      project->sprites[i].visible ? 200 : 120,
                      project->sprites[i].visible ? 200 : 120);

            draw_text(ren, font, item.x + 28, item.y + 5, project->sprites[i].name, 40, 40, 40);
        }

        // Active stats
        int statsY = listTop + project->sprite_count * itemH + 8;
        draw_text(ren, font, spritePane.x + 10, statsY, "Active", 60, 60, 60);
        statsY += 18;

        if (project->sprite_count > 0) {
            int a = project->active_sprite_index;
            if (a < 0) a = 0;
            if (a >= project->sprite_count) a = 0;

            char buf[128];
            std::snprintf(buf, sizeof(buf), "X: %.0f  Y: %.0f", project->sprites[a].x, project->sprites[a].y);
            draw_text(ren, font, spritePane.x + 10, statsY, buf, 40, 40, 40);
            statsY += 18;

            std::snprintf(buf, sizeof(buf), "Dir: %.0f  Size: %.0f", project->sprites[a].dir, project->sprites[a].size);
            draw_text(ren, font, spritePane.x + 10, statsY, buf, 40, 40, 40);
            statsY += 18;
        }

        SDL_RenderPresent(ren);
    }

    if (font) TTF_CloseFont(font);
    if (ttf_ok) TTF_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
}