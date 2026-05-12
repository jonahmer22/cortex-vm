"""
Tests for the persistent VM context API:
  cortexVMCreate / cortexVMExecSource / cortexVMExecBinary / cortexVMDestroy

Each test compiles a small C harness against lib/libcortex-vm.a and exercises
the API in ways that cannot be tested through the CLI (state persistence across
runs, multi-VM isolation, error handling, etc.).
"""

import os
import subprocess
import tempfile

import pytest

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIB_DIR      = os.path.join(PROJECT_ROOT, "lib")
LIB_A        = os.path.join(LIB_DIR, "libcortex-vm.a")
LIB_H        = os.path.join(LIB_DIR, "cortex-vm.h")
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

def compile_and_run(c_source: str, stdin: str = "", timeout: int = 10) -> subprocess.CompletedProcess:
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, "harness.c")
        exe = os.path.join(tmp, "harness")
        with open(src, "w") as f:
            f.write(c_source)
        compile_result = subprocess.run(
            [CC, f"-I{LIB_DIR}", src, LIB_A, "-lm", "-o", exe],
            capture_output=True, text=True
        )
        assert compile_result.returncode == 0, (
            f"Compilation failed:\n{compile_result.stderr}"
        )
        return subprocess.run(
            [exe], input=stdin, capture_output=True, text=True, timeout=timeout
        )


# ---------------------------------------------------------------------------
# Lifecycle
# ---------------------------------------------------------------------------

def test_vm_create_and_destroy():
    """cortexVMCreate / cortexVMDestroy must not crash or leak."""
    c = r"""
#include "cortex-vm.h"
int main(void) {
    CortexVM *vm = cortexVMCreate();
    if (!vm) return 1;
    cortexVMDestroy(vm);
    return 0;
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0


def test_vm_exec_source_basic():
    """cortexVMExecSource runs a program and produces output."""
    c = r"""
#include "cortex-vm.h"
int main(void) {
    CortexVM *vm = cortexVMCreate();
    int code = cortexVMExecSource(vm,
        "main:\n"
        "    addi a0, zero, msg\n"
        "    addi a13, zero, 5\n"
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
        ".data\n"
        "    msg: \"hello from vm\n\"\n"
    );
    cortexVMDestroy(vm);
    return code;
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout == "hello from vm\n"


def test_vm_exec_source_exit_code():
    """Exit code from SYS_EXIT is returned, not used to kill the process."""
    c = r"""
#include <stdio.h>
#include "cortex-vm.h"
int main(void) {
    CortexVM *vm = cortexVMCreate();
    int code = cortexVMExecSource(vm,
        "main:\n"
        "    addi a0, zero, 77\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );
    /* if SYS_EXIT had killed the process, we would never reach here */
    printf("code=%d\n", code);
    cortexVMDestroy(vm);
    return 0;
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout.strip() == "code=77"


def test_vm_exec_binary():
    """cortexVMExecBinary runs a pre-assembled binary."""
    c = r"""
#include <stdlib.h>
#include "cortex-vm.h"
int main(void) {
    uint64_t *binary = cortexAssemble(
        "main:\n"
        "    addi a0, zero, msg\n"
        "    addi a13, zero, 5\n"
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
        ".data\n"
        "    msg: \"from binary\n\"\n",
        "a.out"
    );
    CortexVM *vm = cortexVMCreate();
    int code = cortexVMExecBinary(vm, binary, binary[1]);
    cortexVMDestroy(vm);
    free(binary);
    return code;
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout == "from binary\n"


# ---------------------------------------------------------------------------
# Assembly failure
# ---------------------------------------------------------------------------

def test_vm_exec_source_bad_source_exits_nonzero():
    """
    Passing invalid source to cortexVMExecSource causes a non-zero exit.
    The assembler calls exit() internally on fatal errors (rather than returning),
    so the harness process itself exits non-zero rather than printing a return code.
    """
    c = r"""
#include "cortex-vm.h"
int main(void) {
    CortexVM *vm = cortexVMCreate();
    cortexVMExecSource(vm, "this is not valid assembly @@@@\n");
    cortexVMDestroy(vm);
    return 0;
}
"""
    result = compile_and_run(c)
    assert result.returncode != 0


# ---------------------------------------------------------------------------
# State persistence across runs
# ---------------------------------------------------------------------------

def test_vm_register_state_persists():
    """
    Registers written in one run are visible in the next run on the same context.
    Run 1 writes 42 into s0 (r4) then exits.
    Run 2 reads s0 and prints it -- must print 42.
    """
    c = r"""
#include <stdio.h>
#include "cortex-vm.h"
int main(void) {
    CortexVM *vm = cortexVMCreate();

    /* run 1: store 42 in s0 and exit */
    cortexVMExecSource(vm,
        "main:\n"
        "    addi s0, zero, 42\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );

    /* run 2: print s0 -- should still be 42 */
    cortexVMExecSource(vm,
        "main:\n"
        "    addi a0, s0, 0\n"
        "    addi a1, zero, 0\n"
        "    addi a13, zero, 1\n"
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );

    cortexVMDestroy(vm);
    return 0;
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout.strip() == "42"


def test_vm_heap_persists():
    """
    Heap allocations and their contents survive across runs on the same context.
    Run 1 allocates 1 word, stores 99 at that address, exits.
    Run 2 loads from that address and prints it -- must print 99.
    The heap address is communicated between runs via a callee-saved register (s1).
    """
    c = r"""
#include <stdio.h>
#include "cortex-vm.h"
int main(void) {
    CortexVM *vm = cortexVMCreate();

    /* run 1: alloc 1 word on heap, store 99, save heap address in s1 */
    cortexVMExecSource(vm,
        "main:\n"
        "    addi a0, zero, 1\n"
        "    addi a13, zero, 51\n"
        "    syscall\n"
        "    addi s1, a0, 0\n"
        "    addi t0, zero, 99\n"
        "    sw   s1, t0, 0\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );

    /* run 2: load from s1 (heap address from run 1) and print */
    cortexVMExecSource(vm,
        "main:\n"
        "    lw   t0, s1, 0\n"
        "    addi a0, t0, 0\n"
        "    addi a1, zero, 0\n"
        "    addi a13, zero, 1\n"
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );

    cortexVMDestroy(vm);
    return 0;
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout.strip() == "99"


def test_vm_stack_memory_persists():
    """
    Stack memory written in one run is readable in the next run on the same context.
    sp is not modified between runs, so both runs address the same slot.
    """
    c = r"""
#include <stdio.h>
#include "cortex-vm.h"
int main(void) {
    CortexVM *vm = cortexVMCreate();

    /* run 1: store 55 at sp+0 */
    cortexVMExecSource(vm,
        "main:\n"
        "    addi t0, zero, 55\n"
        "    sw   sp, t0, 0\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );

    /* run 2: load from sp+0 -- should still be 55 */
    cortexVMExecSource(vm,
        "main:\n"
        "    lw   t0, sp, 0\n"
        "    addi a0, t0, 0\n"
        "    addi a1, zero, 0\n"
        "    addi a13, zero, 1\n"
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );

    cortexVMDestroy(vm);
    return 0;
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout.strip() == "55"


def test_vm_sequential_runs_accumulate():
    """
    Three sequential runs on the same context, each incrementing a counter in s0.
    Final value must be 3.
    """
    c = r"""
#include <stdio.h>
#include "cortex-vm.h"
int main(void) {
    CortexVM *vm = cortexVMCreate();

    const char *increment =
        "main:\n"
        "    addi s0, s0, 1\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n";

    cortexVMExecSource(vm, increment);
    cortexVMExecSource(vm, increment);
    cortexVMExecSource(vm, increment);

    /* now print s0 */
    cortexVMExecSource(vm,
        "main:\n"
        "    addi a0, s0, 0\n"
        "    addi a1, zero, 0\n"
        "    addi a13, zero, 1\n"
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );

    cortexVMDestroy(vm);
    return 0;
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout.strip() == "3"


# ---------------------------------------------------------------------------
# Isolation between contexts
# ---------------------------------------------------------------------------

def test_vm_two_contexts_are_isolated():
    """
    Two independent VM contexts must not share register state.
    vm_a writes 100 into s0; vm_b writes 200 into s0.
    A third run on vm_a must still see 100, not 200.
    """
    c = r"""
#include <stdio.h>
#include "cortex-vm.h"
int main(void) {
    CortexVM *vm_a = cortexVMCreate();
    CortexVM *vm_b = cortexVMCreate();

    cortexVMExecSource(vm_a,
        "main:\n"
        "    addi s0, zero, 100\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );
    cortexVMExecSource(vm_b,
        "main:\n"
        "    addi s0, zero, 200\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );

    /* read s0 back from vm_a -- must be 100, not 200 */
    cortexVMExecSource(vm_a,
        "main:\n"
        "    addi a0, s0, 0\n"
        "    addi a1, zero, 0\n"
        "    addi a13, zero, 1\n"
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );

    cortexVMDestroy(vm_a);
    cortexVMDestroy(vm_b);
    return 0;
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout.strip() == "100"


def test_vm_two_contexts_heap_isolated():
    """
    Heap in vm_a and vm_b are independent; writing to vm_a's heap must not
    affect vm_b's heap, and vice versa.
    """
    c = r"""
#include <stdio.h>
#include "cortex-vm.h"
int main(void) {
    CortexVM *vm_a = cortexVMCreate();
    CortexVM *vm_b = cortexVMCreate();

    /* vm_a: alloc 1 word, write 11, save address in s0 */
    cortexVMExecSource(vm_a,
        "main:\n"
        "    addi a0, zero, 1\n"
        "    addi a13, zero, 51\n"
        "    syscall\n"
        "    addi s0, a0, 0\n"
        "    addi t0, zero, 11\n"
        "    sw   s0, t0, 0\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );

    /* vm_b: alloc 1 word, write 22, save address in s0 */
    cortexVMExecSource(vm_b,
        "main:\n"
        "    addi a0, zero, 1\n"
        "    addi a13, zero, 51\n"
        "    syscall\n"
        "    addi s0, a0, 0\n"
        "    addi t0, zero, 22\n"
        "    sw   s0, t0, 0\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );

    /* vm_a: reload from s0 -- must still be 11 */
    cortexVMExecSource(vm_a,
        "main:\n"
        "    lw   t0, s0, 0\n"
        "    addi a0, t0, 0\n"
        "    addi a1, zero, 0\n"
        "    addi a13, zero, 1\n"
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a13, zero, 0\n"
        "    syscall\n"
    );

    cortexVMDestroy(vm_a);
    cortexVMDestroy(vm_b);
    return 0;
}
"""
    result = compile_and_run(c)
    assert result.returncode == 0
    assert result.stdout.strip() == "11"
