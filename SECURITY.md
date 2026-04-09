# Security Policy

## Supported Versions

Only the latest version on the `main` branch is actively maintained and receives security fixes.

## Reporting a Vulnerability

Please do not report security vulnerabilities through public GitHub issues.

Instead, send a description of the issue to **jonah@jonahmerriam.net**. Include:

- A description of the vulnerability and its potential impact
- Steps to reproduce (assembly source, binary input, or C code that triggers it)
- Any suggested fix, if you have one

You can expect an acknowledgement within a few days. If the issue is confirmed, a fix will be prioritized and you will be credited in the release notes unless you prefer to remain anonymous.

## Scope

Areas of particular interest include:

- **Assembler**: malformed or crafted input that causes out-of-bounds writes, memory corruption, or crashes
- **VM execution engine**: guest programs that escape sandboxing or cause undefined behavior in the host process
- **Disassembler**: malformed binaries that trigger unsafe memory access
- **Library API** (`libcortex-vm.a`): embedding scenarios where untrusted assembly or binary is passed to `cortexAssemble`, `cortexExecSource`, or `cortexExecBinary`
