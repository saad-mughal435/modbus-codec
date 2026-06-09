#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

#include "modbus/modbus.hpp"

namespace {

void print_hex(const char* label, const modbus::Bytes& b) {
    std::cout << label;
    char buf[4];
    for (std::size_t i = 0; i < b.size(); ++i) {
        std::snprintf(buf, sizeof(buf), "%02X", b[i]);
        std::cout << buf;
        if (i + 1 < b.size()) std::cout << ' ';
    }
    std::cout << "\n";
}

modbus::Bytes parse_hex_args(int argc, char** argv, int from) {
    modbus::Bytes out;
    int hi = -1;
    for (int i = from; i < argc; ++i) {
        const std::string s = argv[i];
        for (char c : s) {
            int v = -1;
            if (c >= '0' && c <= '9') v = c - '0';
            else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
            if (v < 0) continue;
            if (hi < 0) {
                hi = v;
            } else {
                out.push_back(static_cast<std::uint8_t>((hi << 4) | v));
                hi = -1;
            }
        }
    }
    return out;
}

void describe_pdu(const modbus::Pdu& pdu) {
    char fn[8];
    std::snprintf(fn, sizeof(fn), "0x%02X", pdu.function);
    std::cout << "  function : " << fn << "\n";
    if (modbus::is_exception(pdu)) {
        char ec[8];
        std::snprintf(ec, sizeof(ec), "0x%02X", modbus::exception_code(pdu));
        std::cout << "  exception: " << ec << "\n";
        return;
    }
    if (const auto req = modbus::parse_read_request(pdu)) {
        std::cout << "  read     : start=" << req->start << " quantity=" << req->quantity << "\n";
    }
    if (const auto regs = modbus::parse_register_response(pdu)) {
        std::cout << "  registers:";
        for (std::uint16_t r : *regs) std::cout << ' ' << r;
        std::cout << "\n";
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 2 && std::string(argv[1]) == "rtu-decode") {
        const modbus::Bytes frame = parse_hex_args(argc, argv, 2);
        const auto f = modbus::decode_rtu(frame);
        if (!f) {
            std::cout << "Invalid RTU frame (bad CRC or too short).\n";
            return 1;
        }
        std::cout << "RTU frame OK\n  unit     : " << static_cast<int>(f->unit) << "\n";
        describe_pdu(f->pdu);
        return 0;
    }

    std::cout << "modbus-codec demo\n"
                 "  tip: modbus rtu-decode <hex bytes>   decodes your own RTU frame\n\n";

    const modbus::Pdu req = modbus::build_read_request(modbus::ReadHoldingRegisters, 0x006B, 3);
    const modbus::Bytes rtu = modbus::encode_rtu(0x11, req);
    const modbus::Mbap header{0x0001, 0x0000, 0x11};
    const modbus::Bytes tcp = modbus::encode_tcp(header, req);

    std::cout << "Read Holding Registers  unit=0x11 start=0x006B qty=3\n";
    print_hex("  RTU : ", rtu);
    print_hex("  TCP : ", tcp);

    std::cout << "\nDecoded RTU:\n  unit     : " << static_cast<int>(0x11) << "\n";
    if (const auto f = modbus::decode_rtu(rtu)) {
        describe_pdu(f->pdu);
    }
    return 0;
}
