; Benchmark: mul (integer multiply) throughput (50M iterations)
main:
    addi t1, zero, 0
    addi t2, zero, 50000000
    addi t3, zero, 3
    addi t4, zero, 7
loop:
    mul  t5, t3, t4
    addi t1, t1, 1
    blt  t1, t2, loop
    addi a0, zero, 0
    addi a13, zero, 0
    syscall
