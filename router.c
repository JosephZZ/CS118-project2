#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define SERVICE_PORT 21234
#define BUFSIZE 2048

void router_send(char * server, char * message)
{
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    int slen = sizeof(remote_addr);

    // Create socket
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket failed");
        return;
    }

    // Bind socket to all local addresses and pick any port number
    memset((char *)&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(0);

    if (bind(sock_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind failed");
        return;
    }

    // Define remote_addr, the address to whom we want to send messages
    // Convert ip_addr to binary format using inet_aton
    memset((char *) &remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(SERVICE_PORT);
    if (inet_aton(server, &remote_addr.sin_addr)==0) {
        fprintf(stderr, "inet_aton() failed\n");
        return;
    }

    // Send message
    printf("Sending message \"%s\" to %s port %d\n", message, server, SERVICE_PORT);
    if (sendto(sock_fd, message, strlen(message), 0, (struct sockaddr *)&remote_addr, slen)==-1) {
        perror("sendto");
        return;
    }

    // Wait for ack
    char buf[BUFSIZE];
    int recv_len = recvfrom(sock_fd, buf, BUFSIZE, 0, (struct sockaddr *)&remote_addr, &slen);
    if (recv_len >= 0) {
        buf[recv_len] = 0; // Terminate string
        printf("received message: \"%s\"\n", buf);
    }

    close(sock_fd);
}

void router_listen()
{
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    socklen_t addr_len = sizeof(remote_addr);
    int recv_len;
    int sock_fd;
    int msg_cnt = 0;
    unsigned char msg_buf[BUFSIZE];

    // Create socket
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("cannot create socket\n");
        return;
    }

    // Bind socket to any valid IP address and a specific port
    memset((char *)&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(SERVICE_PORT);

    if (bind(sock_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind failed");
        return;
    }

    // Receive data
    while (1) {
        printf("waiting on port %d\n", SERVICE_PORT);
        recv_len = recvfrom(sock_fd, msg_buf, BUFSIZE, 0, (struct sockaddr *)&remote_addr, &addr_len);
        if (recv_len > 0) {
            msg_buf[recv_len] = 0;
            printf("received message: \"%s\" (%d bytes)\n", msg_buf, recv_len);
        } else {
            printf("uh oh - something went wrong!\n");
        }
        sprintf(msg_buf, "ack %d", msg_cnt++);
        printf("sending response \"%s\"\n", msg_buf);
        if (sendto(sock_fd, msg_buf, strlen(msg_buf), 0, (struct sockaddr *)&remote_addr, addr_len) < 0) {
            perror("sendto");
        }
    }
}

int main(int argc, char const *argv[])
{
    size_t nchildren = 2;
    int pids[2];
    for (size_t child = 0; child < nchildren; child++) {
        pids[child] = fork();
        if (pids[child] < 0) { // error
            perror("fork failed");
            exit(0);
        } else if (pids[child] == 0) { // in child
            switch (child) {
                case 0:
                    router_listen();
                    exit(0);
                case 1:
                    router_send("127.0.0.1", "Hello");
                    exit(0);
            }
        }
    }

    for (size_t child = 0; child < nchildren; child++) {
        if (waitpid(pids[child], NULL, 0) != pids[child]) {
            perror("waitpid failed");
        }
    }

    return 0;
}
