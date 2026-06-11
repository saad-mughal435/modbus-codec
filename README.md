# modbus-codec

A small, dependency-free **C++17** codec for **Modbus** frames — the industrial
fieldbus behind PLCs, drives and building-automation gear. Encodes and decodes
both **RTU** (serial, CRC-checked) and **TCP** (MBAP header) framing, with
helpers for the common function codes and exception responses. Pure logic, no
I/O — drop it next to whatever transport you already have.

[![CI](https://github.com/saad-mughal435/modbus-codec/actions/workflows/ci.yml/badge.svg)](https://github.com/saad-mughal435/modbus-codec/actions/workflows/ci.yml)

## Features

- **CRC-16/MODBUS** (check value `0x4B37` for `"123456789"`, unit-tested).
- **RTU** `encode_rtu` / `decode_rtu` — appends/validates the trailing CRC.
- **TCP** `encode_tcp` / `decode_tcp` — builds/validates the 7-byte MBAP header.
- **PDU helpers** — build & parse read requests (FC 0x01–0x04), parse register
  responses (FC 0x03/0x04), and detect exception responses + codes.

## Usage

```cpp
#include <modbus/modbus.hpp>

// Read 3 holding registers from address 0x006B on unit 0x11
modbus::Pdu   req   = modbus::build_read_request(modbus::ReadHoldingRegisters, 0x006B, 3);
modbus::Bytes frame = modbus::encode_rtu(0x11, req);     // 11 03 00 6B 00 03 + CRC

if (auto f = modbus::decode_rtu(frame)) {                // CRC-checked
    auto r = modbus::parse_read_request(f->pdu);         // r->start, r->quantity
}
```

## Build, test, run the demo

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/modbus                       # self-demo
./build/modbus rtu-decode 11 03 00 6B 00 03 76 87
```

Requires CMake ≥ 3.16 and a C++17 compiler. Tests use
[Catch2](https://github.com/catchorg/Catch2) (fetched automatically) and cover
the CRC check value, RTU/TCP round-trips, CRC/length rejection, register-response
parsing and exception detection.

## License

MIT © Muhammad Saad
