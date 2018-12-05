#include <assert.h>
#include <string.h>
#include "mem_mgr.h"

struct Mem_layout *global_layout;

void
init_magic_str(char *s)
{
	assert(strlen(s) < MAGIC_SZ);
	strcpy(global_layout->magic, s);
}
