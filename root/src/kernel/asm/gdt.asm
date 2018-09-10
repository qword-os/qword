bits 64

global gdt_ptr
global gdt_ptr_lowerhalf
global load_tss

%define kernel_phys_offset 0xffffffffc0000000

section .data

align 16

gdt_ptr_lowerhalf:
    dw gdt_ptr.gdt_end - gdt_ptr.gdt_start - 1  ; GDT size
    dd gdt_ptr.gdt_start - kernel_phys_offset  ; GDT start

align 16

gdt_ptr:
    dw .gdt_end - .gdt_start - 1  ; GDT size
    dq .gdt_start                 ; GDT start

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

; 64 bit mode kernel
.kernel_code_64:
    dw 0x0000           ; Limit
    dw 0x0000           ; Base (low 16 bits)
    db 0x00             ; Base (mid 8 bits)
    db 10011010b        ; Access
    db 00100000b        ; Granularity
    db 0x00             ; Base (high 8 bits)

.kernel_data:
    dw 0x0000           ; Limit
    dw 0x0000           ; Base (low 16 bits)
    db 0x00             ; Base (mid 8 bits)
    db 10010010b        ; Access
    db 00000000b        ; Granularity
    db 0x00             ; Base (high 8 bits)

; 64 bit mode user code
.user_data_64:
    dw 0x0000           ; Limit
    dw 0x0000           ; Base (low 16 bits)
    db 0x00             ; Base (mid 8 bits)
    db 11110010b        ; Access
    db 00000000b        ; Granularity
    db 0x00             ; Base (high 8 bits)
.user_code_64:
    dw 0x0000           ; Limit
    dw 0x0000           ; Base (low 16 bits)
    db 0x00             ; Base (mid 8 bits)
    db 11111010b        ; Access
    db 00100000b        ; Granularity
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
  .tss_upper32:
    dd 0
  .tss_reserved:
    dd 0

.gdt_end:

load_tss:
    ; addr in RDI
    push rbx
    mov eax, edi
    mov rbx, gdt_ptr.tss_low
    mov word [rbx], ax
    mov eax, edi
    and eax, 0xff0000
    shr eax, 16
    mov rbx, gdt_ptr.tss_mid
    mov byte [rbx], al
    mov eax, edi
    and eax, 0xff000000
    shr eax, 24
    mov rbx, gdt_ptr.tss_high
    mov byte [rbx], al
    mov rax, rdi
    shr rax, 32
    mov rbx, gdt_ptr.tss_upper32
    mov dword [rbx], eax
    mov rbx, gdt_ptr.tss_flags1
    mov byte [rbx], 10001001b
    mov rbx, gdt_ptr.tss_flags2
    mov byte [rbx], 0
    pop rbx
    ret
