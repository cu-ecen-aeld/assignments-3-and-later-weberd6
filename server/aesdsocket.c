#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define PORT 9000
#define BUFFER_SIZE 1024

bool accepting = true;
char *filename = "/var/tmp/aesdsocketdata";

static void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        accepting = false;
        syslog(LOG_DEBUG, "Caught signal, exiting");
    }
}

int main(int argc, char *argv[]) {

    int server_fd;
    int opt = 1;
    struct sockaddr_in address;
    int addrlen = sizeof(struct sockaddr_in);
    bool daemonize = false;

    if ((argc > 1) && (0 == strcmp(argv[1], "-d"))) {
        daemonize = true;
    }

    openlog("aesdsocket", 0, LOG_USER);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (daemonize) {
        if (daemon(0, 0) < 0) {
            perror("daemon");
            exit(EXIT_FAILURE);
        }
    }

    if ((listen(server_fd, 5)) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    struct sigaction a;
    a.sa_handler = signal_handler;
    a.sa_flags = 0;
    sigemptyset(&a.sa_mask);

    if (sigaction(SIGINT, &a, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGTERM, &a, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    while (accepting) {

        int accept_fd;
        char *client_address;
        size_t buffer_size = 1024;
        char *buffer = malloc(buffer_size);
        size_t buffer_len = 0;
        ssize_t nread;
        char *file_line = NULL;
        size_t file_line_len = 0;

        FILE *file = fopen(filename, "a+");
        if (!file) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }

        if ((accept_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            if (errno == EINTR) {
                accept_fd = -1;
                goto cleanup;
            }
            perror("accept");
            exit(EXIT_FAILURE);
        }
        client_address = inet_ntoa(address.sin_addr);
        syslog(LOG_DEBUG, "Accepted connection from %s", client_address);

        while ((nread = recv(accept_fd, (void*)&buffer[buffer_len], buffer_size - buffer_len, 0)) != 0) {

            if (nread == -1) {
                if (errno == EINTR)
                    break;
                perror("recv");
                accepting = false;
                break;
            }

            buffer_len += nread;

            // Process all available messages

            char *line_start = buffer;
            char *line_end;
            while ((line_end = (char*)memchr((void*)line_start, '\n', buffer_len - (line_start - buffer)))) {

                *line_end = '\0';
                fputs(line_start, file);
                fputc('\n', file);
                line_start = line_end + 1;

                // Seek to beginning of file

                fseek(file, 0, SEEK_SET);

                // Read one line at a time and send back to client

                while ((nread = getline(&file_line, &file_line_len, file)) != -1) {
                    ssize_t nsend = send(accept_fd, file_line, nread, 0);
                    if (nsend == -1) {
                        if (errno == EINTR)
                            break;
                        perror("send");
                        accepting = false;
                        goto cleanup;
                    } else if (nsend == 0) {
                        goto cleanup;
                    }
                }
            }

            // Shift unprocessed data to start of buffer

            if (line_start != buffer) {
                buffer_len -= (line_start - buffer);
                memmove(buffer, line_start, buffer_len);
            }

            // Increase buffer size and reallocate if buffer is full

            if (buffer_len == buffer_size) {

                buffer_size *= 2;

                char *new_buffer = realloc(buffer, buffer_size);
                if (!new_buffer) {
                    perror("realloc");
                    accepting = false;
                    break;
                }

                buffer = new_buffer;
            }
        }

cleanup:

        free(buffer);
        free(file_line);

        fclose(file);

        if (accept_fd > 0) {
            close(accept_fd);
        }

        syslog(LOG_DEBUG, "Closed connection from %s", client_address);
    }

    remove(filename);

    shutdown(server_fd, SHUT_RDWR);
    closelog();

    return EXIT_SUCCESS;
}
