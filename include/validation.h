#pragma once

#include <cstddef>
#include <iosfwd>

class Cpu6502;
class Bus;

// Compare nestest pre-instruction records against the current CPU state, then
// execute each matched, supported instruction. The caller loads memory and
// resets the CPU; canonical nestest cycles are cpu.cycles() + 7.
// Zero means no instruction limit. Blank records are malformed, not skipped.
// Throws std::runtime_error on invalid input, I/O failure, state mismatch, or
// unsupported opcode. Earlier matched instructions remain executed on failure.
std::size_t validate_trace(Cpu6502& cpu, const Bus& bus, std::istream& log,
                           std::size_t max_instructions = 0);
