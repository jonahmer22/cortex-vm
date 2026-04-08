; Benchmark: sequential store+load throughput (10M sw+lw pairs)
main:
    addi t1, zero, 0
    addi t2, zero, 10000000
    addi t3, zero, 42
loop:
    sw   sp, t3, 0
    lw   t4, sp, 0
    addi t1, t1, 1
    blt  t1, t2, loop
    addi a0, zero, 0
    addi a13, zero, 0
    syscall
