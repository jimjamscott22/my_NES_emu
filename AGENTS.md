# Repository Guidelines

## Project Structure & Module Organization

This is a C++20 NES emulator learning project built with CMake. Public headers live in `include/`, core implementations live in `src/`, and tests live in `tests/`.

- `include/cpu.h`, `include/bus.h`, `include/types.h`: shared interfaces and fixed-width aliases.
- `src/cpu.cpp`, `src/bus.cpp`: emulator core compiled into the `nes_core` library.
- `src/main.cpp`: executable entry point for `nes_emulator`.
- `tests/test_cpu.cpp`: CPU behavior tests using the local test harness.
- `tests/test_framework.*`: zero-dependency test registration and assertion utilities.
- `GH-NES-graphic.png`: README image asset.

## Build, Test, and Development Commands

Configure an out-of-tree build:

```sh
cmake -S . -B build
```

Build all targets:

```sh
cmake --build build
```

Run the emulator executable:

```sh
./build/nes_emulator
```

Build and run CPU tests:

```sh
cmake --build build --target cpu_tests
./build/cpu_tests
```

You can also use the custom check target after configuring:

```sh
cmake --build build --target check
```

## Coding Style & Naming Conventions

Use modern C++20 and keep dependencies minimal. Match the existing style: 4-space indentation, braces on the same line for functions and control blocks, `PascalCase` for types such as `Cpu6502` and `StatusFlag`, and `snake_case` for functions, variables, and opcode helpers such as `zero_page()` and `op_lda_imm()`.

Keep CPU opcode handlers small and deterministic. Use `u8`, `u16`, and `u64` aliases from `include/types.h` for emulator state and bus values. Prefer clear comments for hardware behavior or cycle-count edge cases, not for obvious assignments.

## Testing Guidelines

Tests use the home-grown framework in `tests/test_framework.*`. Add new CPU tests to `tests/test_cpu.cpp` with the `TEST_CASE(test_descriptive_name)` macro and `REQUIRE(...)` assertions. Follow the existing pattern: program memory through `Bus`, call `cpu.reset(...)`, execute with `cpu.step()`, then assert registers, flags, memory writes, stack state, and cycle totals.

When adding opcodes or addressing modes, include tests for flags, page crossings, branch behavior, and cycle accounting where applicable.

## Commit & Pull Request Guidelines

Recent history uses short, imperative commit subjects such as `Add GH-NES-graphic.png image to README.md`. Keep commits focused and describe the visible change.

Pull requests should include a concise summary, test commands run, and any emulator behavior affected. Link related issues when available. Include screenshots only for README or visual asset changes.

## Agent-Specific Instructions

Do not commit build artifacts or generated files from `build/`. Keep changes scoped to the emulator, tests, or documentation requested. If adding ROM-based tests later, avoid committing copyrighted ROMs; document where local test ROMs should be placed instead.
