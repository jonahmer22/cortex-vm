; Real-world benchmark: iterative Fibonacci
; Runs fib(40) iteratively 1,000,000 times to amortize startup cost.
; Expected result: 102334155
main:
    addi s0, zero, 0        ; outer = 0
    addi s1, zero, 1000000  ; outer limit
outer_loop:
    addi t0, zero, 0        ; a = fib(0) = 0
    addi t1, zero, 1        ; b = fib(1) = 1
    addi t2, zero, 40       ; n
    addi t3, zero, 1        ; i = 1
inner_loop:
    add  t4, t0, t1         ; t4 = a + b
    addi t0, t1, 0          ; a = b
    addi t1, t4, 0          ; b = a + b
    addi t3, t3, 1          ; i++
    blt  t3, t2, inner_loop ; if i < 40, repeat (runs 39 times -> fib(40))
    addi s0, s0, 1          ; outer++
    blt  s0, s1, outer_loop ; if outer < 1,000,000, repeat

    addi a0, t1, 0
    addi a1, zero, 0
    addi a13, zero, 1       ; SYS_PRINT_INT
    syscall
    addi a0, zero, 0
    addi a13, zero, 0       ; SYS_EXIT
    syscall
