/* MemoryTransactions.hpp
Provides yet another mechanism for serialising memory
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Sept 2013
*/

#ifndef BOOST_AFIO_SPINLOCK_HPP
#define BOOST_AFIO_SPINLOCK_HPP

// Turn this on if you have a compiler which understands __transaction_relaxed
//#define BOOST_HAVE_TRANSACTIONAL_MEMORY_COMPILER

// Turn this on if you have Haswell TSX
//#define BOOST_USING_INTEL_TSX

#include "std_atomic_mutex_chrono.hpp"

#ifndef BOOST_SMT_PAUSE
#if defined(_MSC_VER) && _MSC_VER >= 1310 && ( defined(_M_IX86) || defined(_M_X64) )

extern "C" void _mm_pause();
#pragma intrinsic( _mm_pause )

#define BOOST_SMT_PAUSE _mm_pause();

#elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) )

#define BOOST_SMT_PAUSE __asm__ __volatile__( "rep; nop" : : : "memory" );

#endif
#endif

namespace boost
{
	namespace afio
	{
		namespace detail
		{
			/*! \class spinlock

			So what's wrong with boost/smart_ptr/detail/spinlock.hpp then, and why
			reinvent the wheel?

			1. Non-configurable spin. AFIO needs a bigger spin than smart_ptr provides.

			2. AFIO is C++ 11, and therefore can implement this in pure C++ 11 atomics.

			3. I don't much care for doing writes during the spin. It generates an
			unnecessary amount of cache line invalidation traffic. Better to spin-read
			and only write when the read suggests you might have a chance.

			4. The other spin lock doesn't use Intel TSX yet.
			*/
			template<typename T> struct spinlockbase
			{
				volatile atomic<T> v;
				spinlockbase() BOOST_NOEXCEPT_OR_NOTHROW : v(0) { }
				bool try_lock() BOOST_NOEXCEPT_OR_NOTHROW
				{
					if(v.load()) // Avoid unnecessary cache line invalidation traffic
						return false;
					T expected=0;
					return v.compare_exchange_weak(expected, 1);
				}
				void unlock() BOOST_NOEXCEPT_OR_NOTHROW
				{
					v.store(0);
				}
				bool int_yield(size_t) BOOST_NOEXCEPT_OR_NOTHROW{ return false; }
			};
			template<size_t spins> struct spins_to_transact
			{
				template<class parenttype> struct policy : parenttype
				{
					static BOOST_CONSTEXPR_OR_CONST size_t spins_to_transact=spins;
				};
			};
			template<size_t spins, bool use_pause=true> struct spins_to_loop
			{
				template<class parenttype> struct policy : parenttype
				{
					static BOOST_CONSTEXPR_OR_CONST size_t spins_to_loop=spins;
					bool int_yield(size_t n) BOOST_NOEXCEPT_OR_NOTHROW
					{
						if(parenttype::int_yield(n)) return true;
						if(n>=spins) return false;
						if(use_pause)
						{
#ifdef BOOST_SMT_PAUSE
							BOOST_SMT_PAUSE;
#endif
						}
						return true;
					}
				};
			};
			template<size_t spins> struct spins_to_yield
			{
				template<class parenttype> struct policy : parenttype
				{
					static BOOST_CONSTEXPR_OR_CONST size_t spins_to_yield=spins;
					bool int_yield(size_t n) BOOST_NOEXCEPT_OR_NOTHROW
					{
						if(parenttype::int_yield(n)) return true;
						if(n>=spins) return false;
						boost::afio::this_thread::yield();
						return true;
					}
				};
			};
			struct spins_to_sleep
			{
				template<class parenttype> struct policy : parenttype
				{
					bool int_yield(size_t n) BOOST_NOEXCEPT_OR_NOTHROW
					{
						if(parenttype::int_yield(n)) return true;
						boost::afio::this_thread::sleep_for(boost::afio::chrono::milliseconds(1));
						return true;
					}
				};
			};
			struct null_spin_policy
			{
				template<class parenttype> struct policy : parenttype
				{
				};
			};
			template<typename T, template<class> class spinpolicy1=spins_to_transact<50>::policy, template<class> class spinpolicy2=spins_to_loop<125>::policy, template<class> class spinpolicy3=spins_to_yield<250>::policy, template<class> class spinpolicy4=spins_to_sleep::policy> class spinlock : public spinpolicy4<spinpolicy3<spinpolicy2<spinpolicy1<spinlockbase<T>>>>>
			{
				typedef spinpolicy4<spinpolicy3<spinpolicy2<spinpolicy1<spinlockbase<T>>>>> parenttype;
			public:
				void lock() BOOST_NOEXCEPT_OR_NOTHROW
				{
					for(size_t n=0;; n++)
					{
						if(parenttype::try_lock())
							return;
						parenttype::int_yield(n);
					}
				}
			};

#ifndef BOOST_BEGIN_MEMORY_TRANSACTION
#ifdef BOOST_HAVE_TRANSACTIONAL_MEMORY_COMPILER
#define BOOST_BEGIN_MEMORY_TRANSACTION(lockable) __transaction_relaxed
#define BOOST_END_MEMORY_TRANSACTION(lockable)
#elif defined(BOOST_USING_INTEL_TSX)
#if defined(_MSC_VER) && _MSC_VER >= 1310 && ( defined(_M_IX86) || defined(_M_X64) )
#include <immintrin.h>
#elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) )
#include <immintrin.h>
#endif
#endif // BOOST_HAVE_TRANSACTIONAL_MEMORY_COMPILER

#ifndef BOOST_BEGIN_MEMORY_TRANSACTION
#ifdef BOOST_USING_INTEL_TSX
			template<class T> struct get_spins_to_transact
			{
				static BOOST_CONSTEXPR_OR_CONST size_t value=4;
			};
			template<class T> struct get_spins_to_transact<spinlock<T>>
			{
				static BOOST_CONSTEXPR_OR_CONST size_t value=spinlock<T>::spins_to_transact;
			};
			template<class T> inline bool is_lockable_locked(T &lockable)
			{
				if(lockable.try_lock())
				{
					lockable.unlock();
					return true;
				}
				return false;
			}
			template<class T> inline bool is_lockable_locked(spinlock<T> &lockable)
			{
				return 1==lockable.v.load();
			}
			// WARNING: I don't have access to a Haswell CPU, so I have no idea if the below works!
			template<class T> struct intel_tsx_transaction
			{
				T &lockable;
				int dismissed; // 0=disabled, 1=abort on exception throw, 2=commit, 3=traditional locks
				intel_tsx_transaction(T &_lockable) : lockable(_lockable), dismissed(0)
				{
					// Try four times
					size_t spins_to_transact=get_spins_to_transact<T>::value;
					for(size_t n=0; n<spins_to_transact; n++)
					{
						unsigned state=_xbegin(); // start transaction, or cope with abort
						if(_XBEGIN_STARTED==state)
						{
							if(!is_lockable_locked(lockable))
							{
								// Lock is free, so we can proceed with the transaction
								dismissed=1; // set to abort on exception throw
								return;
							}
							// If lock is not free, we need to abort transaction as something else is running
							_xabort(0x79);
							// Never reaches this point
						}
						// Transaction aborted due to too many locks or hard abort?
						if((state & _XABORT_CAPACITY) || !(state & _XABORT_RETRY))
						{
							// Fall back onto pure locks
							break;
						}
						// Was it me who aborted?
						if((state & _XABORT_EXPLICIT) && !(state & _XABORT_NESTED))
						{
							switch(_XABORT_CODE(state))
							{
							case 0x78: // exception thrown
								throw std::runtime_error("Unknown exception thrown inside Intel TSX memory transaction");
							case 0x79: // my lock was held by someone else, so repeat
								break;
							default: // something else aborted. Best fall back to locks
								n=(size_t) 1<<(4*sizeof(n));
								break;
							}
						}
					}
					// If the loop exited, we're falling back onto traditional locks
					lockable.lock();
					dismissed=3;
				}
				intel_tsx_transaction(intel_tsx_transaction &&o) BOOST_NOEXCEPT_OR_NOTHROW : lockable(o.lockable), dismissed(o.dismissed)
				{
					o.dismissed=0; // disable original
				}
				~intel_tsx_transaction() BOOST_NOEXCEPT_OR_NOTHROW
				{
					if(dismissed)
					{
						if(1==dismissed)
							_xabort(0x78);
						else if(2==dismissed)
							_xend();
						else if(3==dismissed)
							lockable.unlock();
					}
				}
				void commit() BOOST_NOEXCEPT_OR_NOTHROW
				{
					if(1==dismissed)
						dismissed=2; // commit transaction
				}
			};
			template<class T> inline intel_tsx_transaction<T> make_intel_tsx_transaction(T &lockable) BOOST_NOEXCEPT_OR_NOTHROW
			{
				return intel_tsx_transaction<T>(lockable);
			}
#define BOOST_BEGIN_MEMORY_TRANSACTION(lockable) { auto __tsx_transaction=boost::afio::detail::make_intel_tsx_transaction(lockable); {
#define BOOST_END_MEMORY_TRANSACTION(lockable) } __tsx_transaction.commit(); }

#else // BOOST_USING_INTEL_TSX

#define BOOST_BEGIN_MEMORY_TRANSACTION(lockable) { boost::lock_guard<decltype(lockable)> __tsx_transaction(lockable);
#define BOOST_END_MEMORY_TRANSACTION(lockable) }

#endif // BOOST_USING_INTEL_TSX
#endif // BOOST_BEGIN_MEMORY_TRANSACTION
#endif // BOOST_BEGIN_MEMORY_TRANSACTION

		}
	}
}

#endif