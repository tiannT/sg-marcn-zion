/* SPDX-License-Identifier: BSD-3-Clause */
/* A vault for a protected, RAM-root guest. No plaintext key reaches the host I/O backend. */
#define _GNU_SOURCE
#include "aead.h"
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

#define VAULT_DIRECTORY "/run/tee-vault"
#define VALUE_MAX 4096U

static int complete_io(int fd, uint8_t *buffer, size_t length, bool writing)
{
    size_t position = 0;
    while (position < length) {
        ssize_t count = writing ? write(fd, buffer + position, length - position) :
                                 read(fd, buffer + position, length - position);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return 0;
        position += (size_t)count;
    }
    return 1;
}

static int private_file(int fd, off_t size)
{
    struct stat info;
    return !fstat(fd, &info) && S_ISREG(info.st_mode) && info.st_uid == geteuid() &&
        info.st_nlink == 1 && !(info.st_mode & 077U) && info.st_size == size;
}

static int open_directory(void)
{
    struct stat info;
    struct statfs filesystem;
    int fd;
    if (mkdir("/run", 0755) && errno != EEXIST) return -1;
    if (mkdir(VAULT_DIRECTORY, 0700) && errno != EEXIST) return -1;
    fd = open(VAULT_DIRECTORY, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return -1;
    if (fstat(fd, &info) || !S_ISDIR(info.st_mode) || info.st_uid != geteuid() ||
        (info.st_mode & 077U) || fstatfs(fd, &filesystem) ||
        (filesystem.f_type != 0x01021994L && filesystem.f_type != 0x858458f6L)) {
        close(fd);
        errno = EACCES;
        return -1;
    }
    return fd;
}

static int master_key(int directory, uint8_t key[32])
{
    int fd = openat(directory, ".master", O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    int ok;
    if (fd >= 0) {
        ok = private_file(fd, 32) && complete_io(fd, key, 32, false);
        close(fd);
        return ok;
    }
    if (errno != ENOENT || RAND_priv_bytes(key, 32) != 1) return 0;
    fd = openat(directory, ".master", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return 0;
    ok = complete_io(fd, key, 32, true) && fsync(fd) == 0;
    if (close(fd)) ok = 0;
    if (!ok) unlinkat(directory, ".master", 0);
    return ok;
}

static bool valid_name(const char *name)
{
    size_t i, length = strlen(name);
    if (!length || length > 64U || name[0] == '.') return false;
    for (i = 0; i < length; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

static int put_value(int directory, const char *name, const char *value, const uint8_t key[32])
{
    uint8_t envelope[VALUE_MAX + 32U] = {0};
    char destination[80], temporary[128];
    size_t length = strlen(value);
    int fd = -1, ok = 0;
    if (!length || length > VALUE_MAX) return 0;
    memcpy(envelope, "TV01", 4);
    if (RAND_bytes(envelope + 4U, 12) != 1 || !aead(key, envelope + 4U, name, (int)strlen(name),
        (const uint8_t *)value, (int)length, envelope + 16U, envelope + 16U + length, false)) return 0;
    snprintf(destination, sizeof(destination), "%s.gcm", name);
    snprintf(temporary, sizeof(temporary), ".%s.%02x%02x%02x%02x.%ld", name,
        envelope[4], envelope[5], envelope[6], envelope[7], (long)getpid());
    fd = openat(directory, temporary, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) goto done;
    if (!complete_io(fd, envelope, length + 32U, true) || fsync(fd)) goto done;
    if (close(fd)) { fd = -1; goto done; }
    fd = -1;
    if (renameat(directory, temporary, directory, destination)) goto done;
    ok = 1;
done:
    if (fd >= 0) close(fd);
    if (!ok) unlinkat(directory, temporary, 0);
    OPENSSL_cleanse(envelope, sizeof(envelope));
    return ok;
}

static int get_value(int directory, const char *name, const uint8_t key[32])
{
    uint8_t envelope[VALUE_MAX + 32U] = {0}, plaintext[VALUE_MAX + 16U] = {0};
    struct stat info;
    char filename[80];
    int fd, ok = 0;
    size_t length;
    snprintf(filename, sizeof(filename), "%s.gcm", name);
    fd = openat(directory, filename, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) goto done;
    if (fstat(fd, &info) || info.st_size <= 32 || info.st_size > (off_t)sizeof(envelope) ||
        !private_file(fd, info.st_size)) goto close_file;
    length = (size_t)info.st_size - 32U;
    if (!complete_io(fd, envelope, (size_t)info.st_size, false) || memcmp(envelope, "TV01", 4) ||
        !aead(key, envelope + 4U, name, (int)strlen(name), envelope + 16U, (int)length,
              plaintext, envelope + 16U + length, true)) goto close_file;
    if (fwrite(plaintext, 1, length, stdout) != length || putchar('\n') == EOF) goto close_file;
    ok = 1;
close_file:
    close(fd);
done:
    OPENSSL_cleanse(plaintext, sizeof(plaintext));
    OPENSSL_cleanse(envelope, sizeof(envelope));
    return ok;
}

int main(int argc, char **argv)
{
    uint8_t key[32] = {0};
    bool put = argc == 4 && !strcmp(argv[1], "put");
    bool get = argc == 3 && !strcmp(argv[1], "get");
    int directory, ok = 0;
    if ((!put && !get) || !valid_name(argv[2])) {
        fprintf(stderr, "Usage: tee-vault put NAME KEY\n       tee-vault get NAME\n");
        return 2;
    }
    directory = open_directory();
    if (directory < 0) {
        fprintf(stderr, "tee-vault: requires a private RAM-backed vault directory\n");
        return 1;
    }
    if (master_key(directory, key))
        ok = put ? put_value(directory, argv[2], argv[3], key) : get_value(directory, argv[2], key);
    OPENSSL_cleanse(key, sizeof(key));
    close(directory);
    if (!ok) fprintf(stderr, "tee-vault: operation or authentication failed\n");
    return ok ? 0 : 1;
}
