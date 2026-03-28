addi t0, zero, 0
addi t0, t0, 1
addi t1, zero, 1000000000
blt t0, t1, -2
addi a0, t0, 0
addi a13, zero, 1
syscall
halt