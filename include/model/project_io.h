#pragma once
#include "model/model.h"

#ifdef __cplusplus
extern "C" {
#endif

    // Returns 1 on success, 0 on failure.
    // If err != nullptr, writes a message there (up to err_cap chars).
    int project_save_v2(const Project* p, const char* path, char* err, int err_cap);
    int project_load_v2(Project* p, const char* path, char* err, int err_cap);

#ifdef __cplusplus
}
#endif