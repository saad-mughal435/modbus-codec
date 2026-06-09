#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace modbus {

using Bytes = std::vector<std::uint8_t>;

/// A protocol data unit: function code + payload, without transport framing.
struct Pdu {
    std::uint8_t function = 0;
    Bytes        data;
};

/// CRC-16/MODBUS (reflected, poly 0xA001, init 0xFFFF).
/// The standard check value for the ASCII string "123456789" is 0x4B37.
std::uint16_t crc16(const std::uint8_t* data, std::size_t len);
std::uint16_t crc16(const Bytes& data);

// --------------------------------------------------------------------------
// Modbus RTU (serial): [unit][function][data...][crc_lo][crc_hi]
// --------------------------------------------------------------------------
struct RtuFrame {
    std::uint8_t unit = 0;
    Pdu          pdu;
};
Bytes encode_rtu(std::uint8_t unit, const Pdu& pdu);
/// nullopt if the frame is too short or the trailing CRC does not match.
std::optional<RtuFrame> decode_rtu(const Bytes& frame);

// --------------------------------------------------------------------------
// Modbus TCP: 7-byte MBAP header + PDU
// --------------------------------------------------------------------------
struct Mbap {
    std::uint16_t transaction = 0;
    std::uint16_t protocol    = 0;  // 0 for Modbus
    std::uint8_t  unit        = 0;
};
struct TcpFrame {
    Mbap header;
    Pdu  pdu;
};
Bytes encode_tcp(const Mbap& header, const Pdu& pdu);
/// nullopt if the frame is too short or the MBAP length field is inconsistent.
std::optional<TcpFrame> decode_tcp(const Bytes& frame);

// --------------------------------------------------------------------------
// PDU helpers for common function codes
// --------------------------------------------------------------------------
enum Function : std::uint8_t {
    ReadCoils            = 0x01,
    ReadDiscreteInputs   = 0x02,
    ReadHoldingRegisters = 0x03,
    ReadInputRegisters   = 0x04,
    WriteSingleCoil      = 0x05,
    WriteSingleRegister  = 0x06,
};

/// A "read" request payload: starting address + quantity.
struct ReadRequest {
    std::uint16_t start    = 0;
    std::uint16_t quantity = 0;
};
Pdu build_read_request(std::uint8_t function, std::uint16_t start, std::uint16_t quantity);
/// Parse a 4-byte read request (functions 0x01-0x04). nullopt if malformed.
std::optional<ReadRequest> parse_read_request(const Pdu& pdu);

/// Decode a read-registers response (0x03 / 0x04) into register values.
/// nullopt if the byte count is missing, odd, or inconsistent.
std::optional<std::vector<std::uint16_t>> parse_register_response(const Pdu& pdu);

/// True if the PDU is an exception response (function high bit set).
bool is_exception(const Pdu& pdu);
/// Exception code when is_exception(), otherwise 0.
std::uint8_t exception_code(const Pdu& pdu);

}  // namespace modbus
