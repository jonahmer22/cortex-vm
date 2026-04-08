; Benchmark: fsqrt (float square root) throughput (10M iterations)
main:
    addi  t1, zero, 0
    addi  t2, zero, 10000000
    faddi t3, zero, 2.0
loop:
    fsqrt t4, t3
    addi  t1, t1, 1
    blt   t1, t2, loop
    addi  a0, zero, 0
    addi  a13, zero, 0
    syscall
