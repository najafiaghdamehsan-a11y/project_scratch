#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum ValueType {
        VAL_NUM = 0,
        VAL_BOOL = 1
    } ValueType;

    typedef struct Value {
        ValueType type;
        double num;     // used for VAL_NUM
        int boolean;    // used for VAL_BOOL (0/1)
    } Value;

    static inline Value value_num(double x) { Value v; v.type = VAL_NUM; v.num = x; v.boolean = (x != 0.0); return v; }
    static inline Value value_bool(int b)   { Value v; v.type = VAL_BOOL; v.boolean = (b != 0); v.num = v.boolean ? 1.0 : 0.0; return v; }

    double value_as_num(Value v);
    int    value_as_bool(Value v);

#ifdef __cplusplus
}
#endif