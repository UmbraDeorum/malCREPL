#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

Cmd cmd = {0};

void cc(void)
{
    cmd_append(&cmd, "cc");
    cmd_append(&cmd, "-Wall");
    cmd_append(&cmd, "-Wextra");
    cmd_append(&cmd, "-Wno-unused-function");
    cmd_append(&cmd, "-ggdb");
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    cc();
    cmd_append(&cmd, "-o", "crepl");
    cmd_append(&cmd, "crepl.c");
    cmd_append(&cmd, "-lffi");
    if (!cmd_run(&cmd)) return 1;

    return 0;
}
