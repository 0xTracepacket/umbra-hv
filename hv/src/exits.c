#include "vcpu.h"
#include "vmx.h"

static u32 g_unexpected_exit_logged;

static void vmwrite64(u64 field, u64 value)
{
    __vmx_vmwrite(field, value);
}

static u64 vmread64(u64 field)
{
    u64 value = 0;
    __vmx_vmread(field, &value);
    return value;
}

static void hv_advance_rip(void)
{
    u64 rip = vmread64(VMCS_GUEST_RIP);
    u64 len = vmread64(VMCS_EXIT_INSTRUCTION_LEN);
    vmwrite64(VMCS_GUEST_RIP, rip + len);
}

static void hv_inject_ud(void)
{
    vmwrite64(VMCS_CTRL_ENTRY_INT_INFO, 0x80000000ull | (3ull << 8) | UD_VECTOR);
}

static void hv_handle_cr_access(struct hv_vcpu* vcpu, struct hv_regs* regs)
{
    u64 qual = vmread64(VMCS_EXIT_QUALIFICATION);
    u32 cr = (u32)(qual & 0xF);
    u32 access = (u32)((qual >> 4) & 0x3);
    u32 gpr = (u32)((qual >> 8) & 0xF);

    u64* regp = NULL;
    switch (gpr)
    {
    case 0: regp = &regs->rax; break;
    case 1: regp = &regs->rcx; break;
    case 2: regp = &regs->rdx; break;
    case 3: regp = &regs->rbx; break;
    case 5: regp = &regs->rbp; break;
    case 6: regp = &regs->rsi; break;
    case 7: regp = &regs->rdi; break;
    default: break;
    }

    if (cr == 4 && regp)
    {
        if (access == 0)
        {
            vcpu->cr4_shadow = *regp & ~CR4_VMXE_BIT;
            vmwrite64(VMCS_CTRL_CR4_SHADOW, vcpu->cr4_shadow);
        }
        else if (access == 1)
        {
            *regp = vcpu->cr4_shadow;
        }
    }

    hv_advance_rip();
}

static void hv_handle_vmcall(struct hv_vcpu* vcpu, struct hv_regs* regs)
{
    if (regs->r10 == HV_SIG0 && regs->r11 == HV_SIG1 && regs->r12 == HV_SIG2
        && regs->rax == HV_HYPERCALL_TEARDOWN)
    {
        hv_advance_rip();
        vcpu->teardown = 1;
        regs->rax = 0;
        return;
    }

    hv_inject_ud();
}

static BOOLEAN hv_dispatch(struct hv_vcpu* vcpu, struct hv_regs* regs, u64 reason)
{
    switch (reason)
    {
    case EXIT_CPUID:
        hv_cpuid_handle(vcpu, regs);
        hv_advance_rip();
        hv_timing_on_cpuid_exit(vcpu);
        return TRUE;

    case EXIT_RDTSC:
        if (vcpu->tsc_trap_armed)
        {
            hv_timing_on_rdtsc_exit(vcpu, regs);
        }
        else
        {
            u64 t = __rdtsc();
            regs->rax = t & 0xFFFFFFFFull;
            regs->rdx = (t >> 32) & 0xFFFFFFFFull;
            vcpu->last_guest_tsc = t;
        }
        hv_advance_rip();
        return TRUE;

    case EXIT_CR_ACCESS:
        hv_handle_cr_access(vcpu, regs);
        return TRUE;

    case EXIT_VMCALL:
        hv_handle_vmcall(vcpu, regs);
        return vcpu->teardown ? FALSE : TRUE;

    case EXIT_VMCLEAR:
    case EXIT_VMLAUNCH:
    case EXIT_VMPTRLD:
    case EXIT_VMPTRST:
    case EXIT_VMREAD:
    case EXIT_VMRESUME:
    case EXIT_VMWRITE:
    case EXIT_VMXOFF:
    case EXIT_VMXON:
        hv_inject_ud();
        return TRUE;

    case EXIT_XSETBV:
    {
        u64 value = regs->rax | (regs->rdx << 32);
        _xsetbv((unsigned int)regs->rcx, value);
        hv_advance_rip();
        return TRUE;
    }

    default:
        if (reason != EXIT_EXCEPTION_NMI && reason != EXIT_EXTERNAL_INTERRUPT
            && g_unexpected_exit_logged < 16)
        {
            ++g_unexpected_exit_logged;
            HV_LOG("unexpected exit reason=%llu", reason);
        }
        return TRUE;
    }
}

u8 hv_exit_handler(struct hv_regs* regs)
{
    struct hv_vcpu* vcpu = hv_vcpu_current();
    if (!vcpu)
    {
        __debugbreak();
    }

    ++vcpu->exit_count;

    u64 reason = vmread64(VMCS_EXIT_REASON) & 0xFFFF;

    if (vcpu->tsc_trap_armed && reason != EXIT_RDTSC && reason != EXIT_CPUID)
    {
        hv_timing_disarm(vcpu);
    }

    vmwrite64(VMCS_CTRL_ENTRY_INT_INFO, 0);

    BOOLEAN keep_running = hv_dispatch(vcpu, regs, reason);

    return (u8)!!keep_running;
}
