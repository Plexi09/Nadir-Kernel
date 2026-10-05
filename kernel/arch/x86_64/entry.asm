; Nadir stage-2 entry: 32-bit protected mode -> 64-bit long mode -> C.
;
; Entered via a near jump to 0x10000 with a flat code segment (Ring 0,
; CPL 0 —still the privilege BIOS gave us; no userspace exists yet).
; Privileged instructions used: cpuid, mov cr4/cr0, rdmsr/wrmsr, lgdt.
; Fault behavior: no IDT is installed, so any exception here triple-faults
; and resets the machine. The only graceful failure is a CPU without long
; mode, which prints an error straight to VGA and halts.
;
; Steps: verify long mode exists, identity-map the first 1GB with 2MB
; pages, enable PAE + LME + paging, reload a GDT with a 64-bit code
; segment, zero BSS, copy the BIOS memory map staged by stage 1 into
; kernel .bss, then call the C entry point kmain (System V ABI).
; Interrupts stay disabled for the whole sequence.
;
; WHY copy after zeroing: the map globals live in .bss, so zeroing first
; then copying keeps the staged entries. Zeroing after would wipe them;
; copying before is impossible (BSS addresses are only valid once paging
; and the zeroed state the C code expects are both in place).

bits 32
section .boot32
global boot32_entry
extern kmain
extern __bss_start
extern __bss_end
extern memmap_count
extern memmap_entries

PML4        equ 0x70000
PDPT        equ 0x71000
PD          equ 0x72000
STACK64_TOP equ 0x90000
MEMMAP_COUNT_PHYS equ 0x5000        ; dword staged by stage 1
MEMMAP_BUF_PHYS   equ 0x5020        ; 24-byte E820 entries staged by stage 1
E820_MAX          equ 64
E820_ENTRY_SIZE   equ 24

boot32_entry:
    ; --- CPUID: does this CPU do 64-bit long mode? ---
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_longmode
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29               ; LM bit
    jz .no_longmode

    ; --- page tables: PML4 -> PDPT -> PD, 512 x 2MB identity pages ---
    mov edi, PML4
    xor eax, eax
    mov ecx, 0x1000                 ; clear 16KB (tables + slack)
    rep stosd
    mov eax, PDPT
    or eax, 0x03                    ; present + writable
    mov [PML4], eax
    mov eax, PD
    or eax, 0x03
    mov [PDPT], eax
    mov ecx, 512
    mov edi, PD
    mov eax, 0x83                   ; present + writable + 2MB page
.fill_pd:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .fill_pd

    ; --- enable long mode ---
    mov eax, PML4                   ; page-walk root (CR3 was 0 until now)
    mov cr3, eax
    mov eax, cr4
    or eax, 1 << 5                  ; PAE
    mov cr4, eax
    mov ecx, 0xC0000080             ; IA32_EFER
    rdmsr
    or eax, 1 << 8                  ; LME
    wrmsr
    mov eax, cr0
    or eax, 1 << 31                 ; PG: paging on, long mode active
    mov cr0, eax

    lgdt [gdt64_ptr]
    jmp 0x18:long_entry             ; far jump loads the 64-bit code segment

.no_longmode:
    mov edi, 0xB8000                ; VGA text buffer, line 0
    mov esi, err_nolm
    mov ah, 0x4F                    ; white on red
.print:
    lodsb
    test al, al
    jz .die
    stosw
    jmp .print
.die:
    cli
.hang:
    hlt
    jmp .hang

err_nolm db "ERR: CPU has no 64-bit long mode", 0

; Own GDT: the stage-1 GDT is behind us, so bring null/code32/data32 plus
; the 64-bit code+data segments this stage needs.
align 8
gdt64:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF           ; code32 (kept for completeness)
    dq 0x00CF92000000FFFF           ; data32
    dq 0x00209A0000000000           ; code64: L=1, exec/read
    dq 0x0000920000000000           ; data64: read/write
gdt64_end:
gdt64_ptr:
    dw gdt64_end - gdt64 - 1
    dd gdt64

; ---------------------------------------------------------------------------
bits 64
long_entry:
    mov ax, 0x20                    ; 64-bit data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, STACK64_TOP

    mov rdi, __bss_start            ; zero BSS (uninitialized C statics)
    mov rcx, __bss_end
    sub rcx, rdi
    xor eax, eax
    rep stosb

    ; --- copy staged E820 map into .bss globals (identity map covers
    ; both 0x5000 and .bss, and paging is already on) ---
    ; WHY byte + clamp: stage 1 stages a single count byte (64 max fits);
    ; the clamp is belt-and-braces against a corrupt staging byte.
    movzx eax, byte [abs MEMMAP_COUNT_PHYS]
    cmp eax, E820_MAX
    jbe .map_count_ok
    mov eax, E820_MAX
.map_count_ok:
    mov [rel memmap_count], eax
    test eax, eax
    jz .map_done
    mov rsi, MEMMAP_BUF_PHYS
    lea rdi, [rel memmap_entries]
    mov ecx, eax
    imul ecx, ecx, E820_ENTRY_SIZE  ; bytes = count * 24
    rep movsb
.map_done:

    call kmain                      ; first C code — does not return
    cli
.hang:
    hlt
    jmp .hang
