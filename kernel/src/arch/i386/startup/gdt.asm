global gdt_ptr
global gdt_set_fs_base

global load_tss

section .data

align 16

gdt_ptr:
    dw .gdt_end - .gdt_start - 1  ; GDT size
    dd .gdt_start                 ; GDT start

align 16
.gdt_start:

; Null descriptor (required)
.null_descriptor:
    dw 0x0000           ; Limit
    dw 0x0000           ; Base (low 16 bits)
    db 0x00             ; Base (mid 8 bits)
    db 00000000b        ; Access
    db 00000000b        ; Granularity
    db 0x00             ; Base (high 8 bits)

; 32 bit mode kernel
.kernel_code_32:
    dw 0xFFFF           ; Limit
    dw 0x0000           ; Base (low 16 bits)
    db 0x00             ; Base (mid 8 bits)
    db 10011010b        ; Access
    db 11001111b        ; Granularity
    db 0x00             ; Base (high 8 bits)

.kernel_data_32:
    dw 0xFFFF           ; Limit
    dw 0x0000           ; Base (low 16 bits)
    db 0x00             ; Base (mid 8 bits)
    db 10010010b        ; Access
    db 11001111b        ; Granularity
    db 0x00             ; Base (high 8 bits)

; 32 bit mode user
.user_code_32:
    dw 0xFFFF           ; Limit
    dw 0x0000           ; Base (low 16 bits)
    db 0x00             ; Base (mid 8 bits)
    db 11111010b        ; Access
    db 11001111b        ; Granularity
    db 0x00             ; Base (high 8 bits)

.user_data_32:
    dw 0xFFFF           ; Limit
    dw 0x0000           ; Base (low 16 bits)
    db 0x00             ; Base (mid 8 bits)
    db 11110010b        ; Access
    db 11001111b        ; Granularity
    db 0x00             ; Base (high 8 bits)

; Unreal mode
.unreal_code:
    dw 0xFFFF           ; Limit
    dw 0x0000           ; Base (low 16 bits)
    db 0x00             ; Base (mid 8 bits)
    db 10011010b        ; Access
    db 10001111b        ; Granularity
    db 0x00             ; Base (high 8 bits)

.unreal_data:
    dw 0xFFFF           ; Limit
    dw 0x0000           ; Base (low 16 bits)
    db 0x00             ; Base (mid 8 bits)
    db 10010010b        ; Access
    db 10001111b        ; Granularity
    db 0x00             ; Base (high 8 bits)

.fs:
    dw 0xFFFF           ; Limit
  .fs_base_low:
    dw 0x0000           ; Base (low 16 bits)
  .fs_base_mid:
    db 0x00             ; Base (mid 8 bits)
    db 10010010b        ; Access
    db 11001111b        ; Granularity
  .fs_base_high:
    db 0x00             ; Base (high 8 bits)

; tss
.tss:
    dw 104              ; tss length
  .tss_low:
    dw 0
  .tss_mid:
    db 0
  .tss_flags1:
    db 10001001b
  .tss_flags2:
    db 00000000b
  .tss_high:
    db 0

.gdt_end:

section .text

gdt_set_fs_base:
    ; new base in EAX
    mov word [gdt_ptr.fs_base_low], ax
    shr eax, 16
    mov byte [gdt_ptr.fs_base_mid], al
    shr eax, 8
    mov byte [gdt_ptr.fs_base_high], al
    ret

load_tss:
    ; addr in EDI
    mov eax, edi
    mov word [gdt_ptr.tss_low], ax
    mov eax, edi
    and eax, 0xff0000
    shr eax, 16
    mov byte [gdt_ptr.tss_mid], al
    mov eax, edi
    and eax, 0xff000000
    shr eax, 24
    mov byte [gdt_ptr.tss_high], al
    mov byte [gdt_ptr.tss_flags1], 10001001b
    mov byte [gdt_ptr.tss_flags2], 0
    ret
