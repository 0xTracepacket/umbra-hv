#include "vcpu.h"
#include "ept.h"
#include "vmx.h"

extern u8   hv_asm_vmx_launch(void);
extern void hv_asm_hypercall_teardown(void);
extern NTSTATUS hv_vmcs_setup(struct hv_vcpu* vcpu, u64 eptp);

static BOOLEAN hv_vmx_available(void)
{
    int regs[4] = { 0 };
    __cpuid(regs, 1);
    if (!(regs[2] & (1 << 5)))
    {
        return FALSE;
    }

    u64 feature_control = __readmsr(IA32_FEATURE_CONTROL);
    if (!(feature_control & 1))
    {
        return FALSE;
    }
    if (!(feature_control & (1ull << 2)))
    {
        return FALSE;
    }

    return TRUE;
}

static NTSTATUS hv_vmx_enable_cr4(void)
{
    u64 cr4 = __readcr4();
    if (!(cr4 & CR4_VMXE_BIT))
    {
        __writecr4(cr4 | CR4_VMXE_BIT);
    }
    return STATUS_SUCCESS;
}

NTSTATUS hv_start_vcpu(void)
{
    struct hv_vcpu* vcpu = hv_vcpu_current();
    if (!vcpu)
    {
        return STATUS_UNSUCCESSFUL;
    }

    if (!hv_vmx_available())
    {
        HV_LOG("vmx not available on cpu %u", KeGetCurrentProcessorNumberEx(NULL));
        return STATUS_NOT_SUPPORTED;
    }

    hv_vmx_enable_cr4();

    u32 revision = (u32)(__readmsr(IA32_VMX_BASIC) & 0x7FFFFFFFull);
    RtlZeroMemory(vcpu->vmxon_va, 0x1000);
    *(u32*)vcpu->vmxon_va = revision;

    u64 vmxon_pa = vcpu->vmxon_pa;
    if (__vmx_on(&vmxon_pa))
    {
        HV_LOG("vmxon failed on cpu %u", KeGetCurrentProcessorNumberEx(NULL));
        __writecr4(__readcr4() & ~CR4_VMXE_BIT);
        return STATUS_UNSUCCESSFUL;
    }

    NTSTATUS status = hv_vmcs_setup(vcpu, hv_ept_pointer());
    if (!NT_SUCCESS(status))
    {
        __vmx_off();
        __writecr4(__readcr4() & ~CR4_VMXE_BIT);
        return status;
    }

    if (!hv_asm_vmx_launch())
    {
        u64 error = 0;
        __vmx_vmread(VMCS_VMX_INSTRUCTION_ERROR, &error);
        HV_LOG("vmlaunch failed on cpu %u error=%llu", KeGetCurrentProcessorNumberEx(NULL), error);
        __vmx_off();
        __writecr4(__readcr4() & ~CR4_VMXE_BIT);
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

void hv_vcpu_teardown(void)
{
    struct hv_vcpu* vcpu = hv_vcpu_current();
    if (!vcpu)
    {
        return;
    }

    u64 vmcs_pa = vcpu->vmcs_pa;
    __vmx_vmclear(&vmcs_pa);
    __vmx_off();
    __writecr4(__readcr4() & ~CR4_VMXE_BIT);
}

u64 hv_cpu_teardown_ipi(u64 context)
{
    UNREFERENCED_PARAMETER(context);
    hv_asm_hypercall_teardown();
    return 0;
}
