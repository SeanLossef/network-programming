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
#include <pthread.h>
#include <math.h>
#include <errno.h>

#define MAXLENGTH 1024
#define MAXCLIENTS 10

////// Node Struct and Helpers //////
struct Node {
	char *id;
	float xPos;
	float yPos;
	int range;
	int numLinks;
	int isSensor;
	int connfd;
	struct Node **links;
};

struct Msnr {
	struct Node *nodes;
	int numNodes;
	int connfd;
};

// Returns pointer to Node given id
struct Node* getNode(struct Msnr *msnr, char *id) {
	if (strchr(id, '\n'))
		id[strlen(id) - 1] = '\0';
	for (int i = 0; i < msnr->numNodes; i++) {
		if (strcmp(msnr->nodes[i].id, id) == 0) {
			return &msnr->nodes[i];
		}
	}
	return NULL;
}

// Updates position of node
void updatePosition(struct Msnr *msnr, char *id, int range, float x, float y, int connfd) {
	struct Node *node = getNode(msnr, id);
	if (node == NULL) {
		node = &(msnr->nodes[msnr->numNodes]);
		if (strchr(id, '\n'))
			id[strlen(id) - 1] = '\0';
		node->id = malloc(strlen(id) * sizeof(char));
		strcpy(node->id, id);
		msnr->numNodes++;
	}
	node->isSensor = 1;
	node->connfd = connfd;
	node->range = range;
	node->xPos = x;
	node->yPos = y;
	return;
}

float getDistance(struct Node *node1, struct Node *node2) {
	float dx = (node1->xPos - node2->xPos);
	float dy = (node1->yPos - node2->yPos);
	return (float)sqrt(dx*dx + dy*dy);
}

// Check if 2 nodes are reachable
int isReachable(struct Node *node1, struct Node *node2) {
	// Both nodes are base stations
	if (node1->isSensor == 0 && node2->isSensor == 0) {
		for (int i = 0; i < node1->numLinks; i++) {
			if (node1->links[i] == node2)
				return 1;
		}
		return 0;
	}

	float distance = getDistance(node1, node2);

	// Both nodes are sensors
	if (node1->isSensor == 1 && node2->isSensor == 1) {
		if (distance <= node1->range && distance <= node2->range)
			return 1;
		return 0;
	}

	// Only node1 is a sensor
	if (node1->isSensor == 1) {
		if (distance <= node1->range)
			return 1;
		return 0;
	}

	// Only node2 is a sensor
	if (node2->isSensor == 1) {
		if (distance <= node2->range)
			return 1;
		return 0;
	}

	return 0;
}

struct Node** getReachable(struct Msnr *msnr, char *id) {
	struct Node *node = getNode(msnr, id);
	struct Node **reachable = malloc(msnr->numNodes * sizeof(struct Msnr*));

	// Discover which nodes are reachable
	for (int i = 0; i < msnr->numNodes; i++) {
		reachable[i] = NULL;
		if (&msnr->nodes[i] != node)
			if (isReachable(&msnr->nodes[i], node))
				reachable[i] = &msnr->nodes[i];
	}

	return reachable;
}

// Sends reachable data packet
void sendReachable(struct Msnr *msnr, char *id, int connfd) {
	struct Node *node = getNode(msnr, id);
	struct Node *reachable[msnr->numNodes];
	int numReachable = 0;

	// Discover which nodes are reachable
	for (int i = 0; i < msnr->numNodes; i++) {
		reachable[i] = NULL;
		if (&msnr->nodes[i] != node)
			if (isReachable(&msnr->nodes[i], node)) {
				reachable[i] = &msnr->nodes[i];
				numReachable++;
			}
	}

	// Create and send message
	char *message = malloc(MAXLENGTH * sizeof(char));
	sprintf(message, "REACHABLE %d", numReachable);

	for (int i = 0; i < msnr->numNodes; i++) {
		if (reachable[i] != NULL) {
			sprintf(message, "%s %s %.1f %.1f", message, reachable[i]->id, reachable[i]->xPos, reachable[i]->yPos);
		}
	}

	write(connfd, message, sizeof(char) * strlen(message));
}

// Sends THERE packet
void sendThere(struct Msnr *msnr, char *id, int connfd) {
	struct Node *node = getNode(msnr, id);
	char *message = malloc(MAXLENGTH * sizeof(char));
	sprintf(message, "THERE %s %.1f %.1f", id, node->xPos, node->yPos);
	write(connfd, message, sizeof(char) * strlen(message));
	return;
}

// Upon receiving a DATAMESSAGE, performs appropriate actions
void handleDataMessage(struct Msnr *msnr, char *src, char *nextid, char *dest, int hoplen, char *hoplist) {
	struct Node *node = getNode(msnr, nextid);
	char message[MAXLENGTH];

	if (node != NULL) {
		if (node->isSensor == 1) {
			// Next node is sensor, relaying message to next node
			sprintf(message, "DATAMESSAGE %s %s %s %d %s", src, nextid, dest, hoplen, hoplist);
			write(node->connfd, message, sizeof(char) * strlen(message));
			return;
		}

		// Next node is a base station
		if (strcmp(nextid, dest) == 0) {
			// Next node is destination, message has arrived successfully
			printf("%s: Message from %s to %s successfully received.\n", dest, src, dest);
			return;
		}
		printf("%s: Message from %s to %s being forwarded through %s\n", nextid, src, dest, nextid);

		// Check if all reachable nodes are in hop list
		struct Node **reachable = getReachable(msnr, nextid);
		int flag = 1;
		for (int i = 0; i < msnr->numNodes; i++) {
			if (reachable[i] != NULL) {
				if (strstr(hoplist, reachable[i]->id) == NULL)
					flag = 0;
			}
		}
		if (flag) {
			printf("%s: Message from %s to %s could not be delivered.\n", nextid, src, dest);
			return;
		}

		// Check if destination is reachable
		for (int i = 0; i < msnr->numNodes; i++) {
			if (reachable[i] != NULL) {
				if (strncmp(reachable[i]->id, dest, strlen(dest)) == 0) {
					if (reachable[i]->isSensor == 1) {
						sprintf(message, "DATAMESSAGE %s %s %s %d %s %s", src, reachable[i]->id, dest, hoplen+1, hoplist, reachable[i]->id);
						write(reachable[i]->connfd, message, sizeof(char) * strlen(message));
						return;
					} else {
						char *newhoplist = malloc(MAXLENGTH * sizeof(char));
						sprintf(newhoplist, "%s %s", hoplist, reachable[i]->id);
						handleDataMessage(msnr, src, reachable[i]->id, dest, hoplen+1, newhoplist);
						return;
					}
				}
			}
		}

		// Send to closest node not in hoplist
		struct Node *destNode = getNode(msnr, dest);
		struct Node *closest = NULL;
		float closestDist = 4294967295;
		for (int i = 0; i < msnr->numNodes; i++) {
			if (reachable[i] != NULL) {
				if (strstr(hoplist, reachable[i]->id) == NULL) {
					float distance = getDistance(destNode, reachable[i]);
					if (distance < closestDist) {
						closestDist = distance;
						closest = reachable[i];
					}
				}
			}
		}
		if (closest == NULL) {
			printf("%s: Message from %s to %s could not be delivered.\n", nextid, src, dest);
			return;
		}
		if (closest->isSensor == 1) {
			sprintf(message, "DATAMESSAGE %s %s %s %d %s %s", src, closest->id, dest, hoplen+1, hoplist, closest->id);
			write(closest->connfd, message, sizeof(char) * strlen(message));
			return;
		} else {
			char *newhoplist = malloc(MAXLENGTH * sizeof(char));
			sprintf(newhoplist, "%s %s", hoplist, closest->id);
			handleDataMessage(msnr, src, closest->id, dest, hoplen+1, newhoplist);
			return;
		}
	}
}

// Starts thread for each connected client to handle I/O
void* clientHandler(void *arg) {
	struct Msnr *msnr = *((struct Msnr**)arg);
	char buffer[MAXLENGTH];
	int connfd = msnr->connfd;
	char delim[] = " ";
	int n;

	while (1) {
		bzero(buffer, MAXLENGTH);

		if ((n = read(connfd, buffer, sizeof(buffer))) > 0) {
			char *ptr = strtok(buffer, delim);
			if (strchr(ptr, '\n'))
				ptr[strlen(ptr) - 1] = '\0';

			// UPDATEPOSITION Command
			if (strcmp(ptr, "UPDATEPOSITION") == 0) {
				ptr = strtok(NULL, delim);
				char *id = malloc(strlen(ptr) * sizeof(char));
				strcpy(id, ptr);

				ptr = strtok(NULL, delim);
				int range = (int) strtol(ptr, (char **)NULL, 10);

				ptr = strtok(NULL, delim);
				float x = (float) strtof(ptr, (char **)NULL);

				ptr = strtok(NULL, delim);
				float y = (float) strtof(ptr, (char **)NULL);

				updatePosition(msnr, id, range, x, y, connfd);
				sendReachable(msnr, id, connfd);
				continue;
			}

			// WHERE Command
			if (strcmp(ptr, "WHERE") == 0) {
				ptr = strtok(NULL, delim);
				char *id = malloc(strlen(ptr) * sizeof(char));
				strcpy(id, ptr);

				sendThere(msnr, id, connfd);
				continue;
			}

			// DATAMESSAGE Command
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

				handleDataMessage(msnr, src, nextid, dest, hoplen, ptr);
				
				continue;
			}
		} else {
			fprintf(stderr, "Sensor disconnected!\n");
			close(connfd);
			pthread_exit(NULL);
			return NULL;
		}
	}
}

// Main
int main(int argc, char *argv[]) {

	// Validate
	if (argc != 3) {
		fprintf(stderr, "Usage: %s [control port] [base station file]\n", argv[0]);
		exit(1);
	}

	////// Create MSNR->Object //////
	struct Msnr *msnr = malloc(sizeof(struct Msnr));
	msnr->nodes = calloc(128, sizeof(struct Node));
	msnr->numNodes = 0;



	////// Read base station file //////
	FILE* bs_file;
	char filename[MAXLENGTH];
	char line[MAXLENGTH];
	sscanf(argv[2], "%s", filename);
	bs_file = fopen(filename, "r");
	char delim[] = " ";

	// Create node objects with most data
	while(fgets(line, MAXLENGTH, bs_file) != NULL) {
		char *ptr = strtok(line, delim);
		msnr->nodes[msnr->numNodes].id = malloc(strlen(ptr) * sizeof(char));
		strcpy(msnr->nodes[msnr->numNodes].id, ptr);

		ptr = strtok(NULL, delim);
		msnr->nodes[msnr->numNodes].xPos = (int) strtol(ptr, (char **)NULL, 10);

		ptr = strtok(NULL, delim);
		msnr->nodes[msnr->numNodes].yPos = (float) strtof(ptr, (char **)NULL);

		ptr = strtok(NULL, delim);
		msnr->nodes[msnr->numNodes].numLinks = (float) strtof(ptr, (char **)NULL);

		msnr->nodes[msnr->numNodes].isSensor = 0;

		msnr->numNodes++;
	}

	// Loop data again to connect links
	fseek(bs_file, 0, SEEK_SET);
	int i = 0;
	while (fgets(line, MAXLENGTH, bs_file) != NULL) {
		char *ptr = strtok(line, delim);
		char *id = malloc(strlen(ptr) * sizeof(char));
		strcpy(id, ptr);
		ptr = strtok(NULL, delim);
		ptr = strtok(NULL, delim);
		ptr = strtok(NULL, delim);
		ptr = strtok(NULL, delim);

		msnr->nodes[i].links = malloc(msnr->nodes[i].numLinks * sizeof(struct Node*));

		int j = 0;
		while (ptr != NULL) {
			getNode(msnr, id)->links[j] = getNode(msnr, ptr);
			ptr = strtok(NULL, delim);
			j++;
		}
		i++;
	}

	////// Open control server socket //////
	int sockfd;
	struct sockaddr_in servaddr, cliaddr;
	uint16_t port;
	sscanf(argv[1], "%hu", &port);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

	bzero((char *) &servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	listen(sockfd, 5);
	fprintf(stderr, "TCP server: listening on port %d\n", port);

	// Setup select to listen for commands from both stdin and the server
	char buff[MAXLENGTH]; 
	int n;

	fd_set master;
	fd_set read_fds;

	FD_ZERO(&master);
	FD_ZERO(&read_fds);

	FD_SET(0, &master);
	FD_SET(sockfd, &master);

	pthread_t tids[MAXCLIENTS];
	int clientsock[MAXCLIENTS];
	for (int i = 0; i < MAXCLIENTS; i++) {
		tids[i] = 0;
		clientsock[i] = 0;
	}

	//////// Accept incoming connections and create new thread ///////
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

			if (strcmp(ptr, "QUIT") == 0) {
				for (int i = 0; i < MAXCLIENTS; i++) {
					if (tids[i] != 0) {
						pthread_cancel(tids[i]);
						shutdown(clientsock[i], SHUT_RDWR);
						close(clientsock[i]);
					}
				}
				close(sockfd);
				return 0;
			}
		}

		// Command received from socket
		if (FD_ISSET(sockfd, &read_fds)) {
			pthread_t tid;

			socklen_t len = sizeof(cliaddr);
			int connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &len);

			if (connfd < 0) {
				fprintf(stderr, "Accept error errno=%d\n", errno);
				continue;
			}

			msnr->connfd = connfd;

			pthread_create(&tid, NULL, clientHandler, &msnr);

			for (int i = 0; i < MAXCLIENTS; i++) {
				if (tids[i] == 0) {
					tids[i] = tid;
					clientsock[i] = connfd;
					break;
				}
			}
		}

	}
	


	close(sockfd);

	return 0;
}