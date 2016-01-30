/* handle.hpp
A handle to a file
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

#include "../../../handle.hpp"
#include "import.hpp"

BOOST_AFIO_V2_NAMESPACE_BEGIN

result<handle> handle::clone(io_service &service, handle::mode mode, handle::caching caching) const noexcept
{
  result<handle> ret(handle(&service, _path, native_handle_type(), _caching, _flags));
  ret.value()._v.behaviour = _v.behaviour;
  DWORD access = SYNCHRONIZE;
  if (mode != mode::unchanged)
  {
    ret.value()._v.behaviour = _v.behaviour & ~(native_handle_type::disposition::readable | native_handle_type::disposition::writable | native_handle_type::disposition::append_only);
    switch (mode)
    {
    case mode::unchanged:
    case mode::none:
      break;
    case mode::attr_read:
      access |= FILE_READ_ATTRIBUTES;
      break;
    case mode::attr_write:
      access |= FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES;
      break;
    case mode::read:
      access |= GENERIC_READ;
      ret.value()._v.behaviour |= native_handle_type::disposition::readable;
      break;
    case mode::write:
      access |= GENERIC_WRITE | GENERIC_READ;
      ret.value()._v.behaviour |= native_handle_type::disposition::readable| native_handle_type::disposition::writable;
      break;
    case mode::append:
      access |= FILE_APPEND_DATA;
      ret.value()._v.behaviour |= native_handle_type::disposition::append_only | native_handle_type::disposition::writable;
      break;
    }
  }
  if (caching != caching::unchanged && caching != _caching)
  {
    return make_errored_result<handle>(EINVAL);
  }
  if (!DuplicateHandle(GetCurrentProcess(), _v.h, GetCurrentProcess(), &ret.value()._v.h, access, false, mode == mode::unchanged ? DUPLICATE_SAME_ACCESS : 0))
    return make_errored_result<handle>(GetLastError());
  return ret;
}


result<file_handle> file_handle::file(io_service &service, file_handle::path_type _path, file_handle::mode _mode, file_handle::creation _creation, file_handle::caching _caching, file_handle::flag flags) noexcept
{
  result<file_handle> ret(file_handle(&service, std::move(_path), native_handle_type(), _caching, flags));
  native_handle_type &nativeh = ret.get()._v;
  DWORD access = SYNCHRONIZE;
  switch (_mode)
  {
  case mode::unchanged:
    return make_errored_result<file_handle>(EINVAL);
  case mode::none:
    break;
  case mode::attr_read:
    access |= FILE_READ_ATTRIBUTES;
    break;
  case mode::attr_write:
    access |= FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES;
    break;
  case mode::read:
    access |= GENERIC_READ;
    nativeh.behaviour |= native_handle_type::disposition::seekable|native_handle_type::disposition::readable;
    break;
  case mode::write:
    access |= GENERIC_WRITE | GENERIC_READ;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable| native_handle_type::disposition::writable;
    break;
  case mode::append:
    access |= FILE_APPEND_DATA;
    nativeh.behaviour |= native_handle_type::disposition::writable|native_handle_type::disposition::append_only;
    break;
  }
  DWORD creation = OPEN_EXISTING;
  switch (_creation)
  {
  case creation::open_existing:
    break;
  case creation::only_if_not_exist:
    creation = CREATE_NEW;
    break;
  case creation::if_needed:
    creation = OPEN_ALWAYS;
    break;
  case creation::truncate:
    creation = TRUNCATE_EXISTING;
    break;
  }
  DWORD attribs = FILE_FLAG_OVERLAPPED;
  nativeh.behaviour |= native_handle_type::disposition::file | native_handle_type::disposition::overlapped;
  switch (_caching)
  {
  case caching::unchanged:
    return make_errored_result<file_handle>(EINVAL);
  case caching::none:
      attribs |= FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;
      nativeh.behaviour |= native_handle_type::disposition::aligned_io;
      break;
    case caching::only_metadata:
      attribs |= FILE_FLAG_NO_BUFFERING;
      nativeh.behaviour |= native_handle_type::disposition::aligned_io;
      break;
    case caching::reads:
    case caching::reads_and_metadata:
      attribs |= FILE_FLAG_WRITE_THROUGH;
      break;
    case caching::all:
    case caching::safety_fsyncs:
      break;
    case caching::temporary:
      attribs |= FILE_ATTRIBUTE_TEMPORARY;
      break;
  }
  if(flags && flag::delete_on_close)
    attribs |= FILE_FLAG_DELETE_ON_CLOSE;
  if (INVALID_HANDLE_VALUE == (nativeh.h = CreateFile(ret.value()._path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, creation, attribs, NULL)))
    return make_errored_result<file_handle>(GetLastError());
  if (_creation==creation::truncate && ret.value().are_safety_fsyncs_issued())
    FlushFileBuffers(nativeh.h);
  return ret;
}

handle::~handle()
{
  if (_v)
  {
    if(are_safety_fsyncs_issued())
    {
      FlushFileBuffers(_v.h);
    }
    CloseHandle(_v.h);
    _v = native_handle_type();
  }
}

template<class CompletionRoutine, class BuffersType, class IORoutine> result<file_handle::io_state_ptr<CompletionRoutine, BuffersType>> file_handle::_begin_io(file_handle::operation_t operation, file_handle::io_request<BuffersType> reqs, CompletionRoutine &&completion, IORoutine &&ioroutine) noexcept
{
  // Need to keep a set of OVERLAPPED matching the scatter-gather buffers
  struct state_type : public _io_state_type<CompletionRoutine, BuffersType>
  {
    OVERLAPPED ols[1];
    state_type(handle *_parent, operation_t _operation, CompletionRoutine &&f, size_t _items) : _io_state_type<CompletionRoutine, BuffersType>(_parent, _operation, std::forward<CompletionRoutine>(f), _items) { }
    virtual void operator()(long errcode, ssize_t bytes_transferred, void *internal_state) noexcept
    {
      LPOVERLAPPED ol=(LPOVERLAPPED) internal_state;
      ol->hEvent = nullptr;
      if (this->result)
      {
        if (errcode)
          this->result = make_errored_result<BuffersType>((DWORD) errcode);
        else
        {
          // Figure out which i/o I am and update the buffer in question
          size_t idx = ol - ols;
          if(idx>=this->items)
          {
            BOOST_AFIO_LOG_FATAL_EXIT("file_handle::io_state::operator() called with invalid index " << idx);
            std::terminate();
          }
          this->result.value()[idx].second = bytes_transferred;
        }
      }
      this->parent->service()->_work_done();
      // Are we done?
      if (!--this->items_to_go)
        this->completion(this->state);
    }
    virtual ~state_type() override final
    {
      // Do we need to cancel pending i/o?
      if (this->items_to_go)
      {
        for (size_t n = 0; n < this->items; n++)
        {
          // If this is non-zero, probably this i/o still in flight
          if (ols[n].hEvent)
            CancelIoEx(this->parent->native_handle().h, ols + n);
        }
        // Pump the i/o service until all pending i/o is completed
        while (this->items_to_go)
          this->parent->service()->run();
      }
    }
  } *state;
  extent_type offset = reqs.offset;
  size_t statelen = sizeof(state_type) + (reqs.buffers.size() - 1)*sizeof(OVERLAPPED), items(reqs.buffers.size());
  using return_type = io_state_ptr<CompletionRoutine, BuffersType>;
  // On Windows i/o must be scheduled on the same thread pumping completion
  if (GetCurrentThreadId() != service()->_threadid)
    return make_errored_result<return_type>(EOPNOTSUPP);
  void *mem = ::calloc(1, statelen);
  if (!mem)
    return make_errored_result<return_type>(ENOMEM);
  return_type _state((_io_state_type<CompletionRoutine, BuffersType> *) mem);
  new((state = (state_type *)mem)) state_type(this, operation, std::forward<CompletionRoutine>(completion), items);
  // To be called once each buffer is read
  struct handle_completion
  {
    static VOID CALLBACK Do(DWORD errcode, DWORD bytes_transferred, LPOVERLAPPED ol)
    {
      state_type *state = (state_type *)ol->hEvent;
      (*state)(errcode, bytes_transferred, ol);
    }
  };
  // Noexcept move the buffers from req into result
  BuffersType &out = state->result.value();
  out = std::move(reqs.buffers);
  for (size_t n = 0; n < items; n++)
  {
    LPOVERLAPPED ol = state->ols + n;
    ol->Internal = (ULONG_PTR)-1;
    ol->Offset = offset & 0xffffffff;
    ol->OffsetHigh = (offset >> 32) & 0xffffffff;
    // Use the unused hEvent member to pass through the state
    ol->hEvent = (HANDLE)state;
    offset += out[n].second;
    ++state->items_to_go;
    if (!ioroutine(_v.h, out[n].first, (DWORD)out[n].second, ol, handle_completion::Do))
    {
      --state->items_to_go;
      state->result = make_errored_result<BuffersType>(GetLastError());
      // Fire completion now if we didn't schedule anything
      if (!n)
        state->completion(state);
      return make_result<return_type>(std::move(_state));
    }
    service()->_work_enqueued();
  }
  return make_result<return_type>(std::move(_state));
}

template<class CompletionRoutine> result<file_handle::io_state_ptr<CompletionRoutine, file_handle::buffers_type>> file_handle::async_read(file_handle::io_request<file_handle::buffers_type> reqs, CompletionRoutine &&completion) noexcept
{
  return _begin_io(operation_t::read, std::move(reqs), [completion=std::forward<CompletionRoutine>(completion)](auto *state) {
    completion(state->parent, state->result);
  }, ReadFileEx);
}

template<class CompletionRoutine> result<file_handle::io_state_ptr<CompletionRoutine, file_handle::const_buffers_type>> file_handle::async_write(file_handle::io_request<file_handle::const_buffers_type> reqs, CompletionRoutine &&completion) noexcept
{
  return _begin_io(operation_t::write, std::move(reqs), [completion = std::forward<CompletionRoutine>(completion)](auto *state) {
    completion(state->parent, state->result);
  }, WriteFileEx);
}

file_handle::io_result<file_handle::buffers_type> file_handle::read(file_handle::io_request<file_handle::buffers_type> reqs, deadline d) noexcept
{
  io_result<buffers_type> ret;
  auto _io_state(_begin_io(operation_t::read, std::move(reqs), [&ret](auto *state) {
    ret = std::move(state->result);
  }, ReadFileEx));
  BOOST_OUTCOME_FILTER_ERROR(io_state, _io_state);

  // While i/o is not done pump i/o completion
  while (!ret)
  {
    auto t(_service->run_until(d));
    // If i/o service pump failed or timed out, cancel outstanding i/o and return
    if (!t)
      return make_errored_result<buffers_type>(t.get_error());
  }
  return ret;
}

file_handle::io_result<file_handle::const_buffers_type> file_handle::write(file_handle::io_request<file_handle::const_buffers_type> reqs, deadline d) noexcept
{
  io_result<const_buffers_type> ret;
  auto _io_state(_begin_io(operation_t::write, std::move(reqs), [&ret](auto *state) {
    ret = std::move(state->result);
  }, WriteFileEx));
  BOOST_OUTCOME_FILTER_ERROR(io_state, _io_state);

  // While i/o is not done pump i/o completion
  while (!ret)
  {
    auto t(_service->run_until(d));
    // If i/o service pump failed or timed out, cancel outstanding i/o and return
    if (!t)
      return make_errored_result<const_buffers_type>(t.get_error());
  }
  return ret;
}

result<file_handle::extent_type> file_handle::truncate(file_handle::extent_type newsize) noexcept
{
  FILE_END_OF_FILE_INFO feofi;
  feofi.EndOfFile.QuadPart = newsize;
  if (!SetFileInformationByHandle(_v.h, FileEndOfFileInfo, &feofi, sizeof(feofi)))
    return make_errored_result<extent_type>(GetLastError());
  if (are_safety_fsyncs_issued())
  {
    FlushFileBuffers(_v.h);
  }
  return newsize;
}

BOOST_AFIO_V2_NAMESPACE_END
