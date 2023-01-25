/** @file
    Decoder for Minol Smoke detectors, water, and energy counters.

    Copyright (C) 2023 Robert Jördens <jordens@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 */
/**
Decoder for Minol/Brunata smoke detectors (Minoprotect 4 radio),
water, and heating energy (Minocal) counters. Maybe also others.

- Modulation: FSK PCM
- Frequency: 868.3MHz
- 32.768 kHz bit clock, 30.52 µs
- Most likely based on TI CC1101 as it takes all the default settings
  (whitening poly, CRC poly, sync word)

Payload format:
- Preamble          {32} 0xaaaaaaaa
- Syncword          {32} 0xd391d391
- Length            {8}
- Payload           {n}
- Checksum          {16} CRC16 poly=0x8005 init=0xffff

To get raw data:

    ./rtl_433 -f 868.3M -X 'name=minol,modulation=FSK_PCM,short=30.52,long=30.52,reset=100,preamble={32}d391d391'
*/

#include "decoder.h"

static int minol_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{
    uint8_t const preamble[] = {
            // 0xaa, 0xaa, 0xaa, 0xaa,   // preamble (bit sync)
            0xd3, 0x91, 0xd3, 0x91       // sync (frame sync, access code)
    };

    data_t *data;

    if (bitbuffer->num_rows != 1) {
        return DECODE_ABORT_EARLY;
    }

    int row = 0;
    // Validate message and reject it as fast as possible: check for preamble
    unsigned start_pos = bitbuffer_search(bitbuffer, row, 0, preamble, sizeof (preamble) * 8);

    if (start_pos == bitbuffer->bits_per_row[row]) {
        return DECODE_ABORT_EARLY; // no preamble detected
    }

    // check min length
    if (bitbuffer->bits_per_row[row] < (sizeof (preamble) + 1 /* len */ + 2 /* crc */) * 8) {
        return DECODE_ABORT_LENGTH;
    }

    uint8_t len;
    bitbuffer_extract_bytes(bitbuffer, row, start_pos + sizeof (preamble) * 8, &len, 8);

    uint8_t frame[256 + 2] = {len}; // CC1101 max packet payload size plus len plus CRC
    // Get frame (len don't include the length byte and the crc16 bytes)
    bitbuffer_extract_bytes(bitbuffer, row,
            start_pos + (sizeof (preamble) + 1) * 8,
            frame, (len + 2) * 8);

    // CC1101 PN9 whitening sequence (LFSR poly 0x21, init 0x1ff)
    uint8_t const pn9[256] = {
        0xff, 0xe1, 0x1d, 0x9a, 0xed, 0x85, 0x33, 0x24,
        0xea, 0x7a, 0xd2, 0x39, 0x70, 0x97, 0x57, 0x0a,
        0x54, 0x7d, 0x2d, 0xd8, 0x6d, 0x0d, 0xba, 0x8f,
        0x67, 0x59, 0xc7, 0xa2, 0xbf, 0x34, 0xca, 0x18,
        0x30, 0x53, 0x93, 0xdf, 0x92, 0xec, 0xa7, 0x15,
        0x8a, 0xdc, 0xf4, 0x86, 0x55, 0x4e, 0x18, 0x21,
        0x40, 0xc4, 0xc4, 0xd5, 0xc6, 0x91, 0x8a, 0xcd,
        0xe7, 0xd1, 0x4e, 0x09, 0x32, 0x17, 0xdf, 0x83,
        0xff, 0xf0, 0x0e, 0xcd, 0xf6, 0xc2, 0x19, 0x12,
        0x75, 0x3d, 0xe9, 0x1c, 0xb8, 0xcb, 0x2b, 0x05,
        0xaa, 0xbe, 0x16, 0xec, 0xb6, 0x06, 0xdd, 0xc7,
        0xb3, 0xac, 0x63, 0xd1, 0x5f, 0x1a, 0x65, 0x0c,
        0x98, 0xa9, 0xc9, 0x6f, 0x49, 0xf6, 0xd3, 0x0a,
        0x45, 0x6e, 0x7a, 0xc3, 0x2a, 0x27, 0x8c, 0x10,
        0x20, 0x62, 0xe2, 0x6a, 0xe3, 0x48, 0xc5, 0xe6,
        0xf3, 0x68, 0xa7, 0x04, 0x99, 0x8b, 0xef, 0xc1,
        0x7f, 0x78, 0x87, 0x66, 0x7b, 0xe1, 0x0c, 0x89,
        0xba, 0x9e, 0x74, 0x0e, 0xdc, 0xe5, 0x95, 0x02,
        0x55, 0x5f, 0x0b, 0x76, 0x5b, 0x83, 0xee, 0xe3,
        0x59, 0xd6, 0xb1, 0xe8, 0x2f, 0x8d, 0x32, 0x06,
        0xcc, 0xd4, 0xe4, 0xb7, 0x24, 0xfb, 0x69, 0x85,
        0x22, 0x37, 0xbd, 0x61, 0x95, 0x13, 0x46, 0x08,
        0x10, 0x31, 0x71, 0xb5, 0x71, 0xa4, 0x62, 0xf3,
        0x79, 0xb4, 0x53, 0x82, 0xcc, 0xc5, 0xf7, 0xe0,
        0x3f, 0xbc, 0x43, 0xb3, 0xbd, 0x70, 0x86, 0x44,
        0x5d, 0x4f, 0x3a, 0x07, 0xee, 0xf2, 0x4a, 0x81,
        0xaa, 0xaf, 0x05, 0xbb, 0xad, 0x41, 0xf7, 0xf1,
        0x2c, 0xeb, 0x58, 0xf4, 0x97, 0x46, 0x19, 0x03,
        0x66, 0x6a, 0xf2, 0x5b, 0x92, 0xfd, 0xb4, 0x42,
        0x91, 0x9b, 0xde, 0xb0, 0xca, 0x09, 0x23, 0x04,
        0x88, 0x98, 0xb8, 0xda, 0x38, 0x52, 0xb1, 0xf9,
        0x3c, 0xda, 0x29, 0x41, 0xe6, 0xe2, 0x7b, 0xf0,
    };

    for (int i = 0; i < len; ++i) {
        frame[i + 1] ^= pn9[i];
    }

    uint16_t crc_calc = crc16(frame, len + 1, 0x8005, 0xffff);
    uint16_t crc_recv = frame[len + 1] << 8 | frame[len + 2];
    if (crc_recv != crc_calc) {
        decoder_logf(decoder, 1, __func__, "CRC invalid %04x != %04x",
                crc_recv, crc_calc);
        return DECODE_FAIL_MIC;
    }

    decoder_log_bitrow(decoder, 2, __func__, frame, (1 + len + 2) * 8, "frame data");

    char payload_hex[256 * 2] = {0};
    for (int i = 0; i < len; ++i)
        sprintf(&payload_hex[i * 2], "%02x", frame[i + 1]);

    /* clang-format off */
    data = data_make(
            "model",        "",                 DATA_STRING, "Minol",
            "raw",          "Payload",          DATA_STRING, payload_hex,
            "mic",          "Integrity",        DATA_STRING, "CRC",
            NULL);
    /* clang-format on */
    decoder_output_data(decoder, data);
    return 1;
}

static char *output_fields[] = {
        "model",
        "raw",
        "mic",
        NULL,
};

r_device minol = {
        .name        = "Minol",
        .modulation  = FSK_PULSE_PCM,
        .short_width = 30.52,
        .long_width  = 30.52,
        .reset_limit = 1000,
        .decode_fn   = &minol_decode,
        .fields      = output_fields,
};
