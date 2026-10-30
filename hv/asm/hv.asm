PUBLIC hv_asm_exit_stub
PUBLIC hv_asm_vmx_launch
PUBLIC hv_asm_guest_entry
PUBLIC hv_asm_teardown_call
PUBLIC hv_asm_sgdt
PUBLIC hv_asm_sidt
PUBLIC hv_asm_get_segments
PUBLIC hv_asm_get_dr7

EXTERNDEF hv_exit_handler:proc
EXTERNDEF hv_vcpu_teardown:proc

GuestRsp   EQU 681Ch
GuestRip   EQU 681Eh
GuestFlags EQU 6820h

HV_SIG0 EQU 4F525448h
HV_SIG1 EQU 766D6D21h
HV_SIG2 EQU 67617465h

.code

hv_asm_exit_stub PROC
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rbx
    push rdx
    push rcx
    push rax
    sub  rsp, 28h
    mov  rcx, rsp
    call hv_exit_handler
    add  rsp, 28h
    pop  rax
    pop  rcx
    pop  rdx
    pop  rbx
    pop  rbp
    pop  rsi
    pop  rdi
    pop  r8
    pop  r9
    pop  r10
    pop  r11
    pop  r12
    pop  r13
    pop  r14
    pop  r15
    test al, al
    jz   hv_teardown_continue
    vmresume
    jz   hv_teardown_continue
    jc   hv_teardown_continue
    int  3

hv_teardown_continue:
    mov  r11, GuestRsp
    vmread rax, r11
    push rax
    mov  r11, GuestRip
    vmread rax, r11
    push rax
    sub  rsp, 28h
    call hv_vcpu_teardown
    add  rsp, 28h
    pop  rax
    pop  rsp
    jmp  rax
hv_asm_exit_stub ENDP

hv_asm_vmx_launch PROC
    mov  r10, GuestRsp
    vmwrite r10, rsp
    mov  r10, GuestRip
    mov  rax, offset hv_asm_guest_entry
    vmwrite r10, rax
    pushfq
    pop  rax
    or   rax, 2
    mov  r10, GuestFlags
    vmwrite r10, rax
    vmlaunch
    xor  eax, eax
    ret
hv_asm_vmx_launch ENDP

hv_asm_guest_entry PROC
    mov  eax, 1
    ret
hv_asm_guest_entry ENDP

hv_asm_teardown_call PROC
    mov  r10, HV_SIG0
    mov  r11, HV_SIG1
    mov  r12, HV_SIG2
    mov  rax, 1
    vmcall
    ret
hv_asm_teardown_call ENDP

hv_asm_sgdt PROC
    sgdt fword ptr [rcx]
    ret
hv_asm_sgdt ENDP

hv_asm_sidt PROC
    sidt fword ptr [rcx]
    ret
hv_asm_sidt ENDP

hv_asm_get_segments PROC
    mov  ax, es
    mov  word ptr [rcx+0], ax
    mov  ax, cs
    mov  word ptr [rcx+2], ax
    mov  ax, ss
    mov  word ptr [rcx+4], ax
    mov  ax, ds
    mov  word ptr [rcx+6], ax
    mov  ax, fs
    mov  word ptr [rcx+8], ax
    mov  ax, gs
    mov  word ptr [rcx+10], ax
    sldt ax
    mov  word ptr [rcx+12], ax
    str  ax
    mov  word ptr [rcx+14], ax
    ret
hv_asm_get_segments ENDP

hv_asm_get_dr7 PROC
    mov  rax, dr7
    ret
hv_asm_get_dr7 ENDP

end
