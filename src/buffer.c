#include "ssq/buffer.h"

#include "ssq/utils.h"

#include <stdlib.h>
#include <string.h>

void ssq_buf_init(struct ssq_buf *const buf, const void *const data, const size_t size)
{
    buf->data = data;
    buf->pos  = 0;
    buf->size = size;
}

void ssq_buf_read(struct ssq_buf *const buf, void *const dst, size_t n)
{
    // clamp the size if it overflows the buffer
    if (buf->pos + n > buf->size)
        n = buf->size - buf->pos;

    memcpy(dst, buf->data + buf->pos, n);

    ssq_buf_forward(buf, n);
}

int8_t ssq_buf_read_int8(struct ssq_buf *const buf)
{
    int8_t value = 0;
    ssq_buf_read(buf, &value, sizeof (value));
    return value;
}

int16_t ssq_buf_read_int16(struct ssq_buf *const buf)
{
    int16_t value = 0;
    ssq_buf_read(buf, &value, sizeof (value));
    return value;
}

int32_t ssq_buf_read_int32(struct ssq_buf *const buf)
{
    int32_t value = 0;
    ssq_buf_read(buf, &value, sizeof (value));
    return value;
}

int64_t ssq_buf_read_int64(struct ssq_buf *const buf)
{
    int64_t value = 0;
    ssq_buf_read(buf, &value, sizeof (value));
    return value;
}

uint8_t ssq_buf_read_uint8(struct ssq_buf *const buf)
{
    uint8_t value = 0;
    ssq_buf_read(buf, &value, sizeof (value));
    return value;
}

uint16_t ssq_buf_read_uint16(struct ssq_buf *const buf)
{
    uint16_t value = 0;
    ssq_buf_read(buf, &value, sizeof (value));
    return value;
}

uint32_t ssq_buf_read_uint32(struct ssq_buf *const buf)
{
    uint32_t value = 0;
    ssq_buf_read(buf, &value, sizeof (value));
    return value;
}

uint64_t ssq_buf_read_uint64(struct ssq_buf *const buf)
{
    uint64_t value = 0;
    ssq_buf_read(buf, &value, sizeof (value));
    return value;
}

float ssq_buf_read_float(struct ssq_buf *const buf)
{
    float value = 0.0F;
    ssq_buf_read(buf, &value, sizeof (value));
    return value;
}

double ssq_buf_read_double(struct ssq_buf *const buf)
{
    double value = 0.0;
    ssq_buf_read(buf, &value, sizeof (value));
    return value;
}

long double ssq_buf_read_long_double(struct ssq_buf *const buf)
{
    long double value = 0.0L;
    ssq_buf_read(buf, &value, sizeof (value));
    return value;
}

bool ssq_buf_read_bool(struct ssq_buf *const buf)
{
    const int8_t value = ssq_buf_read_int8(buf);
    return value != 0;
}

/**
 * Determines the length of a null-terminated string in a buffer.
 * @param buf the buffer
 * @returns the length of the null-terminated string at the current position
 */
static size_t ssq_buf_strlen(const struct ssq_buf *const buf)
{
    const char *const str = buf->data + buf->pos;
    size_t str_len = 0;

    while (buf->pos + str_len < buf->size && str[str_len] != '\0')
        ++str_len;

    return str_len;
}

char *ssq_buf_read_string(struct ssq_buf *const buf)
{
    const size_t str_len = ssq_buf_strlen(buf);
    const size_t str_size = str_len + 1;

    const char *const src = buf->data + buf->pos;
    char       *const dst = calloc(str_size, sizeof (*dst));

    if (dst != NULL)
    {
        ssq_utils_strncpy(dst, src, str_len);
        ssq_buf_forward(buf, str_size * sizeof (*dst));
    }

    return dst;
}
