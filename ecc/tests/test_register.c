/*
 * ecc extension - Test registration aggregator
 * All test files are included here for single-compilation-unit build.
 */

/* Pure memory operation tests (no external dependency) */
#include "test_ecc_ce.c"
#include "test_ecc_ue.c"

/* ecc_inject simulation layer tests */
#include "test_ecc_inject_ce.c"
#include "test_ecc_inject_ue.c"
