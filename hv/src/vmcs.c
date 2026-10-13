#include "vcpu.h"
#include "ept.h"
#include "vmx.h"

extern void hv_asm_sgdt(u64* buffer);
extern void hv_asm_sidt(u64* buffer);
extern void hv_asm_get_segments(u16* selectors);
extern u64  hv_asm_get_dr7(void);
extern void hv_asm_exit_stub(void);

static void vmwrite64(u64 field, u64 value)
{
    if (__vmx_vmwrite(field, value))
    {
        HV_LOG("vmwrite failed field=%llx", field);
    }
}

static u64 hv_vmx_basic_revision(void)
{
    return __readmsr(IA32_VMX_BASIC) & 0x7FFFFFFFull;
}

static u64 hv_adjust_controls(u64 desired, u64 cap_msr)
{
    u64 cap = __readmsr(cap_msr);
    return (desired | (cap & 0xFFFFFFFF)) & (cap >> 32);
}

static void hv_segment_from_gdt(u16 selector, u64* base, u32* limit, u32* ar)
{
    *base = 0;
    *limit = 0;
    *ar = 1u << 16;

    u64 offset = selector & ~7ull;
    if (!(selector & ~7ull))
    {
        return;
    }

    u64 buffer[8] = { 0 };
    hv_asm_sgdt(buffer);
    u64 gdt_base = buffer[1];
    u16 gdt_limit = (u16)buffer[0];

    if (offset + 16 > gdt_limit)
    {
        return;
    }

    u8* desc = (u8*)(gdt_base + offset);
    u32 limit_raw = desc[0] | (desc[1] << 8) | ((desc[6] & 0xF) << 16);
    u32 attr = desc[5];
    u64 base_val = desc[2] | (desc[3] << 8) | (desc[4] << 16) | ((u64)desc[7] << 24);

    u32 type = attr & 0xF;
    if (type == 2 || type == 3 || type == 9 || type == 11)
    {
        base_val |= (u64)desc[8] << 32 | (u64)desc[9] << 40 | (u64)desc[10] << 48;
    }

    *base = base_val;
    *limit = limit_raw;

    u32 ar_val = (attr & 0xF)
        | (((attr >> 4) & 1) << 4)
        | (((attr >> 5) & 3) << 5)
        | (((attr >> 7) & 1) << 7)
        | (((desc[6] >> 5) & 1) << 13)
        | (((desc[6] >> 6) & 1) << 14)
        | (((desc[6] >> 7) & 1) << 15);

    if (!(attr & 0x80))
    {
        ar_val |= 1u << 16;
    }

    *ar = ar_val;
}

static void hv_write_guest_segment(u16 selector, u64 base_field, u64 limit_field, u64 ar_field, u64 sel_field, u64 override_base)
{
    u64 base = 0;
    u32 limit = 0;
    u32 ar = 0;

    hv_segment_from_gdt(selector, &base, &limit, &ar);
    if (override_base != (u64)-1)
    {
        base = override_base;
    }

    vmwrite64(base_field, base);
    vmwrite64(limit_field, limit);
    vmwrite64(ar_field, ar);
    vmwrite64(sel_field, selector);
}

NTSTATUS hv_vmcs_setup(struct hv_vcpu* vcpu, u64 eptp)
{
    RtlZeroMemory(vcpu->vmcs_va, 0x1000);
    *(u32*)vcpu->vmcs_va = (u32)hv_vmx_basic_revision();

    u64 vmcs_pa = vcpu->vmcs_pa;
    if (__vmx_vmclear(&vmcs_pa))
    {
        HV_LOG("vmclear failed");
        return STATUS_UNSUCCESSFUL;
    }

    if (__vmx_vmptrld(&vmcs_pa))
    {
        HV_LOG("vmptrld failed");
        return STATUS_UNSUCCESSFUL;
    }

    u64 basic = __readmsr(IA32_VMX_BASIC);
    u64 true_ctls = basic & VMX_TRUE_CTLS_AVAILABLE;
    u64 pin_msr = true_ctls ? IA32_VMX_TRUE_PINBASED : IA32_VMX_PINBASED_CTLS;
    u64 proc_msr = true_ctls ? IA32_VMX_TRUE_PROCBASED : IA32_VMX_PROCBASED_CTLS;
    u64 exit_msr = true_ctls ? IA32_VMX_TRUE_EXIT_CTLS : IA32_VMX_EXIT_CTLS;
    u64 entry_msr = true_ctls ? IA32_VMX_TRUE_ENTRY_CTLS : IA32_VMX_ENTRY_CTLS;

    u64 pin = hv_adjust_controls(0, pin_msr);
    u64 proc1 = hv_adjust_controls(
        PROC1_CPUID_EXITING | PROC1_USE_MSR_BITMAPS | PROC1_ACTIVATE_SECONDARY, proc_msr);
    u64 proc2 = hv_adjust_controls(
        PROC2_ENABLE_EPT | PROC2_ENABLE_VPID | PROC2_ENABLE_RDTSCP
        | PROC2_ENABLE_INVPCID | PROC2_ENABLE_XSAVES | PROC2_ENABLE_USER_WAIT,
        IA32_VMX_PROCBASED_CTLS2);
    u64 exit = hv_adjust_controls(EXIT_HOST_ADDR_SPACE_SIZE, exit_msr);
    u64 entry = hv_adjust_controls(ENTRY_IA32E_MODE, entry_msr);

    vmwrite64(VMCS_CTRL_PIN, pin);
    vmwrite64(VMCS_CTRL_PROC1, proc1);
    vmwrite64(VMCS_CTRL_PROC2, proc2);
    vmwrite64(VMCS_CTRL_EXIT, exit);
    vmwrite64(VMCS_CTRL_ENTRY, entry);

    vmwrite64(VMCS_CTRL_EXCEPTION_BITMAP, 0);
    vmwrite64(VMCS_CTRL_MSR_BITMAP, vcpu->msr_bitmap_pa);
    vmwrite64(VMCS_CTRL_EPT_POINTER, eptp);
    vmwrite64(VMCS_CTRL_VPID, vcpu->vpid);

    u64 cr0 = __readcr0();
    u64 cr4 = __readcr4();
    u64 cr0_fixed0 = __readmsr(IA32_VMX_CR0_FIXED0);
    u64 cr4_fixed0 = __readmsr(IA32_VMX_CR4_FIXED0);

    u64 guest_cr0 = (cr0 | (cr0_fixed0 & 0xFFFFFFFF)) & __readmsr(IA32_VMX_CR0_FIXED1);
    u64 guest_cr4 = (cr4 | (cr4_fixed0 & 0xFFFFFFFF)) & __readmsr(IA32_VMX_CR4_FIXED1);

    vmwrite64(VMCS_GUEST_CR0, guest_cr0);
    vmwrite64(VMCS_GUEST_CR3, __readcr3());
    vmwrite64(VMCS_GUEST_CR4, guest_cr4);
    vmwrite64(VMCS_CTRL_CR0_SHADOW, guest_cr0);
    vmwrite64(VMCS_CTRL_CR4_SHADOW, guest_cr4 & ~CR4_VMXE_BIT);

    u16 sels[8] = { 0 };
    hv_asm_get_segments(sels);

    u64 fs_base = __readmsr(IA32_FS_BASE_MSR);
    u64 gs_base = __readmsr(IA32_GS_BASE_MSR);

    hv_write_guest_segment(sels[0], VMCS_GUEST_ES_BASE, VMCS_GUEST_ES_LIMIT, VMCS_GUEST_ES_AR, VMCS_GUEST_ES_SELECTOR, (u64)-1);
    hv_write_guest_segment(sels[1], VMCS_GUEST_CS_BASE, VMCS_GUEST_CS_LIMIT, VMCS_GUEST_CS_AR, VMCS_GUEST_CS_SELECTOR, (u64)-1);
    hv_write_guest_segment(sels[2], VMCS_GUEST_SS_BASE, VMCS_GUEST_SS_LIMIT, VMCS_GUEST_SS_AR, VMCS_GUEST_SS_SELECTOR, (u64)-1);
    hv_write_guest_segment(sels[3], VMCS_GUEST_DS_BASE, VMCS_GUEST_DS_LIMIT, VMCS_GUEST_DS_AR, VMCS_GUEST_DS_SELECTOR, (u64)-1);
    hv_write_guest_segment(sels[4], VMCS_GUEST_FS_BASE, VMCS_GUEST_FS_LIMIT, VMCS_GUEST_FS_AR, VMCS_GUEST_FS_SELECTOR, fs_base);
    hv_write_guest_segment(sels[5], VMCS_GUEST_GS_BASE, VMCS_GUEST_GS_LIMIT, VMCS_GUEST_GS_AR, VMCS_GUEST_GS_SELECTOR, gs_base);
    hv_write_guest_segment(0, VMCS_GUEST_LDTR_BASE, VMCS_GUEST_LDTR_LIMIT, VMCS_GUEST_LDTR_AR, VMCS_GUEST_LDTR_SELECTOR, (u64)-1);
    hv_write_guest_segment(sels[7], VMCS_GUEST_TR_BASE, VMCS_GUEST_TR_LIMIT, VMCS_GUEST_TR_AR, VMCS_GUEST_TR_SELECTOR, (u64)-1);

    u64 gdtr[8] = { 0 };
    u64 idtr[8] = { 0 };
    hv_asm_sgdt(gdtr);
    hv_asm_sidt(idtr);
    vmwrite64(VMCS_GUEST_GDTR_BASE, gdtr[1]);
    vmwrite64(VMCS_GUEST_GDTR_LIMIT, gdtr[0]);
    vmwrite64(VMCS_GUEST_IDTR_BASE, idtr[1]);
    vmwrite64(VMCS_GUEST_IDTR_LIMIT, idtr[0]);

    vmwrite64(VMCS_GUEST_DR7, hv_asm_get_dr7());
    vmwrite64(VMCS_GUEST_DEBUGCTL, 0);
    vmwrite64(VMCS_GUEST_EFER, __readmsr(IA32_EFER_MSR));
    vmwrite64(VMCS_LINK_POINTER, ~0ull);
    vmwrite64(VMCS_GUEST_INTERRUPTIBILITY, 0);
    vmwrite64(VMCS_GUEST_ACTIVITY_STATE, 0);
    vmwrite64(VMCS_GUEST_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS_MSR));
    vmwrite64(VMCS_GUEST_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP_MSR));
    vmwrite64(VMCS_GUEST_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP_MSR));

    vmwrite64(VMCS_HOST_CR0, guest_cr0);
    vmwrite64(VMCS_HOST_CR3, __readcr3());
    vmwrite64(VMCS_HOST_CR4, guest_cr4);

    vmwrite64(VMCS_HOST_ES_SELECTOR, sels[0]);
    vmwrite64(VMCS_HOST_CS_SELECTOR, sels[1]);
    vmwrite64(VMCS_HOST_SS_SELECTOR, sels[2]);
    vmwrite64(VMCS_HOST_DS_SELECTOR, sels[3]);
    vmwrite64(VMCS_HOST_FS_SELECTOR, sels[4]);
    vmwrite64(VMCS_HOST_GS_SELECTOR, sels[5]);
    vmwrite64(VMCS_HOST_TR_SELECTOR, sels[7]);

    u64 tr_base = 0;
    u32 tr_limit = 0;
    u32 tr_ar = 0;
    hv_segment_from_gdt(sels[7], &tr_base, &tr_limit, &tr_ar);

    vmwrite64(VMCS_HOST_TR_BASE, tr_base);
    vmwrite64(VMCS_HOST_FS_BASE, fs_base);
    vmwrite64(VMCS_HOST_GS_BASE, gs_base);
    vmwrite64(VMCS_HOST_EFER, __readmsr(IA32_EFER_MSR));
    vmwrite64(VMCS_HOST_PAT, __readmsr(IA32_PAT_MSR));
    vmwrite64(VMCS_HOST_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS_MSR));
    vmwrite64(VMCS_HOST_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP_MSR));
    vmwrite64(VMCS_HOST_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP_MSR));

    vmwrite64(VMCS_HOST_RSP, vcpu->host_rsp_top);
    vmwrite64(VMCS_HOST_RIP, (u64)hv_asm_exit_stub);

    vcpu->cr4_shadow = guest_cr4 & ~CR4_VMXE_BIT;
    return STATUS_SUCCESS;
}
