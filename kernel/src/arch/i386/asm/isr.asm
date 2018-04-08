global int_handler
extern dummy_int_handler

; Common handler that saves registers, calls a common function, restores registers and then returns.
%macro common_handler 1
    pusha

    call %1

    popa

    iretd

%endmacro

int_handler:
    common_handler dummy_int_handler
