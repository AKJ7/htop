#include "config.h"

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <stdint.h>

#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/inet_diag.h>
#include <linux/tcp.h>
#include <linux/rtnetlink.h>
#include <netinet/in.h>
#include <linux/sock_diag.h>

#include "NetSpeed.h"
#include "linux/LinuxProcess.h"

#include <errno.h>
#include <sys/param.h>

#include "LinuxMachine.h"

#if (HASH_SIZE & (HASH_SIZE - 1)) != 0
#error "HASH_SIZE must be a power of 2"
#endif
#define NETSPEED_NETLINK_BUFFER_SIZE (256 * 1024u)


struct diag_req_pack {
    struct nlmsghdr nlh;
    struct inet_diag_req_v2 r;
};


static uint32_t hash_inode(uint32_t inode) {
    return inode & (HASH_SIZE - 1);
}

static void cache_put(uint32_t inode, pid_t pid, NetworkSpeedMeterData *speedMeterData) {
    CacheNode **inode_cache = speedMeterData->inode_cache;
    uint32_t slot = hash_inode(inode);
    CacheNode *node = inode_cache[slot];
    while (node != NULL) {
        if (node->inode == inode) {
            node->pid = pid;
            node->last_seen = time(NULL);
            return;
        }
        node = node->next;
    }
    CacheNode *new_node = malloc(sizeof(CacheNode));
    if (!new_node) {
        return;
    }
    new_node->inode = inode;
    new_node->pid = pid;
    new_node->last_seen = time(NULL);
    new_node->next = inode_cache[slot];
    inode_cache[slot] = new_node;
}

static void free_inode_cache(NetworkSpeedMeterData *speedMeterData) {
    CacheNode **inode_cache = speedMeterData->inode_cache;
    for (int i = 0; i < HASH_SIZE; i++) {
        CacheNode *node = inode_cache[i];
        while (node != NULL) {
            CacheNode *tmp = node;
            node = node->next;
            free(tmp);
        }
        inode_cache[i] = NULL;
    }
}

static void rebuild_pid_cache(NetworkSpeedMeterData *speedMeterData) {
    free_inode_cache(speedMeterData);
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        return;
    }
    struct dirent *proc_entry;
    char fd_dir_path[256];
    char link_target[256];

    while ((proc_entry = readdir(proc_dir))) {
        if (proc_entry->d_name[0] < '0' || proc_entry->d_name[0] > '9') {
            continue;
        }
        pid_t pid = atoi(proc_entry->d_name);
        snprintf(fd_dir_path, sizeof(fd_dir_path), "/proc/%d/fd", pid);
        DIR *fd_dir = opendir(fd_dir_path);
        if (!fd_dir) {
            continue;
        }
        struct dirent *fd_entry;
        while ((fd_entry = readdir(fd_dir))) {
            if (fd_entry->d_name[0] == '.') {
                continue;
            }
            char fd_link_path[512];
            snprintf(fd_link_path, sizeof(fd_link_path), "%s/%s", fd_dir_path, fd_entry->d_name);
            ssize_t len = readlink(fd_link_path, link_target, sizeof(link_target) - 1);
            if (len != -1) {
                link_target[len] = '\0';
                uint32_t inode;
                if (sscanf(link_target, "socket:[%u]", &inode) == 1) {
                    cache_put(inode, pid, speedMeterData);
                }
            }
        }
        closedir(fd_dir);
    }
    closedir(proc_dir);
}

static pid_t get_pid_from_inode(uint32_t inode, NetworkSpeedMeterData *speedMeterData) {
    if (speedMeterData == NULL) {
        return -1;
    }
    CacheNode **inode_cache = speedMeterData->inode_cache;
    uint32_t slot = hash_inode(inode);
    CacheNode *node = inode_cache[slot];
    time_t now = time(NULL);
    while (node != NULL) {
        if (node->inode == inode) {
            // if (now - node->last_seen > CACHE_TTL_SEC) {
            //     break;
            // }
            node->last_seen = now;
            return node->pid;
        }
        node = node->next;
    }
    return -1;
}

static double get_time_delta(struct timespec *start, struct timespec *end) {
    return (double) (end->tv_sec - start->tv_sec) +
           (double) (end->tv_nsec - start->tv_nsec) / 1e9;
}

static void calculate_and_update_speed(NetworkSpeedMeterData *speedMeterData, uint32_t inode, pid_t pid,
                                       uint64_t current_sent, uint64_t current_recv, ProcessTable *pt) {
    TrafficNode **traffic_cache = speedMeterData->traffic_cache;
    uint32_t slot = inode & (TRAFFIC_HASH_SIZE - 1);
    TrafficNode *node = traffic_cache[slot];
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    while (node != NULL) {
        if (node->inode == inode) {
            double time_delta = get_time_delta(&node->last_time, &now);
            if (time_delta > 0.001) {
                uint64_t delta_sent = (current_sent >= node->last_bytes_sent)
                                          ? (current_sent - node->last_bytes_sent)
                                          : 0;
                uint64_t delta_recv = (current_recv >= node->last_bytes_recv)
                                          ? (current_recv - node->last_bytes_recv)
                                          : 0;
                bool preExisiting;
                Process *proc = ProcessTable_getProcess(pt, pid, &preExisiting, LinuxProcess_new);
                if (proc != NULL) {
                    LinuxProcess *lp = (LinuxProcess *) proc;
                    if (lp->tcp_rate.generation != speedMeterData->generation) {
                        lp->tcp_rate.download = lp->tcp_rate.upload = 0;
                        lp->tcp_rate.generation = speedMeterData->generation;
                    }
                    lp->tcp_rate.download += (double) delta_recv / time_delta;
                    lp->tcp_rate.upload += (double) delta_sent / time_delta;
                }
            }
            node->last_bytes_sent = current_sent;
            node->last_bytes_recv = current_recv;
            node->last_time = now;
            return;
        }
        node = node->next;
    }
    TrafficNode *new_node = malloc(sizeof(TrafficNode));
    if (!new_node) return;
    new_node->inode = inode;
    new_node->last_bytes_sent = current_sent;
    new_node->last_bytes_recv = current_recv;
    new_node->last_time = now;
    new_node->next = traffic_cache[slot];
    traffic_cache[slot] = new_node;
}


static void free_traffic_cache(NetworkSpeedMeterData *meterData) {
    TrafficNode **traffic_cache = meterData->traffic_cache;
    for (int i = 0; i < TRAFFIC_HASH_SIZE; i++) {
        TrafficNode *node = traffic_cache[i];
        while (node != NULL) {
            TrafficNode *tmp = node;
            node = node->next;
            free(tmp);
        }
        traffic_cache[i] = NULL;
    }
}


static int query_inet_diag(int nl_fd, uint8_t family, uint32_t seq_num) {
    struct diag_req_pack req;
    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct inet_diag_req_v2));
    req.nlh.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
    req.nlh.nlmsg_type = SOCK_DIAG_BY_FAMILY;
    req.nlh.nlmsg_seq = seq_num;
    req.r.sdiag_family = family;
    req.r.sdiag_protocol = IPPROTO_TCP;
    req.r.idiag_states = UINT32_MAX;
    req.r.idiag_ext = (1 << (INET_DIAG_INFO - 1));
    struct sockaddr_nl sa;
    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    if (sendto(nl_fd, &req, req.nlh.nlmsg_len, 0, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
        perror("Failed sending diagnostics request payload");
        return -1;
    }
    return 0;
}


static int netspeed_process_diag_message(const struct nlmsghdr *nlh, NetworkSpeedMeterData *networkData,
                                         ProcessTable *pt) {
    if (nlh->nlmsg_len < NLMSG_LENGTH(sizeof(struct inet_diag_msg))) {
        return 0;
    }
    const struct inet_diag_msg *diag = NLMSG_DATA(nlh);
    uint32_t inode = diag->idiag_inode;
    if (inode == 0) {
        return 0;
    }
    pid_t pid = get_pid_from_inode(inode, networkData);
    if (pid <= 0)
        return 0;
    int attr_len = (int) nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*diag));
    const struct rtattr *attr = (const struct rtattr *) (diag + 1);
    for (; RTA_OK(attr, attr_len); attr = RTA_NEXT(attr, attr_len)) {
        if (attr->rta_type != INET_DIAG_INFO) {
            continue;
        }
        size_t payload = RTA_PAYLOAD(attr);
        size_t need =
                offsetof(struct tcp_info, tcpi_bytes_received) + sizeof(((struct tcp_info *) 0)->tcpi_bytes_received);
        if (payload < need) {
            return 0;
        }
        const struct tcp_info *info = RTA_DATA(attr);
        calculate_and_update_speed(networkData,
                                   inode,
                                   pid,
                                   info->tcpi_bytes_sent,
                                   info->tcpi_bytes_received, pt);
        return 0;
    }
    return 0;
}

static int netspeed_inet_rev(int fd, uint32_t expected_seq, NetworkSpeedMeterData *networkData, ProcessTable *pt) {
    bool done = false;
    int result = 0;
    char *buffer = networkData->recvBuffer;
    while (!done) {
        struct iovec iov = {
            .iov_base = networkData->recvBuffer,
            .iov_len = NETSPEED_NETLINK_BUFFER_SIZE,
        };
        struct sockaddr_nl peer;
        struct msghdr msg = {
            .msg_name = &peer,
            .msg_namelen = sizeof(peer),
            .msg_iov = &iov,
            .msg_iovlen = 1,
        };
        ssize_t len = recvmsg(fd, &msg, 0);
        if (len < 0) {
            if (errno == EINTR) {
                continue;
            }
            result = -1;
            break;
        }
        if (msg.msg_flags & MSG_TRUNC) {
            result = -1;
            break;
        }
        int remaining = (int) len;
        for (struct nlmsghdr *nlh = (struct nlmsghdr *) buffer; NLMSG_OK(nlh, remaining);
             nlh = NLMSG_NEXT(nlh, remaining)) {
            if (nlh->nlmsg_seq != expected_seq) {
                continue;
            }
            if (nlh->nlmsg_type == NLMSG_DONE) {
                done = true;
                break;
            }
            if (nlh->nlmsg_type == NLMSG_ERROR) {
                const struct nlmsgerr *err = NLMSG_DATA(nlh);
                if (nlh->nlmsg_len < NLMSG_LENGTH(sizeof(*err))) {
                } else if (err->error != 0) {
                    errno = -err->error;
                }
                result = -1;
                done = true;
                break;
            }
            netspeed_process_diag_message(nlh, networkData, pt);
        }
    }
    return result;
}


static int netspeed_collect_family(int fd, uint8_t family, uint32_t *seq, NetworkSpeedMeterData *networkSpeedMeterData,
                                   ProcessTable *pt) {
    ++(*seq);
    if (query_inet_diag(fd, family, *seq) == 0) {
        return netspeed_inet_rev(fd, *seq, networkSpeedMeterData, pt);
    }
    return -1;
}


uint8_t NetSpeed_init(NetMonitoringData *netData) {
    if (netData == NULL) {
        return 0u;
    }
    netData->networkSpeedMeterData.inode_cache = xCalloc(HASH_SIZE, sizeof(CacheNode *));
    netData->networkSpeedMeterData.traffic_cache = xCalloc(TRAFFIC_HASH_SIZE, sizeof(TrafficNode *));
    netData->networkSpeedMeterData.recvBuffer = xMalloc(NETSPEED_NETLINK_BUFFER_SIZE);
    int nl_fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_INET_DIAG);
    if (nl_fd < 0) {
        goto fail;
    }
    netData->socket = nl_fd;
    netData->seq = 0;
    return 1u;
fail:
    if (netData->networkSpeedMeterData.inode_cache != NULL) {
        free(netData->networkSpeedMeterData.inode_cache);
    }
    if (netData->networkSpeedMeterData.traffic_cache != NULL) {
        free(netData->networkSpeedMeterData.traffic_cache);
    }
    if (netData->networkSpeedMeterData.recvBuffer != NULL) {
        free(netData->networkSpeedMeterData.recvBuffer);
    }
    return 0u;
}

uint8_t NetSpeed_cleanup(NetMonitoringData *netData) {
    if (netData == NULL) {
        return 0u;
    }
    close(netData->socket);
    free_inode_cache(&netData->networkSpeedMeterData);
    free_traffic_cache(&netData->networkSpeedMeterData);
    free(netData->networkSpeedMeterData.recvBuffer);
    return 1u;
}


void NetSpeed_dirty(LinuxProcess *lp) {
    // Noop
    (void)lp;
}


uint8_t NetSpeed_monitor(LinuxMachine *lhost) {
    NetMonitoringData *netMonitoringData = lhost->netMonitoringData;
    if (netMonitoringData == NULL) {
        lhost->netMonitoringData = xCalloc(1, sizeof(NetMonitoringData));
        if (!NetSpeed_init(lhost->netMonitoringData)) {
            free(lhost->netMonitoringData);
            lhost->netMonitoringData = NULL;
            return 0u;
        }
        netMonitoringData = lhost->netMonitoringData;
    }
    rebuild_pid_cache(&netMonitoringData->networkSpeedMeterData);
    ++netMonitoringData->networkSpeedMeterData.generation;
    if (netspeed_collect_family(netMonitoringData->socket, AF_INET, &netMonitoringData->seq,
                                &netMonitoringData->networkSpeedMeterData,
                                (ProcessTable *) lhost->super.processTable) == 0 &&
        netspeed_collect_family(netMonitoringData->socket, AF_INET6, &netMonitoringData->seq,
                                &netMonitoringData->networkSpeedMeterData,
                                (ProcessTable *) lhost->super.processTable) == 0
    ) {
        return 1u;
    }
    return 0u;
}
