#include "test_functions.h"

#if 0
BOOST_AUTO_TEST_CASE(async_io_works_1_direct)
{
    BOOST_TEST_MESSAGE( "Tests that the direct async i/o implementation works");

    auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::OSDirect);
    std::cout << "\n\n1000 file opens, writes 1 byte, closes, and deletes with direct i/o:\n";
    _1000_open_write_close_deletes(dispatcher, 1);
}
#endif