#include "cartridge.h"
#include "bus.h"
#include "cpu.h"
#include "test_framework.h"

#include <stdexcept>

namespace {
std::vector<u8> make_rom(u8 banks = 1, u8 chr_banks = 0) {
    std::vector<u8> data(16 + static_cast<size_t>(banks) * 0x4000 +
                        static_cast<size_t>(chr_banks) * 0x2000, 0);
    data[0] = 'N'; data[1] = 'E'; data[2] = 'S'; data[3] = 0x1A;
    data[4] = banks; data[5] = chr_banks;
    return data;
}

bool rejects(const std::vector<u8>& data) {
    try {
        Cartridge::from_ines(data);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}
}

TEST_CASE(test_nrom_16k_prg_mirrors_and_allocates_chr_ram) {
    auto data = make_rom();
    data[16] = 0xA9;
    data.back() = 0x42;
    const auto cart = Cartridge::from_ines(data);
    REQUIRE(cart.cpu_read(0x8000) == 0xA9);
    REQUIRE(cart.cpu_read(0xC000) == 0xA9);
    REQUIRE(cart.cpu_read(0xBFFF) == 0x42);
    REQUIRE(cart.cpu_read(0xFFFF) == 0x42);
    REQUIRE(cart.chr().size() == 0x2000);
    REQUIRE(cart.chr()[0] == 0);
}

TEST_CASE(test_nrom_32k_prg_and_chr_rom) {
    auto data = make_rom(2, 1);
    data[16] = 0x11;
    data[16 + 0x4000] = 0x22;
    data[16 + 0x8000] = 0x33;
    const auto cart = Cartridge::from_ines(data);
    REQUIRE(cart.cpu_read(0x8000) == 0x11);
    REQUIRE(cart.cpu_read(0xC000) == 0x22);
    REQUIRE(cart.chr()[0] == 0x33);
}

TEST_CASE(test_ines_rejects_invalid_and_truncated_images) {
    REQUIRE(rejects({}));
    REQUIRE(rejects(std::vector<u8>(15, 0)));
    auto data = make_rom();
    data[0] = 'X';
    REQUIRE(rejects(data));
    data = make_rom(); data.pop_back();
    REQUIRE(rejects(data));
    data = make_rom(1, 1); data.pop_back();
    REQUIRE(rejects(data));
}

TEST_CASE(test_ines_rejects_unsupported_headers_and_sizes) {
    auto data = make_rom(); data[6] = 0x10;
    REQUIRE(rejects(data));
    data = make_rom(); data[7] = 0x10;
    REQUIRE(rejects(data));
    data = make_rom(); data[7] = 0x08;
    REQUIRE(rejects(data));
    data = make_rom(); data[7] = 0x01;
    REQUIRE(rejects(data));
    data = make_rom(); data[6] = 0x04;
    REQUIRE(rejects(data));
    data = make_rom(); data[4] = 0;
    REQUIRE(rejects(data));
    data = make_rom(3);
    REQUIRE(rejects(data));
    data = make_rom(1, 2);
    REQUIRE(rejects(data));
    data = make_rom(); data[8] = 2;
    REQUIRE(rejects(data));
}

TEST_CASE(test_cartridge_bus_rom_protection_and_ram_mirroring) {
    auto data = make_rom(); data[16] = 0xEA;
    Bus bus;
    bus.insert_cartridge(Cartridge::from_ines(data));
    bus.write(0x8000, 0);
    bus.load({0}, 0xC000);
    REQUIRE(bus.read(0x8000) == 0xEA);
    REQUIRE(bus.read(0xC000) == 0xEA);
    bus.write(0x1805, 0x42);
    REQUIRE(bus.read(0x0005) == 0x42);
    REQUIRE(bus.read(0x0805) == 0x42);
    REQUIRE(bus.read(0x1005) == 0x42);
    bus.write(0x6000, 0x55);
    REQUIRE(bus.read(0x6000) == 0x55);
    bus.reset();
    REQUIRE(bus.read(0x0005) == 0);
    REQUIRE(bus.read(0x6000) == 0);
    REQUIRE(bus.read(0xC000) == 0xEA);
}

TEST_CASE(test_cartridge_cpu_executes_mirrored_prg) {
    auto data = make_rom();
    data[16] = 0xA9; data[17] = 0x80;
    Bus bus;
    bus.insert_cartridge(Cartridge::from_ines(data));
    Cpu6502 cpu(bus);
    cpu.reset(0xC000);
    cpu.step();
    REQUIRE(cpu.a() == 0x80);
    REQUIRE(cpu.flag(StatusFlag::Negative));
    REQUIRE(cpu.pc() == 0xC002);
    REQUIRE(cpu.cycles() == 2);
}

TEST_CASE(test_flat_bus_stays_writable_and_unmirrored_without_cartridge) {
    Bus bus;
    bus.write(0x0005, 1);
    bus.write(0x0805, 2);
    bus.load({0xA9, 0x42}, 0xC000);
    REQUIRE(bus.read(0x0005) == 1);
    REQUIRE(bus.read(0x0805) == 2);
    REQUIRE(bus.read(0xC000) == 0xA9);
}

TEST_CASE(test_supported_opcode_query_distinguishes_official_nop) {
    REQUIRE(Cpu6502::supports_opcode(0xEA));
    REQUIRE(Cpu6502::supports_opcode(0xA9));
    REQUIRE(Cpu6502::supports_opcode(0x00));
    REQUIRE(!Cpu6502::supports_opcode(0x02));
    REQUIRE(!Cpu6502::supports_opcode(0x1A));
}
