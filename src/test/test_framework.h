#include "umr.h"

/** A Testing Framework for UMR
 * 
 * This framework is something like GoogleTest on Quaaludes (i.e. the opposite of steroids).
 * 
 * To define new tests, follow this recipe:
 * 1. In a C file, write each test in a function that matches the signature of test_func
 * 2. List the tests at the end of the C file using a DEFINE_TESTS block, e.g.:
 *      DEFINE_TESTS(name_of_test_collection)
 *      TEST(test1, <envdef(see below)>, <asic name(see below)>),
 *      TEST(TEST2)
 *      END_TESTS(name_of_test_collection)
 * 3. Add a DECLARE_TESTS statement to main.c
 * 4. Add a call to REGISTER_TESTS() to main().
 * 
 * 
*/

enum TEST_RESULT
{
    TEST_SUCCESS,
    TEST_INTERNAL_ERROR,
    TEST_NONFATAL_FAIL,
    TEST_FATAL_FAIL
};

typedef enum TEST_RESULT (*test_func) (struct umr_asic*);

/** ASSERT_XXX()
 * Use these macros to test various conditions in your test functions.
 * They will immediately abort the test and print an informative message if the
 * given predicate does not hold.
*/
#define ASSERT_EQ(_x, _y) if ((_x) != (_y)) {fprintf(stderr, "%s:%d: Assertion failed: %s != %s\n", __FILE__, __LINE__, #_x, #_y); return TEST_FATAL_FAIL;}
#define ASSERT_NOT_NULL(_x) if (!(_x)) {fprintf(stderr, "%s:%d: Assertion failed: %s is NULL\n", __FILE__, __LINE__, #_x); return TEST_FATAL_FAIL;}
#define ASSERT_STR_EQ(_x, _y) if (strcmp((_x),(_y))) {fprintf(stderr, "%s:%d: Assertion failed: '%s' != '%s'\n", __FILE__, __LINE__, #_x, #_y); return TEST_FATAL_FAIL;}
#define ASSERT_SUCCESS(_x) if (_x < 0) {fprintf(stderr, "%s:%d: Assertion failed: %s failed\n", __FILE__, __LINE__, #_x); return TEST_FATAL_FAIL;}
#define ASSERT_FAILURE(_x) if (_x >= 0) {fprintf(stderr, "%s:%d: Assertion failed: %s failed\n", __FILE__, __LINE__, #_x); return TEST_FATAL_FAIL;}


/**DEFINE_TESTS()/END_TESTS()
 * Define a group of tests. Wrap multiple calls to TEST() with these two macros to group the 
 * tests together and make it possible to register and run them.
 * 
 * @param _tablename and arbitrary variable name to refer to the group of tests by.
*/
#define DEFINE_TESTS(_tablename) struct test_table_entry _tablename[] = {
#define END_TESTS(_tablename) }; size_t _tablename##_size = sizeof(_tablename)/sizeof(_tablename[0])

/**DECLARE_TESTS()
 * Make the name of a group of tests visible from another file. Use this macro to make the name 
 * visible in e.g. main() so you can call REGISTER_TESTS() on it.
 * 
 * @param _tablename the name that was passed to DEFINE_TESTS
*/
#define DECLARE_TESTS(_tablename) extern struct test_table_entry _tablename[]; extern size_t _tablename##_size

/**REGISTER_TESTS()
 * Register a group of tests. Call this function from main() for every test group that was defined
 * using DEFINE_TESTS.
 * Note that the name of the test group needs to be visible in main() - you can make it visible 
 * using the DECLARE_TESTS() macro.
 * @param _tablename the name that was used in the DEFINE_TEST call that defined the test group.
*/
#define REGISTER_TESTS(_tablename) register_tests(_tablename, _tablename##_size)

/* TEST()
 * The TEST() macro links a test function with a simulated environment. The environment includes
 * the name of the simulated ASIC as well as simulated memory and HW state. The memory and HW
 * state are defined in an external file (see the demo/test_harness or test_inputs directories).
 * This way the same test function can be run on different ASICs, or on the same ASIC in different
 * system configurations.
 * 
 * This macro should only be used in a DEFINE_TESTS/END_TESTS block.
 * 
 * @param _t the name of the test function
 * @param _fname the relative (to the path in the global config) of the .th file that contains 
 *               the ASIC/memory configuarion.
 * @param _asicname the name of the ASIC to simulate
 */
#define TEST(_t, _fname, _asicname) {&_t, #_t, {_fname, _asicname}}

struct global_config
{
    char* envdef_base_dir;
    int   verbose;
};

/**run_tests()
 * Run all the tests that were registered. 
 * 
 * @param config the global configuration that will be used by all the tests.
*/
void run_tests(struct global_config* config);

//****DO NOT WORRY YOUR PRETTY LITTLE HEAD ABOUT ANYTHING BELOW THIS LINE******//

struct test_config
{
    char* envdef_file;
    char* asic_name;
};

struct test_table_entry
{
    test_func func;
    char* test_name;
    struct test_config config;
};

void register_tests(struct test_table_entry*, size_t ntests);
