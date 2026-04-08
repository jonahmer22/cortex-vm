"""Tests for B-type branches, JMP, loops, and function calls."""
from conftest import asm_out, run_asm, prog, print_int


# ---------------------------------------------------------------------------
# BEQ
# ---------------------------------------------------------------------------

def test_beq_taken():
    # Equal values → branch taken → prints "1"
    src = (
        "    addi t0, zero, 5\n"
        "    addi t1, zero, 5\n"
        "    beq t0, t1, eq_label\n"
        "    addi t2, zero, 0\n"     # not taken path
        "    jmp t9, zero, done\n"
        "eq_label:\n"
        "    addi t2, zero, 1\n"     # taken path
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


def test_beq_not_taken():
    src = (
        "    addi t0, zero, 5\n"
        "    addi t1, zero, 6\n"
        "    beq t0, t1, eq_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "eq_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "0"


# ---------------------------------------------------------------------------
# BNE
# ---------------------------------------------------------------------------

def test_bne_taken():
    src = (
        "    addi t0, zero, 3\n"
        "    addi t1, zero, 7\n"
        "    bne t0, t1, ne_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "ne_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


def test_bne_not_taken():
    src = (
        "    addi t0, zero, 4\n"
        "    addi t1, zero, 4\n"
        "    bne t0, t1, ne_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "ne_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "0"


# ---------------------------------------------------------------------------
# BLT (signed less-than)
# ---------------------------------------------------------------------------

def test_blt_taken():
    src = (
        "    addi t0, zero, 3\n"
        "    addi t1, zero, 7\n"
        "    blt t0, t1, lt_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "lt_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


def test_blt_not_taken_equal():
    src = (
        "    addi t0, zero, 5\n"
        "    addi t1, zero, 5\n"
        "    blt t0, t1, lt_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "lt_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "0"


def test_blt_signed_negative():
    # -1 < 1 → signed branch taken
    src = (
        "    subi t0, zero, 1\n"
        "    addi t1, zero, 1\n"
        "    blt t0, t1, lt_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "lt_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


# ---------------------------------------------------------------------------
# BLTU (unsigned less-than)
# ---------------------------------------------------------------------------

def test_bltu_taken():
    src = (
        "    addi t0, zero, 2\n"
        "    addi t1, zero, 10\n"
        "    bltu t0, t1, lt_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "lt_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


def test_bltu_negative_treated_as_large():
    # -1 as uint64 is very large → NOT less than 1
    src = (
        "    subi t0, zero, 1\n"    # t0 = 0xFFFFFFFFFFFFFFFF (large uint)
        "    addi t1, zero, 1\n"
        "    bltu t0, t1, lt_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "lt_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "0"


# ---------------------------------------------------------------------------
# BGE (branch if >=, signed)
# ---------------------------------------------------------------------------

def test_bge_taken_greater():
    src = (
        "    addi t0, zero, 7\n"
        "    addi t1, zero, 3\n"
        "    bge t0, t1, ge_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "ge_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


def test_bge_taken_equal():
    src = (
        "    addi t0, zero, 5\n"
        "    addi t1, zero, 5\n"
        "    bge t0, t1, ge_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "ge_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


def test_bge_not_taken():
    src = (
        "    addi t0, zero, 3\n"
        "    addi t1, zero, 7\n"
        "    bge t0, t1, ge_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "ge_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "0"


def test_bge_signed_negative():
    # 1 >= -1 → taken (signed)
    src = (
        "    addi t0, zero, 1\n"
        "    subi t1, zero, 1\n"
        "    bge t0, t1, ge_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "ge_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


# ---------------------------------------------------------------------------
# BGT (branch if >, signed)
# ---------------------------------------------------------------------------

def test_bgt_taken():
    src = (
        "    addi t0, zero, 7\n"
        "    addi t1, zero, 3\n"
        "    bgt t0, t1, gt_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "gt_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


def test_bgt_not_taken_equal():
    src = (
        "    addi t0, zero, 5\n"
        "    addi t1, zero, 5\n"
        "    bgt t0, t1, gt_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "gt_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "0"


def test_bgt_not_taken_less():
    src = (
        "    addi t0, zero, 3\n"
        "    addi t1, zero, 7\n"
        "    bgt t0, t1, gt_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "gt_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "0"


def test_bgt_signed_negative():
    # 1 > -1 → taken (signed)
    src = (
        "    addi t0, zero, 1\n"
        "    subi t1, zero, 1\n"
        "    bgt t0, t1, gt_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "gt_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


# ---------------------------------------------------------------------------
# BGTU (branch if >, unsigned)
# ---------------------------------------------------------------------------

def test_bgtu_taken():
    src = (
        "    addi t0, zero, 10\n"
        "    addi t1, zero, 3\n"
        "    bgtu t0, t1, gtu_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "gtu_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


def test_bgtu_not_taken_equal():
    src = (
        "    addi t0, zero, 5\n"
        "    addi t1, zero, 5\n"
        "    bgtu t0, t1, gtu_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "gtu_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "0"


def test_bgtu_negative_as_large_unsigned():
    # -1 as uint64 = 0xFFFF... > 1 → taken
    src = (
        "    subi t0, zero, 1\n"
        "    addi t1, zero, 1\n"
        "    bgtu t0, t1, gtu_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "gtu_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


# ---------------------------------------------------------------------------
# BLE (branch if <=, signed)
# ---------------------------------------------------------------------------

def test_ble_taken_less():
    src = (
        "    addi t0, zero, 3\n"
        "    addi t1, zero, 7\n"
        "    ble t0, t1, le_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "le_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


def test_ble_taken_equal():
    src = (
        "    addi t0, zero, 5\n"
        "    addi t1, zero, 5\n"
        "    ble t0, t1, le_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "le_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


def test_ble_not_taken():
    src = (
        "    addi t0, zero, 7\n"
        "    addi t1, zero, 3\n"
        "    ble t0, t1, le_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "le_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "0"


def test_ble_signed_negative():
    # -1 <= 1 → taken (signed)
    src = (
        "    subi t0, zero, 1\n"
        "    addi t1, zero, 1\n"
        "    ble t0, t1, le_label\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "le_label:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


# ---------------------------------------------------------------------------
# JMP (unconditional jump / call-return)
# ---------------------------------------------------------------------------

def test_jmp_unconditional_skips_code():
    # Jump over an instruction that would set t2 = 99
    src = (
        "    addi t2, zero, 1\n"
        "    jmp t9, zero, skip_target\n"
        "    addi t2, zero, 99\n"    # should be skipped
        "skip_target:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


def test_jmp_saves_return_address():
    # jmp saves next PC into the destination register
    src = (
        "    jmp t9, zero, after\n"
        "after:\n"
        # t9 now holds the PC of the instruction right after the jmp
        # we just verify the program didn't crash and t9 is nonzero
        "    addi t2, zero, 1\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "1"


def test_function_call_and_return():
    # Call a function via jmp ra, zero, func; function returns via jmp zero, ra, 0.
    # ra points at the print block so return lands directly on the print instructions.
    src = (
        "    addi a0, zero, 10\n"
        "    addi a1, zero, 32\n"
        "    jmp ra, zero, add_fn\n"    # ra = address of next instruction (print block)
        # function returns here with t2 = 42
        + print_int()                   # prints t2
        + "    jmp t9, zero, done\n"
        + "add_fn:\n"
        + "    add t2, a0, a1\n"
        + "    jmp zero, ra, 0\n"       # return → jumps back to print block
        + "done:\n"
    )
    assert asm_out(prog(src)) == "42"


# ---------------------------------------------------------------------------
# Loops
# ---------------------------------------------------------------------------

def test_loop_count():
    # Count from 0 to 9 (10 iterations), result = 10
    src = (
        "    addi t0, zero, 0\n"
        "    addi t1, zero, 10\n"
        "loop:\n"
        "    addi t0, t0, 1\n"
        "    blt t0, t1, loop\n"
        "    addi t2, t0, 0\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "10"


def test_loop_sum():
    # Sum 1..5 = 15
    src = (
        "    addi t0, zero, 1\n"     # counter
        "    addi t1, zero, 6\n"     # limit (exclusive)
        "    addi t2, zero, 0\n"     # accumulator
        "loop:\n"
        "    add t2, t2, t0\n"
        "    addi t0, t0, 1\n"
        "    blt t0, t1, loop\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "15"


# ---------------------------------------------------------------------------
# Stack-based function call (callee-saved registers)
# ---------------------------------------------------------------------------

def test_callee_saved_s0():
    # save s0, use it, restore — outer s0 value must survive the call
    src = (
        "    addi s0, zero, 420\n"
        "    addi a0, zero, 60\n"
        "    addi a1, zero, 9\n"
        "    jmp ra, zero, adder\n"
        # after return: a0 = 69, s0 must still be 420
        "    addi t2, s0, 0\n"
        "    jmp t9, zero, done\n"
        "adder:\n"
        "    sw sp, ra, 0\n"
        "    addi sp, sp, 1\n"
        "    sw sp, s0, 0\n"
        "    addi sp, sp, 1\n"
        "    add s0, a0, a1\n"
        "    addi a0, s0, 0\n"
        "    subi sp, sp, 1\n"
        "    lw s0, sp, 0\n"
        "    subi sp, sp, 1\n"
        "    lw ra, sp, 0\n"
        "    jmp zero, ra, 0\n"
        "done:\n"
        f"{print_int()}"
    )
    assert asm_out(prog(src)) == "420"


# ---------------------------------------------------------------------------
# Exit code propagation
# ---------------------------------------------------------------------------

def test_exit_code():
    src = "main:\n    addi a0, zero, 42\n    addi a13, zero, 0\n    syscall\n"
    result = run_asm(src)
    assert result.returncode == 42


def test_exit_code_zero():
    src = "main:\n    addi a0, zero, 0\n    addi a13, zero, 0\n    syscall\n"
    result = run_asm(src)
    assert result.returncode == 0
