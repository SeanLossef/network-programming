#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <pthread.h>

#define NUM_CHILD 5

struct adders {
    long a;
    long b;
};

long addrec(struct adders *ab) {
	if (ab->b > 0) {
		struct adders ab2;
		ab2.a = ab->a;
		ab2.b = ab->b - 1;
		return (1 + (long)addrec((void*)&ab2));
	} else {
		return (ab->a);
	}
}

void *add(void *adder) {
	struct adders *ab = (struct adders*)adder;
	printf("Thread %d running add() with [%ld + %ld]\n", (int)pthread_self(), (long)ab->a, (long)ab->b);

	return (void*)addrec(adder);
}

int main() {
	pthread_t children[NUM_CHILD * (NUM_CHILD-1)];

	for (long a = 1; a < NUM_CHILD; a++) {
		for (long b = 1; b <= NUM_CHILD; b++) {

			printf("Main starting thread add() for [%ld + %ld]\n", a, b);

			pthread_t tid;
			struct adders *ab = (struct adders*)malloc(sizeof(struct adders));;
			ab->a = a;
			ab->b = b;
			int val = pthread_create(&tid, NULL, add, (void*)ab);

			if (val < 0) {
				return -1;
			} else {
				children[((a-1)*NUM_CHILD)+b-1] = tid;
			}
		}
	}

	for (long a = 1; a < NUM_CHILD; a++) {
		for (long b = 1; b <= NUM_CHILD; b++) {
			int *ret_val;
			pthread_join(children[((a-1)*NUM_CHILD)+b-1], (void**)&ret_val);
			printf("In main, collecting thread %d computed [%ld + %ld] = %d\n", (int)(children[((a-1)*NUM_CHILD)+b-1]), a, b, (int)ret_val);
		}
	}

	return 0;
}