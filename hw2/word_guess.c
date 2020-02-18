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
#include "unp.h"
// #include "../unpv13e/lib/unp.h"

#define BUFSIZE 1024
#define MAXCLIENTS 5

// Linked List (for storing dictionaries)
typedef struct node *Tpointer;
typedef struct node {
    char* item;
    Tpointer next;
} Tnode;

Tnode* listPush(Tnode *list, char* data) {
    Tnode *newnode, *last = list;
    while (last->next != 0) {
    	last = last->next;
    }

    newnode = (Tnode*)malloc(sizeof(Tnode));
    newnode->item = malloc(strlen(data) + 1);
    strcpy(newnode->item, data);
    newnode->next = NULL;
    last->next = newnode;

    return last->next;
}

char* listGet(Tnode *list, int i) {
	Tnode *newnode = (Tnode*)list;
	while (i >= 0) {
		newnode = newnode->next;
		i--;
	}

	return newnode->item;
}

// Returns true if username exists in usernames array
int usernameTaken(char usernames[MAXCLIENTS][BUFSIZE], char username[]) {
	for (int i = 0; i < MAXCLIENTS; i++)
		if (usernames[i] != NULL)
			if (strcmp(usernames[i], username) == 0)
				return 1;
	return 0;
}

// Returns number of clients currently connected to server
int numUsers(char usernames[MAXCLIENTS][BUFSIZE]) {
	int number = 0;
	for (int i = 0; i < MAXCLIENTS; i++)
		if (usernames[i][0] != '\0')
			number++;
	return number;
}

// Returns number of correct letters
int numCorrect(char *secret_word, char *guess_word) {
	int alphabet1[26] = {0};
	int alphabet2[26] = {0};
	int count = 0;

	for (int i = 0; i < strlen(secret_word); i++)
        alphabet1[(int)tolower(secret_word[i]) - (int)'a']++;
    for (int i = 0; i < strlen(guess_word); i++)
        alphabet2[(int)tolower(guess_word[i]) - (int)'a']++;

    for (int i = 0; i < 26; i++)
    	count += min(alphabet1[i], alphabet2[i]);

    return count;
}

// Returns number of correctly placed letters
int numCorrectlyPlaced(char *secret_word, char *guess_word) {
	int i = 0;
	int count = 0;
	while (secret_word[i] != '\0') {
		if (tolower(secret_word[i]) == tolower(guess_word[i]))
			count++;
		i++;
	}
	return count;
}

// Returns true if secret and guess are the same, case insensitive
int isCorrect(char *secret_word, char *guess_word) {
	int i = 0;
	while (secret_word[i] != '\0') {
		if (tolower(secret_word[i]) != tolower(guess_word[i]))
			return 0;
		i++;
	}
	return 1;
}

// Main
int main(int argc, char *argv[]) {
	int sockfd;
	struct sockaddr_in servaddr, cliaddr;
	uint16_t port;
	socklen_t clilen;

	int seed;
	int longest_word_length;
	int numLines = 0;
	char filename[256];
	

	////////// Read in server settings //////////
	if (argc != 5) {
		fprintf(stderr, "Usage: %s [seed] [port] [dictionary_file] [longest_word_length].\n", argv[0]);
		exit(1);
	}

	sscanf(argv[1], "%d", &seed);
	sscanf(argv[2], "%hu", &port);
	sscanf(argv[3], "%s", filename);
	sscanf(argv[4], "%d", &longest_word_length);

	char *secret_word;
	int secret_word_len;



	////////// Prepare Dictionary //////////
	Tnode *dictionary = (Tnode*)malloc(sizeof(Tnode));
	dictionary->next = NULL;
	Tnode *last = dictionary;
	char word[longest_word_length];

	FILE* dictionary_file;
	dictionary_file = fopen(filename, "r");

	while(fgets(word, longest_word_length, dictionary_file) != NULL) {
		// Remove the trailing newline character
		if(strchr(word,'\n'))
			word[strlen(word)-1] = '\0';

		char *newWord = (char*)malloc(longest_word_length);
		strcpy(newWord, word);
		last = listPush(last, newWord);
		
		numLines++;
	}

	srand(seed);
	secret_word = listGet(dictionary, rand() % numLines);
	secret_word_len = strlen(secret_word);
	fprintf(stderr, "Loaded in %d words from dictionary.\n", numLines);

	
	////////// Initialize Client Sockets //////////
	int clientsock[MAXCLIENTS];
	fd_set readfds;
	for (int i = 0; i < MAXCLIENTS; i++)
		clientsock[i] = 0;

	fprintf(stderr, "Seed number is %d\n", seed);


	////////// Create socket, bind to it, and listen on it //////////
	sockfd = socket(AF_INET, SOCK_STREAM, 0);


	bzero((char *) &servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

	listen(sockfd, 5);

	fprintf(stderr, "TCP server: listening on port %d\n", port);


	int n;
	char buffer[BUFSIZE];
	char *username;
	char usernames[MAXCLIENTS][BUFSIZE];
	char *message;
	char *guess_word;
	int activity;
	int max_sd;
	int new_socket;
	int sd;
	int gameover = 0;
	while (1) {

		fprintf(stderr, "New game has started! Secret word is %s\n", secret_word);
		
		for (int i = 0; i < MAXCLIENTS; i++)
			usernames[i][0] = '\0';


		////////// Game Loop //////////
		while (gameover == 0) {
			fprintf(stderr, "Game loop\n");
			FD_ZERO(&readfds);
			FD_SET(sockfd, &readfds);
			max_sd = sockfd;
				 
			//add child sockets to set  
			for (int i = 0; i < MAXCLIENTS; i++) {
				sd = clientsock[i];

				if(sd > 0)
					FD_SET(sd, &readfds);

				if(sd > max_sd)
					max_sd = sd;
			}


			// Wait for activity on sockets using select
			activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
			fprintf(stderr, "Activity: %d\n", activity);
			if (activity > 0) {
				

				
				// Handle new incoming connections on master socket 
				if (FD_ISSET(sockfd, &readfds)) {
					clilen = sizeof((struct sockaddr *) &cliaddr);
					new_socket = accept(sockfd, (struct sockaddr *) &cliaddr, &clilen);

					if (new_socket < 0) {
						fprintf(stderr, "Accept error errno=%d\n", errno);
					}

					// Send Welcome Message
					fprintf(stderr, "New socket: %d\n", new_socket);
					message = "Welcome to Guess the Word, please enter your username.\n";
					write(new_socket, message, strlen(message));
						 
					// Add new client socket to socket array
					for (int i = 0; i < MAXCLIENTS; i++) {
						if (clientsock[i] == 0) {
							clientsock[i] = new_socket;
							fprintf(stderr, "New client connected on socket number %d\n", i);
							break;
						}
					}
				}
				


				// Handle activity for existing users
				for (int i = 0; i < MAXCLIENTS && gameover == 0; i++) {
					fprintf(stderr, "Checking activity for client number %d\n", i);
					sd = clientsock[i];

					if (FD_ISSET(sd, &readfds)) {
						fprintf(stderr, "fd_isset == 1\n");
						if ((n = read(sd, buffer, BUFSIZE)) != 0) {
							fprintf(stderr, "read length > 0\n");
							// Handle game
							buffer[n-1] = '\0';

							// Accept username, if one isn't set
							if (usernames[i][0] == '\0') {
								fprintf(stderr, "doing username stuff\n");
								// Check if username is already taken
								if (usernameTaken(usernames, buffer)) {
									fprintf(stderr, "username taken\n");
									message = malloc(BUFSIZE);
									sprintf(message, "Username %s is already taken, please enter a different username\n", buffer);
									n = write(sd, message, strlen(message));
								} else {
									fprintf(stderr, "setting username\n");
									strcpy(usernames[i], buffer);

									// Notify user of number of current users and length of secret word
									message = malloc(BUFSIZE);
									sprintf(message, "Let's start playing, %s\nThere are %d player(s) playing. The secret word is %d letter(s).\n",
										usernames[i], numUsers(usernames), secret_word_len);
									n = write(sd, message, strlen(message));
								}
							} else {
								fprintf(stderr, "handling guesses\n");
								// Handle guesses
								guess_word = (char *)malloc(strlen(buffer)+1);
								strcpy(guess_word, buffer);
								message = malloc(BUFSIZE);

								if (strlen(guess_word) == secret_word_len) {
									if (isCorrect(guess_word, secret_word) == 1) {
										// Guess is correct
										sprintf(message, "%s has correctly guessed the word %s\n", usernames[i], secret_word);
										gameover = 1;
									} else {
										// Guess is not correct
										sprintf(message, "%s guessed %s: %d letter(s) were correct and %d letter(s) were correctly placed.\n",
											usernames[i], guess_word, numCorrect(secret_word, guess_word), numCorrectlyPlaced(secret_word, guess_word));
									}

									// Send message to all connected players
									for (int cli = 0; cli < MAXCLIENTS; cli++)
										if (clientsock[i] != 0)
											n = write(clientsock[cli], message, strlen(message));

									fprintf(stderr, "%s\n", message);
								} else {
									// Players guess was not the correct length
									sprintf(message, "Invalid guess length. The secret word is %d letter(s).\n", secret_word_len);
									n = write(sd, message, strlen(message));
								}
							}
						} else {
							// Connection closed by client
							fprintf(stderr, "Client number %d disconnected!\n", i);
							close(sd);
							clientsock[i] = 0;
							usernames[i][0] = '\0';
						}
					}
				}
			}
		}

		fprintf(stderr, "Game over\n");

		// Reset game
		for (int i = 0; i < MAXCLIENTS; i++) {
			if (clientsock[i] > 0) {
				shutdown(clientsock[i], SHUT_RDWR);
				close(clientsock[i]);
			}
			clientsock[i] = 0;
		}

		secret_word = listGet(dictionary, rand() % numLines);
		secret_word_len = strlen(secret_word);

		while ((n = read(sd, buffer, BUFSIZE)) > 0)
			buffer[0] = '\0';

		buffer[0] = '\0';

		gameover = 0;
	}

	close(sockfd);
	return 0;
}