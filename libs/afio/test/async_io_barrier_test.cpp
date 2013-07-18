#include "test_functions.h"

// From http://burtleburtle.net/bob/rand/smallprng.html
typedef unsigned int  u4;
typedef struct ranctx { u4 a; u4 b; u4 c; u4 d; } ranctx;

#define rot(x,k) (((x)<<(k))|((x)>>(32-(k))))
u4 ranval(ranctx *x) {
    u4 e = x->a - rot(x->b, 27);
    x->a = x->b ^ rot(x->c, 17);
    x->b = x->c + x->d;
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}

void raninit(ranctx *x, u4 seed) {
    u4 i;
    x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
    for (i = 0; i < 20; ++i) {
        (void) ranval(x);
    }
}

BOOST_AUTO_TEST_CASE(async_io_barrier)
{
    BOOST_TEST_MESSAGE("Tests that the async i/o barrier works correctly under load");
    using namespace boost::afio;
    using namespace std;
    using boost::afio::future;
    using namespace boost::afio::detail;
    using boost::afio::off_t;
    typedef std::chrono::duration<double, ratio<1>> secs_type;
    vector<pair<size_t, int>> groups;
    // Generate 100,000 sorted random numbers between 0-1000
    {
        ranctx gen;
        raninit(&gen, 0x78adbcff);
        vector<int> manynumbers;
        manynumbers.reserve(100000);
        for (size_t n = 0; n < 100000; n++)
            manynumbers.push_back(ranval(&gen) % 1000);
        sort(manynumbers.begin(), manynumbers.end());

        // Collapse into a collection of runs of the same number
        int lastnumber = -1;
        for (auto &i : manynumbers)
        {
            if (i != lastnumber)
                groups.push_back(make_pair(0, i));
            groups.back().first++;
            lastnumber = i;
        }
    }
    atomic<size_t> callcount[1000];
    memset(&callcount, 0, sizeof(callcount));
    vector<future<bool>> verifies;
    verifies.reserve(groups.size());
    auto inccount = [](atomic<size_t> *count){ for (volatile size_t n = 0; n < 10000; n++); (*count)++; };
    auto verifybarrier = [](atomic<size_t> *count, size_t shouldbe)
    {
        if (*count != shouldbe)
        {
            BOOST_CHECK((*count == shouldbe));
            throw runtime_error("Count was not what it should have been!");
        }
        return true;
    };
    // For each of those runs, dispatch ops and a barrier for them
    auto dispatcher = async_file_io_dispatcher();
    auto begin = std::chrono::high_resolution_clock::now();
    size_t opscount = 0;
    async_io_op next;
    bool isfirst = true;
    for (auto &run : groups)
    {
        vector<function<void()>> thisgroupcalls(run.first, bind(inccount, &callcount[run.second]));
        vector<async_io_op> thisgroupcallops;
        if (isfirst)
        {
            thisgroupcallops = dispatcher->call(thisgroupcalls).second;
            isfirst = false;
        }
        else
        {
            vector<async_io_op> dependency(run.first, next);
            thisgroupcallops = dispatcher->call(dependency, thisgroupcalls).second;
        }
        auto thisgroupbarriered = dispatcher->barrier(thisgroupcallops);
        auto verify = dispatcher->call(thisgroupbarriered.front(), std::function<bool()>(std::bind(verifybarrier, &callcount[run.second], run.first)));
        verifies.push_back(std::move(verify.first));
        next = verify.second;
        opscount += run.first + 2;
    }
    auto dispatched = chrono::high_resolution_clock::now();
    cout << "There are now " << dec << dispatcher->count() << " handles open with a queue depth of " << dispatcher->wait_queue_depth() << endl;
    BOOST_CHECK_NO_THROW(when_all(next).wait());
    // Retrieve any errors
    for (auto &i : verifies)
    {
        BOOST_CHECK_NO_THROW(i.get());
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto diff = chrono::duration_cast<secs_type>(end - begin);
    cout << "It took " << diff.count() << " secs to do " << opscount << " operations" << endl;
    diff = chrono::duration_cast<secs_type>(dispatched - begin);
    cout << "  It took " << diff.count() << " secs to dispatch all operations" << endl;
    diff = chrono::duration_cast<secs_type>(end - dispatched);
    cout << "  It took " << diff.count() << " secs to finish all operations" << endl << endl;
    diff = chrono::duration_cast<secs_type>(end - begin);
    cout << "That's a throughput of " << opscount / diff.count() << " ops/sec" << endl;
}