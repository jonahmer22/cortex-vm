; test11.s - F extension opcodes and float syscalls
; expected output:
;   fadd: 7.000000
;   fsub: 7.000000
;   fmul: 12.000000
;   fdiv: 2.500000
;   faddi: 7.500000
;   fsubi: 7.000000
;   fmuli: 10.000000
;   fdivi: 2.500000
;   fsqrt: 4.000000
;   fabs: 5.000000
;   fneg: -3.000000
;   ftoi: 3
;   ftoui: 3
;   itof: 42.000000
;   uitof: 42.000000
;   fblt: taken
;   fble: taken
;   fbgt: taken
;   fbge: taken
;   rand: [random value 0.0 - 1.0]

main:
    ; --- fadd: 3.0 + 4.0 = 7.0 ---
    addi a13, zero, 5
    addi a0, zero, str_fadd
    syscall
    lw t0, zero, val_3
    lw t1, zero, val_4
    fadd t2, t0, t1
    addi a13, zero, 4
    addi a0, t2, 0
    addi a1, zero, 6
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- fsub: 10.0 - 3.0 = 7.0 ---
    addi a13, zero, 5
    addi a0, zero, str_fsub
    syscall
    lw t0, zero, val_10
    lw t1, zero, val_3
    fsub t2, t0, t1
    addi a13, zero, 4
    addi a0, t2, 0
    addi a1, zero, 6
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- fmul: 3.0 * 4.0 = 12.0 ---
    addi a13, zero, 5
    addi a0, zero, str_fmul
    syscall
    lw t0, zero, val_3
    lw t1, zero, val_4
    fmul t2, t0, t1
    addi a13, zero, 4
    addi a0, t2, 0
    addi a1, zero, 6
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- fdiv: 10.0 / 4.0 = 2.5 ---
    addi a13, zero, 5
    addi a0, zero, str_fdiv
    syscall
    lw t0, zero, val_10
    lw t1, zero, val_4
    fdiv t2, t0, t1
    addi a13, zero, 4
    addi a0, t2, 0
    addi a1, zero, 6
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- faddi: 5.0 + 2.5 = 7.5 ---
    addi a13, zero, 5
    addi a0, zero, str_faddi
    syscall
    lw t0, zero, val_5
    faddi t2, t0, 2.5
    addi a13, zero, 4
    addi a0, t2, 0
    addi a1, zero, 6
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- fsubi: 10.0 - 3.0 = 7.0 ---
    addi a13, zero, 5
    addi a0, zero, str_fsubi
    syscall
    lw t0, zero, val_10
    fsubi t2, t0, 3.0
    addi a13, zero, 4
    addi a0, t2, 0
    addi a1, zero, 6
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- fmuli: 4.0 * 2.5 = 10.0 ---
    addi a13, zero, 5
    addi a0, zero, str_fmuli
    syscall
    lw t0, zero, val_4
    fmuli t2, t0, 2.5
    addi a13, zero, 4
    addi a0, t2, 0
    addi a1, zero, 6
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- fdivi: 10.0 / 4.0 = 2.5 ---
    addi a13, zero, 5
    addi a0, zero, str_fdivi
    syscall
    lw t0, zero, val_10
    fdivi t2, t0, 4.0
    addi a13, zero, 4
    addi a0, t2, 0
    addi a1, zero, 6
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- fsqrt: sqrt(16.0) = 4.0 ---
    addi a13, zero, 5
    addi a0, zero, str_fsqrt
    syscall
    lw t0, zero, val_16
    fsqrt t2, t0
    addi a13, zero, 4
    addi a0, t2, 0
    addi a1, zero, 6
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- fabs: |-5.0| = 5.0 ---
    addi a13, zero, 5
    addi a0, zero, str_fabs
    syscall
    lw t0, zero, val_neg5
    fabs t2, t0
    addi a13, zero, 4
    addi a0, t2, 0
    addi a1, zero, 6
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- fneg: -(3.0) = -3.0 ---
    addi a13, zero, 5
    addi a0, zero, str_fneg
    syscall
    lw t0, zero, val_3
    fneg t2, t0
    addi a13, zero, 4
    addi a0, t2, 0
    addi a1, zero, 6
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- ftoi: 3.7 -> 3 (truncates toward zero) ---
    addi a13, zero, 5
    addi a0, zero, str_ftoi
    syscall
    lw t0, zero, val_3_7
    ftoi t2, t0
    addi a1, zero, 0
    addi a13, zero, 1
    addi a0, t2, 0
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- ftoui: 3.7 -> 3 (unsigned) ---
    addi a13, zero, 5
    addi a0, zero, str_ftoui
    syscall
    lw t0, zero, val_3_7
    ftoui t2, t0
    addi a1, zero, 0
    addi a13, zero, 2
    addi a0, t2, 0
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- itof: 42 (signed int) -> 42.0 ---
    addi a13, zero, 5
    addi a0, zero, str_itof
    syscall
    addi t0, zero, 42
    itof t2, t0
    addi a13, zero, 4
    addi a0, t2, 0
    addi a1, zero, 6
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- uitof: 42 (unsigned int) -> 42.0 ---
    addi a13, zero, 5
    addi a0, zero, str_uitof
    syscall
    addi t0, zero, 42
    uitof t2, t0
    addi a13, zero, 4
    addi a0, t2, 0
    addi a1, zero, 6
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- fblt: 3.0 < 5.0 (should branch) ---
    addi a13, zero, 5
    addi a0, zero, str_fblt
    syscall
    lw t0, zero, val_3
    lw t1, zero, val_5
    fblt t0, t1, fblt_taken
    addi a13, zero, 5
    addi a0, zero, str_fail
    syscall
    jmp t31, zero, fblt_done
fblt_taken:
    addi a13, zero, 5
    addi a0, zero, str_taken
    syscall
fblt_done:
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- fble: 3.0 <= 3.0 (should branch) ---
    addi a13, zero, 5
    addi a0, zero, str_fble
    syscall
    lw t0, zero, val_3
    lw t1, zero, val_3
    fble t0, t1, fble_taken
    addi a13, zero, 5
    addi a0, zero, str_fail
    syscall
    jmp t31, zero, fble_done
fble_taken:
    addi a13, zero, 5
    addi a0, zero, str_taken
    syscall
fble_done:
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- fbgt: 5.0 > 3.0 (should branch) ---
    addi a13, zero, 5
    addi a0, zero, str_fbgt
    syscall
    lw t0, zero, val_5
    lw t1, zero, val_3
    fbgt t0, t1, fbgt_taken
    addi a13, zero, 5
    addi a0, zero, str_fail
    syscall
    jmp t31, zero, fbgt_done
fbgt_taken:
    addi a13, zero, 5
    addi a0, zero, str_taken
    syscall
fbgt_done:
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- fbge: 5.0 >= 5.0 (should branch) ---
    addi a13, zero, 5
    addi a0, zero, str_fbge
    syscall
    lw t0, zero, val_5
    lw t1, zero, val_5
    fbge t0, t1, fbge_taken
    addi a13, zero, 5
    addi a0, zero, str_fail
    syscall
    jmp t31, zero, fbge_done
fbge_taken:
    addi a13, zero, 5
    addi a0, zero, str_taken
    syscall
fbge_done:
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    ; --- rand_float: syscall 24, result in a0 ---
    addi a13, zero, 5
    addi a0, zero, str_rand
    syscall
    addi a13, zero, 24
    syscall
    addi a13, zero, 4
    addi a1, zero, 6
    syscall
    addi a0, zero, '\n'
    addi a13, zero, 3
    syscall

    addi a0, zero, 0
    addi a13, zero, 0
    syscall

.data
    str_fadd:  "fadd: "
    str_fsub:  "fsub: "
    str_fmul:  "fmul: "
    str_fdiv:  "fdiv: "
    str_faddi: "faddi: "
    str_fsubi: "fsubi: "
    str_fmuli: "fmuli: "
    str_fdivi: "fdivi: "
    str_fsqrt: "fsqrt: "
    str_fabs:  "fabs: "
    str_fneg:  "fneg: "
    str_ftoi:  "ftoi: "
    str_ftoui: "ftoui: "
    str_itof:  "itof: "
    str_uitof: "uitof: "
    str_fblt:  "fblt: "
    str_fble:  "fble: "
    str_fbgt:  "fbgt: "
    str_fbge:  "fbge: "
    str_rand:  "rand: "
    str_taken: "taken"
    str_fail:  "NOT TAKEN (FAIL)"
    val_3:    3.0
    val_4:    4.0
    val_5:    5.0
    val_10:   10.0
    val_16:   16.0
    val_neg5: -5.0
    val_3_7:  3.7
