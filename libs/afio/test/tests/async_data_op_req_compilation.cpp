#include "../test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_data_op_req_compilation, "Tests that all the use cases for async_data_op_req compile", 3)
{
	using namespace boost::afio;
	// Note that this test is mainly for testing metaprogramming compilation.
    auto dispatcher=boost::afio::make_async_file_io_dispatcher();
    auto mkdir(dispatcher->dir(async_path_op_req("testdir", file_flags::Create)));
    auto mkfile(dispatcher->file(async_path_op_req(mkdir, "testdir/foo", file_flags::Create|file_flags::ReadWrite)));
	auto last(dispatcher->truncate(mkfile, 256));
	char buffer[256];
	size_t length=sizeof(buffer);

	// Base void * specialisation
	{
		typedef void type;
		typedef const type const_type;

		type *out=buffer;
		// works
		last=dispatcher->write(async_data_op_req<const_type>(last, out, length, 0));
		// auto-consts
		last=dispatcher->write(async_data_op_req<type>(last, out, length, 0));
		// works
		last=dispatcher->read(async_data_op_req<type>(last, out, length, 0));
		// deduces
		last=dispatcher->write(make_async_data_op_req(last, out, length, 0));
	}
	// char * specialisation
	{
		typedef char type;
		typedef const type const_type;

		type *out=buffer;
		// works
		last=dispatcher->write(async_data_op_req<const_type>(last, out, length, 0));
		// auto-consts
		last=dispatcher->write(async_data_op_req<type>(last, out, length, 0));
		// works
		last=dispatcher->read(async_data_op_req<type>(last, out, length, 0));
		// deduces
		last=dispatcher->write(make_async_data_op_req(last, out, length, 0));
	}
	// char array specialisation
	{
		typedef char type;
		typedef const type const_type;

		auto &out=buffer;
		// works
		last=dispatcher->write(async_data_op_req<const_type>(last, out, 0));
		// auto-consts
		last=dispatcher->write(async_data_op_req<type>(last, out, 0));
		// works
		last=dispatcher->read(async_data_op_req<type>(last, out, 0));
		// deduces
		last=dispatcher->write(make_async_data_op_req(last, out, 0));
	}
	// Arbitrary integral type array specialisation
	{
		typedef wchar_t type;
		typedef const type const_type;

		wchar_t out[sizeof(buffer)/sizeof(wchar_t)];
		// works
		last=dispatcher->write(async_data_op_req<const_type>(last, out, 0));
		// auto-consts
		last=dispatcher->write(async_data_op_req<type>(last, out, 0));
		// works
		last=dispatcher->read(async_data_op_req<type>(last, out, 0));
		// deduces
		last=dispatcher->write(make_async_data_op_req(last, out, 0));
	}
	// vector specialisation
	{
		typedef std::vector<char> type;
		typedef const type const_type;

		type out(sizeof(buffer)/sizeof(type::value_type));
		// works
		last=dispatcher->write(async_data_op_req<const_type>(last, out, 0));
		// auto-consts
		last=dispatcher->write(async_data_op_req<type>(last, out, 0));
		// works
		last=dispatcher->read(async_data_op_req<type>(last, out, 0));
		// deduces
		last=dispatcher->write(make_async_data_op_req(last, out, 0));
	}
	// array specialisation
	{
		typedef std::array<char, sizeof(buffer)> type;
		typedef const type const_type;

		type out;
		// works
		last=dispatcher->write(async_data_op_req<const_type>(last, out, 0));
		// auto-consts
		last=dispatcher->write(async_data_op_req<type>(last, out, 0));
		// works
		last=dispatcher->read(async_data_op_req<type>(last, out, 0));
		// deduces
		last=dispatcher->write(make_async_data_op_req(last, out, 0));
	}
	// string specialisation
	{
		typedef std::string type;
		typedef const type const_type;

		type out(sizeof(buffer)/sizeof(type::value_type), ' ');
		// works
		last=dispatcher->write(async_data_op_req<const_type>(last, out, 0));
		// auto-consts
		last=dispatcher->write(async_data_op_req<type>(last, out, 0));
		// works
		last=dispatcher->read(async_data_op_req<type>(last, out, 0));
		// deduces
		last=dispatcher->write(make_async_data_op_req(last, out, 0));
	}
	last=dispatcher->close(last);
    auto rmfile(dispatcher->rmfile(async_path_op_req(last, "testdir/foo")));
    auto rmdir(dispatcher->rmdir(async_path_op_req(rmfile, "testdir")));
	when_all(rmdir).wait();
	BOOST_CHECK(true);
}