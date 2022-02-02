#include "ssq/a2s/info.h"

#include "ssq/buffer.h"
#include "ssq/errops.h"
#include "ssq/query.h"
#include "ssq/utils.h"

#include <stdlib.h>
#include <string.h>

#define PAYLOAD     "\xFF\xFF\xFF\xFF\x54Source Engine Query"
#define PAYLOAD_LEN 25

#define S2A_INFO    0x49

static struct a2s_info *ssq_info_deserialize(
    const char              response[],
    const size_t            response_len,
    struct ssq_error *const err
)
{
    struct a2s_info *info = NULL;

    struct ssq_buf buf;
    ssq_buf_init(&buf, response, response_len);

    if (ssq_utils_response_istruncated(response))
        ssq_buf_forward(&buf, sizeof (int32_t));

    const uint8_t header = ssq_buf_read_uint8(&buf);

    if (header == S2A_INFO)
    {
        info = malloc(sizeof (*info));

        if (info != NULL)
        {
            memset(info, 0, sizeof (*info));

            info->protocol    = ssq_buf_read_uint8(&buf);
            info->name        = ssq_buf_read_string(&buf);
            info->map         = ssq_buf_read_string(&buf);
            info->folder      = ssq_buf_read_string(&buf);
            info->game        = ssq_buf_read_string(&buf);
            info->id          = ssq_buf_read_uint16(&buf);
            info->players     = ssq_buf_read_uint8(&buf);
            info->max_players = ssq_buf_read_uint8(&buf);
            info->bots        = ssq_buf_read_uint8(&buf);

            const uint8_t server_type = ssq_buf_read_uint8(&buf);
            switch (server_type)
            {
                case 'd':
                    info->server_type = A2S_SERVER_TYPE_DEDICATED;
                    break;

                case 'p':
                    info->server_type = A2S_SERVER_TYPE_STV_RELAY;
                    break;

                default: // 'l'
                    info->server_type = A2S_SERVER_TYPE_NON_DEDICATED;
                    break;
            }

            const uint8_t environment = ssq_buf_read_uint8(&buf);
            switch (environment)
            {
                case 'w':
                    info->environment = A2S_ENVIRONMENT_WINDOWS;
                    break;

                case 'm':
                case 'o':
                    info->environment = A2S_ENVIRONMENT_MAC;
                    break;

                default: // 'l'
                    info->environment = A2S_ENVIRONMENT_LINUX;
                    break;
            }

            info->visibility = ssq_buf_read_bool(&buf);
            info->vac        = ssq_buf_read_bool(&buf);
            info->version    = ssq_buf_read_string(&buf);

            if (!ssq_buf_eob(&buf))
            {
                info->edf = ssq_buf_read_uint8(&buf);

                if (info->edf & A2S_INFO_FLAG_PORT)
                    info->port = ssq_buf_read_uint16(&buf);

                if (info->edf & A2S_INFO_FLAG_STEAMID)
                   info->steamid = ssq_buf_read_uint64(&buf);

                if (info->edf & A2S_INFO_FLAG_STV)
                {
                    info->stv_port = ssq_buf_read_uint16(&buf);
                    info->stv_name = ssq_buf_read_string(&buf);
                }

                if (info->edf & A2S_INFO_FLAG_KEYWORDS)
                    info->keywords = ssq_buf_read_string(&buf);

                if (info->edf & A2S_INFO_FLAG_GAMEID)
                    info->gameid = ssq_buf_read_uint64(&buf);
            }
        }
        else
        {
            ssq_error_set_sys(err);
        }
    }
    else
    {
        ssq_error_set(err, SSQ_ERR_BADRES, "Invalid A2S_INFO header");
    }

    return info;
}

static void ssq_info_handlechall(
    struct ssq_querier *const querier,
    char              **const response,
    size_t             *const response_len
)
{
    while (ssq_ok(querier) && ssq_utils_response_haschall(*response))
    {
        // additional 4 bytes for the challenge number
        char payload[PAYLOAD_LEN + 4] = PAYLOAD;

        // copy the challenge number
        memcpy(payload + PAYLOAD_LEN, *response + 1, 4);

        free(*response);
        *response = ssq_query(querier, payload, PAYLOAD_LEN + 4, response_len);
    }
}

struct a2s_info *ssq_info(struct ssq_querier *const querier)
{
    size_t response_len = 0;
    char *response = ssq_query(querier, PAYLOAD, PAYLOAD_LEN, &response_len);

    ssq_info_handlechall(querier, &response, &response_len);

    if (!ssq_ok(querier))
        return NULL;

    struct a2s_info *const info = ssq_info_deserialize(response, response_len, &(querier->err));

    free(response);

    return info;
}

void ssq_info_free(struct a2s_info *const info)
{
    free(info->name);
    free(info->map);
    free(info->folder);
    free(info->game);
    free(info->version);

    if (info->edf & A2S_INFO_FLAG_STV)
        free(info->stv_name);

    if (info->edf & A2S_INFO_FLAG_KEYWORDS)
        free(info->keywords);

    free(info);
}
