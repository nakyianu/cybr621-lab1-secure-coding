#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>

#define BUFFER_SIZE 8192

/* Write every byte in buf, retrying operations interrupted by a signal. */
static int
write_all(int fd, const char *buf, size_t count)
{
    size_t written = 0;

    while (written < count) {
        ssize_t result = write(fd, buf + written, count - written);

        if (result > 0) {
            written += (size_t)result;
        } else if (result < 0 && errno == EINTR) {
            continue;
        } else {
            return -1;
        }
    }

    return 0;
}

static void
report(const char *message)
{
    size_t length = 0;

    while (message[length] != '\0') {
        ++length;
    }

    /* A reporting failure cannot usefully be recovered from. */
    (void)write_all(STDERR_FILENO, message, length);
}

/*
 * On Linux, close(2) releases the descriptor even when it returns EINTR.
 * Retrying it could close a descriptor reused by another operation, so it is
 * deliberately not retried.  Every open descriptor is still closed once.
 */
static int
close_fd(int fd)
{
    return close(fd);
}

int
main(int argc, char *argv[])
{
    char buffer[BUFFER_SIZE];
    int source_fd = -1;
    int destination_fd = -1;
    int status = 1;

    if (argc != 3 || argv[1][0] == '\0' || argv[2][0] == '\0') {
        report("usage: copy SOURCE DESTINATION\n");
        return 1;
    }

    do {
        source_fd = open(argv[1], O_RDONLY);
        if (source_fd < 0 && errno == EINTR) {
            continue;
        }
        break;
    } while (1);
    if (source_fd < 0) {
        report("copy: cannot open source file\n");
        goto cleanup;
    }

    do {
        destination_fd = open(argv[2], O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (destination_fd < 0 && errno == EINTR) {
            continue;
        }
        break;
    } while (1);
    if (destination_fd < 0) {
        report("copy: cannot create destination file\n");
        goto cleanup;
    }

    for (;;) {
        ssize_t bytes_read;

        do {
            bytes_read = read(source_fd, buffer, sizeof(buffer));
        } while (bytes_read < 0 && errno == EINTR);

        if (bytes_read < 0) {
            report("copy: cannot read source file\n");
            goto cleanup;
        }
        if (bytes_read == 0) {
            status = 0;
            break;
        }
        if (write_all(destination_fd, buffer, (size_t)bytes_read) < 0) {
            report("copy: cannot write destination file\n");
            goto cleanup;
        }
    }

cleanup:
    if (destination_fd >= 0 && close_fd(destination_fd) < 0) {
        report("copy: cannot close destination file\n");
        status = 1;
    }
    if (source_fd >= 0 && close_fd(source_fd) < 0) {
        report("copy: cannot close source file\n");
        status = 1;
    }

    return status;
}