#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>

#define MAXLENGTH 1024

// Send UPDATEPOSITION message
char* sendUpdatePosition(int sockfd, char id[MAXLENGTH], int range, float x, float y) {
	char message[MAXLENGTH]; 
	sprintf(message, "UPDATEPOSITION %s %d %f %f", id, range, x, y);
	write(sockfd, message, strlen(message) * sizeof(char));
	read(sockfd, message, sizeof(message));
	//fprintf(stderr, "%s\n", message);

	return strdup(message);
}

// Send WHERE message
char* sendWhere(int sockfd, char id[MAXLENGTH]) {
	char message[MAXLENGTH];
	sprintf(message, "WHERE %s", id);
	write(sockfd, message, strlen(message) * sizeof(char));
	read(sockfd, message, sizeof(message));
	//fprintf(stderr, "%s\n", message);
	
	return strdup(message);
}

// Send DATAMESSAGE message
void sendData(int sockfd, char src[MAXLENGTH], char dest[MAXLENGTH], char id[MAXLENGTH], int range, float x, float y, int hoplen, char *hoplist, int first) {
	// Get destination location from server
	char delim[] = " ";
	char *ptr = sendWhere(sockfd, dest);
	ptr = strtok(ptr, delim);
	ptr = strtok(NULL, delim);

	ptr = strtok(NULL, delim);
	float destX = (float) strtof(ptr, (char **)NULL);

	ptr = strtok(NULL, delim);
	float destY = (float) strtof(ptr, (char **)NULL);

	// Get list of reachable nodes
	char *reachable = sendUpdatePosition(sockfd, id, range, x, y);

	ptr = strtok(reachable, delim);
	ptr = strtok(NULL, delim);
	int numReachable = (int) strtol(ptr, (char **)NULL, 10);

	// Check if there are no reachable nodes
	if (numReachable == 0) {
		printf("%s: Message from %s to %s could not be delivered.\n", id, src, dest);
		return;
	}

	if (first == 1)
		printf("%s: Sent a new message bound for %s.\n", id, dest);
	else
		printf("%s: Message from %s to %s being forwarded through %s\n", id, src, dest, id);

	char message[MAXLENGTH];
	
	char *closest = NULL;
	float closestDist = 4294967295;

	// CASE 1: Check if destination node is reachable
	while ((ptr = strtok(NULL, delim)) != NULL) {
		if (strncmp(dest, ptr, strlen(ptr)) == 0) {
			// Destination IS reachable, sending message directly
			sprintf(message, "DATAMESSAGE %s %s %s %d %s %s", src, dest, dest, hoplen+1, hoplist, dest);
			write(sockfd, message, strlen(message) * sizeof(char));
			return;
		}

		// Calculate distance and update closest node
		char *nodeName = ptr;

		ptr = strtok(NULL, delim);
		float nodeX = (float) strtof(ptr, (char **)NULL);

		ptr = strtok(NULL, delim);
		float nodeY = (float) strtof(ptr, (char **)NULL);

		float distance = (float) sqrt(powf(destX - nodeX, 2) + powf(destY - nodeY, 2));

		if (distance < closestDist) {
			if (strstr(hoplist, nodeName) == NULL) {
				closest = malloc(sizeof(char) * (strlen(nodeName) + 1));
				strcpy(closest, nodeName);
				closestDist = distance;
			}
		}
	}

	// CASE 2: Send to closest node
	if (closest != NULL) {
		sprintf(message, "DATAMESSAGE %s %s %s %d %s %s", src, closest, dest, hoplen+1, hoplist, closest);
		write(sockfd, message, strlen(message) * sizeof(char));
	}
}

// Main
int main(int argc, char *argv[]) {

	// Validate
	if (argc != 7) {
		fprintf(stderr, "Usage: %s [control address] [control port] [SensorID] [SensorRange] [InitialXPosition] [InitialYPosition]\n", argv[0]);
		exit(1);
	}

	// Collect data
	char hostname[MAXLENGTH];
	struct hostent *host;
	char *address;
	int port;
	char id[MAXLENGTH];
	int range;
	float x, y;

	sscanf(argv[1], "%s", hostname);
	sscanf(argv[2], "%d", &port);
	sscanf(argv[3], "%s", id);
	sscanf(argv[4], "%d", &range);
	sscanf(argv[5], "%f", &x);
	sscanf(argv[6], "%f", &y);

	// Setup connection
	int sockfd, connfd; 
	struct sockaddr_in servaddr, cli;

	host = gethostbyname(hostname);
	address = inet_ntoa(*((struct in_addr*) host->h_addr_list[0])); 
  
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	bzero(&servaddr, sizeof(servaddr));
  
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = inet_addr(address); 
	servaddr.sin_port = htons(port); 

  
	// connect the client socket to server socket
	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0) { 
		char buff[MAXLENGTH]; 
		int n;

		// Initial UPDATEPOSITION
		sendUpdatePosition(sockfd, id, range, x, y);


		// Setup select to listen for commands from both stdin and the server
		fd_set master;
		fd_set read_fds;

		FD_ZERO(&master);
		FD_ZERO(&read_fds);

		FD_SET(0, &master);
		FD_SET(sockfd, &master);

		while (1) {
			read_fds = master;
			if (select(sockfd + 1, &read_fds, NULL, NULL, NULL) == -1) {
				return 1;
			}

			bzero(buff, MAXLENGTH);

			// Command received from stdin
			if (FD_ISSET(0, &read_fds)) {
				bzero(buff, sizeof(buff)); 
				n = 0;
				fgets(buff, sizeof(buff), stdin);

				char delim[] = " ";
				char *ptr = strtok(buff, delim);
				if (strchr(ptr, '\n'))
					ptr[strlen(ptr) - 1] = '\0';

				if (strcmp(ptr, "MOVE") == 0) {
					ptr = strtok(NULL, delim);
					x = (float) strtof(ptr, (char **)NULL);

					ptr = strtok(NULL, delim);
					y = (float) strtof(ptr, (char **)NULL);

					sendUpdatePosition(sockfd, id, range, x, y);
				}

				if (strcmp(ptr, "SENDDATA") == 0) {
					ptr = strtok(NULL, delim);
					char *dest = malloc(strlen(ptr) * sizeof(char));
					if (strchr(ptr, '\n'))
						ptr[strlen(ptr) - 1] = '\0';
					strcpy(dest, ptr);

					sendData(sockfd, id, dest, id, range, x, y, 1, id, 1);
				}

				if (strcmp(ptr, "WHERE") == 0) {
					ptr = strtok(NULL, delim);
					char *id = malloc(strlen(ptr) * sizeof(char));
					strcpy(id, ptr);
					sendWhere(sockfd, id);
				}

				if (strcmp(ptr, "QUIT") == 0) {
					shutdown(sockfd, SHUT_RDWR);
					close(sockfd);
					return 0;
				}
			}

			// Command received from the server
			if (FD_ISSET(sockfd, &read_fds)) {
				if ((n = read(sockfd, buff, sizeof(buff))) != 0) {
					char delim[] = " ";
					char *ptr = strtok(buff, delim);
					if (strchr(ptr, '\n'))
						ptr[strlen(ptr) - 1] = '\0';

					if (strcmp(ptr, "DATAMESSAGE") == 0) {
						ptr = strtok(NULL, delim);
						char *src = malloc(strlen(ptr) * sizeof(char));
						strcpy(src, ptr);

						ptr = strtok(NULL, delim);
						char *nextid = malloc(strlen(ptr) * sizeof(char));
						strcpy(nextid, ptr);

						ptr = strtok(NULL, delim);
						char *dest = malloc(strlen(ptr) * sizeof(char));
						strcpy(dest, ptr);

						ptr = strtok(NULL, delim);
						int hoplen = (int) strtol(ptr, (char **)NULL, 10);

						ptr = strtok(NULL, "");

						// Check if message has reached destination
						if (strncmp(id, dest, strlen(id)) == 0) {
							printf("%s: Message from %s to %s successfully received.\n", id, src, id);
							continue;
						}

						sendData(sockfd, src, dest, nextid, range, x, y, hoplen, ptr, 0);
					}
				} else {
					fprintf(stderr, "Disconnected from server!\n");
					return 1;
				}	
			}
		}
	} else {
		fprintf(stderr, "Error connecting\n");
	}

	return 0;
}