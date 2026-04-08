; Benchmark: never-taken branch in hot path (100M iterations)
; Each iteration: one never-taken forward branch + one always-taken backward branch.
main:
    addi t1, zero, 0
    addi t2, zero, 100000000
loop:
    blt  t2, t1, done       ; never taken: limit > counter always
    addi t1, t1, 1
    blt  t1, t2, loop
done:
    addi a0, zero, 0
    addi a13, zero, 0
    syscall
