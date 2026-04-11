"""Tests for heap memory via SYS_HEAP_GROW (51) and SYS_HEAP_TOP (52)."""
from conftest import asm_out, prog, print_int, print_uint

SYS_HEAP_GROW = 51
SYS_HEAP_TOP  = 52


def heap_alloc(nwords: int, dest: str = "a0") -> str:
    """Emit instructions that call SYS_HEAP_GROW and leave the pointer in dest."""
    src = (
        f"    addi a0, zero, {nwords}\n"
        f"    addi a13, zero, {SYS_HEAP_GROW}\n"
        f"    syscall\n"
    )
    if dest != "a0":
        src += f"    addi {dest}, a0, 0\n"
    return src


# ---------------------------------------------------------------------------
# Basic allocation and store/load
# ---------------------------------------------------------------------------

def test_heap_alloc_returns_nonzero():
    # Allocating 1 word should return a non-zero address
    src = (
        heap_alloc(1, dest="t2")
    )
    assert asm_out(prog(f"{src}{print_uint('t2')}")) != "0"


def test_heap_store_load_basic():
    # Allocate 1 word, store a value, load it back
    src = (
        heap_alloc(1, dest="t0")
        + "    addi t1, zero, 42\n"
        + "    sw t0, t1, 0\n"
        + "    lw t2, t0, 0\n"
    )
    assert asm_out(prog(f"{src}{print_int()}")) == "42"


def test_heap_store_load_negative():
    src = (
        heap_alloc(1, dest="t0")
        + "    subi t1, zero, 99\n"
        + "    sw t0, t1, 0\n"
        + "    lw t2, t0, 0\n"
    )
    assert asm_out(prog(f"{src}{print_int()}")) == "-99"


def test_heap_store_load_zero():
    src = (
        heap_alloc(1, dest="t0")
        + "    addi t1, zero, 0\n"
        + "    sw t0, t1, 0\n"
        + "    lw t2, t0, 0\n"
    )
    assert asm_out(prog(f"{src}{print_int()}")) == "0"


# ---------------------------------------------------------------------------
# Multi-word allocations
# ---------------------------------------------------------------------------

def test_heap_store_load_multi_word():
    # Allocate 4 words, store distinct values at each offset, read them back
    src = (
        heap_alloc(4, dest="t0")
        + "    addi t1, zero, 10\n"
        + "    addi t2, zero, 20\n"
        + "    addi t3, zero, 30\n"
        + "    addi t4, zero, 40\n"
        + "    sw t0, t1, 0\n"
        + "    sw t0, t2, 1\n"
        + "    sw t0, t3, 2\n"
        + "    sw t0, t4, 3\n"
        + "    lw t1, t0, 0\n"
        + "    lw t2, t0, 1\n"
        + "    lw t3, t0, 2\n"
        + "    lw t4, t0, 3\n"
        + "    add t2, t1, t2\n"
        + "    add t2, t2, t3\n"
        + "    add t2, t2, t4\n"  # t2 = 10+20+30+40 = 100
    )
    assert asm_out(prog(f"{src}{print_int()}")) == "100"


def test_heap_words_are_independent():
    # Confirm adjacent words don't overwrite each other
    src = (
        heap_alloc(2, dest="t0")
        + "    addi t1, zero, 111\n"
        + "    addi t2, zero, 222\n"
        + "    sw t0, t1, 0\n"
        + "    sw t0, t2, 1\n"
        + "    lw t2, t0, 0\n"  # should still be 111
    )
    assert asm_out(prog(f"{src}{print_int()}")) == "111"


# ---------------------------------------------------------------------------
# Multiple allocations don't overlap
# ---------------------------------------------------------------------------

def test_heap_two_allocations_no_overlap():
    # Allocate two separate 1-word regions, write different values, verify both
    src = (
        heap_alloc(1, dest="t0")   # first allocation -> t0
        + heap_alloc(1, dest="t1") # second allocation -> t1
        + "    addi t2, zero, 7\n"
        + "    addi t3, zero, 13\n"
        + "    sw t0, t2, 0\n"
        + "    sw t1, t3, 0\n"
        + "    lw t4, t0, 0\n"    # should be 7
        + "    lw t5, t1, 0\n"    # should be 13
        + "    add t2, t4, t5\n"  # t2 = 20
    )
    assert asm_out(prog(f"{src}{print_int('t2')}")) == "20"


def test_heap_second_alloc_does_not_clobber_first():
    # Write to first allocation, then allocate again, confirm first is unchanged
    src = (
        heap_alloc(1, dest="t0")
        + "    addi t1, zero, 55\n"
        + "    sw t0, t1, 0\n"
        + heap_alloc(1, dest="t1")  # second alloc, discard pointer
        + "    lw t2, t0, 0\n"      # first alloc should still hold 55
    )
    assert asm_out(prog(f"{src}{print_int()}")) == "55"


# ---------------------------------------------------------------------------
# Heap and stack are independent
# ---------------------------------------------------------------------------

def test_heap_and_stack_independent():
    # Store different values on the heap and stack, confirm neither clobbers the other
    src = (
        heap_alloc(1, dest="t0")
        + "    addi t1, zero, 100\n"
        + "    addi t2, zero, 200\n"
        + "    sw t0, t1, 0\n"   # heap[0] = 100
        + "    sw sp, t2, 0\n"   # stack[sp] = 200
        + "    lw t3, t0, 0\n"   # t3 = heap[0] = 100
        + "    lw t4, sp, 0\n"   # t4 = stack[sp] = 200
        + "    add t2, t3, t4\n" # t2 = 300
    )
    assert asm_out(prog(f"{src}{print_int('t2')}")) == "300"


# ---------------------------------------------------------------------------
# Zero-word allocation
# ---------------------------------------------------------------------------

def test_heap_alloc_zero_words_returns_zero():
    # SYS_HEAP_GROW with 0 words should return 0 (failure/no-op)
    src = (
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 51\n"
        "    syscall\n"
        "    addi t2, a0, 0\n"
    )
    assert asm_out(prog(f"{src}{print_uint()}")) == "0"


# ---------------------------------------------------------------------------
# SYS_HEAP_TOP (syscall 52)
# ---------------------------------------------------------------------------

def test_heap_top_before_any_allocation():
    # Before any SYS_HEAP_GROW call, heap top offset from HEAP_ADDR should be 0.
    # Build HEAP_ADDR = 1 << 48 in registers, subtract from SYS_HEAP_TOP result.
    src = (
        f"    addi a13, zero, {SYS_HEAP_TOP}\n"
        "    syscall\n"                          # a0 = HEAP_ADDR + heapUsed
        "    addi t0, zero, 1\n"
        "    slli t0, t0, 48\n"                  # t0 = HEAP_ADDR
        "    sub t2, a0, t0\n"                   # t2 = heapUsed (should be 0)
    )
    assert asm_out(prog(f"{src}{print_uint()}")) == "0"


def test_heap_top_after_single_alloc():
    # After allocating N words, (SYS_HEAP_TOP - alloc_base) should equal N.
    src = (
        heap_alloc(5, dest="s0")                 # s0 = base of first alloc
        + f"    addi a13, zero, {SYS_HEAP_TOP}\n"
        + "    syscall\n"                         # a0 = HEAP_ADDR + 5
        + "    sub t2, a0, s0\n"                  # t2 = (HEAP_ADDR+5) - (HEAP_ADDR+0) = 5
    )
    assert asm_out(prog(f"{src}{print_uint()}")) == "5"


def test_heap_top_tracks_multiple_allocs():
    # Two allocations of 3 and 4 words: top offset from first base should be 7.
    src = (
        heap_alloc(3, dest="s0")                 # s0 = HEAP_ADDR + 0
        + heap_alloc(4, dest="s1")               # s1 = HEAP_ADDR + 3
        + f"    addi a13, zero, {SYS_HEAP_TOP}\n"
        + "    syscall\n"                         # a0 = HEAP_ADDR + 7
        + "    sub t2, a0, s0\n"                  # t2 = 7
    )
    assert asm_out(prog(f"{src}{print_uint()}")) == "7"


# ---------------------------------------------------------------------------
# Large allocation
# ---------------------------------------------------------------------------

def test_heap_large_allocation():
    # Allocate 1024 words, write to first and last, read both back
    src = (
        heap_alloc(1024, dest="t0")
        + "    addi t1, zero, 1\n"
        + "    addi t2, zero, 1023\n"
        + "    sw t0, t1, 0\n"       # heap[0] = 1
        + "    lw t2, t0, 0\n"       # t2 = 1 (index 0)
        + "    addi t3, t0, 1023\n"
        + "    addi t4, zero, 99\n"
        + "    sw t3, t4, 0\n"       # heap[1023] = 99
        + "    lw t5, t3, 0\n"       # t5 = 99
        + "    add t2, t2, t5\n"     # t2 = 100
    )
    assert asm_out(prog(f"{src}{print_int()}")) == "100"
