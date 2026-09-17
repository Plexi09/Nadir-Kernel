bits 16 ; 16 bit mode
org 0x7c00

boot:
    mov si, message     ; Point SI register to message
    mov ah, 0x0e        ; Set higher bits to the display character command

.loop:
    lodsb               ; Load the character within the AL register, and increment SI
    cmp al, 0           ; Is the AL register a null byte?
    je halt             ; Jump to halt
    int 0x10            ; Trigger video service interrupt
    jmp .loop           ; Loop again

halt:
    hlt                 ; stop

message:
    db "Hello, World ! From Nadir Kernel", 0x0a, 0x0d, 0x00

times 510-($-$$) db 0   ; 510 bytes of zeros
dw 0xAA55               ; boot signature