/*
 * Copyright (c) 2017 Fastly, Kazuho Oku
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include "picotls.h"
#include "picotls/openssl.h"
#include "quicly.h"
#include "quicly/streambuf.h"
#include "../lib/quicly.c"
#include "test.h"

#define RSA_PRIVATE_KEY                                                                                                            \
    "-----BEGIN RSA PRIVATE KEY-----\n"                                                                                            \
    "MIIEowIBAAKCAQEA5soWzSG7iyawQlHM1yaX2dUAATUkhpbg2WPFOEem7E3zYzc6\n"                                                           \
    "A/Z+bViFlfEgL37cbDUb4pnOAHrrsjGgkyBYh5i9iCTVfCk+H6SOHZJORO1Tq8X9\n"                                                           \
    "C7WcNcshpSdm2Pa8hmv9hsHbLSeoPNeg8NkTPwMVaMZ2GpdmiyAmhzSZ2H9mzNI7\n"                                                           \
    "ntPW/XCchVf+ax2yt9haZ+mQE2NPYwHDjqCtdGkP5ZXXnYhJSBzSEhxfGckIiKDy\n"                                                           \
    "OxiNkLFLvUdT4ERSFBjauP2cSI0XoOUsiBxJNwHH310AU8jZbveSTcXGYgEuu2MI\n"                                                           \
    "uDo7Vhkq5+TCqXsIFNbjy0taOoPRvUbPsbqFlQIDAQABAoIBAQCWcUv1wjR/2+Nw\n"                                                           \
    "B+Swp267R9bt8pdxyK6f5yKrskGErremiFygMrFtVBQYjws9CsRjISehSkN4GqjE\n"                                                           \
    "CweygJZVJeL++YvUmQnvFJSzgCjXU6GEStbOKD/A7T5sa0fmzMhOE907V+kpAT3x\n"                                                           \
    "E1rNRaP/ImJ1X1GjuefVb0rOPiK/dehFQWfsUkOvh+J3PU76wcnexxzJgxhVxdfX\n"                                                           \
    "qNa7UDsWzTImUjcHIfnhXc1K/oSKk6HjImQi/oE4lgoJUCEDaUbq0nXNrM0EmTTv\n"                                                           \
    "OQ5TVP5Lds9p8UDEa55eZllGXam0zKjhDKtkQ/5UfnxsAv2adY5cuH+XN0ExfKD8\n"                                                           \
    "wIZ5qINtAoGBAPRbQGZZkP/HOYA4YZ9HYAUQwFS9IZrQ8Y7C/UbL01Xli13nKalH\n"                                                           \
    "xXdG6Zv6Yv0FCJKA3N945lEof9rwriwhuZbyrA1TcKok/s7HR8Bhcsm2DzRD5OiC\n"                                                           \
    "3HK+Xy+6fBaMebffqBPp3Lfj/lSPNt0w/8DdrKBTw/cAy40g0n1zEu07AoGBAPHJ\n"                                                           \
    "V4IfQBiblCqDh77FfQRUNR4hVbbl00Gviigiw563nk7sxdrOJ1edTyTOUBHtM3zg\n"                                                           \
    "AT9sYz2CUXvsyEPqzMDANWMb9e2R//NcP6aM4k7WQRnwkZkp0WOIH95U2o1MHCYc\n"                                                           \
    "5meAHVf2UMl+64xU2ZfY3rjMmPLjWMt0hKYsOmtvAoGAClIQVkJSLXtsok2/Ucrh\n"                                                           \
    "81TRysJyOOe6TB1QNT1Gn8oiKMUqrUuqu27zTvM0WxtrUUTAD3A7yhG71LN1p8eE\n"                                                           \
    "3ytAuQ9dItKNMI6aKTX0czCNU9fKQ0fDp9UCkDGALDOisHFx1+V4vQuUIl4qIw1+\n"                                                           \
    "v9adA+iFzljqP/uy6DmEAyECgYAyWCgecf9YoFxzlbuYH2rukdIVmf9M/AHG9ZQg\n"                                                           \
    "00xEKhuOd4KjErXiamDmWwcVFHzaDZJ08E6hqhbpZN42Nhe4Ms1q+5FzjCjtNVIT\n"                                                           \
    "jdY5cCdSDWNjru9oeBmao7R2I1jhHrdi6awyeplLu1+0cp50HbYSaJeYS3pbssFE\n"                                                           \
    "EIWBhQKBgG3xleD4Sg9rG2OWQz5IrvLFg/Hy7YWyushVez61kZeLDnt9iM2um76k\n"                                                           \
    "/xFNIW0a+eL2VxRTCbXr9z86hjc/6CeSJHKYFQl4zsSAZkaIJ0+HbrhDNBAYh9b2\n"                                                           \
    "mRdX+OMdZ7Z5J3Glt8ENFRqe8RlESMpAKxjR+dID0bjwAjVr2KCh\n"                                                                       \
    "-----END RSA PRIVATE KEY-----\n"

#define RSA_CERTIFICATE                                                                                                            \
    "-----BEGIN CERTIFICATE-----\n"                                                                                                \
    "MIICqDCCAZACCQDI5jeEvExN+TANBgkqhkiG9w0BAQUFADAWMRQwEgYDVQQDEwtl\n"                                                           \
    "eGFtcGxlLmNvbTAeFw0xNjA5MzAwMzQ0NTFaFw0yNjA5MjgwMzQ0NTFaMBYxFDAS\n"                                                           \
    "BgNVBAMTC2V4YW1wbGUuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC\n"                                                           \
    "AQEA5soWzSG7iyawQlHM1yaX2dUAATUkhpbg2WPFOEem7E3zYzc6A/Z+bViFlfEg\n"                                                           \
    "L37cbDUb4pnOAHrrsjGgkyBYh5i9iCTVfCk+H6SOHZJORO1Tq8X9C7WcNcshpSdm\n"                                                           \
    "2Pa8hmv9hsHbLSeoPNeg8NkTPwMVaMZ2GpdmiyAmhzSZ2H9mzNI7ntPW/XCchVf+\n"                                                           \
    "ax2yt9haZ+mQE2NPYwHDjqCtdGkP5ZXXnYhJSBzSEhxfGckIiKDyOxiNkLFLvUdT\n"                                                           \
    "4ERSFBjauP2cSI0XoOUsiBxJNwHH310AU8jZbveSTcXGYgEuu2MIuDo7Vhkq5+TC\n"                                                           \
    "qXsIFNbjy0taOoPRvUbPsbqFlQIDAQABMA0GCSqGSIb3DQEBBQUAA4IBAQAwZQsG\n"                                                           \
    "E/3DQFBOnmBITFsaIVJVXU0fbfIjy3p1r6O9z2zvrfB1i8AMxOORAVjE5wHstGnK\n"                                                           \
    "3sLMjkMYXqu1XEfQbStQN+Bsi8m+nE/x9MmuLthpzJHXUmPYZ4TKs0KJmFPLTXYi\n"                                                           \
    "j0OrP0a5BNcyGj/B4Z33aaU9N3z0TWBwx4OPjJoK3iInBx80sC1Ig2PE6mDBxLOg\n"                                                           \
    "5Ohm/XU/43MrtH8SgYkxr3OyzXTm8J0RFMWhYlo1uqR+pWV3TgacixNnUq5w5h4m\n"                                                           \
    "sqXcikh+j8ReNXsKnMOAfFo+HbRqyKWNE3DekCIiiQ5ds4A4SfT7pYyGAmBkAxht\n"                                                           \
    "sS919x2o8l97kaYf\n"                                                                                                           \
    "-----END CERTIFICATE-----\n"

static int64_t get_now(quicly_context_t *ctx);
static void on_destroy(quicly_stream_t *stream);
static int on_egress_stop(quicly_stream_t *stream, int err);
static int on_ingress_reset(quicly_stream_t *stream, int err);

int64_t quic_now;
quicly_context_t quic_ctx;
quicly_stream_callbacks_t stream_callbacks = {on_destroy,     quicly_streambuf_egress_shift,    quicly_streambuf_egress_emit,
                                              on_egress_stop, quicly_streambuf_ingress_receive, on_ingress_reset};
size_t on_destroy_callcnt;

int64_t get_now(quicly_context_t *ctx)
{
    return quic_now;
}

void on_destroy(quicly_stream_t *stream)
{
    test_streambuf_t *sbuf = stream->data;
    sbuf->is_detached = 1;
    ++on_destroy_callcnt;
}

int on_egress_stop(quicly_stream_t *stream, int err)
{
    assert(QUICLY_ERROR_IS_QUIC_APPLICATION(err));
    test_streambuf_t *sbuf = stream->data;
    sbuf->error_received.stop_sending = err;
    return 0;
}

int on_ingress_reset(quicly_stream_t *stream, int err)
{
    assert(QUICLY_ERROR_IS_QUIC_APPLICATION(err));
    test_streambuf_t *sbuf = stream->data;
    sbuf->error_received.reset_stream = err;
    return 0;
}

int on_stream_open(quicly_stream_t *stream)
{
    test_streambuf_t *sbuf;
    int ret;

    ret = quicly_streambuf_create(stream, sizeof(*sbuf));
    assert(ret == 0);
    sbuf = stream->data;
    sbuf->error_received.stop_sending = -1;
    sbuf->error_received.reset_stream = -1;
    stream->callbacks = &stream_callbacks;

    return 0;
}

static void test_vector(void)
{
    static const uint8_t expected_payload[] = {
        0x06, 0x00, 0x40, 0xc4, 0x01, 0x00, 0x00, 0xc0, 0x03, 0x03, 0x66, 0x60, 0x26, 0x1f, 0xf9, 0x47, 0xce, 0xa4, 0x9c, 0xce,
        0x6c, 0xfa, 0xd6, 0x87, 0xf4, 0x57, 0xcf, 0x1b, 0x14, 0x53, 0x1b, 0xa1, 0x41, 0x31, 0xa0, 0xe8, 0xf3, 0x09, 0xa1, 0xd0,
        0xb9, 0xc4, 0x00, 0x00, 0x06, 0x13, 0x01, 0x13, 0x03, 0x13, 0x02, 0x01, 0x00, 0x00, 0x91, 0x00, 0x00, 0x00, 0x0b, 0x00,
        0x09, 0x00, 0x00, 0x06, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0xff, 0x01, 0x00, 0x01, 0x00, 0x00, 0x0a, 0x00, 0x14, 0x00,
        0x12, 0x00, 0x1d, 0x00, 0x17, 0x00, 0x18, 0x00, 0x19, 0x01, 0x00, 0x01, 0x01, 0x01, 0x02, 0x01, 0x03, 0x01, 0x04, 0x00,
        0x23, 0x00, 0x00, 0x00, 0x33, 0x00, 0x26, 0x00, 0x24, 0x00, 0x1d, 0x00, 0x20, 0x4c, 0xfd, 0xfc, 0xd1, 0x78, 0xb7, 0x84,
        0xbf, 0x32, 0x8c, 0xae, 0x79, 0x3b, 0x13, 0x6f, 0x2a, 0xed, 0xce, 0x00, 0x5f, 0xf1, 0x83, 0xd7, 0xbb, 0x14, 0x95, 0x20,
        0x72, 0x36, 0x64, 0x70, 0x37, 0x00, 0x2b, 0x00, 0x03, 0x02, 0x03, 0x04, 0x00, 0x0d, 0x00, 0x20, 0x00, 0x1e, 0x04, 0x03,
        0x05, 0x03, 0x06, 0x03, 0x02, 0x03, 0x08, 0x04, 0x08, 0x05, 0x08, 0x06, 0x04, 0x01, 0x05, 0x01, 0x06, 0x01, 0x02, 0x01,
        0x04, 0x02, 0x05, 0x02, 0x06, 0x02, 0x02, 0x02, 0x00, 0x2d, 0x00, 0x02, 0x01, 0x01, 0x00, 0x1c, 0x00, 0x02, 0x40, 0x01};
    uint8_t datagram[] = {
        0xc1, 0xff, 0x00, 0x00, 0x12, 0x50, 0x83, 0x94, 0xc8, 0xf0, 0x3e, 0x51, 0x57, 0x08, 0x00, 0x44, 0x9f, 0x0d, 0xbc, 0x19,
        0x5a, 0x00, 0x00, 0xf3, 0xa6, 0x94, 0xc7, 0x57, 0x75, 0xb4, 0xe5, 0x46, 0x17, 0x2c, 0xe9, 0xe0, 0x47, 0xcd, 0x0b, 0x5b,
        0xee, 0x51, 0x81, 0x64, 0x8c, 0x72, 0x7a, 0xdc, 0x87, 0xf7, 0xea, 0xe5, 0x44, 0x73, 0xec, 0x6c, 0xba, 0x6b, 0xda, 0xd4,
        0xf5, 0x98, 0x23, 0x17, 0x4b, 0x76, 0x9f, 0x12, 0x35, 0x8a, 0xbd, 0x29, 0x2d, 0x4f, 0x32, 0x86, 0x93, 0x44, 0x84, 0xfb,
        0x8b, 0x23, 0x9c, 0x38, 0x73, 0x2e, 0x1f, 0x3b, 0xbb, 0xc6, 0xa0, 0x03, 0x05, 0x64, 0x87, 0xeb, 0x8b, 0x5c, 0x88, 0xb9,
        0xfd, 0x92, 0x79, 0xff, 0xff, 0x3b, 0x0f, 0x4e, 0xcf, 0x95, 0xc4, 0x62, 0x4d, 0xb6, 0xd6, 0x5d, 0x41, 0x13, 0x32, 0x9e,
        0xe9, 0xb0, 0xbf, 0x8c, 0xdd, 0x7c, 0x8a, 0x8d, 0x72, 0x80, 0x6d, 0x55, 0xdf, 0x25, 0xec, 0xb6, 0x64, 0x88, 0xbc, 0x11,
        0x9d, 0x7c, 0x9a, 0x29, 0xab, 0xaf, 0x99, 0xbb, 0x33, 0xc5, 0x6b, 0x08, 0xad, 0x8c, 0x26, 0x99, 0x5f, 0x83, 0x8b, 0xb3,
        0xb7, 0xa3, 0xd5, 0xc1, 0x85, 0x8b, 0x8e, 0xc0, 0x6b, 0x83, 0x9d, 0xb2, 0xdc, 0xf9, 0x18, 0xd5, 0xea, 0x93, 0x17, 0xf1,
        0xac, 0xd6, 0xb6, 0x63, 0xcc, 0x89, 0x25, 0x86, 0x8e, 0x2f, 0x6a, 0x1b, 0xda, 0x54, 0x66, 0x95, 0xf3, 0xc3, 0xf3, 0x31,
        0x75, 0x94, 0x4d, 0xb4, 0xa1, 0x1a, 0x34, 0x6a, 0xfb, 0x07, 0xe7, 0x84, 0x89, 0xe5, 0x09, 0xb0, 0x2a, 0xdd, 0x51, 0xb7,
        0xb2, 0x03, 0xed, 0xa5, 0xc3, 0x30, 0xb0, 0x36, 0x41, 0x17, 0x9a, 0x31, 0xfb, 0xba, 0x9b, 0x56, 0xce, 0x00, 0xf3, 0xd5,
        0xb5, 0xe3, 0xd7, 0xd9, 0xc5, 0x42, 0x9a, 0xeb, 0xb9, 0x57, 0x6f, 0x2f, 0x7e, 0xac, 0xbe, 0x27, 0xbc, 0x1b, 0x80, 0x82,
        0xaa, 0xf6, 0x8f, 0xb6, 0x9c, 0x92, 0x1a, 0xa5, 0xd3, 0x3e, 0xc0, 0xc8, 0x51, 0x04, 0x10, 0x86, 0x5a, 0x17, 0x8d, 0x86,
        0xd7, 0xe5, 0x41, 0x22, 0xd5, 0x5e, 0xf2, 0xc2, 0xbb, 0xc0, 0x40, 0xbe, 0x46, 0xd7, 0xfe, 0xce, 0x73, 0xfe, 0x8a, 0x1b,
        0x24, 0x49, 0x5e, 0xc1, 0x60, 0xdf, 0x2d, 0xa9, 0xb2, 0x0a, 0x7b, 0xa2, 0xf2, 0x6d, 0xfa, 0x2a, 0x44, 0x36, 0x6d, 0xbc,
        0x63, 0xde, 0x5c, 0xd7, 0xd7, 0xc9, 0x4c, 0x57, 0x17, 0x2f, 0xe6, 0xd7, 0x9c, 0x90, 0x1f, 0x02, 0x5c, 0x00, 0x10, 0xb0,
        0x2c, 0x89, 0xb3, 0x95, 0x40, 0x2c, 0x00, 0x9f, 0x62, 0xdc, 0x05, 0x3b, 0x80, 0x67, 0xa1, 0xe0, 0xed, 0x0a, 0x1e, 0x0c,
        0xf5, 0x08, 0x7d, 0x7f, 0x78, 0xcb, 0xd9, 0x4a, 0xfe, 0x0c, 0x3d, 0xd5, 0x5d, 0x2d, 0x4b, 0x1a, 0x5c, 0xfe, 0x2b, 0x68,
        0xb8, 0x62, 0x64, 0xe3, 0x51, 0xd1, 0xdc, 0xd8, 0x58, 0x78, 0x3a, 0x24, 0x0f, 0x89, 0x3f, 0x00, 0x8c, 0xee, 0xd7, 0x43,
        0xd9, 0x69, 0xb8, 0xf7, 0x35, 0xa1, 0x67, 0x7e, 0xad, 0x96, 0x0b, 0x1f, 0xb1, 0xec, 0xc5, 0xac, 0x83, 0xc2, 0x73, 0xb4,
        0x92, 0x88, 0xd0, 0x2d, 0x72, 0x86, 0x20, 0x7e, 0x66, 0x3c, 0x45, 0xe1, 0xa7, 0xba, 0xf5, 0x06, 0x40, 0xc9, 0x1e, 0x76,
        0x29, 0x41, 0xcf, 0x38, 0x0c, 0xe8, 0xd7, 0x9f, 0x3e, 0x86, 0x76, 0x7f, 0xbb, 0xcd, 0x25, 0xb4, 0x2e, 0xf7, 0x0e, 0xc3,
        0x34, 0x83, 0x5a, 0x3a, 0x6d, 0x79, 0x2e, 0x17, 0x0a, 0x43, 0x2c, 0xe0, 0xcb, 0x7b, 0xde, 0x9a, 0xaa, 0x1e, 0x75, 0x63,
        0x7c, 0x1c, 0x34, 0xae, 0x5f, 0xef, 0x43, 0x38, 0xf5, 0x3d, 0xb8, 0xb1, 0x3a, 0x4d, 0x2d, 0xf5, 0x94, 0xef, 0xbf, 0xa0,
        0x87, 0x84, 0x54, 0x38, 0x15, 0xc9, 0xc0, 0xd4, 0x87, 0xbd, 0xdf, 0xa1, 0x53, 0x9b, 0xc2, 0x52, 0xcf, 0x43, 0xec, 0x36,
        0x86, 0xe9, 0x80, 0x2d, 0x65, 0x1c, 0xfd, 0x2a, 0x82, 0x9a, 0x06, 0xa9, 0xf3, 0x32, 0xa7, 0x33, 0xa4, 0xa8, 0xae, 0xd8,
        0x0e, 0xfe, 0x34, 0x78, 0x09, 0x3f, 0xbc, 0x69, 0xc8, 0x60, 0x81, 0x46, 0xb3, 0xf1, 0x6f, 0x1a, 0x5c, 0x4e, 0xac, 0x93,
        0x20, 0xda, 0x49, 0xf1, 0xaf, 0xa5, 0xf5, 0x38, 0xdd, 0xec, 0xbb, 0xe7, 0x88, 0x8f, 0x43, 0x55, 0x12, 0xd0, 0xdd, 0x74,
        0xfd, 0x9b, 0x8c, 0x99, 0xe3, 0x14, 0x5b, 0xa8, 0x44, 0x10, 0xd8, 0xca, 0x9a, 0x36, 0xdd, 0x88, 0x41, 0x09, 0xe7, 0x6e,
        0x5f, 0xb8, 0x22, 0x2a, 0x52, 0xe1, 0x47, 0x3d, 0xa1, 0x68, 0x51, 0x9c, 0xe7, 0xa8, 0xa3, 0xc3, 0x2e, 0x91, 0x49, 0x67,
        0x1b, 0x16, 0x72, 0x4c, 0x6c, 0x5c, 0x51, 0xbb, 0x5c, 0xd6, 0x4f, 0xb5, 0x91, 0xe5, 0x67, 0xfb, 0x78, 0xb1, 0x0f, 0x9f,
        0x6f, 0xee, 0x62, 0xc2, 0x76, 0xf2, 0x82, 0xa7, 0xdf, 0x6b, 0xcf, 0x7c, 0x17, 0x74, 0x7b, 0xc9, 0xa8, 0x1e, 0x6c, 0x9c,
        0x3b, 0x03, 0x2f, 0xdd, 0x0e, 0x1c, 0x3a, 0xc9, 0xea, 0xa5, 0x07, 0x7d, 0xe3, 0xde, 0xd1, 0x8b, 0x2e, 0xd4, 0xfa, 0xf3,
        0x28, 0xf4, 0x98, 0x75, 0xaf, 0x2e, 0x36, 0xad, 0x5c, 0xe5, 0xf6, 0xcc, 0x99, 0xef, 0x4b, 0x60, 0xe5, 0x7b, 0x3b, 0x5b,
        0x9c, 0x9f, 0xcb, 0xcd, 0x4c, 0xfb, 0x39, 0x75, 0xe7, 0x0c, 0xe4, 0xc2, 0x50, 0x6b, 0xcd, 0x71, 0xfe, 0xf0, 0xe5, 0x35,
        0x92, 0x46, 0x15, 0x04, 0xe3, 0xd4, 0x2c, 0x88, 0x5c, 0xaa, 0xb2, 0x1b, 0x78, 0x2e, 0x26, 0x29, 0x4c, 0x6a, 0x9d, 0x61,
        0x11, 0x8c, 0xc4, 0x0a, 0x26, 0xf3, 0x78, 0x44, 0x1c, 0xeb, 0x48, 0xf3, 0x1a, 0x36, 0x2b, 0xf8, 0x50, 0x2a, 0x72, 0x3a,
        0x36, 0xc6, 0x35, 0x02, 0x22, 0x9a, 0x46, 0x2c, 0xc2, 0xa3, 0x79, 0x62, 0x79, 0xa5, 0xe3, 0xa7, 0xf8, 0x1a, 0x68, 0xc7,
        0xf8, 0x13, 0x12, 0xc3, 0x81, 0xcc, 0x16, 0xa4, 0xab, 0x03, 0x51, 0x3a, 0x51, 0xad, 0x5b, 0x54, 0x30, 0x6e, 0xc1, 0xd7,
        0x8a, 0x5e, 0x47, 0xe2, 0xb1, 0x5e, 0x5b, 0x7a, 0x14, 0x38, 0xe5, 0xb8, 0xb2, 0x88, 0x2d, 0xbd, 0xad, 0x13, 0xd6, 0xa4,
        0xa8, 0xc3, 0x55, 0x8c, 0xae, 0x04, 0x35, 0x01, 0xb6, 0x8e, 0xb3, 0xb0, 0x40, 0x06, 0x71, 0x52, 0x33, 0x7c, 0x05, 0x1c,
        0x40, 0xb5, 0xaf, 0x80, 0x9a, 0xca, 0x28, 0x56, 0x98, 0x6f, 0xd1, 0xc8, 0x6a, 0x4a, 0xde, 0x17, 0xd2, 0x54, 0xb6, 0x26,
        0x2a, 0xc1, 0xbc, 0x07, 0x73, 0x43, 0xb5, 0x2b, 0xf8, 0x9f, 0xa2, 0x7d, 0x73, 0xe3, 0xc6, 0xf3, 0x11, 0x8c, 0x99, 0x61,
        0xf0, 0xbe, 0xbe, 0x68, 0xa5, 0xc3, 0x23, 0xc2, 0xd8, 0x4b, 0x8c, 0x29, 0xa2, 0x80, 0x7d, 0xf6, 0x63, 0x63, 0x52, 0x23,
        0x24, 0x2a, 0x2c, 0xe9, 0x82, 0x8d, 0x44, 0x29, 0xac, 0x27, 0x0a, 0xab, 0x5f, 0x18, 0x41, 0xe8, 0xe4, 0x9c, 0xf4, 0x33,
        0xb1, 0x54, 0x79, 0x89, 0xf4, 0x19, 0xca, 0xa3, 0xc7, 0x58, 0xff, 0xf9, 0x6d, 0xed, 0x40, 0xcf, 0x34, 0x27, 0xf0, 0x76,
        0x1b, 0x67, 0x8d, 0xaa, 0x1a, 0x9e, 0x55, 0x54, 0x46, 0x5d, 0x46, 0xb7, 0xa9, 0x17, 0x49, 0x3f, 0xc7, 0x0f, 0x9e, 0xc5,
        0xe4, 0xe5, 0xd7, 0x86, 0xca, 0x50, 0x17, 0x30, 0x89, 0x8a, 0xaa, 0x11, 0x51, 0xdc, 0xd3, 0x18, 0x29, 0x64, 0x1e, 0x29,
        0x42, 0x8d, 0x90, 0xe6, 0x06, 0x55, 0x11, 0xc2, 0x4d, 0x31, 0x09, 0xf7, 0xcb, 0xa3, 0x22, 0x25, 0xd4, 0xac, 0xcf, 0xc5,
        0x4f, 0xec, 0x42, 0xb7, 0x33, 0xf9, 0x58, 0x52, 0x52, 0xee, 0x36, 0xfa, 0x5e, 0xa0, 0xc6, 0x56, 0x93, 0x43, 0x85, 0xb4,
        0x68, 0xee, 0xe2, 0x45, 0x31, 0x51, 0x46, 0xb8, 0xc0, 0x47, 0xed, 0x27, 0xc5, 0x19, 0xb2, 0xc0, 0xa5, 0x2d, 0x33, 0xef,
        0xe7, 0x2c, 0x18, 0x6f, 0xfe, 0x0a, 0x23, 0x0f, 0x50, 0x56, 0x76, 0xc5, 0x32, 0x4b, 0xaa, 0x6a, 0xe0, 0x06, 0xa7, 0x3e,
        0x13, 0xaa, 0x8c, 0x39, 0xab, 0x17, 0x3a, 0xd2, 0xb2, 0x77, 0x8e, 0xea, 0x0b, 0x34, 0xc4, 0x6f, 0x2b, 0x3b, 0xea, 0xe2,
        0xc6, 0x2a, 0x2c, 0x8d, 0xb2, 0x38, 0xbf, 0x58, 0xfc, 0x7c, 0x27, 0xbd, 0xce, 0xb9, 0x6c, 0x56, 0xd2, 0x9d, 0xee, 0xc8,
        0x7c, 0x12, 0x35, 0x1b, 0xfd, 0x59, 0x62, 0x49, 0x74, 0x18, 0x71, 0x6a, 0x4b, 0x91, 0x5d, 0x33, 0x4f, 0xfb, 0x5b, 0x92,
        0xca, 0x94, 0xff, 0xe1, 0xe4, 0xf7, 0x89, 0x67, 0x04, 0x26, 0x38, 0x63, 0x9a, 0x9d, 0xe3, 0x25, 0x35, 0x7f, 0x5f, 0x08,
        0xf6, 0x43, 0x50, 0x61, 0xe5, 0xa2, 0x74, 0x70, 0x39, 0x36, 0xc0, 0x6f, 0xc5, 0x6a, 0xf9, 0x2c, 0x42, 0x07, 0x97, 0x49,
        0x9c, 0xa4, 0x31, 0xa7, 0xab, 0xaa, 0x46, 0x18, 0x63, 0xbc, 0xa6, 0x56, 0xfa, 0xcf, 0xad, 0x56, 0x4e, 0x62, 0x74, 0xd4,
        0xa7, 0x41, 0x03, 0x3a, 0xca, 0x1e, 0x31, 0xbf, 0x63, 0x20, 0x0d, 0xf4, 0x1c, 0xdf, 0x41, 0xc1, 0x0b, 0x91, 0x2b, 0xec};
    quicly_decoded_packet_t packet;
    struct st_quicly_cipher_context_t ingress, egress;
    uint64_t pn, next_expected_pn = 0;
    ptls_iovec_t payload;
    int ret;

    ok(quicly_decode_packet(&packet, datagram, sizeof(datagram), 0) == sizeof(datagram));
    ret = setup_initial_encryption(&ingress, &egress, ptls_openssl_cipher_suites, packet.cid.dest, 0);
    ok(ret == 0);
    payload = decrypt_packet(ingress.header_protection, &ingress.aead, &next_expected_pn, &packet, &pn);
    ok(payload.base != NULL);
    ok(pn == 2);
    ok(sizeof(expected_payload) <= payload.len);
    ok(memcmp(expected_payload, payload.base, sizeof(expected_payload)) == 0);

    dispose_cipher(&ingress);
    dispose_cipher(&egress);
}

void free_packets(quicly_datagram_t **packets, size_t cnt)
{
    size_t i;
    for (i = 0; i != cnt; ++i)
        quicly_default_free_packet(&quic_ctx, packets[i]);
}

size_t decode_packets(quicly_decoded_packet_t *decoded, quicly_datagram_t **raw, size_t cnt, size_t host_cidl)
{
    size_t ri, dc = 0;

    for (ri = 0; ri != cnt; ++ri) {
        size_t off = 0;
        do {
            size_t dl = quicly_decode_packet(decoded + dc, raw[ri]->data.base + off, raw[ri]->data.len - off, host_cidl);
            assert(dl != SIZE_MAX);
            ++dc;
            off += dl;
        } while (off != raw[ri]->data.len);
    }

    return dc;
}

int buffer_is(ptls_buffer_t *buf, const char *s)
{
    return buf->off == strlen(s) && memcmp(buf->base, s, buf->off) == 0;
}

size_t transmit(quicly_conn_t *src, quicly_conn_t *dst)
{
    quicly_datagram_t *datagrams[32];
    size_t num_datagrams, i;
    quicly_decoded_packet_t decoded[32];
    int ret;

    num_datagrams = sizeof(datagrams) / sizeof(datagrams[0]);
    ret = quicly_send(src, datagrams, &num_datagrams);
    ok(ret == 0);

    if (num_datagrams != 0) {
        size_t num_packets = decode_packets(decoded, datagrams, num_datagrams, quicly_is_client(dst) ? 0 : 8);
        for (i = 0; i != num_packets; ++i) {
            ret = quicly_receive(dst, decoded + i);
            ok(ret == 0 || ret == QUICLY_ERROR_PACKET_IGNORED);
        }
        free_packets(datagrams, num_datagrams);
    }

    return num_datagrams;
}

int max_data_is_equal(quicly_conn_t *client, quicly_conn_t *server)
{
    uint64_t client_sent, client_consumed;
    uint64_t server_sent, server_consumed;

    quicly_get_max_data(client, NULL, &client_sent, &client_consumed);
    quicly_get_max_data(server, NULL, &server_sent, &server_consumed);

    if (client_sent != server_consumed)
        return 0;
    if (server_sent != client_consumed)
        return 0;

    return 1;
}

static void test_next_packet_number(void)
{
    /* prefer lower in case the distance in both directions are equal; see https://github.com/quicwg/base-drafts/issues/674 */
    uint64_t n = quicly_determine_packet_number(0xc0, 0xff, 0x140);
    ok(n == 0xc0);
    n = quicly_determine_packet_number(0xc0, 0xff, 0x141);
    ok(n == 0x1c0);
    n = quicly_determine_packet_number(0x9b32, 0xffff, 0xa82f30eb);
    ok(n == 0xa82f9b32);
}

int main(int argc, char **argv)
{
    static ptls_iovec_t cert;
    static ptls_openssl_sign_certificate_t cert_signer;
    static ptls_context_t tlsctx = {ptls_openssl_random_bytes,
                                    &ptls_get_time,
                                    ptls_openssl_key_exchanges,
                                    ptls_openssl_cipher_suites,
                                    {&cert, 1},
                                    NULL,
                                    NULL,
                                    NULL,
                                    &cert_signer.super,
                                    NULL,
                                    0,
                                    0,
                                    NULL,
                                    1};
    quic_ctx = quicly_default_context;
    quic_ctx.tls = &tlsctx;
    quic_ctx.transport_params.max_streams_bidi = 10;
    quic_ctx.on_stream_open = on_stream_open;
    quic_ctx.now = get_now;

    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
#if !defined(OPENSSL_NO_ENGINE)
    /* Load all compiled-in ENGINEs */
    ENGINE_load_builtin_engines();
    ENGINE_register_all_ciphers();
    ENGINE_register_all_digests();
#endif

    {
        BIO *bio = BIO_new_mem_buf(RSA_CERTIFICATE, strlen(RSA_CERTIFICATE));
        X509 *x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
        assert(x509 != NULL || !!"failed to load certificate");
        BIO_free(bio);
        cert.len = i2d_X509(x509, &cert.base);
        X509_free(x509);
    }

    {
        BIO *bio = BIO_new_mem_buf(RSA_PRIVATE_KEY, strlen(RSA_PRIVATE_KEY));
        EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
        assert(pkey != NULL || !"failed to load private key");
        BIO_free(bio);
        ptls_openssl_init_sign_certificate(&cert_signer, pkey);
        EVP_PKEY_free(pkey);
    }

    quicly_amend_ptls_context(quic_ctx.tls);

    subtest("next-packet-number", test_next_packet_number);
    subtest("ranges", test_ranges);
    subtest("frame", test_frame);
    subtest("maxsender", test_maxsender);
    subtest("sentmap", test_sentmap);
    subtest("test-vector", test_vector);
    subtest("simple", test_simple);
    subtest("stream-concurrency", test_stream_concurrency);
    subtest("loss", test_loss);

    return done_testing();
}