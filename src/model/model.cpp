#include "model/model.h"
#include <cstring>

void model_init(Project* p) {
    std::memset(p, 0, sizeof(Project));
    p->sprite_count = 1;
    p->active_sprite_index = 0;

    p->sprites[0].id = 1;
    std::strncpy(p->sprites[0].name, "Sprite1", MAX_NAME - 1);
    p->sprites[0].x = 0;
    p->sprites[0].y = 0;
    p->sprites[0].dir = 90;
    p->sprites[0].size = 100;
    p->sprites[0].visible = 1;
}