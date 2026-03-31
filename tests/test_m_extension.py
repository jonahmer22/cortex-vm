"""Tests for the M extension (multiply / divide / remainder)."""
from conftest import asm_out, prog, print_int, print_uint


# ---------------------------------------------------------------------------
# MUL / MULI — lower 64 bits of signed multiplication
# ---------------------------------------------------------------------------

def test_mul_basic():
    src = "    addi t0, zero, 6\n    addi t1, zero, 7\n    mul t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "42"


def test_mul_by_zero():
    src = "    addi t0, zero, 99\n    mul t2, t0, zero\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "0"


def test_mul_negative():
    # -3 * 5 = -15
    src = "    subi t0, zero, 3\n    addi t1, zero, 5\n    mul t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "-15"


def test_mul_both_negative():
    # -4 * -6 = 24
    src = "    subi t0, zero, 4\n    subi t1, zero, 6\n    mul t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "24"


def test_muli():
    src = "    addi t0, zero, 10\n    muli t2, t0, 5\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "50"


def test_muli_by_one():
    src = "    addi t0, zero, 77\n    muli t2, t0, 1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "77"


def test_muli_negative_imm():
    # 10 * -3 = -30
    src = "    addi t0, zero, 10\n    muli t2, t0, -3\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "-30"


# ---------------------------------------------------------------------------
# MULH / MULHU — upper 64 bits of multiplication
# ---------------------------------------------------------------------------

def test_mulh_small_numbers_zero():
    # 3 * 4 = 12 (fits in 64 bits) → upper bits = 0
    src = "    addi t0, zero, 3\n    addi t1, zero, 4\n    mulh t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "0"


def test_mulhu_small_numbers_zero():
    src = "    addi t0, zero, 5\n    addi t1, zero, 6\n    mulhu t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "0"


def test_mulhu_overflow():
    # 2^63 * 2 = 2^64; upper 64 bits = 1
    # Load 2^63 via shift: 1 << 63
    src = (
        "    addi t0, zero, 1\n"
        "    slli t0, t0, 63\n"
        "    addi t1, zero, 2\n"
        "    mulhu t2, t0, t1\n"
    )
    assert asm_out(prog(f"{src}{print_uint()}")) == "1"


def test_mulh_negative_overflow():
    # -1 * -1 = 1; upper 64 bits of 128-bit result = 0 (not -1)
    src = "    subi t0, zero, 1\n    subi t1, zero, 1\n    mulh t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "0"


# ---------------------------------------------------------------------------
# DIV / DIVI — signed division (truncates toward zero)
# ---------------------------------------------------------------------------

def test_div_basic():
    src = "    addi t0, zero, 100\n    addi t1, zero, 7\n    div t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "14"


def test_divi():
    src = "    addi t0, zero, 100\n    divi t2, t0, 5\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "20"


def test_div_negative_dividend():
    # -10 / 3 = -3 (truncates toward zero)
    src = "    subi t0, zero, 10\n    addi t1, zero, 3\n    div t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "-3"


def test_div_negative_divisor():
    # 10 / -3 = -3 (truncates toward zero)
    src = "    addi t0, zero, 10\n    subi t1, zero, 3\n    div t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "-3"


def test_div_exact():
    src = "    addi t0, zero, 42\n    addi t1, zero, 6\n    div t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "7"


# ---------------------------------------------------------------------------
# DIVU / DIVUI — unsigned division
# ---------------------------------------------------------------------------

def test_divu():
    src = "    addi t0, zero, 100\n    addi t1, zero, 7\n    divu t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "14"


def test_divui():
    src = "    addi t0, zero, 100\n    divui t2, t0, 4\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "25"


def test_divui_by_one():
    src = "    addi t0, zero, 99\n    divui t2, t0, 1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "99"


# ---------------------------------------------------------------------------
# REM / REMI — signed remainder
# ---------------------------------------------------------------------------

def test_rem_basic():
    src = "    addi t0, zero, 100\n    addi t1, zero, 7\n    rem t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "2"


def test_remi():
    src = "    addi t0, zero, 100\n    remi t2, t0, 6\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "4"


def test_rem_exact_divisible():
    src = "    addi t0, zero, 42\n    addi t1, zero, 7\n    rem t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "0"


def test_rem_negative_dividend():
    # C remainder: -10 % 3 = -1
    src = "    subi t0, zero, 10\n    addi t1, zero, 3\n    rem t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "-1"


# ---------------------------------------------------------------------------
# REMU / REMUI — unsigned remainder
# ---------------------------------------------------------------------------

def test_remu():
    src = "    addi t0, zero, 100\n    addi t1, zero, 7\n    remu t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "2"


def test_remui():
    src = "    addi t0, zero, 100\n    remui t2, t0, 9\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "1"


def test_remui_by_power_of_two():
    # 100 % 8 = 4
    src = "    addi t0, zero, 100\n    remui t2, t0, 8\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "4"
