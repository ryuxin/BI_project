#include "mem_mgr.h"

int local_node_id;
int num_node_in_use;
int num_core_in_use;
__thread int local_core_id;

uint64_t
bi_global_rtdsc()
{
	if (NODE_ID() == 0) {
		global_layout->time.tsc = bi_local_rdtsc();
		clwb_range(global_layout->time, CACHE_LINE);
	} else {
		clflush_range(global_layout->time, CACHE_LINE);
	}
	return global_layout->time.tsc;
}
