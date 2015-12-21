/* fs_probe.cpp
Probes the OS and filing system for various characteristics
(C) 2015 Niall Douglas http://www.nedprod.com/
File Created: Nov 2015


Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include "include/handle.hpp"
#include "include/storage_profile.hpp"

#include <fstream>
#include <iomanip>
#include <regex>

using namespace BOOST_AFIO_V2_NAMESPACE;

constexpr unsigned permute_flags_max = 4;

static storage_profile::storage_profile profile[permute_flags_max];

int main(int argc, char *argv[])
{
  std::regex torun(".*");
  bool regexvalid = false;
  unsigned torunflags = permute_flags_max-1;
  if (argc > 1)
  {
    try
    {
      torun.assign(argv[1]);
      regexvalid = true;
    }
    catch (...) {}
    if (argc > 2)
      torunflags = atoi(argv[2]);
    if (!regexvalid)
    {
      std::cerr << "Usage: " << argv[0] << " <regex for tests to run> [<flags>]" << std::endl;
      return 1;
    }
  }

  std::ofstream results("fs_probe_results.yaml", std::ios::app);
  {
    std::time_t t = std::time(nullptr);
    results << "---\ntimestamp: " << std::put_time(std::gmtime(&t), "%F %T %z") << std::endl;
  }
  for (unsigned flags = 0; flags <= torunflags; flags++)
  {
    if (!flags || !!(flags & torunflags))
    {
      io_service service;
      handle::caching strategy=handle::caching::write_later;
      switch (flags)
      {
      case 1:
        strategy = handle::caching::metadata;
        break;
      case 2:
        strategy = handle::caching::reads;
        break;
      case 3:
        strategy = handle::caching::none;
        break;
      }
      auto _testfile(handle::create(service, "test", handle::mode::write, handle::creation::if_needed, strategy));
      if (!_testfile)
      {
        std::cerr << "WARNING: Failed to create test file due to '" << _testfile.get_error().message() << "', skipping" << std::endl;
        continue;
      }
      handle testfile(std::move(_testfile.get()));
      for (auto &test : profile[flags])
      {
        if (std::regex_match(test.name, torun))
        {
          auto result = test(profile[flags]);
          if (!result)
          {
            std::cerr << "ERROR running test '" << test.name << "': " << result.get_error().message() << std::endl;
          }
        }
      }
      // Write out results for this combination of flags
      std::cout << "\ndirect=" << !!(flags & handle::flag_direct) << " sync=" << !!(flags & handle::flag_sync) << ":\n";
      profile[flags].write(std::cout, 0);
      std::cout.flush();
      results << "direct=" << !!(flags & handle::flag_direct) << " sync=" << !!(flags & handle::flag_sync) << ":\n";
      profile[flags].write(results, 0);
      results.flush();
    }
  }

  return 0;
}