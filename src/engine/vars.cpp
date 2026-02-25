#include "engine/vars.h"
#include <string.h>
#include <stdio.h>

static void make_default_name(char out[VAR_NAME_MAX], int id) {
    if (!out) return;
    snprintf(out, VAR_NAME_MAX, "var%d", id);
    out[VAR_NAME_MAX - 1] = 0;
}

static int name_equal(const char* a, const char* b) {
    if (!a || !b) return 0;
    return strcmp(a, b) == 0;
}

static int normalize_name(const char* in, char out[VAR_NAME_MAX]) {
    if (!out) return 0;
    out[0] = 0;
    if (!in) return 0;

    // trim leading/trailing spaces
    const char* s = in;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    const char* e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) e--;

    int n = (int)(e - s);
    if (n <= 0) return 0;
    if (n >= VAR_NAME_MAX) n = VAR_NAME_MAX - 1;

    memcpy(out, s, (size_t)n);
    out[n] = 0;
    return 1;
}

void varstore_clear_all(VarStore* vs) {
    if (!vs) return;
    for (int i = 0; i < VAR_MAX; i++) {
        vs->used[i] = 0;
        vs->vals[i] = value_num(0.0);
        vs->names[i][0] = 0;
    }

    // Always create var0 by default.
    vs->used[0] = 1;
    snprintf(vs->names[0], VAR_NAME_MAX, "var0");
    vs->vals[0] = value_num(0.0);
}

void varstore_init(VarStore* vs) {
    varstore_clear_all(vs);
}

void varstore_clear(VarStore* vs) {
    varstore_clear_all(vs);
}

void varstore_reset_values(VarStore* vs) {
    if (!vs) return;
    for (int i = 0; i < VAR_MAX; i++) {
        if (!vs->used[i]) continue;
        vs->vals[i] = value_num(0.0);
    }

    // Ensure var0 always exists.
    if (!vs->used[0]) {
        vs->used[0] = 1;
        snprintf(vs->names[0], VAR_NAME_MAX, "var0");
        vs->vals[0] = value_num(0.0);
    }
}

int varstore_is_used(const VarStore* vs, int id) {
    if (!vs) return 0;
    if (id < 0 || id >= VAR_MAX) return 0;
    return vs->used[id] ? 1 : 0;
}

const char* varstore_name(const VarStore* vs, int id) {
    if (!vs) return "";
    if (id < 0 || id >= VAR_MAX) return "";
    if (!vs->used[id]) return "";
    if (!vs->names[id][0]) return "";
    return vs->names[id];
}

int varstore_find_by_name(const VarStore* vs, const char* name) {
    if (!vs || !name) return -1;
    char norm[VAR_NAME_MAX];
    if (!normalize_name(name, norm)) return -1;

    for (int i = 0; i < VAR_MAX; i++) {
        if (!vs->used[i]) continue;
        if (vs->names[i][0] && name_equal(vs->names[i], norm)) return i;
    }
    return -1;
}

int varstore_create(VarStore* vs, const char* name) {
    if (!vs) return -1;

    char norm[VAR_NAME_MAX];
    if (!normalize_name(name, norm)) return -1;

    int existing = varstore_find_by_name(vs, norm);
    if (existing != -1) return existing; // duplicate prevention

    for (int i = 0; i < VAR_MAX; i++) {
        if (vs->used[i]) continue;
        vs->used[i] = 1;
        snprintf(vs->names[i], VAR_NAME_MAX, "%s", norm);
        vs->names[i][VAR_NAME_MAX - 1] = 0;
        vs->vals[i] = value_num(0.0);
        return i;
    }

    return -1; // full
}

int varstore_delete(VarStore* vs, int id) {
    if (!vs) return 0;
    if (id < 0 || id >= VAR_MAX) return 0;
    if (!vs->used[id]) return 0;
    if (id == 0) return 0; // keep var0

    vs->used[id] = 0;
    vs->vals[id] = value_num(0.0);
    vs->names[id][0] = 0;
    return 1;
}

void varstore_set(VarStore* vs, int id, Value v) {
    if (!vs) return;
    if (id < 0 || id >= VAR_MAX) return;
    vs->used[id] = 1;

    // If this slot was set without a name, give it a default name.
    if (!vs->names[id][0]) make_default_name(vs->names[id], id);

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
