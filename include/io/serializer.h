#pragma once
#include "model/model.h"

#ifdef __cplusplus
extern "C" {
#endif

    // Simple, stable format (you can upgrade later)
    int save_project(const Project* p, const char* path);
    int load_project(Project* p, const char* path);

#ifdef __cplusplus
}
#endif