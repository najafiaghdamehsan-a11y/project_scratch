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
        // Events
        BLK_EVENT_GREEN_FLAG = 0,
        BLK_EVENT_KEY_SPACE,
        BLK_EVENT_RECV_MSG1,

        // Events (broadcast)
        BLK_BROADCAST_MSG1,

        // Motion
        BLK_MOVE_STEPS,
        BLK_TURN_DEG,
        BLK_GOTO_XY,
        BLK_CHANGE_X_BY,
        BLK_CHANGE_Y_BY,
        BLK_GOTO_RANDOM,
        BLK_BOUNCE_EDGE,

        // Motion (stack-input variants)
        BLK_CHANGE_X_BY_POP,
        BLK_CHANGE_Y_BY_POP,
        BLK_SET_X_POP,
        BLK_SET_Y_POP,

        // Operators (stack-based)
        BLK_PUSH_NUM,
        BLK_RANDOM_RANGE,
        BLK_ADD,
        BLK_SUB,
        BLK_MUL,
        BLK_DIV,
        BLK_GT,
        BLK_LT,
        BLK_EQ,
        BLK_AND,
        BLK_OR,
        BLK_NOT,

        // Variables (var0 only, stack-based)
        BLK_VAR0_READ,
        BLK_VAR0_SET,
        BLK_VAR0_CHANGE,

        // Control
        BLK_WAIT_MS,
        BLK_REPEAT_BEGIN,
        BLK_REPEAT_END,
        BLK_FOREVER_BEGIN,
        BLK_FOREVER_END,

        // Simple IF family
        BLK_IF_X_GT,
        BLK_IF_X_LT,
        BLK_IF_Y_GT,
        BLK_IF_Y_LT,
        BLK_IF_RANDOM_LT,
        BLK_ELSE,
        BLK_ENDIF,

        // Sensing (stack-based reporters + ask/answer)
        BLK_SENSE_MOUSE_X,
        BLK_SENSE_MOUSE_Y,
        BLK_SENSE_MOUSE_DOWN,
        BLK_SENSE_TIMER,
        BLK_SENSE_DISTANCE_MOUSE,
        BLK_SENSE_ASK_WAIT,
        BLK_SENSE_ANSWER,

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

        // workspace scroll (vertical). blocks are stored in "world" coords,
        // and rendered at (world_y - scroll_y).
        int scroll_y; // pixels

        // state
        BlockCategory cat;

        // Variables UX: which variable id new var blocks should target
        int selected_var_id;

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

    void block_editor_render(BlockEditor* be, SDL_Renderer* ren, TTF_Font* font, uint64_t highlight_id);

    int  block_editor_block_count(const BlockEditor* be);
    const BlockInstance* block_editor_blocks(const BlockEditor* be);

#ifdef __cplusplus
}
#endif