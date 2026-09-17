; Nadir stage-1 boot sector.
;
; Runs in 16-bit real mode at 0x7C00 (Ring 0, CPL 0, BIOS hands us full
; privilege; privileged instructions used: cli, lgdt, mov cr0).
; Fault behavior: none possible here beyond a failed disk read, which prints
; an error and halts. There is no IDT yet, so any CPU exception would
; triple-fault and reset the machine.
;
; Job: enable A20, load the kernel image (LBA 1..N) to 0x10000, install a
; flat GDT, enter 32-bit protected mode and jump to the stage-2 entry.
;
; Memory map used by the whole boot chain:
;   0x007C00  this sector (512 bytes)
;   0x010000  kernel image (loaded here, linked here)
;   0x70000   PML4 | 0x71000 PDPT | 0x72000 PD (built by stage 2)
;   0x90000   stack top (real-mode stack grows down from 0x7C00 instead)
;   0xB8000   VGA text buffer

bits 16
org 0x7C00

%ifndef KERNEL_SECTORS
%error "KERNEL_SECTORS not defined - build with make"
%endif

KERNEL_LOAD_SEG equ 0x1000          ; ES for load target 0x1000:0x0000 = 0x10000
STACK_TOP       equ 0x7C00          ; real-mode stack grows down from here

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, STACK_TOP
    sti
    mov [boot_drive], dl            ; BIOS passes the boot drive in DL

    mov si, msg_loading
    call puts

    call enable_a20
    call read_drive_params
    call load_kernel

    mov si, msg_entering
    call puts

    cli
    lgdt [gdt_ptr]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:prot32

; ---------------------------------------------------------------------------
; puts: print NUL-terminated string at DS:SI via BIOS teletype.
; Clobbers AX, BX, SI.
puts:
    push ax
    push bx
    mov ah, 0x0E
    mov bh, 0
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; enable_a20: try BIOS, keyboard-controller and fast-A20 methods in turn.
; Each is harmless if A20 is already on.
enable_a20:
    mov ax, 0x2401                  ; BIOS: enable A20
    int 0x15
    call kbd_wait                   ; keyboard controller: pulse output port
    mov al, 0xD1
    out 0x64, al
    call kbd_wait
    mov al, 0xDF
    out 0x60, al
    call kbd_wait
    in al, 0x92                     ; fast A20 (skip if already set or absent)
    test al, 0x02
    jnz .done
    or al, 0x02
    out 0x92, al
.done:
    ret

kbd_wait:
    in al, 0x64
    test al, 0x02
    jnz kbd_wait
    ret

; ---------------------------------------------------------------------------
; read_drive_params: query sectors/track and head count via int 0x13 AH=08.
; Falls back to floppy defaults (18 spt, 2 heads) on failure or nonsense.
read_drive_params:
    xor ax, ax
    mov ds, ax                      ; re-establish DS (BIOS may clobber it)
    mov ah, 0x08
    mov dl, [boot_drive]
    int 0x13
    jc .defaults
    and cx, 0x3F                    ; CL[5:0] = max sector = sectors per track
    jz .defaults
    mov [spt], cl
    inc dh                          ; DH = max head -> head count
    jz .defaults
    mov [heads], dh
    ret
.defaults:
    mov byte [spt], 18
    mov byte [heads], 2
    ret

; ---------------------------------------------------------------------------
; lba_to_chs: convert LBA in AX to int 0x13 CHS in CH/CL/DH.
; Clobbers AX, BX, CX, DX.
lba_to_chs:
    xor dx, dx
    div word [spt]                  ; AX = LBA/spt, DX = sector offset
    inc dx                          ; sectors are 1-based
    push dx                         ; save sector number
    xor dx, dx
    div word [heads]                ; AX = cylinder, DX = head
    mov dh, dl
    mov bx, ax                      ; BX = cylinder
    pop ax                          ; AL = sector
    mov cl, al
    mov ch, bl                      ; CH = cylinder bits 0-7
    shr bx, 2
    and bl, 0xC0
    or cl, bl                       ; CL[7:6] = cylinder bits 8-9
    ret

; ---------------------------------------------------------------------------
; load_kernel: read KERNEL_SECTORS sectors starting at LBA 1 to 0x10000.
; Halts with an error message after 3 failed retries per sector.
;
; NOTE: no register value is trusted across int 0x13 — SeaBIOS/real BIOSes
; may clobber general registers (observed: BX zeroed) and disturb the
; stack, so all loop state lives in memory and ES:BX/DS/DL are reloaded
; from memory before every disk call.
load_kernel:
    xor ax, ax
    mov ds, ax                      ; re-establish DS (BIOS may clobber it)
    mov ax, KERNEL_LOAD_SEG
    mov [buf_seg], ax
    mov word [buf_off], 0
    mov word [lba], 1
    mov ax, KERNEL_SECTORS
    mov [sectors_left], ax
.next:
    mov ax, [lba]
    call lba_to_chs
    mov byte [tries], 3
.attempt:
    mov ax, [buf_seg]               ; reload buffer: ES:BX must not survive
    mov es, ax                      ; a BIOS call
    mov bx, [buf_off]
    mov dl, [boot_drive]
    mov ah, 0x02                    ; read 1 sector to ES:BX
    mov al, 1
    int 0x13
    xor ax, ax
    mov ds, ax                      ; DS may not have survived either
    jnc .ok
    mov ah, 0x00                    ; reset drive, then retry
    mov dl, [boot_drive]
    int 0x13
    xor ax, ax
    mov ds, ax
    dec byte [tries]
    jnz .attempt
    mov si, msg_disk_err
    call puts
    jmp halt
.ok:
    add word [buf_off], 512         ; advance buffer past the sector
    jnc .no_carry
    add word [buf_seg], 0x1000      ; +64K on segment wrap
.no_carry:
    inc word [lba]
    dec word [sectors_left]
    jnz .next
    ret

halt:
    cli
.hang:
    hlt
    jmp .hang

; ---------------------------------------------------------------------------
bits 32
prot32:
    mov ax, 0x10                    ; flat data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    mov eax, 0x10000                ; stage-2 entry at start of kernel image
    jmp eax

; ---------------------------------------------------------------------------
bits 16

; Flat GDT: null, 32-bit code, 32-bit data. (64-bit descriptors live in the
; stage-2 image, which brings its own GDT.)
gdt:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF           ; code32: base 0, 4GB, exec/read
    dq 0x00CF92000000FFFF           ; data32: base 0, 4GB, read/write
gdt_end:
gdt_ptr:
    dw gdt_end - gdt - 1
    dd gdt

msg_loading db "Nadir: loading kernel...", 0x0D, 0x0A, 0
msg_entering db "Nadir: entering protected mode", 0x0D, 0x0A, 0
msg_disk_err db "Nadir: disk read error", 0x0D, 0x0A, 0

boot_drive   db 0
spt          dw 18
heads        dw 2
lba          dw 0
buf_seg      dw 0                    ; load buffer segment (starts 0x1000)
buf_off      dw 0                    ; load buffer offset within segment
sectors_left dw 0                    ; sectors still to load
tries        db 0                    ; retries left for current sector

times 510-($-$$) db 0
dw 0xAA55
