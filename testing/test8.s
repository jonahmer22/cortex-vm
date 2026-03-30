; test8.s - M extension opcodes (multiply / divide / remainder)
; expected output:
;   mul   6 * 7    = 42
;   muli  10 * 5   = 50
;   mulh  3 * 4    = 0
;   mulhu 5 * 6    = 0
;   div   100 / 7  = 14
;   divi  100 / 5  = 20
;   divu  100 / 7  = 14
;   divui 100 / 4  = 25
;   rem   100 % 7  = 2
;   remi  100 % 6  = 4
;   remu  100 % 7  = 2
;   remui 100 % 9  = 1

main:
    ; --- mul: 6 * 7 = 42 ---
    addi a13, zero, 5
    addi a0, zero, str_mul
    syscall
    addi t0, zero, 6
    addi t1, zero, 7
    mul t2, t0, t1
    addi a13, zero, 1
    addi a0, t2, 0
    addi a1, zero, 0
    syscall
    addi a13, zero, 3
    addi a0, zero, '\n'
    syscall

    ; --- muli: 10 * 5 = 50 ---
    addi a13, zero, 5
    addi a0, zero, str_muli
    syscall
    addi t0, zero, 10
    muli t2, t0, 5
    addi a13, zero, 1
    addi a0, t2, 0
    addi a1, zero, 0
    syscall
    addi a13, zero, 3
    addi a0, zero, '\n'
    syscall

    ; --- mulh: upper 64 bits of 3 * 4 = 0 (no overflow) ---
    addi a13, zero, 5
    addi a0, zero, str_mulh
    syscall
    addi t0, zero, 3
    addi t1, zero, 4
    mulh t2, t0, t1
    addi a13, zero, 1
    addi a0, t2, 0
    addi a1, zero, 0
    syscall
    addi a13, zero, 3
    addi a0, zero, '\n'
    syscall

    ; --- mulhu: upper 64 bits of 5 * 6 = 0 (no overflow) ---
    addi a13, zero, 5
    addi a0, zero, str_mulhu
    syscall
    addi t0, zero, 5
    addi t1, zero, 6
    mulhu t2, t0, t1
    addi a13, zero, 2
    addi a0, t2, 0
    addi a1, zero, 0
    syscall
    addi a13, zero, 3
    addi a0, zero, '\n'
    syscall

    ; --- div: 100 / 7 = 14 ---
    addi a13, zero, 5
    addi a0, zero, str_div
    syscall
    addi t0, zero, 100
    addi t1, zero, 7
    div t2, t0, t1
    addi a13, zero, 1
    addi a0, t2, 0
    addi a1, zero, 0
    syscall
    addi a13, zero, 3
    addi a0, zero, '\n'
    syscall

    ; --- divi: 100 / 5 = 20 ---
    addi a13, zero, 5
    addi a0, zero, str_divi
    syscall
    addi t0, zero, 100
    divi t2, t0, 5
    addi a13, zero, 1
    addi a0, t2, 0
    addi a1, zero, 0
    syscall
    addi a13, zero, 3
    addi a0, zero, '\n'
    syscall

    ; --- divu: 100 / 7 = 14 (unsigned) ---
    addi a13, zero, 5
    addi a0, zero, str_divu
    syscall
    addi t0, zero, 100
    addi t1, zero, 7
    divu t2, t0, t1
    addi a13, zero, 2
    addi a0, t2, 0
    addi a1, zero, 0
    syscall
    addi a13, zero, 3
    addi a0, zero, '\n'
    syscall

    ; --- divui: 100 / 4 = 25 (unsigned immediate) ---
    addi a13, zero, 5
    addi a0, zero, str_divui
    syscall
    addi t0, zero, 100
    divui t2, t0, 4
    addi a13, zero, 2
    addi a0, t2, 0
    addi a1, zero, 0
    syscall
    addi a13, zero, 3
    addi a0, zero, '\n'
    syscall

    ; --- rem: 100 % 7 = 2 ---
    addi a13, zero, 5
    addi a0, zero, str_rem
    syscall
    addi t0, zero, 100
    addi t1, zero, 7
    rem t2, t0, t1
    addi a13, zero, 1
    addi a0, t2, 0
    addi a1, zero, 0
    syscall
    addi a13, zero, 3
    addi a0, zero, '\n'
    syscall

    ; --- remi: 100 % 6 = 4 ---
    addi a13, zero, 5
    addi a0, zero, str_remi
    syscall
    addi t0, zero, 100
    remi t2, t0, 6
    addi a13, zero, 1
    addi a0, t2, 0
    addi a1, zero, 0
    syscall
    addi a13, zero, 3
    addi a0, zero, '\n'
    syscall

    ; --- remu: 100 % 7 = 2 (unsigned) ---
    addi a13, zero, 5
    addi a0, zero, str_remu
    syscall
    addi t0, zero, 100
    addi t1, zero, 7
    remu t2, t0, t1
    addi a13, zero, 2
    addi a0, t2, 0
    addi a1, zero, 0
    syscall
    addi a13, zero, 3
    addi a0, zero, '\n'
    syscall

    ; --- remui: 100 % 9 = 1 (unsigned immediate) ---
    addi a13, zero, 5
    addi a0, zero, str_remui
    syscall
    addi t0, zero, 100
    remui t2, t0, 9
    addi a13, zero, 2
    addi a0, t2, 0
    addi a1, zero, 0
    syscall
    addi a13, zero, 3
    addi a0, zero, '\n'
    syscall

    ; exit
    addi a13, zero, 0
    addi a0, zero, 0
    syscall

.data
    str_mul:   "mul   6 * 7    = "
    str_muli:  "muli  10 * 5   = "
    str_mulh:  "mulh  3 * 4    = "
    str_mulhu: "mulhu 5 * 6    = "
    str_div:   "div   100 / 7  = "
    str_divi:  "divi  100 / 5  = "
    str_divu:  "divu  100 / 7  = "
    str_divui: "divui 100 / 4  = "
    str_rem:   "rem   100 % 7  = "
    str_remi:  "remi  100 % 6  = "
    str_remu:  "remu  100 % 7  = "
    str_remui: "remui 100 % 9  = "