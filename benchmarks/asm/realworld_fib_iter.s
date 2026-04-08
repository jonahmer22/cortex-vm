; Real-world benchmark: iterative Fibonacci
; Computes fib(40) iteratively. Expected result: 102334155
; Prints the result then exits with code 0.
main:
    addi t0, zero, 0        ; a = fib(0) = 0
    addi t1, zero, 1        ; b = fib(1) = 1
    addi t2, zero, 40       ; n
    addi t3, zero, 1        ; i = 1
loop:
    add  t4, t0, t1         ; t4 = a + b
    addi t0, t1, 0          ; a = b
    addi t1, t4, 0          ; b = a + b
    addi t3, t3, 1          ; i++
    blt  t3, t2, loop       ; if i < 40, repeat (runs 39 times → fib(40))

    addi a0, t1, 0
    addi a1, zero, 0
    addi a13, zero, 1       ; SYS_PRINT_INT
    syscall
    addi a0, zero, 0
    addi a13, zero, 0       ; SYS_EXIT
    syscall
