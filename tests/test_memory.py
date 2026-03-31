"""Tests for S-type (store) and L-type (load) instructions, and the .data section."""
from conftest import asm_out, prog, print_int, print_uint


# ---------------------------------------------------------------------------
# Stack store/load round-trips
# ---------------------------------------------------------------------------

def test_sw_lw_stack_basic():
    src = (
        "    addi t0, zero, 99\n"
        "    sw sp, t0, 0\n"
        "    lw t2, sp, 0\n"
    )
    assert asm_out(prog(f"{src}{print_int()}")) == "99"


def test_sw_lw_stack_two_values():
    # Push two values, pop in LIFO order
    src = (
        "    addi t0, zero, 10\n"
        "    addi t1, zero, 20\n"
        "    sw sp, t0, 0\n"      # stack[sp+0] = 10
        "    sw sp, t1, 1\n"      # stack[sp+1] = 20
        "    lw t2, sp, 1\n"      # t2 = 20
        "    lw t3, sp, 0\n"      # t3 = 10
        "    add t2, t2, t3\n"    # t2 = 30
    )
    assert asm_out(prog(f"{src}{print_int()}")) == "30"


def test_sw_lw_negative_value():
    src = (
        "    subi t0, zero, 42\n"
        "    sw sp, t0, 0\n"
        "    lw t2, sp, 0\n"
    )
    assert asm_out(prog(f"{src}{print_int()}")) == "-42"


def test_sw_lw_with_sp_increment():
    # Classic push/pop using sp increment
    src = (
        "    addi t0, zero, 123\n"
        "    sw sp, t0, 0\n"
        "    addi sp, sp, 1\n"
        "    subi sp, sp, 1\n"
        "    lw t2, sp, 0\n"
    )
    assert asm_out(prog(f"{src}{print_int()}")) == "123"


# ---------------------------------------------------------------------------
# .data section: integer literals
# ---------------------------------------------------------------------------

def test_lw_data_integer():
    src = "    lw t2, zero, my_val\n"
    data = "    my_val: 1234\n"
    assert asm_out(prog(f"{src}{print_int()}", data=data)) == "1234"


def test_lw_data_negative_integer():
    src = "    lw t2, zero, my_val\n"
    data = "    my_val: -99\n"
    assert asm_out(prog(f"{src}{print_int()}", data=data)) == "-99"


def test_lw_data_hex_integer():
    src = "    lw t2, zero, my_val\n"
    data = "    my_val: 0xFF\n"
    assert asm_out(prog(f"{src}{print_int()}", data=data)) == "255"


# ---------------------------------------------------------------------------
# .data section: strings (SYS_PRINT_STR)
# ---------------------------------------------------------------------------

def test_print_str_from_data():
    src = (
        "    addi a0, zero, greeting\n"
        "    addi a13, zero, 5\n"
        "    syscall\n"
    )
    data = '    greeting: "hello"\n'
    assert asm_out(prog(src, data=data)) == "hello"


def test_print_str_with_escape():
    src = (
        "    addi a0, zero, msg\n"
        "    addi a13, zero, 5\n"
        "    syscall\n"
    )
    data = '    msg: "hi\\n"\n'
    assert asm_out(prog(src, data=data)) == "hi\n"


def test_print_multiple_strings():
    src = (
        "    addi a0, zero, s1\n"
        "    addi a13, zero, 5\n"
        "    syscall\n"
        "    addi a0, zero, s2\n"
        "    addi a13, zero, 5\n"
        "    syscall\n"
    )
    data = '    s1: "foo"\n    s2: "bar"\n'
    assert asm_out(prog(src, data=data)) == "foobar"


# ---------------------------------------------------------------------------
# .data section: float literals (load and print)
# ---------------------------------------------------------------------------

def test_lw_data_float():
    from conftest import print_float
    src = "    lw t2, zero, pi\n"
    data = "    pi: 3.14\n"
    # 3.14 stored as double, loaded into t2, printed with 2 decimal places
    assert asm_out(prog(f"{src}{print_float(precision=2)}", data=data)) == "3.14"


def test_lw_data_float_negative():
    from conftest import print_float
    src = "    lw t2, zero, val\n"
    data = "    val: -5.0\n"
    assert asm_out(prog(f"{src}{print_float(precision=1)}", data=data)) == "-5.0"
