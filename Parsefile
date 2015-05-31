#include <string>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sstream>

/* Assumptions */
/*
This program, for simplicity, assumes a hard coded number of nodes, 6.

*/

#define NUMROUTE 6
using namespace std;

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

int main(int argc, char* argv[])
{
	if (argc < 3)
		error_message("need 2 arguments\n");

	// Take care of input arguments
	const char* filename = argv[1];
	int SID = atoi(argv[2]);
	if (SID >= NUMROUTE)
		error_message("Node number out of range"); 

	// Initialize all route to 9999, or infinity
	int RTable[NUMROUTE][NUMROUTE];
	for (int i = 0; i < NUMROUTE; i++)
		for (int j = 0; j < NUMROUTE; j++)
		{
			RTable[i][j] = 9999;
		}
	
	FILE* file = fopen(filename, "r");
	char line[256];
	
	// This number by the end of the loop will tell us which port we can use for 
	// this node and the others
	int portnum[NUMROUTE];
	for (int i = 0; i < NUMROUTE; i++)
		portnum[i] = -1;

	while (fgets(line, sizeof(line), file)) 
	{
		// Now we're parsing the line
		bool Selfsource = false;
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
				if (Subscript == SID)
					Selfsource = true;
			}
			else if (index == 1)
			{
				// This is the destination node in 2nd column
				Destination = ConvertToSubscript(token.c_str());
				if (Destination == -1)
					error_message("Invalid file content\n");
			}
			else if (index == 2)
			{
				// This is the third column, port number for destination
				if (Destination != -1)
					portnum[Destination] = atoi(token.c_str());
			}
			else if (index == 3)
			{
				// This is the fourth column, link cost
				if (Selfsource)
				{
					if (Destination == -1)
						error_message("Index out of range for destination");
					RTable[SID][Destination] = atoi(token.c_str());
				}	
			}
			else
				break;
			index++;
		}
	}
		
	// This is for debugging only
	// Print out the routing table, 999 = infinity
	for (int i = 0; i < NUMROUTE; i++)
	{
		for (int j = 0; j < NUMROUTE; j++)
		{
			printf("%d | ", RTable[i][j]);
		}
		printf("\n");
	}
	printf("\n\n");

	// Print out all the port numbers of nodes A,B,C,D,E,F respectively as deduced from file	
	for (int i = 0; i < NUMROUTE; i++)
	{
		printf("Port %d Distance: %d\n", i, portnum[i]);
	}
	
	fclose(file);
	return 0;
}
