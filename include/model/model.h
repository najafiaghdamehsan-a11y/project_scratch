#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SPRITES 64
#define MAX_NAME    64

    // Pen extension (minimal): a command queue so the runtime can request drawing,
    // and the UI can render pen strokes on a persistent canvas texture.
#define MAX_PEN_CMDS 8192

    typedef enum PenCmdType {
        PEN_CMD_CLEAR = 0,
        PEN_CMD_LINE  = 1,
        PEN_CMD_STAMP = 2,
    } PenCmdType;

    typedef struct PenCmd {
        PenCmdType type;
        double x1, y1;
        double x2, y2;
        uint8_t r, g, b;
        int size;
    } PenCmd;

    typedef struct Sprite {
        uint64_t id;
        char name[MAX_NAME];
        double x, y;
        double dir;
        double size;
        int visible;

        // Pen state (per-sprite)
        int pen_down;
        int pen_size;
        uint8_t pen_r, pen_g, pen_b;
    } Sprite;

    typedef struct Project {
        Sprite sprites[MAX_SPRITES];
        int sprite_count;
        int active_sprite_index;

        int dirty;   // NEW: 1 if unsaved edits exist

        // Pen draw requests produced by the runtime (consumed by UI each frame)
        PenCmd pen_cmds[MAX_PEN_CMDS];
        int pen_cmd_count;
    } Project;

    void model_init(Project* p);

#ifdef __cplusplus
}
#endif