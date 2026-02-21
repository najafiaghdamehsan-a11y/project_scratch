#include "model/project_io.h"
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cinttypes> // PRIu64

static void set_err(char* err, int cap, const char* msg) {
    if (!err || cap <= 0) return;
    std::snprintf(err, (size_t)cap, "%s", msg ? msg : "error");
}

static void set_err_errno(char* err, int cap, const char* prefix) {
    if (!err || cap <= 0) return;
    std::snprintf(err, (size_t)cap, "%s: %s",
                  prefix ? prefix : "error", std::strerror(errno));
}

static int read_line(FILE* f, char* buf, int cap) {
    if (!std::fgets(buf, cap, f)) return 0;
    // trim CRLF
    size_t n = std::strlen(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = 0;
    return 1;
}

int project_save_v2(const Project* p, const char* path, char* err, int err_cap) {
    if (!p || !path) {
        set_err(err, err_cap, "project_save_v2: null arg");
        return 0;
    }

    char tmp[1024];
    std::snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE* f = std::fopen(tmp, "wb");
    if (!f) {
        set_err_errno(err, err_cap, "project_save_v2 fopen");
        return 0;
    }

    int n = p->sprite_count;
    if (n < 0) n = 0;
    if (n > MAX_SPRITES) n = MAX_SPRITES;

    int active = p->active_sprite_index;
    if (active < 0) active = 0;
    if (active >= n && n > 0) active = 0;
    if (n == 0) active = 0;

    std::fprintf(f, "PROJECT_SCRATCH_V2\n");
    std::fprintf(f, "SPRITES %d\n", n);
    std::fprintf(f, "ACTIVE %d\n", active);

    for (int i = 0; i < n; i++) {
        const Sprite* s = &p->sprites[i];

        // ensure name is safe
        char namebuf[MAX_NAME];
        std::snprintf(namebuf, sizeof(namebuf), "%s", s->name[0] ? s->name : "Sprite");

        // store name in quotes (supports spaces)
        std::fprintf(f,
            "SPRITE %d %" PRIu64 " \"%s\" %.6f %.6f %.6f %.6f %d\n",
            i,
            (uint64_t)s->id,
            namebuf,
            (double)s->x, (double)s->y, (double)s->dir, (double)s->size,
            (int)s->visible
        );
    }

    if (std::fclose(f) != 0) {
        set_err_errno(err, err_cap, "project_save_v2 fclose");
        return 0;
    }

    std::remove(path); // ignore failure
    if (std::rename(tmp, path) != 0) {
        set_err_errno(err, err_cap, "project_save_v2 rename");
        std::remove(tmp);
        return 0;
    }

    return 1;
}

int project_load_v2(Project* p, const char* path, char* err, int err_cap) {
    if (!p || !path) {
        set_err(err, err_cap, "project_load_v2: null arg");
        return 0;
    }

    FILE* f = std::fopen(path, "rb");
    if (!f) {
        set_err_errno(err, err_cap, "project_load_v2 fopen");
        return 0;
    }

    // Start from a clean model state
    model_init(p);

    char line[1024];

    // header
    if (!read_line(f, line, (int)sizeof(line))) {
        set_err(err, err_cap, "project_load_v2: empty file");
        std::fclose(f);
        return 0;
    }
    if (std::strcmp(line, "PROJECT_SCRATCH_V2") != 0) {
        set_err(err, err_cap, "project_load_v2: bad header/version");
        std::fclose(f);
        return 0;
    }

    // sprites count
    int n = 0;
    if (!read_line(f, line, (int)sizeof(line)) || std::sscanf(line, "SPRITES %d", &n) != 1) {
        set_err(err, err_cap, "project_load_v2: missing SPRITES line");
        std::fclose(f);
        return 0;
    }
    if (n < 0 || n > MAX_SPRITES) {
        set_err(err, err_cap, "project_load_v2: sprite count out of range");
        std::fclose(f);
        return 0;
    }

    // active index
    int active = 0;
    if (!read_line(f, line, (int)sizeof(line)) || std::sscanf(line, "ACTIVE %d", &active) != 1) {
        set_err(err, err_cap, "project_load_v2: missing ACTIVE line");
        std::fclose(f);
        return 0;
    }
    if (active < 0) active = 0;
    if (active >= n && n > 0) active = 0;

    // read sprites
    for (int i = 0; i < n; i++) {
        if (!read_line(f, line, (int)sizeof(line))) {
            set_err(err, err_cap, "project_load_v2: unexpected EOF reading sprites");
            std::fclose(f);
            return 0;
        }

        int idx = 0;
        uint64_t id = 0;
        char name[MAX_NAME] = {0};
        double x = 0, y = 0, dir = 90, size = 100;
        int visible = 1;

        // Parse: SPRITE idx id "name" x y dir size visible
        // name can contain spaces, stops at closing quote
        int ok = std::sscanf(
            line,
            "SPRITE %d %" SCNu64 " \"%63[^\"]\" %lf %lf %lf %lf %d",
            &idx, &id, name, &x, &y, &dir, &size, &visible
        );

        if (ok < 7) { // visible optional -> ok could be 7 without visible
            set_err(err, err_cap, "project_load_v2: bad SPRITE line");
            std::fclose(f);
            return 0;
        }

        if (idx < 0 || idx >= MAX_SPRITES) {
            set_err(err, err_cap, "project_load_v2: sprite index out of range");
            std::fclose(f);
            return 0;
        }

        Sprite* s = &p->sprites[idx];
        s->id = id;
        std::snprintf(s->name, sizeof(s->name), "%s", (name[0] ? name : "Sprite"));
        s->x = x;
        s->y = y;
        s->dir = dir;
        s->size = size;
        s->visible = (ok >= 8) ? visible : 1;
    }

    p->sprite_count = n;
    p->active_sprite_index = active;

    std::fclose(f);
    return 1;
}