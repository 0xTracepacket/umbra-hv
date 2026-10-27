#include "vcpu.h"
#include "svm.h"

extern void hv_asm_svm_launch(struct hv_vcpu* vcpu);
extern void hv_asm_svm_teardown(void);
extern void hv_asm_svm_guest_entry(void);
extern void hv_asm_sgdt(u64* buffer);
extern void hv_asm_sidt(u64* buffer);
extern void hv_asm_get_segments(u16* selectors);
extern u64  hv_asm_get_dr7(void);

VOID KeGenericCallDpc(_In_ PKDEFERRED_ROUTINE Routine, _In_opt_ PVOID Context);
VOID KeSignalCallDpcDone(_In_ PKDPC Dpc);
VOID KeSignalCallDpcSynchronize(_In_ PVOID SynchronizeContext);

static u64 g_npt_pml4_pa;
static u8  g_svm_active;

BOOLEAN hv_svm_supported(void)
{
    int regs[4] = { 0 };

    __cpuid(regs, 0x80000000);
    if ((u32)regs[0] < 0x8000000A)
    {
        return FALSE;
    }

    __cpuid(regs, 0x80000001);
    if (!(regs[2] & (1 << 2)))
    {
        hv_status_set(0x201);
        return FALSE;
    }

    __cpuid(regs, 0x8000000A);
    if (!(regs[3] & (1 << 0)))
    {
        hv_status_set(0x202);
        return FALSE;
    }

    u64 vm_cr = __readmsr(SVM_VM_CR_MSR);
    if (vm_cr & SVM_VM_CR_SVMDIS)
    {
        hv_status_set(0x203);
        return FALSE;
    }

    return TRUE;
}

static void hv_svm_write_segment(u8* vmcb, u64 field_off, u16 selector, u32 ar, u32 limit, u64 base)
{
    u64 off = 0x400 + field_off;
    *(u16*)(vmcb + off + 0) = selector;
    *(u16*)(vmcb + off + 2) = (u16)ar;
    *(u32*)(vmcb + off + 4) = limit;
    *(u64*)(vmcb + off + 8) = base;
}

static void hv_svm_fill_segment(u8* vmcb, u64 field_off, u16 selector, u64 override_base)
{
    u64 base = 0;
    u32 limit = 0;
    u32 ar = 0;
    u64 gdtr[8] = { 0 };
    hv_asm_sgdt(gdtr);

    u64 offset = selector & ~7ull;
    if ((selector & ~7ull) && offset + 16 > gdtr[0])
    {
        u8* desc = (u8*)(gdtr[1] + offset);
        limit = desc[0] | (desc[1] << 8) | ((desc[6] & 0xF) << 16);
        ar = desc[5];
        base = desc[2] | (desc[3] << 8) | (desc[4] << 16) | ((u64)desc[7] << 24);

        u32 type = ar & 0xF;
        if (type == 2 || type == 3 || type == 9 || type == 11)
        {
            base |= (u64)desc[8] << 32 | (u64)desc[9] << 40 | (u64)desc[10] << 48;
        }

        u32 ar_val = (ar & 0xF)
            | (((ar >> 4) & 1) << 4)
            | (((ar >> 5) & 3) << 5)
            | (((ar >> 7) & 1) << 7)
            | (((desc[6] >> 5) & 1) << 13)
            | (((desc[6] >> 6) & 1) << 14)
            | (((desc[6] >> 7) & 1) << 15);
        ar = ar_val;
    }
    else
    {
        ar = 0;
    }

    if (override_base != (u64)-1)
    {
        base = override_base;
    }

    hv_svm_write_segment(vmcb, field_off, selector, ar, limit, base);
}

static void hv_svm_setup_vmcb(struct hv_vcpu* vcpu)
{
    u8* vmcb = vcpu->svm_vmcb_va;

    RtlZeroMemory(vmcb, 0x1000);

    *(u32*)(vmcb + SVM_VMCB_MISC1_OFF) = SVM_INTERCEPT_MISC1_CPUID | SVM_INTERCEPT_MISC1_MSR_PROT;
    *(u32*)(vmcb + SVM_VMCB_MISC2_OFF) = SVM_INTERCEPT_MISC2_VMRUN;
    *(u64*)(vmcb + SVM_VMCB_MSRPM_PA_OFF) = vcpu->svm_msrpm_pa;
    *(u32*)(vmcb + SVM_VMCB_ASID_OFF) = vcpu->svm_asid;
    *(u64*)(vmcb + SVM_VMCB_NP_ENABLE_OFF) = SVM_NP_ENABLE;
    *(u64*)(vmcb + SVM_VMCB_NCR3_OFF) = g_npt_pml4_pa;

    u8* msrpm = vcpu->svm_msrpm_va;
    RtlZeroMemory(msrpm, SVM_MSRPM_SIZE);
    u64 efer_bit = (0x800ull * 8) + (SVM_EFER_MSR - 0xC0000000ull) * 2;
    msrpm[efer_bit / 8] |= (1 << (efer_bit % 8));
    msrpm[efer_bit / 8] |= (1 << ((efer_bit + 1) % 8));
    if ((efer_bit % 8) == 7)
    {
        msrpm[efer_bit / 8 + 1] |= 1;
    }

    u16 sels[8] = { 0 };
    hv_asm_get_segments(sels);

    u64 fs_base = __readmsr(0xC0000100);
    u64 gs_base = __readmsr(0xC0000101);

    hv_svm_fill_segment(vmcb, SVM_VMCB_ES_SEL_OFF - 0x400, sels[0], (u64)-1);
    hv_svm_fill_segment(vmcb, SVM_VMCB_CS_SEL_OFF - 0x400, sels[1], (u64)-1);
    hv_svm_fill_segment(vmcb, SVM_VMCB_SS_SEL_OFF - 0x400, sels[2], (u64)-1);
    hv_svm_fill_segment(vmcb, SVM_VMCB_DS_SEL_OFF - 0x400, sels[3], (u64)-1);
    hv_svm_fill_segment(vmcb, SVM_VMCB_FS_SEL_OFF - 0x400, sels[4], fs_base);
    hv_svm_fill_segment(vmcb, SVM_VMCB_GS_SEL_OFF - 0x400, sels[5], gs_base);
    hv_svm_fill_segment(vmcb, SVM_VMCB_LDTR_SEL_OFF - 0x400, 0, (u64)-1);
    hv_svm_fill_segment(vmcb, SVM_VMCB_TR_SEL_OFF - 0x400, sels[7], (u64)-1);

    u64 gdtr[8] = { 0 };
    u64 idtr[8] = { 0 };
    hv_asm_sgdt(gdtr);
    hv_asm_sidt(idtr);
    hv_svm_write_segment(vmcb, SVM_VMCB_GDTR_OFF - 0x400, 0, 0, (u32)gdtr[0], gdtr[1]);
    hv_svm_write_segment(vmcb, SVM_VMCB_IDTR_OFF - 0x400, 0, 0, (u32)idtr[0], idtr[1]);

    *(u8*)(vmcb + SVM_VMCB_CPL_OFF) = 0;
    vcpu->svm_guest_efer = __readmsr(SVM_EFER_MSR);
    *(u64*)(vmcb + SVM_VMCB_EFER_OFF) = vcpu->svm_guest_efer;
    *(u64*)(vmcb + SVM_VMCB_CR0_OFF) = __readcr0();
    *(u64*)(vmcb + SVM_VMCB_CR2_OFF) = __readcr2();
    *(u64*)(vmcb + SVM_VMCB_CR3_OFF) = __readcr3();
    *(u64*)(vmcb + SVM_VMCB_CR4_OFF) = __readcr4();
    *(u64*)(vmcb + SVM_VMCB_DR6_OFF) = 0;
    *(u64*)(vmcb + SVM_VMCB_DR7_OFF) = hv_asm_get_dr7();
    *(u64*)(vmcb + SVM_VMCB_STAR_OFF) = __readmsr(0xC0000081);
    *(u64*)(vmcb + SVM_VMCB_LSTAR_OFF) = __readmsr(0xC0000082);
    *(u64*)(vmcb + SVM_VMCB_CSTAR_OFF) = __readmsr(0xC0000083);
    *(u64*)(vmcb + SVM_VMCB_SFMASK_OFF) = __readmsr(0xC0000084);
    *(u64*)(vmcb + SVM_VMCB_KGSBASE_OFF) = __readmsr(0xC0000102);
    *(u64*)(vmcb + SVM_VMCB_SYSENTER_CS_OFF) = __readmsr(0x174);
    *(u64*)(vmcb + SVM_VMCB_SYSENTER_ESP_OFF) = __readmsr(0x175);
    *(u64*)(vmcb + SVM_VMCB_SYSENTER_EIP_OFF) = __readmsr(0x176);
    *(u64*)(vmcb + SVM_VMCB_GPAT_OFF) = __readmsr(0x277);
    *(u64*)(vmcb + SVM_VMCB_DBGCTL_OFF) = 0;
    *(u64*)(vmcb + SVM_VMCB_RAX_OFF) = 0;
}

static void hv_svm_setup_host_vmcb(struct hv_vcpu* vcpu)
{
    u8* host_vmcb = vcpu->svm_host_vmcb_va;
    RtlZeroMemory(host_vmcb, 0x1000);
    *(u64*)(host_vmcb + SVM_VMCB_EFER_OFF) = __readmsr(SVM_EFER_MSR);
    __svm_vmsave(vcpu->svm_host_vmcb_pa);
}

#ifndef HV_SVM_STAGE
#define HV_SVM_STAGE 1
#endif

NTSTATUS hv_svm_start_vcpu(void)
{
    struct hv_vcpu* vcpu = hv_vcpu_current();
    if (!vcpu)
    {
        return STATUS_UNSUCCESSFUL;
    }

    hv_status_set(0x210);

    if (!hv_svm_supported())
    {
        return STATUS_NOT_SUPPORTED;
    }

    __writemsr(SVM_VM_HSAVE_PA_MSR, vcpu->svm_hsave_pa);

#if HV_SVM_STAGE < 2
    hv_status_set(0x87);
    return STATUS_SUCCESS;
#endif

    __writemsr(SVM_EFER_MSR, __readmsr(SVM_EFER_MSR) | SVM_EFER_SVME);

#if HV_SVM_STAGE < 3
    __writemsr(SVM_EFER_MSR, __readmsr(SVM_EFER_MSR) & ~SVM_EFER_SVME);
    hv_status_set(0x88);
    return STATUS_SUCCESS;
#endif

    hv_status_set(0x230);

    hv_svm_setup_vmcb(vcpu);
    hv_svm_setup_host_vmcb(vcpu);

    hv_asm_svm_launch(vcpu);

    hv_status_set(0x99);
    return STATUS_SUCCESS;
}

static VOID hv_svm_start_dpc(
    _In_ PKDPC dpc,
    _In_opt_ PVOID deferred_context,
    _In_opt_ PVOID system_argument1,
    _In_opt_ PVOID system_argument2)
{
    UNREFERENCED_PARAMETER(dpc);
    UNREFERENCED_PARAMETER(deferred_context);

    NTSTATUS status = hv_svm_start_vcpu();

    LONG* result = (LONG*)system_argument1;
    if (result && !NT_SUCCESS(status))
    {
        InterlockedIncrement(result);
    }

    KeSignalCallDpcSynchronize(system_argument2);
    KeSignalCallDpcDone(dpc);
}

static u64 hv_svm_teardown_ipi(u64 context)
{
    UNREFERENCED_PARAMETER(context);
    hv_asm_svm_teardown();
    return 0;
}

static void hv_svm_teardown_all_cpus(void)
{
    KeIpiGenericCall(hv_svm_teardown_ipi, 0);
}

NTSTATUS hv_svm_init(void)
{
    if (g_svm_active)
    {
        return STATUS_ALREADY_INITIALIZED;
    }

    u32 cpu_count = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    if (!cpu_count || cpu_count > HV_MAX_VCPU)
    {
        return STATUS_NOT_SUPPORTED;
    }

    hv_cpuid_cache_capture();

    NTSTATUS status = hv_vcpu_alloc(cpu_count);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    g_npt_pml4_pa = hv_npt_build();
    if (!g_npt_pml4_pa)
    {
        hv_status_set(0x240);
        hv_vcpu_free();
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    LONG failed = 0;
    KeGenericCallDpc(hv_svm_start_dpc, &failed);
    if (failed)
    {
        hv_status_set(0x250);
        hv_npt_destroy();
        hv_vcpu_free();
        return STATUS_UNSUCCESSFUL;
    }

    g_svm_active = 1;
    hv_status_flush();
    HV_LOG("svm active on %u processors, ncr3=%llx", cpu_count, g_npt_pml4_pa);
    return STATUS_SUCCESS;
}

void hv_svm_shutdown(void)
{
    if (!g_svm_active)
    {
        return;
    }

    hv_svm_teardown_all_cpus();
    g_svm_active = 0;
    hv_npt_destroy();
    hv_vcpu_free();
}

static u64 hv_vmcb_read(u8* vmcb, u64 off)
{
    return *(u64*)(vmcb + off);
}

static void hv_vmcb_write(u8* vmcb, u64 off, u64 value)
{
    *(u64*)(vmcb + off) = value;
}

static void hv_svm_advance_rip(u8* vmcb)
{
    hv_vmcb_write(vmcb, SVM_VMCB_RIP_OFF, hv_vmcb_read(vmcb, SVM_VMCB_NRIP_OFF));
}

static void hv_svm_inject_ud(u8* vmcb)
{
    hv_vmcb_write(vmcb, SVM_VMCB_EVENT_INJ_OFF,
        SVM_EVENTINJ_VALID | SVM_EVENTINJ_TYPE_EXCP | SVM_EVENTINJ_VEC_UD);
}

static void hv_svm_handle_cpuid(struct hv_vcpu* vcpu, struct hv_regs* regs, u8* vmcb)
{
    regs->rax = hv_vmcb_read(vmcb, SVM_VMCB_RAX_OFF);

    hv_cpuid_handle(vcpu, regs);

    hv_vmcb_write(vmcb, SVM_VMCB_RAX_OFF, regs->rax);
    hv_svm_advance_rip(vmcb);
}

static void hv_svm_handle_msr(struct hv_vcpu* vcpu, struct hv_regs* regs, u8* vmcb)
{
    u64 efer = vcpu->svm_guest_efer;
    u8 nbytes = *(u8*)(vmcb + 0xD0);
    u8* instr = vmcb + 0xD1;

    u32 msr = (u32)regs->rcx;
    u64 value = regs->rax | (regs->rdx << 32);

    if (msr == SVM_EFER_MSR)
    {
        if (nbytes >= 2 && instr[0] == 0x0F && instr[1] == 0x32)
        {
            u64 masked = efer & ~SVM_EFER_SVME;
            regs->rax = masked & 0xFFFFFFFFull;
            regs->rdx = (masked >> 32) & 0xFFFFFFFFull;
        }
        else
        {
            vcpu->svm_guest_efer = value;
            __writemsr(SVM_EFER_MSR, value | SVM_EFER_SVME);
        }
    }

    hv_svm_advance_rip(vmcb);
}

u8 hv_svm_exit_handler(struct hv_vcpu* vcpu, struct hv_regs* regs)
{
    u8* vmcb = vcpu->svm_vmcb_va;
    u64 exit_code = hv_vmcb_read(vmcb, SVM_VMCB_EXIT_CODE_OFF) & 0xFFFFFFFFFFFFull;

    ++vcpu->exit_count;

    switch (exit_code)
    {
    case SVM_EXIT_CPUID:
        hv_svm_handle_cpuid(vcpu, regs, vmcb);
        return 0;

    case SVM_EXIT_MSR:
        hv_svm_handle_msr(vcpu, regs, vmcb);
        return 0;

    case SVM_EXIT_VMMCALL:
        if (regs->r10 == HV_SIG0 && regs->r11 == HV_SIG1 && regs->r12 == HV_SIG2
            && regs->rax == HV_HYPERCALL_TEARDOWN)
        {
            hv_svm_advance_rip(vmcb);
            return 1;
        }
        hv_svm_inject_ud(vmcb);
        return 0;

    case SVM_EXIT_VMRUN:
        hv_svm_inject_ud(vmcb);
        return 0;

    case SVM_EXIT_NPF:
        KeBugCheckEx(0xC0DE0003, exit_code,
            hv_vmcb_read(vmcb, SVM_VMCB_EXIT_INFO1_OFF),
            hv_vmcb_read(vmcb, SVM_VMCB_EXIT_INFO2_OFF),
            hv_vmcb_read(vmcb, SVM_VMCB_RIP_OFF));

    default:
        KeBugCheckEx(0xC0DE0004, exit_code,
            hv_vmcb_read(vmcb, SVM_VMCB_EXIT_INFO1_OFF),
            hv_vmcb_read(vmcb, SVM_VMCB_RIP_OFF),
            vcpu->exit_count);
    }
}
