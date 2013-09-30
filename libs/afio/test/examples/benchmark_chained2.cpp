#include "boost/afio/afio.hpp"
#include <iostream>

/*  My Intel Core i7 3770K running Windows 8 x64: 596318 closures/sec
    My Intel Core i7 3770K running     Linux x64: 794384 closures/sec
*/

int main(void)
{
#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700) && !(defined(__GLIBCXX__) && __GLIBCXX__<=20120920 /* <= GCC 4.7 */)
	using namespace boost::afio;
    auto dispatcher=make_async_file_io_dispatcher();
	typedef std::chrono::duration<double, std::ratio<1>> secs_type;
	auto begin=std::chrono::high_resolution_clock::now();
	while(std::chrono::duration_cast<secs_type>(std::chrono::high_resolution_clock::now()-begin).count()<3);
	
	auto callback=std::function<int()>([]
	{
		return 1;
	});
	size_t threads=0;
	begin=std::chrono::high_resolution_clock::now();
#pragma omp parallel
	{
		async_io_op last;
		threads++;
		for(size_t n=0; n<500000; n++)
		{
			last=dispatcher->call(last, callback).second;
		}
	}
	while(dispatcher->wait_queue_depth())
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	auto end=std::chrono::high_resolution_clock::now();
	auto diff=std::chrono::duration_cast<secs_type>(end-begin);
	std::cout << "It took " << diff.count() << " secs to execute " << (500000*threads) << " closures which is " << (500000*threads/diff.count()) << " chained closures/sec" << std::endl;
	std::cout << "\nPress Return to exit ..." << std::endl;
	getchar();
#endif
	return 0;
}
