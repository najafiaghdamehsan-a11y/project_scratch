#pragma once
#include <stdint.h>
#include <SDL.h>
#include <SDL_ttf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BE_MAX_BLOCKS 512

    typedef enum BE_Category {
        BE_CAT_MOTION = 0,
        BE_CAT_LOOKS,
        BE_CAT_SOUND,
        BE_CAT_EVENTS,
        BE_CAT_CONTROL,
        BE_CAT_SENSING,
        BE_CAT_OPERATORS,
        BE_CAT_VARIABLES,
        BE_CAT_COUNT
    } BE_Category;

    typedef enum BE_Opcode {
        BE_OP_MOVE_STEPS = 1,
        BE_OP_TURN_DEG,
        BE_OP_GOTO_XY
    } BE_Opcode;

    typedef struct BE_BlockInstance {
        uint64_t id;
        int opcode;     // BE_Opcode
        int a;          // arg0
        int b;          // arg1 (used by goto x/y)
        int x, y;       // screen-space position
        int w, h;       // size
        int category;   // BE_Category (for color)
    } BE_BlockInstance;

    typedef struct BlockEditor {
        // Layout (screen-space)
        SDL_Rect rect_cat;
        SDL_Rect rect_palette;
        SDL_Rect rect_workspace;

        int selected_category;   // BE_Category
        int palette_scroll_y;    // (optional, for wheel later)

        // Workspace blocks
        BE_BlockInstance blocks[BE_MAX_BLOCKS];
        int block_count;
        uint64_t next_id;

        // Drag state
        int dragging_index;      // -1 none
        int drag_off_x;
        int drag_off_y;

        // Hover state (optional)
        int hover_palette_index; // -1 none
        int hover_block_index;   // -1 none
    } BlockEditor;

    void block_editor_init(BlockEditor* be);
    void block_editor_set_layout(BlockEditor* be, SDL_Rect cat, SDL_Rect palette, SDL_Rect workspace);

    void block_editor_handle_event(BlockEditor* be, const SDL_Event* e);
    void block_editor_draw(BlockEditor* be, SDL_Renderer* ren, TTF_Font* font);

#ifdef __cplusplus
}
#endif