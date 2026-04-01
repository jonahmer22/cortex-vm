"""
Tests for the cortex-vm embedding API (libcortex-vm.a + cortex-vm.h).

Each test compiles a small C harness against lib/libcortex-vm.a and runs it,
verifying stdout/exit code — the same pattern used by every other test in this
suite, just one layer deeper.
"""

import os
import subprocess
import tempfile

import pytest

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIB_DIR      = os.path.join(PROJECT_ROOT, "lib")
LIB_A        = os.path.join(LIB_DIR, "libcortex-vm.a")
LIB_H        = os.path.join(LIB_DIR, "cortex-vm.h")
INC_DIR      = os.path.join(PROJECT_ROOT, "include")   # needed for cortexExecBinary test
CC           = "gcc"


# ---------------------------------------------------------------------------
# Session fixture: skip entire module if the library hasn't been built
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session", autouse=True)
def require_library():
    if not os.path.isfile(LIB_A) or not os.path.isfile(LIB_H):
        pytest.skip(
            "libcortex-vm.a / cortex-vm.h not found in lib/. Run `make lib` first."
        )


# ---------------------------------------------------------------------------
# Helper
# ---------------------------------------------------------------------------

def compile_and_run(c_source: str, extra_inc: list[str] = None, stdin: str = "", timeout: int = 10) -> subprocess.CompletedProcess:
    """
    Write c_source to a temp file, compile against libcortex-vm.a, run, return
    CompletedProcess.  extra_inc is a list of additional -I paths needed beyond
    lib/ (used only when the test reaches into internal headers).
    """
    inc_flags = [f"-I{LIB_DIR}"]
    if extra_inc:
        inc_flags += [f"-I{p}" for p in extra_inc]

    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, "harness.c")
        exe = os.path.join(tmp, "harness")

        with open(src, "w") as f:
            f.write(c_source)

        compile_result = subprocess.run(
            [CC] + inc_flags + [src, LIB_A, "-lm", "-o", exe],
            capture_output=True, text=True
        )
        assert compile_result.returncode == 0, (
            f"Compilation failed:\n{compile_result.stderr}"
        )

        return subprocess.run(
            [exe],
            input=stdin,
            capture_output=True,
            text=True,
            timeout=timeout,
        )


# ---------------------------------------------------------------------------
# cortexExecSource tests
# ---------------------------------------------------------------------------

def test_exec_source_hello_world():
    c = r"""
#include "cortex-vm.h"
int main(void) {
    return cortexExecSource(
        "main:\n"
        "    addi a0, zero, msg\n"
        "    addi a13, zero, 5\n"
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
        ".data\n"
        "    msg: \"Hello, World!\\n\"\n"
    );
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout == "Hello, World!\n"


def test_exec_source_exit_code():
    c = r"""
#include "cortex-vm.h"
int main(void) {
    return cortexExecSource(
        "main:\n"
        "    addi a0, zero, 42\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );
}
"""
    result = compile_and_run(c)
    assert result.returncode == 42


def test_exec_source_print_int():
    c = r"""
#include "cortex-vm.h"
int main(void) {
    return cortexExecSource(
        "main:\n"
        "    addi t0, zero, 0\n"
        "    addi t1, zero, 1\n"
        "    addi t2, zero, 11\n"
        "loop:\n"
        "    add  t0, t0, t1\n"
        "    addi t1, t1, 1\n"
        "    blt  t1, t2, loop\n"
        "    addi a0, t0, 0\n"
        "    addi a1, zero, 0\n"
        "    addi a13, zero, 1\n"
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout == "55"


def test_exec_source_m_extension():
    c = r"""
#include "cortex-vm.h"
int main(void) {
    return cortexExecSource(
        "main:\n"
        "    addi t0, zero, 6\n"
        "    addi t1, zero, 7\n"
        "    mul  t2, t0, t1\n"
        "    addi a0, t2, 0\n"
        "    addi a1, zero, 0\n"
        "    addi a13, zero, 1\n"
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout == "42"


def test_exec_source_f_extension():
    c = r"""
#include "cortex-vm.h"
int main(void) {
    return cortexExecSource(
        "main:\n"
        "    faddi t0, zero, 1.5\n"
        "    faddi t1, zero, 2.5\n"
        "    fadd  t2, t0, t1\n"
        "    addi  a0, t2, 0\n"
        "    addi  a1, zero, 1\n"
        "    addi  a13, zero, 4\n"
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout == "4.0"


def test_exec_source_data_section():
    c = r"""
#include "cortex-vm.h"
int main(void) {
    return cortexExecSource(
        "main:\n"
        "    addi a0, zero, greeting\n"
        "    addi a13, zero, 5\n"
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
        ".data\n"
        "    greeting: \"embedded\\n\"\n"
    );
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout == "embedded\n"


def test_exec_source_returns_exit_code_zero():
    c = r"""
#include <stdio.h>
#include "cortex-vm.h"
int main(void) {
    int code = cortexExecSource(
        "main:\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );
    printf("%d\n", code);
    return 0;
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout.strip() == "0"


def test_exec_source_exit_code_captured_not_process_exit():
    """cortexExecSource returns the exit code; it must not call exit() itself."""
    c = r"""
#include <stdio.h>
#include "cortex-vm.h"
int main(void) {
    int code = cortexExecSource(
        "main:\n"
        "    addi a0, zero, 7\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );
    /* if cortexExecSource called exit(7) we would never reach here */
    printf("after: %d\n", code);
    return 0;
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout.strip() == "after: 7"


# ---------------------------------------------------------------------------
# cortexExecBinary test (needs assembler.h from include/)
# ---------------------------------------------------------------------------

def test_exec_binary():
    """Assemble a program manually then run it via cortexExecBinary."""
    c = r"""
#include <stdlib.h>
#include "cortex-vm.h"
#include "assembler.h"
int main(void) {
    const char *src =
        "main:\n"
        "    addi a0, zero, msg\n"
        "    addi a13, zero, 5\n"
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
        ".data\n"
        "    msg: \"from binary\\n\"\n";

    uint64_t *binary = assemble(src, "", 1);
    int code = cortexExecBinary(binary, binary[1]);
    free(binary);
    return code;
}
"""
    result = compile_and_run(c, extra_inc=[INC_DIR])
    assert result.returncode == 0
    assert result.stdout == "from binary\n"
