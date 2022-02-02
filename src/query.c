#include "ssq/query.h"

#include "ssq/errops.h"
#include "ssq/packet.h"
#include "ssq/utils.h"

#ifdef _WIN32
# include <ws2tcpip.h>
#else
# include <netdb.h>
# include <unistd.h>
# define INVALID_SOCKET (-1)
typedef int SOCKET;
#endif

#include <stdlib.h>
#include <string.h>

#define SSQ_PACKET_SIZE 1400

struct ssq_querier *ssq_init(void)
{
    struct ssq_querier *const querier = malloc(sizeof (*querier));
    if (querier == NULL)
        return NULL;

    memset(querier, 0, sizeof (*querier));

    return querier;
}

void ssq_set_target(struct ssq_querier *const querier, const char hostname[], const uint16_t port)
{
    freeaddrinfo(querier->addr_list);

    char service[6];
    ssq_utils_porttostr(port, service);

    struct addrinfo hints;
    memset(&hints, 0, sizeof (hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    const int gai_code = getaddrinfo(hostname, service, &hints, &(querier->addr_list));
    if (gai_code != 0)
    {
        querier->addr_list = NULL;
        ssq_error_set(&(querier->err), SSQ_ERR_SYS, gai_strerror(gai_code));
    }
}

void ssq_set_timeout(
    struct ssq_querier    *const querier,
    const enum ssq_timeout       timeout,
#ifdef _WIN32
    const DWORD                  value
#else
    const time_t                 value
#endif
)
{
#ifdef _WIN32
    if (timeout & SSQ_TIMEOUT_RECV)
        querier->timeout_recv = value;

    if (timeout & SSQ_TIMEOUT_SEND)
        querier->timeout_send = value;
#else
    if (timeout & SSQ_TIMEOUT_RECV)
        ssq_utils_mstotv(value, &(querier->timeout_recv));

    if (timeout & SSQ_TIMEOUT_SEND)
        ssq_utils_mstotv(value, &(querier->timeout_send));
#endif
}

enum ssq_error_code ssq_errc(const struct ssq_querier *const querier)
{
    return querier->err.code;
}

bool ssq_ok(const struct ssq_querier *const querier)
{
    return ssq_errc(querier) == SSQ_OK;
}

const char *ssq_errm(const struct ssq_querier *const querier)
{
    return querier->err.message;
}

void ssq_errclr(struct ssq_querier *const querier)
{
    ssq_error_clear(&(querier->err));
}

void ssq_free(struct ssq_querier *const querier)
{
    freeaddrinfo(querier->addr_list);
    free(querier);
}

/**
 * Sets the SNDTIMEO and RCVTIMEO options of a socket.
 * @param sockfd the socket file descriptor
 * @param timeout the timeout value in milliseconds
 * @returns 0 when successful, -1 in case of an error
 */
static int ssq_query_socket_timeout(
    const SOCKET                sockfd,
#ifdef _WIN32
    const DWORD                 timeout_recv,
    const DWORD                 timeout_send
#else
    const struct timeval *const timeout_recv,
    const struct timeval *const timeout_send
#endif
)
{
#ifdef _WIN32
    int res = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &timeout_recv, sizeof (timeout_recv));
    if (res == SOCKET_ERROR)
        return -1;

    res = setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char *) &timeout_send, sizeof (timeout_send));
    if (res == SOCKET_ERROR)
        return -1;
#else
    int res = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, timeout_recv, sizeof (*timeout_recv));
    if (res == -1)
        return -1;

    res = setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, timeout_send, sizeof (*timeout_send));
    if (res == -1)
        return -1;
#endif

    return 0;
}

/**
 * Prepares a socket for a Source server querier.
 * @param inst the Source server querier
 * @param addr where to store the address that was used to create the socket
 * @returns the socket file descriptor
 */
static SOCKET ssq_query_socket(struct ssq_querier *const querier, struct addrinfo **const addr)
{
    SOCKET sockfd = INVALID_SOCKET;

    for (*addr = querier->addr_list; *addr != NULL; *addr = (*addr)->ai_next)
    {
        sockfd = socket((*addr)->ai_family, (*addr)->ai_socktype, (*addr)->ai_protocol);
        if (sockfd != INVALID_SOCKET)
            break;
    }

    if (sockfd != INVALID_SOCKET)
    {
#ifdef _WIN32
        if (ssq_query_socket_timeout(sockfd, querier->timeout_recv, querier->timeout_send) == -1)
        {
            ssq_error_set_wsa(&(querier->err));
            closesocket(sockfd);
#else
        if (ssq_query_socket_timeout(sockfd, &(querier->timeout_recv), &(querier->timeout_send)) == -1)
        {
            ssq_error_set_sys(&(querier->err));
            close(sockfd);
#endif

            sockfd = INVALID_SOCKET;
            *addr = NULL;
        }
    }
    else
    {
#ifdef _WIN32
        ssq_error_set_wsa(&(querier->err));
#else
        ssq_error_set_sys(&(querier->err));
#endif
    }

    return sockfd;
}

/**
 * Processes a packet and stores it in an ordered packet array.
 * @param payload the raw packet's payload
 * @param size the raw packet's payload size
 * @param packets address of the heap-allocated packets array
 * @param packet_count where to store the packet count
 * @param err where to report potential errors
 */
static void ssq_query_recv_packet(
    const char                 payload[],
    const size_t               size,
    struct ssq_packet ***const packets,
    uint8_t             *const packet_count,
    struct ssq_error    *const err
)
{
    struct ssq_packet *const packet = ssq_packet_init(payload, size, err);
    if (err->code != SSQ_OK)
        return;

    // no packets received yet
    if (*packets == NULL)
    {
        // update the packet count in case of a multi-packet response
        *packet_count = packet->total;

        *packets = calloc(*packet_count, sizeof (struct ssq_packet *));
        if (*packets == NULL)
            ssq_error_set_sys(err);
    }

    if (err->code == SSQ_OK)
        (*packets)[packet->number] = packet;
}

/**
 * Receives packets from a socket.
 * @param sockfd the socket file descriptor
 * @param packet_count where to store the packet count
 * @param err where to report potential errors
 * @returns an ordered array of packets when successful, NULL otherwise
 */
static struct ssq_packet **ssq_query_recv(
    const SOCKET               sockfd,
    uint8_t             *const packet_count,
    struct ssq_error    *const err
)
{
    *packet_count = 1;

    // ordered array of packets in the response
    struct ssq_packet **packets = NULL;

    // loop until we received all the packets
    for (uint8_t packets_received = 0; packets_received < *packet_count; ++packets_received)
    {
        char payload[SSQ_PACKET_SIZE];

#ifdef _WIN32
        const int bytes_received = recvfrom(sockfd, payload, SSQ_PACKET_SIZE, 0, NULL, NULL);
        if (bytes_received != SOCKET_ERROR)
#else
        const ssize_t bytes_received = recvfrom(sockfd, payload, SSQ_PACKET_SIZE, 0, NULL, NULL);
        if (bytes_received != -1)
#endif
        {
            ssq_query_recv_packet(payload, bytes_received, &packets, packet_count, err);
        }
        else
        {
#ifdef _WIN32
            ssq_error_set_wsa(err);
#else
            ssq_error_set_sys(err);
#endif
        }

        if (err->code != SSQ_OK)
        {
            if (packets != NULL)
            {
                ssq_packet_freearr(packets, *packet_count);
                packets = NULL;
            }

            break;
        }
    }

    return packets;
}

char *ssq_query(
    struct ssq_querier *const querier,
    const char                payload[],
#ifdef _WIN32
    const int                 payload_len,
#else
    const size_t              payload_len,
#endif
    size_t             *const response_len
)
{
    char *response = NULL;

    struct addrinfo *addr = NULL;
    SOCKET sockfd = ssq_query_socket(querier, &addr);

    if (!ssq_ok(querier))
        return NULL;

#ifdef _WIN32
    if (sendto(sockfd, payload, payload_len, 0, addr->ai_addr, (int) addr->ai_addrlen) != SOCKET_ERROR)
#else
    if (sendto(sockfd, payload, payload_len, 0, addr->ai_addr, addr->ai_addrlen) != -1)
#endif
    {
        uint8_t packet_count = 0;
        struct ssq_packet **packets = ssq_query_recv(sockfd, &packet_count, &(querier->err));

        if (ssq_ok(querier))
        {
            response = ssq_packet_mergepayloads(packets, packet_count, response_len, &(querier->err));
            ssq_packet_freearr(packets, packet_count);
        }
    }
    else // sendto failure
    {
#ifdef _WIN32
        ssq_error_set_wsa(&(querier->err));
#else
        ssq_error_set_sys(&(querier->err));
#endif
    }

#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif

    return response;
}
