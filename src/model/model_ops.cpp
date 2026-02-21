#include "model/model_ops.h"
#include <cstdio>
#include <cstring>

static int in_range_sprite(const Project* p, int index) {
    return p && index >= 0 && index < p->sprite_count;
}

void project_mark_dirty(Project* p) {
    if (!p) return;
    p->dirty = 1;
}

void project_clear_dirty(Project* p) {
    if (!p) return;
    p->dirty = 0;
}

int project_is_dirty(const Project* p) {
    if (!p) return 0;
    return p->dirty != 0;
}

void project_new(Project* p) {
    if (!p) return;
    model_init(p);
    p->dirty = 0;
}

int project_set_active_sprite(Project* p, int index) {
    if (!p) return 0;
    if (p->sprite_count <= 0) { p->active_sprite_index = 0; return 1; }
    if (index < 0 || index >= p->sprite_count) return 0;
    p->active_sprite_index = index;
    // selecting a sprite is not an edit -> do NOT mark dirty
    return 1;
}

int project_add_sprite(Project* p, const char* name) {
    if (!p) return -1;
    if (p->sprite_count < 0) p->sprite_count = 0;
    if (p->sprite_count >= MAX_SPRITES) return -1;

    int idx = p->sprite_count++;
    Sprite* s = &p->sprites[idx];

    // minimal defaults (Scratch-like)
    s->id = (uint64_t)idx + 1;      // simple id for now
    std::snprintf(s->name, MAX_NAME, "%s", (name && name[0]) ? name : "Sprite");
    s->x = 0.0;
    s->y = 0.0;
    s->dir = 90.0;
    s->size = 100.0;
    s->visible = 1;

    if (p->sprite_count == 1) p->active_sprite_index = 0;

    project_mark_dirty(p);
    return idx;
}

int project_delete_sprite(Project* p, int index) {
    if (!p) return 0;
    if (!in_range_sprite(p, index)) return 0;

    // shift left
    for (int i = index; i < p->sprite_count - 1; i++) {
        p->sprites[i] = p->sprites[i + 1];
    }
    p->sprite_count--;

    if (p->sprite_count <= 0) {
        p->sprite_count = 0;
        p->active_sprite_index = 0;
    } else if (p->active_sprite_index >= p->sprite_count) {
        p->active_sprite_index = p->sprite_count - 1;
    }

    project_mark_dirty(p);
    return 1;
}

int sprite_set_name(Project* p, int index, const char* name) {
    if (!p || !in_range_sprite(p, index)) return 0;
    Sprite* s = &p->sprites[index];
    std::snprintf(s->name, MAX_NAME, "%s", (name && name[0]) ? name : "Sprite");
    project_mark_dirty(p);
    return 1;
}

int sprite_set_pos(Project* p, int index, double x, double y) {
    if (!p || !in_range_sprite(p, index)) return 0;
    Sprite* s = &p->sprites[index];
    s->x = x;
    s->y = y;
    project_mark_dirty(p);
    return 1;
}

int sprite_set_dir(Project* p, int index, double dir) {
    if (!p || !in_range_sprite(p, index)) return 0;
    p->sprites[index].dir = dir;
    project_mark_dirty(p);
    return 1;
}

int sprite_set_size(Project* p, int index, double size) {
    if (!p || !in_range_sprite(p, index)) return 0;
    p->sprites[index].size = size;
    project_mark_dirty(p);
    return 1;
}

int sprite_set_visible(Project* p, int index, int visible) {
    if (!p || !in_range_sprite(p, index)) return 0;
    p->sprites[index].visible = (visible != 0);
    project_mark_dirty(p);
    return 1;
}