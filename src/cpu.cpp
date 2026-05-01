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

u16 Cpu6502::read16_zp(u8 zp_addr) {
    u16 lo = bus_->read(zp_addr);
    u16 hi = bus_->read(static_cast<u8>(zp_addr + 1));  // wraps within zero page
    return static_cast<u16>((hi << 8) | lo);
}

// ---------------------------------------------------------------------------
// Addressing modes
// ---------------------------------------------------------------------------

AddressingResult Cpu6502::immediate() {
    u16 addr = pc_++;
    return {addr, bus_->read(addr), false};
}

AddressingResult Cpu6502::zero_page() {
    u16 addr = fetch8();
    return {addr, bus_->read(addr), false};
}

AddressingResult Cpu6502::zero_page_x() {
    u8 addr = static_cast<u8>(fetch8() + x_);
    return {addr, bus_->read(addr), false};
}

AddressingResult Cpu6502::zero_page_y() {
    u8 addr = static_cast<u8>(fetch8() + y_);
    return {addr, bus_->read(addr), false};
}

AddressingResult Cpu6502::absolute() {
    u16 addr = fetch16();
    return {addr, bus_->read(addr), false};
}

AddressingResult Cpu6502::absolute_x() {
    u16 base = fetch16();
    u16 addr = static_cast<u16>(base + x_);
    bool crossed = (base & 0xFF00) != (addr & 0xFF00);
    return {addr, bus_->read(addr), crossed};
}

AddressingResult Cpu6502::absolute_y() {
    u16 base = fetch16();
    u16 addr = static_cast<u16>(base + y_);
    bool crossed = (base & 0xFF00) != (addr & 0xFF00);
    return {addr, bus_->read(addr), crossed};
}

AddressingResult Cpu6502::relative() {
    i8 offset = static_cast<i8>(fetch8());
    u16 target = static_cast<u16>(pc_ + offset);
    bool crossed = ((pc_ & 0xFF00) != (target & 0xFF00));
    return {target, 0, crossed};
}

AddressingResult Cpu6502::indirect() {
    u16 ptr = fetch16();
    // 6502 hardware bug: if ptr LSB is $FF, high byte wraps within the same page
    u16 lo = bus_->read(ptr);
    u16 hi = bus_->read((ptr & 0xFF00) | ((ptr + 1) & 0x00FF));
    u16 addr = static_cast<u16>((hi << 8) | lo);
    return {addr, 0, false};
}

AddressingResult Cpu6502::indexed_indirect() {
    // (zp,X): dereference zero-page pointer at (zp + X)
    u8 zp = static_cast<u8>(fetch8() + x_);
    u16 addr = read16_zp(zp);
    return {addr, bus_->read(addr), false};
}

AddressingResult Cpu6502::indirect_indexed() {
    // (zp),Y: dereference zero-page pointer, then add Y
    u8 zp = fetch8();
    u16 base = read16_zp(zp);
    u16 addr = static_cast<u16>(base + y_);
    bool crossed = (base & 0xFF00) != (addr & 0xFF00);
    return {addr, bus_->read(addr), crossed};
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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

void Cpu6502::compare(u8 reg, u8 value) {
    u8 result = static_cast<u8>(reg - value);
    set_flag(StatusFlag::Carry, reg >= value);
    update_zn(result);
}

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

// CPX
u8 Cpu6502::op_cpx_imm() { auto r = immediate(); compare(x_, r.value); return 0; }
u8 Cpu6502::op_cpx_zp()  { auto r = zero_page(); compare(x_, r.value); return 0; }
u8 Cpu6502::op_cpx_abs() { auto r = absolute();  compare(x_, r.value); return 0; }

// CPY
u8 Cpu6502::op_cpy_imm() { auto r = immediate(); compare(y_, r.value); return 0; }
u8 Cpu6502::op_cpy_zp()  { auto r = zero_page(); compare(y_, r.value); return 0; }
u8 Cpu6502::op_cpy_abs() { auto r = absolute();  compare(y_, r.value); return 0; }

// Remaining branches
u8 Cpu6502::op_bmi() { return branch_if(flag(StatusFlag::Negative)); }
u8 Cpu6502::op_bpl() { return branch_if(!flag(StatusFlag::Negative)); }
u8 Cpu6502::op_bvs() { return branch_if(flag(StatusFlag::Overflow)); }
u8 Cpu6502::op_bvc() { return branch_if(!flag(StatusFlag::Overflow)); }
// Stack transfers
u8 Cpu6502::op_tsx() { x_ = sp_; update_zn(x_); return 0; }
u8 Cpu6502::op_txs() { sp_ = x_; return 0; }

// ---------------------------------------------------------------------------
// Opcode handlers
// ---------------------------------------------------------------------------

u8 Cpu6502::op_nop() { return 0; }

// LDA
u8 Cpu6502::op_lda_imm()  { auto r = immediate();        a_ = r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_lda_zp()   { auto r = zero_page();        a_ = r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_lda_zpx()  { auto r = zero_page_x();      a_ = r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_lda_abs()  { auto r = absolute();         a_ = r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_lda_absx() { auto r = absolute_x();       a_ = r.value; update_zn(a_); return r.page_crossed; }
u8 Cpu6502::op_lda_absy() { auto r = absolute_y();       a_ = r.value; update_zn(a_); return r.page_crossed; }
u8 Cpu6502::op_lda_indx() { auto r = indexed_indirect(); a_ = r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_lda_indy() { auto r = indirect_indexed(); a_ = r.value; update_zn(a_); return r.page_crossed; }

// STA — stores never incur a page-crossing penalty
u8 Cpu6502::op_sta_zp()   { u16 addr = fetch8();                  bus_->write(addr, a_);    return 0; }
u8 Cpu6502::op_sta_zpx()  { auto r = zero_page_x();               bus_->write(r.addr, a_);  return 0; }
u8 Cpu6502::op_sta_abs()  { u16 addr = fetch16();                  bus_->write(addr, a_);    return 0; }
u8 Cpu6502::op_sta_absx() { auto r = absolute_x();                bus_->write(r.addr, a_);  return 0; }
u8 Cpu6502::op_sta_absy() { auto r = absolute_y();                bus_->write(r.addr, a_);  return 0; }
u8 Cpu6502::op_sta_indx() { auto r = indexed_indirect();          bus_->write(r.addr, a_);  return 0; }
u8 Cpu6502::op_sta_indy() { auto r = indirect_indexed();          bus_->write(r.addr, a_);  return 0; }

// LDX
u8 Cpu6502::op_ldx_imm()  { auto r = immediate();   x_ = r.value; update_zn(x_); return 0; }
u8 Cpu6502::op_ldx_zp()   { auto r = zero_page();   x_ = r.value; update_zn(x_); return 0; }
u8 Cpu6502::op_ldx_zpy()  { auto r = zero_page_y(); x_ = r.value; update_zn(x_); return 0; }
u8 Cpu6502::op_ldx_abs()  { auto r = absolute();    x_ = r.value; update_zn(x_); return 0; }
u8 Cpu6502::op_ldx_absy() { auto r = absolute_y();  x_ = r.value; update_zn(x_); return r.page_crossed; }

// LDY
u8 Cpu6502::op_ldy_imm()  { auto r = immediate();   y_ = r.value; update_zn(y_); return 0; }
u8 Cpu6502::op_ldy_zp()   { auto r = zero_page();   y_ = r.value; update_zn(y_); return 0; }
u8 Cpu6502::op_ldy_zpx()  { auto r = zero_page_x(); y_ = r.value; update_zn(y_); return 0; }
u8 Cpu6502::op_ldy_abs()  { auto r = absolute();    y_ = r.value; update_zn(y_); return 0; }
u8 Cpu6502::op_ldy_absx() { auto r = absolute_x();  y_ = r.value; update_zn(y_); return r.page_crossed; }

// STX
u8 Cpu6502::op_stx_zp()   { u16 addr = fetch8();            bus_->write(addr, x_);   return 0; }
u8 Cpu6502::op_stx_abs()  { u16 addr = fetch16();           bus_->write(addr, x_);   return 0; }
u8 Cpu6502::op_stx_zpy()  { auto r = zero_page_y();         bus_->write(r.addr, x_); return 0; }

// STY
u8 Cpu6502::op_sty_zp()   { u16 addr = fetch8();            bus_->write(addr, y_);   return 0; }
u8 Cpu6502::op_sty_abs()  { u16 addr = fetch16();           bus_->write(addr, y_);   return 0; }
u8 Cpu6502::op_sty_zpx()  { auto r = zero_page_x();         bus_->write(r.addr, y_); return 0; }

// ADC
u8 Cpu6502::op_adc_imm()  { auto r = immediate();        return adc(r.value); }
u8 Cpu6502::op_adc_zp()   { auto r = zero_page();        return adc(r.value); }
u8 Cpu6502::op_adc_zpx()  { auto r = zero_page_x();      return adc(r.value); }
u8 Cpu6502::op_adc_abs()  { auto r = absolute();         return adc(r.value); }
u8 Cpu6502::op_adc_absx() { auto r = absolute_x();       adc(r.value); return r.page_crossed; }
u8 Cpu6502::op_adc_absy() { auto r = absolute_y();       adc(r.value); return r.page_crossed; }
u8 Cpu6502::op_adc_indx() { auto r = indexed_indirect(); return adc(r.value); }
u8 Cpu6502::op_adc_indy() { auto r = indirect_indexed(); adc(r.value); return r.page_crossed; }

// SBC (implemented as ADC with ones-complement of operand)
u8 Cpu6502::op_sbc_imm()  { auto r = immediate();        return adc(static_cast<u8>(r.value ^ 0xFF)); }
u8 Cpu6502::op_sbc_zp()   { auto r = zero_page();        return adc(static_cast<u8>(r.value ^ 0xFF)); }
u8 Cpu6502::op_sbc_zpx()  { auto r = zero_page_x();      return adc(static_cast<u8>(r.value ^ 0xFF)); }
u8 Cpu6502::op_sbc_abs()  { auto r = absolute();         return adc(static_cast<u8>(r.value ^ 0xFF)); }
u8 Cpu6502::op_sbc_absx() { auto r = absolute_x();       adc(static_cast<u8>(r.value ^ 0xFF)); return r.page_crossed; }
u8 Cpu6502::op_sbc_absy() { auto r = absolute_y();       adc(static_cast<u8>(r.value ^ 0xFF)); return r.page_crossed; }
u8 Cpu6502::op_sbc_indx() { auto r = indexed_indirect(); return adc(static_cast<u8>(r.value ^ 0xFF)); }
u8 Cpu6502::op_sbc_indy() { auto r = indirect_indexed(); adc(static_cast<u8>(r.value ^ 0xFF)); return r.page_crossed; }

// AND
u8 Cpu6502::op_and_imm()  { auto r = immediate();        a_ &= r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_and_zp()   { auto r = zero_page();        a_ &= r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_and_zpx()  { auto r = zero_page_x();      a_ &= r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_and_abs()  { auto r = absolute();         a_ &= r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_and_absx() { auto r = absolute_x();       a_ &= r.value; update_zn(a_); return r.page_crossed; }
u8 Cpu6502::op_and_absy() { auto r = absolute_y();       a_ &= r.value; update_zn(a_); return r.page_crossed; }
u8 Cpu6502::op_and_indx() { auto r = indexed_indirect(); a_ &= r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_and_indy() { auto r = indirect_indexed(); a_ &= r.value; update_zn(a_); return r.page_crossed; }

// ORA
u8 Cpu6502::op_ora_imm()  { auto r = immediate();        a_ |= r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_ora_zp()   { auto r = zero_page();        a_ |= r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_ora_zpx()  { auto r = zero_page_x();      a_ |= r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_ora_abs()  { auto r = absolute();         a_ |= r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_ora_absx() { auto r = absolute_x();       a_ |= r.value; update_zn(a_); return r.page_crossed; }
u8 Cpu6502::op_ora_absy() { auto r = absolute_y();       a_ |= r.value; update_zn(a_); return r.page_crossed; }
u8 Cpu6502::op_ora_indx() { auto r = indexed_indirect(); a_ |= r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_ora_indy() { auto r = indirect_indexed(); a_ |= r.value; update_zn(a_); return r.page_crossed; }

// EOR
u8 Cpu6502::op_eor_imm()  { auto r = immediate();        a_ ^= r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_eor_zp()   { auto r = zero_page();        a_ ^= r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_eor_zpx()  { auto r = zero_page_x();      a_ ^= r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_eor_abs()  { auto r = absolute();         a_ ^= r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_eor_absx() { auto r = absolute_x();       a_ ^= r.value; update_zn(a_); return r.page_crossed; }
u8 Cpu6502::op_eor_absy() { auto r = absolute_y();       a_ ^= r.value; update_zn(a_); return r.page_crossed; }
u8 Cpu6502::op_eor_indx() { auto r = indexed_indirect(); a_ ^= r.value; update_zn(a_); return 0; }
u8 Cpu6502::op_eor_indy() { auto r = indirect_indexed(); a_ ^= r.value; update_zn(a_); return r.page_crossed; }

// CMP
u8 Cpu6502::op_cmp_imm()  { auto r = immediate();        compare(a_, r.value); return 0; }
u8 Cpu6502::op_cmp_zp()   { auto r = zero_page();        compare(a_, r.value); return 0; }
u8 Cpu6502::op_cmp_zpx()  { auto r = zero_page_x();      compare(a_, r.value); return 0; }
u8 Cpu6502::op_cmp_abs()  { auto r = absolute();         compare(a_, r.value); return 0; }
u8 Cpu6502::op_cmp_absx() { auto r = absolute_x();       compare(a_, r.value); return r.page_crossed; }
u8 Cpu6502::op_cmp_absy() { auto r = absolute_y();       compare(a_, r.value); return r.page_crossed; }
u8 Cpu6502::op_cmp_indx() { auto r = indexed_indirect(); compare(a_, r.value); return 0; }
u8 Cpu6502::op_cmp_indy() { auto r = indirect_indexed(); compare(a_, r.value); return r.page_crossed; }

// Register transfers
u8 Cpu6502::op_tax() { x_ = a_; update_zn(x_); return 0; }
u8 Cpu6502::op_tay() { y_ = a_; update_zn(y_); return 0; }
u8 Cpu6502::op_txa() { a_ = x_; update_zn(a_); return 0; }
u8 Cpu6502::op_tya() { a_ = y_; update_zn(a_); return 0; }

// Stack
u8 Cpu6502::op_pha() { push(a_); return 0; }
u8 Cpu6502::op_pla() { a_ = pop(); update_zn(a_); return 0; }

u8 Cpu6502::op_php() {
    // When pushing status, Break + Unused bits are set
    push(status_ | static_cast<u8>(StatusFlag::Break) | kUnusedFlag);
    return 0;
}

u8 Cpu6502::op_plp() {
    status_ = pop();
    status_ |= kUnusedFlag;  // bit 5 is always set on 6502
    return 0;
}

// Control flow
u8 Cpu6502::op_jmp_abs() { pc_ = fetch16(); return 0; }
u8 Cpu6502::op_jmp_ind() { pc_ = indirect().addr; return 0; }

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

// Flag ops
u8 Cpu6502::op_clc() { set_flag(StatusFlag::Carry, false);             return 0; }
u8 Cpu6502::op_sec() { set_flag(StatusFlag::Carry, true);              return 0; }
u8 Cpu6502::op_cli() { set_flag(StatusFlag::InterruptDisable, false);  return 0; }
u8 Cpu6502::op_sei() { set_flag(StatusFlag::InterruptDisable, true);   return 0; }
u8 Cpu6502::op_clv() { set_flag(StatusFlag::Overflow, false);          return 0; }
u8 Cpu6502::op_cld() { set_flag(StatusFlag::Decimal, false);           return 0; }
u8 Cpu6502::op_sed() { set_flag(StatusFlag::Decimal, true);            return 0; }

// ---------------------------------------------------------------------------
// Instruction table
// ---------------------------------------------------------------------------

std::array<Instruction, 256> Cpu6502::build_table() {
    std::array<Instruction, 256> t{};
    for (auto& inst : t) {
        inst = {"NOP", 2, &Cpu6502::op_nop};
    }

    // LDA
    t[0xA9] = {"LDA IMM",    2, &Cpu6502::op_lda_imm};
    t[0xA5] = {"LDA ZP",     3, &Cpu6502::op_lda_zp};
    t[0xB5] = {"LDA ZP,X",   4, &Cpu6502::op_lda_zpx};
    t[0xAD] = {"LDA ABS",    4, &Cpu6502::op_lda_abs};
    t[0xBD] = {"LDA ABS,X",  4, &Cpu6502::op_lda_absx};
    t[0xB9] = {"LDA ABS,Y",  4, &Cpu6502::op_lda_absy};
    t[0xA1] = {"LDA (ZP,X)", 6, &Cpu6502::op_lda_indx};
    t[0xB1] = {"LDA (ZP),Y", 5, &Cpu6502::op_lda_indy};

    // STA
    t[0x85] = {"STA ZP",     3, &Cpu6502::op_sta_zp};
    t[0x95] = {"STA ZP,X",   4, &Cpu6502::op_sta_zpx};
    t[0x8D] = {"STA ABS",    4, &Cpu6502::op_sta_abs};
    t[0x9D] = {"STA ABS,X",  5, &Cpu6502::op_sta_absx};
    t[0x99] = {"STA ABS,Y",  5, &Cpu6502::op_sta_absy};
    t[0x81] = {"STA (ZP,X)", 6, &Cpu6502::op_sta_indx};
    t[0x91] = {"STA (ZP),Y", 6, &Cpu6502::op_sta_indy};

    // LDX
    t[0xA2] = {"LDX IMM",   2, &Cpu6502::op_ldx_imm};
    t[0xA6] = {"LDX ZP",    3, &Cpu6502::op_ldx_zp};
    t[0xB6] = {"LDX ZP,Y",  4, &Cpu6502::op_ldx_zpy};
    t[0xAE] = {"LDX ABS",   4, &Cpu6502::op_ldx_abs};
    t[0xBE] = {"LDX ABS,Y", 4, &Cpu6502::op_ldx_absy};

    // LDY
    t[0xA0] = {"LDY IMM",   2, &Cpu6502::op_ldy_imm};
    t[0xA4] = {"LDY ZP",    3, &Cpu6502::op_ldy_zp};
    t[0xB4] = {"LDY ZP,X",  4, &Cpu6502::op_ldy_zpx};
    t[0xAC] = {"LDY ABS",   4, &Cpu6502::op_ldy_abs};
    t[0xBC] = {"LDY ABS,X", 4, &Cpu6502::op_ldy_absx};

    // STX
    t[0x86] = {"STX ZP",    3, &Cpu6502::op_stx_zp};
    t[0x8E] = {"STX ABS",   4, &Cpu6502::op_stx_abs};
    t[0x96] = {"STX ZP,Y",  4, &Cpu6502::op_stx_zpy};

    // STY
    t[0x84] = {"STY ZP",    3, &Cpu6502::op_sty_zp};
    t[0x8C] = {"STY ABS",   4, &Cpu6502::op_sty_abs};
    t[0x94] = {"STY ZP,X",  4, &Cpu6502::op_sty_zpx};

    // ADC
    t[0x69] = {"ADC IMM",    2, &Cpu6502::op_adc_imm};
    t[0x65] = {"ADC ZP",     3, &Cpu6502::op_adc_zp};
    t[0x75] = {"ADC ZP,X",   4, &Cpu6502::op_adc_zpx};
    t[0x6D] = {"ADC ABS",    4, &Cpu6502::op_adc_abs};
    t[0x7D] = {"ADC ABS,X",  4, &Cpu6502::op_adc_absx};
    t[0x79] = {"ADC ABS,Y",  4, &Cpu6502::op_adc_absy};
    t[0x61] = {"ADC (ZP,X)", 6, &Cpu6502::op_adc_indx};
    t[0x71] = {"ADC (ZP),Y", 5, &Cpu6502::op_adc_indy};

    // SBC
    t[0xE9] = {"SBC IMM",    2, &Cpu6502::op_sbc_imm};
    t[0xE5] = {"SBC ZP",     3, &Cpu6502::op_sbc_zp};
    t[0xF5] = {"SBC ZP,X",   4, &Cpu6502::op_sbc_zpx};
    t[0xED] = {"SBC ABS",    4, &Cpu6502::op_sbc_abs};
    t[0xFD] = {"SBC ABS,X",  4, &Cpu6502::op_sbc_absx};
    t[0xF9] = {"SBC ABS,Y",  4, &Cpu6502::op_sbc_absy};
    t[0xE1] = {"SBC (ZP,X)", 6, &Cpu6502::op_sbc_indx};
    t[0xF1] = {"SBC (ZP),Y", 5, &Cpu6502::op_sbc_indy};

    // AND
    t[0x29] = {"AND IMM",    2, &Cpu6502::op_and_imm};
    t[0x25] = {"AND ZP",     3, &Cpu6502::op_and_zp};
    t[0x35] = {"AND ZP,X",   4, &Cpu6502::op_and_zpx};
    t[0x2D] = {"AND ABS",    4, &Cpu6502::op_and_abs};
    t[0x3D] = {"AND ABS,X",  4, &Cpu6502::op_and_absx};
    t[0x39] = {"AND ABS,Y",  4, &Cpu6502::op_and_absy};
    t[0x21] = {"AND (ZP,X)", 6, &Cpu6502::op_and_indx};
    t[0x31] = {"AND (ZP),Y", 5, &Cpu6502::op_and_indy};

    // ORA
    t[0x09] = {"ORA IMM",    2, &Cpu6502::op_ora_imm};
    t[0x05] = {"ORA ZP",     3, &Cpu6502::op_ora_zp};
    t[0x15] = {"ORA ZP,X",   4, &Cpu6502::op_ora_zpx};
    t[0x0D] = {"ORA ABS",    4, &Cpu6502::op_ora_abs};
    t[0x1D] = {"ORA ABS,X",  4, &Cpu6502::op_ora_absx};
    t[0x19] = {"ORA ABS,Y",  4, &Cpu6502::op_ora_absy};
    t[0x01] = {"ORA (ZP,X)", 6, &Cpu6502::op_ora_indx};
    t[0x11] = {"ORA (ZP),Y", 5, &Cpu6502::op_ora_indy};

    // EOR
    t[0x49] = {"EOR IMM",    2, &Cpu6502::op_eor_imm};
    t[0x45] = {"EOR ZP",     3, &Cpu6502::op_eor_zp};
    t[0x55] = {"EOR ZP,X",   4, &Cpu6502::op_eor_zpx};
    t[0x4D] = {"EOR ABS",    4, &Cpu6502::op_eor_abs};
    t[0x5D] = {"EOR ABS,X",  4, &Cpu6502::op_eor_absx};
    t[0x59] = {"EOR ABS,Y",  4, &Cpu6502::op_eor_absy};
    t[0x41] = {"EOR (ZP,X)", 6, &Cpu6502::op_eor_indx};
    t[0x51] = {"EOR (ZP),Y", 5, &Cpu6502::op_eor_indy};

    // CMP
    t[0xC9] = {"CMP IMM",    2, &Cpu6502::op_cmp_imm};
    t[0xC5] = {"CMP ZP",     3, &Cpu6502::op_cmp_zp};
    t[0xD5] = {"CMP ZP,X",   4, &Cpu6502::op_cmp_zpx};
    t[0xCD] = {"CMP ABS",    4, &Cpu6502::op_cmp_abs};
    t[0xDD] = {"CMP ABS,X",  4, &Cpu6502::op_cmp_absx};
    t[0xD9] = {"CMP ABS,Y",  4, &Cpu6502::op_cmp_absy};
    t[0xC1] = {"CMP (ZP,X)", 6, &Cpu6502::op_cmp_indx};
    t[0xD1] = {"CMP (ZP),Y", 5, &Cpu6502::op_cmp_indy};

    // Transfers
    t[0xAA] = {"TAX", 2, &Cpu6502::op_tax};
    t[0xA8] = {"TAY", 2, &Cpu6502::op_tay};
    t[0x8A] = {"TXA", 2, &Cpu6502::op_txa};
    t[0x98] = {"TYA", 2, &Cpu6502::op_tya};
    t[0xBA] = {"TSX", 2, &Cpu6502::op_tsx};
    t[0x9A] = {"TXS", 2, &Cpu6502::op_txs};

    // Stack
    t[0x48] = {"PHA", 3, &Cpu6502::op_pha};
    t[0x68] = {"PLA", 4, &Cpu6502::op_pla};
    t[0x08] = {"PHP", 3, &Cpu6502::op_php};
    t[0x28] = {"PLP", 4, &Cpu6502::op_plp};

    // Control flow
    t[0x4C] = {"JMP ABS", 3, &Cpu6502::op_jmp_abs};
    t[0x6C] = {"JMP IND", 5, &Cpu6502::op_jmp_ind};
    t[0x20] = {"JSR",     6, &Cpu6502::op_jsr};
    t[0x60] = {"RTS",     6, &Cpu6502::op_rts};

    // Branches
    t[0xF0] = {"BEQ", 2, &Cpu6502::op_beq};
    t[0xD0] = {"BNE", 2, &Cpu6502::op_bne};
    t[0xB0] = {"BCS", 2, &Cpu6502::op_bcs};
    t[0x90] = {"BCC", 2, &Cpu6502::op_bcc};

    // Remaining branches
    t[0x30] = {"BMI", 2, &Cpu6502::op_bmi};
    t[0x10] = {"BPL", 2, &Cpu6502::op_bpl};
    t[0x70] = {"BVS", 2, &Cpu6502::op_bvs};
    t[0x50] = {"BVC", 2, &Cpu6502::op_bvc};

    // ASL
    t[0x0A] = {"ASL ACC",   2, &Cpu6502::op_asl_acc};
    t[0x06] = {"ASL ZP",    5, &Cpu6502::op_asl_zp};
    t[0x16] = {"ASL ZP,X",  6, &Cpu6502::op_asl_zpx};
    t[0x0E] = {"ASL ABS",   6, &Cpu6502::op_asl_abs};
    t[0x1E] = {"ASL ABS,X", 7, &Cpu6502::op_asl_absx};

    // LSR
    t[0x4A] = {"LSR ACC",   2, &Cpu6502::op_lsr_acc};
    t[0x46] = {"LSR ZP",    5, &Cpu6502::op_lsr_zp};
    t[0x56] = {"LSR ZP,X",  6, &Cpu6502::op_lsr_zpx};
    t[0x4E] = {"LSR ABS",   6, &Cpu6502::op_lsr_abs};
    t[0x5E] = {"LSR ABS,X", 7, &Cpu6502::op_lsr_absx};

    // ROL
    t[0x2A] = {"ROL ACC",   2, &Cpu6502::op_rol_acc};
    t[0x26] = {"ROL ZP",    5, &Cpu6502::op_rol_zp};
    t[0x36] = {"ROL ZP,X",  6, &Cpu6502::op_rol_zpx};
    t[0x2E] = {"ROL ABS",   6, &Cpu6502::op_rol_abs};
    t[0x3E] = {"ROL ABS,X", 7, &Cpu6502::op_rol_absx};

    // ROR
    t[0x6A] = {"ROR ACC",   2, &Cpu6502::op_ror_acc};
    t[0x66] = {"ROR ZP",    5, &Cpu6502::op_ror_zp};
    t[0x76] = {"ROR ZP,X",  6, &Cpu6502::op_ror_zpx};
    t[0x6E] = {"ROR ABS",   6, &Cpu6502::op_ror_abs};
    t[0x7E] = {"ROR ABS,X", 7, &Cpu6502::op_ror_absx};

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

    // CPX
    t[0xE0] = {"CPX IMM", 2, &Cpu6502::op_cpx_imm};
    t[0xE4] = {"CPX ZP",  3, &Cpu6502::op_cpx_zp};
    t[0xEC] = {"CPX ABS", 4, &Cpu6502::op_cpx_abs};

    // CPY
    t[0xC0] = {"CPY IMM", 2, &Cpu6502::op_cpy_imm};
    t[0xC4] = {"CPY ZP",  3, &Cpu6502::op_cpy_zp};
    t[0xCC] = {"CPY ABS", 4, &Cpu6502::op_cpy_abs};

    // Register INC/DEC
    t[0xE8] = {"INX", 2, &Cpu6502::op_inx};
    t[0xC8] = {"INY", 2, &Cpu6502::op_iny};
    t[0xCA] = {"DEX", 2, &Cpu6502::op_dex};
    t[0x88] = {"DEY", 2, &Cpu6502::op_dey};

    // Flag ops
    t[0x18] = {"CLC", 2, &Cpu6502::op_clc};
    t[0x38] = {"SEC", 2, &Cpu6502::op_sec};
    t[0x58] = {"CLI", 2, &Cpu6502::op_cli};
    t[0x78] = {"SEI", 2, &Cpu6502::op_sei};
    t[0xB8] = {"CLV", 2, &Cpu6502::op_clv};
    t[0xD8] = {"CLD", 2, &Cpu6502::op_cld};
    t[0xF8] = {"SED", 2, &Cpu6502::op_sed};

    return t;
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
