#include <klib.h>
#include <serial.h>
#include <tty.h>
#include <vga_textmode.h>
#include <vbe_tty.h>
#include <vbe.h>
#include <e820.h>
#include <mm.h>
#include <idt.h>
#include <pic.h>
#include <acpi.h>
#include <cmdline.h>
#include <pit.h>
#include <smp.h>
#include <task.h>
#include <cio.h>

void *ktask(void *arg) {
    kprint(0,"CPU %U, hello world, arg: %U", fsr(&global_cpu_local->cpu_number), arg);

    for (;;) { asm volatile ("hlt"); }
}

/* Main kernel entry point, all the things should be initialised */
int kmain(void) {
    init_idt();

    init_com1();
    init_vga_textmode();

    init_tty();

    kprint(KPRN_INFO, "Kernel booted");
    kprint(KPRN_INFO, "Build time: %s", BUILD_TIME);
    kprint(KPRN_INFO, "Command line: %s", cmdline);

    /* Memory-related stuff */
    init_e820();
    init_pmm();
    init_vmm();

    /* Driver inits */
    init_vbe();
    init_vbe_tty();
    init_acpi();
    init_pic();
    
    /* TODO move this someplace else */
    asm volatile ("sti");

    init_pit();
    init_smp();
    init_sched();
    
    for (size_t i = 0; i < MAX_THREADS; i++) {
        kprint(KPRN_DBG, "%u", thread_create(0, kalloc(KRNL_STACK_SIZE), ktask, (void *)i));
    }
    
    for (;;)
        asm volatile ("hlt;");

    return 0;
}
