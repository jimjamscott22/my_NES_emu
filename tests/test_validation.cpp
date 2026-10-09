#include "validation.h"

#include <array>
#include <ios>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>

#include "bus.h"
#include "cpu.h"
#include "test_framework.h"

namespace {
const std::string reset_row =
    "C000  EA        NOP                             A:00 X:00 Y:00 P:24 SP:FD PPU:  0, 21 CYC:7";
const std::string second_row =
    "C001  EA        NOP                             A:00 X:00 Y:00 P:24 SP:FD CYC:9 PPU:  0, 27";

std::string expect_failure(Cpu6502& cpu, const Bus& bus, std::istream& log,
                           std::size_t limit = 0) {
    try {
        validate_trace(cpu, bus, log, limit);
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    REQUIRE(false);
    return {};
}

void require_full_states(const std::string& message) {
    const auto expected = message.find("Expected:");
    const auto actual = message.find("Actual:");
    REQUIRE(expected != std::string::npos);
    REQUIRE(actual != std::string::npos);
    for (const auto* field : {"PC:", "A:", "X:", "Y:", "P:", "SP:", "CYC:"}) {
        REQUIRE(message.substr(expected, actual - expected).find(field) != std::string::npos);
        REQUIRE(message.substr(actual).find(field) != std::string::npos);
    }
}

class BrokenBuffer : public std::streambuf {
protected:
    int_type underflow() override {
        throw std::runtime_error("synthetic read failure");
    }
};
}

TEST_CASE(test_validation_matches_pre_instruction_states_and_steps_final_record) {
    Bus bus;
    Cpu6502 cpu(bus);

    bus.load({0xA9, 0x80, 0xAA, 0xE8}, 0xC000);
    cpu.reset(0xC000);
    std::istringstream log(
        "C000 A9 80 LDA #$80 A:00 X:00 Y:00 P:24 SP:FD PPU: 0,21 CYC:7\n"
        "C002 AA TAX A:80 X:00 Y:00 P:A4 SP:FD CYC:9\n"
        "C003 E8 INX A:80 X:80 Y:00 P:A4 SP:FD CYC:11");
    REQUIRE(validate_trace(cpu, bus, log) == 3);
    REQUIRE(cpu.pc() == 0xC004);
    REQUIRE(cpu.a() == 0x80);
    REQUIRE(cpu.x() == 0x81);
    REQUIRE(cpu.cycles() == 6);
}

TEST_CASE(test_validation_reports_every_state_field_mismatch_without_stepping) {
    const std::array<std::string, 7> wrong_rows{
        "C001 A:00 X:00 Y:00 P:24 SP:FD CYC:7",
        "C000 A:01 X:00 Y:00 P:24 SP:FD CYC:7",
        "C000 A:00 X:01 Y:00 P:24 SP:FD CYC:7",
        "C000 A:00 X:00 Y:01 P:24 SP:FD CYC:7",
        "C000 A:00 X:00 Y:00 P:25 SP:FD CYC:7",
        "C000 A:00 X:00 Y:00 P:24 SP:FC CYC:7",
        "C000 A:00 X:00 Y:00 P:24 SP:FD CYC:0"
    };
    for (const auto& row : wrong_rows) {
        Bus bus;
        Cpu6502 cpu(bus);
        bus.write(0xC000, 0xEA);
        cpu.reset(0xC000);
        std::istringstream log(row);
        const auto message = expect_failure(cpu, bus, log);
        REQUIRE(message.find("Line 1: state mismatch") != std::string::npos);
        require_full_states(message);
        REQUIRE(cpu.pc() == 0xC000);
        REQUIRE(cpu.cycles() == 0);
    }
}

TEST_CASE(test_validation_reports_later_cycle_mismatch) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.load({0xEA, 0xEA}, 0xC000);
    cpu.reset(0xC000);
    std::istringstream log(reset_row + "\nC001 A:00 X:00 Y:00 P:24 SP:FD CYC:10\n");
    const auto message = expect_failure(cpu, bus, log);
    REQUIRE(message.find("Line 2: state mismatch") != std::string::npos);
    REQUIRE(message.find("CYC:10") != std::string::npos);
    REQUIRE(message.find("CYC:9") != std::string::npos);
    require_full_states(message);
    REQUIRE(cpu.pc() == 0xC001);
    REQUIRE(cpu.cycles() == 2);
}

TEST_CASE(test_validation_rejects_malformed_and_out_of_range_fields) {
    const std::array<std::string, 17> rows{
        "\n", "   ", " C000 A:00 X:00 Y:00 P:24 SP:FD CYC:7",
        "10000 A:00 X:00 Y:00 P:24 SP:FD CYC:7",
        "C000junk A:00 X:00 Y:00 P:24 SP:FD CYC:7",
        "C000 A:100 X:00 Y:00 P:24 SP:FD CYC:7",
        "C000 A:GG X:00 Y:00 P:24 SP:FD CYC:7",
        "C000 A:00 X:00 Y:00 P:24 CYC:7",
        "C000 A:00 X:00 Y:00 P:24 SP:FD",
        "C000 A:00 X:00 Y:00 P:24 SP:FD CYC:",
        "C000 A:00 X:00 Y:00 P:24 SP:FD CYC:-7",
        "C000 A:00 X:00 Y:00 P:24 SP:FD CYC:+7",
        "C000 A:00 X:00 Y:00 P:24 SP:FD CYC:7junk",
        "C000 A:00 X:00 Y:00 P:24 SP:FD CYC:7.0",
        "C000 A:00 X:00 Y:00 P:24 SP:FD CYC:18446744073709551616",
        "C000 A:00 X:00 Y:00 P:24 SP:FD CYC:7 A:00",
        "C000 A:00 X:00 Y:00 P:24 SP:FD CYC:7 CYC:7"
    };
    for (const auto& row : rows) {
        Bus bus;
        Cpu6502 cpu(bus);
        cpu.reset(0xC000);
        std::istringstream log(row);
        const auto message = expect_failure(cpu, bus, log);
        REQUIRE(message.find("Line 1: malformed record") != std::string::npos);
        REQUIRE(message.find("Actual:") != std::string::npos);
        REQUIRE(cpu.cycles() == 0);
    }
}

TEST_CASE(test_validation_accepts_hex_case_and_whitespace_after_labels) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.write(0xC000, 0xEA);
    cpu.reset(0xC000);
    std::istringstream log("c000 EA NOP A: 00\tX:00 Y:00 P:24 SP:fd PPU: 0,21 CYC: 7\r\n");
    REQUIRE(validate_trace(cpu, bus, log) == 1);
    REQUIRE(cpu.cycles() == 2);
}

TEST_CASE(test_validation_rejects_empty_log_even_with_limit) {
    for (const auto limit : {0u, 1u}) {
        Bus bus;
        Cpu6502 cpu(bus);
        cpu.reset(0xC000);
        std::istringstream log;
        const auto message = expect_failure(cpu, bus, log, limit);
        REQUIRE(message.find("Line 1: empty log") != std::string::npos);
        REQUIRE(cpu.cycles() == 0);
    }
}

TEST_CASE(test_validation_limit_does_not_read_next_row) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.write(0xC000, 0xEA);
    cpu.reset(0xC000);
    std::istringstream log(reset_row + "\nmalformed unread row\n");
    REQUIRE(validate_trace(cpu, bus, log, 1) == 1);
    REQUIRE(cpu.pc() == 0xC001);
    REQUIRE(cpu.cycles() == 2);
    std::string next;
    REQUIRE(static_cast<bool>(std::getline(log, next)));
    REQUIRE(next == "malformed unread row");
}

TEST_CASE(test_validation_eof_before_limit_returns_matched_count) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.load({0xEA, 0xEA}, 0xC000);
    cpu.reset(0xC000);
    std::istringstream log(reset_row + "\n" + second_row + "\n");
    REQUIRE(validate_trace(cpu, bus, log, 10) == 2);
    REQUIRE(cpu.cycles() == 4);
}

TEST_CASE(test_validation_rejects_unsupported_opcode_on_final_or_limited_row) {
    REQUIRE(!Cpu6502::supports_opcode(0x02));
    for (const auto limit : {0u, 2u}) {
        Bus bus;
        Cpu6502 cpu(bus);
        bus.load({0xEA, 0x02}, 0xC000);
        cpu.reset(0xC000);
        // Opcode support comes from memory, not the ignored disassembly bytes.
        std::istringstream log(reset_row + "\n" + second_row);
        const auto message = expect_failure(cpu, bus, log, limit);
        REQUIRE(message.find("Line 2: unsupported opcode $02") != std::string::npos);
        require_full_states(message);
        REQUIRE(cpu.pc() == 0xC001);
        REQUIRE(cpu.cycles() == 2);
    }
}

TEST_CASE(test_validation_compares_state_before_checking_opcode) {
    Bus bus;
    Cpu6502 cpu(bus);
    bus.write(0xC000, 0x02);
    cpu.reset(0xC000);
    std::istringstream log("C000 A:01 X:00 Y:00 P:24 SP:FD CYC:7");
    REQUIRE(expect_failure(cpu, bus, log).find("state mismatch") != std::string::npos);
    REQUIRE(cpu.cycles() == 0);
}

TEST_CASE(test_validation_reports_stream_errors_with_and_without_exceptions) {
    for (const bool exceptions : {false, true}) {
        Bus bus;
        Cpu6502 cpu(bus);
        cpu.reset(0xC000);
        BrokenBuffer buffer;
        std::istream log(&buffer);
        if (exceptions) {
            log.exceptions(std::ios::badbit);
        }
        const auto message = expect_failure(cpu, bus, log);
        REQUIRE(message.find("Line 1: stream read error") != std::string::npos);
        REQUIRE(cpu.cycles() == 0);
    }
}

TEST_CASE(test_validation_reports_preexisting_stream_failure) {
    Bus bus;
    Cpu6502 cpu(bus);
    cpu.reset(0xC000);
    std::istringstream log(reset_row);
    log.setstate(std::ios::failbit);
    REQUIRE(expect_failure(cpu, bus, log).find("stream read error") != std::string::npos);
}

TEST_CASE(test_validation_handles_exception_enabled_eof) {
    for (const auto* ending : {"", "\n"}) {
        Bus bus;
        Cpu6502 cpu(bus);
        bus.write(0xC000, 0xEA);
        cpu.reset(0xC000);
        std::istringstream log(reset_row + ending);
        log.exceptions(std::ios::failbit | std::ios::badbit | std::ios::eofbit);
        REQUIRE(validate_trace(cpu, bus, log) == 1);
        REQUIRE(cpu.cycles() == 2);
    }
}
