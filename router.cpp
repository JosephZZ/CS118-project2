#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
using namespace std;


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
    int my_portnum;
    int costs[NODE_COUNT];
} costs_t;

//char ID_to_Name[6]="ABCDEF";
////////////////////////////////////////////////////////////////////////////////
// Functions

void initialize_tables(costs_t *costs, int *forward_table, int *neighbor_portnums, const char *filename);
void router_send(char *server, char *message, int recvr_portnum);
void router_listen(int *forward_table, costs_t *costs, int* neighbor_portnums);
void costs_to_message(costs_t *costs, char *message);
void message_to_costs(char *message, costs_t *costs);
void datagram_to_message(datagram_t *dg, char *message);
void message_to_datagram(char *message, datagram_t *dg);
void run_dv_algorithm(int *forward_table, costs_t *costs, costs_t *neighbor_costs);
void propagate_datagrams(datagram_t *dg);
////////////////////////////////////////////////////////////////////////////////
// Main

int main(int argc, char const *argv[])
{
    const char* filename = argv[1];
    costs_t costs;
    costs.src_node_id = static_cast<node_id_t>(atoi(argv[2]));
    int forward_table[NODE_COUNT];//subscript denotes the destination portID, 
                                  //and value denotes the outgoing node ID
    int neighbor_portnums[NODE_COUNT];//only store port number of neighbors,
                                      // for non-neibors it'll be -1
    initialize_tables(&costs, forward_table, neighbor_portnums, filename);

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
        sleep(5);
        char message[BUFSIZE];
        costs_to_message(&costs, message);
        for (int i=0; i<NODE_COUNT; i++){
            if (neighbor_portnums[i] > 0)
                router_send("127.0.0.1",message,neighbor_portnums[i]);
        }
        // Test send datagram
        // datagram_t dg;
        // dg.src_node_id = NODE_A;
        // dg.dst_node_id = NODE_B;
        // strcpy(dg.data, "hello node b");
        // dg.data_len = strlen(dg.data);
        // datagram_to_message(&dg, msg);
        // router_send("127.0.0.1", msg);
    } else {
        // parent
        router_listen(forward_table, &costs, neighbor_portnums); //listens and also reacts
    }

    return 0;
}

void error_message(string s)
{
    perror(s.c_str());
    exit(1);
}

int ConvertToSubscript(const char* letter)
{
    switch(letter[0])
    {
        case 'A': 
            return 0;
        case 'B':
            return 1;
        case 'C':
            return 2;
        case 'D':
            return 3;
        case 'E':
            return 4;
        case 'F':
            return 5;
        default:
            //Error
            return -1;
    }
    return -1;
}

void initialize_tables(costs_t *costs, int *forward_table, int *neighbor_portnums, const char *filename)
{
    FILE* file = fopen(filename, "r");
    char line[256];
    
    for (int i = 0; i < NODE_COUNT; i++){
        neighbor_portnums[i] = -1;//initialize them to be nothing
        costs->costs[i] = 9999;//initialize them to be infinity, kinda
        forward_table[i] = 9;//initialize them to be bigger than the nodeID of all nodes
                             //for the ease of calculation in the algorithm part
    }

    while (fgets(line, sizeof(line), file)) 
    {
        // Now we're parsing the line
        bool Selfsource = false;
        bool Selfport = false;
        int Destination = -1;   

        int index = 0;
        string linestr(line);
        stringstream ss(linestr);
        string token;
        while (getline(ss, token, ','))
        {
            if (index == 0)
            {
                // This is the source node in 1st column
                int Subscript = ConvertToSubscript(token.c_str());
                if (Subscript == -1)
                    error_message("Invalid file content\n");
                if (Subscript == costs->src_node_id)
                    Selfsource = true;
            }
            else if (index == 1)
            {
                // This is the destination node in 2nd column
                Destination = ConvertToSubscript(token.c_str());
                if (Destination == -1)
                    error_message("Invalid file content\n");
                else if (Destination == costs->src_node_id)
                    Selfport = true;
            }
            else if (index == 2)
            {
                // This is the third column, port number for destination
                // We only record the port number of my neighbors

                if (Selfsource && Destination != -1)
                    neighbor_portnums[Destination] = atoi(token.c_str());
                else if (Selfport)
                    costs->my_portnum = atoi(token.c_str());
            }
            else if (index == 3)
            {
                // This is the fourth column, link cost
                if (Selfsource)
                {
                    if (Destination == -1)
                        error_message("Index out of range for destination");
                    costs->costs[Destination] = atoi(token.c_str());
                }   
            }
            else
                break;
            index++;
        }
    }
        
    // This is for debugging only
    // Print out the routing table, 999 = infinity
    for (int i = 0; i < NODE_COUNT; i++) printf("Link Costs: %d | ", costs->costs[i]);

    printf("\n\n");

    // Print out all the port numbers of nodes A,B,C,D,E,F respectively as deduced from file    
    for (int i = 0; i < NODE_COUNT; i++) printf("Port %d Number: %d\n", i, neighbor_portnums[i]);
    
    fclose(file);
}


void router_send(char *server, char *message, int recvr_portnum)
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
    remote_addr.sin_port = htons(recvr_portnum);
    if (inet_aton(server, &remote_addr.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        return;
    }

    // Send message
    printf("Sending message \"%s\" to %s port %d\n", message, server, recvr_portnum);
    if (sendto(sock_fd, message, strlen(message), 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) == -1) {
        perror("sendto");
        return;
    }

    close(sock_fd);
}

void router_listen(int *forward_table, costs_t *costs, int* neighbor_portnums)
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
    local_addr.sin_port = htons(costs->my_portnum);

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
        printf("waiting on port %d\n", costs->my_portnum);

        // if (time(0)>= retTime){
        //     //notify neighbors every some seconds
        //     //printf ("I am running");
        //     costs_to_message(costs, msg);
        //     for (int i=0; i<NODE_COUNT; i++){
        //         if (neighbor_portnums[i] > 0)
        //             router_send("127.0.0.1",msg,neighbor_portnums[i]);
        //     }
        //     retTime = time(0)+3;
        // }

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
                costs_t neighbor_costs;
                message_to_costs(msg, &neighbor_costs);
                run_dv_algorithm(forward_table, costs, &neighbor_costs);
                char message[BUFSIZE];
                costs_to_message(costs, message);
                for (int i=0; i<NODE_COUNT; i++){
                    if (neighbor_portnums[i] > 0)
                        router_send("127.0.0.1",message,neighbor_portnums[i]);
                }
                printf("Received costs from %d: A=%d, B=%d, C=%d, D=%d, E=%d, F=%d\n",
                    neighbor_costs.src_node_id,
                    neighbor_costs.costs[NODE_A], neighbor_costs.costs[NODE_B], neighbor_costs.costs[NODE_C],
                    neighbor_costs.costs[NODE_D], neighbor_costs.costs[NODE_E], neighbor_costs.costs[NODE_F]);
                break;
            }
            case 'D': {
                datagram_t dg;
                message_to_datagram(msg, &dg);
                bool final_destination = false;
                if (dg.dst_node_id == costs->src_node_id) final_destination = true;
                else{
                    int next_node = forward_table[dg.dst_node_id];
                    int forwarding_port = neighbor_portnums[next_node];
                    router_send("127.0.0.1",msg,forwarding_port);
                }
                //need to print more stuff 
                printf("Received datagram for %d->%d (%zu): \"%s\"\n",
                    dg.src_node_id, dg.dst_node_id, dg.data_len, dg.data);
                break;
            }
            default:
                printf("invalid message\n");
                break;
        }
        sleep(1);
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

void run_dv_algorithm(int *forward_table, costs_t *costs, costs_t *neighbor_costs)
{
    int neighbor_ID = neighbor_costs->src_node_id;
    printf ("neighborid: %d\n",neighbor_ID);
    for (int i =0; i<6; i++)
    {
        int cost_through_neighbor;
        cost_through_neighbor = costs->costs[neighbor_ID]+neighbor_costs->costs[i];
        printf("cost_through_neighbor: %d\n",cost_through_neighbor);
        if (costs->costs[i]> cost_through_neighbor){
            costs->costs[i] = cost_through_neighbor;
            forward_table[i] = neighbor_ID;
        }
        else if (costs->costs[i] == cost_through_neighbor){
            //if the cost of the two paths are same, choose the neighbor with lowest ID
            if (forward_table[i] > neighbor_ID)
                forward_table[i] = neighbor_ID;
        }
        for (int i=0;i < (sizeof (costs->costs) /sizeof (costs->costs[0]));i++) {
            printf("%d\n",costs->costs[i]);
        }
    }

}
