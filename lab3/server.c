#include	"../unpv13e/lib/unp.h"

int main(int argc, char **argv)
{
	int					sockfd;
	struct sockaddr_in	servaddr, cliaddr;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(argv[1]);

	bind(sockfd, (SA *) &servaddr, sizeof(servaddr));

	int			n;
	socklen_t	len;
	char		mesg[MAXLINE];

	n = recvfrom(sockfd, mesg, MAXLINE, 0, &cliaddr, &len);



	char	sendline[MAXLINE];

	while (fgets(sendline, MAXLINE, stdin) != NULL) {

		sendto(sockfd, sendline, strlen(sendline), 0, &cliaddr, len);

		if (feof(sendline)) {
		    close(sockfd);
		    return 0;
		}

	}






	int count = 5;

	SOCKET max_sd = 0;
    SOCKET socketId[10] = {0};
    SOCKET sd = 0 ;

    fd_set readfds;
    int i ,j , ret;
    char recvBuf[1024] = "";
    char errMsg[256] = "" ;
    struct sockaddr_in server ;

    // Socket Initialization
    for(i = 0; i<count ; i++)
    {
        ret = initSocket(&socketId[i]);//Small function to create socket
        if (ret != 1)
        {
            return ret ;
        }
        //Server info
        server.sin_addr.s_addr  = inet_addr(ipAddr[i]);
        server.sin_family       = AF_INET;
        server.sin_port         = htons(port);
        // Conect to server
        if (connect(socketId[i], (struct sockaddr *)&server , sizeof(server)) < 0)
        {
            printf("connect ::Failed to connect to server %s:%d",ipAddr[i],port);
            return -1;
        }       
    }


    while(TRUE)
    {
        // init fd_set
        FD_ZERO(&readfds); // added by siva to initialize socket descriptors
        for(i = 0; i<count ; i++)
        {
            //FD_ZERO(&readfds); // Commented by siva to avoid initialization for each socket
            FD_SET(socketId[i], &readfds); 
            max_sd = (max_sd>socketId[i])?max_sd:socketId[i];
        }
        ret = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (ret < 0)
        {
            printf("select failed\n ");
            return -1;
        }
        // warning: you don't know the max_sd value
        for(i = 0; i<count ; i++)
        {
            sd = socketId[i] ;
            if (FD_ISSET(sd, &readfds)) 
            {
                ret = recv(sd,(char *)recvBuf,sizeof(recvBuf), 0);
                if(ret > 0 )
                {
                    printf("Message received from socket %d : %s\n",sd,recvBuf);
                    send(sd,(char *)recvBuf,strlen(recvBuf),0);
                }
            }
        }
    }
}
