#include "hv.h"

static HANDLE g_status_key;
static volatile LONG g_status_pending = -1;

void hv_status_init(PCUNICODE_STRING registry_path)
{
    OBJECT_ATTRIBUTES attrs;
    UNICODE_STRING name;
    ULONG disposition = 0;

    if (!registry_path || !registry_path->Buffer)
    {
        return;
    }

    name.Buffer = registry_path->Buffer;
    name.Length = registry_path->Length;
    name.MaximumLength = registry_path->MaximumLength;

    InitializeObjectAttributes(&attrs, &name, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    NTSTATUS status = ZwCreateKey(&g_status_key, KEY_SET_VALUE, &attrs, 0, NULL, REG_OPTION_NON_VOLATILE, &disposition);
    if (!NT_SUCCESS(status))
    {
        g_status_key = NULL;
    }
}

static void hv_status_commit(u32 value)
{
    UNICODE_STRING name;
    RtlInitUnicodeString(&name, L"Status");
    ZwSetValueKey(g_status_key, &name, 0, REG_DWORD, &value, sizeof(value));
    ZwFlushKey(g_status_key);
}

void hv_status_set(u32 value)
{
    if (!g_status_key)
    {
        return;
    }

    if (KeGetCurrentIrql() > PASSIVE_LEVEL)
    {
        InterlockedExchange(&g_status_pending, (LONG)value);
        return;
    }

    hv_status_commit(value);
}

void hv_status_flush(void)
{
    if (!g_status_key)
    {
        return;
    }

    LONG pending = InterlockedExchange(&g_status_pending, -1);
    if (pending >= 0)
    {
        hv_status_commit((u32)pending);
    }
}
