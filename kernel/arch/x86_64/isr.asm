; Nadir exception stubs: vectors 0-31 -> exception_handler.
;
; Runs in 64-bit long mode at Ring 0 (CPL 0). Entered by the CPU with
; IF cleared (interrupt gate); no privilege change, so the CPU pushed
; RIP/CS/RFLAGS plus an error code only on vectors 8, 10-14, 17, 21.
; Fault behavior: a fault here (before the IDT is loaded, or inside the
; handler) triple-faults. Never returns: the C handler halts.
;
; Contract with include/idt.h `struct interrupt_frame`: each stub leaves
; [vector, error] on the stack (pushing a dummy 0 where the CPU did not),
; then isr_common pushes the 15 GPRs and calls the C handler with RDI
; pointing at the frame. Field order must stay in sync. Interrupts stay
; disabled; -mno-red-zone makes the pushes safe.

bits 64
section .text

global isr_stub_table
extern exception_handler

; Vectors without a CPU error code: normalize with a dummy 0.
%macro ISR_NOERR 1
global isr_stub_%1
isr_stub_%1:
    push 0
    push %1
    jmp isr_common
%endmacro

; Vectors where the CPU already pushed an error code: only push the vector.
%macro ISR_ERR 1
global isr_stub_%1
isr_stub_%1:
    push %1
    jmp isr_common
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; Common entry: save GPRs, call void exception_handler(frame in RDI), halt
; if it ever returns (it must not).
isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp
    call exception_handler
    ; Unreachable in practice; park the CPU if the handler returns.
    cli
.hang:
    hlt
    jmp .hang

section .rodata
align 8
isr_stub_table:
    dq isr_stub_0,  isr_stub_1,  isr_stub_2,  isr_stub_3
    dq isr_stub_4,  isr_stub_5,  isr_stub_6,  isr_stub_7
    dq isr_stub_8,  isr_stub_9,  isr_stub_10, isr_stub_11
    dq isr_stub_12, isr_stub_13, isr_stub_14, isr_stub_15
    dq isr_stub_16, isr_stub_17, isr_stub_18, isr_stub_19
    dq isr_stub_20, isr_stub_21, isr_stub_22, isr_stub_23
    dq isr_stub_24, isr_stub_25, isr_stub_26, isr_stub_27
    dq isr_stub_28, isr_stub_29, isr_stub_30, isr_stub_31
