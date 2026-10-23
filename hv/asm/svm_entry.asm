PUBLIC hv_asm_svm_launch
PUBLIC hv_asm_svm_guest_entry
PUBLIC hv_asm_svm_teardown

EXTERNDEF hv_svm_exit_handler:proc

GuestVmcbPa EQU 0
VcpuPtr     EQU 8
HostVmcbPa  EQU 16

.code

hv_asm_svm_launch PROC
    mov rax, rcx
    mov r10, [rax + 78h]
    mov r11, [rax + 38h]

    mov rdx, [rax + 80h]
    mov [r11 + GuestVmcbPa], rdx
    mov [r11 + VcpuPtr], rcx
    mov rdx, [rax + 90h]
    mov [r11 + HostVmcbPa], rdx

    mov rdx, 578h
    lea r8, [hv_asm_svm_guest_entry]
    mov [r10 + rdx], r8

    mov rdx, 5D8h
    mov [r10 + rdx], rsp

    pushfq
    pop rdx
    or rdx, 2
    mov r8, 570h
    mov [r10 + r8], rdx

    mov rsp, r11
    mov rax, [rsp + GuestVmcbPa]

svm_run_loop:
    vmload rax
    vmrun rax
    vmsave rax

    mov rax, [rsp + HostVmcbPa]
    vmload rax

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
    mov  rdx, rsp
    mov  rcx, [rsp + 28h + 78h + VcpuPtr]
    call hv_svm_exit_handler
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
    jnz  svm_unload
    mov  rax, [rsp + GuestVmcbPa]
    jmp  svm_run_loop

svm_unload:
    mov  rcx, 0C0000080h
    rdmsr
    and  eax, 0FFFFEFFFh
    wrmsr

    mov  rcx, [rsp + VcpuPtr]
    mov  r10, [rcx + 78h]
    mov  rsp, [r10 + 5D8h]
    mov  r10, [r10 + 578h]
    jmp  r10
hv_asm_svm_launch ENDP

hv_asm_svm_guest_entry PROC
    mov eax, 1
    ret
hv_asm_svm_guest_entry ENDP

hv_asm_svm_teardown PROC
    mov  r10, 4F525448h
    mov  r11, 766D6D21h
    mov  r12, 67617465h
    mov  rax, 1
    vmmcall
    ret
hv_asm_svm_teardown ENDP

end
