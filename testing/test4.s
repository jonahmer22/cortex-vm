; test4.s - function call with callee-saved register spill/restore
; calls add_and_print(10, 32), which saves ra and s0 to the stack,
; computes a0 + a1 into s0, prints it, restores, then returns.
; expected output: 42

main:
	addi a0, zero, 60			; first argument
	addi a1, zero, 9			; second argument
	addi s0, zero, 420			; store value in s0 to make sure restoring works
	jmp ra, zero, add_and_print	; call (saves return addr in ra)
	addi a13, zero, 1
	addi a0, s0, 0
	syscall
	halt

add_and_print:
	; prologue: spill ra and s0 to stack (post-increment push)
	sw sp, ra, 0
	addi sp, sp, 1
	sw sp, s0, 0
	addi sp, sp, 1

	; body: s0 = a0 + a1
	add s0, a0, a1

	; print result via syscall 1 (print_int)
	addi a13, zero, 1
	addi a0, s0, 0
	syscall

	; epilogue: restore s0 and ra (pre-decrement pop)
	subi sp, sp, 1
	lw s0, sp, 0
	subi sp, sp, 1
	lw ra, sp, 0

	; return
	jmp zero, ra, 0