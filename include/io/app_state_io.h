#pragma once

#include "model/model.h"
#include "ui/block_editor.h"
#include "engine/vars.h"

#ifdef __cplusplus
extern "C" {
#endif

    // Save/load full app state (sprites + workspace blocks + variables).
    // Returns 1 on success, 0 on failure.
    // If err != nullptr, writes a message there (up to err_cap chars).
    int app_state_save_v1(const Project* p,
                          const BlockEditor* be,
                          const VarStore* vs,
                          const char* path,
                          char* err,
                          int err_cap);

    int app_state_load_v1(Project* p,
                          BlockEditor* be,
                          VarStore* vs,
                          const char* path,
                          char* err,
                          int err_cap);

#ifdef __cplusplus
}
#endif
