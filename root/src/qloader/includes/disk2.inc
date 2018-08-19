disk_buffer times 512 db 0
disk_buffer_status db 0
sector_in_buffer dd 0

disk_read_byte:
    ; EAX = byte address
    ; DL = drive
    ; returns AL = byte (trashes EAX)
    
    push ebx
    push ecx
    push edx
    
    push edx
    mov ebx, 512
    xor edx, edx
    div ebx
    mov dword [.offset], edx
    pop edx
    
    cmp dword [sector_in_buffer], eax
    je .dont_load
    .load:
    mov dword [sector_in_buffer], eax
    mov byte [disk_buffer_status], 1
    mov ebx, disk_buffer
    mov ecx, 1
    call read_sectors
    .dont_load:
    cmp byte [disk_buffer_status], 0
    je .load
    
    mov ebx, disk_buffer
    add ebx, dword [.offset]
    xor eax, eax
    mov al, byte [ebx]
    
    pop edx
    pop ecx
    pop ebx
    ret   
    
    .offset dd 0

disk_read_word:
    ; EAX = word address
    ; DL = drive
    ; returns AX = word (trashes EAX)
    
    push eax
    inc eax
    call disk_read_byte
    shl eax, 8
    mov dword [.result], eax
    pop eax
    call disk_read_byte
    add dword [.result], eax
    mov eax, dword [.result]
    ret
    
    .result dd 0

disk_read_dword:
    ; EAX = dword address
    ; DL = drive
    ; returns EAX = dword (trashes EAX)
    
    push eax
    add eax, 2
    call disk_read_word
    shl eax, 16
    mov dword [.result], eax
    pop eax
    call disk_read_word
    add dword [.result], eax
    mov eax, dword [.result]
    ret
    
    .result dd 0

disk_cmp_strings:
    ; EAX = string address
    ; ESI = string to compare
    ; DL = drive
    ; returns carry if equal
    
    push eax
    push esi
    
    clc
    .loop:
        push eax
        call disk_read_byte
        mov ah, al
        a32 o32 lodsb
        cmp ah, al
        jne .notequ
        test al, al
        jz .equ
        pop eax
        inc eax
        jmp .loop
    
    .notequ:
        clc
        jmp .done
    
    .equ:
        stc

    .done:
        pop eax
        pop esi
        pop eax
        ret
