; Benchmark: strided store+load (10M iterations, stride=64 words)
main:
    addi t1, zero, 0
    addi t2, zero, 10000000
    addi t3, zero, 42
loop:
    sw   sp, t3, 0
    sw   sp, t3, 64
    lw   t4, sp, 0
    lw   t5, sp, 64
    addi t1, t1, 1
    blt  t1, t2, loop
    addi a0, zero, 0
    addi a13, zero, 0
    syscall
