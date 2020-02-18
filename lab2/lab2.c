#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "../unpv13e/lib/unp.h"

void handler(int sig) {
	pid_t pid;
	int stat;

	while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
		printf("Parent sees child PID %d has terminated.\r\n", pid);
	
	return;
}

int main() {
	int * n;
	printf("Number of children to spawn: ");
	scanf("%d", n);

	getchar();

	signal(SIGCHLD, handler);

	pid_t pid;
	for (int i = 0; i < *n; i++) {
		
		pid = fork();
		if (pid != 0) {
			printf("Parent spawned child PID %d\r\n", pid);
		} else {
			pid = getpid();

			srand(pid);
			int t = rand() % 6;
			printf("Child PID %d dying in %d seconds.\r\n", pid, t);

			sleep(t);

			printf("Child PID %d terminating.\r\n", pid);

			return 0;
		}
	}

	getchar();

	return 0;
}