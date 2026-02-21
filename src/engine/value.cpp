#include "engine/value.h"

double value_as_num(Value v) {
    if (v.type == VAL_BOOL) return v.boolean ? 1.0 : 0.0;
    return v.num;
}

int value_as_bool(Value v) {
    if (v.type == VAL_BOOL) return v.boolean != 0;
    return v.num != 0.0;
}