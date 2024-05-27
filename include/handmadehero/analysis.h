#ifndef HANDMADEHERO_ANALYSIS_H
#define HANDMADEHERO_ANALYSIS_H

#if defined(__clang__)

#define BEGIN_ANALYSIS(x) __asm volatile("# LLVM-MCA-BEGIN " x);
#define END_ANALYSIS() __asm volatile("# LLVM-MCA-END")

#else

#define BEGIN_ANALYSIS(x)
#define END_ANALYSIS()

#endif

#endif /* HANDMADEHERO_ANALYSIS_H */
