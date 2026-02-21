#pragma once
#include "model/model.h"

#ifdef __cplusplus
extern "C" {
#endif

    // Dirty flag
    void project_mark_dirty(Project* p);
    void project_clear_dirty(Project* p);
    int  project_is_dirty(const Project* p);

    // "New project" helper
    void project_new(Project* p);

    // Sprite CRUD helpers (for UI)
    int  project_add_sprite(Project* p, const char* name);   // returns index or -1
    int  project_delete_sprite(Project* p, int index);       // returns 1/0
    int  project_set_active_sprite(Project* p, int index);   // returns 1/0

    // Sprite property setters (mark dirty)
    int  sprite_set_name(Project* p, int index, const char* name);
    int  sprite_set_pos(Project* p, int index, double x, double y);
    int  sprite_set_dir(Project* p, int index, double dir);
    int  sprite_set_size(Project* p, int index, double size);
    int  sprite_set_visible(Project* p, int index, int visible);

#ifdef __cplusplus
}
#endif