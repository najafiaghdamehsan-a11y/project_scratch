#include "engine/vars.h"

void varstore_init(VarStore* vs) {
    if (!vs) return;
    for (int i = 0; i < VAR_MAX; i++) {
        vs->used[i] = 0;
        vs->vals[i] = value_num(0.0);
    }
}

void varstore_clear(VarStore* vs) {
    varstore_init(vs);
}

void varstore_set(VarStore* vs, int id, Value v) {
    if (!vs) return;
    if (id < 0 || id >= VAR_MAX) return;
    vs->used[id] = 1;
    vs->vals[id] = v;
}

Value varstore_get(const VarStore* vs, int id) {
    if (!vs) return value_num(0.0);
    if (id < 0 || id >= VAR_MAX) return value_num(0.0);
    if (!vs->used[id]) return value_num(0.0);
    return vs->vals[id];
}

void varstore_change_num(VarStore* vs, int id, double delta) {
    if (!vs) return;
    if (id < 0 || id >= VAR_MAX) return;

    Value cur = varstore_get(vs, id);
    double x = value_as_num(cur) + delta;
    varstore_set(vs, id, value_num(x));
}