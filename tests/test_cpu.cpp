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

TEST_CASE(test_branch_taken_adds_cycles) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.reset();

    bus.write(0xC0FE, 0xA9);  // LDA #$00
    bus.write(0xC0FF, 0x00);
    bus.write(0xC100, 0xF0);  // BEQ +2
    bus.write(0xC101, 0x02);
    bus.write(0xC102, 0xA9);  // LDA #$01 (skipped)
    bus.write(0xC103, 0x01);
    bus.write(0xC104, 0xA9);  // LDA #$02 (branch target)
    bus.write(0xC105, 0x02);

    cpu.reset(0xC0FE);
    cpu.step();  // LDA #$00
    cpu.step();  // BEQ (taken, crosses page)
    cpu.step();  // LDA #$02

    REQUIRE(cpu.a() == 0x02);
    // Branch costs: base 2 +1 for taken +1 for page cross
    REQUIRE(cpu.cycles() == (2 + 4 + 2));
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

