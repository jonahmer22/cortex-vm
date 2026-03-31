"""Tests for R-type and I-type ALU instructions."""
from conftest import asm_out, prog, print_int, print_uint


# ---------------------------------------------------------------------------
# I-type arithmetic (immediate)
# ---------------------------------------------------------------------------

def test_addi_positive():
    assert asm_out(prog(f"    addi t2, zero, 42\n{print_int()}")) == "42"


def test_addi_zero():
    assert asm_out(prog(f"    addi t2, zero, 0\n{print_int()}")) == "0"


def test_addi_chain():
    # 10 + 5 + 3 = 18
    src = "    addi t0, zero, 10\n    addi t1, t0, 5\n    addi t2, t1, 3\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "18"


def test_subi_positive_result():
    # 20 - 8 = 12
    src = "    addi t0, zero, 20\n    subi t2, t0, 8\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "12"


def test_subi_negative_result():
    # 0 - 5 = -5
    src = "    subi t2, zero, 5\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "-5"


def test_andi():
    # 0b1010 (10) AND 0b1100 (12) = 0b1000 (8)
    src = "    addi t0, zero, 0b1010\n    andi t2, t0, 0b1100\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "8"


def test_ori():
    # 0b1010 (10) OR 0b0101 (5) = 0b1111 (15)
    src = "    addi t0, zero, 0b1010\n    ori t2, t0, 0b0101\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "15"


def test_xori():
    # 0b1010 (10) XOR 0b1100 (12) = 0b0110 (6)
    src = "    addi t0, zero, 0b1010\n    xori t2, t0, 0b1100\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "6"


def test_slli():
    # 1 << 3 = 8
    src = "    addi t0, zero, 1\n    slli t2, t0, 3\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "8"


def test_srli():
    # 64 >> 2 = 16 (logical, fills with 0)
    src = "    addi t0, zero, 64\n    srli t2, t0, 2\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "16"


def test_srli_on_negative():
    # -8 (0xFFFFFFFFFFFFFFF8) >> 2 logical = large positive
    src = "    subi t0, zero, 8\n    srli t2, t0, 2\n"
    assert asm_out(prog(f"{src}{print_uint()}")) == "4611686018427387902"


def test_srai():
    # -8 >> 2 arithmetic = -2 (sign preserved)
    src = "    subi t0, zero, 8\n    srai t2, t0, 2\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "-2"


def test_srai_positive():
    # 16 >> 2 arithmetic = 4
    src = "    addi t0, zero, 16\n    srai t2, t0, 2\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "4"


# ---------------------------------------------------------------------------
# R-type arithmetic (register operands)
# ---------------------------------------------------------------------------

def test_add():
    src = "    addi t0, zero, 30\n    addi t1, zero, 12\n    add t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "42"


def test_add_negative():
    src = "    subi t0, zero, 5\n    addi t1, zero, 3\n    add t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "-2"


def test_sub():
    src = "    addi t0, zero, 60\n    addi t1, zero, 18\n    sub t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "42"


def test_sub_negative_result():
    src = "    addi t0, zero, 3\n    addi t1, zero, 10\n    sub t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "-7"


def test_and():
    src = "    addi t0, zero, 0b1010\n    addi t1, zero, 0b1100\n    and t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "8"


def test_or():
    src = "    addi t0, zero, 0b1010\n    addi t1, zero, 0b0101\n    or t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "15"


def test_xor():
    src = "    addi t0, zero, 0b1010\n    addi t1, zero, 0b1100\n    xor t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "6"


def test_xor_self_is_zero():
    src = "    addi t0, zero, 12345\n    xor t2, t0, t0\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "0"


def test_sll():
    src = "    addi t0, zero, 1\n    addi t1, zero, 4\n    sll t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "16"


def test_srl():
    src = "    addi t0, zero, 128\n    addi t1, zero, 3\n    srl t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "16"


def test_sra_negative():
    # -16 >> 2 arithmetic = -4
    src = "    subi t0, zero, 16\n    addi t1, zero, 2\n    sra t2, t0, t1\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "-4"


# ---------------------------------------------------------------------------
# Zero register is read-only (writes to r0/zero are ignored)
# ---------------------------------------------------------------------------

def test_zero_register_immutable():
    # Attempt to write 99 into zero, then read it back — should still be 0
    src = "    addi zero, zero, 99\n    addi t2, zero, 0\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "0"


# ---------------------------------------------------------------------------
# Hex and binary literals in immediates
# ---------------------------------------------------------------------------

def test_hex_literal():
    src = "    addi t2, zero, 0xFF\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "255"


def test_binary_literal():
    src = "    addi t2, zero, 0b110101\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "53"


def test_char_literal():
    src = "    addi t2, zero, 'A'\n"
    assert asm_out(prog(f"{src}{print_int()}")) == "65"
