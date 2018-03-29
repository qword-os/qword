global gdt_ptr

section .data

align 16

gdt_ptr:
dw .gdt_end - .gdt_start - 1  ; GDT size
dd .gdt_start                 ; GDT start

align 16

.gdt_start:

; Null descriptor (required)

.NullDescriptor:

dw 0x0000           ; Limit
dw 0x0000           ; Base (low 16 bits)
db 0x00             ; Base (mid 8 bits)
db 00000000b        ; Access
db 00000000b        ; Granularity
db 0x00             ; Base (high 8 bits)

; 64 bit mode kernel

.KernelCode64:

dw 0x0000           ; Limit
dw 0x0000           ; Base (low 16 bits)
db 0x00             ; Base (mid 8 bits)
db 10011010b        ; Access
db 00100000b        ; Granularity
db 0x00             ; Base (high 8 bits)

.KernelData64:

dw 0x0000           ; Limit
dw 0x0000           ; Base (low 16 bits)
db 0x00             ; Base (mid 8 bits)
db 10010010b        ; Access
db 00000000b        ; Granularity
db 0x00             ; Base (high 8 bits)

; 64 bit mode user

.UserCode64:

dw 0x0000           ; Limit
dw 0x0000           ; Base (low 16 bits)
db 0x00             ; Base (mid 8 bits)
db 11111010b        ; Access
db 00100000b        ; Granularity
db 0x00             ; Base (high 8 bits)

.UserData64:

dw 0x0000           ; Limit
dw 0x0000           ; Base (low 16 bits)
db 0x00             ; Base (mid 8 bits)
db 11110010b        ; Access
db 00000000b        ; Granularity
db 0x00             ; Base (high 8 bits)

; Unreal mode

.UnrealCode:

dw 0xFFFF           ; Limit
dw 0x0000           ; Base (low 16 bits)
db 0x00             ; Base (mid 8 bits)
db 10011010b        ; Access
db 10001111b        ; Granularity
db 0x00             ; Base (high 8 bits)

.UnrealData:

dw 0xFFFF           ; Limit
dw 0x0000           ; Base (low 16 bits)
db 0x00             ; Base (mid 8 bits)
db 10010010b        ; Access
db 10001111b        ; Granularity
db 0x00             ; Base (high 8 bits)

.gdt_end:
