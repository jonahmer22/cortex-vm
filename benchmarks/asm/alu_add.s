; Benchmark: add (register-register) throughput (100M iterations)
main:
    addi t1, zero, 0
    addi t2, zero, 100000000
    addi t3, zero, 1
loop:
    add  t4, t4, t3
    addi t1, t1, 1
    blt  t1, t2, loop
    addi a0, zero, 0
    addi a13, zero, 0
    syscall
