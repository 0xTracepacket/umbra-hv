#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <intrin.h>

typedef unsigned __int32 u32;
typedef unsigned __int64 u64;

static int g_failures;
static int g_checks;

static void report(const char* name, int pass, const char* detail)
{
    ++g_checks;
    printf("[ %s ] %-34s %s\n", pass ? "PASS" : "FAIL", name, detail ? detail : "");
    if (!pass)
    {
        ++g_failures;
    }
}

static void check_hypervisor_bit(void)
{
    int regs[4] = { 0 };
    __cpuid(regs, 1);
    int present = (regs[2] >> 31) & 1;
    char buf[64];
    sprintf_s(buf, sizeof(buf), "leaf1 ECX[31]=%d", present);
    report("cpuid hypervisor bit", present == 0, buf);
}

static void check_hypervisor_leaf(void)
{
    int regs[4] = { 0 };
    __cpuid(regs, 0);
    u32 max_std = (u32)regs[0];
    if (max_std < 0x40000000)
    {
        __cpuid(regs, 0x40000000);
        u32 zero = !regs[0] && !regs[1] && !regs[2] && !regs[3];
        report("no hv signature", zero, "leaf 0x40000000 returns data");
        return;
    }
    report("no hv signature", 1, "host already reports an hv leaf");
}

static void check_invalid_leaf(void)
{
    int a[4] = { 0 };
    int b[4] = { 0 };
    __cpuid(a, 0x40000001);
    __cpuid(b, 0x4FFFFFFF);
    int same = !memcmp(a, b, sizeof(a));
    report("invalid leaf consistency", same, "0x40000001 vs 0x4FFFFFFF");
}

static u64 timing_cost(void (*fn)(void))
{
    u64 best = ~0ull;
    for (int i = 0; i < 200; ++i)
    {
        u64 t0 = __rdtsc();
        fn();
        u64 t1 = __rdtsc();
        if (t1 - t0 < best)
        {
            best = t1 - t0;
        }
    }
    return best;
}

static void nop_cpuid(void)
{
    int regs[4];
    __cpuid(regs, 1);
}

static void nop_nothing(void)
{
    __rdtsc();
}

static void check_timing_cost(void)
{
    u64 overhead = timing_cost(nop_nothing);
    u64 cpuid_cost = timing_cost(nop_cpuid);
    u64 net = cpuid_cost > overhead ? cpuid_cost - overhead : 0;

    char buf[64];
    sprintf_s(buf, sizeof(buf), "cpuid net=%llu ticks (overhead=%llu)", net, overhead);
    report("timing cost (<400 ticks)", net < 400, buf);
}

static int g_illegal_instruction;

static LONG WINAPI veh_handler(struct _EXCEPTION_POINTERS* ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION)
    {
        g_illegal_instruction = 1;
        ep->ContextRecord->Rip += 3;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void check_vmcall_ud(void)
{
    unsigned char stub[] = { 0x0F, 0x01, 0xC1, 0xC3 };
    void* page = VirtualAlloc(NULL, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!page)
    {
        report("vmcall #UD", 0, "alloc failed");
        return;
    }
    memcpy(page, stub, sizeof(stub));
    void (*fn)(void) = (void (*)(void))page;

    g_illegal_instruction = 0;
    PVOID handler = AddVectoredExceptionHandler(1, veh_handler);
    fn();
    RemoveVectoredExceptionHandler(handler);
    VirtualFree(page, 0, MEM_RELEASE);

    report("vmcall #UD (ring3)", g_illegal_instruction, "bare-metal fault semantics");
}

static void check_tsc_monotonic(void)
{
    int bad = 0;
    u64 prev = __rdtsc();
    for (int i = 0; i < 10000; ++i)
    {
        int regs[4];
        __cpuid(regs, 1);
        u64 t = __rdtsc();
        if (t <= prev)
        {
            ++bad;
        }
        prev = t;
        u64 t2 = __rdtsc();
        if (t2 <= t)
        {
            ++bad;
        }
        prev = t2;
    }
    char buf[64];
    sprintf_s(buf, sizeof(buf), "backwards reads=%d", bad);
    report("tsc monotonic", bad == 0, buf);
}

int main(void)
{
    printf("hvprobe - hypervisor self-check\n\n");

    check_hypervisor_bit();
    check_hypervisor_leaf();
    check_invalid_leaf();
    check_timing_cost();
    check_tsc_monotonic();
    check_vmcall_ud();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
