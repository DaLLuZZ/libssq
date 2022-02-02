#include "ssq/a2s/rules.h"

#include "ssq/buffer.h"
#include "ssq/errops.h"
#include "ssq/query.h"
#include "ssq/utils.h"

#include <stdlib.h>
#include <string.h>

#define PAYLOAD     "\xFF\xFF\xFF\xFF\x56\xFF\xFF\xFF\xFF"
#define PAYLOAD_LEN 9

#define S2A_RULES   0x45

struct a2s_rules *ssq_rules_deserialize(
    const char              response[],
    const size_t            response_len,
    uint16_t         *const rule_count,
    struct ssq_error *const err
)
{
    struct a2s_rules *rules = NULL;

    struct ssq_buf buf;
    ssq_buf_init(&buf, response, response_len);

    if (ssq_utils_response_istruncated(response))
        ssq_buf_forward(&buf, sizeof (int32_t));

    const uint8_t header = ssq_buf_read_uint8(&buf);

    if (header == S2A_RULES)
    {
        *rule_count = ssq_buf_read_uint16(&buf);

        if (*rule_count != 0)
        {
            rules = calloc(*rule_count, sizeof (*rules));

            if (rules != NULL)
            {
                for (uint16_t i = 0; i < *rule_count; ++i)
                {
                    rules[i].name  = ssq_buf_read_string(&buf);
                    rules[i].value = ssq_buf_read_string(&buf);
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
        ssq_error_set(err, SSQ_ERR_BADRES, "Invalid A2S_RULES header");
    }

    return rules;
}

void ssq_rules_handlechall(
    struct ssq_querier *const querier,
    char              **const response,
    size_t             *const response_len
)
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

struct a2s_rules *ssq_rules(
    struct ssq_querier *const querier,
    uint16_t           *const rule_count
)
{
    size_t response_len = 0;
    char *response = ssq_query(querier, PAYLOAD, PAYLOAD_LEN, &response_len);

    ssq_rules_handlechall(querier, &response, &response_len);

    if (!ssq_ok(querier))
        return NULL;

    struct a2s_rules *const rules = ssq_rules_deserialize(response, response_len, rule_count, &(querier->err));

    free(response);

    return rules;
}

void ssq_rules_free(struct a2s_rules rules[], const uint16_t rule_count)
{
    for (uint16_t i = 0; i < rule_count; ++i)
    {
        free(rules[i].name);
        free(rules[i].value);
    }

    free(rules);
}
