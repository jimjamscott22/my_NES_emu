#include "cpu.h"

#include "bus.h"

namespace {
constexpr u8 kUnusedFlag = static_cast<u8>(StatusFlag::Unused);
}

Cpu6502::Cpu6502(Bus& bus) : bus_(&bus) {}

void Cpu6502::reset(u16 start_vector) {
    a_ = x_ = y_ = 0;
    sp_ = 0xFD;
    status_ = static_cast<u8>(StatusFlag::InterruptDisable) | kUnusedFlag;
    cycles_ = 0;
    pc_ = start_vector;
}

u8 Cpu6502::fetch8() {
    return bus_->read(pc_++);
}

u16 Cpu6502::fetch16() {
    u16 lo = fetch8();
    u16 hi = fetch8();
    return static_cast<u16>((hi << 8) | lo);
}

AddressingResult Cpu6502::immediate() {
    u16 addr = pc_++;
    return {addr, bus_->read(addr), false};
}

AddressingResult Cpu6502::zero_page() {
    u16 addr = fetch8();
    return {addr, bus_->read(addr), false};
}

AddressingResult Cpu6502::absolute() {
    u16 addr = fetch16();
    return {addr, bus_->read(addr), false};
}

AddressingResult Cpu6502::relative() {
    i8 offset = static_cast<i8>(fetch8());
    u16 target = static_cast<u16>(pc_ + offset);
    bool crossed = ((pc_ & 0xFF00) != (target & 0xFF00));
    return {target, 0, crossed};
}

void Cpu6502::set_flag(StatusFlag flag, bool value) {
    if (value) {
        status_ |= static_cast<u8>(flag);
    } else {
        status_ &= ~static_cast<u8>(flag);
    }
}

void Cpu6502::update_zn(u8 value) {
    set_flag(StatusFlag::Zero, value == 0);
    set_flag(StatusFlag::Negative, (value & 0x80) != 0);
}

void Cpu6502::push(u8 value) {
    bus_->write(static_cast<u16>(0x0100 | sp_), value);
    --sp_;
}

u8 Cpu6502::pop() {
    ++sp_;
    return bus_->read(static_cast<u16>(0x0100 | sp_));
}

u8 Cpu6502::adc(u8 value) {
    u16 sum = static_cast<u16>(a_) + static_cast<u16>(value) +
              (flag(StatusFlag::Carry) ? 1 : 0);
    set_flag(StatusFlag::Carry, sum > 0xFF);

    u8 result = static_cast<u8>(sum & 0xFF);
    set_flag(StatusFlag::Overflow,
             ((~(a_ ^ value) & (a_ ^ result)) & 0x80) != 0);

    a_ = result;
    update_zn(a_);
    return 0;
}

u8 Cpu6502::branch_if(bool condition) {
    i8 offset = static_cast<i8>(fetch8());
    if (!condition) {
        return 0;
    }

    u16 prev_pc = pc_;
    pc_ = static_cast<u16>(pc_ + offset);

    u8 extra = 1;  // branch taken
    if ((prev_pc & 0xFF00) != (pc_ & 0xFF00)) {
        extra += 1;  // crossed page boundary
    }
    return extra;
}

u8 Cpu6502::op_nop() { return 0; }

u8 Cpu6502::op_lda_imm() {
    auto res = immediate();
    a_ = res.value;
    update_zn(a_);
    return 0;
}

u8 Cpu6502::op_lda_zp() {
    auto res = zero_page();
    a_ = res.value;
    update_zn(a_);
    return 0;
}

u8 Cpu6502::op_lda_abs() {
    auto res = absolute();
    a_ = res.value;
    update_zn(a_);
    return 0;
}

u8 Cpu6502::op_sta_zp() {
    u16 addr = fetch8();
    bus_->write(addr, a_);
    return 0;
}

u8 Cpu6502::op_sta_abs() {
    u16 addr = fetch16();
    bus_->write(addr, a_);
    return 0;
}

u8 Cpu6502::op_ldx_imm() {
    auto res = immediate();
    x_ = res.value;
    update_zn(x_);
    return 0;
}

u8 Cpu6502::op_ldx_zp() {
    auto res = zero_page();
    x_ = res.value;
    update_zn(x_);
    return 0;
}

u8 Cpu6502::op_ldx_abs() {
    auto res = absolute();
    x_ = res.value;
    update_zn(x_);
    return 0;
}

u8 Cpu6502::op_ldy_imm() {
    auto res = immediate();
    y_ = res.value;
    update_zn(y_);
    return 0;
}

u8 Cpu6502::op_ldy_zp() {
    auto res = zero_page();
    y_ = res.value;
    update_zn(y_);
    return 0;
}

u8 Cpu6502::op_ldy_abs() {
    auto res = absolute();
    y_ = res.value;
    update_zn(y_);
    return 0;
}

u8 Cpu6502::op_adc_imm() {
    auto res = immediate();
    return adc(res.value);
}

u8 Cpu6502::op_adc_zp() {
    auto res = zero_page();
    return adc(res.value);
}

u8 Cpu6502::op_adc_abs() {
    auto res = absolute();
    return adc(res.value);
}

u8 Cpu6502::op_sbc_imm() {
    auto res = immediate();
    return adc(static_cast<u8>(res.value ^ 0xFF));
}

u8 Cpu6502::op_sbc_zp() {
    auto res = zero_page();
    return adc(static_cast<u8>(res.value ^ 0xFF));
}

u8 Cpu6502::op_sbc_abs() {
    auto res = absolute();
    return adc(static_cast<u8>(res.value ^ 0xFF));
}

u8 Cpu6502::op_tax() {
    x_ = a_;
    update_zn(x_);
    return 0;
}

u8 Cpu6502::op_tay() {
    y_ = a_;
    update_zn(y_);
    return 0;
}

u8 Cpu6502::op_txa() {
    a_ = x_;
    update_zn(a_);
    return 0;
}

u8 Cpu6502::op_tya() {
    a_ = y_;
    update_zn(a_);
    return 0;
}

u8 Cpu6502::op_pha() {
    push(a_);
    return 0;
}

u8 Cpu6502::op_pla() {
    a_ = pop();
    update_zn(a_);
    return 0;
}

u8 Cpu6502::op_php() {
    // When pushing status, the Break + Unused bits are set.
    push(status_ | static_cast<u8>(StatusFlag::Break) | kUnusedFlag);
    return 0;
}

u8 Cpu6502::op_plp() {
    status_ = pop();
    status_ |= kUnusedFlag;  // bit 5 is always set on 6502
    return 0;
}

u8 Cpu6502::op_jmp_abs() {
    pc_ = fetch16();
    return 0;
}

u8 Cpu6502::op_jsr() {
    u16 target = fetch16();
    u16 return_addr = static_cast<u16>(pc_ - 1);
    push(static_cast<u8>(return_addr >> 8));
    push(static_cast<u8>(return_addr & 0xFF));
    pc_ = target;
    return 0;
}

u8 Cpu6502::op_rts() {
    u8 lo = pop();
    u8 hi = pop();
    pc_ = static_cast<u16>((hi << 8) | lo);
    pc_ = static_cast<u16>(pc_ + 1);
    return 0;
}

u8 Cpu6502::op_beq() { return branch_if(flag(StatusFlag::Zero)); }
u8 Cpu6502::op_bne() { return branch_if(!flag(StatusFlag::Zero)); }
u8 Cpu6502::op_bcs() { return branch_if(flag(StatusFlag::Carry)); }
u8 Cpu6502::op_bcc() { return branch_if(!flag(StatusFlag::Carry)); }

std::array<Instruction, 256> Cpu6502::build_table() {
    std::array<Instruction, 256> table{};
    for (auto& inst : table) {
        inst = {"NOP", 2, &Cpu6502::op_nop};
    }

    table[0xA9] = {"LDA IMM", 2, &Cpu6502::op_lda_imm};
    table[0xA5] = {"LDA ZP", 3, &Cpu6502::op_lda_zp};
    table[0xAD] = {"LDA ABS", 4, &Cpu6502::op_lda_abs};

    table[0x85] = {"STA ZP", 3, &Cpu6502::op_sta_zp};
    table[0x8D] = {"STA ABS", 4, &Cpu6502::op_sta_abs};

    table[0xA2] = {"LDX IMM", 2, &Cpu6502::op_ldx_imm};
    table[0xA6] = {"LDX ZP", 3, &Cpu6502::op_ldx_zp};
    table[0xAE] = {"LDX ABS", 4, &Cpu6502::op_ldx_abs};

    table[0xA0] = {"LDY IMM", 2, &Cpu6502::op_ldy_imm};
    table[0xA4] = {"LDY ZP", 3, &Cpu6502::op_ldy_zp};
    table[0xAC] = {"LDY ABS", 4, &Cpu6502::op_ldy_abs};

    table[0x69] = {"ADC IMM", 2, &Cpu6502::op_adc_imm};
    table[0x65] = {"ADC ZP", 3, &Cpu6502::op_adc_zp};
    table[0x6D] = {"ADC ABS", 4, &Cpu6502::op_adc_abs};

    table[0xE9] = {"SBC IMM", 2, &Cpu6502::op_sbc_imm};
    table[0xE5] = {"SBC ZP", 3, &Cpu6502::op_sbc_zp};
    table[0xED] = {"SBC ABS", 4, &Cpu6502::op_sbc_abs};

    table[0xAA] = {"TAX", 2, &Cpu6502::op_tax};
    table[0xA8] = {"TAY", 2, &Cpu6502::op_tay};
    table[0x8A] = {"TXA", 2, &Cpu6502::op_txa};
    table[0x98] = {"TYA", 2, &Cpu6502::op_tya};

    table[0x48] = {"PHA", 3, &Cpu6502::op_pha};
    table[0x68] = {"PLA", 4, &Cpu6502::op_pla};
    table[0x08] = {"PHP", 3, &Cpu6502::op_php};
    table[0x28] = {"PLP", 4, &Cpu6502::op_plp};

    table[0x4C] = {"JMP ABS", 3, &Cpu6502::op_jmp_abs};
    table[0x20] = {"JSR", 6, &Cpu6502::op_jsr};
    table[0x60] = {"RTS", 6, &Cpu6502::op_rts};

    table[0xF0] = {"BEQ", 2, &Cpu6502::op_beq};
    table[0xD0] = {"BNE", 2, &Cpu6502::op_bne};
    table[0xB0] = {"BCS", 2, &Cpu6502::op_bcs};
    table[0x90] = {"BCC", 2, &Cpu6502::op_bcc};

    return table;
}

const std::array<Instruction, 256> Cpu6502::kInstructionTable =
    Cpu6502::build_table();

void Cpu6502::step() {
    u8 opcode = fetch8();
    const auto& inst = kInstructionTable[opcode];

    cycles_ += inst.base_cycles;
    u8 extra = (this->*inst.handler)();
    cycles_ += extra;
}

