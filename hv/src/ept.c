#include "ept.h"
#include "vmx.h"

static u8* g_ept_buffer;
static u64 g_ept_buffer_size;
static u64 g_ept_pml4_pa;

typedef struct _HV_PHYS_RANGE
{
    u64 base;
    u64 size;
} HV_PHYS_RANGE;

static HV_PHYS_RANGE g_ram_ranges[64];
static u32 g_ram_range_count;

static u64 hv_phys_address_bits(void)
{
    int regs[4] = { 0 };
    __cpuid(regs, 0x80000008);
    return (u64)regs[0] & 0xFF;
}

static NTSTATUS hv_capture_ram_ranges(void)
{
    PPHYSICAL_MEMORY_RANGE ranges = MmGetPhysicalMemoryRanges();
    if (!ranges)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    for (u32 i = 0; ranges[i].NumberOfBytes.QuadPart != 0 && i < 64; ++i)
    {
        g_ram_ranges[i].base = ranges[i].BaseAddress.QuadPart;
        g_ram_ranges[i].size = ranges[i].NumberOfBytes.QuadPart;
        ++g_ram_range_count;
    }

    ExFreePool(ranges);
    return g_ram_range_count ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

static BOOLEAN hv_is_ram(u64 pa, u64 len)
{
    u64 end = pa + len;
    for (u32 i = 0; i < g_ram_range_count; ++i)
    {
        u64 start = g_ram_ranges[i].base;
        u64 stop = start + g_ram_ranges[i].size;
        if (pa >= start && end <= stop)
        {
            return TRUE;
        }
    }
    return FALSE;
}

static u64 epte(u32 memtype)
{
    return EPTE_READ | EPTE_WRITE | EPTE_EXEC | EPTE_IGNORE_PAT | EPTE_MEMTYPE(memtype) | EPTE_LARGE;
}

u64 hv_ept_build(void)
{
    NTSTATUS status = hv_capture_ram_ranges();
    if (!NT_SUCCESS(status))
    {
        return 0;
    }

    u64 phys_bits = hv_phys_address_bits();
    u64 phys_space_end = 1ull << phys_bits;
    u64 gb_chunks = (phys_space_end + (1ull << 30) - 1) >> 30;

    g_ept_buffer_size = (1 + gb_chunks + gb_chunks) << 12;

    PHYSICAL_ADDRESS low = { 0 };
    PHYSICAL_ADDRESS high = { .QuadPart = 0xFFFFFFFF };
    g_ept_buffer = MmAllocateContiguousMemorySpecifyCache((SIZE_T)g_ept_buffer_size, low, high, low, MmCached);
    if (!g_ept_buffer)
    {
        return 0;
    }
    RtlZeroMemory(g_ept_buffer, (SIZE_T)g_ept_buffer_size);

    g_ept_pml4_pa = MmGetPhysicalAddress(g_ept_buffer).QuadPart;

    u64* pml4 = (u64*)g_ept_buffer;
    u8*  free_page = g_ept_buffer + 0x1000;

    for (u64 gb = 0; gb < gb_chunks; ++gb)
    {
        u64 chunk_base = gb << 30;
        u32 pml4_index = (u32)(chunk_base >> 39);
        u32 pdpt_index = (u32)((chunk_base >> 30) & 0x1FF);

        if (!pml4[pml4_index])
        {
            pml4[pml4_index] = MmGetPhysicalAddress(free_page).QuadPart | EPTE_READ | EPTE_WRITE | EPTE_EXEC;
            RtlZeroMemory(free_page, 0x1000);
            free_page += 0x1000;
        }

        u64* pdpt = (u64*)((u8*)pml4 + (pml4[pml4_index] & ~0xFFFull) - (g_ept_pml4_pa & ~0xFFFull));

        pdpt[pdpt_index] = MmGetPhysicalAddress(free_page).QuadPart | EPTE_READ | EPTE_WRITE | EPTE_EXEC;
        u64* pd = (u64*)free_page;
        RtlZeroMemory(free_page, 0x1000);
        free_page += 0x1000;

        for (u32 mb = 0; mb < 512; ++mb)
        {
            u64 mb_base = chunk_base + ((u64)mb << 21);
            pd[mb] = mb_base | epte(hv_is_ram(mb_base, 1ull << 21) ? VMX_MEMTYPE_WB : VMX_MEMTYPE_UC);
        }
    }

    return g_ept_pml4_pa | EPTP_FLAGS;
}

void hv_ept_destroy(void)
{
    if (g_ept_buffer)
    {
        MmFreeContiguousMemory(g_ept_buffer);
        g_ept_buffer = NULL;
    }
    g_ept_pml4_pa = 0;
}

u64 hv_ept_pointer(void)
{
    return g_ept_pml4_pa | EPTP_FLAGS;
}
