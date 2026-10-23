#include "vcpu.h"

struct hv_vcpu* g_hv_vcpus;
u32 g_hv_vcpu_count;

struct hv_vcpu* hv_vcpu_current(void)
{
    u32 index = KeGetCurrentProcessorNumberEx(NULL);
    if (index >= g_hv_vcpu_count)
    {
        return NULL;
    }
    return &g_hv_vcpus[index];
}

NTSTATUS hv_vcpu_alloc(u32 count)
{
    g_hv_vcpus = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(struct hv_vcpu) * count, HV_TAG);
    if (!g_hv_vcpus)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(g_hv_vcpus, sizeof(struct hv_vcpu) * count);
    g_hv_vcpu_count = count;

    NTSTATUS status = STATUS_SUCCESS;
    u32 pages_per_vcpu = 2 + 1 + HV_HOST_STACK_PAGES + 5;

    for (u32 i = 0; i < count && NT_SUCCESS(status); ++i)
    {
        struct hv_vcpu* vcpu = &g_hv_vcpus[i];

        PHYSICAL_ADDRESS low = { 0 };
        PHYSICAL_ADDRESS high = { .QuadPart = 0xFFFFFFFF };
        vcpu->vmxon_va = MmAllocateContiguousMemorySpecifyCache((SIZE_T)(pages_per_vcpu << 12), low, high, low, MmCached);
        if (!vcpu->vmxon_va)
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        vcpu->vmcs_va = vcpu->vmxon_va + 0x1000;
        vcpu->msr_bitmap_va = vcpu->vmcs_va + 0x1000;
        vcpu->host_stack_va = vcpu->msr_bitmap_va + 0x1000;

        vcpu->svm_vmcb_va = vcpu->host_stack_va + (HV_HOST_STACK_PAGES << 12);
        vcpu->svm_host_vmcb_va = vcpu->svm_vmcb_va + 0x1000;
        vcpu->svm_hsave_va = vcpu->svm_host_vmcb_va + 0x1000;
        vcpu->svm_msrpm_va = vcpu->svm_hsave_va + 0x1000;

        vcpu->vmxon_pa = MmGetPhysicalAddress(vcpu->vmxon_va).QuadPart;
        vcpu->vmcs_pa = MmGetPhysicalAddress(vcpu->vmcs_va).QuadPart;
        vcpu->msr_bitmap_pa = MmGetPhysicalAddress(vcpu->msr_bitmap_va).QuadPart;

        vcpu->svm_vmcb_pa = MmGetPhysicalAddress(vcpu->svm_vmcb_va).QuadPart;
        vcpu->svm_host_vmcb_pa = MmGetPhysicalAddress(vcpu->svm_host_vmcb_va).QuadPart;
        vcpu->svm_hsave_pa = MmGetPhysicalAddress(vcpu->svm_hsave_va).QuadPart;
        vcpu->svm_msrpm_pa = MmGetPhysicalAddress(vcpu->svm_msrpm_va).QuadPart;

        vcpu->host_rsp_top = (u64)(vcpu->host_stack_va + (HV_HOST_STACK_PAGES << 12)) - 0x100;
        vcpu->host_rsp_top &= ~0xFull;

        vcpu->vpid = (u32)(i + 1);
        vcpu->svm_asid = (u32)(i + 1);
    }

    if (!NT_SUCCESS(status))
    {
        hv_vcpu_free();
    }
    return status;
}

void hv_vcpu_free(void)
{
    if (!g_hv_vcpus)
    {
        return;
    }

    for (u32 i = 0; i < g_hv_vcpu_count; ++i)
    {
        if (g_hv_vcpus[i].vmxon_va)
        {
            MmFreeContiguousMemory(g_hv_vcpus[i].vmxon_va);
        }
    }

    ExFreePool(g_hv_vcpus);
    g_hv_vcpus = NULL;
    g_hv_vcpu_count = 0;
}
