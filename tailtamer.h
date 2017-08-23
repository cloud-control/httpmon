#ifndef TAILTAMER_H
#define TAILTAMER_H

#include <stdint.h>
#include <sys/prctl.h>

#ifndef PR_TAILTAMER
/* Tailtamer */
#define PR_TAILTAMER	48
# define PR_TT_GET_COMM_UAT	0
# define PR_TT_SET_COMM_UAT	1
# define PR_TT_GET_SCHED_UAT	2
#endif // PR_TAILTAMER

typedef uint16_t be16;
typedef uint32_t be32;
typedef uint64_t be64;

#define IP_TT_OPT 85

struct ip_tt_hdr {
    uint8_t type;
    uint8_t hdrlen;
    be16 reserved1;
    be32 reserved2;
    be64 uat;     /* nanoseconds since UNIX epoch; lasts until 2554 */
};

/**
 * Set the uat ip option for a socket.
 * @param socket_fd the socket
 * @param uat       the uat value
 */
void set_socket_uat(int socket_fd, uint64_t uat) {
    struct ip_tt_hdr uat_opt;

    // Set options field with uat
    uat_opt.type = IP_TT_OPT;
    uat_opt.hdrlen = sizeof(uat_opt);
    uat_opt.reserved1 = 0;
    uat_opt.reserved2 = 0;
    uat_opt.uat = htobe64(uat);

    if (setsockopt(socket_fd, IPPROTO_IP, IP_OPTIONS, &uat_opt,
                   sizeof(uat_opt))) {
        perror("setsockopt (UAT)");
    }
}

/**
 * Set the comm uat of the current process
 */
void set_comm_uat(uint64_t uat) {
    if (prctl(PR_TAILTAMER, PR_TT_SET_COMM_UAT, uat)) {
        perror("prctl (UAT)");
    }
}

#endif // TAILTAMER_H

