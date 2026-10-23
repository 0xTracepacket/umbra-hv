#include "svm.h"

#define NPT_PRESENT   (1ull << 0)
#define NPT_WRITE     (1ull << 1)
#define NPT_USER      (1ull << 2)
#define NPT_LARGE     (1ull << 7)

static u8* g_npt_buffer;
static u64 g_npt_buffer_size;
static u64 g_npt_pml4_pa;

static u64 hv_phys_address_bits(void)
{
    int regs[4] = { 0 };
    __cpuid(regs, 0x80000008);
    return (u64)regs[0] & 0xFF;
}

static u64 npte_intermediate(u64 pa)
{
    return pa | NPT_PRESENT | NPT_WRITE | NPT_USER;
}

static u64 npte_large(u64 pa)
{
    return pa | NPT_PRESENT | NPT_WRITE | NPT_USER | NPT_LARGE;
}

u64 hv_npt_build(void)
{
    u64 phys_bits = hv_phys_address_bits();
    u64 phys_space_end = 1ull << phys_bits;
    u64 gb_chunks = (phys_space_end + (1ull << 30) - 1) >> 30;

    g_npt_buffer_size = (1 + gb_chunks + gb_chunks) << 12;

    PHYSICAL_ADDRESS low = { 0 };
    PHYSICAL_ADDRESS high = { .QuadPart = 0xFFFFFFFF };
    g_npt_buffer = MmAllocateContiguousMemorySpecifyCache((SIZE_T)g_npt_buffer_size, low, high, low, MmCached);
    if (!g_npt_buffer)
    {
        return 0;
    }
    RtlZeroMemory(g_npt_buffer, (SIZE_T)g_npt_buffer_size);

    g_npt_pml4_pa = MmGetPhysicalAddress(g_npt_buffer).QuadPart;

    u64* pml4 = (u64*)g_npt_buffer;
    u8*  free_page = g_npt_buffer + 0x1000;

    for (u64 gb = 0; gb < gb_chunks; ++gb)
    {
        u64 chunk_base = gb << 30;
        u32 pml4_index = (u32)(chunk_base >> 39);
        u32 pdpt_index = (u32)((chunk_base >> 30) & 0x1FF);

        if (!pml4[pml4_index])
        {
            pml4[pml4_index] = npte_intermediate(MmGetPhysicalAddress(free_page).QuadPart);
            RtlZeroMemory(free_page, 0x1000);
            free_page += 0x1000;
        }

        u64* pdpt = (u64*)((u8*)pml4 + (pml4[pml4_index] & 0x000FFFFFFFFFF000ull) - (g_npt_pml4_pa & 0x000FFFFFFFFFF000ull));

        pdpt[pdpt_index] = npte_intermediate(MmGetPhysicalAddress(free_page).QuadPart);
        u64* pd = (u64*)free_page;
        RtlZeroMemory(free_page, 0x1000);
        free_page += 0x1000;

        for (u32 mb = 0; mb < 512; ++mb)
        {
            u64 mb_base = chunk_base + ((u64)mb << 21);
            pd[mb] = npte_large(mb_base);
        }
    }

    return g_npt_pml4_pa;
}

void hv_npt_destroy(void)
{
    if (g_npt_buffer)
    {
        MmFreeContiguousMemory(g_npt_buffer);
        g_npt_buffer = NULL;
    }
    g_npt_pml4_pa = 0;
}
