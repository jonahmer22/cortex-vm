; Real-world benchmark: Sieve of Eratosthenes up to 1000
; Stores prime flags in stack memory (1=prime, 0=composite).
; Prints the count of primes found (expected: 168 primes <= 1000).
;
; Layout: flag[i] stored at sp+i for i in 0..999
;   0 and 1 are not prime → initialize all to 1, then clear 0 and 1.
;
main:
    ; Initialize flag[0..999] = 1
    addi t0, zero, 0        ; i = 0
    addi t1, zero, 1000
    addi t2, zero, 1
init_loop:
    add  t3, sp, t0         ; addr = sp + i
    sw   t3, t2, 0          ; flag[i] = 1
    addi t0, t0, 1
    blt  t0, t1, init_loop

    ; flag[0] = 0, flag[1] = 0
    sw   sp, zero, 0
    sw   sp, zero, 1

    ; Sieve: for p = 2..31 (sqrt(1000) ~ 31)
    addi t0, zero, 2        ; p = 2
    addi t8, zero, 32       ; sqrt limit
sieve_outer:
    blt  t8, t0, sieve_done   ; if p >= 32, done
    ; check if flag[p] == 1
    add  t3, sp, t0
    lw   t4, t3, 0
    beq  t4, zero, sieve_next ; if flag[p] == 0, skip

    ; mark multiples of p starting at p*p
    ; inner: j = p*p, step p
    mul  t5, t0, t0         ; j = p*p
    addi t6, zero, 1000
sieve_inner:
    blt  t5, t6, sieve_mark   ; while j < 1000
    jmp  t9, zero, sieve_next
sieve_mark:
    add  t3, sp, t5
    sw   t3, zero, 0        ; flag[j] = 0
    add  t5, t5, t0         ; j += p
    jmp  t9, zero, sieve_inner

sieve_next:
    addi t0, t0, 1
    jmp  t9, zero, sieve_outer

sieve_done:
    ; count primes: sum flag[0..999]
    addi t0, zero, 0        ; i = 0
    addi t1, zero, 1000
    addi t7, zero, 0        ; count = 0
count_loop:
    add  t3, sp, t0
    lw   t4, t3, 0
    add  t7, t7, t4
    addi t0, t0, 1
    blt  t0, t1, count_loop

    addi a0, t7, 0
    addi a1, zero, 0
    addi a13, zero, 1       ; SYS_PRINT_INT
    syscall
    addi a0, zero, 0
    addi a13, zero, 0       ; SYS_EXIT
    syscall
