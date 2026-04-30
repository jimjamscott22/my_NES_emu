# Step 2 Opcode Additions — Design Spec

**Date:** 2026-04-29
**Status:** Approved

## Scope

Add the remaining opcodes needed to unblock nestest.nes validation (Step 3):

- Shifts: `ASL`, `LSR`, `ROL`, `ROR` (accumulator + memory modes)
- Memory inc/dec: `INC`, `DEC` (ZP, ZP,X, ABS, ABS,X)
- Register inc/dec: `INX`, `INY`, `DEX`, `DEY`
- Flag ops: `CLC`, `SEC`, `CLI`, `SEI`, `CLV`, `CLD`, `SED`
- Compare X/Y: `CPX`, `CPY` (IMM, ZP, ABS)
- Remaining branches: `BMI`, `BPL`, `BVS`, `BVC`
- Stack transfers: `TSX`, `TXS`
- Bit test: `BIT` (ZP, ABS)

## Architecture

All additions follow the established handler pattern in `cpu.cpp`. No new abstractions are introduced.

### Shift helpers

Four private helpers `do_asl`, `do_lsr`, `do_rol`, `do_ror` (signature: `u8(u8 v)`) update Carry/N/Z and return the shifted result. They are called by both accumulator and memory handlers.

- **ASL**: `carry = bit7(v)`, `result = v << 1`
- **LSR**: `carry = bit0(v)`, `result = v >> 1` (N always clear)
- **ROL**: `result = (v << 1) | old_carry`, `new_carry = bit7(v)`
- **ROR**: `result = (v >> 1) | (old_carry << 7)`, `new_carry = bit0(v)`

### Accumulator mode (Option A — separate handlers)

`op_asl_acc()`, `op_lsr_acc()`, `op_rol_acc()`, `op_ror_acc()` operate directly on `a_` using the helpers above. No `AddressingResult` involved.

### Read-modify-write cycle rule

`ABS,X` shifts and `INC`/`DEC` ABS,X always pay the full cycle count (7 for shifts, 7 for INC/DEC) with **no page-crossing conditional**. The 6502 always takes the extra cycle for RMW regardless of whether a page was crossed. These opcodes get their full cycle count in `base_cycles` and handlers return 0.

This differs from load instructions (`LDA ABS,X` = base 4, +1 conditional) and is the only subtle cycle-counting difference in this batch.

### INC / DEC memory

Read-modify-write: get addr from addressing mode, read value, increment/decrement, write back, update ZN.

### INX / INY / DEX / DEY

Implied mode: operate directly on `x_`/`y_`, 2 cycles, update ZN.

### Flag ops

Seven implied handlers, all 2 cycles:

| Op  | Opcode | Effect |
|-----|--------|--------|
| CLC | 0x18   | Carry = 0 |
| SEC | 0x38   | Carry = 1 |
| CLI | 0x58   | InterruptDisable = 0 |
| SEI | 0x78   | InterruptDisable = 1 |
| CLV | 0xB8   | Overflow = 0 |
| CLD | 0xD8   | Decimal = 0 |
| SED | 0xF8   | Decimal = 1 |

### CPX / CPY

Three modes each (IMM, ZP, ABS). Reuse existing `compare()` helper passing `x_` or `y_`.

### Remaining branches

`BMI` (0x30), `BPL` (0x10), `BVS` (0x70), `BVC` (0x50) — use `branch_if()` exactly like `BEQ`/`BNE`.

### TSX / TXS

- `TSX` (0xBA): `x_ = sp_; update_zn(x_)` — 2 cycles
- `TXS` (0x9A): `sp_ = x_` — 2 cycles, **no ZN update** (6502 quirk)

### BIT

ZP (0x24, 3 cycles) and ABS (0x2C, 4 cycles):

- `Z = (A & mem) == 0`
- `N = bit 7 of mem`
- `V = bit 6 of mem`
- Does **not** modify A

## Files Modified

- `include/cpu.h` — add ~50 handler declarations + 4 shift helper declarations
- `src/cpu.cpp` — implement all handlers + populate instruction table entries
- `tests/test_cpu.cpp` — add tests covering: shift flags/carry, RMW cycle counts, INC/DEC, flag ops, CPX/CPY, BIT, new branches, TSX/TXS
