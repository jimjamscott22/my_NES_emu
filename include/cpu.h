#pragma once

#include <array>

#include "types.h"

class Bus;

enum class StatusFlag : u8 {
    Carry = 1 << 0,
    Zero = 1 << 1,
    InterruptDisable = 1 << 2,
    Decimal = 1 << 3,
    Break = 1 << 4,
    Unused = 1 << 5,
    Overflow = 1 << 6,
    Negative = 1 << 7
};

struct Instruction {
    const char* name;
    u8 base_cycles;
    u8 (class Cpu6502::*handler)();
};

struct AddressingResult {
    u16 addr;
    u8 value;
    bool page_crossed;
};

class Cpu6502 {
public:
    explicit Cpu6502(Bus& bus);

    // Reset registers and cycle counter. Default PC is 0xC000, which is the
    // entry point used by the common nestest ROM. Override as needed.
    void reset(u16 start_vector = 0xC000);
    void step();

    u64 cycles() const { return cycles_; }
    u16 pc() const { return pc_; }
    u8 a() const { return a_; }
    u8 x() const { return x_; }
    u8 y() const { return y_; }
    u8 sp() const { return sp_; }
    u8 status() const { return status_; }
    bool flag(StatusFlag f) const { return (status_ & static_cast<u8>(f)) != 0; }

private:
    Bus* bus_{nullptr};

    u8 a_{0};
    u8 x_{0};
    u8 y_{0};
    u8 sp_{0xFD};
    u8 status_{static_cast<u8>(StatusFlag::Unused) |
               static_cast<u8>(StatusFlag::InterruptDisable)};
    u16 pc_{0};
    u64 cycles_{0};

    u8 fetch8();
    u16 fetch16();

    AddressingResult immediate();
    AddressingResult zero_page();
    AddressingResult absolute();
    AddressingResult relative();

    void set_flag(StatusFlag flag, bool value);
    void update_zn(u8 value);
    void push(u8 value);
    u8 pop();

    u8 adc(u8 value);
    u8 branch_if(bool condition);

    // opcode handlers return any extra cycles incurred (page crossings/branches)
    u8 op_nop();
    u8 op_lda_imm();
    u8 op_lda_zp();
    u8 op_lda_abs();
    u8 op_sta_zp();
    u8 op_sta_abs();
    u8 op_ldx_imm();
    u8 op_ldx_zp();
    u8 op_ldx_abs();
    u8 op_ldy_imm();
    u8 op_ldy_zp();
    u8 op_ldy_abs();
    u8 op_adc_imm();
    u8 op_adc_zp();
    u8 op_adc_abs();
    u8 op_sbc_imm();
    u8 op_sbc_zp();
    u8 op_sbc_abs();
    u8 op_tax();
    u8 op_tay();
    u8 op_txa();
    u8 op_tya();
    u8 op_pha();
    u8 op_pla();
    u8 op_php();
    u8 op_plp();
    u8 op_jmp_abs();
    u8 op_jsr();
    u8 op_rts();
    u8 op_beq();
    u8 op_bne();
    u8 op_bcs();
    u8 op_bcc();

    static std::array<Instruction, 256> build_table();
    static const std::array<Instruction, 256> kInstructionTable;
};

