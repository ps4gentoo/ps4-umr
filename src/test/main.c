#include "test_framework.h"

DECLARE_TESTS(mmio_tests);
DECLARE_TESTS(vm_tests);
DECLARE_TESTS(server_tests);

int main(int argc, char **argv)
{
    struct global_config global_config;

    REGISTER_TESTS(mmio_tests);
    REGISTER_TESTS(vm_tests);
    REGISTER_TESTS(server_tests);

    if (1 < argc) {
        global_config.envdef_base_dir = argv[1];
        global_config.verbose = 0;
        run_tests(&global_config);
    } else {
        fprintf(stderr, "[ERROR]: %s requires one parameter\n", argv[0]);
        return EXIT_FAILURE;
    }

}
