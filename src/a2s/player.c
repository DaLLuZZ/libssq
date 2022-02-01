#include "ssq/a2s/player.h"

#include "ssq/buffer.h"
#include "ssq/errops.h"
#include "ssq/query.h"
#include "ssq/utils.h"

#include <stdlib.h>
#include <string.h>

#define PAYLOAD     "\xFF\xFF\xFF\xFF\x55\xFF\xFF\xFF\xFF"
#define PAYLOAD_LEN 9

#define S2A_PLAYER  0x44

static struct a2s_player *ssq_player_deserialize(
    const char                 response[],
    const size_t               response_len,
    uint8_t             *const player_count,
    struct ssq_error    *const err
)
{
    struct a2s_player *players = NULL;

    struct ssq_buf buf;
    ssq_buf_init(&buf, response, response_len);

    if (ssq_utils_response_istruncated(response))
        ssq_buf_forward(&buf, sizeof (int32_t));

    const uint8_t header = ssq_buf_read_uint8(&buf);

    if (header == S2A_PLAYER)
    {
        *player_count = ssq_buf_read_uint8(&buf);

        if (*player_count != 0)
        {
            players = calloc(*player_count, sizeof (*players));

            if (players != NULL)
            {
                for (uint8_t i = 0; i < *player_count; ++i)
                {
                    struct a2s_player *const player = &(players[i]);

                    player->index    = ssq_buf_read_uint8(&buf);
                    player->name     = ssq_buf_read_string(&buf);
                    player->score    = ssq_buf_read_int32(&buf);
                    player->duration = ssq_buf_read_float(&buf);
                }
            }
            else
            {
                ssq_error_set_sys(err);
            }
        }
    }
    else
    {
        ssq_error_set(err, SSQ_ERR_BADRES, "Invalid A2S_PLAYER header");
    }

    return players;
}

void ssq_player_handlechall(struct ssq_querier *const querier, char **const response, size_t *const response_len)
{
    while (ssq_ok(querier) && ssq_utils_response_haschall(*response))
    {
        char payload[PAYLOAD_LEN] = PAYLOAD;

        // copy the challenge number
        memcpy(payload + 5, *response + 1, 4);

        free(*response);
        *response = ssq_query(querier, payload, PAYLOAD_LEN, response_len);
    }
}

struct a2s_player *ssq_player(struct ssq_querier *const querier, uint8_t *const player_count)
{
    size_t response_len = 0;
    char *response = ssq_query(querier, PAYLOAD, PAYLOAD_LEN, &response_len);

    ssq_player_handlechall(querier, &response, &response_len);

    if (!ssq_ok(querier))
        return NULL;

    struct a2s_player *const players = ssq_player_deserialize(response, response_len, player_count, &(querier->err));

    free(response);

    return players;
}

void ssq_player_free(struct a2s_player players[], const uint8_t player_count)
{
    for (uint8_t i = 0; i < player_count; ++i)
        free(players[i].name);

    free(players);
}
