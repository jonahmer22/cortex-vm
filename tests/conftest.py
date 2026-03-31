"""Shared fixtures and utilities for cortex-vm pytest suite."""
import os
import subprocess
import tempfile

import pytest

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VM_BINARY = os.path.join(PROJECT_ROOT, "cortex-vm")


def run_asm(source: str, stdin: str = "", timeout: int = 10) -> subprocess.CompletedProcess:
    """Write assembly source to a temp file, assemble and run it, return CompletedProcess."""
    with tempfile.NamedTemporaryFile(mode="w", suffix=".s", delete=False) as f:
        f.write(source)
        path = f.name
    try:
        return subprocess.run(
            [VM_BINARY, "-a", path],
            input=stdin,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    finally:
        os.unlink(path)


def asm_out(source: str, stdin: str = "") -> str:
    """Assemble, run, and return stdout. Fails the test if the VM crashes."""
    result = run_asm(source, stdin=stdin)
    assert result.returncode == 0, (
        f"VM exited with code {result.returncode}\nstderr:\n{result.stderr}"
    )
    return result.stdout


# ---------------------------------------------------------------------------
# Assembly template helpers
# ---------------------------------------------------------------------------

def _exit0() -> str:
    return "    addi a0, zero, 0\n    addi a13, zero, 0\n    syscall\n"


def prog(body: str, data: str = "") -> str:
    """
    Wrap body in a main: label with a SYS_EXIT(0) footer.
    Optionally append a .data section.
    """
    data_section = f"\n.data\n{data}" if data else ""
    return f"main:\n{body}\n{_exit0()}{data_section}"


def print_int(reg: str = "t2") -> str:
    """Emit instructions that print `reg` as a signed decimal integer."""
    move = f"    addi a0, {reg}, 0\n" if reg != "a0" else ""
    return f"{move}    addi a1, zero, 0\n    addi a13, zero, 1\n    syscall\n"


def print_uint(reg: str = "t2") -> str:
    """Emit instructions that print `reg` as an unsigned decimal integer."""
    move = f"    addi a0, {reg}, 0\n" if reg != "a0" else ""
    return f"{move}    addi a1, zero, 0\n    addi a13, zero, 2\n    syscall\n"


def print_float(reg: str = "t2", precision: int = 6) -> str:
    """Emit instructions that print `reg` as a float with `precision` decimal places."""
    move = f"    addi a0, {reg}, 0\n" if reg != "a0" else ""
    return f"{move}    addi a1, zero, {precision}\n    addi a13, zero, 4\n    syscall\n"


# ---------------------------------------------------------------------------
# Session-scoped guard: skip everything if the binary is not built
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session", autouse=True)
def require_binary():
    if not os.path.isfile(VM_BINARY):
        pytest.skip(
            f"cortex-vm binary not found at {VM_BINARY}. Run `make` first."
        )
