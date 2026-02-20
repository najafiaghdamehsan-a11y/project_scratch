#include "io/serializer.h"
#include <fstream>
#include <string>

int save_project(const Project* p, const char* path) {
    std::ofstream out(path);
    if (!out) return 0;

    out << "SPRITES " << p->sprite_count << "\n";
    out << "ACTIVE " << p->active_sprite_index << "\n";

    for (int i = 0; i < p->sprite_count; i++) {
        const Sprite& s = p->sprites[i];
        out << "S "
            << s.id << "|"
            << s.name << "|"
            << s.x << "|"
            << s.y << "|"
            << s.dir << "|"
            << s.size << "|"
            << s.visible
            << "\n";
    }
    return 1;
}

int load_project(Project* p, const char* path) {
    std::ifstream in(path);
    if (!in) return 0;

    model_init(p);

    std::string tag;
    in >> tag;
    if (tag != "SPRITES") return 0;
    in >> p->sprite_count;

    in >> tag;
    if (tag != "ACTIVE") return 0;
    in >> p->active_sprite_index;

    std::string line;
    std::getline(in, line); // consume endline

    p->sprite_count = (p->sprite_count > MAX_SPRITES) ? MAX_SPRITES : p->sprite_count;

    for (int i = 0; i < p->sprite_count; i++) {
        std::getline(in, line);
        if (line.rfind("S ", 0) != 0) return 0;

        std::string payload = line.substr(2);
        // quick split by '|'
        auto next = [&](size_t& pos) {
            size_t n = payload.find('|', pos);
            std::string part = (n == std::string::npos) ? payload.substr(pos) : payload.substr(pos, n - pos);
            pos = (n == std::string::npos) ? payload.size() : n + 1;
            return part;
        };

        size_t pos = 0;
        Sprite& s = p->sprites[i];
        s.id = std::stoull(next(pos));

        std::string nm = next(pos);
        std::snprintf(s.name, MAX_NAME, "%s", nm.c_str());

        s.x = std::stod(next(pos));
        s.y = std::stod(next(pos));
        s.dir = std::stod(next(pos));
        s.size = std::stod(next(pos));
        s.visible = std::stoi(next(pos));
    }
    return 1;
}