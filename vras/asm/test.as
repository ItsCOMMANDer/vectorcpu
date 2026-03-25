#include <bigint.as>
#include       "incl.as"
#include    "dir/nestedIncl.as"
@offset 0x0
start:
  ldi r1, 13
  call fib
  halt // result in r0

fib:
  addi r2, r1, 0
  beq zc
  addi r1, r1, -1
  beq oc
  push r1
  call fib
  pop r1
  push r0
  addi r1, r1, -1
  call fib
  pop r1
  add r0, r0, r1
  ret

zc:
  ldi r0, 0
  ret

oc:
  ldi r0, 1
  ret