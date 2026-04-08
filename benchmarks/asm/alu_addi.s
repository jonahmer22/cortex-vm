; Benchmark: addi throughput (100M iterations)
; Timing is done externally by the Python runner.
main:
    addi t1, zero, 0
    addi t2, zero, 100000000
loop:
    addi t3, t3, 1
    addi t1, t1, 1
    blt  t1, t2, loop
    addi a0, zero, 0
    addi a13, zero, 0
    syscall
