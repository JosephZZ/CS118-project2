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
#include <pthread.h>
#include <ctime>

using namespace std;

#define INFINTY 9999
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

////////////////////////////////////////////////////////////////////////////////
// Functions

void initialize_tables(costs_t *costs, int *forward_table, int *neighbor_portnums, const char *filename);

void *router_listen_thread(void *args);
void router_listen(int *forward_table, costs_t *costs, int* neighbor_portnums);

void *router_broadcast_thread(void *args);
void router_broadcast(costs_t *costs, int* neighbor_portnums);
void broadcast_costs(costs_t *costs, int* neighbor_portnums);

void router_send(const char *server, const char *message, int recvr_portnum);
void costs_to_message(costs_t *costs, char *message);
void message_to_costs(char *message, costs_t *costs);
void datagram_to_message(datagram_t *dg, char *message);
void message_to_datagram(char *message, datagram_t *dg);
void message_terminate(char *message, costs_t *costs);

bool run_dv_algorithm(int *forward_table, costs_t *costs, costs_t *neighbor_costs);
void propagate_datagrams(datagram_t *dg);

void print_separator();
void print_costs(costs_t *costs);
void print_ports(int *neighbor_portnums);

void log(node_id_t node_id, const char *prefix, const char *text);
void log_routing_table(int *forward_table, costs_t *costs);
void log_datagram_forward(datagram_t *dg, node_id_t thisnode, int thisport, int fwdport);
void log_datagram_received(datagram_t *dg, node_id_t thisnode);

string ConverToName="ABCDEF";
int ConvertToSubscript(const char* letter);

////////////////////////////////////////////////////////////////////////////////
// Globals

pthread_mutex_t g_costs_mutex;
costs_t g_costs;

pthread_mutex_t g_forward_table_mutex;
int g_forward_table[NODE_COUNT];// subscript denotes the destination portID,
                                // and value denotes the outgoing node ID

////////////////////////////////////////////////////////////////////////////////
// Main

void print_help()
{
    printf("2 options:\n");
    printf("     run router:  router <init file> <node id>\n");
    printf("  send datagram:  router <init file> <node id> -d <message> \n");
}

int main(int argc, char const *argv[])
{
    if (argc < 2) {
        // Invalid
        print_help();
    } else {
        // Init
        const char *filename = argv[1];
        g_costs.src_node_id = static_cast<node_id_t>(atoi(argv[2]));
        int neighbor_portnums[NODE_COUNT];//only store port number of neighbors,
                                          // for non-neibors it'll be -1
        initialize_tables(&g_costs, g_forward_table, neighbor_portnums, filename);

        printf("******************\nServer %c started.\n",ConverToName[g_costs.src_node_id]);
        print_ports(neighbor_portnums);
        print_costs(&g_costs);

        if (argc == 3) {
            // Run router
            pthread_t threads[2];
            pthread_mutex_init(&g_costs_mutex, NULL);
            pthread_mutex_init(&g_forward_table_mutex, NULL);
            for(int i = 0; i < 2; i++) {
                if (i == 0) {
                    void *args[2];
                    args[0] = (void *)&g_costs;
                    args[1] = (void *)neighbor_portnums;
                    pthread_create(threads + i, NULL, &router_broadcast_thread, args);
                } else {
                    void *args[3];
                    args[0] = (void *)g_forward_table;
                    args[1] = (void *)&g_costs;
                    args[2] = (void *)neighbor_portnums;
                    pthread_create(threads + i, NULL, &router_listen_thread, args);
                }
            }

            //Sony's code
            int input;
            while(1)
            {
                scanf("%d", &input);
                if (input == 1)
                {
                    // User ended the router
                    // Broadcast the message to all neighbors
                    char x_message[BUFSIZE];
                    sprintf(x_message,"X,%d,9999,9999,9999,9999,9999,9999",g_costs.src_node_id); 
                    //printf("Node %d terminated\n",g_costs.src_node_id);               

                    for (int i = 0; i < NODE_COUNT; i++)
                    {
                        if (neighbor_portnums[i] > 0)
                        {
                            router_send("127.0.0.1",x_message,neighbor_portnums[i]);
                            printf("Sent death message %s\n",x_message);
                        }
                    }
                    exit(0);    
                }
            }
            // Wait on the other threads (never reached)
            for(int i = 0; i < 2; i++) {
                pthread_join(threads[i], NULL);
            }
        } else if (argc == 5 && strcmp(argv[3], "-d") == 0) {
            // Send message
            const char *message = argv[4];
            router_send("127.0.0.1", message, g_costs.my_portnum);
        } else {
            // Invalid
            print_help();
        }
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
        if (i==costs->src_node_id)
            costs->costs[i] = 0;//no cost to go to itself
        else
            costs->costs[i] = INFINTY;//initialize them to be infinity, kinda
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

                if (Selfsource && Destination != -1) {
                    forward_table[Destination] = Destination;
                    neighbor_portnums[Destination] = atoi(token.c_str());
                }
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


    fclose(file);
}

void print_separator()
{
    printf("  ********************\n");
}

void print_costs(costs_t *costs)
{
    print_separator();
    // Print out the routing table
    for (int i = 0; i < NODE_COUNT; i++) {
        if (costs->costs[i] != INFINTY) {
            printf("  *  Node %c cost: %d\n", ConverToName[i], costs->costs[i]);
        }
    }
    print_separator();
    printf("\n");
}

void print_ports(int *neighbor_portnums)
{
    printf("\n");
    print_separator();
    // Print out all the port numbers of nodes A,B,C,D,E,F respectively as deduced from file
    for (int i = 0; i < NODE_COUNT; i++) {
        if (neighbor_portnums[i] > 0) {
            printf("  *  Node %c port: %d\n", ConverToName[i], neighbor_portnums[i]);
        }
    }
    print_separator();
    printf("\n");
}

////////////////////////////////////////////////////////////////////////////////
// Broadcast

void *router_broadcast_thread(void *args)
{
    void **a = (void **)args;
    router_broadcast((costs_t *)a[0], (int *)a[1]);
    return NULL;
}

void router_broadcast(costs_t *costs, int *neighbor_portnums)
{
    while (1) {
        broadcast_costs(costs, neighbor_portnums);
        sleep(5);
    }
}

void broadcast_costs(costs_t *costs, int* neighbor_portnums)
{
    char message[BUFSIZE];
    pthread_mutex_lock(&g_costs_mutex);
    costs_to_message(costs, message);
    pthread_mutex_unlock(&g_costs_mutex);
    for (int i = 0; i < NODE_COUNT; i++){
        if (neighbor_portnums[i] > 0){
            router_send("127.0.0.1", message, neighbor_portnums[i]);
        }
    }
    printf("Broadcasting costs: A=%d, B=%d, C=%d, D=%d, E=%d, F=%d\n",
            costs->costs[NODE_A], costs->costs[NODE_B], costs->costs[NODE_C],
            costs->costs[NODE_D], costs->costs[NODE_E], costs->costs[NODE_F]);
}

////////////////////////////////////////////////////////////////////////////////
// Listen

void *router_listen_thread(void *args)
{
    void **a = (void **)args;
    router_listen((int *)a[0], (costs_t *)a[1], (int *)a[2]);
    return NULL;
}

void router_listen(int *forward_table, costs_t *costs, int *neighbor_portnums)
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
        // printf("Number on port %d\n", costs->my_portnum);

        int recv_len = recvfrom(sock_fd, msg, BUFSIZE, 0, (struct sockaddr *)&remote_addr, &addr_len);
        if (recv_len > 0) {
            msg[recv_len] = 0;
            // printf("Received message: \"%s\" (%d bytes)\n", msg, recv_len);
        } else {
            printf("receive failed\n");
            continue;
        }

        // Handle
        switch (msg[0]) {
            case 'C': {
                costs_t neighbor_costs;
                message_to_costs(msg, &neighbor_costs);

                if (run_dv_algorithm(forward_table, costs, &neighbor_costs)) {
                    // Log if changes were made
                    printf("\nNew costs from %c (A=%d, B=%d, C=%d, D=%d, E=%d, F=%d) caused update:\n",
                        ConverToName[neighbor_costs.src_node_id],
                        neighbor_costs.costs[NODE_A], neighbor_costs.costs[NODE_B], neighbor_costs.costs[NODE_C],
                        neighbor_costs.costs[NODE_D], neighbor_costs.costs[NODE_E], neighbor_costs.costs[NODE_F]);
                    print_costs(costs);
                }

                // printf("Received costs from %d: A=%d, B=%d, C=%d, D=%d, E=%d, F=%d\n",
                //
                break;
            }
            case 'D': {
                datagram_t dg;
                message_to_datagram(msg, &dg);
                if (dg.dst_node_id == costs->src_node_id) {
                    log_datagram_received(&dg, costs->src_node_id);
                } else {
                    pthread_mutex_lock(&g_forward_table_mutex);
                    int next_node = forward_table[dg.dst_node_id];
                    pthread_mutex_unlock(&g_forward_table_mutex);
                    int forwarding_port = neighbor_portnums[next_node];
                    router_send("127.0.0.1",msg,forwarding_port);
                    log_datagram_forward(&dg, costs->src_node_id, costs->my_portnum, forwarding_port);
                }
                break;
            }
            //Sony's code
            case 'X': {
                costs_t neighbor_costs; 
                message_terminate(msg, &neighbor_costs);
                printf("Received shutdown message from %c\n", ConverToName[neighbor_costs.src_node_id]); 
                // Check and update the tables
                for (int i = 0;i < NODE_COUNT;i++)
                {
                    if (forward_table[i] == neighbor_costs.src_node_id)
                    {
                        // Reset all infos to blank for this node
                        forward_table[i] = -1;
                        costs->costs[i] = 9999;
                    }
                }
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

void router_send(const char *server, const char *message, int recvr_portnum)
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
    //printf("Sending message \"%s\" to %s port %d\n", message, server, recvr_portnum);

    if (sendto(sock_fd, message, strlen(message), 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) == -1) {
        perror("sendto");
        return;
    }

    close(sock_fd);
}

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
//Sony's code
void message_terminate(char *message, costs_t *costs)
{
    sscanf(message, "X,%d,%d,%d,%d,%d,%d,%d", &costs->src_node_id, &costs->costs[NODE_A], &costs->costs[NODE_B], &costs->costs[NODE_C],&costs->costs[NODE_D], &costs->costs[NODE_E], &costs->costs[NODE_F]);
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
    strncpy(dg->data, message + pos + 1, BUFSIZE - 1);
    dg->data[BUFSIZE - 1] = 0;
}

// Returns true if changes made
bool run_dv_algorithm(int *forward_table, costs_t *costs, costs_t *neighbor_costs)
{
    bool changed = false;
    pthread_mutex_lock(&g_costs_mutex);

    int neighbor_ID = neighbor_costs->src_node_id;
    //printf ("neighborid: %d\n",neighbor_ID);
    for (int i =0; i<6; i++)
    {
        int cost_through_neighbor;
        cost_through_neighbor = costs->costs[neighbor_ID]+neighbor_costs->costs[i];
        //printf("cost_through_neighbor: %d\n",cost_through_neighbor);

        //Sony's code
        if (neighbor_ID == forward_table[i] && cost_through_neighbor > costs->costs[i])
        {
            costs->costs[i] = cost_through_neighbor;
            pthread_mutex_lock(&g_forward_table_mutex);
            if (cost_through_neighbor >= 9999)
            forward_table[i] = -1; // Invalid
            pthread_mutex_unlock(&g_forward_table_mutex);
            changed = true;
        }
        
            if (costs->costs[i] > cost_through_neighbor){
                costs->costs[i] = cost_through_neighbor;
                pthread_mutex_lock(&g_forward_table_mutex);
                forward_table[i] = neighbor_ID;
                pthread_mutex_unlock(&g_forward_table_mutex);
                changed = true;
            }
            else if (costs->costs[i] == cost_through_neighbor){
                //if the cost of the two paths are same, choose the neighbor with lowest ID
                pthread_mutex_lock(&g_forward_table_mutex);
                if (forward_table[i] > neighbor_ID) {
                    forward_table[i] = neighbor_ID;
                    changed = true;
                }
                pthread_mutex_unlock(&g_forward_table_mutex);
            }
            // for (int i=0;i < (sizeof (costs->costs) /sizeof (costs->costs[0]));i++) {
            //     printf("%d\n",costs->costs[i]);
            // }
        
    }

    pthread_mutex_unlock(&g_costs_mutex);
    return changed;
}

////////////////////////////////////////////////////////////////////////////////
// Logging

/*
 * Log timestamped routing tables of this router.
 * Only log when changed.
 */
void log_routing_table(int *forward_table, costs_t *costs)
{
    char text[BUFSIZE];
    size_t pos = 0;
    for (int node_id = 0; node_id < NODE_COUNT; node_id++) {
        int fwd_node_id =  forward_table[node_id];

        // Spacing between entries
        if (node_id != 0) {
            strcpy(text + pos, "  ");
            pos += 1;
        }
        if (fwd_node_id >= NODE_COUNT) {
            // Not reachable, show --
            sprintf(text + pos, "%c=N/A", ConverToName[node_id]);
        } else {
            // Reachable, show fwd node and cost
            sprintf(text + pos, "%c=%d(via%c)",
                ConverToName[node_id], costs->costs[node_id], ConverToName[fwd_node_id]);
        }

        // Update pos
        pos += strlen(text + pos);
    }

    log(costs->src_node_id, "UPDATED ROUTES", text);
}

/*
 * Log information about any data packets routed through this router. Including:
 *   1. source node of this data packet (A,B,...)
 *   2. destination node
 *   3. Current UDP port
 *   4. The next UDP port that the current one is forwarding to
 */
void log_datagram_forward(datagram_t *dg, node_id_t thisnode, int thisport, int fwdport)
{
    char text[BUFSIZE];
    sprintf(text, "%d->%d via %d->%d",
        dg->src_node_id, dg->dst_node_id, thisport, fwdport);

    log(thisnode, "FORWARDED", text);
}

/*
 * Log when datagram reaches final destination.
 * See log_datagram_forward - ports + data.
 */
void log_datagram_received(datagram_t *dg, node_id_t thisnode)
{
    char text[BUFSIZE];
    sprintf(text, "%d->%d: (%zu bytes) \"%s\"",
        dg->src_node_id, dg->dst_node_id, dg->data_len, dg->data);

    log(thisnode, "RECEIVED", text);
}


void log(node_id_t node_id, const char *prefix, const char *text)
{
    // Open
    char filename[] = "routing-outputX.txt";
    filename[14] = ConverToName[node_id];
    FILE * log = fopen(filename, "a");
    if (!log) {
        perror("can't open log file");
        return;
    }

    // Log
    time_t t = time(0);
    char *timestr = ctime(&t);
    timestr[strlen(timestr) - 1] = 0;
    fprintf(log, "%s -- %s -- %s\n", prefix, timestr, text);
    printf("%s -- %s -- %s\n", prefix, timestr, text);

    // Close
    fclose(log);
}
