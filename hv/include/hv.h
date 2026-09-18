#pragma once

#include <ntddk.h>
#include <intrin.h>

#define HV_TAG 'vrth'
#define HV_MAX_VCPU 64

#define HV_LOG(msg, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[hv] " msg "\n", __VA_ARGS__)

typedef unsigned __int8  u8;
typedef unsigned __int16 u16;
typedef unsigned __int32 u32;
typedef unsigned __int64 u64;
typedef __int8  i8;
typedef __int16 i16;
typedef __int32 i32;
typedef __int64 i64;

struct hv_vcpu;
struct hv_regs;

enum hv_hypercall
{
    HV_HYPERCALL_TEARDOWN = 0x1,
};

#define HV_SIG0 0x4F525448ull
#define HV_SIG1 0x766D6D21ull
#define HV_SIG2 0x67617465ull

NTSTATUS hv_init(void);
void     hv_shutdown(void);

NTSTATUS hv_start_vcpu(void);
void     hv_vcpu_teardown(void);

extern u64 g_hv_native_cpuid_ticks;

void hv_cpuid_cache_capture(void);
void hv_cpuid_handle(struct hv_vcpu* vcpu, struct hv_regs* regs);

void hv_timing_calibrate(void);
void hv_timing_on_cpuid_exit(struct hv_vcpu* vcpu);
void hv_timing_on_rdtsc_exit(struct hv_vcpu* vcpu, struct hv_regs* regs);
void hv_timing_disarm(struct hv_vcpu* vcpu);
