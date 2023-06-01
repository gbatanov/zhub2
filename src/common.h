#ifndef GSB_COMMON_H
#define GSB_COMMON_H

#define LOWBYTE(a) (static_cast<uint8_t>(a))
#define HIGHBYTE(a) static_cast<uint8_t>((static_cast<uint16_t>((a)) >> 8))
#define _UINT16(a, b) (static_cast<uint16_t>(a) + (static_cast<uint16_t>(b) << 8))
#define _UINT64(a, b, c, d, e, f, g, h) (static_cast<uint64_t>(a) + (static_cast<uint64_t>(b) << 8) + (static_cast<uint64_t>(c) << 16) + (static_cast<uint64_t>(d) << 24) + (static_cast<uint64_t>(e) << 32) + (static_cast<uint64_t>(f) << 40) + (static_cast<uint64_t>(g) << 48) + (static_cast<uint64_t>(h) << 56))

#endif