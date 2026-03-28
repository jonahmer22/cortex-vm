main:
    addi s0, r0, 69
    jmp ra, r0, funct
    addi a13, zero, 1
    syscall
    halt

funct:
    addi a0, s0, 0
    jmp r0, ra, 0