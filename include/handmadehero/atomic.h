#ifndef HANDMADEHERO_ATOMIC_H
#define HANDMADEHERO_ATOMIC_H

#if COMPILER_GCC || COMPILER_CLANG

#define AtomicStore(ptr, value) __atomic_store_n(ptr, value, __ATOMIC_RELEASE)
#define AtomicCompareExchange(ptr, expected, desired)                                                                  \
  __atomic_compare_exchange_n(ptr, expected, desired, 1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)
#define AtomicCompareExchangeExplicit(ptr, expected, desired, weak, successMemOrder, failureMemOrder)                  \
  __atomic_compare_exchange_n(ptr, expected, desired, weak, successMemOrder, failureMemOrder)
#define AtomicFetchAdd(ptr, value) __atomic_fetch_add(ptr, value, __ATOMIC_RELEASE)

#elif COMPILER_MSVC
#error "TODO: msvc atomics"
#else
#error "atomics unsupported with this compiler"
#endif

#endif /* HANDMADEHERO_ATOMIC_H */
