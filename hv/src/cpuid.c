#include "vcpu.h"

#define HV_CPUID_CACHE_MAX 48

typedef struct _HV_CPUID_ENTRY
{
    u32 leaf;
    u32 subleaf;
    int regs[4];
    u8 valid;
} HV_CPUID_ENTRY;

static HV_CPUID_ENTRY g_cpuid_cache[HV_CPUID_CACHE_MAX];
static u32 g_cpuid_cache_count;

static void cache_put(u32 leaf, u32 subleaf)
{
    if (g_cpuid_cache_count >= HV_CPUID_CACHE_MAX)
    {
        return;
    }

    HV_CPUID_ENTRY* e = &g_cpuid_cache[g_cpuid_cache_count++];
    e->leaf = leaf;
    e->subleaf = subleaf;
    e->valid = 1;
    __cpuidex(e->regs, (int)leaf, (int)subleaf);
}

void hv_cpuid_cache_capture(void)
{
    int regs[4] = { 0 };
    __cpuid(regs, 0);
    u32 max_std = (u32)regs[0];
    __cpuid(regs, 0x80000000);
    u32 max_ext = (u32)regs[0];

    cache_put(0, 0);
    cache_put(1, 0);
    if (max_std >= 7)
    {
        cache_put(7, 0);
    }
    cache_put(0x40000000, 0);
    cache_put(0x40000001, 0);
    cache_put(0x4FFFFFFF, 0);

    if (max_std < 0x40000000)
    {
        cache_put(max_std + 1, 0);
    }

    cache_put(0x7FFFFFFF, 0);

    for (u32 leaf = 0x80000000; leaf <= max_ext && leaf <= 0x80000008; ++leaf)
    {
        cache_put(leaf, 0);
    }
}

static const HV_CPUID_ENTRY* cache_find(u32 leaf, u32 subleaf)
{
    for (u32 i = 0; i < g_cpuid_cache_count; ++i)
    {
        if (g_cpuid_cache[i].leaf == leaf && g_cpuid_cache[i].subleaf == subleaf)
        {
            return &g_cpuid_cache[i];
        }
    }
    return NULL;
}

void hv_cpuid_handle(struct hv_vcpu* vcpu, struct hv_regs* regs)
{
    UNREFERENCED_PARAMETER(vcpu);

    u64 leaf = regs->rax;
    u64 subleaf = regs->rcx;

    const HV_CPUID_ENTRY* e = cache_find((u32)leaf, (u32)subleaf);
    int out[4] = { 0 };

    if (e)
    {
        out[0] = e->regs[0];
        out[1] = e->regs[1];
        out[2] = e->regs[2];
        out[3] = e->regs[3];
    }
    else
    {
        __cpuidex(out, (int)leaf, (int)subleaf);
    }

    regs->rax = (u64)(u32)out[0];
    regs->rbx = (u64)(u32)out[1];
    regs->rcx = (u64)(u32)out[2];
    regs->rdx = (u64)(u32)out[3];
}
