main:
	; get an input float and print it with p = 3
	addi	a13, zero, 14
	syscall
	addi	s0, a0, 0	; move the original float to s0
	; print it
	faddi	a0, s0, 0
	addi	a1, zero, 3
	addi	a13, zero, 4
	syscall
	addi	a0, zero, '\n'
	addi	a13, zero, 3
	syscall

	; convert the float to an int and print
	ftoi	a0, s0
	addi	a13, zero, 1
	addi	a1, zero, 0
	syscall
	addi	a0, zero, '\n'
	addi	a13, zero, 3
	syscall
	
	; convert the float to a unsigned int
	ftoui	a0, s0
	addi	a13, zero, 1
	addi	a1, zero, 0
	syscall
	addi	a0, zero, '\n'
	addi	a13, zero, 3
	syscall

	; exit safely
	addi a0, zero, 0
	addi a13, zero, 0
	syscall	
