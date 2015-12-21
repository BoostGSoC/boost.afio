/* storage_profile.hpp
A profile of an OS and filing system
(C) 2015 Niall Douglas http://www.nedprod.com/
File Created: Dec 2015
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

#include "../../storage_profile.hpp"

BOOST_AFIO_V2_NAMESPACE_BEGIN

namespace storage_profile {

  /* YAML's syntax is amazingly powerful ... we can express a map
  of a map to a map using this syntax:

  ?
  direct: 0
  sync: 0
  :
  concurrency:
  atomicity:
  min_atomic_write: 1
  max_atomic_write: 1

  Some YAML parsers appear to accept this more terse form too:

  {direct: 0, sync: 0}:
  concurrency:
  atomicity:
  min_atomic_write: 1
  max_atomic_write: 1

  We don't do any of this as some YAML parsers are basically JSON parsers with
  some rules relaxed. We just use:

  direct=0 sync=0:
  concurrency:
  atomicity:
  min_atomic_write: 1
  max_atomic_write: 1
  */
  void storage_profile::write(std::ostream &out, size_t _indent) const
  {
    std::vector<std::string> lastsection;
    auto print = [_indent, &out, &lastsection](auto &i) {
      size_t indent = _indent;
      if (i.value != default_value<decltype(i.value)>())
      {
        std::vector<std::string> thissection;
        const char *s, *e;
        for (s = i.name, e = i.name; *e; e++)
        {
          if (*e == ':')
          {
            thissection.push_back(std::string(s, e - s));
            s = e + 1;
          }
        }
        std::string name(s, e - s);
        for (size_t n = 0; n < thissection.size(); n++)
        {
          indent += 4;
          if (n >= lastsection.size() || thissection[n] != lastsection[n])
          {
            out << std::string(indent, ' ') << thissection[n] << ":\n";
          }
        }
        out << std::string(indent + 4, ' ') << name << ": " << i.value << "\n";
        lastsection = std::move(thissection);
      }
    };
    for (const item_erased &i : *this)
      i.invoke(print);
  }
}

namespace system
{
  // OS name, version
  outcome<void> os(storage_profile::storage_profile &sp, handle &h) noexcept;
  // CPU name, architecture, physical cores
  outcome<void> cpu(storage_profile::storage_profile &sp, handle &h) noexcept;
  // System memory quantity, in use, max and min bandwidth
  outcome<void> mem(storage_profile::storage_profile &sp, handle &h) noexcept;
}
namespace storage
{
  // Device name, size, min i/o size
  outcome<void> device(storage_profile::storage_profile &sp, handle &h) noexcept;
  // FS name, config, size, in use
  outcome<void> fs(storage_profile::storage_profile &sp, handle &h) noexcept;
}
namespace concurrency
{
  outcome<void> atomic_write_quantum(storage_profile::storage_profile &sp, handle &srch) noexcept
  {
    using off_t = io_service::extent_type;
    sp.max_atomic_write.value = 1;
    for (off_t size = srch.needs_aligned_io() ? 512 : 64; size <= 1 * 1024 * 1024 && size<sp.atomic_write_quantum.value; size = size * 2)
    {
      // Create two concurrent writer threads
      std::vector<std::thread> writers;
      std::atomic<size_t> done(2);
      for (char no = '1'; no <= '2'; no++)
        writers.push_back(std::thread([size, &srch, no, &done] {
        io_service service;
        auto _h(srch.clone(service, handle::mode::write));
        if (!_h)
        {
          std::cerr << "FATAL ERROR: Could not open work file due to " << _h.get_error().message() << std::endl;
          abort();
        }
        auto h(std::move(_h.get()));
        std::vector<char> buffer(size, no);
        handle::io_request<handle::const_buffers_type> reqs({ std::make_pair(buffer.data(), size) }, 0);
        // Preallocate space before testing
        h.truncate(size);
        h.write(reqs);
        --done;
        while (done)
          std::this_thread::yield();
        while (!done)
        {
          h.write(reqs);
        }
      }));
      while (done)
        std::this_thread::yield();
      // Repeatedly read from the file and check for torn writes
      io_service service;
      auto _h(srch.clone(service, handle::mode::read));
      if (!_h)
      {
        std::cerr << "FATAL ERROR: Could not open work file due to " << _h.get_error().message() << std::endl;
        abort();
      }
      auto h(std::move(_h.get()));
      std::vector<char> buffer(size, 0);
      handle::io_request<handle::buffers_type> reqs({ std::make_pair(buffer.data(), size) }, 0);
      bool failed = false;
      std::cout << "direct=" << srch.are_reads_from_cache() << " sync=" << srch.are_writes_durable() << " testing atomicity of writes of " << size << " bytes ..." << std::endl;
      for (size_t transitions = 0; transitions < 10000; transitions++)
      {
        h.read(reqs);
        const size_t *data = (size_t *)buffer.data(), *end = (size_t *)(buffer.data() + size);
        for (const size_t *d = data; d < end; d++)
        {
          if (*d != *data)
          {
            failed = true;
            off_t failedat = d - data;
            if (failedat < sp.atomic_write_quantum.value)
            {
              std::cout << "  Torn write at offset " << failedat << std::endl;
              sp.atomic_write_quantum.value = failedat;
            }
            break;
          }
        }
      }
      if (!failed)
      {
        if (size > sp.max_atomic_write.value)
          sp.max_atomic_write.value = size;
      }
      done = true;
      for (auto &writer : writers)
        writer.join();
      if (failed)
        break;
    }
    if (sp.atomic_write_quantum.value > sp.max_atomic_write.value)
      sp.atomic_write_quantum.value = sp.max_atomic_write.value;
  }
}

BOOST_AFIO_V2_NAMESPACE_END
