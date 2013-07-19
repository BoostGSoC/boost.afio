
#define BOOST_TEST_MODULE tester

#include "test_functions.h"

BOOST_AUTO_TEST_SUITE(all)
#include "async_io_threadpool_test.cpp"
#include "async_io_works_1_prime_test.cpp"
#include "async_io_works_1_test.cpp"
#include "async_io_works_64_test.cpp"
#include "async_io_works_1_sync_test.cpp"
#include "async_io_works_64_sync_test.cpp"
#include "async_io_works_1_autoflush_test.cpp"
#include "async_io_works_64_autoflush_test.cpp"
#include "async_io_works_1_direct_test.cpp"
#include "async_io_works_64_direct_test.cpp"
#include "async_io_works_1_directsync_test.cpp"
#include "async_io_works_64_directsync_test.cpp"
#include "async_io_barrier_test.cpp"
#include "async_io_errors_test.cpp"
#include "async_io_torture_test.cpp"
#include "async_io_torture_sync_test.cpp"
#include "async_io_torture_autoflush_test.cpp"
#include "async_io_torture_direct_test.cpp"
#include "async_io_torture_direct_sync_test.cpp"
#include "async_io_sync_test.cpp"
BOOST_AUTO_TEST_SUITE_END()

