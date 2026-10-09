#include "validation.h"

#include <array>
#include <charconv>
#include <cctype>
#include <iomanip>
#include <istream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "bus.h"
#include "cpu.h"

namespace {
struct State {
    u64 pc, a, x, y, p, sp, cycles;
    bool operator==(const State&) const = default;
};

std::string format_state(const State& state) {
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0')
        << "PC:" << std::setw(4) << state.pc
        << " A:" << std::setw(2) << state.a
        << " X:" << std::setw(2) << state.x
        << " Y:" << std::setw(2) << state.y
        << " P:" << std::setw(2) << state.p
        << " SP:" << std::setw(2) << state.sp
        << " CYC:" << std::dec << state.cycles;
    return out.str();
}

[[noreturn]] void fail(std::size_t line, const std::string& reason,
                       const std::string& expected, const State& actual) {
    throw std::runtime_error("Line " + std::to_string(line) + ": " + reason +
                             "\nExpected: " + expected +
                             "\nActual:   " + format_state(actual));
}

u64 parse_number(std::string_view token, int base, u64 maximum,
                 std::string_view field) {
    u64 value = 0;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value, base);
    if (token.empty() || result.ec != std::errc{} ||
        result.ptr != token.data() + token.size() || value > maximum) {
        throw std::runtime_error("invalid or out-of-range " + std::string(field));
    }
    return value;
}

State parse_record(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> tokens;
    for (std::string token; input >> token;) {
        tokens.push_back(token);
    }
    if (tokens.empty() || std::isspace(static_cast<unsigned char>(line.front()))) {
        throw std::runtime_error("missing PC at beginning of record");
    }
    State state{};
    state.pc = parse_number(tokens.front(), 16, 0xFFFF, "PC");
    constexpr std::array<std::string_view, 6> labels{"A:", "X:", "Y:", "P:", "SP:", "CYC:"};
    std::array<bool, 6> seen{};
    std::array<u64*, 6> values{&state.a, &state.x, &state.y, &state.p, &state.sp, &state.cycles};
    for (std::size_t i = 1; i < tokens.size(); ++i) {
        for (std::size_t field = 0; field < labels.size(); ++field) {
            if (!std::string_view(tokens[i]).starts_with(labels[field])) {
                continue;
            }
            if (seen[field]) {
                throw std::runtime_error("duplicate " + std::string(labels[field]));
            }
            seen[field] = true;
            std::string_view number = std::string_view(tokens[i]).substr(labels[field].size());
            if (number.empty() && i + 1 < tokens.size()) {
                number = tokens[++i];
            }
            *values[field] = parse_number(number, field == 5 ? 10 : 16,
                                         field == 5 ? std::numeric_limits<u64>::max() : 0xFF,
                                         labels[field]);
            break;
        }
    }
    for (std::size_t field = 0; field < labels.size(); ++field) {
        if (!seen[field]) {
            throw std::runtime_error("missing " + std::string(labels[field]));
        }
    }
    return state;
}
}

std::size_t validate_trace(Cpu6502& cpu, const Bus& bus, std::istream& log,
                           std::size_t max_instructions) {
    std::size_t matched = 0;
    while (max_instructions == 0 || matched < max_instructions) {
        State actual{cpu.pc(), cpu.a(), cpu.x(), cpu.y(), cpu.status(), cpu.sp(), cpu.cycles()};
        if (actual.cycles > std::numeric_limits<u64>::max() - 7) {
            fail(matched + 1, "cycle offset overflow", "representable CYC", actual);
        }
        actual.cycles += 7;
        std::string line;
        try {
            std::getline(log, line);
        } catch (...) {
            // An exception-enabled stream can throw at ordinary EOF, including
            // after extracting the final record without a newline.
            if (log.bad() || !log.eof()) {
                fail(matched + 1, "stream read error", "readable record", actual);
            }
        }
        if (log.bad() || (log.fail() && !log.eof())) {
            fail(matched + 1, "stream read error", "readable record", actual);
        }
        if (log.eof() && line.empty()) {
            if (matched == 0) {
                fail(1, "empty log", "at least one complete state record", actual);
            }
            return matched;
        }
        State expected{};
        try {
            expected = parse_record(line);
        } catch (const std::runtime_error& error) {
            fail(matched + 1, "malformed record: " + std::string(error.what()),
                 "complete PC/A/X/Y/P/SP/CYC state; record: " + line, actual);
        }
        if (!(expected == actual)) {
            fail(matched + 1, "state mismatch", format_state(expected), actual);
        }
        const u8 opcode = bus.read(cpu.pc());
        if (!Cpu6502::supports_opcode(opcode)) {
            std::ostringstream reason;
            reason << "unsupported opcode $" << std::uppercase << std::hex
                   << std::setfill('0') << std::setw(2) << static_cast<unsigned>(opcode);
            fail(matched + 1, reason.str(), format_state(expected), actual);
        }
        cpu.step();
        ++matched;
    }
    return matched;
}
