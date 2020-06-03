section .stivalehdr

align 4

stivale_header:
    .stack: dq 0xeffff0  ; Stack pointer.
    .videomode: dw 1     ; VESA.
    .fbwidth: dw 0       ; Framebuffer info: 0 for default.
    .fbheight: dw 0      ; Ditto.
    .fbbpp: dw 0         ; Ditto.
    .entry: dq 0
