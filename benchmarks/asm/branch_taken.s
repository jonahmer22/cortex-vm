; Benchmark: always-taken branch throughput (100M backward branches)
main:
    addi t1, zero, 0
    addi t2, zero, 100000000
loop:
    addi t1, t1, 1
    blt  t1, t2, loop
    addi a0, zero, 0
    addi a13, zero, 0
    syscall
