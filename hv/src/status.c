#include "hv.h"

static HANDLE g_status_key;

#define HV_STATUS_KEY L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\orthrus"

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

void hv_status_set(u32 value)
{
    if (!g_status_key)
    {
        return;
    }

    UNICODE_STRING name;
    RtlInitUnicodeString(&name, L"Status");
    ZwSetValueKey(g_status_key, &name, 0, REG_DWORD, &value, sizeof(value));
}
