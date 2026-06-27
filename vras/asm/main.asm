_start:
	dszr

	ldi r1, 0xff
	wrss r1
	wrsp r1

	ldi r1, 0
	wrds r1

	ldi r1, 13
	call fib // result in r0

#include <fib.asm>
