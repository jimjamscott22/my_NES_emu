#include "bus.h"

Bus::Bus() {
    reset();
}

u8 Bus::read(u16 addr) const {
    return ram_[addr];
}

void Bus::write(u16 addr, u8 data) {
    ram_[addr] = data;
}

void Bus::load(const std::vector<u8>& data, u16 start_addr) {
    u32 end = static_cast<u32>(start_addr) + static_cast<u32>(data.size());
    if (end > ram_.size()) {
        end = static_cast<u32>(ram_.size());
    }

    for (u32 i = start_addr; i < end; ++i) {
        ram_[i] = data[i - start_addr];
    }
}

void Bus::reset() {
    ram_.fill(0);
}

