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
;   0x005000  E820 staging: count byte at 0x5000, 24-byte entries from
;             0x5020 (64 max = 1536 bytes, ends 0x5620). Free: below the
;             0x7C00 sector, clear of the 0x10000 kernel load, the 0x70000
;             tables and the 0x90000 stack. Stage 2 copies it into kernel
;             .bss (memmap_count/memmap_entries) after BSS zeroing.
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
E820_COUNT      equ 0x5000          ; staged E820 entry count (one byte)
E820_BUF        equ 0x5020          ; staged E820 entries (24 bytes each)
E820_MAX        equ 64              ; cap: 64 * 24 = 1536 bytes, ends 0x5620
E820_ENTRY_SIZE equ 24

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
    call load_kernel
    call query_e820

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
; enable_a20: try the BIOS and fast-A20 methods in turn. Each is harmless
; if A20 is already on.
; WHY only two methods: the keyboard-controller pulse was dropped to free
; sector bytes for the E820 query above — every 64-bit CPU postdates
; chipsets with the 0x92 fast gate (or a BIOS that honors 0x2401), so the
; legacy AT pulse buys nothing on this kernel's hardware floor.
enable_a20:
    mov ax, 0x2401                  ; BIOS: enable A20
    int 0x15
    in al, 0x92                     ; fast A20 (skip if already set or absent)
    test al, 0x02
    jnz .done
    or al, 0x02
    out 0x92, al
.done:
    ret

; ---------------------------------------------------------------------------
; Floppy geometry is fixed at 18 sectors/track, 2 heads (the image only
; ever boots via -fda; see [spt]/[heads] inits below). The old int 0x13
; AH=08 probe was dropped to free sector bytes for the E820 query above.
; HDD generality wants EDD packet reads (AH=42), planned, not CHS probing.
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

; ---------------------------------------------------------------------------
; query_e820: ask BIOS for the physical memory map (int 0x15, eax=0xE820,
; edx='SMAP') and stage it at E820_BUF with the entry count byte at
; E820_COUNT. Must run BEFORE the protected-mode switch as BIOS calls are
; unavailable after. Runs in 16-bit real mode at Ring 0; clobbers AX,
; BX, CX, DX, DI, ES.
;
; NOTE: like load_kernel, no register survives int 0x15 — SeaBIOS/real
; BIOSes may clobber general registers across the call, so the
; continuation (EBX) lives in memory ([e820_next]) and ECX/EDX/ES:DI are
; reloaded from immediates before every call. DS is saved on the stack
; across the call (push/pop restores OUR value even if BIOS clobbers it);
; it must NOT be rebuilt with xor ax,ax + mov ds,ax after the call, as
; that zeroes EAX's low half and breaks the 'SMAP' check below. DI is recomputed from
; the in-memory count each pass (index < 64, so the 8-bit multiply
; cannot overflow) instead of keeping a second pointer.
;
; Fallback choice: if no entry was staged (first query failed via carry
; or EAX magic mismatch), synthesize a single usable region at 1MB,
; length 127MB (type 1) — enough for the kernel + PMM to boot on an
; unknown box — and continue silently (no message: the sector has no
; bytes to spare). A mid-list failure keeps whatever entries were
; already staged. Documented here so the PMM knows the map may be
; synthetic on old hardware. Kept golfed on purpose: this sector is
; exactly 510 bytes, every instruction here displaced a boot message.
query_e820:
    xor ax, ax
    mov ds, ax                      ; re-establish DS
    mov [E820_COUNT], al            ; count byte = 0
.loop:
    xor ax, ax
    mov es, ax                      ; ES:DI rebuilt per call
    mov al, [E820_COUNT]
    mov cl, E820_ENTRY_SIZE
    mul cl                          ; AX = index * 24
    mov di, E820_BUF
    add di, ax
    mov ecx, E820_ENTRY_SIZE        ; reload every call: BIOS owns nothing
    mov edx, 0x534D4150 
    mov eax, 0xE820
    mov ebx, [e820_next]            ; continuation lives in memory
    push ds                         ; DS survives on the stack, EAX intact
    int 0x15
    pop ds                          ; our DS back, whatever BIOS did
    jc .done                        ; carry = end of list or call failed
    cmp eax, 0x534D4150             ; BIOS must echo 'SMAP' in EAX
    jne .done
    cmp byte [E820_COUNT], E820_MAX ; buffer full: stop, keep what we have
    jae .done
    inc byte [E820_COUNT]           ; entry already written by BIOS at ES:DI
    mov [e820_next], ebx            ; save continuation across the next call
    test ebx, ebx                   ; EBX=0 ends the list
    jnz .loop
.done:
    cmp byte [E820_COUNT], 0
    jne .ok
    xor ax, ax                      ; fallback: ES:DI may not have survived
    mov es, ax
    mov di, E820_BUF
    mov eax, 0x00100000
    stosd                           ; base = 1MB
    xor eax, eax
    stosd                           ; base high = 0
    mov eax, 0x07F00000
    stosd                           ; length = 127MB
    xor eax, eax
    stosd                           ; length high = 0
    inc eax
    stosd                           ; type = 1 (usable)
    inc byte [E820_COUNT]           ; count was 0, now 1
.ok:
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

msg_loading db "Nadir: load", 0x0D, 0x0A, 0
msg_entering db "Nadir: pmode", 0x0D, 0x0A, 0
msg_disk_err db "Nadir: disk err", 0x0D, 0x0A, 0

boot_drive   db 0
spt          dw 18
heads        dw 2
lba          dw 0
buf_seg      dw 0                    ; load buffer segment (starts 0x1000)
buf_off      dw 0                    ; load buffer offset within segment
sectors_left dw 0                    ; sectors still to load
tries        db 0                    ; retries left for current sector
e820_next    dd 0                    ; E820 continuation across int 0x15
                                    ; (image-zeroed; first call starts at 0)

times 510-($-$$) db 0
dw 0xAA55
