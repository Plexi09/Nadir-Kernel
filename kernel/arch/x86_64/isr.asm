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
global irq_stub_table
extern exception_handler
extern irq_dispatch

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

; ---------------------------------------------------------------------------
; Nadir IRQ stubs: vectors 32-47 (remapped 8259 IRQs 0-15) -> irq_dispatch.
;
; Same Ring 0 / CPL 0 entry context as the exception stubs above: the CPU
; entered with IF cleared (interrupt gate), no privilege change, so only
; RIP/CS/RFLAGS were pushed. IRQs never carry a CPU error code, so every
; stub normalizes with a dummy 0 exactly like ISR_NOERR, keeping the
; [vector, error] layout — and thus struct interrupt_frame — identical to
; the exception path. Field order must stay in sync with include/idt.h.
; The EOI is sent by the C dispatcher (pic_eoi), not here, so a future
; per-IRQ EOI policy only touches C. -mno-red-zone keeps pushes safe.

section .text
%macro IRQ_NOERR 1
global isr_stub_%1
isr_stub_%1:
    push 0
    push %1
    jmp irq_common
%endmacro

IRQ_NOERR 32
IRQ_NOERR 33
IRQ_NOERR 34
IRQ_NOERR 35
IRQ_NOERR 36
IRQ_NOERR 37
IRQ_NOERR 38
IRQ_NOERR 39
IRQ_NOERR 40
IRQ_NOERR 41
IRQ_NOERR 42
IRQ_NOERR 43
IRQ_NOERR 44
IRQ_NOERR 45
IRQ_NOERR 46
IRQ_NOERR 47

; Common IRQ entry: save GPRs, call void irq_dispatch(frame in RDI),
; restore GPRs, drop [vector, error], return to the interrupted code.
; Unlike isr_common (which halts), this path must iretq: IRQs are
; resumable. Interrupts stay disabled throughout (IF=0 on entry via an
; interrupt gate; irq_dispatch runs with IF cleared; iretq restores the
; caller's RFLAGS).
irq_common:
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
    call irq_dispatch
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
    iretq

section .rodata
align 8
irq_stub_table:
    dq isr_stub_32, isr_stub_33, isr_stub_34, isr_stub_35
    dq isr_stub_36, isr_stub_37, isr_stub_38, isr_stub_39
    dq isr_stub_40, isr_stub_41, isr_stub_42, isr_stub_43
    dq isr_stub_44, isr_stub_45, isr_stub_46, isr_stub_47
