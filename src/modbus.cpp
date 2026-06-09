#include "modbus/modbus.hpp"

namespace modbus {

std::uint16_t crc16(const std::uint8_t* data, std::size_t len) {
    std::uint16_t crc = 0xFFFF;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= static_cast<std::uint16_t>(data[i]);
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x0001) {
                crc = static_cast<std::uint16_t>((crc >> 1) ^ 0xA001);
            } else {
                crc = static_cast<std::uint16_t>(crc >> 1);
            }
        }
    }
    return crc;
}

std::uint16_t crc16(const Bytes& data) {
    return crc16(data.data(), data.size());
}

Bytes encode_rtu(std::uint8_t unit, const Pdu& pdu) {
    Bytes out;
    out.reserve(pdu.data.size() + 4);
    out.push_back(unit);
    out.push_back(pdu.function);
    out.insert(out.end(), pdu.data.begin(), pdu.data.end());
    const std::uint16_t crc = crc16(out);
    out.push_back(static_cast<std::uint8_t>(crc & 0xFF));         // low byte first
    out.push_back(static_cast<std::uint8_t>((crc >> 8) & 0xFF));  // then high byte
    return out;
}

std::optional<RtuFrame> decode_rtu(const Bytes& frame) {
    if (frame.size() < 4) return std::nullopt;  // unit + function + 2 CRC
    const std::size_t n = frame.size();
    const std::uint16_t recv =
        static_cast<std::uint16_t>(frame[n - 2]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(frame[n - 1]) << 8);
    if (crc16(frame.data(), n - 2) != recv) return std::nullopt;

    RtuFrame f;
    f.unit = frame[0];
    f.pdu.function = frame[1];
    f.pdu.data.assign(frame.begin() + 2, frame.begin() + (n - 2));
    return f;
}

Bytes encode_tcp(const Mbap& header, const Pdu& pdu) {
    // Length counts the unit id + the PDU (function + data).
    const std::uint16_t length = static_cast<std::uint16_t>(2 + pdu.data.size());
    Bytes out;
    out.reserve(7 + 1 + pdu.data.size());
    out.push_back(static_cast<std::uint8_t>((header.transaction >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(header.transaction & 0xFF));
    out.push_back(static_cast<std::uint8_t>((header.protocol >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(header.protocol & 0xFF));
    out.push_back(static_cast<std::uint8_t>((length >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(length & 0xFF));
    out.push_back(header.unit);
    out.push_back(pdu.function);
    out.insert(out.end(), pdu.data.begin(), pdu.data.end());
    return out;
}

std::optional<TcpFrame> decode_tcp(const Bytes& frame) {
    if (frame.size() < 8) return std::nullopt;  // 7 MBAP + at least a function code
    const std::size_t n = frame.size();
    const std::uint16_t length =
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(frame[4]) << 8) |
        static_cast<std::uint16_t>(frame[5]);
    // Length must equal everything after the length field (unit + PDU).
    if (static_cast<std::size_t>(length) != n - 6) return std::nullopt;

    TcpFrame f;
    f.header.transaction =
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(frame[0]) << 8) |
        static_cast<std::uint16_t>(frame[1]);
    f.header.protocol =
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(frame[2]) << 8) |
        static_cast<std::uint16_t>(frame[3]);
    f.header.unit = frame[6];
    f.pdu.function = frame[7];
    f.pdu.data.assign(frame.begin() + 8, frame.end());
    return f;
}

Pdu build_read_request(std::uint8_t function, std::uint16_t start, std::uint16_t quantity) {
    Pdu pdu;
    pdu.function = function;
    pdu.data = {
        static_cast<std::uint8_t>((start >> 8) & 0xFF),
        static_cast<std::uint8_t>(start & 0xFF),
        static_cast<std::uint8_t>((quantity >> 8) & 0xFF),
        static_cast<std::uint8_t>(quantity & 0xFF),
    };
    return pdu;
}

std::optional<ReadRequest> parse_read_request(const Pdu& pdu) {
    if (pdu.function < ReadCoils || pdu.function > ReadInputRegisters) return std::nullopt;
    if (pdu.data.size() != 4) return std::nullopt;
    ReadRequest r;
    r.start = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(pdu.data[0]) << 8 | pdu.data[1]);
    r.quantity = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(pdu.data[2]) << 8 | pdu.data[3]);
    return r;
}

std::optional<std::vector<std::uint16_t>> parse_register_response(const Pdu& pdu) {
    if (pdu.function != ReadHoldingRegisters && pdu.function != ReadInputRegisters) {
        return std::nullopt;
    }
    if (pdu.data.empty()) return std::nullopt;
    const std::uint8_t byte_count = pdu.data[0];
    if ((byte_count % 2) != 0) return std::nullopt;
    if (pdu.data.size() != static_cast<std::size_t>(byte_count) + 1) return std::nullopt;

    std::vector<std::uint16_t> regs;
    regs.reserve(byte_count / 2);
    for (std::size_t i = 1; i + 1 < pdu.data.size(); i += 2) {
        regs.push_back(static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(pdu.data[i]) << 8 | pdu.data[i + 1]));
    }
    return regs;
}

bool is_exception(const Pdu& pdu) {
    return (pdu.function & 0x80) != 0;
}

std::uint8_t exception_code(const Pdu& pdu) {
    if (!is_exception(pdu) || pdu.data.empty()) return 0;
    return pdu.data[0];
}

}  // namespace modbus
