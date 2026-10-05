#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"

static volatile sig_atomic_t exit_requested = 0;

static void handle_signal(int signal_number){
    (void)signal_number;
    exit_requested = 1;
}

// Reads from client_fd until a '\n' is seen. On success, returns a malloc'd
// buffer holding exactly one complete packet (including the trailing '\n')
// and sets *packet_len to its size. A single recv() can return more than one
// newline-terminated packet at once; any bytes past the first '\n' are handed
// back via *leftover/*leftover_len (caller-owned, initialize to NULL/0 before
// the first call) so the next call consumes them before doing another recv(),
// instead of merging them into the current packet or silently dropping them.
// Returns NULL on malloc failure, recv() error, or if the peer closes the
// connection before sending a newline (any pending fragment is discarded).
char *receive_packet(int client_fd, size_t *packet_len, char **leftover, size_t *leftover_len){
    size_t capacity = 1024;
    size_t total = 0;
    char *buffer = malloc(capacity);
    if(buffer == NULL){
        syslog(LOG_ERR,"malloc() failed with message: %s", strerror(errno));
        return NULL;
    }

    // consume any bytes left over from a previous call before reading more
    if(*leftover_len > 0){
        while(*leftover_len > capacity){
            capacity *= 2;
        }
        char *bigger = realloc(buffer, capacity);
        if(bigger == NULL){
            syslog(LOG_ERR,"realloc() failed with message: %s", strerror(errno));
            free(buffer);
            return NULL;
        }
        buffer = bigger;
        memcpy(buffer, *leftover, *leftover_len);
        total = *leftover_len;
        free(*leftover);
        *leftover = NULL;
        *leftover_len = 0;
    }

    char *newline_pos = memchr(buffer, '\n', total);

    while(newline_pos == NULL){
        if(total == capacity){
            capacity *= 2;
            char *new_buffer = realloc(buffer, capacity);
            if(new_buffer == NULL){
                syslog(LOG_ERR,"realloc() failed with message: %s", strerror(errno));
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
        }

        ssize_t bytes_received = recv(client_fd, buffer + total, capacity - total, 0);
        if(bytes_received == -1){
            if(errno != EINTR){
                syslog(LOG_ERR,"recv() failed with message: %s", strerror(errno));
            }
            free(buffer);
            return NULL;
        }
        if(bytes_received == 0){
            // peer closed the connection before sending a full packet
            free(buffer);
            return NULL;
        }

        // only need to scan the newly-received bytes for '\n'
        newline_pos = memchr(buffer + total, '\n', (size_t)bytes_received);
        total += (size_t)bytes_received;
    }

    size_t this_packet_len = (size_t)(newline_pos - buffer) + 1;
    size_t remainder_len = total - this_packet_len;

    if(remainder_len > 0){
        *leftover = malloc(remainder_len);
        if(*leftover == NULL){
            syslog(LOG_ERR,"malloc() failed with message: %s", strerror(errno));
            // not fatal to the packet already assembled; the remainder is lost
        }
        else{
            memcpy(*leftover, buffer + this_packet_len, remainder_len);
            *leftover_len = remainder_len;
        }
    }

    *packet_len = this_packet_len;
    return buffer;
}

int connectSocket(char port[], int run_as_daemon){
    struct addrinfo hints;
    struct addrinfo *servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1){
        syslog(LOG_ERR,"sigaction() failed with mesage: %s", strerror(errno));
        return -1;
    }

    int addressinfo = getaddrinfo(NULL, port, &hints, &servinfo);
    if(addressinfo != 0){
        // getaddrinfo failed
        syslog(LOG_ERR,"getaddrinfo failed with mesage: %s", gai_strerror(addressinfo));
        return -1;
    }

    int fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if(fd == -1){
        // socket failed
        syslog(LOG_ERR,"socket() failed with mesage: %s", strerror(errno));
        freeaddrinfo(servinfo);
        return -1;
    }

    int bind_result = bind(fd, servinfo->ai_addr, servinfo->ai_addrlen);
    freeaddrinfo(servinfo);
    servinfo = NULL;
    if(bind_result != 0){
        // bind failed
        syslog(LOG_ERR,"bind() failed with mesage: %s", strerror(errno));
        close(fd);
        return -1;
    }

    if(run_as_daemon){
        pid_t pid = fork();
        if(pid == -1){
            syslog(LOG_ERR,"fork() failed with mesage: %s", strerror(errno));
            close(fd);
            return -1;
        }
        if(pid > 0){
            // parent's job is done once the child is running the service
            close(fd);
            exit(0);
        }

        // child: detach from the controlling terminal/session
        if(setsid() == -1){
            syslog(LOG_ERR,"setsid() failed with mesage: %s", strerror(errno));
            close(fd);
            return -1;
        }

        int devnull = open("/dev/null", O_RDWR);
        if(devnull != -1){
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if(devnull > STDERR_FILENO){
                close(devnull);
            }
        }
    }

    int listen_result = listen(fd, 10);
    if(listen_result != 0){
        // listen failed
        syslog(LOG_ERR,"listen() failed with mesage: %s", strerror(errno));
        close(fd);
        return -1;
    }

    while(!exit_requested){
        struct sockaddr_storage client_address;
        socklen_t client_addr_len = sizeof(client_address);

        int accept_result = accept(fd, (struct sockaddr *)&client_address, &client_addr_len);
        if(accept_result == -1){
            if(errno == EINTR){
                // interrupted by SIGINT/SIGTERM
                break;
            }
            syslog(LOG_ERR,"accept() failed with message: %s", strerror(errno));
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        struct sockaddr_in *client_addr_in = (struct sockaddr_in *)&client_address;
        size_t packet_size = 0;
        inet_ntop(AF_INET, &(client_addr_in->sin_addr), client_ip, sizeof(client_ip));
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        char *packet;
        char *leftover = NULL;
        size_t leftover_len = 0;
        do {
            packet = receive_packet(accept_result, &packet_size, &leftover, &leftover_len);
            if(packet != NULL){
                int data_fd = open(DATA_FILE_PATH, O_CREAT | O_APPEND | O_WRONLY, 0644);
                if(data_fd == -1){
                    syslog(LOG_ERR,"open() failed with mesage: %s", strerror(errno));
                }
                else{
                    ssize_t written = write(data_fd, packet, packet_size);
                    if(written == -1 || (size_t)written != packet_size){
                        syslog(LOG_ERR,"write() failed with mesage: %s", strerror(errno));
                    }
                    close(data_fd);
                }
                // send the file's content back to the client
                data_fd = open(DATA_FILE_PATH, O_RDONLY);
                if(data_fd == -1){
                    syslog(LOG_ERR,"open() failed with mesage: %s", strerror(errno));
                }
                else{
                    char read_buf[1024];
                    ssize_t bytes_read;
                    while((bytes_read = read(data_fd, read_buf, sizeof(read_buf))) > 0){
                        ssize_t total_sent = 0;
                        while(total_sent < bytes_read){
                            ssize_t sent = send(accept_result, read_buf + total_sent, (size_t)(bytes_read - total_sent), 0);
                            if(sent == -1){
                                syslog(LOG_ERR,"send() failed with mesage: %s", strerror(errno));
                                break;
                            }
                            total_sent += sent;
                        }
                    }
                    if(bytes_read == -1){
                        syslog(LOG_ERR,"read() failed with mesage: %s", strerror(errno));
                    }
                    close(data_fd);
                }

                free(packet);
            }
        } while(packet != NULL && !exit_requested);

        free(leftover);
        close(accept_result);

        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    syslog(LOG_INFO, "Caught signal, exiting");
    close(fd);
    if(unlink(DATA_FILE_PATH) == -1 && errno != ENOENT){
        syslog(LOG_ERR,"unlink() failed with mesage: %s", strerror(errno));
    }

    return 0;
}

int main(int argc, char *argv[]){
    int run_as_daemon = (argc > 1 && strcmp(argv[1], "-d") == 0);
    return connectSocket("9000", run_as_daemon);
}
