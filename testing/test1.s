; print number in decimal
addi a13, r0, 1
addi a0, zero, 69
syscall
; print newline character
addi a13, zero, 3
addi a0, zero, '\n'
syscall
; print number binary
addi a13, zero, 2
addi a1, zero, 1
syscall
addi a0, zero, 100
addi a13, zero, 0
syscall
addi a13, zero, 1
syscall