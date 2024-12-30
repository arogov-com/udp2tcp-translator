#include <arpa/inet.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>


#define EXIT_PARAM     -2
#define EXIT_LOG       -3
#define EXIT_MALLOC    -4
#define EXIT_SOCK      -5
#define EXIT_BIND      -6
#define EXIT_CONNECT   -7
#define EXIT_EPOLL     -8
#define EXIT_SOCK_OPT  -9

#define PACKET_LEN 128
#define PREFIX_LEN 4


struct CONFIG {
    struct in_addr udp_ip;
    struct in_addr tcp_ip;
    int udp_port;
    int tcp_port;
    char *log_file_path;
    char *prefix;
};
int log_file;


void usage() {
    printf("Usage: usd2tcp [-h] -l <log_file> -s <prefix> --udp_ip <udp_ip> --udp_port <udp_port> --tcp_ip <server_address> --tcp_port <server_port>\n"
           "Options:\n"
           "  -h         : this help\n"
           "  -s         : 4 bytes prefix\n"
           "  -l         : logfile path\n"
           "  --udp_ip   : UDP packets source address\n"
           "  --udp_port : UDP listen port\n"
           "  --tcp_ip   : TCP server address\n"
           "  --tcp_port : TCP server port\n"
           "\n"
    );
}


void write_log(int fd, int console_fd, char *message) {
    #define LOG_BUFFER 80

    if(message == NULL) {
        return;
    }

    char buffer[LOG_BUFFER];
    struct timeval te;
    gettimeofday(&te, NULL);
    struct tm tm = *localtime(&te.tv_sec);
    int size = snprintf(buffer, LOG_BUFFER, "[%04i-%02i-%02i %02i:%02i:%02i.%04lu] %s\n", tm.tm_year + 1900, tm.tm_mon,
                        tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, te.tv_usec, message);

    if(fd != 0) {
        write(fd, buffer, size);
    }
    if(console_fd >= 0 || console_fd <= 2) {
        write(console_fd, buffer, size);
    }
}


void parse_args(int argc, char **argv, struct CONFIG *config) {
    const char* short_options = "l:s:h";
    const struct option long_options[] = {
        {"udp_ip", required_argument, NULL, 1},
        {"udp_port", required_argument, NULL, 2},
        {"tcp_ip", required_argument, NULL, 3},
        {"tcp_port", required_argument, NULL, 4},
        {NULL, 0, NULL, 0}
    };
    memset(config, 0, sizeof(struct CONFIG));

    int res;
    int option_index;
    struct in_addr inp;
    while ((res = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch(res) {
            case 1: if(inet_aton(optarg, &inp) == 0) {
                        fprintf(stderr, "error: invalid UDP address %s\n", optarg);
                        exit(EXIT_PARAM);
                    }
                    config->udp_ip.s_addr = inp.s_addr;
                    break;
            case 2: config->udp_port = atoi(optarg);
                    if(config->udp_port < 1 || config->udp_port > 65535) {
                        fprintf(stderr, "error: invalid UDP port %s\n", optarg);
                        exit(EXIT_PARAM);
                    }
                    break;
            case 3: if(inet_aton(optarg, &inp) == 0) {
                        fprintf(stderr, "error: invalid TCP address %s\n", optarg);
                        exit(EXIT_PARAM);
                    }
                    config->tcp_ip.s_addr = inp.s_addr;
                    break;
            case 4: config->tcp_port = atoi(optarg);
                    if(config->tcp_port < 1 || config->tcp_port > 65535) {
                        fprintf(stderr, "error: invalid TCP port %s\n", optarg);
                        exit(EXIT_PARAM);
                    }
                    break;
            case 'l': if(strlen(optarg) > PATH_MAX) {
                          fprintf(stderr, "error: invalid path length\n");
                          exit(EXIT_PARAM);
                      }
                      config->log_file_path = optarg;
                      break;
            case 's': if(strlen(optarg) != 4) {
                          fprintf(stderr, "error: prefix must be 4 bytes long\n");
                          exit(EXIT_PARAM);
                      }
                      config->prefix = optarg;
                      break;
            case 'h': usage();
                      exit(0);
            default: usage();
                     exit(EXIT_PARAM);
        }
    }

    if(config->log_file_path == NULL) {
        fprintf(stderr, "error: log file path must be specified\n");
        exit(EXIT_PARAM);
    }
    if(config->prefix == NULL) {
        fprintf(stderr, "error: prefix must be specified\n");
        exit(EXIT_PARAM);
    }
    if(config->tcp_ip.s_addr == 0) {
        fprintf(stderr, "error: TCP IP must be specified\n");
        exit(EXIT_PARAM);
    }
    if(config->udp_ip.s_addr == 0) {
        fprintf(stderr, "error: UDP IP must be specified\n");
        exit(EXIT_PARAM);
    }
    if(config->tcp_port == 0) {
        fprintf(stderr, "error: TCP port must be specified\n");
        exit(EXIT_PARAM);
    }
    if(config->udp_port == 0) {
        fprintf(stderr, "error: UDP port must be specified\n");
        exit(EXIT_PARAM);
    }
}


int connect_tcp(in_addr_t addr, int tcp_port) {
    int sock_tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sock_tcp == -1) {
        return EXIT_SOCK;
    }

    struct sockaddr_in tcp_addr;
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_port = htons(tcp_port);
    tcp_addr.sin_addr.s_addr = addr;

    while(connect(sock_tcp, (struct sockaddr *) &tcp_addr, sizeof(tcp_addr)) == -1) {
        write_log(log_file, 0, "trying to connect to the server");
        sleep(1);
    }

    int flags = fcntl(sock_tcp, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if(fcntl(sock_tcp, F_SETFL, flags) == -1) {
        write_log(log_file, 2, "couldn\'t change tcp socket");
        return EXIT_SOCK;
    }
    write_log(log_file, 2, "TCP connection has been established");
    return sock_tcp;
}


int bind_udp(in_addr_t addr, int udp_port) {
    int sock_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock_udp == -1) {
        write_log(log_file, 2, "couldn\'t create udp socket");
        return EXIT_SOCK;
    }

    int flags = fcntl(sock_udp, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if(fcntl(sock_udp, F_SETFL, flags) == -1) {
        write_log(log_file, 2, "couldn\'t change udp socket");
        return EXIT_SOCK_OPT;
    }

    struct sockaddr_in udp_addr;
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(udp_port);
    udp_addr.sin_addr.s_addr = addr;
    if(bind(sock_udp, (struct sockaddr *) &udp_addr, sizeof(udp_addr)) < 0) {
        write_log(log_file, 2, "couldn\'t bind udp socket");
        return EXIT_BIND;
    }
    write_log(log_file, 0, "listen to UDP");
    return sock_udp;
}

int main(int argc, char *argv[]) {
    int ret;
    struct CONFIG config;
    parse_args(argc, argv, &config);

    log_file = open(config.log_file_path, O_WRONLY | O_CREAT | O_APPEND | O_NONBLOCK, 0644);
    if(log_file == -1) {
        fprintf(stderr, "couldn\'t open log file\n");
        ret = EXIT_LOG;
        goto L_RET;
    }

    char *socket_buffer = malloc(PACKET_LEN + PREFIX_LEN);
    if(socket_buffer == NULL) {
        fprintf(stderr, "couldn\'t malloc socket buffer\n");
        ret = EXIT_MALLOC;
        goto L_MALLOC;
    }
    memcpy(socket_buffer, config.prefix, PREFIX_LEN);

    int sock_udp = bind_udp(config.udp_ip.s_addr, config.udp_port);
    if(sock_udp < 0) {
        ret = sock_udp;
        goto L_SOCKET;
    }

    int sock_tcp = connect_tcp(config.tcp_ip.s_addr, config.tcp_port);
    if(sock_tcp < 0) {
        ret = sock_tcp;
        goto L_SOCKET;
    }

    struct epoll_event ev, events[2];
    int nfds, epollfd;
    epollfd = epoll_create1(0);
    if(epollfd == -1) {
        write_log(log_file, 0, "couldn\'t create epoll");
        ret = EXIT_EPOLL;
        goto L_EPOLL;
    }

    ev.events = EPOLLIN;
    ev.data.fd = sock_udp;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, sock_udp, &ev) == -1) {
        write_log(log_file, 0, "couldn\'t add udp socket to epoll");
        ret = EXIT_EPOLL;
        goto L_CLEANALL;
    }

    ev.events = EPOLLHUP | EPOLLRDHUP | EPOLLERR;
    ev.data.fd = sock_tcp;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, sock_tcp, &ev) == -1) {
        write_log(log_file, 0, "couldn\'t add tcp socket to epoll");
        ret = EXIT_EPOLL;
        goto L_CLEANALL;
    }

    while(1) {
        nfds = epoll_wait(epollfd, events, 2, -1);
        if(nfds == -1) {
            write_log(log_file, 0, "epoll_wait error");
            ret = EXIT_FAILURE;
            goto L_CLEANALL;
        }
        for(int i = 0; i != nfds; ++i) {
            if(events[i].data.fd == sock_udp) {
                struct sockaddr_in from;
                socklen_t fromlen = sizeof(from);
                int recvd = recvfrom(events[i].data.fd, socket_buffer + PREFIX_LEN, PACKET_LEN, 0, (struct sockaddr*)&from, &fromlen);
                if(recvd == -1) {
                    write_log(log_file, 2, "couldn't receive packet");
                    continue;
                }
                else {
                    if(send(sock_tcp, socket_buffer, recvd + PREFIX_LEN, MSG_NOSIGNAL) == -1) {
                        write_log(log_file, 2, "couldn't send packet to the server");
                    }
                }
            }
            else if(events[i].data.fd == sock_tcp) {
                write_log(log_file, 0, "connection to the server has been lost");
                close(events[i].data.fd);
                sock_tcp = connect_tcp(config.tcp_ip.s_addr, config.tcp_port);
                ev.events = EPOLLHUP | EPOLLRDHUP | EPOLLERR;
                ev.data.fd = sock_tcp;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, sock_tcp, &ev);
                write_log(log_file, 0, "connection to the server has been restored");
            }
        }
    }

    L_CLEANALL:
        close(epollfd);
    L_EPOLL:
        close(sock_tcp);
        close(sock_udp);
    L_SOCKET:
        free(socket_buffer);
    L_MALLOC:
        close(log_file);
    L_RET:
    return ret;
}
