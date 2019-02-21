org 0x7C00						; BIOS loads us here (0000:7C00)
bits 16							; 16-bit real mode code

jmp short code_start			; Jump to the start of the code

times 64-($-$$) db 0x00			; Pad some space for the echidnaFS header

; Start of main bootloader code

code_start:

cli
jmp 0x0000:initialise_cs		; Initialise CS to 0x0000 with a long jump
initialise_cs:
xor ax, ax
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax
mov ss, ax
mov sp, 0x7BF0
sti

mov byte [drive_number], dl		; Save boot drive in memory

mov si, LoadingMsg				; Print loading message using simple print (BIOS)
call simple_print

; ****************** Load stage 2 ******************

mov si, Stage2Msg				; Print loading stage 2 message
call simple_print

mov ax, 1						; Start from LBA sector 1
mov ebx, 0x7E00					; Load to offset 0x7E00
mov cx, 7						; Load 7 sectors
call read_sectors

jc err							; Catch any error

mov si, DoneMsg
call simple_print				; Display done message

jmp 0x7E00						; Jump to stage 2

err:
mov si, ErrMsg
call simple_print

halt:
hlt
jmp halt

;Data

LoadingMsg		db 0x0D, 0x0A, 'Loading qword...', 0x0D, 0x0A, 0x0A, 0x00
Stage2Msg		db 'Loading Stage 2...', 0x00
ErrMsg			db 0x0D, 0x0A, 'Error, system halted.', 0x00
DoneMsg			db '  DONE', 0x0D, 0x0A, 0x00

;Includes

%include 'includes/simple_print.inc'
%include 'includes/disk.inc'

drive_number				db 0x00				; Drive number

times 510-($-$$)			db 0x00				; Fill rest with 0x00
bios_signature				dw 0xAA55			; BIOS signature

; ************************* STAGE 2 ************************

; ***** A20 *****

mov si, A20Msg					; Display A20 message
call simple_print

call enable_a20					; Enable the A20 address line to access the full memory
jc err							; If it fails, print an error and halt

mov si, DoneMsg
call simple_print				; Display done message

; ***** Unreal Mode *****

mov si, UnrealMsg				; Display unreal message
call simple_print

lgdt [GDT]						; Load the GDT

; enter unreal mode

cli						; Disable interrupts

mov eax, cr0			; Enable bit 0 of cr0 and enter protected mode
or eax, 00000001b
mov cr0, eax

jmp 0x08:.pmodeu

.pmodeu:					; Now in protected mode

mov ax, 0x10
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax
mov ss, ax

mov eax, cr0			; Exit protected mode
and eax, 11111110b
mov cr0, eax

jmp 0x0000:.unreal_mode

.unreal_mode:			; Now in Unreal Mode

xor ax, ax
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax
mov ss, ax
mov sp, 0x7BF0

sti						; Enable interrupts

mov si, DoneMsg
call simple_print				; Display done message

; ***** Kernel *****

; Load the kernel to 0x100000 (1 MiB)

mov si, KernelMsg				; Show loading kernel message
call simple_print

mov esi, kernel_name
mov ebx, 0x100000				; Load to offset 0x100000
call load_file

jc err							; Catch any error

mov si, DoneMsg
call simple_print				; Display done message

; enter pmode

cli						; Disable interrupts

mov eax, cr0			; enter pmode
or eax, 0x00000001
mov cr0, eax

jmp 0x18:.pmode

bits 32

.pmode:					; Now in protected mode

mov ax, 0x20
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax
mov ss, ax

; *** Setup registers ***

mov esp, 0xEFFFF0

xor eax, eax
xor ecx, ecx
xor edx, edx
xor esi, esi
xor edi, edi
xor ebp, ebp

mov ebx, kernel_cmdline
jmp 0x100000					; Jump to the newly loaded kernel

;Data

kernel_name		db 'kernel.bin', 0x00
kernel_cmdline  db 'display=vbe edid=enabled root=/dev/sata0 rootfs=echfs init=/bin/bash', 0x00

A20Msg			db 'Enabling A20 line...', 0x00
UnrealMsg		db 'Entering Unreal Mode...', 0x00
KernelMsg		db 'Loading kernel...', 0x00

;Includes

bits 16
%include 'includes/echfs.inc'
%include 'includes/disk2.inc'
%include 'includes/a20_enabler.inc'
%include 'includes/gdt.inc'

times 4096-($-$$)			db 0x00				; Padding
