#pragma once

#include "hv.h"

#define SVM_EFER_MSR                0xC0000080
#define SVM_EFER_SVME               (1ull << 12)
#define SVM_VM_CR_MSR               0xC0010114
#define SVM_VM_CR_SVMDIS            (1ull << 4)
#define SVM_VM_HSAVE_PA_MSR         0xC0010117

#define SVM_MSRPM_SIZE              0x2000

#define SVM_INTERCEPT_MISC1_CPUID   (1u << 18)
#define SVM_INTERCEPT_MISC1_MSR_PROT (1u << 28)
#define SVM_INTERCEPT_MISC2_VMRUN   (1u << 0)
#define SVM_NP_ENABLE               (1ull << 0)

#define SVM_EXIT_CPUID              0x72
#define SVM_EXIT_MSR                0x7C
#define SVM_EXIT_SHUTDOWN           0x7F
#define SVM_EXIT_VMRUN              0x80
#define SVM_EXIT_VMMCALL            0x81
#define SVM_EXIT_NPF                0x400

#define SVM_EVENTINJ_VALID          (1ull << 31)
#define SVM_EVENTINJ_TYPE_EXCP      (2ull << 8)
#define SVM_EVENTINJ_VEC_UD         6

#define SVM_SELECTOR_MASK           0xF8

#define SVM_VMCB_CR_READ_OFF        0x000
#define SVM_VMCB_CR_WRITE_OFF       0x002
#define SVM_VMCB_DR_READ_OFF        0x004
#define SVM_VMCB_DR_WRITE_OFF       0x006
#define SVM_VMCB_EXCP_INTERCEPT_OFF 0x008
#define SVM_VMCB_MISC1_OFF          0x00C
#define SVM_VMCB_MISC2_OFF          0x010
#define SVM_VMCB_IOPM_PA_OFF        0x040
#define SVM_VMCB_MSRPM_PA_OFF       0x048
#define SVM_VMCB_TSC_OFFSET_OFF     0x050
#define SVM_VMCB_ASID_OFF           0x058
#define SVM_VMCB_TLB_CTRL_OFF       0x05C
#define SVM_VMCB_VINTR_OFF          0x060
#define SVM_VMCB_INTR_SHADOW_OFF    0x068
#define SVM_VMCB_EXIT_CODE_OFF      0x070
#define SVM_VMCB_EXIT_INFO1_OFF     0x078
#define SVM_VMCB_EXIT_INFO2_OFF     0x080
#define SVM_VMCB_EXIT_INT_INFO_OFF  0x088
#define SVM_VMCB_NP_ENABLE_OFF      0x090
#define SVM_VMCB_EVENT_INJ_OFF      0x0A8
#define SVM_VMCB_NCR3_OFF           0x0B0
#define SVM_VMCB_CLEAN_OFF          0x0C0
#define SVM_VMCB_NRIP_OFF           0x0C8

#define SVM_VMCB_ES_SEL_OFF         0x400
#define SVM_VMCB_CS_SEL_OFF         0x410
#define SVM_VMCB_SS_SEL_OFF         0x420
#define SVM_VMCB_DS_SEL_OFF         0x430
#define SVM_VMCB_FS_SEL_OFF         0x440
#define SVM_VMCB_GS_SEL_OFF         0x450
#define SVM_VMCB_GDTR_OFF           0x460
#define SVM_VMCB_LDTR_OFF           0x470
#define SVM_VMCB_IDTR_OFF           0x480
#define SVM_VMCB_TR_OFF             0x490
#define SVM_VMCB_LDTR_SEL_OFF       0x470
#define SVM_VMCB_TR_SEL_OFF         0x490
#define SVM_VMCB_CPL_OFF            0x4CB
#define SVM_VMCB_EFER_OFF           0x4D0
#define SVM_VMCB_CR4_OFF            0x548
#define SVM_VMCB_CR3_OFF            0x550
#define SVM_VMCB_CR0_OFF            0x558
#define SVM_VMCB_DR7_OFF            0x560
#define SVM_VMCB_DR6_OFF            0x568
#define SVM_VMCB_RFLAGS_OFF         0x570
#define SVM_VMCB_RIP_OFF            0x578
#define SVM_VMCB_RSP_OFF            0x5D8
#define SVM_VMCB_RAX_OFF            0x5F8
#define SVM_VMCB_STAR_OFF           0x600
#define SVM_VMCB_LSTAR_OFF          0x608
#define SVM_VMCB_CSTAR_OFF          0x610
#define SVM_VMCB_SFMASK_OFF         0x618
#define SVM_VMCB_KGSBASE_OFF        0x620
#define SVM_VMCB_SYSENTER_CS_OFF    0x628
#define SVM_VMCB_SYSENTER_ESP_OFF   0x630
#define SVM_VMCB_SYSENTER_EIP_OFF   0x638
#define SVM_VMCB_CR2_OFF            0x640
#define SVM_VMCB_GPAT_OFF           0x668
#define SVM_VMCB_DBGCTL_OFF         0x670

#define SVM_SEG_ATTRIB_OFF_SEL      0x0
#define SVM_SEG_ATTRIB_OFF_ATTR     0x2
#define SVM_SEG_ATTRIB_OFF_LIMIT    0x4
#define SVM_SEG_ATTRIB_OFF_BASE     0x8

struct hv_vcpu;

NTSTATUS hv_svm_init(void);
void     hv_svm_shutdown(void);
NTSTATUS hv_svm_start_vcpu(void);
u8       hv_svm_exit_handler(struct hv_vcpu* vcpu, struct hv_regs* regs);

u64      hv_npt_build(void);
void     hv_npt_destroy(void);

BOOLEAN  hv_svm_supported(void);
