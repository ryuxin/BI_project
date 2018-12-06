#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#define _GNU_SOURCE
#include <sched.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "rpc.h"
#include "mem_mgr.h"

#define SIZE (1024*1024*1024)

int main(void)
{
	char *file = "/lfs/cache_test";
	int fd = open(file, O_CREAT | O_RDWR, 0666);
	void *mem;

	ftruncate(fd, SIZE);
	mem = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mem == MAP_FAILED) {
		perror(mmap failed");
		exit(-1);
	}
	init_global_memory(mem, "rpc simple tests");
	return 0;
}
