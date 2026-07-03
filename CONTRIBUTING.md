# Contributing to Zenith OS

Thank you for your interest in contributing to Zenith OS! We welcome contributions to help improve the kernel, graphics engine, filesystem, and userland applications.

---

## Code of Conduct

By participating in this project, you agree to abide by standard open-source citizenship guidelines. Please treat all contributors with respect.

---

## How Can I Contribute?

### 1. Reporting Bugs
* Check the existing issues (if any) or documentation before creating a new report.
* Provide a clear description of the bug, instructions to reproduce it, and the guest OS serial log output from standard output (`-serial stdio`).

### 2. Suggesting Enhancements
* Describe the feature you want to add and the engineering rationale behind it.
* Keep x86 architecture constraints in mind (e.g., memory overhead, CPU real-mode/protected-mode differences).

### 3. Submitting Code Changes
* Follow standard C99/freestanding code style guidelines.
* Avoid warnings; compile with `-Wall -Wextra` flags enabled.
* Ensure all code compiles cleanly using:
  ```bash
  make clean && make
  ```

---

## Coding Standards

* **Architecture Isolation**: Keep kernel logic separated from user space logic.
* **Safety Protocols**: Always perform address boundary checks in new system call implementations using `syscall_verify_pointer()`.
* **Resource Management**: Avoid memory leaks; always release allocated kernel heap blocks (`kfree()`) and GDT/IDT mappings when a process terminates.
