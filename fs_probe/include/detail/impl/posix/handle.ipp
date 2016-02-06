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

#include <unistd.h>
#include <fcntl.h>
#if BOOST_AFIO_USE_POSIX_AIO
# include <aio.h>
#endif

BOOST_AFIO_V2_NAMESPACE_BEGIN

result<handle> handle::clone(io_service &service, handle::mode mode, handle::caching caching) const noexcept
{
  result<handle> ret(handle(&service, _path, native_handle_type(), _caching, _flags));
  ret.value()._v.behaviour = _v.behaviour;
  // If current handle is read-only and clone request is to add write powers, we can't use dup()
  if (mode != mode::unchanged && !_v.is_writable() && (mode==mode::write || mode==mode::append))
  {
    // Race free fetch the handle's path and reopen it with the new permissions
    // TODO FIXME
    return make_errored_result<handle>(ENOSYS);
  }
  else
  {
    if (-1 == (ret.value()._v.fd = ::dup(_v.fd)))
      return make_errored_result<handle>(errno);
    // Only care if cloning and changing append only flag
    if (mode != mode::unchanged && (mode==mode::write || mode==mode::append))
    {
      ret.value()._v.behaviour = _v.behaviour & ~(native_handle_type::disposition::seekable | native_handle_type::disposition::readable | native_handle_type::disposition::writable | native_handle_type::disposition::append_only);
      int attribs = 0;
      if (-1 == (attribs = fcntl(ret.value()._v.fd, F_GETFL)))
        return make_errored_result<handle>(errno);
      switch (mode)
      {
      case mode::unchanged:
        break;
      case mode::none:
      case mode::attr_read:
      case mode::attr_write:
      case mode::read:
        return make_errored_result<handle>(EINVAL);
      case mode::write:
        attribs&=~O_APPEND;
        ret.value()._v.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable| native_handle_type::disposition::writable;
        break;
      case mode::append:
        attribs |= O_APPEND;
        ret.value()._v.behaviour |= native_handle_type::disposition::append_only | native_handle_type::disposition::writable;
        break;
      }
      if(-1==fcntl(ret.value()._v.fd, F_SETFL, attribs))
        return make_errored_result<handle>(errno);
    }
    if (caching != caching::unchanged && caching != _caching)
    {
      // TODO: Allow fiddling with O_DIRECT
      return make_errored_result<handle>(EINVAL);
    }
  }
  return ret;
}

result<file_handle> file_handle::file(io_service &service, file_handle::path_type _path, file_handle::mode _mode, file_handle::creation _creation, file_handle::caching _caching, file_handle::flag flags) noexcept
{
  result<file_handle> ret(file_handle(&service, std::move(_path), native_handle_type(), _caching, flags));
  native_handle_type &nativeh = ret.get()._v;
  int attribs = 0;
  switch (_mode)
  {
  case mode::unchanged:
    return make_errored_result<file_handle>(EINVAL);
  case mode::none:
    break;
  case mode::attr_read:
  case mode::read:
    attribs = O_RDONLY;
    nativeh.behaviour |= native_handle_type::disposition::seekable|native_handle_type::disposition::readable;
    break;
  case mode::attr_write:
  case mode::write:
    attribs = O_RDWR;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable| native_handle_type::disposition::writable;
    break;
  case mode::append:
    attribs = O_APPEND;
    nativeh.behaviour |= native_handle_type::disposition::writable|native_handle_type::disposition::append_only;
    break;
  }
  switch (_creation)
  {
  case creation::open_existing:
    break;
  case creation::only_if_not_exist:
    attribs |= O_CREAT | O_EXCL;
    break;
  case creation::if_needed:
    attribs |= O_CREAT;
    break;
  case creation::truncate:
    attribs |= O_TRUNC;
    break;
  }
  nativeh.behaviour |= native_handle_type::disposition::file;
  switch (_caching)
  {
  case caching::unchanged:
    return make_errored_result<file_handle>(EINVAL);
  case caching::none:
    attribs |= O_SYNC | O_DIRECT;
    nativeh.behaviour |= native_handle_type::disposition::aligned_io;
    break;
  case caching::only_metadata:
    attribs |= O_DIRECT;
    nativeh.behaviour |= native_handle_type::disposition::aligned_io;
    break;
  case caching::reads:
    attribs |= O_SYNC;
    break;
  case caching::reads_and_metadata:
#ifdef O_DSYNC
    attribs |= O_DSYNC;
#else
    attribs |= O_SYNC;
#endif
    break;
  case caching::all:
  case caching::safety_fsyncs:
  case caching::temporary:
    break;
  }
  const char *path_=ret.value()._path.c_str();
  if (-1 == (nativeh.fd = ::open(path_, attribs, 0x1b0/*660*/)))
    return make_errored_result<file_handle>(errno);
  if (_creation == creation::truncate && ret.value().are_safety_fsyncs_issued())
    fsync(nativeh.fd);
  return ret;
}

handle::~handle()
{
  if (_v)
  {
    if(are_safety_fsyncs_issued())
    {
      fsync(_v.fd);
    }
    ::close(_v.fd);
    _v = native_handle_type();
  }
}

template<class CompletionRoutine, class BuffersType, class IORoutine> result<file_handle::io_state_ptr<CompletionRoutine, BuffersType>> file_handle::_begin_io(file_handle::operation_t operation, file_handle::io_request<BuffersType> reqs, CompletionRoutine &&completion, IORoutine &&ioroutine) noexcept
{
  // Need to keep a set of aiocbs matching the scatter-gather buffers
  struct state_type : public _io_state_type<CompletionRoutine, BuffersType>
  {
#if BOOST_AFIO_USE_POSIX_AIO
    struct aiocb aiocbs[1];
#else
#error todo
#endif
    state_type(handle *_parent, operation_t _operation, CompletionRoutine &&f, size_t _items) : _io_state_type<CompletionRoutine, BuffersType>(_parent, _operation, std::forward<CompletionRoutine>(f), _items) { }
    virtual void operator()(long errcode, long bytes_transferred, void *internal_state) noexcept override final
    {
#if BOOST_AFIO_USE_POSIX_AIO
      struct aiocb **_paiocb=(struct aiocb **) internal_state;
      struct aiocb *aiocb=*_paiocb;
      assert(aiocb>=aiocbs && aiocb<aiocbs+this->items);
      *_paiocb=nullptr;
#else
#error todo
#endif
      if (this->result)
      {
        if (errcode)
          this->result = make_errored_result<BuffersType>((int) errcode);
        else
        {
          // Figure out which i/o I am and update the buffer in question
#if BOOST_AFIO_USE_POSIX_AIO
          size_t idx = aiocb - aiocbs;
#else
#error todo
#endif
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
        this->completion(this);
    }
    virtual ~state_type() override final
    {
      // Do we need to cancel pending i/o?
      if (this->items_to_go)
      {
        for (size_t n = 0; n < this->items; n++)
        {
#if BOOST_AFIO_USE_POSIX_AIO
          int ret=aio_cancel(this->parent->native_handle().fd, aiocbs + n);
#if 0
          if(ret<0 || ret==AIO_NOTCANCELED)
          {
            std::cout << "Failed to cancel " << (aiocbs+n) << std::endl;
          }
          else if(ret==AIO_CANCELED)
          {
            std::cout << "Cancelled " << (aiocbs+n) << std::endl;
          }
          else if(ret==AIO_ALLDONE)
          {
            std::cout << "Already done " << (aiocbs+n) << std::endl;
          }
#endif
#else
#error todo
#endif
        }
        // Pump the i/o service until all pending i/o is completed
        while (this->items_to_go)
        {
          auto res=this->parent->service()->run();
#ifndef NDEBUG
          if(res.has_error())
          {
            BOOST_AFIO_LOG_FATAL_EXIT("file_handle: io_service failed due to '" << res.get_error().message() << "'");
            std::terminate();
          }
          if(!res.get())
          {
            BOOST_AFIO_LOG_FATAL_EXIT("file_handle: io_service returns no work when i/o has not completed");
            std::terminate();
          }
#endif
        }
      }
    }
  } *state;
  extent_type offset = reqs.offset;
  size_t statelen = sizeof(state_type) + (reqs.buffers.size() - 1)*sizeof(struct aiocb), items(reqs.buffers.size());
  using return_type = io_state_ptr<CompletionRoutine, BuffersType>;
#if BOOST_AFIO_USE_POSIX_AIO
  if(items>AIO_LISTIO_MAX)
    return make_errored_result<return_type>(EINVAL);
#endif
  void *mem = ::calloc(1, statelen);
  if (!mem)
    return make_errored_result<return_type>(ENOMEM);
  return_type _state((_io_state_type<CompletionRoutine, BuffersType> *) mem);
  new((state = (state_type *)mem)) state_type(this, operation, std::forward<CompletionRoutine>(completion), items);
  // Noexcept move the buffers from req into result
  BuffersType &out = state->result.value();
  out = std::move(reqs.buffers);
  for (size_t n = 0; n < items; n++)
  {
#if BOOST_AFIO_USE_POSIX_AIO
    struct aiocb *aiocb = state->aiocbs + n;
    aiocb->aio_fildes = _v.fd;
    aiocb->aio_offset = offset;
    aiocb->aio_buf = (void *) out[n].first;
    aiocb->aio_nbytes = out[n].second;
    aiocb->aio_sigevent.sigev_notify = SIGEV_NONE;
    aiocb->aio_sigevent.sigev_value.sival_ptr=(void *) state;
    aiocb->aio_lio_opcode = (operation==operation_t::write) ? LIO_WRITE : LIO_READ;
#else
#error todo
#endif
    offset += out[n].second;
    ++state->items_to_go;
  }
  int ret=0;
#if BOOST_AFIO_USE_POSIX_AIO
  if(service()->using_kqueues())
  {
# if BOOST_AFIO_COMPILE_KQUEUES
    // Only issue one kqueue event when entire scatter-gather has completed
    struct _sigev={0};
#error todo
#endif
  }
  else
  {
    // Add these i/o's to the quick aio_suspend list
    service()->_aiocbsv.resize(service()->_aiocbsv.size()+items);
    struct aiocb **thislist=service()->_aiocbsv.data()+service()->_aiocbsv.size()-items;    
    for (size_t n = 0; n < items; n++)
    {
      struct aiocb *aiocb = state->aiocbs + n;
      thislist[n]=aiocb;
    }    
    ret=lio_listio(LIO_NOWAIT, thislist, items, nullptr);
  }
#else
#error todo
#endif
  if(ret<0)
  {
    service()->_aiocbsv.resize(service()->_aiocbsv.size()-items);
    state->items_to_go=0;
    state->result = make_errored_result<BuffersType>(errno);
    state->completion(state);
    return make_result<return_type>(std::move(_state));
  }
  service()->_work_enqueued(items);
  return make_result<return_type>(std::move(_state));
}

template<class CompletionRoutine> result<file_handle::io_state_ptr<CompletionRoutine, file_handle::buffers_type>> file_handle::async_read(file_handle::io_request<file_handle::buffers_type> reqs, CompletionRoutine &&completion) noexcept
{
  return _begin_io(operation_t::read, std::move(reqs), [completion=std::forward<CompletionRoutine>(completion)](auto *state) {
    completion(state->parent, state->result);
  }, nullptr);
}

template<class CompletionRoutine> result<file_handle::io_state_ptr<CompletionRoutine, file_handle::const_buffers_type>> file_handle::async_write(file_handle::io_request<file_handle::const_buffers_type> reqs, CompletionRoutine &&completion) noexcept
{
  return _begin_io(operation_t::write, std::move(reqs), [completion = std::forward<CompletionRoutine>(completion)](auto *state) {
    completion(state->parent, state->result);
  }, nullptr);
}

file_handle::io_result<file_handle::buffers_type> file_handle::read(file_handle::io_request<file_handle::buffers_type> reqs, deadline d) noexcept
{
  io_result<buffers_type> ret;
  auto _io_state(_begin_io(operation_t::read, std::move(reqs), [&ret](auto *state) {
    ret = std::move(state->result);
  }, nullptr));
  BOOST_OUTCOME_FILTER_ERROR(io_state, _io_state);

  // While i/o is not done pump i/o completion
  while (!ret.is_ready())
  {
    auto t(_service->run_until(d));
    // If i/o service pump failed or timed out, cancel outstanding i/o and return
    if (!t)
      return make_errored_result<buffers_type>(t.get_error());
#ifndef NDEBUG
    if(!ret.is_ready() && t && !t.get())
    {
      BOOST_AFIO_LOG_FATAL_EXIT("file_handle: io_service returns no work when i/o has not completed");
      std::terminate();
    }
#endif
  }
  return ret;
}

file_handle::io_result<file_handle::const_buffers_type> file_handle::write(file_handle::io_request<file_handle::const_buffers_type> reqs, deadline d) noexcept
{
  io_result<const_buffers_type> ret;
  auto _io_state(_begin_io(operation_t::write, std::move(reqs), [&ret](auto *state) {
    ret = std::move(state->result);
  }, nullptr));
  BOOST_OUTCOME_FILTER_ERROR(io_state, _io_state);

  // While i/o is not done pump i/o completion
  while (!ret.is_ready())
  {
    auto t(_service->run_until(d));
    // If i/o service pump failed or timed out, cancel outstanding i/o and return
    if (!t)
      return make_errored_result<const_buffers_type>(t.get_error());
#ifndef NDEBUG
    if(!ret.is_ready() && t && !t.get())
    {
      BOOST_AFIO_LOG_FATAL_EXIT("file_handle: io_service returns no work when i/o has not completed");
      std::terminate();
    }
#endif
  }
  return ret;
}

result<file_handle::extent_type> file_handle::truncate(file_handle::extent_type newsize) noexcept
{
  if (ftruncate(_v.fd, newsize)<0)
    return make_errored_result<extent_type>(errno);
  if (are_safety_fsyncs_issued())
  {
    fsync(_v.fd);
  }
  return newsize;
}

BOOST_AFIO_V2_NAMESPACE_END
