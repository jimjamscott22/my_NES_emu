#include <charconv>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "bus.h"
#include "cpu.h"
#include "validation.h"

int main(int argc, char* argv[]) {
    if (argc == 1) {
        std::cout << "NES CPU core ready. Validate a local ROM with:\n"
                  << "  nes_emulator --nestest ROM.nes reference.log [instruction-limit]\n";
        return 0;
    }
    if ((argc != 4 && argc != 5) || std::string_view(argv[1]) != "--nestest") {
        std::cerr << "Usage: nes_emulator --nestest ROM.nes reference.log [instruction-limit]\n";
        return 2;
    }
    try {
        std::size_t limit = 0;
        if (argc == 5) {
            const std::string_view text(argv[4]);
            const auto result = std::from_chars(text.data(), text.data() + text.size(), limit);
            if (text.empty() || result.ec != std::errc{} ||
                result.ptr != text.data() + text.size() || limit == 0) {
                throw std::runtime_error("Instruction limit must be a positive integer");
            }
        }
        Bus bus;
        bus.insert_cartridge(Cartridge::load(argv[2]));
        std::ifstream log(argv[3]);
        if (!log) {
            throw std::runtime_error("Cannot open reference log: " + std::string(argv[3]));
        }
        Cpu6502 cpu(bus);
        cpu.reset(0xC000);
        const auto matched = validate_trace(cpu, bus, log, limit);
        std::cout << "Matched " << matched << " instruction(s)"
                  << (limit != 0 && matched == limit ? " (requested limit reached)" : " (end of supplied log)")
                  << ". This validates only the supplied trace, not the complete NES.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Validation failed: " << error.what() << '\n';
        return 1;
    }
}
