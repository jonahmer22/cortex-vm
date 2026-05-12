; Real-world benchmark: Newton's method for sqrt(2.0)
; Runs 1,000,000 iterations: x = (x + n/x) / 2
; Converges in ~50 iterations; the remainder stress-tests the float loop.
; Prints result with 6 decimal places. Expected: ~1.414214
main:
    faddi t0, zero, 2.0     ; n = 2.0
    faddi t1, zero, 1.0     ; x = initial guess = 1.0
    faddi t2, zero, 2.0     ; divisor constant
    addi  t3, zero, 0       ; iteration counter
    addi  t4, zero, 1000000 ; max iterations

newton_loop:
    fdiv  t5, t0, t1        ; t5 = n / x
    fadd  t6, t1, t5        ; t6 = x + n/x
    fdiv  t1, t6, t2        ; x = (x + n/x) / 2.0
    addi  t3, t3, 1
    blt   t3, t4, newton_loop

    addi  a0, t1, 0
    addi  a1, zero, 6
    addi  a13, zero, 4      ; SYS_PRINT_FLOAT
    syscall
    addi  a0, zero, 0
    addi  a13, zero, 0      ; SYS_EXIT
    syscall
