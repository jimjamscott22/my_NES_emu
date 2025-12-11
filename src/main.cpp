#include <iomanip>
#include <iostream>

#include "bus.h"
#include "cpu.h"

int main() {
    Bus bus;
    Cpu6502 cpu(bus);

    // For now we only bring up the CPU core. Later we will load a ROM,
    // wire up the PPU/APU, and clock everything together.
    cpu.reset(0xC000);

    std::cout << "NES emulator skeleton ready. PC=0x" << std::hex
              << std::uppercase << cpu.pc() << std::dec << "\n";
    return 0;
}

