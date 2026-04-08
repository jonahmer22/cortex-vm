; Benchmark: fadd (float add) throughput (50M iterations)
main:
    addi  t1, zero, 0
    addi  t2, zero, 50000000
    faddi t3, zero, 1.0
    faddi t4, zero, 1.5
loop:
    fadd  t3, t3, t4
    addi  t1, t1, 1
    blt   t1, t2, loop
    addi  a0, zero, 0
    addi  a13, zero, 0
    syscall
