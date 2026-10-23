#pragma once

#include "hv.h"

#define HV_HOST_STACK_PAGES 4

struct hv_regs
{
    u64 rax;
    u64 rcx;
    u64 rdx;
    u64 rbx;
    u64 rbp;
    u64 rsi;
    u64 rdi;
    u64 r8;
    u64 r9;
    u64 r10;
    u64 r11;
    u64 r12;
    u64 r13;
    u64 r14;
    u64 r15;
};

struct hv_vcpu
{
    u8*   vmxon_va;
    u8*   vmcs_va;
    u8*   msr_bitmap_va;
    u8*   host_stack_va;
    u64   vmxon_pa;
    u64   vmcs_pa;
    u64   msr_bitmap_pa;
    u64   host_rsp_top;
    u32   vpid;
    u64   cr4_shadow;
    u64   tsc_trap_armed;
    u64   tsc_comp_value;
    u64   last_guest_tsc;
    u64   exit_count;
    u8    teardown;
    u8*   svm_vmcb_va;
    u64   svm_vmcb_pa;
    u8*   svm_host_vmcb_va;
    u64   svm_host_vmcb_pa;
    u8*   svm_hsave_va;
    u64   svm_hsave_pa;
    u8*   svm_msrpm_va;
    u64   svm_msrpm_pa;
    u64   svm_guest_efer;
    u32   svm_asid;
};

extern struct hv_vcpu* g_hv_vcpus;
extern u32 g_hv_vcpu_count;

struct hv_vcpu* hv_vcpu_current(void);
NTSTATUS hv_vcpu_alloc(u32 count);
void     hv_vcpu_free(void);
