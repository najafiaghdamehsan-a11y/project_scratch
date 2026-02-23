#pragma once
#include <stdint.h>
#include <SDL.h>
#include <SDL_ttf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_WORKSPACE_BLOCKS 512

    typedef enum BlockCategory {
        CAT_MOTION = 0,
        CAT_LOOKS,
        CAT_SOUND,
        CAT_EVENTS,
        CAT_CONTROL,
        CAT_SENSING,
        CAT_OPERATORS,
        CAT_VARIABLES,
        CAT_COUNT
    } BlockCategory;

    typedef enum BlockType {
        // Motion
        BLK_MOVE_STEPS = 0,
        BLK_TURN_DEG,
        BLK_GOTO_XY,

        // Events (hat blocks)
        BLK_EVENT_GREEN_FLAG,     // start of a script stack

        // Control
        BLK_WAIT_MS,              // a = ms
        BLK_REPEAT_BEGIN,         // a = repeat count
        BLK_REPEAT_END,           // closes repeat
        BLK_FOREVER_BEGIN,
        BLK_FOREVER_END,          // closes forever

        // If/Else/Endif (using your engine's CondCode)
        BLK_IF_X_GT,              // a = threshold
        BLK_IF_X_LT,
        BLK_IF_Y_GT,
        BLK_IF_Y_LT,
        BLK_IF_RANDOM_LT,         // a = percent (0..100)
        BLK_ELSE,
        BLK_ENDIF,

        BLK_COUNT
    } BlockType;

    typedef struct BlockInstance {
        uint64_t id;
        BlockType type;
        int a;          // argument 1 (steps / degrees / x)
        int b;          // argument 2 (y)
        SDL_Rect r;     // position in workspace
    } BlockInstance;

    typedef struct BlockEditor {
        // layout
        SDL_Rect cat_r;
        SDL_Rect palette_r;
        SDL_Rect work_r;

        // state
        BlockCategory cat;

        // workspace blocks
        BlockInstance blocks[MAX_WORKSPACE_BLOCKS];
        int block_count;

        int selected_index;   // -1 = none

        // dragging
        int dragging;         // 0/1
        int drag_index;       // index in blocks[]
        int drag_from_palette;// 0/1
        int drag_off_x;
        int drag_off_y;

        uint64_t next_id;
    } BlockEditor;

    void block_editor_init(BlockEditor* be);
    void block_editor_set_layout(BlockEditor* be, SDL_Rect cat_r, SDL_Rect palette_r, SDL_Rect work_r);

    void block_editor_handle_event(BlockEditor* be, const SDL_Event* e);

    // render (draws category list + palette + workspace blocks + dotted grid)
    void block_editor_render(BlockEditor* be, SDL_Renderer* ren, TTF_Font* font);

    int  block_editor_block_count(const BlockEditor* be);
    const BlockInstance* block_editor_blocks(const BlockEditor* be);

#ifdef __cplusplus
}
#endif