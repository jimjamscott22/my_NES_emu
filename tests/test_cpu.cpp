#include "cpu.h"
#include "bus.h"
#include "test_framework.h"

// Each test programs memory directly, then resets the CPU to the program entry
// point so the fetch-decode-execute loop can run as the real hardware would.

TEST_CASE(test_lda_immediate_sets_zero_flag) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0xC000, 0xA9);  // LDA #$00
    bus.write(0xC001, 0x00);

    cpu.reset(0xC000);
    cpu.step();

    REQUIRE(cpu.a() == 0x00);
    REQUIRE(cpu.flag(StatusFlag::Zero));
    REQUIRE(!cpu.flag(StatusFlag::Negative));
    REQUIRE(cpu.cycles() == 2);
}

TEST_CASE(test_lda_zero_page_reads_memory) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0x0004, 0x7F);
    bus.write(0xC000, 0xA5);  // LDA $04
    bus.write(0xC001, 0x04);

    cpu.reset(0xC000);
    cpu.step();

    REQUIRE(cpu.a() == 0x7F);
    REQUIRE(!cpu.flag(StatusFlag::Zero));
    REQUIRE(cpu.flag(StatusFlag::Negative) == false);
    REQUIRE(cpu.cycles() == 3);
}

TEST_CASE(test_sta_absolute_stores_accumulator) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0xC000, 0xA9);  // LDA #$11
    bus.write(0xC001, 0x11);
    bus.write(0xC002, 0x8D);  // STA $2000
    bus.write(0xC003, 0x00);
    bus.write(0xC004, 0x20);

    cpu.reset(0xC000);
    cpu.step();  // LDA
    cpu.step();  // STA

    REQUIRE(bus.read(0x2000) == 0x11);
    REQUIRE(cpu.cycles() == (2 + 4));
}

TEST_CASE(test_adc_sets_carry_and_overflow) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0xC000, 0xA9);  // LDA #$50
    bus.write(0xC001, 0x50);
    bus.write(0xC002, 0x69);  // ADC #$50
    bus.write(0xC003, 0x50);

    cpu.reset(0xC000);
    cpu.step();  // LDA
    cpu.step();  // ADC

    REQUIRE(cpu.a() == 0xA0);
    REQUIRE(!cpu.flag(StatusFlag::Carry));
    REQUIRE(cpu.flag(StatusFlag::Overflow));  // 0x50 + 0x50 crosses sign bit
    REQUIRE(cpu.flag(StatusFlag::Negative));
    REQUIRE(cpu.cycles() == (2 + 2));
}

TEST_CASE(test_branch_taken_same_page) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0xC000, 0xA9); bus.write(0xC001, 0x00);  // LDA #$00 (sets Zero)
    bus.write(0xC002, 0xF0); bus.write(0xC003, 0x02);  // BEQ +2 → $C006
    bus.write(0xC004, 0xA9); bus.write(0xC005, 0x01);  // LDA #$01 (skipped)
    bus.write(0xC006, 0xA9); bus.write(0xC007, 0x02);  // LDA #$02 (branch target)

    cpu.reset(0xC000);
    cpu.step();  // LDA
    cpu.step();  // BEQ taken, no page cross → base 2 + 1
    cpu.step();  // LDA at target

    REQUIRE(cpu.a() == 0x02);
    REQUIRE(cpu.cycles() == (2 + 3 + 2));
}

TEST_CASE(test_branch_taken_page_cross_adds_extra_cycle) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    // JMP lets us position the BEQ right before a page boundary without
    // stepping through hundreds of NOPs.
    bus.write(0xC000, 0xA9); bus.write(0xC001, 0x00);                           // LDA #$00
    bus.write(0xC002, 0x4C); bus.write(0xC003, 0xFD); bus.write(0xC004, 0xC0);  // JMP $C0FD
    // BEQ at $C0FD: after fetching operand PC=$C0FF, target=$C0FF+4=$C103
    bus.write(0xC0FD, 0xF0); bus.write(0xC0FE, 0x04);                           // BEQ +4 (crosses page)
    bus.write(0xC0FF, 0xA9); bus.write(0xC100, 0x01);                           // LDA #$01 (skipped)
    bus.write(0xC103, 0xA9); bus.write(0xC104, 0x02);                           // LDA #$02 (target)

    cpu.reset(0xC000);
    cpu.step();  // LDA #$00 (2)
    cpu.step();  // JMP      (3)
    cpu.step();  // BEQ taken, crosses $C0xx→$C1xx: base 2 +1 taken +1 cross = 4
    cpu.step();  // LDA #$02 (2)

    REQUIRE(cpu.a() == 0x02);
    REQUIRE(cpu.cycles() == (2 + 3 + 4 + 2));
}

TEST_CASE(test_jsr_and_rts_round_trip) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0xC000, 0x20);  // JSR $C005
    bus.write(0xC001, 0x05);
    bus.write(0xC002, 0xC0);
    bus.write(0xC003, 0xA9);  // LDA #$10 (will run after RTS)
    bus.write(0xC004, 0x10);
    bus.write(0xC005, 0xA9);  // subroutine: LDA #$99
    bus.write(0xC006, 0x99);
    bus.write(0xC007, 0x60);  // RTS

    cpu.reset(0xC000);
    cpu.step();  // JSR -> jumps to C005
    cpu.step();  // LDA #$99
    cpu.step();  // RTS
    cpu.step();  // LDA #$10 (back in caller)

    REQUIRE(cpu.a() == 0x10);
    REQUIRE(cpu.cycles() == (6 + 2 + 6 + 2));
    REQUIRE(cpu.sp() == 0xFD);  // stack balanced after return
}

TEST_CASE(test_stack_push_pull_roundtrip) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0xC000, 0xA9);  // LDA #$33
    bus.write(0xC001, 0x33);
    bus.write(0xC002, 0x48);  // PHA
    bus.write(0xC003, 0xA9);  // LDA #$00
    bus.write(0xC004, 0x00);
    bus.write(0xC005, 0x68);  // PLA

    cpu.reset(0xC000);
    cpu.step();  // LDA #$33
    cpu.step();  // PHA
    cpu.step();  // LDA #$00
    cpu.step();  // PLA

    REQUIRE(cpu.a() == 0x33);
    REQUIRE(cpu.sp() == 0xFD);
    REQUIRE(cpu.cycles() == (2 + 3 + 2 + 4));
}

// ---------------------------------------------------------------------------
// Addressing mode tests
// ---------------------------------------------------------------------------

TEST_CASE(test_lda_abs_x_no_page_cross) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0x0205, 0xBE);          // data at base+X
    bus.write(0xC000, 0xA2); bus.write(0xC001, 0x05);  // LDX #$05
    bus.write(0xC002, 0xBD); bus.write(0xC003, 0x00); bus.write(0xC004, 0x02);  // LDA $0200,X

    cpu.reset(0xC000);
    cpu.step();  // LDX
    cpu.step();  // LDA

    REQUIRE(cpu.a() == 0xBE);
    REQUIRE(cpu.cycles() == (2 + 4));  // no page cross
}

TEST_CASE(test_lda_abs_x_page_cross_adds_cycle) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0x0100, 0xAB);          // data at $00FF + 1 = $0100
    bus.write(0xC000, 0xA2); bus.write(0xC001, 0x01);  // LDX #$01
    bus.write(0xC002, 0xBD); bus.write(0xC003, 0xFF); bus.write(0xC004, 0x00);  // LDA $00FF,X

    cpu.reset(0xC000);
    cpu.step();  // LDX
    cpu.step();  // LDA

    REQUIRE(cpu.a() == 0xAB);
    REQUIRE(cpu.cycles() == (2 + 5));  // +1 for page crossing
}

TEST_CASE(test_sta_abs_x_no_page_cross_penalty) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0xC000, 0xA9); bus.write(0xC001, 0x77);  // LDA #$77
    bus.write(0xC002, 0xA2); bus.write(0xC003, 0x01);  // LDX #$01
    // STA $00FF,X  (crosses to $0100)
    bus.write(0xC004, 0x9D); bus.write(0xC005, 0xFF); bus.write(0xC006, 0x00);

    cpu.reset(0xC000);
    cpu.step();  // LDA
    cpu.step();  // LDX
    cpu.step();  // STA

    REQUIRE(bus.read(0x0100) == 0x77);
    REQUIRE(cpu.cycles() == (2 + 2 + 5));  // STA abs,X is always 5; no extra cycle
}

TEST_CASE(test_lda_indexed_indirect) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    // Pointer at zero page 0x04: lo=$10, hi=$02 → address $0210
    bus.write(0x04, 0x10);
    bus.write(0x05, 0x02);
    bus.write(0x0210, 0xCC);  // data

    bus.write(0xC000, 0xA2); bus.write(0xC001, 0x02);  // LDX #$02
    bus.write(0xC002, 0xA1); bus.write(0xC003, 0x02);  // LDA ($02,X) → ptr at 0x04

    cpu.reset(0xC000);
    cpu.step();  // LDX
    cpu.step();  // LDA

    REQUIRE(cpu.a() == 0xCC);
    REQUIRE(cpu.cycles() == (2 + 6));
}

TEST_CASE(test_lda_indirect_indexed_page_cross) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    // Pointer at zero page 0x10: lo=$FF, hi=$01 → base $01FF
    bus.write(0x10, 0xFF);
    bus.write(0x11, 0x01);
    bus.write(0x0200, 0xDD);  // data at base + Y = $01FF + 1 = $0200

    bus.write(0xC000, 0xA0); bus.write(0xC001, 0x01);  // LDY #$01
    bus.write(0xC002, 0xB1); bus.write(0xC003, 0x10);  // LDA ($10),Y

    cpu.reset(0xC000);
    cpu.step();  // LDY
    cpu.step();  // LDA

    REQUIRE(cpu.a() == 0xDD);
    REQUIRE(cpu.cycles() == (2 + 6));  // base 5 +1 for page cross
}

TEST_CASE(test_cmp_sets_flags_correctly) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0xC000, 0xA9); bus.write(0xC001, 0x42);  // LDA #$42
    bus.write(0xC002, 0xC9); bus.write(0xC003, 0x42);  // CMP #$42

    cpu.reset(0xC000);
    cpu.step();  // LDA
    cpu.step();  // CMP

    REQUIRE(cpu.flag(StatusFlag::Zero));    // equal
    REQUIRE(cpu.flag(StatusFlag::Carry));   // A >= operand
    REQUIRE(!cpu.flag(StatusFlag::Negative));

    // Now compare with a larger value: A < operand
    bus.write(0xC004, 0xC9); bus.write(0xC005, 0x50);  // CMP #$50
    cpu.step();

    REQUIRE(!cpu.flag(StatusFlag::Zero));
    REQUIRE(!cpu.flag(StatusFlag::Carry));  // A < operand → borrow, carry clear
}

TEST_CASE(test_and_ora_eor) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0xC000, 0xA9); bus.write(0xC001, 0xFF);  // LDA #$FF
    bus.write(0xC002, 0x29); bus.write(0xC003, 0x0F);  // AND #$0F  → A = $0F
    bus.write(0xC004, 0x09); bus.write(0xC005, 0xF0);  // ORA #$F0  → A = $FF
    bus.write(0xC006, 0x49); bus.write(0xC007, 0xFF);  // EOR #$FF  → A = $00

    cpu.reset(0xC000);
    cpu.step();  // LDA
    cpu.step();  // AND
    REQUIRE(cpu.a() == 0x0F);

    cpu.step();  // ORA
    REQUIRE(cpu.a() == 0xFF);

    cpu.step();  // EOR
    REQUIRE(cpu.a() == 0x00);
    REQUIRE(cpu.flag(StatusFlag::Zero));
}

TEST_CASE(test_jmp_indirect_page_wrap_bug) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    // JMP ($01FF): lo is at $01FF, hi should wrap to $0100 (not $0200)
    bus.write(0x01FF, 0x80);  // lo byte of target
    bus.write(0x0100, 0xC0);  // hi byte of target (6502 bug: wraps to $0100)
    bus.write(0x0200, 0x00);  // would be hi if no bug

    bus.write(0xC000, 0x6C); bus.write(0xC001, 0xFF); bus.write(0xC002, 0x01);  // JMP ($01FF)

    cpu.reset(0xC000);
    cpu.step();

    REQUIRE(cpu.pc() == 0xC080);  // hi=$C0, lo=$80
    REQUIRE(cpu.cycles() == 5);
}

TEST_CASE(test_stx_and_sty) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0xC000, 0xA2); bus.write(0xC001, 0x55);  // LDX #$55
    bus.write(0xC002, 0x86); bus.write(0xC003, 0x20);  // STX $20
    bus.write(0xC004, 0xA0); bus.write(0xC005, 0x66);  // LDY #$66
    bus.write(0xC006, 0x84); bus.write(0xC007, 0x21);  // STY $21

    cpu.reset(0xC000);
    cpu.step();  // LDX
    cpu.step();  // STX
    cpu.step();  // LDY
    cpu.step();  // STY

    REQUIRE(bus.read(0x20) == 0x55);
    REQUIRE(bus.read(0x21) == 0x66);
    REQUIRE(cpu.cycles() == (2 + 3 + 2 + 3));
}

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
    REQUIRE(cpu.cycles() == 18);
}

TEST_CASE(test_shifts_accumulator) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    // ASL A: 0x81 << 1 = 0x02, bit7 → Carry
    bus.write(0xC000, 0xA9); bus.write(0xC001, 0x81);  // LDA #$81
    bus.write(0xC002, 0x0A);                            // ASL A
    // LSR A: 0x02 >> 1 = 0x01, bit0 → Carry
    bus.write(0xC003, 0x4A);                            // LSR A
    // SEC then ROL A: 0x01 ROL with carry=1 → (0x01<<1)|1=0x03, old bit7(0x01)=0 → carry=0
    bus.write(0xC004, 0x38);                            // SEC
    bus.write(0xC005, 0x2A);                            // ROL A
    // ROR A: 0x03 ROR with carry=0 → 0x03>>1=0x01, old bit0(0x03)=1 → carry=1
    bus.write(0xC006, 0x6A);                            // ROR A
    // LDA #$80, then ASL A → 0x00, Carry=1, Zero=1
    bus.write(0xC007, 0xA9); bus.write(0xC008, 0x80);  // LDA #$80
    bus.write(0xC009, 0x0A);                            // ASL A → 0x00, Carry=1, Zero=1

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
    REQUIRE(!cpu.flag(StatusFlag::Negative));  // LSR always clears N

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

    cpu.step();  // LDA #$80 (2)
    cpu.step();  // ASL A → A=0x00, Carry=1, Zero=1, N=0
    REQUIRE(cpu.a() == 0x00);
    REQUIRE(cpu.flag(StatusFlag::Zero));
    REQUIRE(cpu.flag(StatusFlag::Carry));
    REQUIRE(!cpu.flag(StatusFlag::Negative));
    REQUIRE(cpu.cycles() == 16);  // 12 + LDA(2) + ASL(2)
}

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

TEST_CASE(test_asl_absx_page_cross_still_7_cycles) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0x0301, 0x01);  // data at $02FF + X=$02 = $0301 (crosses page)
    bus.write(0xC000, 0xA2); bus.write(0xC001, 0x02);            // LDX #$02
    bus.write(0xC002, 0x1E); bus.write(0xC003, 0xFF); bus.write(0xC004, 0x02);  // ASL $02FF,X

    cpu.reset(0xC000);
    cpu.step();  // LDX (2)
    cpu.step();  // ASL ABS,X crossing $02xx→$03xx (must still be 7, not 8)

    REQUIRE(bus.read(0x0301) == 0x02);
    REQUIRE(cpu.cycles() == (2 + 7));
}

TEST_CASE(test_rol_zpx_modifies_memory) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    // ROL $10,X with X=2 → address $12, value 0x01, carry=0 → result=0x02
    bus.write(0x0012, 0x01);
    bus.write(0xC000, 0xA2); bus.write(0xC001, 0x02);  // LDX #$02
    bus.write(0xC002, 0x36); bus.write(0xC003, 0x10);  // ROL $10,X

    cpu.reset(0xC000);
    cpu.step();  // LDX (2)
    cpu.step();  // ROL ZP,X (6)

    REQUIRE(bus.read(0x0012) == 0x02);
    REQUIRE(!cpu.flag(StatusFlag::Carry));
    REQUIRE(cpu.cycles() == (2 + 6));
}

TEST_CASE(test_lsr_abs_modifies_memory) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0x0300, 0xFE);  // data at abs address $0300
    bus.write(0xC000, 0x4E); bus.write(0xC001, 0x00); bus.write(0xC002, 0x03);  // LSR $0300

    cpu.reset(0xC000);
    cpu.step();

    REQUIRE(bus.read(0x0300) == 0x7F);
    REQUIRE(!cpu.flag(StatusFlag::Carry));  // bit0 of 0xFE = 0
    REQUIRE(!cpu.flag(StatusFlag::Negative));
    REQUIRE(cpu.cycles() == 6);
}

TEST_CASE(test_inc_dec_zp) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0x0010, 0xFE);
    bus.write(0xC000, 0xE6); bus.write(0xC001, 0x10);  // INC $10 → $FF, N=1
    bus.write(0xC002, 0xE6); bus.write(0xC003, 0x10);  // INC $10 → $00, Z=1, wraps
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
