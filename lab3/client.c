#include	"../unpv13e/lib/unp.h"

int main(int argc, char **argv)
{
	int					sockfd[5];
	struct sockaddr_in	servaddr;


	for (int i = 0; i < 5; i++)   
    {   
        sockfd[i] = 0;   
    } 



	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(9877);
	inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);



	int	n;
	char	sendline[MAXLINE], recvline[MAXLINE + 1];
	socklen_t servlen = sizeof(servaddr);

	sendto(sockfd, "1\r\n", 3, 0, &servaddr, servlen);

	for ( ; ; ) {

		n = recvfrom(sockfd, recvline, MAXLINE, 0, NULL, NULL);

		recvline[n] = 0;	/* null terminate */
		fputs(recvline, stdout);
	}



	return 0;
}
