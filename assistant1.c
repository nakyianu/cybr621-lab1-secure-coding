#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>

#define BUFFER_SIZE 8192

static int write_all(int fd, const char *buf, size_t len) {
    size_t total = 0;

    while (total < len) {
        ssize_t written;

        do {
            written = write(fd, buf + total, len - total);
        } while (written < 0 && errno == EINTR);

        if (written < 0) {
            return -1;
        }
        if (written == 0) {
            return -1;
        }

        total += (size_t)written;
    }

    return 0;
}

static int print_text(int fd, const char *text) {
    while (*text != '\0') {
        if (write_all(fd, text, 1) < 0) {
            return -1;
        }
        text++;
    }
    return 0;
}

static int print_path(int fd, const char *path) {
    while (*path != '\0') {
        if (write_all(fd, path, 1) < 0) {
            return -1;
        }
        path++;
    }
    return 0;
}

static int open_retry(const char *path, int flags, mode_t mode) {
    int fd;

    while (1) {
        fd = open(path, flags, mode);
        if (fd >= 0) {
            return fd;
        }
        if (errno == EINTR) {
            continue;
        }
        return -1;
    }
}

static ssize_t read_retry(int fd, void *buf, size_t len) {
    ssize_t bytes_read;

    while (1) {
        bytes_read = read(fd, buf, len);
        if (bytes_read >= 0) {
            return bytes_read;
        }
        if (errno == EINTR) {
            continue;
        }
        return -1;
    }
}

static int close_retry(int fd) {
    int rc;

    while (1) {
        rc = close(fd);
        if (rc == 0) {
            return 0;
        }
        if (errno == EINTR) {
            continue;
        }
        return -1;
    }
}

int main(int argc, char **argv) {
    const char *src;
    const char *dst;
    int src_fd = -1;
    int dst_fd = -1;
    int status = 1;

    if (argc != 3) {
        print_text(STDERR_FILENO, "Usage: secure_copy SOURCE DESTINATION\n");
        return 2;
    }

    src = argv[1];
    dst = argv[2];

    if (src[0] == '\0' || dst[0] == '\0') {
        print_text(STDERR_FILENO, "Error: source and destination paths must not be empty.\n");
        return 2;
    }

    src_fd = open_retry(src, O_RDONLY, 0);
    if (src_fd < 0) {
        print_text(STDERR_FILENO, "Error: cannot open source file ");
        print_path(STDERR_FILENO, src);
        print_text(STDERR_FILENO, "\n");
        return 1;
    }

    dst_fd = open_retry(dst, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (dst_fd < 0) {
        if (close_retry(src_fd) < 0) {
            print_text(STDERR_FILENO, "Error: failed to close source file\n");
        }

        if (errno == EEXIST) {
            print_text(STDERR_FILENO, "Error: destination file already exists; refusing to overwrite.\n");
        } else {
            print_text(STDERR_FILENO, "Error: cannot create destination file ");
            print_path(STDERR_FILENO, dst);
            print_text(STDERR_FILENO, "\n");
        }
        return 1;
    }

    while (1) {
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read = read_retry(src_fd, buffer, sizeof(buffer));
        size_t offset = 0;

        if (bytes_read < 0) {
            print_text(STDERR_FILENO, "Error: failed while reading source file ");
            print_path(STDERR_FILENO, src);
            print_text(STDERR_FILENO, "\n");
            status = 1;
            goto cleanup;
        }

        if (bytes_read == 0) {
            status = 0;
            break;
        }

        while (offset < (size_t)bytes_read) {
            ssize_t written;

            do {
                written = write(dst_fd, buffer + offset, (size_t)bytes_read - offset);
            } while (written < 0 && errno == EINTR);

            if (written < 0) {
                print_text(STDERR_FILENO, "Error: failed while writing destination file ");
                print_path(STDERR_FILENO, dst);
                print_text(STDERR_FILENO, "\n");
                status = 1;
                goto cleanup;
            }
            if (written == 0) {
                print_text(STDERR_FILENO, "Error: failed while writing destination file.\n");
                status = 1;
                goto cleanup;
            }

            offset += (size_t)written;
        }
    }

cleanup:
    if (close_retry(dst_fd) < 0) {
        print_text(STDERR_FILENO, "Error: failed to close destination file ");
        print_path(STDERR_FILENO, dst);
        print_text(STDERR_FILENO, "\n");
        status = 1;
    }

    if (close_retry(src_fd) < 0) {
        print_text(STDERR_FILENO, "Error: failed to close source file ");
        print_path(STDERR_FILENO, src);
        print_text(STDERR_FILENO, "\n");
        status = 1;
    }

    return status;
}