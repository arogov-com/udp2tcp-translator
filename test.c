#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define THRESHOLD    100000
#define BUFFER_SIZE  132

int sock = 0;
int client_socket = 0;
size_t counter = 0;
size_t misses = 0;

void sigint_handler(int signum) {
    close(client_socket);
    close(sock);

    printf("Counter = %ld\nMisses = %ld\n", counter, misses);
    exit(2);
}


int main(int argc, char const *argv[]) {
    if(argc != 4) {
        printf("Usage: %s <s|g> <ip> <port>", argv[0]);
        return 1;
    }

    signal(SIGINT, sigint_handler);

    char buffer[BUFFER_SIZE] = {'1'};
    for(int i = 0; i != 40; ++i) {
        buffer[i] = '0';
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(atoi(argv[3]));
    server_address.sin_addr.s_addr = inet_addr(argv[2]);
    if(argv[1][0] == 'g') {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if(sock == -1) {
            perror("");
            return 1;
        }

        size_t threshold = 0;
        while(1) {
            sendto(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&server_address, sizeof(server_address));
            ++counter;
            for(int i = 39; i >= 0; --i) {
                if(buffer[i] == '9') {
                    buffer[i] = '0';
                }
                else {
                    ++buffer[i];
                    break;
                }
            }

            if(counter >= threshold) {
                printf("counter %ld\n", counter);
                threshold += THRESHOLD;
            }

        }
    }
    else if(argv[1][0] == 's') {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if(sock == -1) {
            perror("");
            return 1;
        }
        if(bind(sock, (struct sockaddr*) &server_address, sizeof(server_address)) == -1) {
            perror("");
            return 1;
        }
        if(listen(sock, 5) == -1) {
            perror("");
            return 1;
        }
        while(1) {
            client_socket = accept(sock, NULL, NULL);
            size_t threshold = 0;
            char last = 0;
            while(1) {
                if(recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
                    break;
                }
                ++counter;

                if(last + 1 != buffer[43] && !(last == '9' && buffer[43] == '0')) {
                    ++misses;
                }
                last = buffer[43];

                if(counter >= threshold) {
                    printf("counter %ld\n", counter);
                    threshold += THRESHOLD;
                }
            }
        }
    }
    close(sock);

    return 0;
}