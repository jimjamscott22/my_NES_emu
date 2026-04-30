# Step 2 Opcode Additions — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add shifts, INC/DEC, flag ops, CPX/CPY, remaining branches, TSX/TXS, and BIT to unblock nestest.nes validation.

**Architecture:** All handlers follow the existing one-liner pattern in `cpu.cpp`. Four private shift helpers (`do_asl/lsr/rol/ror`) centralize flag logic. Accumulator-mode shifts use separate handlers operating directly on `a_`. Read-modify-write instructions (shifts/INC/DEC on memory) always pay their full cycle count — no conditional page-cross penalty.

**Tech Stack:** C++20, CMake, custom test framework (`TEST_CASE` / `REQUIRE` macros in `tests/test_framework.h`)

---

## Files

- Modify: `include/cpu.h` — Task 1 only (add ~55 declarations)
- Modify: `src/cpu.cpp` — Tasks 2–8 (add implementations + table entries)
- Modify: `tests/test_cpu.cpp` — Tasks 2–8 (add tests)

**Build command:** `make -C build cpu_tests && ./build/cpu_tests`

---

## Task 1: Add all handler declarations to cpu.h

**Files:**
- Modify: `include/cpu.h`

- [ ] **Step 1: Add declarations**

In `include/cpu.h`, after the `op_bcc()` declaration and before the `static` table methods, add:

```cpp
    // Shift helpers — update Carry/N/Z, return shifted value
    u8 do_asl(u8 v);
    u8 do_lsr(u8 v);
    u8 do_rol(u8 v);
    u8 do_ror(u8 v);

    // ASL
    u8 op_asl_acc();
    u8 op_asl_zp();
    u8 op_asl_zpx();
    u8 op_asl_abs();
    u8 op_asl_absx();

    // LSR
    u8 op_lsr_acc();
    u8 op_lsr_zp();
    u8 op_lsr_zpx();
    u8 op_lsr_abs();
    u8 op_lsr_absx();

    // ROL
    u8 op_rol_acc();
    u8 op_rol_zp();
    u8 op_rol_zpx();
    u8 op_rol_abs();
    u8 op_rol_absx();

    // ROR
    u8 op_ror_acc();
    u8 op_ror_zp();
    u8 op_ror_zpx();
    u8 op_ror_abs();
    u8 op_ror_absx();

    // INC memory
    u8 op_inc_zp();
    u8 op_inc_zpx();
    u8 op_inc_abs();
    u8 op_inc_absx();

    // DEC memory
    u8 op_dec_zp();
    u8 op_dec_zpx();
    u8 op_dec_abs();
    u8 op_dec_absx();

    // Register INC/DEC
    u8 op_inx();
    u8 op_iny();
    u8 op_dex();
    u8 op_dey();

    // Flag ops
    u8 op_clc();
    u8 op_sec();
    u8 op_cli();
    u8 op_sei();
    u8 op_clv();
    u8 op_cld();
    u8 op_sed();

    // CPX
    u8 op_cpx_imm();
    u8 op_cpx_zp();
    u8 op_cpx_abs();

    // CPY
    u8 op_cpy_imm();
    u8 op_cpy_zp();
    u8 op_cpy_abs();

    // Branches
    u8 op_bmi();
    u8 op_bpl();
    u8 op_bvs();
    u8 op_bvc();

    // Stack transfers
    u8 op_tsx();
    u8 op_txs();

    // BIT
    u8 op_bit_zp();
    u8 op_bit_abs();
```

- [ ] **Step 2: Verify it compiles (link will fail — implementations missing)**

```bash
make -C build cpu_tests 2>&1 | head -30
```

Expected: linker errors for undefined references to `op_asl_acc` etc. No compiler errors.

- [ ] **Step 3: Commit**

```bash
cd "/home/jimjamscozz/Desktop/Coding Files/Other/my_NES_emu"
git add include/cpu.h
git commit -m "feat(cpu): declare Step 2 opcode handlers"
```

---

## Task 2: Flag ops (CLC / SEC / CLI / SEI / CLV / CLD / SED)

Implement these first — SEC and SEI are used as setup in later shift/branch tests.

**Files:**
- Modify: `src/cpu.cpp`
- Modify: `tests/test_cpu.cpp`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_cpu.cpp`:

```cpp
TEST_CASE(test_flag_ops) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    // SEC sets Carry; CLC clears it
    bus.write(0xC000, 0x38);  // SEC
    bus.write(0xC001, 0x18);  // CLC
    // SEI sets InterruptDisable; CLI clears it
    bus.write(0xC002, 0x78);  // SEI
    bus.write(0xC003, 0x58);  // CLI
    // SED sets Decimal; CLD clears it
    bus.write(0xC004, 0xF8);  // SED
    bus.write(0xC005, 0xD8);  // CLD
    // CLV clears Overflow (set it first via ADC overflow)
    bus.write(0xC006, 0xA9); bus.write(0xC007, 0x50);  // LDA #$50
    bus.write(0xC008, 0x69); bus.write(0xC009, 0x50);  // ADC #$50 → overflow
    bus.write(0xC00A, 0xB8);  // CLV

    cpu.reset(0xC000);
    cpu.step();  // SEC
    REQUIRE(cpu.flag(StatusFlag::Carry));

    cpu.step();  // CLC
    REQUIRE(!cpu.flag(StatusFlag::Carry));

    cpu.step();  // SEI
    REQUIRE(cpu.flag(StatusFlag::InterruptDisable));

    cpu.step();  // CLI
    REQUIRE(!cpu.flag(StatusFlag::InterruptDisable));

    cpu.step();  // SED
    REQUIRE(cpu.flag(StatusFlag::Decimal));

    cpu.step();  // CLD
    REQUIRE(!cpu.flag(StatusFlag::Decimal));

    cpu.step();  // LDA #$50
    cpu.step();  // ADC #$50 → sets Overflow
    REQUIRE(cpu.flag(StatusFlag::Overflow));

    cpu.step();  // CLV
    REQUIRE(!cpu.flag(StatusFlag::Overflow));
}
```

- [ ] **Step 2: Run — verify it fails**

```bash
make -C build cpu_tests && ./build/cpu_tests
```

Expected: `FAIL test_flag_ops` — SEC/CLC hit the NOP handler, flags never change.

- [ ] **Step 3: Implement flag ops in cpu.cpp**

After the `op_bcc()` handler block, add:

```cpp
// Flag ops
u8 Cpu6502::op_clc() { set_flag(StatusFlag::Carry, false);             return 0; }
u8 Cpu6502::op_sec() { set_flag(StatusFlag::Carry, true);              return 0; }
u8 Cpu6502::op_cli() { set_flag(StatusFlag::InterruptDisable, false);  return 0; }
u8 Cpu6502::op_sei() { set_flag(StatusFlag::InterruptDisable, true);   return 0; }
u8 Cpu6502::op_clv() { set_flag(StatusFlag::Overflow, false);          return 0; }
u8 Cpu6502::op_cld() { set_flag(StatusFlag::Decimal, false);           return 0; }
u8 Cpu6502::op_sed() { set_flag(StatusFlag::Decimal, true);            return 0; }
```

- [ ] **Step 4: Add table entries in build_table()**

After the Branches section in `build_table()`:

```cpp
    // Flag ops
    t[0x18] = {"CLC", 2, &Cpu6502::op_clc};
    t[0x38] = {"SEC", 2, &Cpu6502::op_sec};
    t[0x58] = {"CLI", 2, &Cpu6502::op_cli};
    t[0x78] = {"SEI", 2, &Cpu6502::op_sei};
    t[0xB8] = {"CLV", 2, &Cpu6502::op_clv};
    t[0xD8] = {"CLD", 2, &Cpu6502::op_cld};
    t[0xF8] = {"SED", 2, &Cpu6502::op_sed};
```

- [ ] **Step 5: Run — verify it passes**

```bash
make -C build cpu_tests && ./build/cpu_tests
```

Expected: all tests PASS.

- [ ] **Step 6: Commit**

```bash
cd "/home/jimjamscozz/Desktop/Coding Files/Other/my_NES_emu"
git add src/cpu.cpp tests/test_cpu.cpp
git commit -m "feat(cpu): implement CLC/SEC/CLI/SEI/CLV/CLD/SED"
```

---

## Task 3: Shift helpers + accumulator mode (ASL / LSR / ROL / ROR acc)

**Files:**
- Modify: `src/cpu.cpp`
- Modify: `tests/test_cpu.cpp`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_cpu.cpp`:

```cpp
TEST_CASE(test_shifts_accumulator) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    // ASL A: 0x81 << 1 = 0x02, bit7 → Carry
    bus.write(0xC000, 0xA9); bus.write(0xC001, 0x81);  // LDA #$81
    bus.write(0xC002, 0x0A);                            // ASL A
    // LSR A: 0x81 >> 1 = 0x40, bit0 → Carry
    bus.write(0xC003, 0x4A);                            // LSR A
    // SEC then ROL A: 0x40 ROL with carry=1 → 0x81, old bit7(0x40)=0 → carry=0
    bus.write(0xC004, 0x38);                            // SEC
    bus.write(0xC005, 0x2A);                            // ROL A
    // ROR A: 0x81 ROR with carry=0 → 0x40, old bit0(0x81)=1 → carry=1
    bus.write(0xC006, 0x6A);                            // ROR A

    cpu.reset(0xC000);

    cpu.step();  // LDA #$81
    cpu.step();  // ASL A → A=0x02, Carry=1, N=0, Z=0
    REQUIRE(cpu.a() == 0x02);
    REQUIRE(cpu.flag(StatusFlag::Carry));
    REQUIRE(!cpu.flag(StatusFlag::Negative));
    REQUIRE(!cpu.flag(StatusFlag::Zero));

    cpu.step();  // LSR A → A=0x01, Carry=0 (bit0 of 0x02 = 0), N=0
    REQUIRE(cpu.a() == 0x01);
    REQUIRE(!cpu.flag(StatusFlag::Carry));

    // At this point A=0x01. SEC sets carry, then ROL: (0x01<<1)|1 = 0x03, old bit7=0 → carry=0
    cpu.step();  // SEC → carry=1
    cpu.step();  // ROL A: result=(0x01<<1)|1=0x03, new carry=bit7(0x01)=0
    REQUIRE(cpu.a() == 0x03);
    REQUIRE(!cpu.flag(StatusFlag::Carry));

    // ROR A with carry=0: result=(0x03>>1)|0=0x01, new carry=bit0(0x03)=1
    cpu.step();  // ROR A
    REQUIRE(cpu.a() == 0x01);
    REQUIRE(cpu.flag(StatusFlag::Carry));

    // Cycle check: LDA(2) + ASL(2) + LSR(2) + SEC(2) + ROL(2) + ROR(2) = 12
    REQUIRE(cpu.cycles() == 12);
}
```

- [ ] **Step 2: Run — verify it fails**

```bash
make -C build cpu_tests && ./build/cpu_tests
```

Expected: `FAIL test_shifts_accumulator` — shift opcodes hit NOP, A never changes.

- [ ] **Step 3: Implement shift helpers and accumulator handlers in cpu.cpp**

After the `compare()` helper, add:

```cpp
u8 Cpu6502::do_asl(u8 v) {
    set_flag(StatusFlag::Carry, (v & 0x80) != 0);
    u8 result = static_cast<u8>(v << 1);
    update_zn(result);
    return result;
}

u8 Cpu6502::do_lsr(u8 v) {
    set_flag(StatusFlag::Carry, (v & 0x01) != 0);
    u8 result = v >> 1;
    update_zn(result);
    return result;
}

u8 Cpu6502::do_rol(u8 v) {
    u8 old_carry = flag(StatusFlag::Carry) ? 1 : 0;
    set_flag(StatusFlag::Carry, (v & 0x80) != 0);
    u8 result = static_cast<u8>((v << 1) | old_carry);
    update_zn(result);
    return result;
}

u8 Cpu6502::do_ror(u8 v) {
    u8 old_carry = flag(StatusFlag::Carry) ? 0x80 : 0;
    set_flag(StatusFlag::Carry, (v & 0x01) != 0);
    u8 result = static_cast<u8>((v >> 1) | old_carry);
    update_zn(result);
    return result;
}

// Shifts — accumulator mode
u8 Cpu6502::op_asl_acc() { a_ = do_asl(a_); return 0; }
u8 Cpu6502::op_lsr_acc() { a_ = do_lsr(a_); return 0; }
u8 Cpu6502::op_rol_acc() { a_ = do_rol(a_); return 0; }
u8 Cpu6502::op_ror_acc() { a_ = do_ror(a_); return 0; }
```

- [ ] **Step 4: Add table entries**

```cpp
    // ASL
    t[0x0A] = {"ASL ACC",   2, &Cpu6502::op_asl_acc};

    // LSR
    t[0x4A] = {"LSR ACC",   2, &Cpu6502::op_lsr_acc};

    // ROL
    t[0x2A] = {"ROL ACC",   2, &Cpu6502::op_rol_acc};

    // ROR
    t[0x6A] = {"ROR ACC",   2, &Cpu6502::op_ror_acc};
```

- [ ] **Step 5: Run — verify it passes**

```bash
make -C build cpu_tests && ./build/cpu_tests
```

Expected: all tests PASS.

- [ ] **Step 6: Commit**

```bash
cd "/home/jimjamscozz/Desktop/Coding Files/Other/my_NES_emu"
git add src/cpu.cpp tests/test_cpu.cpp
git commit -m "feat(cpu): implement shift helpers + ASL/LSR/ROL/ROR accumulator mode"
```

---

## Task 4: Shift memory modes (ZP / ZP,X / ABS / ABS,X)

**Files:**
- Modify: `src/cpu.cpp`
- Modify: `tests/test_cpu.cpp`

- [ ] **Step 1: Write the failing tests**

Append to `tests/test_cpu.cpp`:

```cpp
TEST_CASE(test_asl_zp_modifies_memory) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0x0010, 0x41);                            // value at ZP $10
    bus.write(0xC000, 0x06); bus.write(0xC001, 0x10);  // ASL $10

    cpu.reset(0xC000);
    cpu.step();

    REQUIRE(bus.read(0x0010) == 0x82);
    REQUIRE(cpu.flag(StatusFlag::Negative));
    REQUIRE(!cpu.flag(StatusFlag::Carry));
    REQUIRE(cpu.cycles() == 5);
}

TEST_CASE(test_lsr_zp_modifies_memory) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0x0020, 0x03);                            // value at ZP $20
    bus.write(0xC000, 0x46); bus.write(0xC001, 0x20);  // LSR $20

    cpu.reset(0xC000);
    cpu.step();

    REQUIRE(bus.read(0x0020) == 0x01);
    REQUIRE(cpu.flag(StatusFlag::Carry));   // bit0 of 0x03 = 1
    REQUIRE(!cpu.flag(StatusFlag::Zero));
    REQUIRE(cpu.cycles() == 5);
}

TEST_CASE(test_asl_absx_always_7_cycles) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0x0205, 0x01);
    bus.write(0xC000, 0xA2); bus.write(0xC001, 0x05);            // LDX #$05
    bus.write(0xC002, 0x1E); bus.write(0xC003, 0x00); bus.write(0xC004, 0x02);  // ASL $0200,X

    cpu.reset(0xC000);
    cpu.step();  // LDX (2)
    cpu.step();  // ASL ABS,X (always 7, no page cross involved)

    REQUIRE(bus.read(0x0205) == 0x02);
    REQUIRE(cpu.cycles() == (2 + 7));
}

TEST_CASE(test_rol_ror_zp_roundtrip) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    // ROL $10 with carry=0: 0x80 → 0x00, Carry=1, Zero=1
    bus.write(0x0010, 0x80);
    bus.write(0xC000, 0x26); bus.write(0xC001, 0x10);  // ROL $10
    // ROR $10 with carry=1 (from ROL): 0x00 → 0x80, Carry=0
    bus.write(0xC002, 0x66); bus.write(0xC003, 0x10);  // ROR $10

    cpu.reset(0xC000);
    cpu.step();  // ROL $10: 0x80→0x00, carry=1, zero=1
    REQUIRE(bus.read(0x0010) == 0x00);
    REQUIRE(cpu.flag(StatusFlag::Zero));
    REQUIRE(cpu.flag(StatusFlag::Carry));

    cpu.step();  // ROR $10: 0x00 with carry=1 → 0x80, carry=0
    REQUIRE(bus.read(0x0010) == 0x80);
    REQUIRE(!cpu.flag(StatusFlag::Carry));
    REQUIRE(cpu.flag(StatusFlag::Negative));

    // ROL(5) + ROR(5) = 10
    REQUIRE(cpu.cycles() == 10);
}
```

- [ ] **Step 2: Run — verify they fail**

```bash
make -C build cpu_tests && ./build/cpu_tests
```

Expected: new shift memory tests FAIL (memory unchanged, cycles wrong).

- [ ] **Step 3: Implement shift memory handlers in cpu.cpp**

After the accumulator shift handlers:

```cpp
// Shifts — memory (read-modify-write; ABS,X always pays full cycles)
u8 Cpu6502::op_asl_zp()   { auto r = zero_page();  u8 v = do_asl(r.value); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_asl_zpx()  { auto r = zero_page_x(); u8 v = do_asl(r.value); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_asl_abs()  { auto r = absolute();   u8 v = do_asl(r.value); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_asl_absx() { auto r = absolute_x(); u8 v = do_asl(r.value); bus_->write(r.addr, v); return 0; }

u8 Cpu6502::op_lsr_zp()   { auto r = zero_page();  u8 v = do_lsr(r.value); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_lsr_zpx()  { auto r = zero_page_x(); u8 v = do_lsr(r.value); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_lsr_abs()  { auto r = absolute();   u8 v = do_lsr(r.value); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_lsr_absx() { auto r = absolute_x(); u8 v = do_lsr(r.value); bus_->write(r.addr, v); return 0; }

u8 Cpu6502::op_rol_zp()   { auto r = zero_page();  u8 v = do_rol(r.value); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_rol_zpx()  { auto r = zero_page_x(); u8 v = do_rol(r.value); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_rol_abs()  { auto r = absolute();   u8 v = do_rol(r.value); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_rol_absx() { auto r = absolute_x(); u8 v = do_rol(r.value); bus_->write(r.addr, v); return 0; }

u8 Cpu6502::op_ror_zp()   { auto r = zero_page();  u8 v = do_ror(r.value); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_ror_zpx()  { auto r = zero_page_x(); u8 v = do_ror(r.value); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_ror_abs()  { auto r = absolute();   u8 v = do_ror(r.value); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_ror_absx() { auto r = absolute_x(); u8 v = do_ror(r.value); bus_->write(r.addr, v); return 0; }
```

- [ ] **Step 4: Add table entries**

```cpp
    // ASL
    t[0x06] = {"ASL ZP",    5, &Cpu6502::op_asl_zp};
    t[0x16] = {"ASL ZP,X",  6, &Cpu6502::op_asl_zpx};
    t[0x0E] = {"ASL ABS",   6, &Cpu6502::op_asl_abs};
    t[0x1E] = {"ASL ABS,X", 7, &Cpu6502::op_asl_absx};

    // LSR
    t[0x46] = {"LSR ZP",    5, &Cpu6502::op_lsr_zp};
    t[0x56] = {"LSR ZP,X",  6, &Cpu6502::op_lsr_zpx};
    t[0x4E] = {"LSR ABS",   6, &Cpu6502::op_lsr_abs};
    t[0x5E] = {"LSR ABS,X", 7, &Cpu6502::op_lsr_absx};

    // ROL
    t[0x26] = {"ROL ZP",    5, &Cpu6502::op_rol_zp};
    t[0x36] = {"ROL ZP,X",  6, &Cpu6502::op_rol_zpx};
    t[0x2E] = {"ROL ABS",   6, &Cpu6502::op_rol_abs};
    t[0x3E] = {"ROL ABS,X", 7, &Cpu6502::op_rol_absx};

    // ROR
    t[0x66] = {"ROR ZP",    5, &Cpu6502::op_ror_zp};
    t[0x76] = {"ROR ZP,X",  6, &Cpu6502::op_ror_zpx};
    t[0x6E] = {"ROR ABS",   6, &Cpu6502::op_ror_abs};
    t[0x7E] = {"ROR ABS,X", 7, &Cpu6502::op_ror_absx};
```

- [ ] **Step 5: Run — verify it passes**

```bash
make -C build cpu_tests && ./build/cpu_tests
```

Expected: all tests PASS.

- [ ] **Step 6: Commit**

```bash
cd "/home/jimjamscozz/Desktop/Coding Files/Other/my_NES_emu"
git add src/cpu.cpp tests/test_cpu.cpp
git commit -m "feat(cpu): implement ASL/LSR/ROL/ROR memory modes"
```

---

## Task 5: INC / DEC (memory + register)

**Files:**
- Modify: `src/cpu.cpp`
- Modify: `tests/test_cpu.cpp`

- [ ] **Step 1: Write the failing tests**

Append to `tests/test_cpu.cpp`:

```cpp
TEST_CASE(test_inc_dec_zp) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0x0010, 0xFE);
    bus.write(0xC000, 0xE6); bus.write(0xC001, 0x10);  // INC $10 → $FF, N=1
    bus.write(0xC002, 0xE6); bus.write(0xC003, 0x10);  // INC $10 → $00, Z=1, carry from wrapping
    bus.write(0xC004, 0xC6); bus.write(0xC005, 0x10);  // DEC $10 → $FF, N=1

    cpu.reset(0xC000);

    cpu.step();  // INC → $FF
    REQUIRE(bus.read(0x0010) == 0xFF);
    REQUIRE(cpu.flag(StatusFlag::Negative));
    REQUIRE(!cpu.flag(StatusFlag::Zero));

    cpu.step();  // INC → $00 (wraps)
    REQUIRE(bus.read(0x0010) == 0x00);
    REQUIRE(cpu.flag(StatusFlag::Zero));
    REQUIRE(!cpu.flag(StatusFlag::Negative));

    cpu.step();  // DEC → $FF
    REQUIRE(bus.read(0x0010) == 0xFF);
    REQUIRE(cpu.flag(StatusFlag::Negative));

    // INC(5) + INC(5) + DEC(5) = 15
    REQUIRE(cpu.cycles() == 15);
}

TEST_CASE(test_inc_absx_always_7_cycles) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0x0206, 0x10);
    bus.write(0xC000, 0xA2); bus.write(0xC001, 0x06);            // LDX #$06
    bus.write(0xC002, 0xFE); bus.write(0xC003, 0x00); bus.write(0xC004, 0x02);  // INC $0200,X

    cpu.reset(0xC000);
    cpu.step();  // LDX (2)
    cpu.step();  // INC ABS,X (always 7)

    REQUIRE(bus.read(0x0206) == 0x11);
    REQUIRE(cpu.cycles() == (2 + 7));
}

TEST_CASE(test_inx_iny_dex_dey) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0xC000, 0xA2); bus.write(0xC001, 0xFF);  // LDX #$FF
    bus.write(0xC002, 0xE8);                            // INX → $00, Z=1
    bus.write(0xC003, 0xA0); bus.write(0xC004, 0x01);  // LDY #$01
    bus.write(0xC005, 0x88);                            // DEY → $00, Z=1
    bus.write(0xC006, 0xC8);                            // INY → $01
    bus.write(0xC007, 0xCA);                            // DEX → $FF, N=1

    cpu.reset(0xC000);

    cpu.step();  // LDX #$FF
    cpu.step();  // INX → $00
    REQUIRE(cpu.x() == 0x00);
    REQUIRE(cpu.flag(StatusFlag::Zero));

    cpu.step();  // LDY #$01
    cpu.step();  // DEY → $00
    REQUIRE(cpu.y() == 0x00);
    REQUIRE(cpu.flag(StatusFlag::Zero));

    cpu.step();  // INY → $01
    REQUIRE(cpu.y() == 0x01);

    cpu.step();  // DEX → $FF
    REQUIRE(cpu.x() == 0xFF);
    REQUIRE(cpu.flag(StatusFlag::Negative));

    // LDX(2)+INX(2)+LDY(2)+DEY(2)+INY(2)+DEX(2) = 12
    REQUIRE(cpu.cycles() == 12);
}
```

- [ ] **Step 2: Run — verify they fail**

```bash
make -C build cpu_tests && ./build/cpu_tests
```

Expected: new INC/DEC tests FAIL.

- [ ] **Step 3: Implement in cpu.cpp**

```cpp
// INC memory
u8 Cpu6502::op_inc_zp()   { auto r = zero_page();  u8 v = static_cast<u8>(r.value + 1); update_zn(v); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_inc_zpx()  { auto r = zero_page_x(); u8 v = static_cast<u8>(r.value + 1); update_zn(v); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_inc_abs()  { auto r = absolute();   u8 v = static_cast<u8>(r.value + 1); update_zn(v); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_inc_absx() { auto r = absolute_x(); u8 v = static_cast<u8>(r.value + 1); update_zn(v); bus_->write(r.addr, v); return 0; }

// DEC memory
u8 Cpu6502::op_dec_zp()   { auto r = zero_page();  u8 v = static_cast<u8>(r.value - 1); update_zn(v); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_dec_zpx()  { auto r = zero_page_x(); u8 v = static_cast<u8>(r.value - 1); update_zn(v); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_dec_abs()  { auto r = absolute();   u8 v = static_cast<u8>(r.value - 1); update_zn(v); bus_->write(r.addr, v); return 0; }
u8 Cpu6502::op_dec_absx() { auto r = absolute_x(); u8 v = static_cast<u8>(r.value - 1); update_zn(v); bus_->write(r.addr, v); return 0; }

// Register INC/DEC
u8 Cpu6502::op_inx() { ++x_; update_zn(x_); return 0; }
u8 Cpu6502::op_iny() { ++y_; update_zn(y_); return 0; }
u8 Cpu6502::op_dex() { --x_; update_zn(x_); return 0; }
u8 Cpu6502::op_dey() { --y_; update_zn(y_); return 0; }
```

- [ ] **Step 4: Add table entries**

```cpp
    // INC
    t[0xE6] = {"INC ZP",    5, &Cpu6502::op_inc_zp};
    t[0xF6] = {"INC ZP,X",  6, &Cpu6502::op_inc_zpx};
    t[0xEE] = {"INC ABS",   6, &Cpu6502::op_inc_abs};
    t[0xFE] = {"INC ABS,X", 7, &Cpu6502::op_inc_absx};

    // DEC
    t[0xC6] = {"DEC ZP",    5, &Cpu6502::op_dec_zp};
    t[0xD6] = {"DEC ZP,X",  6, &Cpu6502::op_dec_zpx};
    t[0xCE] = {"DEC ABS",   6, &Cpu6502::op_dec_abs};
    t[0xDE] = {"DEC ABS,X", 7, &Cpu6502::op_dec_absx};

    // Register INC/DEC
    t[0xE8] = {"INX", 2, &Cpu6502::op_inx};
    t[0xC8] = {"INY", 2, &Cpu6502::op_iny};
    t[0xCA] = {"DEX", 2, &Cpu6502::op_dex};
    t[0x88] = {"DEY", 2, &Cpu6502::op_dey};
```

- [ ] **Step 5: Run — verify it passes**

```bash
make -C build cpu_tests && ./build/cpu_tests
```

Expected: all tests PASS.

- [ ] **Step 6: Commit**

```bash
cd "/home/jimjamscozz/Desktop/Coding Files/Other/my_NES_emu"
git add src/cpu.cpp tests/test_cpu.cpp
git commit -m "feat(cpu): implement INC/DEC memory + INX/INY/DEX/DEY"
```

---

## Task 6: CPX / CPY

**Files:**
- Modify: `src/cpu.cpp`
- Modify: `tests/test_cpu.cpp`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_cpu.cpp`:

```cpp
TEST_CASE(test_cpx_cpy) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    // CPX #$42 with X=$42 → equal: Z=1, C=1, N=0
    bus.write(0xC000, 0xA2); bus.write(0xC001, 0x42);  // LDX #$42
    bus.write(0xC002, 0xE0); bus.write(0xC003, 0x42);  // CPX #$42

    // CPX #$50 with X=$42 → X < operand: Z=0, C=0, N=1
    bus.write(0xC004, 0xE0); bus.write(0xC005, 0x50);  // CPX #$50

    // CPY ZP: Y=$10 vs mem=$10 → equal
    bus.write(0x0020, 0x10);
    bus.write(0xC006, 0xA0); bus.write(0xC007, 0x10);  // LDY #$10
    bus.write(0xC008, 0xC4); bus.write(0xC009, 0x20);  // CPY $20

    cpu.reset(0xC000);
    cpu.step();  // LDX #$42
    cpu.step();  // CPX #$42
    REQUIRE(cpu.flag(StatusFlag::Zero));
    REQUIRE(cpu.flag(StatusFlag::Carry));
    REQUIRE(!cpu.flag(StatusFlag::Negative));

    cpu.step();  // CPX #$50 (X=$42 < $50)
    REQUIRE(!cpu.flag(StatusFlag::Zero));
    REQUIRE(!cpu.flag(StatusFlag::Carry));
    REQUIRE(cpu.flag(StatusFlag::Negative));  // result = $42-$50 = $F2, bit7 set

    cpu.step();  // LDY #$10
    cpu.step();  // CPY $20 (Y=$10 == mem=$10)
    REQUIRE(cpu.flag(StatusFlag::Zero));
    REQUIRE(cpu.flag(StatusFlag::Carry));
}
```

- [ ] **Step 2: Run — verify it fails**

```bash
make -C build cpu_tests && ./build/cpu_tests
```

Expected: `FAIL test_cpx_cpy`.

- [ ] **Step 3: Implement in cpu.cpp**

```cpp
// CPX
u8 Cpu6502::op_cpx_imm() { auto r = immediate(); compare(x_, r.value); return 0; }
u8 Cpu6502::op_cpx_zp()  { auto r = zero_page(); compare(x_, r.value); return 0; }
u8 Cpu6502::op_cpx_abs() { auto r = absolute();  compare(x_, r.value); return 0; }

// CPY
u8 Cpu6502::op_cpy_imm() { auto r = immediate(); compare(y_, r.value); return 0; }
u8 Cpu6502::op_cpy_zp()  { auto r = zero_page(); compare(y_, r.value); return 0; }
u8 Cpu6502::op_cpy_abs() { auto r = absolute();  compare(y_, r.value); return 0; }
```

- [ ] **Step 4: Add table entries**

```cpp
    // CPX
    t[0xE0] = {"CPX IMM", 2, &Cpu6502::op_cpx_imm};
    t[0xE4] = {"CPX ZP",  3, &Cpu6502::op_cpx_zp};
    t[0xEC] = {"CPX ABS", 4, &Cpu6502::op_cpx_abs};

    // CPY
    t[0xC0] = {"CPY IMM", 2, &Cpu6502::op_cpy_imm};
    t[0xC4] = {"CPY ZP",  3, &Cpu6502::op_cpy_zp};
    t[0xCC] = {"CPY ABS", 4, &Cpu6502::op_cpy_abs};
```

- [ ] **Step 5: Run — verify it passes**

```bash
make -C build cpu_tests && ./build/cpu_tests
```

Expected: all tests PASS.

- [ ] **Step 6: Commit**

```bash
cd "/home/jimjamscozz/Desktop/Coding Files/Other/my_NES_emu"
git add src/cpu.cpp tests/test_cpu.cpp
git commit -m "feat(cpu): implement CPX/CPY"
```

---

## Task 7: Remaining branches (BMI/BPL/BVS/BVC) + TSX/TXS

**Files:**
- Modify: `src/cpu.cpp`
- Modify: `tests/test_cpu.cpp`

- [ ] **Step 1: Write the failing tests**

Append to `tests/test_cpu.cpp`:

```cpp
TEST_CASE(test_bpl_bmi) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    // BPL: load positive ($01), branch taken (+2 offset → skip LDA #$FF)
    bus.write(0xC000, 0xA9); bus.write(0xC001, 0x01);  // LDA #$01 (positive)
    bus.write(0xC002, 0x10); bus.write(0xC003, 0x02);  // BPL +2 → $C006
    bus.write(0xC004, 0xA9); bus.write(0xC005, 0xFF);  // LDA #$FF (skipped)
    bus.write(0xC006, 0xA9); bus.write(0xC007, 0x80);  // LDA #$80 (target, sets N)
    bus.write(0xC008, 0x30); bus.write(0xC009, 0x02);  // BMI +2 → $C00C
    bus.write(0xC00A, 0xA9); bus.write(0xC00B, 0x00);  // LDA #$00 (skipped)
    bus.write(0xC00C, 0xA9); bus.write(0xC00D, 0x42);  // LDA #$42 (target)

    cpu.reset(0xC000);
    cpu.step();  // LDA #$01
    cpu.step();  // BPL taken
    cpu.step();  // LDA #$80 (branch target)
    cpu.step();  // BMI taken
    cpu.step();  // LDA #$42 (branch target)

    REQUIRE(cpu.a() == 0x42);
}

TEST_CASE(test_bvs_bvc) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    // ADC 0x50+0x50 sets overflow; BVS taken; CLV; BVC taken
    bus.write(0xC000, 0xA9); bus.write(0xC001, 0x50);  // LDA #$50
    bus.write(0xC002, 0x69); bus.write(0xC003, 0x50);  // ADC #$50 → V=1
    bus.write(0xC004, 0x70); bus.write(0xC005, 0x02);  // BVS +2 → $C008
    bus.write(0xC006, 0xA9); bus.write(0xC007, 0x00);  // LDA #$00 (skipped)
    bus.write(0xC008, 0xB8);                            // CLV → V=0
    bus.write(0xC009, 0x50); bus.write(0xC00A, 0x02);  // BVC +2 → $C00D
    bus.write(0xC00B, 0xA9); bus.write(0xC00C, 0x00);  // LDA #$00 (skipped)
    bus.write(0xC00D, 0xA9); bus.write(0xC00E, 0x11);  // LDA #$11 (target)

    cpu.reset(0xC000);
    cpu.step();  // LDA
    cpu.step();  // ADC → V=1
    cpu.step();  // BVS taken
    cpu.step();  // CLV → V=0
    cpu.step();  // BVC taken
    cpu.step();  // LDA #$11

    REQUIRE(cpu.a() == 0x11);
    REQUIRE(!cpu.flag(StatusFlag::Overflow));
}

TEST_CASE(test_tsx_txs) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    // After reset SP=0xFD. TSX → X=$FD, N=1
    bus.write(0xC000, 0xBA);                            // TSX
    // TXS: LDX #$EF, TXS → SP=$EF (no ZN update)
    bus.write(0xC001, 0xA2); bus.write(0xC002, 0xEF);  // LDX #$EF
    bus.write(0xC003, 0x9A);                            // TXS

    cpu.reset(0xC000);
    cpu.step();  // TSX
    REQUIRE(cpu.x() == 0xFD);
    REQUIRE(cpu.flag(StatusFlag::Negative));

    cpu.step();  // LDX #$EF
    cpu.step();  // TXS → SP=$EF
    REQUIRE(cpu.sp() == 0xEF);
    // TXS does NOT update N/Z — N should still reflect last update_zn call (LDX #$EF → N=1)
    REQUIRE(cpu.flag(StatusFlag::Negative));  // unchanged from LDX
}
```

- [ ] **Step 2: Run — verify they fail**

```bash
make -C build cpu_tests && ./build/cpu_tests
```

Expected: new branch/transfer tests FAIL.

- [ ] **Step 3: Implement in cpu.cpp**

```cpp
// Remaining branches
u8 Cpu6502::op_bmi() { return branch_if(flag(StatusFlag::Negative)); }
u8 Cpu6502::op_bpl() { return branch_if(!flag(StatusFlag::Negative)); }
u8 Cpu6502::op_bvs() { return branch_if(flag(StatusFlag::Overflow)); }
u8 Cpu6502::op_bvc() { return branch_if(!flag(StatusFlag::Overflow)); }

// Stack transfers
u8 Cpu6502::op_tsx() { x_ = sp_; update_zn(x_); return 0; }
u8 Cpu6502::op_txs() { sp_ = x_; return 0; }
```

- [ ] **Step 4: Add table entries**

```cpp
    // Branches
    t[0x30] = {"BMI", 2, &Cpu6502::op_bmi};
    t[0x10] = {"BPL", 2, &Cpu6502::op_bpl};
    t[0x70] = {"BVS", 2, &Cpu6502::op_bvs};
    t[0x50] = {"BVC", 2, &Cpu6502::op_bvc};

    // Stack transfers
    t[0xBA] = {"TSX", 2, &Cpu6502::op_tsx};
    t[0x9A] = {"TXS", 2, &Cpu6502::op_txs};
```

- [ ] **Step 5: Run — verify it passes**

```bash
make -C build cpu_tests && ./build/cpu_tests
```

Expected: all tests PASS.

- [ ] **Step 6: Commit**

```bash
cd "/home/jimjamscozz/Desktop/Coding Files/Other/my_NES_emu"
git add src/cpu.cpp tests/test_cpu.cpp
git commit -m "feat(cpu): implement BMI/BPL/BVS/BVC branches and TSX/TXS"
```

---

## Task 8: BIT

**Files:**
- Modify: `src/cpu.cpp`
- Modify: `tests/test_cpu.cpp`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_cpu.cpp`:

```cpp
TEST_CASE(test_bit_zp_and_abs) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    // BIT ZP: mem=$C2 (1100 0010), A=$01
    // Z = (A & mem)==0 → (0x01 & 0xC2)=0x00 → Z=1
    // N = bit7(mem) = 1
    // V = bit6(mem) = 1
    // A unchanged
    bus.write(0x0010, 0xC2);
    bus.write(0xC000, 0xA9); bus.write(0xC001, 0x01);  // LDA #$01
    bus.write(0xC002, 0x24); bus.write(0xC003, 0x10);  // BIT $10

    // BIT ABS: mem=$3F (0011 1111), A=$01
    // Z = (0x01 & 0x3F) = 0x01 ≠ 0 → Z=0
    // N = bit7(0x3F) = 0
    // V = bit6(0x3F) = 0
    bus.write(0x0200, 0x3F);
    bus.write(0xC004, 0x2C); bus.write(0xC005, 0x00); bus.write(0xC006, 0x02);  // BIT $0200

    cpu.reset(0xC000);
    cpu.step();  // LDA #$01
    cpu.step();  // BIT $10
    REQUIRE(cpu.a() == 0x01);  // A unchanged
    REQUIRE(cpu.flag(StatusFlag::Zero));
    REQUIRE(cpu.flag(StatusFlag::Negative));
    REQUIRE(cpu.flag(StatusFlag::Overflow));

    cpu.step();  // BIT $0200
    REQUIRE(cpu.a() == 0x01);  // A still unchanged
    REQUIRE(!cpu.flag(StatusFlag::Zero));
    REQUIRE(!cpu.flag(StatusFlag::Negative));
    REQUIRE(!cpu.flag(StatusFlag::Overflow));

    // LDA(2) + BIT ZP(3) + BIT ABS(4) = 9
    REQUIRE(cpu.cycles() == 9);
}
```

- [ ] **Step 2: Run — verify it fails**

```bash
make -C build cpu_tests && ./build/cpu_tests
```

Expected: `FAIL test_bit_zp_and_abs`.

- [ ] **Step 3: Implement in cpu.cpp**

```cpp
// BIT
u8 Cpu6502::op_bit_zp() {
    auto r = zero_page();
    set_flag(StatusFlag::Zero,     (a_ & r.value) == 0);
    set_flag(StatusFlag::Negative, (r.value & 0x80) != 0);
    set_flag(StatusFlag::Overflow, (r.value & 0x40) != 0);
    return 0;
}

u8 Cpu6502::op_bit_abs() {
    auto r = absolute();
    set_flag(StatusFlag::Zero,     (a_ & r.value) == 0);
    set_flag(StatusFlag::Negative, (r.value & 0x80) != 0);
    set_flag(StatusFlag::Overflow, (r.value & 0x40) != 0);
    return 0;
}
```

- [ ] **Step 4: Add table entries**

```cpp
    // BIT
    t[0x24] = {"BIT ZP",  3, &Cpu6502::op_bit_zp};
    t[0x2C] = {"BIT ABS", 4, &Cpu6502::op_bit_abs};
```

- [ ] **Step 5: Run — verify it passes**

```bash
make -C build cpu_tests && ./build/cpu_tests
```

Expected: all tests PASS.

- [ ] **Step 6: Commit**

```bash
cd "/home/jimjamscozz/Desktop/Coding Files/Other/my_NES_emu"
git add src/cpu.cpp tests/test_cpu.cpp
git commit -m "feat(cpu): implement BIT ZP and BIT ABS"
```
