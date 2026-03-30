main:
    addi t0, zero, 42
    muli t0, t0, 69
    addi a13, zero, 5
    addi a0, zero, printstr
    syscall
    addi a13, zero, 1
    addi a0, t0, 0
    syscall
    addi a13, zero, 3
    addi a0, zero, '\n'
    syscall
    ; need to exit before you hit data section
    addi a13, zero, 0
    addi a0, zero, 0
    syscall

.data
    printstr: "69 * 42 = "