#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "util.h"

char* get_ipv4_address(char *hostname) {
    struct addrinfo hints, *res;
    int status;
    char *ipstr = malloc(sizeof(char) * INET_ADDRSTRLEN);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;

    if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return NULL;
    }

    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    inet_ntop(res->ai_family, &(ipv4->sin_addr), ipstr, sizeof(char) * INET_ADDRSTRLEN);

    freeaddrinfo(res);

    return ipstr;
}








