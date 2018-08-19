BYTES_PER_BLOCK equ 32768

load_file:

; **********************************************************************
;     Loads a file from an echidnaFS formatted drive (root dir only)
; **********************************************************************

; IN:
; es:ebx	-->		Target segment:offset
; esi		-->		Filename
; dl		-->		Drive number

push eax
push ebx
push ecx
push edx
push esi
push edi

mov byte [.current_drive], dl
mov dword [.target_buffer], ebx
mov dword [.buffer_pushing], 0

mov eax, 12
call disk_read_dword
mov ebx, 8
mul ebx
mov ebx, BYTES_PER_BLOCK
xor edx, edx
div ebx
test edx, edx
jz .noincr
inc eax
.noincr:
mov ebx, BYTES_PER_BLOCK
mul ebx
add eax, (16 * BYTES_PER_BLOCK) ; fat start
mov dword [.directory_start], eax

.entry_test:
push eax
mov dl, byte [.current_drive]
call disk_read_dword
test eax, eax
jz .not_found
cmp eax, 0xffffffff
jne .next_entry
pop eax
push eax
add eax, 8
call disk_read_byte
cmp al, 0x00
jne .next_entry
pop eax
push eax
add eax, 9
call disk_cmp_strings
jnc .next_entry
pop eax

; entry found
add eax, 240
call disk_read_dword
mov dword [.cur_block], eax
jmp .load_chain

.next_entry:
pop eax
add eax, 256
jmp .entry_test

.load_chain:
; load block
mov eax, dword [.cur_block]
mov ebx, BYTES_PER_BLOCK / 512
mul ebx
mov ebx, dword [.target_buffer]
add ebx, dword [.buffer_pushing]
mov dl, byte [.current_drive]
mov ecx, BYTES_PER_BLOCK / 512
call read_sectors
; print '.'
pusha
mov ah, 0x0e
mov al, '.'
int 0x10
popa
; fetch next block
mov eax, dword [.cur_block]
mov ebx, 8
mul ebx
add eax, (16 * BYTES_PER_BLOCK) ; fat start
mov dl, byte [.current_drive]
call disk_read_dword
cmp eax, 0xffffffff
je .success
mov dword [.cur_block], eax
add dword [.buffer_pushing], BYTES_PER_BLOCK
jmp .load_chain

.not_found:
pop eax
stc
jmp .done

.success:
clc

.done:
pop edi
pop esi
pop edx
pop ecx
pop ebx
pop eax
ret

.buffer_pushing         dd 0
.target_buffer          dd 0
.directory_start        dd 0
.cur_block              dd 0
.current_drive          db 0
