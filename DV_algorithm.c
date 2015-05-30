/* subscript 0,1,2,3,4,5 stands for A, B, C, D, E, F respectively*/
int  ID_to_Port[6];// matches the UDP port number to the node ID
char ID_to_Name[6]="ABCDEF";

int  my_cost[6]; //stands for the cost from current node to destination node
int  forward_table[6];// stands for the outgoing UDP node for the destination node denoted by the subscript

char neighbor_Name; //A or B or...
int  neighbor_ID; //0, 1, or ..
int  neighbor_cost[6];// stands for the cost from neighbor node to destination node


/* DV algorithm implementation */
for (int i =0; i<6; i++){
	int cost_through_neighbor;
	cost_through_neighbor = my_cost[neighbor_ID]+neighbor_cost[i];

	if (my_cost[i]> cost_through_neighbor){
		my_cost[i] = cost_through_neighbor;
		forward_table[i] = neighbor_ID;
	}
	else if (my_cost[i] == cost_through_neighbor){
		//if the cost of the two paths are same, choose the neighbor with lowest ID
		if (forward_table[i] > neighbor_ID)
			forward_table[i] = neighbor_ID;
	}
}

