#include "model/project_io.h"
#include <cstdio>
#include <cstring>
#include <cerrno>

static void set_err(char* err, int cap, const char* msg) {
    if (!err || cap <= 0) return;
    std::snprintf(err, (size_t)cap, "%s", msg ? msg : "error");
}

static void set_err_errno(char* err, int cap, const char* prefix) {
    if (!err || cap <= 0) return;
    std::snprintf(err, (size_t)cap, "%s: %s", prefix ? prefix : "error", std::strerror(errno));
}

static int sprite_capacity(const Project* p) {
    // Assumes sprites is a fixed-size array in Project (common in your codebase).
    // If it’s not an array, you’ll need a PROJECT_MAX_SPRITES constant in model.h.
    return (int)(sizeof(p->sprites) / sizeof(p->sprites[0]));
}

int project_save_v1(const Project* p, const char* path, char* err, int err_cap) {
    if (!p || !path) {
        set_err(err, err_cap, "project_save_v1: null arg");
        return 0;
    }

    // write to temp then rename (atomic-ish)
    char tmp[1024];
    std::snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE* f = std::fopen(tmp, "wb");
    if (!f) {
        set_err_errno(err, err_cap, "project_save_v1 fopen");
        return 0;
    }

    std::fprintf(f, "PROJECT_SCRATCH_V1\n");
    std::fprintf(f, "SPRITES %d\n", p->sprite_count);

    for (int i = 0; i < p->sprite_count; i++) {
        const Sprite* s = &p->sprites[i];
        std::fprintf(f, "SPRITE %d %.6f %.6f %.6f %d\n",
                     i, (double)s->x, (double)s->y, (double)s->dir, (int)s->visible);
    }

    if (std::fclose(f) != 0) {
        set_err_errno(err, err_cap, "project_save_v1 fclose");
        return 0;
    }

    // replace target
    std::remove(path); // ignore failure
    if (std::rename(tmp, path) != 0) {
        set_err_errno(err, err_cap, "project_save_v1 rename");
        std::remove(tmp);
        return 0;
    }

    return 1;
}

int project_load_v1(Project* p, const char* path, char* err, int err_cap) {
    if (!p || !path) {
        set_err(err, err_cap, "project_load_v1: null arg");
        return 0;
    }

    FILE* f = std::fopen(path, "rb");
    if (!f) {
        set_err_errno(err, err_cap, "project_load_v1 fopen");
        return 0;
    }

    char line[512];

    // line 1: header
    if (!std::fgets(line, (int)sizeof(line), f)) {
        set_err(err, err_cap, "project_load_v1: empty file");
        std::fclose(f);
        return 0;
    }
    if (std::strncmp(line, "PROJECT_SCRATCH_V1", 17) != 0) {
        set_err(err, err_cap, "project_load_v1: bad header/version");
        std::fclose(f);
        return 0;
    }

    // line 2: SPRITES n
    int n = 0;
    if (!std::fgets(line, (int)sizeof(line), f) || std::sscanf(line, "SPRITES %d", &n) != 1) {
        set_err(err, err_cap, "project_load_v1: missing SPRITES line");
        std::fclose(f);
        return 0;
    }

    const int cap = sprite_capacity(p);
    if (n < 0 || n > cap) {
        set_err(err, err_cap, "project_load_v1: sprite count out of range");
        std::fclose(f);
        return 0;
    }

    // Clear minimal fields (don’t memset whole Project, keep safe)
    p->sprite_count = 0;

    for (int i = 0; i < n; i++) {
        if (!std::fgets(line, (int)sizeof(line), f)) {
            set_err(err, err_cap, "project_load_v1: unexpected EOF reading sprites");
            std::fclose(f);
            return 0;
        }

        int idx = 0;
        double x=0, y=0, dir=90;
        int visible = 1;

        int ok = std::sscanf(line, "SPRITE %d %lf %lf %lf %d", &idx, &x, &y, &dir, &visible);
        if (ok < 4) {
            set_err(err, err_cap, "project_load_v1: bad SPRITE line");
            std::fclose(f);
            return 0;
        }
        if (idx < 0 || idx >= cap) {
            set_err(err, err_cap, "project_load_v1: sprite index out of range");
            std::fclose(f);
            return 0;
        }

        Sprite* s = &p->sprites[idx];
        s->x = x;
        s->y = y;
        s->dir = dir;
        s->visible = (ok >= 5) ? visible : 1;
    }

    p->sprite_count = n;
    std::fclose(f);
    return 1;
}