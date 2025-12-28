#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "network.h"

#define ERROR -1

int create_server_socket_and_listen(int port) {
    int err;

    int server_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sockfd < 0) {
        return ERROR;
    }

    int one = 1;
    err =
        setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (err) {
        close(server_sockfd);
        return ERROR;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    err = bind(server_sockfd, (struct sockaddr*)&server_addr,
               sizeof(server_addr));
    if (err) {
        close(server_sockfd);
        return ERROR;
    }

    err = listen(server_sockfd, 5);
    if (err == -1) {
        close(server_sockfd);
        return ERROR;
    }

    printf("[INFO] Proxy server listening on port %d\n", port);
    return server_sockfd;
}

int connect_to_remote_server(char* url, const int max_url_size) {
    char host[max_url_size], path[max_url_size];
    sscanf(url, "http://%[^/]%s", host, path);

    char* service = "http";
    const struct addrinfo hints = {.ai_family = AF_INET,
                                   .ai_socktype = SOCK_STREAM,
                                   .ai_protocol = IPPROTO_TCP};
    struct addrinfo* addresses;
    int err = getaddrinfo(host, service, &hints, &addresses);
    if (err == -1) {
        return -1;
    }
    struct addrinfo* curr = addresses;

    int server_sockfd = -1;
    while (curr != NULL) {
        int fd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
        if (fd < 0) {
            curr = curr->ai_next;
            continue;
        }

        err = connect(fd, curr->ai_addr, curr->ai_addrlen);
        if (err) {
            curr = curr->ai_next;
            close(fd);
            continue;
        }

        server_sockfd = fd;
        break;
    }

    return server_sockfd;
}
