#include "cartridge.h"

#include <string>
#include <fstream>
#include <stdexcept>

Cartridge Cartridge::from_ines(const std::vector<u8>& data) {
    if (data.size() < 16 || data[0] != 'N' || data[1] != 'E' ||
        data[2] != 'S' || data[3] != 0x1A) {
        throw std::runtime_error("Invalid or truncated iNES header");
    }
    if ((data[7] & 0x0C) != 0 || (data[7] & 0x03) != 0) {
        throw std::runtime_error("Only standard iNES NES images are supported (not NES 2.0/VS/PlayChoice)");
    }
    const unsigned mapper = (data[6] >> 4) | (data[7] & 0xF0);
    if (mapper != 0) {
        throw std::runtime_error("Unsupported mapper: " + std::to_string(mapper));
    }
    if ((data[4] != 1 && data[4] != 2) || data[5] > 1 || data[8] > 1) {
        throw std::runtime_error("Invalid NROM PRG/CHR/RAM size");
    }
    // Trainers require board-specific initialization; reject rather than ignore them.
    if ((data[6] & 0x04) != 0) {
        throw std::runtime_error("Trainer-equipped ROMs are not supported");
    }
    const size_t prg_size = static_cast<size_t>(data[4]) * 0x4000;
    const size_t chr_size = static_cast<size_t>(data[5]) * 0x2000;
    if (data.size() < 16 + prg_size + chr_size) {
        throw std::runtime_error("Truncated iNES PRG/CHR data");
    }
    Cartridge cartridge;
    cartridge.prg_.assign(data.begin() + 16, data.begin() + 16 + prg_size);
    if (chr_size == 0) {
        cartridge.chr_.resize(0x2000, 0); // NROM with CHR RAM
    } else {
        cartridge.chr_.assign(data.begin() + 16 + prg_size,
                              data.begin() + 16 + prg_size + chr_size);
    }
    return cartridge;
}

Cartridge Cartridge::load(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Cannot open ROM: " + path.string());
    }
    const auto size = file.tellg();
    // Bound allocation before reading an untrusted file. NROM needs at most 40 KiB + header.
    if (size < 0 || size > 16 + 0x8000 + 0x2000) {
        throw std::runtime_error("Invalid NROM file size");
    }
    std::vector<u8> data(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
        throw std::runtime_error("Failed to read ROM: " + path.string());
    }
    return from_ines(data);
}

u8 Cartridge::cpu_read(u16 addr) const {
    if (addr < 0x8000 || prg_.empty()) {
        throw std::out_of_range("NROM PRG read outside mapped ROM");
    }
    return prg_[(addr - 0x8000) % prg_.size()];
}
