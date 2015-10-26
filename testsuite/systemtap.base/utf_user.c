#include <stdint.h>

static uint16_t utf16_string[] = {
    0x73, 0x74, 0x61, 0x70, // "stap" (one-byte UTF-8 each)
    0x7F, 0x80,             // last one-byte, first two-byte
    0x391, 0x3A9,           // "ΑΩ" (two-byte UTF-8 each)
    0x7FF, 0x800,           // last two-byte, first three-byte
    0x263A,                 // "☺" (three-byte UTF-8)
    0xFFFF, 0xD800, 0xDC00, // last three-byte, first four-byte
    0xD83D, 0xDE08,         // U+1F608 "😈" (four-byte UTF-8)
    0xDBFF, 0xDFFF,         // last supported four-byte
    0
};

static uint32_t utf32_string[] = {
    0x73, 0x74, 0x61, 0x70, // "stap" (one-byte UTF-8 each)
    0x7F, 0x80,             // last one-byte, first two-byte
    0x391, 0x3A9,           // "ΑΩ" (two-byte UTF-8 each)
    0x7FF, 0x800,           // last two-byte, first three-byte
    0x263A,                 // "☺" (three-byte UTF-8)
    0xFFFF, 0x10000,        // last three-byte, first four-byte
    0x1F608,                // "😈" (four-byte UTF-8)
    0x10FFFF,               // last supported four-byte
    0
};

int main()
{
    return 0;
}
