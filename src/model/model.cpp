#include "model/model.h"
#include <cstdio>

void model_init(Project* p) {
    if (!p) return;

    // Scratch-like default: one sprite exists
    p->sprite_count = 1;
    p->active_sprite_index = 0;
    p->dirty = 0;

    // Sprite 0 defaults
    Sprite* s0 = &p->sprites[0];
    s0->id = 1;
    std::snprintf(s0->name, MAX_NAME, "Sprite1");
    s0->x = 0.0;
    s0->y = 0.0;
    s0->dir = 90.0;     // Scratch default direction
    s0->size = 100.0;   // percent
    s0->visible = 1;

    // Clear remaining sprite slots (safe defaults)
    for (int i = 1; i < MAX_SPRITES; i++) {
        p->sprites[i].id = 0;
        p->sprites[i].name[0] = '\0';
        p->sprites[i].x = 0.0;
        p->sprites[i].y = 0.0;
        p->sprites[i].dir = 90.0;
        p->sprites[i].size = 100.0;
        p->sprites[i].visible = 0;
    }
}