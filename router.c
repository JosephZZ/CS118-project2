#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <unistd.h>

#define SERVICE_PORT 21234
#define BUFSIZE 2048

////////////////////////////////////////////////////////////////////////////////
// Types

typedef enum {
    NODE_A, NODE_B, NODE_C, NODE_D, NODE_E, NODE_F, NODE_COUNT
} node_id_t;

typedef struct {
    node_id_t src_node_id;
    node_id_t dst_node_id;
    size_t data_len;
    char data[BUFSIZE];
} datagram_t;

typedef struct {
    node_id_t src_node_id;
    int costs[NODE_COUNT];
} costs_t;

////////////////////////////////////////////////////////////////////////////////
// Functions

void router_send(char *server, char *message);
void router_listen();
void costs_to_message(costs_t *costs, char *message);
void message_to_costs(char *message, costs_t *costs);
void datagram_to_message(datagram_t *dg, char *message);
void message_to_datagram(char *message, datagram_t *dg);

////////////////////////////////////////////////////////////////////////////////
// Main

int main(int argc, char const *argv[])
{
    printf("\nStarted\n");
    size_t nchildren = 2;
    int pids[2];
    pid_t pid = fork();
    if (pid < 0) {
        // error
        perror("fork failed");
        exit(0);
    } else if (pid == 0) {
        // child
        char msg[BUFSIZE];

        // Test send costs
        costs_t costs;
        costs.src_node_id = NODE_A;
        int c;
        for (c = 0; c < NODE_COUNT; c++) {
            costs.costs[c] = c;
        }
        costs_to_message(&costs, msg);
        router_send("127.0.0.1", msg);

        // Test send datagram
        datagram_t dg;
        dg.src_node_id = NODE_A;
        dg.dst_node_id = NODE_B;
        strcpy(dg.data, "hello node b");
        dg.data_len = strlen(dg.data);
        datagram_to_message(&dg, msg);
        router_send("127.0.0.1", msg);
    } else {
        // parent
        router_listen();
    }

    return 0;
}

void router_send(char *server, char *message)
{
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("send socket failed");
        return;
    }

    // Bind socket to all local addresses and pick any port number
    struct sockaddr_in local_addr;
    memset((char *)&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(0);
    if (bind(sock_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("send bind failed");
        sock_fd = -1;
        return;
    }

    // Define remote_addr, the address to whom we want to send messages
    // Convert ip_addr to binary format using inet_aton
    struct sockaddr_in remote_addr;
    memset((char *) &remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(SERVICE_PORT);
    if (inet_aton(server, &remote_addr.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        return;
    }

    // Send message
    printf("Sending message \"%s\" to %s port %d\n", message, server, SERVICE_PORT);
    if (sendto(sock_fd, message, strlen(message), 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) == -1) {
        perror("sendto");
        return;
    }

    close(sock_fd);
}

void router_listen()
{
    // Create socket
    int sock_fd;
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("send socket failed");
        return;
    }

    // Bind socket to any valid IP address and a specific port
    struct sockaddr_in local_addr;
    memset((char *)&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(SERVICE_PORT);

    if (bind(sock_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("listen bind failed");
        return;
    }

    // Listen forever
    struct sockaddr_in remote_addr;
    socklen_t addr_len = sizeof(remote_addr);
    char msg[BUFSIZE];
    while (1) {
        // Receive
        printf("waiting on port %d\n", SERVICE_PORT);
        int recv_len = recvfrom(sock_fd, msg, BUFSIZE, 0, (struct sockaddr *)&remote_addr, &addr_len);
        if (recv_len > 0) {
            msg[recv_len] = 0;
            printf("Received message: \"%s\" (%d bytes)\n", msg, recv_len);
        } else {
            printf("receive failed\n");
            continue;
        }

        // Handle
        switch (msg[0]) {
            case 'C': {
                costs_t costs;
                message_to_costs(msg, &costs);
                printf("Received costs from %d: A=%d, B=%d, C=%d, D=%d, E=%d, F=%d\n",
                    costs.src_node_id,
                    costs.costs[NODE_A], costs.costs[NODE_B], costs.costs[NODE_C],
                    costs.costs[NODE_D], costs.costs[NODE_E], costs.costs[NODE_F]);
                break;
            }
            case 'D': {
                datagram_t dg;
                message_to_datagram(msg, &dg);
                printf("Received datagram for %d->%d (%zu): \"%s\"\n",
                    dg.src_node_id, dg.dst_node_id, dg.data_len, dg.data);
                break;
            }
            default:
                printf("invalid message\n");
                break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Messages

/* For the Control Message sent between servers,
 * send them in string, seperated by commas, in the sequence:
 *   "Type,NodeID,CostForA,CostForB,...,CostForF"
 * For type, 'C' for control, 'D' for data. So here we should use 'C'.
 * For NodeID, we use integers to represent, 0 stands for A, 1 stands for B ... 5 for F;
 * CostForA and CostForB ... and CostForF would be an integer representing the link cost
    from node indicated by NodeID to the other nodes,and they must be in the order from A to F,
    and if the cost is unknown yet, use 9999 to represent infinity.
 * eg: "C,0,0,3,5,9999,2,5"
 * meaning: it is a control message from node A, and the costs from A to A,B,...F are
    0,3,5,infinity,2,5, respectively.
 */
void costs_to_message(costs_t *costs, char *message)
{
    sprintf(message, "C,%d,%d,%d,%d,%d,%d,%d", costs->src_node_id,
        costs->costs[NODE_A], costs->costs[NODE_B], costs->costs[NODE_C],
        costs->costs[NODE_D], costs->costs[NODE_E], costs->costs[NODE_F]);
}

void message_to_costs(char *message, costs_t *costs)
{
    sscanf(message, "C,%d,%d,%d,%d,%d,%d,%d", &costs->src_node_id,
        &costs->costs[NODE_A], &costs->costs[NODE_B], &costs->costs[NODE_C],
        &costs->costs[NODE_D], &costs->costs[NODE_E], &costs->costs[NODE_F]);
}

/* For the Datagram
 * send them in string, seperated by commas, in the sequence:
 *    "Type,SourceNodeID,DestinationNodeID,LengthOfData,Data"
 * For type we should use 'D' here.
 * SourceNodeID and DestinationNodeID are represented by integers too.
 * LengthOfData is the number of bytes of the Data
 * eg: "D,2,88,somedatastringhere..."
 * Be aware that Data part might also has comma
 */
void datagram_to_message(datagram_t *dg, char *message)
{
    sprintf(message, "D,%d,%d,%zu,%s", dg->src_node_id, dg->dst_node_id, dg->data_len, dg->data);
}

void message_to_datagram(char *message, datagram_t *dg)
{
    // Find 4th comma (start of data)
    int ncommas = 0;
    size_t pos = 0;
    while (1) {
        if (message[pos] == ',') {
            if (++ncommas == 4) {
                break;
            }
        }
        pos++;
    }

    // Get dg info (not data)
    message[pos] = 0;
    sscanf(message, "D,%d,%d,%zu", &dg->src_node_id, &dg->dst_node_id, &dg->data_len);
    message[pos] = ',';

    // Get dg data
    if (dg->data_len > BUFSIZE - 1) {
        printf("datagram data too long");
    }
    strncpy(dg->data, message + pos + 1, BUFSIZE);
    dg->data[BUFSIZE] = 0;
}
