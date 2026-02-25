#pragma once
#include "engine/value.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VAR_MAX 64
#define VAR_NAME_MAX 32

    typedef struct VarStore {
        Value   vals[VAR_MAX];
        uint8_t used[VAR_MAX];
        char    names[VAR_MAX][VAR_NAME_MAX];
    } VarStore;

    // Initializes store, creates default var0.
    void  varstore_init(VarStore* vs);

    // Clears variables + names ("new project" semantics).
    void  varstore_clear_all(VarStore* vs);

    // Back-compat: old code called varstore_clear; treat as clear_all.
    void  varstore_clear(VarStore* vs);

    // Scratch-like: reset values to 0 but keep variable names.
    void  varstore_reset_values(VarStore* vs);

    int   varstore_is_used(const VarStore* vs, int id);
    const char* varstore_name(const VarStore* vs, int id);

    // Returns id in [0,VAR_MAX) or -1.
    int   varstore_find_by_name(const VarStore* vs, const char* name);

    // Creates a variable if name is valid and not duplicate.
    // Returns new id, or existing id if duplicate, or -1 on error.
    int   varstore_create(VarStore* vs, const char* name);

    // Deletes variable id (keeps slot free). Returns 1 if deleted.
    int   varstore_delete(VarStore* vs, int id);

    void  varstore_set(VarStore* vs, int id, Value v);
    Value varstore_get(const VarStore* vs, int id);

    void  varstore_change_num(VarStore* vs, int id, double delta);

#ifdef __cplusplus
}
#endif
