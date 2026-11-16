/* SPDX-License-Identifier: BSD-3-Clause */
/* VM0 backend: regular-file block storage and isolated TAP or framed Ethernet.
 * It has no mapping of Guest private RAM or of the monitor's saved context.
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <asm/planb_abi.h>

#define SHARED_BASE 0x92000000ULL
#define SHARED_SIZE 0x10000U
static volatile sig_atomic_t running = 1;
static volatile uint32_t *control;
static bool tamper_once, tampered;
static unsigned long block_reads, block_writes, network_tx, network_rx;

/* Keep syscall buffers in ordinary RAM. Kernel copy_to_user/copy_from_user
 * may use vector or cache-block operations that are invalid for an O_SYNC
 * /dev/mem mapping. Only scalar byte accesses cross the shared-window edge.
 */
static void shared_read(void *destination, const volatile void *source, size_t size)
{
    uint8_t *out = destination;
    const volatile uint8_t *in = source;
    while (size--) *out++ = *in++;
}

static void shared_write(volatile void *destination, const void *source, size_t size)
{
    volatile uint8_t *out = destination;
    const uint8_t *in = source;
    while (size--) *out++ = *in++;
}

static void stop_backend(int signal_number)
{
    (void)signal_number;
    running = 0;
}

static void die(const char *message)
{
    perror(message);
    exit(1);
}

static bool firmware_guard(void)
{
    uint8_t bytes[5];
    int fd = open("/proc/device-tree/chosen/macrn,planb-services-abi", O_RDONLY | O_CLOEXEC);
    ssize_t count = fd < 0 ? -1 : read(fd, bytes, sizeof(bytes));
    if (fd >= 0) close(fd);
    if (count == 4 && bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 0 && bytes[3] == PLANB_ABI)
        return true;
    fprintf(stderr, "Refusing access: matching macrn,planb-services-abi is absent.\n");
    return false;
}

static int open_disk(const char *path, bool create)
{
    struct stat info;
    int fd = open(path, O_RDWR | O_CLOEXEC | O_NOFOLLOW | (create ? O_CREAT | O_EXCL : 0), 0600);
    if (fd < 0) die("open regular backing file");
    if (create) {
        static const char marker[] = "PLANB REAL VIRTIO BLOCK BACKING\n";
        uint8_t block[512];
        unsigned int i;
        for (i = 0; i < sizeof(block); i++) block[i] = (uint8_t)i;
        memcpy(block, marker, sizeof(marker) - 1U);
        if (ftruncate(fd, PLANB_DISK_BYTES) || pwrite(fd, block, sizeof(block), 0) != sizeof(block))
            die("initialize new disposable backing file");
    }
    if (fstat(fd, &info) || !S_ISREG(info.st_mode) || info.st_size != PLANB_DISK_BYTES) {
        fprintf(stderr, "Backing must be an existing 512 MiB regular file; block devices are refused.\n");
        exit(1);
    }
    return fd;
}

static int open_tcp(const char *endpoint)
{
    char text[128], *separator, *end;
    struct sockaddr_in address = {.sin_family = AF_INET};
    unsigned long port;
    int fd, flags;
    if (strlen(endpoint) >= sizeof(text)) { errno = EINVAL; die("network endpoint"); }
    strcpy(text, endpoint);
    separator = strrchr(text, ':');
    if (!separator) { errno = EINVAL; die("network endpoint needs IPv4:port"); }
    *separator++ = 0;
    port = strtoul(separator, &end, 10);
    if (*end || port == 0 || port > 65535 || inet_pton(AF_INET, text, &address.sin_addr) != 1) {
        errno = EINVAL; die("network endpoint");
    }
    address.sin_port = htons((uint16_t)port);
    fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0 || connect(fd, (struct sockaddr *)&address, sizeof(address))) die("connect Ethernet relay");
    flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK)) die("nonblocking relay");
    return fd;
}

static void reject_existing_subnet(const struct in_addr *address)
{
    FILE *routes = fopen("/proc/net/route", "r");
    char line[512], interface[IFNAMSIZ];
    unsigned long destination, gateway, mask;
    unsigned int flags;
    uint32_t wanted_mask = inet_addr("255.255.255.0");
    if (!routes) die("read existing routes before TAP creation");
    while (fgets(line, sizeof(line), routes)) {
        if (sscanf(line, "%15s %lx %lx %x %*u %*u %*u %lx",
                   interface, &destination, &gateway, &flags, &mask) != 5) continue;
        if (!(flags & 1U) || !mask) continue;
        if ((address->s_addr & mask) == (destination & mask) ||
            (address->s_addr & wanted_mask) == (destination & wanted_mask)) {
            fprintf(stderr, "Refusing TAP subnet overlapping existing interface %s.\n", interface);
            fclose(routes); exit(2);
        }
    }
    fclose(routes);
}

static int open_tap(const char *name, const char *tap_address)
{
    struct ifreq request = {0};
    struct sockaddr_in *address;
    int fd, config;
    if (!*name || strlen(name) >= IFNAMSIZ || if_nametoindex(name)) {
        errno = EEXIST; die("TAP name must identify a new isolated interface");
    }
    address = (struct sockaddr_in *)&request.ifr_addr;
    inet_pton(AF_INET, tap_address, &address->sin_addr);
    reject_existing_subnet(&address->sin_addr);
    fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) die("open TUN/TAP");
    strcpy(request.ifr_name, name);
    request.ifr_flags = IFF_TAP | IFF_NO_PI | IFF_TUN_EXCL;
    if (ioctl(fd, TUNSETIFF, &request)) die("create temporary TAP");
    config = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (config < 0) die("TAP configuration socket");
    address = (struct sockaddr_in *)&request.ifr_addr;
    address->sin_family = AF_INET;
    inet_pton(AF_INET, tap_address, &address->sin_addr);
    if (ioctl(config, SIOCSIFADDR, &request)) die("set isolated TAP address");
    inet_pton(AF_INET, "255.255.255.0", &address->sin_addr);
    if (ioctl(config, SIOCSIFNETMASK, &request)) die("set isolated TAP mask");
    request.ifr_flags = IFF_UP | IFF_RUNNING;
    if (ioctl(config, SIOCSIFFLAGS, &request)) die("enable isolated TAP");
    close(config);
    return fd;
}

static int disk_transfer(int fd, struct planb_slot *request, uint8_t *data)
{
    uint32_t done = 0;
    uint64_t byte_offset;
    bool write_request = request->operation == PLANB_BLK_WRITE;
    if (request->operation == PLANB_BLK_FLUSH) return fdatasync(fd);
    if ((request->operation != PLANB_BLK_READ && !write_request) || !request->length ||
        request->length > PLANB_BLK_BYTES || (request->length & 511U) ||
        request->sector >= PLANB_DISK_BYTES / 512U ||
        request->length / 512U > PLANB_DISK_BYTES / 512U - request->sector) {
        errno = EINVAL;
        return -1;
    }
    byte_offset = request->sector * 512U;
    while (done < request->length) {
        ssize_t count = write_request ? pwrite(fd, data + done, request->length - done, byte_offset + done) :
                                      pread(fd, data + done, request->length - done, byte_offset + done);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { if (!count) errno = EIO; return -1; }
        done += (uint32_t)count;
    }
    if (write_request) block_writes++;
    else block_reads++;
    return 0;
}

static int transmit(int fd, bool tap, const uint8_t *data, uint32_t size)
{
    uint8_t frame[PLANB_NET_BYTES + 4U];
    uint32_t network_length = htonl(size);
    size_t sent = 0, total = size;
    if (size < 14U || size > PLANB_NET_BYTES) { errno = EINVAL; return -1; }
    if (tap) {
        ssize_t count = write(fd, data, size);
        return count == size ? 0 : -1;
    }
    memcpy(frame, &network_length, sizeof(network_length));
    memcpy(frame + 4U, data, size);
    total += 4U;
    while (sent < total && running) {
        ssize_t count = send(fd, frame + sent, total - sent, MSG_NOSIGNAL);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct pollfd descriptor = {.fd = fd, .events = POLLOUT};
            if (poll(&descriptor, 1, 500) <= 0) { errno = ETIMEDOUT; return -1; }
            continue;
        }
        if (count <= 0) return -1;
        sent += (size_t)count;
    }
    return sent == total ? 0 : -1;
}

/* TCP framing belongs only to the external test relay. Native TAP is L2. */
static int receive_frame(int fd, bool tap, uint8_t *output, uint32_t capacity)
{
    static uint8_t frame[PLANB_NET_BYTES + 4U];
    static size_t received, target = 4U;
    if (tap) return (int)read(fd, output, capacity);
    for (;;) {
        ssize_t count = recv(fd, frame + received, target - received, 0);
        if (count < 0) return -1;
        if (!count) { errno = ECONNRESET; return -2; }
        received += (size_t)count;
        if (received < target) continue;
        if (target == 4U) {
            uint32_t size;
            memcpy(&size, frame, sizeof(size));
            size = ntohl(size);
            if (size < 14U || size > PLANB_NET_BYTES) { errno = EPROTO; return -2; }
            target = size + 4U;
        } else {
            uint32_t size = (uint32_t)target - 4U;
            if (size > capacity) { errno = EMSGSIZE; return -2; }
            memcpy(output, frame + 4U, size);
            received = 0;
            target = 4U;
            return (int)size;
        }
    }
}

static void respond(struct planb_slot *shared, const struct planb_slot *request,
    int status, uint32_t length)
{
    shared->status = status;
    shared->response_length = length;
    shared->response_sequence = request->sequence;
    if (tamper_once && !tampered && request->operation == PLANB_BLK_READ) {
        shared->saved_s2_s10[1] = 0x5a494f4eU;
        tampered = true;
        printf("PLANB_INJECTED_S3 sequence=%llu value=0x5a494f4e\n", (unsigned long long)request->sequence);
    }
    /* /dev/mem O_SYNC may map this shared RAM with an I/O memory type.
     * RISC-V AMOs are not supported by every such mapping (SG2044 faults).
     * This is a single-producer slot: publish with an ordered scalar store,
     * then order the shared response before the MMIO doorbell.
     */
    __sync_synchronize();
    *(volatile uint32_t *)&shared->state = PLANB_RESPONSE;
    __sync_synchronize();
    control[2] = 1U;
    __sync_synchronize();
}

static void service(struct planb_slot *shared, unsigned int slot, int disk, int network, bool tap)
{
    struct planb_slot request;
    uint8_t *data = (uint8_t *)shared + PLANB_SLOT_HEADER;
    uint8_t buffer[PLANB_BLK_BYTES];
    int result, saved_error;
    uint32_t state = *(volatile uint32_t *)&shared->state;
    if (state != PLANB_REQUEST) return;
    __sync_synchronize();
    shared_read(&request, shared, sizeof(request));
    if (slot == 0) {
        if (request.operation == PLANB_BLK_WRITE && request.length <= sizeof(buffer))
            shared_read(buffer, data, request.length);
        result = disk_transfer(disk, &request, buffer);
        saved_error = errno;
        if (!result && request.operation == PLANB_BLK_READ)
            shared_write(data, buffer, request.length);
        respond(shared, &request, result ? saved_error ? saved_error : EIO : 0,
            result ? 0U : request.length);
    } else if (slot == 1) {
        errno = EINVAL;
        result = -1;
        if (request.operation == PLANB_NET_TX && request.length <= PLANB_NET_BYTES) {
            shared_read(buffer, data, request.length);
            result = transmit(network, tap, buffer, request.length);
        }
        saved_error = errno;
        if (!result) network_tx++;
        respond(shared, &request, result ? saved_error ? saved_error : EIO : 0,
            result ? 0U : request.length);
    } else {
        if (request.operation != PLANB_NET_RX || request.length > PLANB_NET_BYTES || !request.length) {
            respond(shared, &request, EINVAL, 0);
            return;
        }
        result = receive_frame(network, tap, buffer, request.length);
        saved_error = errno;
        if (result == -1 && (saved_error == EAGAIN || saved_error == EWOULDBLOCK || saved_error == EINTR)) return;
        if (result > 0) {
            shared_write(data, buffer, (uint32_t)result);
            network_rx++;
        }
        respond(shared, &request, result > 0 ? 0 : saved_error ? saved_error : EIO,
            result > 0 ? (uint32_t)result : 0U);
        if (result == -2) running = 0;
    }
}

int main(int argc, char **argv)
{
    const char *backing = NULL, *endpoint = NULL, *tap_name = NULL;
    const char *tap_address = "10.0.3.1";
    struct planb_slot *slots[3];
    uint64_t addresses[3] = {PLANB_SLOT_BLK, PLANB_SLOT_TX, PLANB_SLOT_RX};
    uint8_t *shared;
    int memory, disk, network, i;
    bool status_only = false, create_disk = false;
    setvbuf(stdout, NULL, _IOLBF, 0);
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--disk") && i + 1 < argc) backing = argv[++i];
        else if (!strcmp(argv[i], "--network-connect") && i + 1 < argc) endpoint = argv[++i];
        else if (!strcmp(argv[i], "--tap") && i + 1 < argc) tap_name = argv[++i];
        else if (!strcmp(argv[i], "--tap-address") && i + 1 < argc) tap_address = argv[++i];
        else if (!strcmp(argv[i], "--tamper-s3-once")) tamper_once = true;
        else if (!strcmp(argv[i], "--create-disk")) create_disk = true;
        else if (!strcmp(argv[i], "--status")) status_only = true;
        else {
            fprintf(stderr, "Usage: %s --disk FILE (--tap NEWNAME [--tap-address 10.0.3.1|10.0.2.2] | --network-connect IPv4:PORT) [--tamper-s3-once]\n       %s --status\n", argv[0], argv[0]);
            return 2;
        }
    }
    if (!status_only && (!backing || (!!endpoint == !!tap_name))) return 2;
    if (strcmp(tap_address, "10.0.3.1") && strcmp(tap_address, "10.0.2.2")) return 2;
    if (!firmware_guard()) return 2;
    memory = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
    if (memory < 0) die("open /dev/mem");
    control = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, memory, PLANB_CTL_MMIO);
    if (control == MAP_FAILED) die("map checked control device");
    if (((uint64_t)control[1] << 32 | control[0]) != PLANB_MAGIC || control[3] != PLANB_ABI) {
        fprintf(stderr, "Control device ABI does not match.\n"); return 2;
    }
    if (status_only) {
        printf("PLANB_STATUS warnings=%u completed=%u\n", control[6], control[7]);
        munmap((void *)control, 4096); close(memory); return 0;
    }
    shared = mmap(NULL, SHARED_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, memory, SHARED_BASE);
    if (shared == MAP_FAILED) die("map explicit shared I/O window");
    for (i = 0; i < 3; i++) slots[i] = (struct planb_slot *)(shared + addresses[i] - SHARED_BASE);
    disk = open_disk(backing, create_disk);
    network = tap_name ? open_tap(tap_name, tap_address) : open_tcp(endpoint);
    signal(SIGINT, stop_backend);
    signal(SIGTERM, stop_backend);
    signal(SIGPIPE, SIG_IGN);
    printf("PLANB_BACKEND_READY abi=%u network=%s disk_bytes=%llu\n", PLANB_ABI,
        tap_name ? tap_name : endpoint, (unsigned long long)PLANB_DISK_BYTES);
    while (running) {
        for (i = 0; i < 3; i++) service(slots[i], (unsigned int)i, disk, network, tap_name != NULL);
        usleep(1000);
    }
    printf("PLANB_BACKEND_STOP reads=%lu writes=%lu tx=%lu rx=%lu warnings=%u completed=%u\n",
        block_reads, block_writes, network_tx, network_rx, control[6], control[7]);
    close(network); close(disk);
    munmap(shared, SHARED_SIZE); munmap((void *)control, 4096); close(memory);
    return 0;
}
