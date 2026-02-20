#include "core/log.h"
#include "model/model.h"
#include "engine/runtime.h"
#include "ui/app.h"
#include "io/serializer.h"

int main() {
    log_init_stdout();

    Project project;
    Runtime runtime;

    model_init(&project);
    runtime_init(&runtime);

    // quick save/load smoke test (so IO is “real” from day 1)
    save_project(&project, "autosave.txt");
    load_project(&project, "autosave.txt");

    return app_run(&project, &runtime) ? 0 : 1;
}