#ifndef HTOP_HEADER_NETSPEED
#define HTOP_HEADER_NETSPEED

/*
htop - NetSpeed.h
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdint.h>
#include <sys/time.h>
#include "Compat.h"

#define TRAFFIC_HASH_SIZE 4096
#define HASH_SIZE 4096
#define CACHE_TTL_SEC 10

struct LinuxMachine_;
struct LinuxProcess_;
struct NetMonitoringData_;

typedef struct CacheNode {
    uint32_t inode;
    pid_t pid;
    time_t last_seen;
    struct CacheNode *next;
} CacheNode;

typedef struct TrafficNode {
    uint32_t inode;
    uint64_t last_bytes_sent;
    uint64_t last_bytes_recv;
    struct timespec last_time;
    struct TrafficNode *next;
} TrafficNode;

typedef struct NetworkSpeedMeterData_ {
    char* recvBuffer;
    size_t generation;
    CacheNode** inode_cache;
    TrafficNode** traffic_cache;
} NetworkSpeedMeterData;

void NetSpeed_dirty(struct LinuxProcess_* lp);
uint8_t NetSpeed_monitor(struct LinuxMachine_* machine);
uint8_t NetSpeed_init(struct NetMonitoringData_ *netData);
uint8_t NetSpeed_cleanup(struct NetMonitoringData_ *netData);

#endif //HTOP_HEADER_NETSPEED