#include "core/log.h"
#include "model/model.h"
#include "engine/runtime.h"
#include "ui/app.h"

int main() {
    log_init_stdout();

    Project project;
    Runtime runtime;

    model_init(&project);
    runtime_init(&runtime);

    return app_run(&project, &runtime) ? 0 : 1;
}