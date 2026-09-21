#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 8192

/* Open file descriptor, keep trying only if process interrupted by signal */
static int open_retry(const char *path, int flags, mode_t mode) {
    int fd;
    do {
       fd = open(path, flags, mode); 
    } while (fd < 0 && errno == EINTR);
    
    if (fd >= 0) {
        return fd;
    }

    return -1;
}

/* 
 * Read from file descriptor
 * Attempt to read until EOF or buffer is full 
 */
static ssize_t read_retry(int fd, char *buf, size_t len) {
    ssize_t bytes_read;
    ssize_t total_bytes_read = 0;
    while(1) {
        bytes_read = read(fd, buf, len);

        if(bytes_read < 0 && errno == EINTR) {
            continue;
        }
        if(bytes_read > 0) {
            total_bytes_read += bytes_read;
            if(total_bytes_read != BUFFER_SIZE && buf[total_bytes_read] != '\0') {
                len -= total_bytes_read;
                continue;
            } 
            return total_bytes_read;
        }
        return -1;
    }
}

/* Write to file descriptor until buffer is empty or until unrecoverable error occurs. */
static int write_all(int fd, const char *buf, size_t len) {
    size_t total = 0;

    while (total < len) {
        ssize_t written;
        do {
            written = write(fd, buf, len - total);
        } while (written < 0 && errno == EINTR);

        if (written <= 0) {
            return -1;
        }
        total += (size_t)written;
    }
    return total;
}

/* Print error message to STDERR */
static int print_error(const char *text) {
    size_t length = 0;
    while (text[length] != '\0') { 
        length++;
    }
    return write_all(STDERR_FILENO, text, length);
}


/* 
 * Cleans up any open file descriptors, only tries once
 * On Linux, close(2) releases the descriptor even when it returns EINTR.
 * Retrying it could close a descriptor reused by another operation, so it is
 * deliberately not retried.
 */
static int cleanup(int src_fd, int dest_fd, int status) {
    if (src_fd >= 0 && close(src_fd) < 0) {
        print_error("Error: Cannot close source file\n");
        return 1;
    }
    if (dest_fd >= 0 && close(dest_fd) < 0) {
            print_error("Error: Cannot close destination file\n");
            return 1;
        }
    return status;
}
  

int main(int argc, char **argv) {
    int src_fd = -1;
    int dst_fd = -1;

    // Validate arguments
    if (argc != 3) {
        print_error("Usage: secure_copy SOURCE DESTINATION\n");
        return 1;
    }

    const char *src = argv[1];
    const char *dst = argv[2];

    if (src[0] == '\0' || dst[0] == '\0') {
        print_error("Error: source and destination paths must not be empty.\n");
        return 1;
    }

    // Open source file
    src_fd = open_retry(src, O_RDONLY, 0);
    if (src_fd < 0) {
        print_error("Error: cannot open source file \n");
        print_error(src);
        print_error("\n");
        return cleanup(src_fd, dst_fd, 1);
    }

    // Create destination if not available
    dst_fd = open_retry(dst, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (dst_fd < 0) {
        if (errno == EEXIST) {
            print_error("Error: Destination file already exists; refusing to overwrite.\n");
            print_error(dst);
            print_error("\n");
        } else {
            print_error("Error: Cannot create destination file \n");
        }
        return cleanup(src_fd, dst_fd, 1);
    }


    // Read from source file
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read_retry(src_fd, buffer, BUFFER_SIZE);

    if (bytes_read < 0) {
        print_error("Error: failed while reading source file \n");
        return cleanup(src_fd, dst_fd, 1);

    } else if (bytes_read == 0) {
        return cleanup(src_fd, dst_fd, 0);

    // If buffer was filled before end of file was reached, report unsuccessful copy
    } else if (bytes_read == BUFFER_SIZE) {
        print_error("Error: Source file too large to copy.\n");
        return cleanup(src_fd, dst_fd, 1);
    }

    // Write to destination file
    if(write_all(dst_fd, buffer, (size_t)bytes_read) <= 0) {
        print_error("Error: failed while writing to destination file \n");
        return cleanup(src_fd, dst_fd, 1);
    }

    return 0;
}
