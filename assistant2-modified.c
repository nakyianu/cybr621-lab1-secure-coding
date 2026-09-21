#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 8192

/* Open file from given filepath, attempts to reopen if interrupted by signal*/
static int open_fd(const char *filepath, int flags, mode_t mode) {
    int fd;
    do {
        fd = open(filepath, flags, mode);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) {
        return -1;
    }
    return fd;
}

/* Reads from source file, ensures that the entire file is read or until buffer is full */
static int read_fd(int fd, char* buf, size_t length) {
    ssize_t bytes_read;
    ssize_t total_bytes_read = 0;
    while(1) {
        bytes_read = read(fd, buf, length);

        if(bytes_read < 0 && errno == EINTR) {
            continue;
        }
        if(bytes_read > 0) {
            total_bytes_read += bytes_read;
            if(total_bytes_read != BUFFER_SIZE && buf[total_bytes_read] != '\0') {
                length -= total_bytes_read;
                continue;
            } 
            return total_bytes_read;
        }
        return -1;
    }
}

/* Write every byte in buf, retrying operations interrupted by a signal. */
static int write_fd(int fd, const char *buf, size_t count) {
    size_t written = 0;
    ssize_t result;

    while (written < count ) {
        do {
            result = write(fd, buf + written, count - written);
        } while(result < 0 && errno == EINTR);
    
        if (result > 0) {
            written += (size_t)result;
        } else {
            return -1;
        }
    }
    return 0;
}


/* Writes a message to stderr*/
static void report(const char *message) {
    size_t length = 0;

    while (message[length] != '\0') {
        ++length;
    }
    /* A reporting failure cannot usefully be recovered from. */
    (void)write_fd(STDERR_FILENO, message, length);
}

/* 
 * Cleans up any open file descriptors, only tries once
 *
 * On Linux, close(2) releases the descriptor even when it returns EINTR.
 * Retrying it could close a descriptor reused by another operation, so it is
 * deliberately not retried.
 */
int cleanup(int src_fd, int dest_fd, int status) {
    if (src_fd >= 0 && close(src_fd) < 0) {
        report("Error: Cannot close source file.\n");
        return 1;
    }
    if (dest_fd >= 0 && close(dest_fd) < 0) {
            report("Error: Cannot close destination file.\n");
            return 1;
        }
    return status;
}
    

int main(int argc, char *argv[]) {
    char buffer[BUFFER_SIZE];
    int source_fd = -1;
    int destination_fd = -1;
    ssize_t bytes_read;

    // Validate arguments
    if (argc != 3 || argv[1][0] == '\0' || argv[2][0] == '\0') {
        report("usage: copy SOURCE DESTINATION\n");
        return 1;
    }

    // Open source file
    source_fd = open_fd(argv[1], O_RDONLY, 0);
    if (source_fd < 0) {
        report("Error: Cannot open source file\n");
        return cleanup(source_fd, destination_fd, 1);
    }

    // Create destination if not available
    destination_fd = open_fd(argv[2], O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (destination_fd < 0) {
        if (errno == EEXIST) {
            report("Error: Destination file already exists; refusing to overwrite.\n");
        } else {
            report("Error: Cannot create destination file.\n");
        }
        return cleanup(source_fd, destination_fd, 1);
    }

    // Read from source file
    bytes_read = read_fd(source_fd, buffer, BUFFER_SIZE);
    if (bytes_read < 0) {
        report("Error: Cannot read source file.\n");
        return cleanup(source_fd, destination_fd, 1);

    } else if (bytes_read == 0) {
        return cleanup(source_fd, destination_fd, 0);

    // If buffer was filled before end of file was reached, report unsuccessful copy
    } else if (bytes_read == BUFFER_SIZE) {
        report("Error: Source file too large to copy.\n");
        return cleanup(source_fd, destination_fd, 1);
    }

    // Write to destination file
    if (write_fd(destination_fd, buffer, (size_t)bytes_read) <= 0) {
        report("Error: Cannot write to destination file.\n");
        return cleanup(source_fd, destination_fd, 1);
    }
    
    return 0;
}
