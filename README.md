# NES Emulator (Learning Project)

![NES Emulator](GH-NES-graphic.png)

This repo is a clean starting point for a Nintendo Entertainment System
emulator written in C++20. It focuses on clarity and testability so you can
understand how each subsystem works while you build it up incrementally.

## Stack
- CMake for builds
- C++20
- Tiny home-grown test harness (`tests/test_framework.*`) to keep dependencies
  zero; swap in your preferred framework later if desired.

## Layout
- `include/` public headers (`cpu.h`, `bus.h`, `types.h`)
- `src/` core implementations (`cpu.cpp`, `bus.cpp`, `main.cpp`)
- `tests/` self-contained test runner and opcode tests
- `roms/` (create later) place test ROMs like `nestest.nes`

## Building
```
cmake -S . -B build
cmake --build build
```

Run tests:
```
cmake --build build --target cpu_tests
./build/cpu_tests   # or build\\Debug\\cpu_tests.exe on Windows
```

## Current CPU coverage
Implemented opcodes with cycle counting:
- Loads: `LDA` (A9, A5, AD), `LDX` (A2, A6, AE), `LDY` (A0, A4, AC)
- Stores: `STA` (85, 8D)
- Arithmetic: `ADC` (69, 65, 6D), `SBC` (E9, E5, ED)
- Transfers: `TAX`, `TAY`, `TXA`, `TYA`
- Stack: `PHA`, `PLA`, `PHP`, `PLP`
- Flow: `JMP` absolute, `JSR`, `RTS`, `BEQ`, `BNE`, `BCS`, `BCC`
- Default `NOP` handler for unimplemented opcodes

Status flags and cycle accounting follow 6502 behavior (branch adds 1 when
taken, another +1 on page crossing).

## Next steps
- Add addressing modes indexed by X/Y and indirect variants.
- Expand opcode set (shifts, comparisons, logical ops).
- Introduce a cartridge/memory mapper abstraction on top of the `Bus`.
- Integrate `nestest.nes` (load into memory, set PC to 0xC000, run until the
  known stop address, and compare CPU state logs).

