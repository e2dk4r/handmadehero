#ifndef HANDMADEHERO_ANALYSIS_H
#define HANDMADEHERO_ANALYSIS_H

#if COMPILER_CLANG

#define BEGIN_ANALYSIS(x) __asm__ volatile("# LLVM-MCA-BEGIN " x ::: "memory");
#define END_ANALYSIS() __asm__ volatile("# LLVM-MCA-END" ::: "memory")

#else

#define BEGIN_ANALYSIS(x)
#define END_ANALYSIS()

#endif

#endif /* HANDMADEHERO_ANALYSIS_H */
