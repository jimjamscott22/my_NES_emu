#pragma once

#include <filesystem>
#include <vector>

#include "types.h"

// CPU-side mapper 0 (NROM). CHR data is retained for a future PPU.
class Cartridge {
public:
    static Cartridge from_ines(const std::vector<u8>& data);
    static Cartridge load(const std::filesystem::path& path);

    u8 cpu_read(u16 addr) const;
    const std::vector<u8>& chr() const { return chr_; }

private:
    std::vector<u8> prg_;
    std::vector<u8> chr_;
};
