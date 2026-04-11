"""Tests for file I/O syscalls (SYS_FILE_OPEN=31, SYS_FILE_READ=32,
SYS_FILE_CLOSE=33, SYS_FILE_WRITE=34).

SYS_FILE_READ API (post-fix):
  a0 = file descriptor
  a1 = destination buffer address (heap or stack)
  returns a0 = number of words written (not counting null terminator)
"""
import os
import pytest
from conftest import asm_out, run_asm, prog


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _open_read_close(path: str, buf_reg: str = "s1") -> str:
    """
    Emit assembly that:
      1. Allocates a heap buffer large enough (256 words)
      2. Opens `path` (embedded in .data) for reading
      3. Calls SYS_FILE_READ into the heap buffer
      4. Closes the fd
    Leaves the buffer address in buf_reg and word-count in s2.
    """
    return (
        # allocate a 256-word heap buffer
        "    addi a0, zero, 256\n"
        "    addi a13, zero, 51\n"   # SYS_HEAP_GROW
        "    syscall\n"
        f"    addi {buf_reg}, a0, 0\n"  # save buf address

        # open the file
        "    addi a0, zero, path\n"
        "    addi a1, zero, 0\n"     # mode=read
        "    addi a13, zero, 31\n"   # SYS_FILE_OPEN
        "    syscall\n"
        "    addi s0, a0, 0\n"       # save fd

        # read into heap buffer
        "    addi a0, s0, 0\n"       # fd
        f"    addi a1, {buf_reg}, 0\n"  # buf addr
        "    addi a13, zero, 32\n"   # SYS_FILE_READ
        "    syscall\n"
        "    addi s2, a0, 0\n"       # save word count

        # close
        "    addi a0, s0, 0\n"
        "    addi a13, zero, 33\n"   # SYS_FILE_CLOSE
        "    syscall\n"
    )


# ---------------------------------------------------------------------------
# SYS_FILE_READ — reads into heap buffer
# ---------------------------------------------------------------------------

def test_file_read_simple_string(tmp_path):
    """Read a short ASCII file into a heap buffer and print it."""
    p = tmp_path / "data.txt"
    p.write_text("hello")

    src = prog(
        _open_read_close(str(p), buf_reg="s1")
        + "    addi a0, s1, 0\n"
        + "    addi a13, zero, 5\n"    # SYS_PRINT_STR
        + "    syscall\n",
        data=f'    path: "{p}"\n',
    )
    assert asm_out(src) == "hello"


def test_file_read_returns_word_count(tmp_path):
    """SYS_FILE_READ return value equals the number of characters in the file."""
    p = tmp_path / "abc.txt"
    p.write_text("abcde")   # 5 chars → should return 5

    src = prog(
        _open_read_close(str(p), buf_reg="s1")
        + "    addi a0, s2, 0\n"       # s2 holds word count
        + "    addi a1, zero, 0\n"
        + "    addi a13, zero, 2\n"    # SYS_PRINT_UINT
        + "    syscall\n",
        data=f'    path: "{p}"\n',
    )
    assert asm_out(src) == "5"


def test_file_read_empty_file(tmp_path):
    """Reading an empty file writes only the null terminator; returns 0."""
    p = tmp_path / "empty.txt"
    p.write_text("")

    src = prog(
        _open_read_close(str(p), buf_reg="s1")
        + "    addi a0, s2, 0\n"
        + "    addi a1, zero, 0\n"
        + "    addi a13, zero, 2\n"    # SYS_PRINT_UINT
        + "    syscall\n",
        data=f'    path: "{p}"\n',
    )
    assert asm_out(src) == "0"


def test_file_read_into_stack_buffer(tmp_path):
    """SYS_FILE_READ also works when the destination is a stack address."""
    p = tmp_path / "hi.txt"
    p.write_text("hi")

    src = prog(
        # open
        "    addi a0, zero, path\n"
        "    addi a1, zero, 0\n"
        "    addi a13, zero, 31\n"     # SYS_FILE_OPEN
        "    syscall\n"
        "    addi s0, a0, 0\n"         # save fd

        # read into stack at sp
        "    addi a0, s0, 0\n"
        "    addi a1, sp, 0\n"         # stack buffer at sp
        "    addi a13, zero, 32\n"     # SYS_FILE_READ
        "    syscall\n"

        # close
        "    addi a0, s0, 0\n"
        "    addi a13, zero, 33\n"
        "    syscall\n"

        # print from stack
        "    addi a0, sp, 0\n"
        "    addi a13, zero, 5\n"      # SYS_PRINT_STR
        "    syscall\n",
        data=f'    path: "{p}"\n',
    )
    assert asm_out(src) == "hi"


# ---------------------------------------------------------------------------
# SYS_FILE_WRITE — round-trip: write then read back
# ---------------------------------------------------------------------------

def test_file_write_and_read_back(tmp_path):
    """Write a string to a file with SYS_FILE_WRITE then read it back."""
    out_path = tmp_path / "out.txt"

    src = prog(
        # open for write
        "    addi a0, zero, wpath\n"
        "    addi a1, zero, 1\n"       # mode=write
        "    addi a13, zero, 31\n"
        "    syscall\n"
        "    addi s0, a0, 0\n"

        # write the string
        "    addi a0, s0, 0\n"
        "    addi a1, zero, msg\n"
        "    addi a13, zero, 34\n"     # SYS_FILE_WRITE
        "    syscall\n"

        # close
        "    addi a0, s0, 0\n"
        "    addi a13, zero, 33\n"
        "    syscall\n"

        # re-open for read
        "    addi a0, zero, wpath\n"
        "    addi a1, zero, 0\n"       # mode=read
        "    addi a13, zero, 31\n"
        "    syscall\n"
        "    addi s0, a0, 0\n"

        # allocate heap buffer and read
        "    addi a0, zero, 64\n"
        "    addi a13, zero, 51\n"     # SYS_HEAP_GROW
        "    syscall\n"
        "    addi s1, a0, 0\n"

        "    addi a0, s0, 0\n"
        "    addi a1, s1, 0\n"
        "    addi a13, zero, 32\n"     # SYS_FILE_READ
        "    syscall\n"

        "    addi a0, s0, 0\n"
        "    addi a13, zero, 33\n"     # close
        "    syscall\n"

        # print contents
        "    addi a0, s1, 0\n"
        "    addi a13, zero, 5\n"      # SYS_PRINT_STR
        "    syscall\n",
        data=(
            f'    wpath: "{out_path}"\n'
            '    msg: "cortex"\n'
        ),
    )
    assert asm_out(src) == "cortex"
