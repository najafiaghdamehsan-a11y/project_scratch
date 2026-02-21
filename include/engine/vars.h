#pragma once
#include "engine/value.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VAR_MAX 64

    typedef struct VarStore {
        Value vals[VAR_MAX];
        uint8_t used[VAR_MAX];
    } VarStore;

    void  varstore_init(VarStore* vs);
    void  varstore_clear(VarStore* vs);

    void  varstore_set(VarStore* vs, int id, Value v);
    Value varstore_get(const VarStore* vs, int id);

    void  varstore_change_num(VarStore* vs, int id, double delta);

#ifdef __cplusplus
}
#endif