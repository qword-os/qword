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
#include <lock.h>
#include <time.h>

void *ktask(void *arg) {
    for (int i = 0; ; i++) {
        spinlock_acquire(&scheduler_lock);
        kprint(0, "CPU %U, hello world, tid: %U, iter: %u", current_cpu, arg, i);
        spinlock_release(&scheduler_lock);
        ksleep(1000);
        if (i == (int)(size_t)arg) {
            spinlock_acquire(&scheduler_lock);
            //thread_destroy(0, (size_t)arg);     /* die */
            spinlock_release(&scheduler_lock);
        }
    }

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

    spinlock_acquire(&scheduler_lock);
    for (int i = 0; i < 32; i++) {
        size_t *stack = kalloc(1024 * sizeof(size_t));
        task_tcreate(0, &stack[1023], ktask, (void *)(size_t)i);
    }
    spinlock_release(&scheduler_lock);

    for (;;)
        asm volatile ("hlt;");

    return 0;
}
