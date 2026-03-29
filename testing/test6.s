; test6.s - syscall test
; tests: SYS_PRINT_INT (all formats), SYS_PRINT_UINT, SYS_PRINT_CHAR,
;        SYS_PRINT_STR, SYS_READ_INT, SYS_READ_CHAR, SYS_RAND_SEED,
;        SYS_RAND_INT, SYS_RAND_R_INT, SYS_EXIT

main:
    ; --- SYS_PRINT_STR ---
    addi a0, zero, str_header
    addi a13, zero, 5
    syscall

    ; --- SYS_PRINT_INT decimal ---
    addi a0, zero, str_int_dec
    addi a13, zero, 5
    syscall
    addi a0, zero, -42
    addi a1, zero, 0
    addi a13, zero, 1
    syscall
    addi a0, zero, str_newline
    addi a13, zero, 5
    syscall

    ; --- SYS_PRINT_INT hex ---
    addi a0, zero, str_int_hex
    addi a13, zero, 5
    syscall
    addi a0, zero, 255
    addi a1, zero, 3
    addi a13, zero, 1
    syscall
    addi a0, zero, str_newline
    addi a13, zero, 5
    syscall

    ; --- SYS_PRINT_UINT decimal ---
    addi a0, zero, str_uint_dec
    addi a13, zero, 5
    syscall
    addi a0, zero, 1000
    addi a1, zero, 0
    addi a13, zero, 2
    syscall
    addi a0, zero, str_newline
    addi a13, zero, 5
    syscall

    ; --- SYS_PRINT_CHAR ---
    addi a0, zero, str_char
    addi a13, zero, 5
    syscall
    addi a0, zero, 'Z'
    addi a13, zero, 3
    syscall
    addi a0, zero, str_newline
    addi a13, zero, 5
    syscall

    ; --- SYS_RAND_SEED + SYS_RAND_INT ---
    addi a0, zero, str_rand
    addi a13, zero, 5
    syscall
    addi a0, zero, 12345
    addi a13, zero, 21
    syscall
    addi a13, zero, 22
    syscall
    addi a1, zero, 0
    addi a13, zero, 2
    syscall
    addi a0, zero, str_newline
    addi a13, zero, 5
    syscall

    ; --- SYS_RAND_R_INT (0 to 100) ---
    addi a0, zero, str_rand_r
    addi a13, zero, 5
    syscall
    addi a0, zero, 0
    addi a1, zero, 100
    addi a13, zero, 23
    syscall
    addi a1, zero, 0
    addi a13, zero, 2
    syscall
    addi a0, zero, str_newline
    addi a13, zero, 5
    syscall

    ; --- SYS_READ_INT ---
    addi a0, zero, str_prompt_int
    addi a13, zero, 5
    syscall
    addi a1, zero, 0
    addi a13, zero, 11
    syscall
    ; echo it back
    addi a1, zero, 0
    addi a13, zero, 1
    syscall
    addi a0, zero, str_newline
    addi a13, zero, 5
    syscall

    ; --- SYS_READ_CHAR ---
    addi a0, zero, str_prompt_char
    addi a13, zero, 5
    syscall
    addi a13, zero, 13
    syscall
    addi a13, zero, 3
    syscall
    addi a0, zero, str_newline
    addi a13, zero, 5
    syscall

    ; --- SYS_READ_STR ---
    addi a0, zero, str_prompt_str
    addi a13, zero, 5
    syscall
    addi a0, sp, 0          ; destination buffer at current sp
    addi a1, zero, 64       ; max 64 chars
    addi a13, zero, 15
    syscall
    addi a0, zero, str_echo
    addi a13, zero, 5
    syscall
    addi a0, sp, 0          ; print the buffer back
    addi a13, zero, 5
    syscall
    addi a0, zero, str_newline
    addi a13, zero, 5
    syscall

    ; --- SYS_EXIT ---
    addi a0, zero, str_done
    addi a13, zero, 5
    syscall
    addi a0, zero, 0
    addi a13, zero, 0
    syscall

.data
    str_header:      "=== syscall test ===\n"
    str_newline:     "\n"
    str_int_dec:     "PRINT_INT decimal: "
    str_int_hex:     "PRINT_INT hex:     "
    str_uint_dec:    "PRINT_UINT decimal: "
    str_char:        "PRINT_CHAR: "
    str_rand:        "RAND_INT:   "
    str_rand_r:      "RAND_R_INT [0,100]: "
    str_prompt_int:  "enter an integer: "
    str_prompt_char: "enter a char: "
    str_prompt_str:  "enter a string: "
    str_echo:        "you entered: "
    str_done:        "done.\n"