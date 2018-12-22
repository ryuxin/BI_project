#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include "mem_mgr.h"

#define TRACE_NAME_LEN 50

int local_node_id;
int num_node_in_use;
int num_core_in_use;
__thread int local_core_id;

static void
trace_gen(int fd, long nops, unsigned int percent_update)
{
	unsigned int i;
	srand(time(NULL));
	for (i = 0 ; i < nops ; i++) {
		char value;
		if ((unsigned int)rand() % 100 < percent_update) value = 'U';
		else                               value = 'R';
		if (write(fd, &value, 1) < 1) {
			perror("Writing to trace file");
			exit(-1);
		}
	}
	lseek(fd, 0, SEEK_SET);
}

void
load_trace(long nops, unsigned int percent_update, char *ops)
{
        int fd, ret;
        int bytes;
        long i, n_read, n_update;
	char trace_name[TRACE_NAME_LEN];

	sprintf(trace_name, "/tmp/%u_update.dat", percent_update);
        ret = mlock(ops, nops);
        if (ret) {
                printf("Cannot lock memory (%d). Check privilege (i.e. use sudo). Exit.\n", ret);
                exit(-1);
        }

        printf("loading trace file @ %s.\n", trace_name);
        /* read the entire trace into memory. */
        fd = open(trace_name, O_RDONLY);
        if (fd < 0) {
                fd = open(trace_name, O_CREAT | O_RDWR, S_IRWXU);
                assert(fd >= 0);
                trace_gen(fd, nops, percent_update);
        }

        bytes = read(fd, &ops[0], nops);
        assert(bytes == nops);
        n_read = n_update = 0;

        for (i = 0 ; i < nops ; i++) {
                if      (ops[i] == 'R') { ops[i] = 0; n_read++; }
                else if (ops[i] == 'U') { ops[i] = 1; n_update++; }
                else assert(0);
        }
        printf("Trace: read %ld, update %ld, total %ld\n", n_read, n_update, (n_read+n_update));
        assert(n_read+n_update == nops);

        close(fd);
        return;
}

uint64_t
bi_global_rtdsc()
{
	if (NODE_ID() == 0) {
		global_layout->time.tsc = bi_local_rdtsc();
		clwb_range(&global_layout->time, CACHE_LINE);
	} else {
		clflush_range(&global_layout->time, CACHE_LINE);
	}
	return global_layout->time.tsc;
}
