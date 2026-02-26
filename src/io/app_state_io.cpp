#include "io/app_state_io.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cinttypes>

static void set_err(char* err, int cap, const char* msg) {
    if (!err || cap <= 0) return;
    std::snprintf(err, (size_t)cap, "%s", msg ? msg : "error");
}

static void set_err_errno(char* err, int cap, const char* prefix) {
    if (!err || cap <= 0) return;
    std::snprintf(err, (size_t)cap, "%s: %s", prefix ? prefix : "error", std::strerror(errno));
}

static int read_line(FILE* f, char* buf, int cap) {
    if (!std::fgets(buf, cap, f)) return 0;
    size_t n = std::strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
    return 1;
}

// Keep workspace block rectangle consistent with editor defaults.
static SDL_Rect default_block_rect(int x, int y) {
    SDL_Rect r;
    r.x = x;
    r.y = y;
    r.w = 240;
    r.h = 40;
    return r;
}

int app_state_save_v1(const Project* p,
                      const BlockEditor* be,
                      const VarStore* vs,
                      const char* path,
                      char* err,
                      int err_cap) {
    if (!p || !be || !vs || !path) {
        set_err(err, err_cap, "app_state_save_v1: null arg");
        return 0;
    }

    char tmp[1024];
    std::snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE* f = std::fopen(tmp, "wb");
    if (!f) {
        set_err_errno(err, err_cap, "app_state_save_v1 fopen");
        return 0;
    }

    int n = p->sprite_count;
    if (n < 0) n = 0;
    if (n > MAX_SPRITES) n = MAX_SPRITES;

    int active = p->active_sprite_index;
    if (active < 0) active = 0;
    if (active >= n && n > 0) active = 0;
    if (n == 0) active = 0;

    std::fprintf(f, "SCRATCHCPP_APPSTATE_V1\n");

    // ---- Sprites ----
    std::fprintf(f, "SPRITES %d\n", n);
    std::fprintf(f, "ACTIVE %d\n", active);
    for (int i = 0; i < n; i++) {
        const Sprite* s = &p->sprites[i];
        char namebuf[MAX_NAME];
        std::snprintf(namebuf, sizeof(namebuf), "%s", s->name[0] ? s->name : "Sprite");
        std::fprintf(
            f,
            // Backward-compatible: older loaders may ignore trailing fields.
            // Fields after 'visible' store pen state.
            "SPRITE %d %" PRIu64 " \"%s\" %.6f %.6f %.6f %.6f %d %d %d %d %d %d\n",
            i,
            (uint64_t)s->id,
            namebuf,
            (double)s->x,
            (double)s->y,
            (double)s->dir,
            (double)s->size,
            (int)s->visible,
            (int)s->pen_down,
            (int)s->pen_size,
            (int)s->pen_r,
            (int)s->pen_g,
            (int)s->pen_b);
    }

    // ---- Variables ----
    int vcount = 0;
    for (int i = 0; i < VAR_MAX; i++) if (vs->used[i]) vcount++;
    std::fprintf(f, "VARS %d\n", vcount);
    for (int i = 0; i < VAR_MAX; i++) {
        if (!vs->used[i]) continue;
        const char* nm = vs->names[i][0] ? vs->names[i] : "";
        const Value v = vs->vals[i];
        std::fprintf(
            f,
            "VAR %d \"%s\" %d %.17g %d\n",
            i,
            nm,
            (int)v.type,
            (double)v.num,
            (int)v.boolean);
    }

    // ---- Editor meta ----
    std::fprintf(f, "EDITOR_CAT %d\n", (int)be->cat);
    std::fprintf(f, "EDITOR_SELECTED_VAR %d\n", (int)be->selected_var_id);
    std::fprintf(f, "EDITOR_NEXT_ID %" PRIu64 "\n", (uint64_t)be->next_id);

    // ---- Blocks ----
    int bcount = be->block_count;
    if (bcount < 0) bcount = 0;
    if (bcount > MAX_WORKSPACE_BLOCKS) bcount = MAX_WORKSPACE_BLOCKS;
    std::fprintf(f, "BLOCKS %d\n", bcount);
    for (int i = 0; i < bcount; i++) {
        const BlockInstance* b = &be->blocks[i];
        std::fprintf(
            f,
            "BLOCK %" PRIu64 " %d %d %d %d %d\n",
            (uint64_t)b->id,
            (int)b->type,
            (int)b->a,
            (int)b->b,
            (int)b->r.x,
            (int)b->r.y);
    }

    if (std::fclose(f) != 0) {
        set_err_errno(err, err_cap, "app_state_save_v1 fclose");
        return 0;
    }

    std::remove(path);
    if (std::rename(tmp, path) != 0) {
        set_err_errno(err, err_cap, "app_state_save_v1 rename");
        std::remove(tmp);
        return 0;
    }

    return 1;
}

int app_state_load_v1(Project* p,
                      BlockEditor* be,
                      VarStore* vs,
                      const char* path,
                      char* err,
                      int err_cap) {
    if (!p || !be || !vs || !path) {
        set_err(err, err_cap, "app_state_load_v1: null arg");
        return 0;
    }

    FILE* f = std::fopen(path, "rb");
    if (!f) {
        set_err_errno(err, err_cap, "app_state_load_v1 fopen");
        return 0;
    }

    // clean state
    model_init(p);
    block_editor_init(be);
    varstore_clear_all(vs);

    char line[1024];
    if (!read_line(f, line, (int)sizeof(line))) {
        set_err(err, err_cap, "app_state_load_v1: empty file");
        std::fclose(f);
        return 0;
    }
    if (std::strcmp(line, "SCRATCHCPP_APPSTATE_V1") != 0) {
        set_err(err, err_cap, "app_state_load_v1: bad header/version");
        std::fclose(f);
        return 0;
    }

    // sprites count
    int n = 0;
    if (!read_line(f, line, (int)sizeof(line)) || std::sscanf(line, "SPRITES %d", &n) != 1) {
        set_err(err, err_cap, "app_state_load_v1: missing SPRITES line");
        std::fclose(f);
        return 0;
    }
    if (n < 0) n = 0;
    if (n > MAX_SPRITES) n = MAX_SPRITES;

    int active = 0;
    if (!read_line(f, line, (int)sizeof(line)) || std::sscanf(line, "ACTIVE %d", &active) != 1) {
        set_err(err, err_cap, "app_state_load_v1: missing ACTIVE line");
        std::fclose(f);
        return 0;
    }
    if (active < 0) active = 0;
    if (active >= n && n > 0) active = 0;

    // read sprites
    for (int i = 0; i < n; i++) {
        if (!read_line(f, line, (int)sizeof(line))) {
            set_err(err, err_cap, "app_state_load_v1: unexpected EOF reading sprites");
            std::fclose(f);
            return 0;
        }

        int idx = 0;
        uint64_t id = 0;
        char name[MAX_NAME] = {0};
        double x = 0, y = 0, dir = 90, size = 100;
        int visible = 1;
        int pen_down = 0;
        int pen_size = 4;
        int pen_r = 0, pen_g = 0, pen_b = 0;
        int ok = std::sscanf(
            line,
            // Backward-compatible parser: accepts older lines without visible/pen fields.
            "SPRITE %d %" SCNu64 " \"%63[^\"]\" %lf %lf %lf %lf %d %d %d %d %d %d",
            &idx,
            &id,
            name,
            &x,
            &y,
            &dir,
            &size,
            &visible,
            &pen_down,
            &pen_size,
            &pen_r,
            &pen_g,
            &pen_b);

        if (ok < 7) {
            set_err(err, err_cap, "app_state_load_v1: bad SPRITE line");
            std::fclose(f);
            return 0;
        }

        if (idx < 0 || idx >= MAX_SPRITES) continue;
        Sprite* s = &p->sprites[idx];
        s->id = id;
        std::snprintf(s->name, sizeof(s->name), "%s", name[0] ? name : "Sprite");
        s->x = x;
        s->y = y;
        s->dir = dir;
        s->size = size;
        s->visible = (ok >= 8) ? visible : 1;

        // Pen state (optional in file)
        if (ok >= 10) {
            s->pen_down = pen_down;
            s->pen_size = pen_size;
        }
        if (ok >= 13) {
            s->pen_r = (uint8_t)pen_r;
            s->pen_g = (uint8_t)pen_g;
            s->pen_b = (uint8_t)pen_b;
        }
    }
    p->sprite_count = n;
    p->active_sprite_index = active;

    // ---- VARS ----
    int vcount = 0;
    if (!read_line(f, line, (int)sizeof(line)) || std::sscanf(line, "VARS %d", &vcount) != 1) {
        set_err(err, err_cap, "app_state_load_v1: missing VARS line");
        std::fclose(f);
        return 0;
    }
    if (vcount < 0) vcount = 0;
    if (vcount > VAR_MAX) vcount = VAR_MAX;

    for (int i = 0; i < vcount; i++) {
        if (!read_line(f, line, (int)sizeof(line))) {
            set_err(err, err_cap, "app_state_load_v1: unexpected EOF reading VAR");
            std::fclose(f);
            return 0;
        }
        int id = 0;
        char name[VAR_NAME_MAX] = {0};
        int type = 0;
        double num = 0.0;
        int boolean = 0;
        int ok = std::sscanf(line, "VAR %d \"%31[^\"]\" %d %lf %d", &id, name, &type, &num, &boolean);
        if (ok < 4) continue;
        if (id < 0 || id >= VAR_MAX) continue;
        vs->used[id] = 1;
        std::snprintf(vs->names[id], VAR_NAME_MAX, "%s", name);
        vs->names[id][VAR_NAME_MAX - 1] = 0;
        Value v;
        v.type = (type == (int)VAL_BOOL) ? VAL_BOOL : VAL_NUM;
        v.num = num;
        v.boolean = (ok >= 5) ? (boolean != 0) : (num != 0.0);
        vs->vals[id] = v;
    }

    // Ensure var0 exists
    if (!vs->used[0]) {
        vs->used[0] = 1;
        std::snprintf(vs->names[0], VAR_NAME_MAX, "var0");
        vs->vals[0] = value_num(0.0);
    }

    // ---- Editor meta ----
    int cat = 0;
    int sel_var = 0;
    uint64_t next_id = 1;

    if (!read_line(f, line, (int)sizeof(line)) || std::sscanf(line, "EDITOR_CAT %d", &cat) != 1) {
        set_err(err, err_cap, "app_state_load_v1: missing EDITOR_CAT");
        std::fclose(f);
        return 0;
    }
    if (!read_line(f, line, (int)sizeof(line)) || std::sscanf(line, "EDITOR_SELECTED_VAR %d", &sel_var) != 1) {
        set_err(err, err_cap, "app_state_load_v1: missing EDITOR_SELECTED_VAR");
        std::fclose(f);
        return 0;
    }
    if (!read_line(f, line, (int)sizeof(line)) || std::sscanf(line, "EDITOR_NEXT_ID %" SCNu64, &next_id) != 1) {
        set_err(err, err_cap, "app_state_load_v1: missing EDITOR_NEXT_ID");
        std::fclose(f);
        return 0;
    }

    if (cat < 0) cat = 0;
    if (cat >= (int)CAT_COUNT) cat = 0;
    be->cat = (BlockCategory)cat;
    be->selected_var_id = (sel_var >= 0 && sel_var < VAR_MAX && vs->used[sel_var]) ? sel_var : 0;
    be->next_id = next_id;

    // ---- Blocks ----
    int bcount = 0;
    if (!read_line(f, line, (int)sizeof(line)) || std::sscanf(line, "BLOCKS %d", &bcount) != 1) {
        set_err(err, err_cap, "app_state_load_v1: missing BLOCKS");
        std::fclose(f);
        return 0;
    }
    if (bcount < 0) bcount = 0;
    if (bcount > MAX_WORKSPACE_BLOCKS) bcount = MAX_WORKSPACE_BLOCKS;

    uint64_t max_id = be->next_id;
    be->block_count = 0;
    for (int i = 0; i < bcount; i++) {
        if (!read_line(f, line, (int)sizeof(line))) {
            set_err(err, err_cap, "app_state_load_v1: unexpected EOF reading BLOCK");
            std::fclose(f);
            return 0;
        }
        uint64_t id = 0;
        int type = 0;
        int a = 0;
        int b = 0;
        int x = 0;
        int y = 0;
        int ok = std::sscanf(line, "BLOCK %" SCNu64 " %d %d %d %d %d", &id, &type, &a, &b, &x, &y);
        if (ok < 6) continue;
        if (type < 0 || type >= (int)BLK_COUNT) continue;

        int out_idx = be->block_count;
        if (out_idx >= MAX_WORKSPACE_BLOCKS) break;
        BlockInstance* bi = &be->blocks[out_idx];
        bi->id = id;
        bi->type = (BlockType)type;
        bi->a = a;
        bi->b = b;
        bi->r = default_block_rect(x, y);
        be->block_count++;

        if (id >= max_id) max_id = id + 1;
    }

    // keep next_id consistent even if file is old
    if (be->next_id < max_id) be->next_id = max_id;

    std::fclose(f);
    return 1;
}
