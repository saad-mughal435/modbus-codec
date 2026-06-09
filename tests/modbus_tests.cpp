#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>

#include "modbus/modbus.hpp"

using namespace modbus;

TEST_CASE("CRC-16/MODBUS check value", "[crc]") {
    const std::string s = "123456789";
    const std::uint16_t crc =
        crc16(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
    CHECK(crc == 0x4B37);  // canonical CRC-16/MODBUS check value
}

TEST_CASE("build and parse a read request", "[pdu]") {
    const Pdu pdu = build_read_request(ReadHoldingRegisters, 0x006B, 3);
    REQUIRE(pdu.function == 0x03);
    REQUIRE(pdu.data == Bytes{0x00, 0x6B, 0x00, 0x03});

    const auto req = parse_read_request(pdu);
    REQUIRE(req.has_value());
    CHECK(req->start == 0x006B);
    CHECK(req->quantity == 3);
}

TEST_CASE("RTU encode/decode round-trips", "[rtu]") {
    const Pdu pdu = build_read_request(ReadHoldingRegisters, 0x006B, 3);
    const Bytes frame = encode_rtu(0x11, pdu);
    REQUIRE(frame.size() == 8);  // unit + function + 4 data + 2 CRC
    CHECK(frame[0] == 0x11);
    CHECK(frame[1] == 0x03);

    const auto decoded = decode_rtu(frame);
    REQUIRE(decoded.has_value());
    CHECK(decoded->unit == 0x11);
    CHECK(decoded->pdu.function == 0x03);
    CHECK(decoded->pdu.data == pdu.data);
}

TEST_CASE("RTU rejects a corrupted CRC and short frames", "[rtu]") {
    Bytes frame = encode_rtu(0x11, build_read_request(ReadHoldingRegisters, 1, 1));
    frame.back() = static_cast<std::uint8_t>(frame.back() ^ 0xFF);
    CHECK_FALSE(decode_rtu(frame).has_value());
    CHECK_FALSE(decode_rtu(Bytes{0x11, 0x03}).has_value());
}

TEST_CASE("TCP encode/decode round-trips", "[tcp]") {
    Mbap h;
    h.transaction = 0x0001;
    h.protocol = 0x0000;
    h.unit = 0x11;
    const Pdu pdu = build_read_request(ReadHoldingRegisters, 0x006B, 3);
    const Bytes frame = encode_tcp(h, pdu);
    REQUIRE(frame.size() == 12);  // MBAP(7) + function(1) + data(4)
    CHECK(frame[4] == 0x00);
    CHECK(frame[5] == 0x06);  // length = unit + function + 4 data = 6

    const auto decoded = decode_tcp(frame);
    REQUIRE(decoded.has_value());
    CHECK(decoded->header.transaction == 0x0001);
    CHECK(decoded->header.unit == 0x11);
    CHECK(decoded->pdu.function == 0x03);
    CHECK(decoded->pdu.data == pdu.data);
}

TEST_CASE("TCP rejects an inconsistent length field", "[tcp]") {
    Mbap h;
    h.unit = 1;
    Bytes frame = encode_tcp(h, build_read_request(ReadHoldingRegisters, 0, 1));
    frame[5] = 0x42;  // corrupt the length
    CHECK_FALSE(decode_tcp(frame).has_value());
}

TEST_CASE("parse a read-registers response", "[pdu]") {
    const Pdu pdu{0x03, Bytes{0x04, 0x00, 0x0A, 0x01, 0x02}};
    const auto regs = parse_register_response(pdu);
    REQUIRE(regs.has_value());
    REQUIRE(regs->size() == 2);
    CHECK((*regs)[0] == 0x000A);
    CHECK((*regs)[1] == 0x0102);
}

TEST_CASE("register response rejects malformed byte counts", "[pdu]") {
    CHECK_FALSE(parse_register_response(Pdu{0x03, Bytes{0x03, 0x00, 0x0A, 0x01}}).has_value());
    CHECK_FALSE(parse_register_response(Pdu{0x03, Bytes{0x04, 0x00, 0x0A}}).has_value());
}

TEST_CASE("exception responses are detected", "[exc]") {
    const Pdu exc{0x83, Bytes{0x02}};  // 0x03 | 0x80, code 0x02 (illegal data address)
    CHECK(is_exception(exc));
    CHECK(exception_code(exc) == 0x02);

    const Pdu ok{0x03, Bytes{0x02, 0x00, 0x0A}};
    CHECK_FALSE(is_exception(ok));
    CHECK(exception_code(ok) == 0);
}
