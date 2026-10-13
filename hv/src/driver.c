#include "vcpu.h"
#include "ept.h"
#include "vmx.h"

extern NTSTATUS hv_vmcs_setup(struct hv_vcpu* vcpu, u64 eptp);
extern u64     hv_cpu_teardown_ipi(u64 context);

VOID KeGenericCallDpc(_In_ PKDEFERRED_ROUTINE Routine, _In_opt_ PVOID Context);
VOID KeSignalCallDpcDone(_In_ PKDPC Dpc);
VOID KeSignalCallDpcSynchronize(_In_ PVOID SynchronizeContext);

static u64 g_eptp;
static u8  g_hv_active;

static VOID hv_start_dpc(
    _In_ PKDPC dpc,
    _In_opt_ PVOID deferred_context,
    _In_opt_ PVOID system_argument1,
    _In_opt_ PVOID system_argument2)
{
    UNREFERENCED_PARAMETER(dpc);
    UNREFERENCED_PARAMETER(deferred_context);

    NTSTATUS status = hv_start_vcpu();

    LONG* result = (LONG*)system_argument1;
    if (result && !NT_SUCCESS(status))
    {
        InterlockedIncrement(result);
    }

    KeSignalCallDpcSynchronize(system_argument2);
    KeSignalCallDpcDone(dpc);
}

static NTSTATUS hv_bring_up_all_cpus(void)
{
    LONG failed = 0;
    KeGenericCallDpc(hv_start_dpc, &failed);
    return failed ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS;
}

static void hv_teardown_all_cpus(void)
{
    KeIpiGenericCall(hv_cpu_teardown_ipi, 0);
}

NTSTATUS hv_init(void)
{
    if (g_hv_active)
    {
        return STATUS_ALREADY_INITIALIZED;
    }

    u32 cpu_count = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    if (!cpu_count || cpu_count > HV_MAX_VCPU)
    {
        return STATUS_NOT_SUPPORTED;
    }

    hv_cpuid_cache_capture();
    hv_timing_calibrate();

    NTSTATUS status = hv_vcpu_alloc(cpu_count);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    g_eptp = hv_ept_build();
    if (!g_eptp)
    {
        hv_vcpu_free();
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = hv_bring_up_all_cpus();
    if (!NT_SUCCESS(status))
    {
        hv_teardown_all_cpus();
        hv_ept_destroy();
        hv_vcpu_free();
        return status;
    }

    g_hv_active = 1;
    HV_LOG("virtualized %u processors, eptp=%llx", cpu_count, g_eptp);
    return STATUS_SUCCESS;
}

void hv_shutdown(void)
{
    if (!g_hv_active)
    {
        return;
    }

    hv_teardown_all_cpus();
    hv_ept_destroy();
    hv_vcpu_free();
    g_hv_active = 0;
}

static VOID hv_driver_unload(_In_ PDRIVER_OBJECT driver)
{
    UNREFERENCED_PARAMETER(driver);
    hv_shutdown();
    HV_LOG("unloaded");
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT driver, _In_ PUNICODE_STRING registry_path)
{
    UNREFERENCED_PARAMETER(registry_path);

    NTSTATUS status = hv_init();
    if (!NT_SUCCESS(status))
    {
        HV_LOG("init failed status=%08lX", status);
        return status;
    }

    driver->DriverUnload = hv_driver_unload;
    return STATUS_SUCCESS;
}
