extern real_routine

global bios_get_time

%define kernel_phys_offset 0xffffffffc0000000

section .data

%define get_time_size           get_time_end - get_time_bin
get_time_bin:                   incbin "real/get_time.bin"
get_time_end:

bcd_time:
    .seconds db 0
    .minutes db 0
    .hours db 0
    .days db 0
    .months db 0
    .years db 0
    .centuries db 0

section .text

bcd_to_int:
    ; in: RBX = address of byte to convert
    push rax
    push rcx

    mov al, byte [rbx]      ; seconds
    and al, 00001111b
    mov cl, al
    mov al, byte [rbx]
    and al, 11110000b
    shr al, 4
    mov ch, 10
    mul ch
    add cl, al

    mov byte [rbx], cl

    pop rcx
    pop rax
    ret

bios_get_time:
    ; void get_time(struct s_time_t *s_time);
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    push rdi

    mov rbx, bcd_time
    mov rdi, kernel_phys_offset
    sub rbx, rdi
    mov rsi, get_time_bin
    mov rcx, get_time_size
    call real_routine

    pop rdi

    xor eax, eax
    mov rsi, bcd_time

    mov rbx, bcd_time.seconds
    call bcd_to_int
    lodsb
    stosd

    mov rbx, bcd_time.minutes
    call bcd_to_int
    lodsb
    stosd

    mov rbx, bcd_time.hours
    call bcd_to_int
    lodsb
    stosd

    mov rbx, bcd_time.days
    call bcd_to_int
    lodsb
    stosd

    mov rbx, bcd_time.months
    call bcd_to_int
    lodsb
    stosd

    mov rbx, bcd_time.years
    call bcd_to_int
    lodsb
    mov dword [rdi], eax

    mov rbx, bcd_time.centuries
    call bcd_to_int
    lodsb
    mov cx, 100
    mul cx
    add dword [rdi], eax

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
