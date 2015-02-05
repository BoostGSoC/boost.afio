/*
 * File:   Undoer.hpp
 * Author: Niall Douglas
 *(C) 2012 Niall Douglas http://www.nedprod.com/
 * Created on June 25, 2013, 11:35 AM


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

/*! \file Undoer.hpp
\brief Declares Undoer class and implementation
*/

#include <utility>

BOOST_AFIO_V1_NAMESPACE_BEGIN
namespace detail
{

  namespace Impl
  {
    template <typename T, bool iscomparable> struct is_nullptr
    {
      bool operator()(T c) const BOOST_NOEXCEPT_OR_NOTHROW { return !c; }
    };
    template <typename T> struct is_nullptr<T, false>
    {
      bool operator()(T) const BOOST_NOEXCEPT_OR_NOTHROW { return false; }
    };
  }
//! Compile-time safe detector of if \em v is nullptr (can cope with non-pointer convertibles)
#if defined(__GNUC__) && (BOOST_GCC < 41000 || defined(__MINGW32__))
  template <typename T> bool is_nullptr(T v) BOOST_NOEXCEPT_OR_NOTHROW { return Impl::is_nullptr<T, std::is_constructible<bool, T>::value>()(std::forward<T>(v)); }
#else
  template <typename T> bool is_nullptr(T v) BOOST_NOEXCEPT_OR_NOTHROW { return Impl::is_nullptr<T, std::is_trivially_constructible<bool, T>::value>()(std::forward<T>(v)); }
#endif


  template <typename callable> class UndoerImpl
  {
    bool _dismissed;
    callable undoer;
    UndoerImpl() = delete;
    UndoerImpl(const UndoerImpl &) = delete;
    UndoerImpl &operator=(const UndoerImpl &) = delete;
    explicit UndoerImpl(callable &&c) : _dismissed(false), undoer(std::move(c)) {}
    void int_trigger()
    {
      if(!_dismissed && !is_nullptr(undoer))
      {
        undoer();
        _dismissed = true;
      }
    }

  public:
    UndoerImpl(UndoerImpl &&o) : _dismissed(o._dismissed), undoer(std::move(o.undoer)) { o._dismissed = true; }
    UndoerImpl &operator=(UndoerImpl &&o)
    {
      int_trigger();
      _dismissed = o._dismissed;
      undoer = std::move(o.undoer);
      o._dismissed = true;
      return *this;
    }
    template <typename _callable> friend UndoerImpl<_callable> Undoer(_callable c);
    ~UndoerImpl() { int_trigger(); }
    //! Returns if the Undoer is dismissed
    bool dismissed() const { return _dismissed; }
    //! Dismisses the Undoer
    void dismiss(bool d = true) { _dismissed = d; }
    //! Undismisses the Undoer
    void undismiss(bool d = true) { _dismissed = !d; }
  };  // UndoerImpl


  /*! \brief Alexandrescu style rollbacks, a la C++ 11.

  Example of usage:
  \code
  auto resetpos=Undoer([&s]() { s.seekg(0, std::ios::beg); });
  ...
  resetpos.dismiss();
  \endcode
  */
  template <typename callable> inline UndoerImpl<callable> Undoer(callable c)
  {
    // static_assert(!std::is_function<callable>::value && !std::is_member_function_pointer<callable>::value && !std::is_member_object_pointer<callable>::value && !has_call_operator<callable>::value, "Undoer applied to a type not providing a call operator");
    auto foo = UndoerImpl<callable>(std::move(c));
    return foo;
  }  // Undoer

}  // namespace detail

BOOST_AFIO_V1_NAMESPACE_END
