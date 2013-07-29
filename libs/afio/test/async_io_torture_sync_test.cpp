#include "test_functions.hpp"

BOOST_AUTO_TEST_CASE(async_io_torture_sync)
{
    BOOST_TEST_MESSAGE("Tortures the synchronous async i/o implementation");
    auto dispatcher = boost::afio::make_async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::AlwaysSync);
    std::cout << "\n\nSustained random synchronous i/o to 10 files of 1Mb:\n";
    evil_random_io(dispatcher, 10, 1 * 1024 * 1024);
}