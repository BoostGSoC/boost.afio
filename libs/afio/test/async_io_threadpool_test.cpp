/* Unit tests for TripleGit
(C) 2013 Niall Douglas http://www.nedprod.com/
Created: Feb 2013
*/

#include "test_functions.hpp"

 static int task()
{
   boost::afio::thread::id this_id = boost::afio::get_this_thread_id();
        std::cout << "I am worker thread " << this_id << std::endl;
        return 78;
}

 //I think the test setup in the jamfile will take care of the testuite without needing these macros
//BOOST_AUTO_TEST_SUITE(all)
//   BOOST_AUTO_TEST_SUITE(exclude_async_io_errors)

 
 BOOST_AUTO_TEST_CASE(async_io_thread_pool_works)
{
    BOOST_TEST_MESSAGE("Tests that the async i/o thread pool implementation works");
    using namespace boost::afio;
    
    boost::afio::thread::id this_id = boost::afio::get_this_thread_id();
    
        std::cout << "I am main thread " << this_id << std::endl;
        thread_pool pool(4);
        auto r=task();
        BOOST_CHECK(r==78);
        std::vector<future<int>> results(8);
        
        for(auto &i : results)
        {
            i=std::move(pool.enqueue(task));
        }
        
        std::vector<future<int>> results2;
        results2.push_back(pool.enqueue(task));
        results2.push_back(pool.enqueue(task));
        std::pair<size_t, int> allresults2=when_any(results2.begin(), results2.end()).get();
        BOOST_CHECK(allresults2.first<2);
        BOOST_CHECK(allresults2.second==78);
        std::vector<int> allresults=when_all(results.begin(), results.end()).get();
        
        for(int i : allresults)
        {
            BOOST_CHECK(i==78);
        }
}
     //BOOST_AUTO_TEST_SUITE_END() //end exclude_async_io_erros
//BOOST_AUTO_TEST_SUITE_END() // end all        
