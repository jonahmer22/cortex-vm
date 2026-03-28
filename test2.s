addi a13, zero, 1
addi a0, zero, 42
syscall
addi t0, zero, 20
addi t1, zero, 15
add t2, t0, t1
addi a0, t2, 0
syscall
sub t2, t0, t1
addi a0, t2, 0
syscall
addi t0, zero, 10
addi t1, zero, 12
and t2, t0, t1
addi a0, t2, 0
syscall
or t2, t0, t1
addi a0, t2, 0
syscall
xor t2, t0, t1
addi a0, t2, 0
syscall
addi t0, zero, 1
slli t1, t0, 3
addi a0, t1, 0
syscall
addi t0, zero, 16
srli t1, t0, 2
addi a0, t1, 0
syscall
addi t0, zero, 99
sw sp, t0, 0
add t0, r0, r0
lw t1, sp, 0
addi a0, t1, 0
syscall
halt
