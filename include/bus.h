#pragma once

#include <array>
#include <vector>

#include "types.h"

// Simple 64KB memory bus placeholder.
// Mappers and memory-mapped devices can be hooked in later.
class Bus {
public:
    Bus();

    u8 read(u16 addr) const;
    void write(u16 addr, u8 data);

    // Convenience helper to load a binary blob (e.g., test program)
    // into memory at the given start address.
    void load(const std::vector<u8>& data, u16 start_addr = 0x8000);

    void reset();

private:
    std::array<u8, 0x10000> ram_;
};

