#include "ssq/packet.h"

#include "ssq/buffer.h"
#include "ssq/errops.h"

#include <stdlib.h>
#include <string.h>

static void ssq_packet_init_payload(
    struct ssq_packet   *const packet,
    struct ssq_buf      *const buf,
    struct ssq_error    *const err
)
{
    packet->payload = malloc(packet->size);

    if (packet->payload != NULL)
        ssq_buf_read(buf, packet->payload, packet->size);
    else
        ssq_error_set_sys(err);
}

static void ssq_packet_init_single(
    struct ssq_packet   *const packet,
    struct ssq_buf      *const buf,
    const uint16_t             size,
    struct ssq_error    *const err
)
{
    packet->number = 0;
    packet->total  = 1;
    packet->size   = size - ssq_buf_pos(buf);

    ssq_packet_init_payload(packet, buf, err);
}

static void ssq_packet_init_multi(
    struct ssq_packet   *const packet,
    struct ssq_buf      *const buf,
    struct ssq_error    *const err
)
{
    packet->id     = ssq_buf_read_int32(buf);
    packet->total  = ssq_buf_read_uint8(buf);
    packet->number = ssq_buf_read_uint8(buf);
    packet->size   = ssq_buf_read_uint16(buf);

    if (packet->id & 0x80000000)
        ssq_error_set(err, SSQ_ERR_UNSUPPORTED, "Packet decompression is not supported");
    else
        ssq_packet_init_payload(packet, buf, err);
}

struct ssq_packet *ssq_packet_init(
    const char                 data[],
    const uint16_t             size,
    struct ssq_error    *const err
)
{
    struct ssq_packet *packet = malloc(sizeof (*packet));

    if (packet != NULL)
    {
        memset(packet, 0, sizeof (*packet));

        struct ssq_buf buf;
        ssq_buf_init(&buf, data, size);

        packet->header = ssq_buf_read_int32(&buf);

        if (packet->header == A2S_PACKET_HEADER_SINGLE)
            ssq_packet_init_single(packet, &buf, size, err);
        else if (packet->header == A2S_PACKET_HEADER_MULTI)
            ssq_packet_init_multi(packet, &buf, err);
        else
            ssq_error_set(err, SSQ_ERR_BADRES, "Invalid packet header");

        if (err->code != SSQ_OK)
        {
            free(packet->payload);
            free(packet);
            packet = NULL;
        }
    }
    else
    {
        ssq_error_set_sys(err);
    }

    return packet;
}

static size_t ssq_packet_gettotalsize(struct ssq_packet *const packets[], const uint8_t packet_count)
{
    size_t total_size = 0;

    for (uint8_t i = 0; i < packet_count; ++i)
        total_size += packets[i]->size;

    return total_size;
}

char *ssq_packet_mergepayloads(
    struct ssq_packet *const packets[],
    const uint8_t            packet_count,
    size_t            *const size,
    struct ssq_error  *const err
)
{
    *size = ssq_packet_gettotalsize(packets, packet_count);

    char *merged = calloc(*size, sizeof (*merged));

    if (merged != NULL)
    {
        size_t offset = 0;

        for (uint8_t i = 0; i < packet_count; ++i)
        {
            if (packets[i]->id != packets[0]->id)
            {
                ssq_error_set(err, SSQ_ERR_BADRES, "Packet IDs mismatch");

                free(merged);
                merged = NULL;

                break;
            }
            else
            {
                memcpy(merged + offset, packets[i]->payload, packets[i]->size);
                offset += packets[i]->size;
            }
        }
    }
    else
    {
        ssq_error_set_sys(err);
    }

    return merged;
}

void ssq_packet_free(struct ssq_packet *const packet)
{
    free(packet->payload);
    free(packet);
}

void ssq_packet_freearr(struct ssq_packet *packets[], const uint8_t packet_count)
{
    for (uint8_t i = 0; i < packet_count; ++i)
    {
        if (packets[i] != NULL)
        {
            ssq_packet_free(packets[i]);
        }
    }

    free(packets);
}
