#include "bus.h"

#include <utility>

Bus::Bus() {
    reset();
}

u8 Bus::read(u16 addr) const {
    if (cartridge_) {
        if (addr >= 0x8000) {
            return cartridge_->cpu_read(addr);
        }
        if (addr < 0x2000) {
            addr &= 0x07FF;
        }
    }
    return ram_[addr];
}

void Bus::write(u16 addr, u8 data) {
    if (cartridge_) {
        if (addr >= 0x8000) {
            return; // NROM PRG ROM cannot be written by the CPU.
        }
        if (addr < 0x2000) {
            addr &= 0x07FF;
        }
    }
    ram_[addr] = data;
}

void Bus::load(const std::vector<u8>& data, u16 start_addr) {
    u32 end = static_cast<u32>(start_addr) + static_cast<u32>(data.size());
    if (end > ram_.size()) {
        end = static_cast<u32>(ram_.size());
    }

    for (u32 i = start_addr; i < end; ++i) {
        write(static_cast<u16>(i), data[i - start_addr]);
    }
}

void Bus::insert_cartridge(Cartridge cartridge) {
    cartridge_ = std::move(cartridge);
    reset();
}

void Bus::reset() {
    ram_.fill(0);
}

