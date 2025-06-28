#include "test_framework.h"
#include <stdio.h>
#include <stdarg.h>

struct registered_tests
{
    struct test_table_entry* tests;
    size_t ntests;
    struct registered_tests* next_block;
};

static struct registered_tests* registered_tests = NULL;

static void error(char* str)
{
    fputs(str, stderr);
}

static int vm_printf(const char *fmt, ...)
{
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vfprintf(stderr, fmt, ap);
	fflush(stderr);
	va_end(ap);
	return r;
}

void register_tests(struct test_table_entry* tests, size_t ntests)
{
    struct registered_tests** ptr_to_update = &registered_tests;

    while (*ptr_to_update != NULL)
    {
        ptr_to_update = &((*ptr_to_update)->next_block);
    }

    (*ptr_to_update) = calloc(1, sizeof(struct registered_tests));
    (*ptr_to_update)->ntests = ntests;
    (*ptr_to_update)->tests = tests;
}

static enum TEST_RESULT run_test(struct global_config* global_config, struct test_config* test_config, test_func func)
{
	struct umr_test_harness *th;
    struct umr_options options;
    struct umr_asic* asic;
    int test_ret;

    size_t envdef_path_len = strlen(test_config->envdef_file)+strlen(global_config->envdef_base_dir)+1/*connecting slash*/+1/*terminating '\0'*/;
    char* envdef_full_path = (char*)calloc(1,envdef_path_len);

    if (!envdef_full_path)
    {
        error("Out of memory");
        return TEST_INTERNAL_ERROR;
    }

    snprintf(envdef_full_path, envdef_path_len, "%s/%s", global_config->envdef_base_dir, test_config->envdef_file);

    th = umr_create_test_harness_file(envdef_full_path);
    if (!th)
    {
        error("Failed to load environment definition file");
        free(envdef_full_path);
        return TEST_INTERNAL_ERROR;
    }

    memset(&options, 0, sizeof(options));
    options.verbose = global_config->verbose;
    options.is_virtual = 1;
    options.force_asic_file = 1;
    asic = umr_discover_asic_by_name(&options, test_config->asic_name, vm_printf);
    asic->options.verbose = global_config->verbose;
    asic->std_msg = vm_printf;

    umr_attach_test_harness(th, asic);

    test_ret = func(asic);

	umr_free_test_harness(th);
    umr_close_asic(asic);
    free(envdef_full_path);

    return test_ret;
}

void run_tests(struct global_config* global_config)
{
    int result;
    int success = 0, fail = 0;
    struct registered_tests* test_block = registered_tests;

    while (test_block != NULL)
    {
        for (size_t i = 0; i < test_block->ntests; ++i)
        {
            result = run_test(global_config, &test_block->tests[i].config, test_block->tests[i].func);
            printf("[%s] %s\n", (result == TEST_SUCCESS)?"PASS":"FAIL", test_block->tests[i].test_name);
            if(result == TEST_SUCCESS)
            {
                ++success;
            }
            else
            {
                ++fail;
            }            
        }
        test_block = test_block->next_block;
    }

    printf("---\n%d Passed. %d Failed\n", success, fail);
    if (fail) {
	exit(EXIT_FAILURE);
    } else {
	exit(EXIT_SUCCESS);
    }
}
