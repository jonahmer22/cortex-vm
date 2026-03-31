"""Tests for the F extension (IEEE 754 double-precision floats)."""
from conftest import asm_out, prog, print_int, print_uint, print_float


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _load(label: str) -> str:
    return f"    lw t0, zero, {label}\n"


def _load2(l0: str, l1: str) -> str:
    return f"    lw t0, zero, {l0}\n    lw t1, zero, {l1}\n"


FLOATS = (
    "    v3:    3.0\n"
    "    v4:    4.0\n"
    "    v5:    5.0\n"
    "    v10:   10.0\n"
    "    v16:   16.0\n"
    "    vneg5: -5.0\n"
    "    v3_7:  3.7\n"
    "    v2_5:  2.5\n"
)


# ---------------------------------------------------------------------------
# FR-type: fadd, fsub, fmul, fdiv (register OP register)
# ---------------------------------------------------------------------------

def test_fadd():
    src = f"{_load2('v3', 'v4')}    fadd t2, t0, t1\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "7.000000"


def test_fsub():
    src = f"{_load2('v10', 'v3')}    fsub t2, t0, t1\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "7.000000"


def test_fsub_negative_result():
    src = f"{_load2('v3', 'v10')}    fsub t2, t0, t1\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "-7.000000"


def test_fmul():
    src = f"{_load2('v3', 'v4')}    fmul t2, t0, t1\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "12.000000"


def test_fdiv():
    src = f"{_load2('v10', 'v4')}    fdiv t2, t0, t1\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "2.500000"


def test_fdiv_by_itself():
    src = f"{_load('v5')}    fdiv t2, t0, t0\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "1.000000"


# ---------------------------------------------------------------------------
# FI-type: faddi, fsubi, fmuli, fdivi (register OP float-immediate)
# ---------------------------------------------------------------------------

def test_faddi():
    src = f"{_load('v5')}    faddi t2, t0, 2.5\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "7.500000"


def test_faddi_zero():
    src = f"{_load('v3')}    faddi t2, t0, 0.0\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "3.000000"


def test_fsubi():
    src = f"{_load('v10')}    fsubi t2, t0, 3.0\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "7.000000"


def test_fsubi_negative_result():
    src = f"{_load('v3')}    fsubi t2, t0, 10.0\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "-7.000000"


def test_fmuli():
    src = f"{_load('v4')}    fmuli t2, t0, 2.5\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "10.000000"


def test_fdivi():
    src = f"{_load('v10')}    fdivi t2, t0, 4.0\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "2.500000"


# ---------------------------------------------------------------------------
# FI-type unary: fsqrt, fabs, fneg
# ---------------------------------------------------------------------------

def test_fsqrt():
    src = f"{_load('v16')}    fsqrt t2, t0\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "4.000000"


def test_fsqrt_of_one():
    src = "    lw t0, zero, one\n    fsqrt t2, t0\n" + print_float()
    data = "    one: 1.0\n"
    assert asm_out(prog(src, data=data)) == "1.000000"


def test_fabs_negative():
    src = f"{_load('vneg5')}    fabs t2, t0\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "5.000000"


def test_fabs_positive():
    # fabs of a positive should be unchanged
    src = f"{_load('v3')}    fabs t2, t0\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "3.000000"


def test_fneg():
    src = f"{_load('v3')}    fneg t2, t0\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "-3.000000"


def test_fneg_double_negation():
    # fneg twice → original value
    src = f"{_load('v3')}    fneg t2, t0\n    fneg t2, t2\n{print_float()}"
    assert asm_out(prog(src, data=FLOATS)) == "3.000000"


# ---------------------------------------------------------------------------
# Conversion: ftoi, ftoui (float → integer)
# ---------------------------------------------------------------------------

def test_ftoi_truncates():
    src = f"{_load('v3_7')}    ftoi t2, t0\n    addi a1, zero, 0\n{print_int()}"
    assert asm_out(prog(src, data=FLOATS)) == "3"


def test_ftoi_exact():
    src = f"{_load('v4')}    ftoi t2, t0\n    addi a1, zero, 0\n{print_int()}"
    assert asm_out(prog(src, data=FLOATS)) == "4"


def test_ftoui_truncates():
    src = f"{_load('v3_7')}    ftoui t2, t0\n    addi a1, zero, 0\n{print_uint()}"
    assert asm_out(prog(src, data=FLOATS)) == "3"


# ---------------------------------------------------------------------------
# Conversion: itof, uitof (integer → float)
# ---------------------------------------------------------------------------

def test_itof():
    src = "    addi t0, zero, 42\n    itof t2, t0\n" + print_float()
    assert asm_out(prog(src)) == "42.000000"


def test_itof_negative():
    src = "    subi t0, zero, 7\n    itof t2, t0\n" + print_float()
    assert asm_out(prog(src)) == "-7.000000"


def test_itof_zero():
    src = "    addi t0, zero, 0\n    itof t2, t0\n" + print_float()
    assert asm_out(prog(src)) == "0.000000"


def test_uitof():
    src = "    addi t0, zero, 42\n    uitof t2, t0\n" + print_float()
    assert asm_out(prog(src)) == "42.000000"


def test_uitof_zero():
    src = "    addi t0, zero, 0\n    uitof t2, t0\n" + print_float()
    assert asm_out(prog(src)) == "0.000000"


# ---------------------------------------------------------------------------
# FB-type: fblt, fble, fbgt, fbge (float branches)
# Prints "1" if branch taken, "0" if not.
# ---------------------------------------------------------------------------

def _fbranch_prog(op: str, lv: str, rv: str) -> str:
    """Build a program that prints 1 if `op lv, rv` branches, 0 otherwise."""
    src = (
        f"    lw t0, zero, {lv}\n"
        f"    lw t1, zero, {rv}\n"
        f"    {op} t0, t1, taken\n"
        "    addi t2, zero, 0\n"
        "    jmp t9, zero, done\n"
        "taken:\n"
        "    addi t2, zero, 1\n"
        "done:\n"
        + print_int()
    )
    return prog(src, data=FLOATS)


def test_fblt_taken():
    assert asm_out(_fbranch_prog("fblt", "v3", "v5")) == "1"


def test_fblt_not_taken_equal():
    assert asm_out(_fbranch_prog("fblt", "v3", "v3")) == "0"


def test_fblt_not_taken_greater():
    assert asm_out(_fbranch_prog("fblt", "v5", "v3")) == "0"


def test_fble_taken_less():
    assert asm_out(_fbranch_prog("fble", "v3", "v5")) == "1"


def test_fble_taken_equal():
    assert asm_out(_fbranch_prog("fble", "v3", "v3")) == "1"


def test_fble_not_taken():
    assert asm_out(_fbranch_prog("fble", "v5", "v3")) == "0"


def test_fbgt_taken():
    assert asm_out(_fbranch_prog("fbgt", "v5", "v3")) == "1"


def test_fbgt_not_taken_equal():
    assert asm_out(_fbranch_prog("fbgt", "v3", "v3")) == "0"


def test_fbgt_not_taken_less():
    assert asm_out(_fbranch_prog("fbgt", "v3", "v5")) == "0"


def test_fbge_taken_greater():
    assert asm_out(_fbranch_prog("fbge", "v5", "v3")) == "1"


def test_fbge_taken_equal():
    assert asm_out(_fbranch_prog("fbge", "v3", "v3")) == "1"


def test_fbge_not_taken():
    assert asm_out(_fbranch_prog("fbge", "v3", "v5")) == "0"


# ---------------------------------------------------------------------------
# Float loop using fblt
# ---------------------------------------------------------------------------

def test_float_loop():
    # Add 1.0 to accumulator while acc < 5.0 → result = 5.0
    src = (
        "    lw t1, zero, one\n"
        "    lw t2, zero, limit\n"
        "    addi t0, zero, 0\n"
        "    itof t0, t0\n"          # t0 = 0.0
        "floop:\n"
        "    fadd t0, t0, t1\n"      # t0 += 1.0
        "    fblt t0, t2, floop\n"   # while t0 < 5.0
        + print_float(reg="t0")
    )
    data = "    one: 1.0\n    limit: 5.0\n"
    assert asm_out(prog(src, data=data)) == "5.000000"


# ---------------------------------------------------------------------------
# SYS_RAND_FLOAT (a13=24)
# ---------------------------------------------------------------------------

def test_rand_float_range():
    src = (
        "    addi a13, zero, 24\n"
        "    syscall\n"
        "    addi a1, zero, 6\n"
        "    addi a13, zero, 4\n"
        "    syscall\n"
    )
    out = asm_out(prog(src))
    assert 0.0 <= float(out) <= 1.0
