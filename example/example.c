#include <ssq/a2s.h>

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
# include <winsock2.h>
#endif

/* Pretty-prints the result of an A2S_INFO query */
void ssq_info_print(const struct a2s_info *info);

/* Pretty-prints the result of an A2S_PLAYER query */
void ssq_player_print(const struct a2s_player *players, uint8_t player_count);

/* Pretty-prints the result of an A2S_RULES query */
void ssq_rules_print(const struct a2s_rules *rules, uint16_t rule_count);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s hostname port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        fprintf(stderr, "WSAStartup failed with code %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
#endif

    char *hostname = argv[1];
    uint16_t port = atoi(argv[2]);

    // timeout for sendto/recvfrom calls when sending queries
#ifdef _WIN32
    DWORD timeout = 3000; // ms
#else
    time_t timeout = 3000; // ms
#endif

    // initialize a new Source server querier
    struct ssq_querier *querier = ssq_init();

    // assert there was no memory allocation failure
    assert(querier != NULL);

    // set the timeout
    ssq_set_timeout(querier, SSQ_TIMEOUT_RECV | SSQ_TIMEOUT_SEND, timeout);

    // set the target Source game server
    ssq_set_target(querier, hostname, port);

    // error handling
    if (!ssq_ok(querier)) // shortcut for `ssq_errc(querier) != SSQ_OK`
    {
        fprintf(stderr, "ssq_set_target: %s: %s\n", hostname, ssq_errm(querier));

        // always clear the error once we are done handling it
        ssq_errclr(querier);

        exit(EXIT_FAILURE);
    }


    /* A2S_INFO */

    // send an A2S_INFO query to the target server using `ssq_info`
    struct a2s_info *info = ssq_info(querier);

    // error handling
    if (!ssq_ok(querier)) // shortcut for `ssq_errc(querier) != SSQ_OK`
    {
        fprintf(stderr, "ssq_info: %s\n", ssq_errm(querier));

        // always clear the error once we are done handling it
        ssq_errclr(querier);

        exit(EXIT_FAILURE);
    }

    ssq_info_print(info);

    // always free the A2S_INFO struct once we are done using it
    ssq_info_free(info);


    /* A2S_PLAYER */

    // send an A2S_PLAYER query
    uint8_t player_count = 0;
    struct a2s_player *players = ssq_player(querier, &player_count);

    // error handling
    if (!ssq_ok(querier)) // shortcut for `ssq_errc(querier) != SSQ_OK`
    {
        fprintf(stderr, "ssq_player: %s\n", ssq_errm(querier));

        // always clear the error once we are done handling it
        ssq_errclr(querier);

        exit(EXIT_FAILURE);
    }

    ssq_player_print(players, player_count);

    // always free the A2S_PLAYER array once we are done using it
    ssq_player_free(players, player_count);


    /* A2S_RULES */

    // send an A2S_RULES query
    uint16_t rule_count = 0;
    struct a2s_rules *rules = ssq_rules(querier, &rule_count);

    // error handling
    if (!ssq_ok(querier)) // shortcut for `ssq_errc(querier) != SSQ_OK`
    {
        fprintf(stderr, "ssq_rules: %s\n", ssq_errm(querier));

        // always clear the error once we are done handling it
        ssq_errclr(querier);

        exit(EXIT_FAILURE);
    }

    ssq_rules_print(rules, rule_count);

    // always free the A2S_RULES array once we are done using it
    ssq_rules_free(rules, rule_count);


    // always free the querier once we are done using it
    ssq_free(querier);

#ifdef _WIN32
    WSACleanup();
#endif

    return EXIT_SUCCESS;
}

void ssq_info_print(const struct a2s_info *const info)
{
    printf("----- INFO BEGIN -----\n");

    printf("Protocol.......: %" PRIu8 "\n", info->protocol);
    printf("Name...........: %s\n", info->name);
    printf("Map............: %s\n", info->map);
    printf("Folder.........: %s\n", info->folder);
    printf("Game...........: %s\n", info->game);
    printf("ID.............: %" PRIu16 "\n", info->id);
    printf("Players........: %" PRIu8 "/%" PRIu8 " (%" PRIu8 " bots)\n", info->players, info->max_players, info->bots);
    printf("Server type....: %s\n",
        ((info->server_type == A2S_SERVER_TYPE_DEDICATED) ? "dedicated" :
        ((info->server_type == A2S_SERVER_TYPE_STV_RELAY) ? "SourceTV relay (proxy)" :
                                                            "non dedicated"))
    );
    printf("Environment....: %s\n",
        ((info->environment == A2S_ENVIRONMENT_WINDOWS) ? "windows" :
        ((info->environment == A2S_ENVIRONMENT_MAC)     ? "mac" :
                                                          "linux"))
    );
    printf("Visibility.....: %s\n", ((info->visibility) ? "private" : "public"));
    printf("VAC............: %s\n", ((info->vac)        ? "secured" : "unsecured"));
    printf("Version........: %s\n", info->version);

    if (info->edf & A2S_INFO_FLAG_PORT)
        printf("Port...........: %" PRIu16 "\n", info->port);

    if (info->edf & A2S_INFO_FLAG_STEAMID)
        printf("SteamID........: %" PRIu64 "\n", info->steamid);

    if (info->edf & A2S_INFO_FLAG_STV)
    {
        printf("Port (SourceTV): %" PRIu16 "\n", info->stv_port);
        printf("Name (SourceTV): %s\n", info->stv_name);
    }

    if (info->edf & A2S_INFO_FLAG_KEYWORDS)
        printf("Keywords.......: %s\n", info->keywords);

    if (info->edf & A2S_INFO_FLAG_GAMEID)
        printf("GameID.........: %" PRIu64 "\n", info->gameid);

    printf("------ INFO END ------\n");
}

void ssq_player_print(const struct a2s_player players[], const uint8_t player_count)
{
    for (uint8_t i = 0; i < player_count; ++i)
    {
        printf("--- PLAYER %" PRIu8 " BEGIN ---\n", i);
        printf("Name....: %s\n", players[i].name);
        printf("Score...: %" PRId32 "\n", players[i].score);
        printf("Duration: %f\n", players[i].duration);
        printf("---- PLAYER %" PRIu8 " END ----\n", i);
    }
}

void ssq_rules_print(const struct a2s_rules rules[], const uint16_t rule_count)
{
    printf("----- RULES BEGIN -----\n");

    for (uint16_t i = 0; i < rule_count; ++i)
        printf("%s = %s\n", rules[i].name, rules[i].value);

    printf("------ RULES END ------\n");
}
