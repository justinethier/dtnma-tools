/*
 * Copyright (c) 2011-2023 The Johns Hopkins University Applied Physics
 * Laboratory LLC.
 *
 * This file is part of the Delay-Tolerant Networking Management
 * Architecture (DTNMA) Tools package.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "ari_text_util.h"
#include "util.h"
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <stdlib.h>
#include <math.h>

/// Type for timespec::tv_nsec
typedef unsigned long subsec_t;

/** Get the size of text ignoring a terminating null.
 */
static size_t text_real_len(const ari_data_t *data)
{
    if (!(data->ptr) || (data->len == 0))
    {
        return 0;
    }
    const uint8_t last = data->ptr[data->len - 1];
    return (last == 0) ? data->len - 1 : data->len;
}

static int take_hex_1byte(uint8_t *out, const char **curs, const char *end)
{
    if (*curs + 2 > end)
    {
        return 1;
    }

    char  buf[] = { *((*curs)++), *((*curs)++), 0 };
    char *numend;
    *out = strtoul(buf, &numend, 16);
    if (numend < buf + 2)
    {
        return 2;
    }
    return 0;
}

static int take_hex_2byte(uint16_t *out, const char **curs, const char *end)
{
    if (*curs + 4 > end)
    {
        return 1;
    }

    char  buf[] = { *((*curs)++), *((*curs)++), *((*curs)++), *((*curs)++), 0 };
    char *numend;
    *out = strtoul(buf, &numend, 16);
    if (numend < buf + 4)
    {
        return 2;
    }
    return 0;
}

static const char id_text_first[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";
static const char id_text_rest[]  = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-";

bool ari_text_is_identity(const ari_data_t *text)
{
    CHKFALSE(text);
    const size_t in_len = text_real_len(text);
    const char  *curs   = (const char *)(text->ptr);
    if (in_len < 1)
    {
        return false;
    }

    if (strchr(id_text_first, *curs) == NULL)
    {
        return false;
    }
    ++curs;
    if (strspn(curs, id_text_rest) < in_len - 1)
    {
        return false;
    }
    return true;
}

/** Set of unreserved characters from Section 2.3 of RFC 3986 @cite rfc3986.
 */
static const char *unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-~";

int uri_percent_encode(string_t out, const ari_data_t *in, const char *safe)
{
    CHKERR1(out);
    CHKERR1(in);

    const size_t in_len = text_real_len(in);
    const char  *curs   = (const char *)(in->ptr);
    const char  *end    = curs + in_len;

    string_t allsafe;
    string_init(allsafe);
    string_cat_str(allsafe, unreserved);
    string_cat_str(allsafe, safe);

    // assume no more than half of the input chars are escaped,
    // which gives total output size of: 0.5 + 0.5 * 3 => 2
    string_reserve(out, string_size(out) + 2 * in_len);

    int retval = 0;
    while (curs < end)
    {
        const size_t partlen = strspn(curs, string_get_cstr(allsafe));

        if (partlen)
        {
            string_cat_printf(out, "%.*s", partlen, curs);
        }
        curs += partlen;

        if (curs >= end)
        {
            // no unsafe char and no more text
            break;
        }

        const uint8_t chr = *(curs++);
        string_cat_printf(out, "%%%02X", chr);
    }

    string_clear(allsafe);
    return retval;
}

int uri_percent_decode(string_t out, const ari_data_t *in)
{
    CHKERR1(out);
    CHKERR1(in);

    const size_t in_len = text_real_len(in);
    const char  *curs   = (const char *)(in->ptr);
    const char  *end    = curs + in_len;

    // potentially no escaping used
    string_reserve(out, string_size(out) + in_len);

    while (curs < end)
    {
        const char *partend = strchr(curs, '%');
        if (partend == NULL)
        {
            partend = end;
        }
        const size_t partlen = partend - curs;

        if (partlen)
        {
            string_cat_printf(out, "%.*s", partlen, curs);
        }
        curs += partlen + 1;

        if (curs > end)
        {
            // no percent and no more text
            break;
        }

        // cursor is on the percent char
        uint8_t val;
        if (take_hex_1byte(&val, &curs, end))
        {
            return 2;
        }
        string_push_back(out, val);
    }

    return 0;
}

int ari_uint64_encode(string_t out, uint64_t value, int base)
{
    CHKERR1(out);

    if (base == 2)
    {
        // reverse-order holding buffer
        char  tmp[64];
        char *end  = tmp + 64;
        char *curs = end;
        while (value)
        {
            *(--curs) = ((value % 2 == 1) ? '1' : '0');
            value /= 2;
        }
        if (curs == end)
        {
            // ensure at least one digit
            *(--curs) = '0';
        }

        string_reserve(out, string_size(out) + 2 + (end - curs));
        string_push_back(out, '0');
        string_push_back(out, 'b');
        for (; curs < end; ++curs)
        {
            string_push_back(out, *curs);
        }
    }
    else
    {
        const char *fmt;
        if (base == 10)
        {
            fmt = "%" PRIu64;
        }
        else if (base == 16)
        {
            fmt = "0x%" PRIX64;
        }
        else
        {
            return 1;
        }
        string_cat_printf(out, fmt, value);
    }

    return 0;
}

int ari_uint64_decode(uint64_t *out, const string_t in)
{
    CHKERR1(out);
    CHKERR1(in);

    const char  *begin  = string_get_cstr(in);
    const size_t in_len = string_size(in);
    const char  *end    = begin + in_len;

    uint64_t tmp;
    if ((in_len >= 2) && (string_get_char(in, 0) == '0') && (string_get_char(in, 1) == 'b'))
    {
        tmp = 0;
        for (size_t ix = 2; ix < in_len; ++ix)
        {
            tmp <<= 1;

            const char bit = string_get_char(in, ix);
            switch (bit)
            {
                case '0':
                    // nothing
                    break;
                case '1':
                    tmp += 1;
                    break;
                default:
                    return 2;
            }
        }
    }
    else
    {
        char *numend;
        tmp = strtoull(begin, &numend, 0);
        if (numend != end)
        {
            return 2;
        }
    }

    *out = tmp;
    return 0;
}

int ari_float64_encode(string_t out, double value, char form)
{
    if (isnan(value))
    {
        string_cat_str(out, "NaN");
    }
    else if (isinf(value))
    {
        string_push_back(out, (value < 0) ? '-' : '+');
        string_cat_str(out, "Infinity");
    }
    else if (form == 'f')
    {
        string_cat_printf(out, "%f", value);
    }
    else if (form == 'g')
    {
        string_cat_printf(out, "%g", value);
    }
    else if (form == 'e')
    {
        string_cat_printf(out, "%e", value);
    }
    else if (form == 'a')
    {
        string_cat_printf(out, "%a", value);
    }
    else
    {
        return 1;
    }

    return 0;
}

static int subsec_decode(subsec_t *subsec, const char **curs, const char *const end)
{
    if (*curs == end)
    {
        // nothing to do
        return 0;
    }

    if (**curs != '.')
    {
        return 2;
    }
    ++*curs;

    if (*curs < end)
    {
        char *subend;
        *subsec = strtoul(*curs, &subend, 10);
        if (subend == *curs)
        {
            return 2;
        }
        size_t digits = subend - *curs;
        if (digits > 9)
        {
            return 4;
        }
        for (; digits < 9; ++digits)
        {
            *subsec *= 10;
        }

        *curs = subend;
    }
    return 0;
}

int subsec_encode(string_t out, subsec_t subsec)
{
    if (!subsec)
    {
        return 0;
    }

    // trim trailing zeros
    int digits = 9;
    while (subsec % 10 == 0)
    {
        subsec /= 10;
        --digits;
    }
    string_cat_printf(out, ".%.*u", digits, subsec);
    return 0;
}

int decfrac_encode(string_t out, const struct timespec *in)
{
    CHKERR1(in);

    string_cat_printf(out, "%" PRId64, in->tv_sec);
    subsec_encode(out, in->tv_nsec);

    return 0;
}

int decfrac_decode(struct timespec *out, const ari_data_t *in)
{
    CHKERR1(out);
    CHKERR1(in);

    const size_t in_len = text_real_len(in);
    const char  *curs   = (const char *)(in->ptr);
    const char  *end    = curs + in_len;
    char        *subend;

    time_t sec_num = strtoll(curs, &subend, 10);
    if (subend == curs)
    {
        return 2;
    }
    curs = subend;

    // extract subseconds as nanoseconds
    subsec_t subsec = 0;
    if (subsec_decode(&subsec, &curs, end))
    {
        return 2;
    }

    // extra text
    if (curs < end)
    {
        return 3;
    }

    out->tv_sec  = sec_num;
    out->tv_nsec = subsec;
    return 0;
}

bool ari_data_is_utf8(const ari_data_t *in)
{
    CHKFALSE(in);
    CHKFALSE(in->ptr);
    CHKFALSE(in->len > 0);

    // only and exactly is the last byte is zero
    if (memchr(in->ptr, 0x0, in->len) != (in->ptr + in->len - 1))
    {
        return false;
    }

    return m_str1ng_utf8_valid_str_p((const char *)(in->ptr));
}

int slash_escape(string_t out, const ari_data_t *in, const char quote)
{
    CHKERR1(out);
    CHKERR1(in);

    string_t in_text;
    if (in->len == 0)
    {
        // fine, but nothing to do
        return 0;
    }
    string_init_set_str(in_text, (const char *)(in->ptr));

    // unicode iterator
    string_it_t it;
    for (string_it(it, in_text); !string_end_p(it); string_next(it))
    {
        const m_string_unicode_t *chr = string_cref(it);
        if (*chr == (m_string_unicode_t)quote)
        {
            string_push_back(out, '\\');
            string_push_back(out, quote);
        }
        else if (*chr == 0x08)
        {
            string_cat_str(out, "\\b");
        }
        else if (*chr == 0x0C)
        {
            string_cat_str(out, "\\f");
        }
        else if (*chr == 0x0A)
        {
            string_cat_str(out, "\\n");
        }
        else if (*chr == 0x0D)
        {
            string_cat_str(out, "\\r");
        }
        else if (*chr == 0x09)
        {
            string_cat_str(out, "\\t");
        }
        else if ((*chr <= 0xFF) && isprint(*chr))
        {
            string_push_u(out, *chr);
        }
        else if ((*chr <= 0xD7FF) || ((*chr >= 0xE000) && (*chr <= 0xFFFF)))
        {
            const uint16_t uprime = *chr;
            string_cat_printf(out, "\\u%04" PRIX16, uprime);
        }
        else
        {
            // surrogate pair creation
            const uint32_t uprime = *chr - 0x10000;
            const uint16_t high   = 0xD800 + (uprime >> 10);
            const uint16_t low    = 0xDC00 + (uprime & 0x03FF);
            string_cat_printf(out, "\\u%04" PRIX16 "\\u%04" PRIX16, high, low);
        }
    }
    return 0;
}

int slash_unescape(string_t out, const ari_data_t *in)
{
    CHKERR1(out);
    CHKERR1(in);

    ari_data_t   tmp;
    const size_t in_len = text_real_len(in);
    if (in_len == 0)
    {
        return 0;
    }
    else if (in_len == in->len)
    {
        // no terminating null
        ari_data_init(&tmp);
        ari_data_copy(&tmp, in);
        ari_data_append_byte(&tmp, '\0');
    }
    else
    {
        // direct access
        ari_data_init_view(&tmp, in->len, in->ptr);
    }

    // potentially no escaping used
    string_reserve(out, string_size(out) + in_len);

    const char *curs   = (const char *)(tmp.ptr);
    const char *end    = curs + in_len;
    int         retval = 0;
    while (curs < end)
    {
        const char *partend = strchr(curs, '\\');
        if (partend == NULL)
        {
            partend = end;
        }
        const size_t partlen = partend - curs;

        if (partlen)
        {
            string_cat_printf(out, "%.*s", partlen, curs);
        }
        curs += partlen + 1;

        if (curs > end)
        {
            // no backslash and no more text
            break;
        }
        else if (curs == end)
        {
            // backslash with no trailing character
            retval = 3;
            break;
        }

        if (*curs == 'b')
        {
            string_push_back(out, 0x08);
            curs += 1;
        }
        else if (*curs == 'f')
        {
            string_push_back(out, 0x0C);
            curs += 1;
        }
        else if (*curs == 'n')
        {
            string_push_back(out, 0x0A);
            curs += 1;
        }
        else if (*curs == 'r')
        {
            string_push_back(out, 0x0D);
            curs += 1;
        }
        else if (*curs == 't')
        {
            string_push_back(out, 0x09);
            curs += 1;
        }
        else if (*curs == 'u')
        {
            ++curs;

            uint16_t val;
            if (take_hex_2byte(&val, &curs, end))
            {
                retval = 5;
                break;
            }

            m_string_unicode_t unival;
            if ((val >= 0xD800) && (val <= 0xDFFF))
            {
                // surrogate pair removal
                unival = (val - 0xD800) << 10;

                if (curs + 2 >= end)
                {
                    retval = 5;
                    break;
                }
                if (*curs != '\\')
                {
                    retval = 5;
                    break;
                }
                ++curs;
                if (*curs != 'u')
                {
                    retval = 5;
                    break;
                }
                ++curs;

                if (take_hex_2byte(&val, &curs, end))
                {
                    retval = 5;
                    break;
                }
                unival |= val - 0xDC00;

                unival += 0x10000;
            }
            else
            {
                unival = val;
            }

            string_push_u(out, unival);
        }
        else
        {
            string_push_back(out, *curs);
            curs += 1;
        }
    }

    ari_data_deinit(&tmp);
    return retval;
}

static void strip_chars(string_t out, const char *in, size_t in_len, const char *chars)
{
    const char *curs = in;
    const char *end  = curs + in_len;
    size_t      plen;

    // likely no removal
    string_reserve(out, in_len);

    while (curs < end)
    {
        plen = strcspn(curs, chars);
        string_cat_printf(out, "%.*s", plen, curs);
        curs += plen;

        plen = strspn(curs, chars);
        curs += plen;
    }
}

void strip_space(string_t out, const char *in, size_t in_len)
{
    strip_chars(out, in, in_len, " \b\f\n\r\t");
}

int base16_encode(string_t out, const ari_data_t *in, bool uppercase)
{
    const char *fmt = uppercase ? "%02X" : "%02x";

    const uint8_t *curs = in->ptr;
    const uint8_t *end  = curs + in->len;
    for (; curs < end; ++curs)
    {
        string_cat_printf(out, fmt, *curs);
    }
    return 0;
}

/// Size of the base16_decode_table
static const size_t base16_decode_lim = 0x80;
// clang-format off
/// Decode table for base16
static const int base16_decode_table[0x80] =
{
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -2, -2, -1, -1, -2, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};
// clang-format on

/** Decode a single character.
 *
 * @param chr The character to decode.
 * @return If positive, the decoded value.
 * -1 to indicate error.
 * -2 to indicate whitespace.
 */
static int base16_decode_char(uint8_t chr)
{
    if (chr >= base16_decode_lim)
    {
        return -1;
    }
    return base16_decode_table[chr];
}

int base16_decode(ari_data_t *out, const string_t in)
{
    CHKERR1(out);
    CHKERR1(in);

    const size_t in_len = string_size(in);
    if (in_len % 2 != 0)
    {
        return 1;
    }
    const char *curs = string_get_cstr(in);
    const char *end  = curs + in_len;

    if (ari_data_resize(out, in_len / 2))
    {
        return 2;
    }
    uint8_t *out_curs = out->ptr;

    while (curs < end)
    {
        const int high = base16_decode_char(*(curs++));
        const int low  = base16_decode_char(*(curs++));
        if ((high < 0) || (low < 0))
        {
            return 3;
        }

        const uint8_t byte = (high << 4) | low;
        *(out_curs++)      = byte;
    }
    return 0;
}

// clang-format off
static const char *base64_alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
static const char *base64url_alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789-_";
// clang-format on

int base64_encode(string_t out, const ari_data_t *in, bool useurl)
{
    size_t         in_len = in->len;
    const uint8_t *curs   = (const uint8_t *)(in->ptr);
    const uint8_t *end    = curs + in_len;

    const char *const abet = useurl ? base64url_alphabet : base64_alphabet;

    // output length is the ceiling of ratio 8/6
    size_t out_len = ((in_len + 2) / 3) * 4;
    string_reserve(out, string_size(out) + out_len);

    for (; curs < end; curs += 3)
    {
        uint8_t byte = (curs[0] >> 2) & 0x3F;
        char    chr  = abet[byte];
        string_push_back(out, chr);
        --in_len;
        if (--out_len == 0)
        {
            break;
        }

        byte = ((curs[0] << 4) | (in_len ? curs[1] >> 4 : 0)) & 0x3F;
        chr  = abet[byte];
        string_push_back(out, chr);
        if (--out_len == 0)
        {
            break;
        }

        if (in_len)
        {
            --in_len;
            byte = ((curs[1] << 2) | (in_len ? curs[2] >> 6 : 0)) & 0x3F;
            chr  = abet[byte];
        }
        else
        {
            chr = '=';
        }
        string_push_back(out, chr);
        if (--out_len == 0)
        {
            break;
        }

        if (in_len)
        {
            --in_len;
            byte = curs[2] & 0x3F;
            chr  = abet[byte];
        }
        else
        {
            chr = '=';
        }
        string_push_back(out, chr);
        if (--out_len == 0)
        {
            break;
        }
    }
    return 0;
}

/// Size of the base16_decode_table
static const size_t base64_decode_lim = 0x80;
// clang-format off
/// Decode table for base64 and base64uri
static const int base64_decode_table[0x80] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -2, -2, -2, -2, -2, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, 62, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, 63,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
};
// clang-format on

/** Decode a single character.
 *
 * @param chr The character to decode.
 * @return If positive, the decoded value.
 * -1 to indicate error.
 * -2 to indicate whitespace.
 */
static int base64_decode_char(uint8_t chr)
{
    if (chr >= base64_decode_lim)
    {
        return -1;
    }
    return base64_decode_table[chr];
}

int base64_decode(ari_data_t *out, const string_t in)
{
    CHKERR1(out);
    CHKERR1(in);

    size_t      in_len = string_size(in);
    const char *curs   = string_get_cstr(in);

    size_t out_len = (in_len / 4) * 3 + 2;
    if (ari_data_resize(out, out_len))
    {
        return 2;
    }
    uint8_t *out_curs = out->ptr;

    for (; in_len > 2; curs += 4, in_len -= 4)
    {
        // ignoring excess padding
        if (curs[0] == '=')
        {
            break;
        }

        const int seg0 = base64_decode_char(curs[0]);
        const int seg1 = base64_decode_char(curs[1]);
        if ((seg0 < 0) || (seg1 < 0))
        {
            return 3;
        }

        if (out_len)
        {
            const uint8_t byte = (seg0 << 2) | (seg1 >> 4);
            *(out_curs++)      = byte;
            --out_len;
        }

        if (in_len == 2)
        {
            break;
        }
        if (curs[2] == '=')
        {
            if (in_len != 4)
            {
                break;
            }
            if (curs[3] != '=')
            {
                break;
            }
        }
        else
        {
            const int seg2 = base64_decode_char(curs[2]);
            if (seg2 < 0)
            {
                return 3;
            }

            if (out_len)
            {
                const uint8_t byte = ((seg1 << 4) & 0xF0) | (seg2 >> 2);
                *(out_curs++)      = byte;
                --out_len;
            }

            if (in_len == 3)
            {
                break;
            }
            if (curs[3] == '=')
            {
                if (in_len != 4)
                {
                    break;
                }
            }
            else
            {
                const int seg3 = base64_decode_char(curs[3]);
                if (seg3 < 0)
                {
                    return 3;
                }

                if (out_len)
                {
                    const uint8_t byte = ((seg2 << 6) & 0xC0) | seg3;
                    *(out_curs++)      = byte;
                    --out_len;
                }
            }
        }
    }

    // trim if necessary
    ari_data_extend_back(out, -(ssize_t)out_len);

    // Per Section 3.3 of RFC 4648, ignoring excess padding
    while (*curs == '=')
    {
        ++curs;
        --in_len;
    }

    return (in_len > 0) ? 4 : 0;
}

/// POSIX time at 2000-01-01T00:00:00Z
static const time_t dtn_epoch = 946684800L;

int utctime_encode(string_t out, const struct timespec *in, bool usesep)
{
    CHKERR1(out);
    CHKERR1(in);

    struct tm parts = { 0 };
    {
        const time_t fullsecs = dtn_epoch + in->tv_sec;
        // do not apply local zone offset
        struct tm *got = gmtime_r(&fullsecs, &parts);
        if (!got)
        {
            return 2;
        }
    }

    {
        const char *fmt;
        if (usesep)
        {
            fmt = "%Y-%m-%dT%H:%M:%S";
        }
        else
        {
            fmt = "%Y%m%dT%H%M%S";
        }
        char fullsec[20]; // maximum with-separator size and trailing null
        strftime(fullsec, sizeof(fullsec), fmt, &parts);
        string_cat_str(out, fullsec);
    }

    subsec_encode(out, in->tv_nsec);

    string_push_back(out, 'Z');
    return 0;
}

int utctime_decode(struct timespec *out, const ari_data_t *in)
{
    CHKERR1(out);
    CHKERR1(in);

    const size_t in_len = text_real_len(in);
    const char  *curs   = (const char *)(in->ptr);
    const char  *end    = curs + in_len;

    // remove optional separators
    string_t unsep;
    string_init(unsep);
    strip_chars(unsep, curs, in_len, "-:");
    curs = string_get_cstr(unsep);
    end  = curs + string_size(unsep);

    // extract full seconds
    time_t fullsec;
    int    retval = 0;
    {
        struct tm parts = { 0 };
        // FIXME doesn't handle compressed forms
        const char *subend = strptime(curs, "%Y%m%dT%H%M%S", &parts);
        if (subend == NULL)
        {
            retval = 2;
            goto utctime_decode_cleanup;
        }
        curs = subend;

        {
            // work-around lack of portable timegm()
            const char *oldtz = getenv("TZ");
            setenv("TZ", "UTC", 1);
            fullsec = mktime(&parts);
            if (oldtz)
            {
                setenv("TZ", oldtz, 1);
            }
            else
            {
                unsetenv("TZ");
            }
        }
        if (fullsec < 0)
        {
            retval = 2;
            goto utctime_decode_cleanup;
        }
        fullsec -= dtn_epoch;
    }

    // extract subseconds as nanoseconds
    subsec_t subsec = 0;
    if (subsec_decode(&subsec, &curs, end - 1))
    {
        retval = 2;
        goto utctime_decode_cleanup;
    }

    if ((curs == end) || (*curs != 'Z'))
    {
        // no trailing zone
        retval = 2;
        goto utctime_decode_cleanup;
    }
    ++curs;

    // extra text
    if (curs < end)
    {
        retval = 3;
        goto utctime_decode_cleanup;
    }

    out->tv_sec  = fullsec;
    out->tv_nsec = subsec;

utctime_decode_cleanup:
    string_clear(unsep);
    return retval;
}

#define TIMEPERIOD_DAY    (24 * 3600)
#define TIMEPERIOD_HOUR   3600
#define TIMEPERIOD_MINUTE 60

int timeperiod_encode(string_t out, const struct timespec *in)
{
    CHKERR1(out);
    CHKERR1(in);

    if ((in->tv_sec == 0) && (in->tv_nsec == 0))
    {
        string_cat_str(out, "PT0S");
        return 0;
    }

    time_t   fullsec = in->tv_sec;
    subsec_t subsec  = in->tv_nsec;

    if (fullsec < 0)
    {
        string_push_back(out, '-');
        fullsec = -fullsec;
    }

    string_push_back(out, 'P');

    if (fullsec >= TIMEPERIOD_DAY)
    {
        const time_t part = fullsec / TIMEPERIOD_DAY;
        fullsec %= TIMEPERIOD_DAY;
        string_cat_printf(out, "%" PRId64 "D", part);
    }

    string_push_back(out, 'T');

    if (fullsec >= TIMEPERIOD_HOUR)
    {
        const time_t part = fullsec / TIMEPERIOD_HOUR;
        fullsec %= TIMEPERIOD_HOUR;
        string_cat_printf(out, "%" PRId64 "H", part);
    }

    if (fullsec >= TIMEPERIOD_MINUTE)
    {
        const time_t part = fullsec / TIMEPERIOD_MINUTE;
        fullsec %= TIMEPERIOD_MINUTE;
        string_cat_printf(out, "%" PRId64 "M", part);
    }

    if (fullsec || subsec)
    {
        string_cat_printf(out, "%" PRId64, fullsec);
        subsec_encode(out, subsec);
        string_push_back(out, 'S');
    }

    return 0;
}

int timeperiod_decode(struct timespec *out, const ari_data_t *in)
{
    CHKERR1(out);
    CHKERR1(in);

    const size_t in_len = text_real_len(in);
    const char  *curs   = (const char *)(in->ptr);
    const char  *end    = curs + in_len;
    const char  *partend;
    char        *numend;

    // first character is optional sign
    const bool sign_neg = (*curs == '-');
    if ((*curs == '-') || (*curs == '+'))
    {
        ++curs;
    }

    if (*curs != 'P')
    {
        return 2;
    }
    ++curs;

    // accumulate seconds
    time_t   fullsec = 0;
    subsec_t subsec  = 0;

    partend = strchr(curs, 'D');
    if (partend)
    {
        fullsec += TIMEPERIOD_DAY * strtoul(curs, &numend, 10);
        if (numend == curs)
        {
            return 2;
        }
        curs = partend + 1;
    }

    if (*curs != 'T')
    {
        return 2;
    }
    ++curs;

    partend = strchr(curs, 'H');
    if (partend)
    {
        fullsec += TIMEPERIOD_HOUR * strtoul(curs, &numend, 10);
        if (numend == curs)
        {
            return 2;
        }
        curs = partend + 1;
    }

    partend = strchr(curs, 'M');
    if (partend)
    {
        fullsec += TIMEPERIOD_MINUTE * strtoul(curs, &numend, 10);
        if (numend == curs)
        {
            return 2;
        }
        curs = partend + 1;
    }

    partend = strchr(curs, 'S');
    if (partend)
    {
        fullsec += strtoul(curs, &numend, 10);
        if (numend == curs)
        {
            return 2;
        }
        curs = numend;

        if (subsec_decode(&subsec, &curs, partend))
        {
            return 2;
        }

        curs = partend + 1;
    }

    // extra text
    if (curs < end)
    {
        return 3;
    }

    if (sign_neg)
    {
        fullsec = -fullsec;
    }

    out->tv_sec  = fullsec;
    out->tv_nsec = subsec;
    return 0;
}