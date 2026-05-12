; Real-world benchmark: recursive Fibonacci
; Computes fib(35) recursively. Expected result: 9227465
; Prints the result then exits.
;
; Calling convention:
;   a0 = argument n, return value
;   ra = return address (r3)
;   jmp ra, zero, func  => call
;   jmp zero, ra, 0     => return
;
main:
    addi a0, zero, 35
    jmp  ra, zero, fib
    addi t2, a0, 0
    addi a0, t2, 0
    addi a1, zero, 0
    addi a13, zero, 1       ; SYS_PRINT_INT
    syscall
    addi a0, zero, 0
    addi a13, zero, 0       ; SYS_EXIT
    syscall

fib:
    ; if n < 2: return n
    addi t0, zero, 2
    blt  a0, t0, fib_base

    ; push ra and n onto stack
    sw   sp, ra, 0
    sw   sp, a0, 1
    addi sp, sp, 2

    ; call fib(n-1)
    subi a0, a0, 1
    jmp  ra, zero, fib

    ; a0 = fib(n-1); save it, load n
    addi t1, a0, 0          ; t1 = fib(n-1)
    lw   a0, sp, -1         ; a0 = original n
    subi a0, a0, 2          ; a0 = n-2

    ; push fib(n-1) before second call
    sw   sp, t1, 0
    addi sp, sp, 1

    ; call fib(n-2)
    jmp  ra, zero, fib

    ; a0 = fib(n-2); load fib(n-1) and add
    lw   t1, sp, -1         ; t1 = fib(n-1)
    add  a0, a0, t1         ; a0 = fib(n-1) + fib(n-2)

    ; restore ra and clean stack: sp was incremented by 3 total (2 + 1)
    lw   ra, sp, -3         ; ra is at original sp (now sp-3)
    subi sp, sp, 3

    jmp  zero, ra, 0        ; return

fib_base:
    jmp  zero, ra, 0        ; return n unchanged (a0 already = n)
