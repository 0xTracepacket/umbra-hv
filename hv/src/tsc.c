#include "vcpu.h"
#include "vmx.h"

u64 g_hv_cpuid_ticks;

static u64 hv_measure_cpuid_ticks(void)
{
    u64 best = ~0ull;
    for (u32 i = 0; i < 256; ++i)
    {
        u64 t0 = __rdtsc();
        int regs[4];
        __cpuidex(regs, 1, 0);
        u64 t1 = __rdtsc();
        u64 delta = t1 - t0;
        if (delta && delta < best)
        {
            best = delta;
        }
    }
    return best == ~0ull ? 0 : best;
}

void hv_tsc_calibrate(void)
{
    g_hv_cpuid_ticks = hv_measure_cpuid_ticks();
    HV_LOG("native cpuid cost: %llu ticks", g_hv_cpuid_ticks);
}

static void hv_arm_rdtsc(void)
{
    u64 proc1 = 0;
    __vmx_vmread(VMCS_CTRL_PROC1, &proc1);
    __vmx_vmwrite(VMCS_CTRL_PROC1, proc1 | PROC1_RDTSC_EXITING);
}

void hv_tsc_disarm(struct hv_vcpu* vcpu)
{
    if (!vcpu->tsc_trap_armed)
    {
        return;
    }

    vcpu->tsc_trap_armed = 0;

    u64 proc1 = 0;
    __vmx_vmread(VMCS_CTRL_PROC1, &proc1);
    __vmx_vmwrite(VMCS_CTRL_PROC1, proc1 & ~PROC1_RDTSC_EXITING);
}

void hv_tsc_arm(struct hv_vcpu* vcpu)
{
    u64 exit_tsc = __rdtsc();
    u64 comp = exit_tsc + g_hv_cpuid_ticks;

    if (comp <= vcpu->last_guest_tsc)
    {
        comp = vcpu->last_guest_tsc + 1;
    }

    vcpu->tsc_comp_value = comp;
    vcpu->tsc_trap_armed = 1;
    hv_arm_rdtsc();
}

void hv_tsc_compensate(struct hv_vcpu* vcpu, struct hv_regs* regs)
{
    hv_tsc_disarm(vcpu);

    u64 t = vcpu->tsc_comp_value;
    vcpu->last_guest_tsc = t;

    regs->rax = t & 0xFFFFFFFFull;
    regs->rdx = (t >> 32) & 0xFFFFFFFFull;
}
