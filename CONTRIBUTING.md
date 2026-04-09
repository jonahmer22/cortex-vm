# Contributing to Cortex-VM

Thank you for your interest in contributing! Please read and follow our [Code of Conduct](CODE_OF_CONDUCT.md) in all interactions.

## Getting Started

### Prerequisites

- GCC-15 (or a system `gcc` with C17 support)
- Python 3
- `pytest` (and optionally `matplotlib` for benchmarks)

### Clone the Repository

This project uses git submodules for its dependencies:

```sh
git clone --recurse-submodules https://github.com/jonahmer22/language-cortex-vm.git
```

If you already cloned without submodules:

```sh
git submodule update --init --recursive
```

### Install Python Dependencies

```sh
pip install -r requirements.txt
```

## Building

| Command      | Description                               |
|--------------|-------------------------------------------|
| `make`       | Build the `cortex-vm` binary              |
| `make lib`   | Build `lib/libcortex-vm.a` static library |
| `make debug` | Rebuild with `-DDEBUG`                    |
| `make clean` | Remove build artifacts                    |

## Running Tests

The test suite uses pytest. All tests must pass before opening a PR.

```sh
# Run the full suite
pytest

# Run a specific test file
pytest tests/test_alu.py
```

## Running Benchmarks

For performance-sensitive changes, run the benchmark suite to check for regressions:

```sh
python benchmarks/run.py
```

## Code Style

- **Standard**: C17 (`-std=c17`)
- **Warnings**: Code must compile cleanly under `-Wall -Wextra -Wpedantic`
- **Naming**: functions use `camelCase`; macros use `UPPER_SNAKE_CASE`
- **Comments**: prefer `//` single-line style
- **Includes**: system headers first, then local includes

## Submitting a Pull Request

1. Fork the repository and create a feature branch off `main`
2. Make your changes, ensuring `make` builds without warnings
3. Run `pytest` and confirm all tests pass
4. Open a PR against `main` with a clear description of what changed and why
5. Use a concise, imperative commit message (e.g., `Add support for X`, `Fix Y in Z`)
