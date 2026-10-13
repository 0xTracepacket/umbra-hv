#pragma once

#define IA32_FEATURE_CONTROL        0x3A
#define IA32_VMX_BASIC              0x480
#define IA32_VMX_PINBASED_CTLS      0x481
#define IA32_VMX_PROCBASED_CTLS     0x482
#define IA32_VMX_EXIT_CTLS          0x483
#define IA32_VMX_ENTRY_CTLS         0x484
#define IA32_VMX_TRUE_PINBASED      0x48D
#define IA32_VMX_TRUE_PROCBASED     0x48E
#define IA32_VMX_TRUE_EXIT_CTLS     0x48F
#define IA32_VMX_TRUE_ENTRY_CTLS    0x490
#define IA32_VMX_PROCBASED_CTLS2    0x48B
#define IA32_VMX_EPT_VPID_CAP       0x48C
#define IA32_VMX_CR0_FIXED0         0x486
#define IA32_VMX_CR0_FIXED1         0x487
#define IA32_VMX_CR4_FIXED0         0x488
#define IA32_VMX_CR4_FIXED1         0x489

#define IA32_FS_BASE_MSR            0xC0000100
#define IA32_GS_BASE_MSR            0xC0000101
#define IA32_KERNEL_GS_BASE_MSR     0xC0000102
#define IA32_EFER_MSR               0xC0000080
#define IA32_PAT_MSR                0x277
#define IA32_SYSENTER_CS_MSR        0x174
#define IA32_SYSENTER_ESP_MSR       0x175
#define IA32_SYSENTER_EIP_MSR       0x176

#define IA32_TSC_MSR                0x10

#define CR4_VMXE_BIT                (1ull << 13)

#define PIN_EXT_INTERRUPT_EXITING   (1ull << 0)
#define PIN_NMI_EXITING             (1ull << 3)

#define PROC1_INTERRUPT_WINDOW      (1ull << 2)
#define PROC1_TSC_OFFSETTING        (1ull << 3)
#define PROC1_HLT_EXITING           (1ull << 7)
#define PROC1_INVLPG_EXITING        (1ull << 9)
#define PROC1_MWAIT_EXITING         (1ull << 10)
#define PROC1_RDPMC_EXITING         (1ull << 11)
#define PROC1_RDTSC_EXITING         (1ull << 12)
#define PROC1_CR3_LOAD_EXITING      (1ull << 15)
#define PROC1_CR3_STORE_EXITING     (1ull << 16)
#define PROC1_CR8_LOAD_EXITING      (1ull << 19)
#define PROC1_CR8_STORE_EXITING     (1ull << 20)
#define PROC1_CPUID_EXITING         (1ull << 21)
#define PROC1_MOV_DR_EXITING        (1ull << 23)
#define PROC1_UNCOND_IO_EXITING     (1ull << 24)
#define PROC1_USE_IO_BITMAPS        (1ull << 25)
#define PROC1_USE_MSR_BITMAPS       (1ull << 28)
#define PROC1_MONITOR_EXITING       (1ull << 29)
#define PROC1_ACTIVATE_SECONDARY    (1ull << 31)

#define PROC2_ENABLE_EPT            (1ull << 1)
#define PROC2_DESC_TABLE_EXITING    (1ull << 2)
#define PROC2_ENABLE_RDTSCP         (1ull << 3)
#define PROC2_ENABLE_VPID           (1ull << 5)
#define PROC2_WBINVD_EXITING        (1ull << 6)
#define PROC2_UNRESTRICTED_GUEST    (1ull << 7)
#define PROC2_ENABLE_INVPCID        (1ull << 10)
#define PROC2_EPT_VIOLATION_VE      (1ull << 15)
#define PROC2_ENABLE_XSAVES         (1ull << 17)
#define PROC2_ENABLE_USER_WAIT      (1ull << 28)

#define EXIT_HOST_ADDR_SPACE_SIZE   (1ull << 9)
#define ENTRY_IA32E_MODE            (1ull << 9)

#define VMCS_LINK_POINTER           0x2800
#define VMCS_GUEST_DEBUGCTL         0x2802
#define VMCS_GUEST_EFER             0x2806

#define VMCS_GUEST_ES_BASE          0x0680
#define VMCS_GUEST_CS_BASE          0x0682
#define VMCS_GUEST_SS_BASE          0x0684
#define VMCS_GUEST_DS_BASE          0x0686
#define VMCS_GUEST_FS_BASE          0x0688
#define VMCS_GUEST_GS_BASE          0x068A
#define VMCS_GUEST_LDTR_BASE        0x068C
#define VMCS_GUEST_TR_BASE          0x0690
#define VMCS_GUEST_GDTR_BASE        0x0690
#define VMCS_GUEST_IDTR_BASE        0x0692

#define VMCS_GUEST_CR0              0x6806
#define VMCS_GUEST_CR3              0x6808
#define VMCS_GUEST_CR4              0x680A

#define VMCS_GUEST_DR7              0x6818
#define VMCS_GUEST_RSP              0x681C
#define VMCS_GUEST_RIP              0x681E
#define VMCS_GUEST_RFLAGS           0x6820
#define VMCS_GUEST_SYSENTER_ESP     0x6824
#define VMCS_GUEST_SYSENTER_EIP     0x6826

#define VMCS_GUEST_ES_SELECTOR      0x0800
#define VMCS_GUEST_CS_SELECTOR      0x0802
#define VMCS_GUEST_SS_SELECTOR      0x0804
#define VMCS_GUEST_DS_SELECTOR      0x0806
#define VMCS_GUEST_FS_SELECTOR      0x0808
#define VMCS_GUEST_GS_SELECTOR      0x080A
#define VMCS_GUEST_LDTR_SELECTOR    0x080C
#define VMCS_GUEST_TR_SELECTOR      0x080E

#define VMCS_GUEST_ES_LIMIT         0x4800
#define VMCS_GUEST_CS_LIMIT         0x4802
#define VMCS_GUEST_SS_LIMIT         0x4804
#define VMCS_GUEST_DS_LIMIT         0x4806
#define VMCS_GUEST_FS_LIMIT         0x4808
#define VMCS_GUEST_GS_LIMIT         0x480A
#define VMCS_GUEST_LDTR_LIMIT       0x480C
#define VMCS_GUEST_TR_LIMIT         0x480E
#define VMCS_GUEST_ES_AR            0x4810
#define VMCS_GUEST_CS_AR            0x4812
#define VMCS_GUEST_SS_AR            0x4814
#define VMCS_GUEST_DS_AR            0x4816
#define VMCS_GUEST_FS_AR            0x4818
#define VMCS_GUEST_GS_AR            0x481A
#define VMCS_GUEST_LDTR_AR          0x481C
#define VMCS_GUEST_TR_AR            0x481E
#define VMCS_GUEST_INTERRUPTIBILITY 0x4824
#define VMCS_GUEST_ACTIVITY_STATE   0x4826
#define VMCS_GUEST_SYSENTER_CS      0x482A
#define VMCS_GUEST_GDTR_LIMIT       0x4820
#define VMCS_GUEST_IDTR_LIMIT       0x4822

#define VMCS_HOST_ES_SELECTOR       0x0C00
#define VMCS_HOST_CS_SELECTOR       0x0C02
#define VMCS_HOST_SS_SELECTOR       0x0C04
#define VMCS_HOST_DS_SELECTOR       0x0C06
#define VMCS_HOST_FS_SELECTOR       0x0C08
#define VMCS_HOST_GS_SELECTOR       0x0C0A
#define VMCS_HOST_TR_SELECTOR       0x0C0C

#define VMCS_HOST_CR0               0x6C00
#define VMCS_HOST_CR3               0x6C02
#define VMCS_HOST_CR4               0x6C04
#define VMCS_HOST_FS_BASE           0x6C06
#define VMCS_HOST_GS_BASE           0x6C08
#define VMCS_HOST_TR_BASE           0x6C0A
#define VMCS_HOST_SYSENTER_ESP      0x6C0C
#define VMCS_HOST_SYSENTER_EIP      0x6C0E
#define VMCS_HOST_SYSENTER_CS       0x4C00
#define VMCS_HOST_RSP               0x6C14
#define VMCS_HOST_RIP               0x6C16
#define VMCS_HOST_EFER              0x2C02
#define VMCS_HOST_PAT               0x2C00

#define VMCS_CTRL_PIN               0x4000
#define VMCS_CTRL_PROC1             0x4002
#define VMCS_CTRL_EXCEPTION_BITMAP  0x4004
#define VMCS_CTRL_CR0_SHADOW        0x4006
#define VMCS_CTRL_CR4_SHADOW        0x4008
#define VMCS_CTRL_EXIT              0x400C
#define VMCS_CTRL_ENTRY             0x4012
#define VMCS_CTRL_PROC2             0x401E
#define VMCS_CTRL_ENTRY_INT_INFO    0x4016
#define VMCS_CTRL_VPID              0x0000
#define VMCS_CTRL_EPT_POINTER       0x2018
#define VMCS_CTRL_MSR_BITMAP        0x2004

#define VMCS_EXIT_REASON            0x4402
#define VMCS_EXIT_QUALIFICATION     0x6400
#define VMCS_EXIT_INSTRUCTION_LEN   0x440C
#define VMCS_VMX_INSTRUCTION_ERROR  0x4400

#define EXIT_EXCEPTION_NMI          0
#define EXIT_EXTERNAL_INTERRUPT     1
#define EXIT_TRIPLE_FAULT           2
#define EXIT_CPUID                  10
#define EXIT_HLT                    12
#define EXIT_INVD                   13
#define EXIT_INVLPG                 14
#define EXIT_RDPMC                  15
#define EXIT_RDTSC                  16
#define EXIT_VMCALL                 18
#define EXIT_VMCLEAR                19
#define EXIT_VMLAUNCH               20
#define EXIT_VMPTRLD                21
#define EXIT_VMPTRST                22
#define EXIT_VMREAD                 23
#define EXIT_VMRESUME               24
#define EXIT_VMWRITE                25
#define EXIT_VMXOFF                 26
#define EXIT_VMXON                  27
#define EXIT_CR_ACCESS              28
#define EXIT_DR_ACCESS              29
#define EXIT_MSR_READ               31
#define EXIT_MSR_WRITE              32
#define EXIT_EPT_VIOLATION          48
#define EXIT_EPT_MISCONFIG          49
#define EXIT_XSETBV                 55
#define EXIT_XSAVES                 56
#define EXIT_XRSTORS                57

#define UD_VECTOR                   6
#define VMX_MEMTYPE_UC              0
#define VMX_MEMTYPE_WB              6

#define VMX_TRUE_CTLS_AVAILABLE     (1ull << 55)
