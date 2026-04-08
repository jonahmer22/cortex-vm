; Benchmark: mixed branch (alternating taken/not-taken, 100M iterations)
main:
    addi t1, zero, 0
    addi t2, zero, 100000000
    addi t3, zero, 0
    addi t4, zero, 1
loop:
    xor  t3, t3, t4         ; flip flag between 0 and 1
    beq  t3, t4, skip       ; taken when flag==1 (every other iteration)
skip:
    addi t1, t1, 1
    blt  t1, t2, loop
    addi a0, zero, 0
    addi a13, zero, 0
    syscall
