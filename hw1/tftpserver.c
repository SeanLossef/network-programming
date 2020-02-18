#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include "../unpv13e/lib/unp.h"

#define MAX_DGRAM_SIZE 516
#define RECV_TIMEOUT 10
#define RECV_RETRIES 5

/* Opcodes */
enum opcode {
	RRQ   = 1,
	WRQ   = 2,
	DATA  = 3,
	ACK   = 4,
	ERROR = 5
};

/* Message Structure */
typedef union {
	uint16_t opcode;

	struct {
		uint16_t opcode;        
		uint8_t filename[514];
	} request;     

	struct {
		uint16_t opcode;
		uint16_t block_number;
		uint8_t data[512];
	} data;

	struct {
		uint16_t opcode;         
		uint16_t block_number;
	} ack;

	struct {
		uint16_t opcode; 
		uint16_t error_code;
		uint8_t error_string[512];
	} error;

} message;

// Signal handler
void handler(int sig) {
     int status;
     wait(&status);
}

// Send Ack packet
void send_ack(int sockfd, uint16_t block_number, struct sockaddr_in *cliaddr, socklen_t *len) {
	message message;

	message.opcode = htons(ACK);
	message.ack.block_number = htons(block_number);

	sendto(sockfd, &message, sizeof(message.ack), 0, (SA *) cliaddr, *len);
}

// Send Data packet
void send_data(int sockfd, uint16_t block_number, struct sockaddr_in *cliaddr, socklen_t *len, uint8_t *data, ssize_t dlen) {
	message message;

	message.opcode = htons(DATA);
	message.data.block_number = htons(block_number);
	memcpy(message.data.data, data, dlen);

	sendto(sockfd, &message, dlen + 4, 0, (SA *) cliaddr, *len);
}

// Send Error packet
void send_error(int sockfd, uint16_t error_code, char *error_string, struct sockaddr_in *cliaddr, socklen_t *len) {
	message message;

	message.opcode = htons(ERROR);
	message.error.error_code = htons(error_code);
	strcpy((char*)(message.error.error_string), error_string);

	sendto(sockfd, &message, 4 + strlen(error_string) + 1, 0, (SA *) cliaddr, *len);
	fprintf(stderr, "ERROR: %s\n", error_string);
}

// Handle read and write requests
int handle_request(message *message, socklen_t len, struct sockaddr_in *cliaddr, uint16_t port) {
	int	n;

	int sockfd;
	struct timeval tv;
	struct sockaddr_in	servaddr;

	// Open new socket on new port
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	tv.tv_sec  = RECV_TIMEOUT;
	tv.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(port);

	bind(sockfd, (SA *) &servaddr, sizeof(servaddr));

	fprintf(stderr, "Handling new request on port %d\n", ntohs(servaddr.sin_port));


	if (ntohs(message->opcode) == RRQ) {
		if (fork() == 0) {
			fprintf(stderr, "Read request received for %s\n", (char*)(message->request.filename));

			FILE *file = fopen((char*)(message->request.filename), "r");

			// File does not exist error
			if (file == NULL) {
				send_error(sockfd, 0, "File does not exist", cliaddr, &len);
				exit(1);
			}

			uint8_t data[MAX_DGRAM_SIZE - 4];
			size_t dlen;
			uint16_t block_number = 1;

			int active = 1;
			int attempt;

			// While connection is not terminated
			while (active) {
				dlen = fread(data, 1, MAX_DGRAM_SIZE - 4, file);

				// Last block to send
				if (dlen < MAX_DGRAM_SIZE - 4)
					active = 0;

				for (attempt = RECV_RETRIES; attempt; attempt--) {
					send_data(sockfd, block_number, cliaddr, &len, data, dlen);

					fprintf(stderr, "\tSent block %d (%d bytes)\n", block_number, (uint)dlen);

					n = recvfrom(sockfd, message, MAX_DGRAM_SIZE, 0, (SA *) cliaddr, &len);

					if (n >= 4)
						break;

					if (n >= 0 && n < 4) {
						send_error(sockfd, 4, "Request sent with invalid length", cliaddr, &len);
						exit(1);
					}
				}

				// Transfer timed out
				if (!attempt) {
					fprintf(stderr, "ERROR: Transfer timed out\n");
					exit(1);
				}

				// Error message received from client
				if (ntohs(message->opcode) == ERROR)  {
					fprintf(stderr, "Client sent error: %s\n", message->error.error_string);
					exit(1);
				}

				// Received invalid message, expected ack
				if (ntohs(message->opcode) != ACK)  {
					send_error(sockfd, 4, "Invalid message sent during transfer", cliaddr, &len);
					exit(1);
				}

				// Wrong block number ack received
				if (ntohs(message->ack.block_number) != block_number) {
					send_error(sockfd, 0, "Invalid block number receieved", cliaddr, &len);
					exit(1);
				}

				fprintf(stderr, "\tReceived ACK : %d\n", ntohs(message->ack.block_number));

				block_number++;
			}

			fprintf(stderr, "\tDone!\n");

			// Close file
			fclose(file);

			return 0;
		}
	} else if (ntohs(message->opcode) == WRQ) {
		if (fork() == 0) {
			fprintf(stderr, "Write request received for %s\n", (char*)(message->request.filename));

			FILE *file = fopen((char*)(message->request.filename), "wb"); 

			if (file == NULL) {
				send_error(sockfd, 4, "Error creating file", cliaddr, &len);
				exit(1);
			}

			uint16_t block_number = 0;

			send_ack(sockfd, block_number, cliaddr, &len);
			fprintf(stderr, "\tSent ACK : %d\n", block_number);

			int active = 1;
			int attempt;

			// While connection is not terminated
			while (active) {
				for (attempt = RECV_RETRIES; attempt; attempt--) {
					n = recvfrom(sockfd, message, MAX_DGRAM_SIZE, 0, (SA *) cliaddr, &len);

					fprintf(stderr, "\tReceived block %d (%d bytes)\n", ntohs(message->data.block_number), n - 4);

					if (n >= 4)
						break;

					if (n >= 0 && n < 4) {
						send_error(sockfd, 4, "Request sent with invalid length", cliaddr, &len);
						exit(1);
					}
				}

				// Transfer timed out
				if (!attempt) {
					fprintf(stderr, "ERROR: Transfer timed out\n");
					exit(1);
				}

				// Terminate connection
				if (n < MAX_DGRAM_SIZE)
					active = 0;

				block_number++;

				// Error message received from client
				if (ntohs(message->opcode) == ERROR)  {
					fprintf(stderr, "Client sent error: %s\n", message->error.error_string);
					exit(1);
				}

				// Received invalid message, expected data
				if (ntohs(message->opcode) != DATA)  {
					send_error(sockfd, 4, "Invalid message sent during transfer", cliaddr, &len);
					exit(1);
				}

				// Wrong block received
				if (ntohs(message->data.block_number) != block_number) {
					send_error(sockfd, 0, "Invalid block number receieved", cliaddr, &len);
					exit(1);
				}

				fwrite(message->data.data, 1, n - 4, file);

				send_ack(sockfd, block_number, cliaddr, &len);
				fprintf(stderr, "\tSent ACK : %d\n", block_number);
			}

			fprintf(stderr, "\tDone!\n");

			// Close file
			fclose(file);

			return 0;
		}
	}
	return 0;
}

// Main
int main(int argc, char *argv[]) {

	int					sockfd;
	struct sockaddr_in	servaddr, cliaddr;
	uint16_t port = 0;
	uint16_t child_port = 0;

	// Check syntax
	if (argc != 3) {
		fprintf(stderr, "Usage: %s [start of port range] [end of port range].\n", argv[0]);
		exit(1);
	}

	// Determine port
	if (sscanf(argv[1], "%hu", &port)) {
		port = htons(port);
		child_port = htons(port);
	} else {
		fprintf(stderr, "error: invalid port number\n");
		exit(1);
	}

	// Create and bind socket
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = port;

	bind(sockfd, (SA *) &servaddr, sizeof(servaddr));

	signal(SIGCHLD, (void *) handler) ;

	fprintf(stderr, "TFTP server: listening on port %d\n", ntohs(servaddr.sin_port));


	// Server loop
	int			n;
	socklen_t	len;
	message message;

	while (1) {
		len = sizeof(cliaddr);
		n = recvfrom(sockfd, &message, MAX_DGRAM_SIZE, 0, (SA *) &cliaddr, &len);

		// Bad message receieved
		if (n < 0) {
			fprintf(stderr, "Error receieving packets\n");
			continue;
		}

		// Invalid message length error
		if (n < 4) {
			send_error(sockfd, 4, "Request sent with invalid length", &cliaddr, &len);
			exit(1);
		}

		uint16_t opcode = ntohs(message.opcode);

		if (opcode == RRQ || opcode == WRQ) {
			child_port++;
			handle_request(&message, len, &cliaddr, child_port);
		}
	}

	return 0;
}