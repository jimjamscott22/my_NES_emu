#pragma once

#include <array>

#include "types.h"

class Bus;
class Cpu6502;

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
    u8 (Cpu6502::*handler)();
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
    u16 read16_zp(u8 zp_addr);  // zero-page 16-bit read; high byte wraps within page

    // Addressing modes — each returns {effective_addr, value_at_addr, page_crossed}
    AddressingResult immediate();
    AddressingResult zero_page();
    AddressingResult zero_page_x();
    AddressingResult zero_page_y();
    AddressingResult absolute();
    AddressingResult absolute_x();
    AddressingResult absolute_y();
    AddressingResult relative();
    AddressingResult indirect();          // for JMP ($addr); implements 6502 page-wrap bug
    AddressingResult indexed_indirect();  // (zp,X)
    AddressingResult indirect_indexed();  // (zp),Y

    void set_flag(StatusFlag flag, bool value);
    void update_zn(u8 value);
    void push(u8 value);
    u8 pop();

    u8 adc(u8 value);
    u8 branch_if(bool condition);
    void compare(u8 reg, u8 value);

    // opcode handlers return any extra cycles incurred (page crossings/branches)
    u8 op_nop();

    // LDA
    u8 op_lda_imm();
    u8 op_lda_zp();
    u8 op_lda_zpx();
    u8 op_lda_abs();
    u8 op_lda_absx();
    u8 op_lda_absy();
    u8 op_lda_indx();
    u8 op_lda_indy();

    // STA
    u8 op_sta_zp();
    u8 op_sta_zpx();
    u8 op_sta_abs();
    u8 op_sta_absx();
    u8 op_sta_absy();
    u8 op_sta_indx();
    u8 op_sta_indy();

    // LDX
    u8 op_ldx_imm();
    u8 op_ldx_zp();
    u8 op_ldx_zpy();
    u8 op_ldx_abs();
    u8 op_ldx_absy();

    // LDY
    u8 op_ldy_imm();
    u8 op_ldy_zp();
    u8 op_ldy_zpx();
    u8 op_ldy_abs();
    u8 op_ldy_absx();

    // STX
    u8 op_stx_zp();
    u8 op_stx_abs();
    u8 op_stx_zpy();

    // STY
    u8 op_sty_zp();
    u8 op_sty_abs();
    u8 op_sty_zpx();

    // ADC
    u8 op_adc_imm();
    u8 op_adc_zp();
    u8 op_adc_zpx();
    u8 op_adc_abs();
    u8 op_adc_absx();
    u8 op_adc_absy();
    u8 op_adc_indx();
    u8 op_adc_indy();

    // SBC
    u8 op_sbc_imm();
    u8 op_sbc_zp();
    u8 op_sbc_zpx();
    u8 op_sbc_abs();
    u8 op_sbc_absx();
    u8 op_sbc_absy();
    u8 op_sbc_indx();
    u8 op_sbc_indy();

    // AND
    u8 op_and_imm();
    u8 op_and_zp();
    u8 op_and_zpx();
    u8 op_and_abs();
    u8 op_and_absx();
    u8 op_and_absy();
    u8 op_and_indx();
    u8 op_and_indy();

    // ORA
    u8 op_ora_imm();
    u8 op_ora_zp();
    u8 op_ora_zpx();
    u8 op_ora_abs();
    u8 op_ora_absx();
    u8 op_ora_absy();
    u8 op_ora_indx();
    u8 op_ora_indy();

    // EOR
    u8 op_eor_imm();
    u8 op_eor_zp();
    u8 op_eor_zpx();
    u8 op_eor_abs();
    u8 op_eor_absx();
    u8 op_eor_absy();
    u8 op_eor_indx();
    u8 op_eor_indy();

    // CMP
    u8 op_cmp_imm();
    u8 op_cmp_zp();
    u8 op_cmp_zpx();
    u8 op_cmp_abs();
    u8 op_cmp_absx();
    u8 op_cmp_absy();
    u8 op_cmp_indx();
    u8 op_cmp_indy();

    // Transfers
    u8 op_tax();
    u8 op_tay();
    u8 op_txa();
    u8 op_tya();

    // Stack
    u8 op_pha();
    u8 op_pla();
    u8 op_php();
    u8 op_plp();

    // Control flow
    u8 op_jmp_abs();
    u8 op_jmp_ind();
    u8 op_jsr();
    u8 op_rts();
    u8 op_beq();
    u8 op_bne();
    u8 op_bcs();
    u8 op_bcc();

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

    static std::array<Instruction, 256> build_table();
    static const std::array<Instruction, 256> kInstructionTable;
};
