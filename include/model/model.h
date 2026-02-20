#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SPRITES 64
#define MAX_NAME    64

    typedef struct Sprite {
        uint64_t id;
        char name[MAX_NAME];
        double x, y;
        double dir;
        double size;
        int visible;
    } Sprite;

    typedef struct Project {
        Sprite sprites[MAX_SPRITES];
        int sprite_count;
        int active_sprite_index;
    } Project;

    void model_init(Project* p);

#ifdef __cplusplus
}
#endif