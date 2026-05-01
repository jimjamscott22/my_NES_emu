# CLAUDE.md

C++20 NES emulator (6502 CPU) built with CMake. No external dependencies.

## Commands

```sh
cmake -S . -B build           # configure (once)
cmake --build build           # build all targets
./build/nes_emulator          # run emulator
cmake --build build --target cpu_tests && ./build/cpu_tests   # build & run tests
cmake --build build --target check   # alias for above
```

## Architecture

| Path | Purpose |
|------|---------|
| `include/cpu.h`, `include/bus.h`, `include/types.h` | Public interfaces; `u8`/`u16`/`u64` type aliases live in `types.h` |
| `src/cpu.cpp`, `src/bus.cpp` | Core implementation compiled into `nes_core` static library |
| `src/main.cpp` | `nes_emulator` executable entry point |
| `tests/test_cpu.cpp` | CPU opcode tests |
| `tests/test_framework.*` | Zero-dependency test harness (no external libs) |

## Code Style

- C++20, 4-space indent, braces on same line
- `PascalCase` for types (`Cpu6502`, `StatusFlag`), `snake_case` for functions/variables
- Use `u8`, `u16`, `u64` from `include/types.h` — never raw `uint8_t` etc.
- Opcode handlers stay small and deterministic

## Testing Pattern

```cpp
TEST_CASE(op_lda_immediate) {
    Bus bus; Cpu6502 cpu(bus);
    bus.write(0x0200, 0xA9); bus.write(0x0201, 0x42);
    cpu.reset(0x0200);
    cpu.step();
    REQUIRE(cpu.a() == 0x42);
}
```

Cover flags, page crossings, branch behavior, and cycle totals for every opcode.

## Gotchas

- `cpu.reset()` defaults PC to `0xC000` (nestest ROM entry). Override: `cpu.reset(0x0200)`.
- Do not commit build artifacts — `build/` is gitignored.
- Do not commit copyrighted ROMs; document local test ROM paths in comments instead.

## Commits

Conventional commits format: `feat(cpu): implement ADC`, `fix(cpu): clear Break flag in PLP`.
