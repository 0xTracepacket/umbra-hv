#pragma once

#include "hv.h"

#define EPTE_READ       (1ull << 0)
#define EPTE_WRITE      (1ull << 1)
#define EPTE_EXEC       (1ull << 2)
#define EPTE_MEMTYPE(m) ((u64)(m) << 3)
#define EPTE_IGNORE_PAT (1ull << 6)
#define EPTE_LARGE      (1ull << 7)

#define EPT_WALK_4_LEVEL 3
#define EPTP_FLAGS (VMX_MEMTYPE_WB | (EPT_WALK_4_LEVEL << 3))

u64  hv_ept_build(void);
void hv_ept_destroy(void);
u64  hv_ept_pointer(void);
