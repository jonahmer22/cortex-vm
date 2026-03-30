main:
    addi a13, zero, 5
    addi a0, zero, print_str
    syscall
    ; print the number
    lw a0, zero, double
    addi a1, zero, 14
    addi a13, zero, 4
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall
    addi a0, zero, 0
    addi a13, zero, 0
    syscall

.data
    print_str: "this should be a float: "
    double: 3.14159265358979