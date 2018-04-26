extern real_routine

global get_vbe_info
global get_edid_info
global get_vbe_mode_info
global set_vbe_mode
global dump_vga_font

%define kernel_phys_offset 0xffffffffc0000000

section .data

%define get_vbe_info_size           get_vbe_info_end - get_vbe_info_bin
get_vbe_info_bin:                   incbin "real/get_vbe_info.bin"
get_vbe_info_end:

%define get_edid_info_size           get_edid_info_end - get_edid_info_bin
get_edid_info_bin:                   incbin "real/get_edid_info.bin"
get_edid_info_end:

%define get_vbe_mode_info_size           get_vbe_mode_info_end - get_vbe_mode_info_bin
get_vbe_mode_info_bin:                   incbin "real/get_vbe_mode_info.bin"
get_vbe_mode_info_end:

%define set_vbe_mode_size           set_vbe_mode_end - set_vbe_mode_bin
set_vbe_mode_bin:                   incbin "real/set_vbe_mode.bin"
set_vbe_mode_end:

%define dump_vga_font_size           dump_vga_font_end - dump_vga_font_bin
dump_vga_font_bin:                   incbin "real/dump_vga_font.bin"
dump_vga_font_end:

section .text

bits 64

get_vbe_info:
    ; void get_vbe_info(vbe_info_struct_t* vbe_info_struct);
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov rbx, rdi
    sub rbx, kernel_phys_offset
    mov rsi, get_vbe_info_bin
    mov rcx, get_vbe_info_size
    call real_routine

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

get_edid_info:
    ; void get_edid_info(edid_info_struct_t* edid_info_struct);
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov rbx, rdi
    sub rbx, kernel_phys_offset
    mov rsi, get_edid_info_bin
    mov rcx, get_edid_info_size
    call real_routine

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

get_vbe_mode_info:
    ; void get_vbe_mode_info(get_vbe_t* get_vbe);
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov rbx, rdi
    sub rbx, kernel_phys_offset
    mov rsi, get_vbe_mode_info_bin
    mov rcx, get_vbe_mode_info_size
    call real_routine

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

set_vbe_mode:
    ; void set_vbe_mode(uint16_t mode);
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov rbx, rdi
    mov rsi, set_vbe_mode_bin
    mov rcx, set_vbe_mode_size
    call real_routine

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

dump_vga_font:
    ; void dump_vga_font(uint8_t *bitmap);
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov rbx, rdi
    sub rbx, kernel_phys_offset
    mov rsi, dump_vga_font_bin
    mov rcx, dump_vga_font_size
    call real_routine

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
