#pragma once

#include <array>
#include <vector>
#include <optional>

#include "cartridge.h"

#include "types.h"

// Flat memory for CPU unit tests; inserting a cartridge enables NES RAM mirroring
// and mapper-0 PRG ROM. PPU/APU registers remain placeholders.
class Bus {
public:
    Bus();

    u8 read(u16 addr) const;
    void write(u16 addr, u8 data);

    // Convenience helper to load a binary blob (e.g., test program)
    // into memory at the given start address.
    void load(const std::vector<u8>& data, u16 start_addr = 0x8000);

    void reset();
    void insert_cartridge(Cartridge cartridge);

private:
    std::array<u8, 0x10000> ram_;
    std::optional<Cartridge> cartridge_;
};

