SECTION .text
GLOBAL rdtsc  
rdtsc:
  rdtsc         ; returns time in edx:eax, edx high order
  shl rdx, 0x20
  or rax, rdx
  ret
