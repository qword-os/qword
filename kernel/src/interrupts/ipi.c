#include <ipi.h>
#include <panic.h>
#include <smp.h>
#include <task.h>

void ipi_abort_handler(void) {
    panic("Received inter-processor interrupt ABORT...", 0, 0);
}

void ipi_resched_handler(void) {
    size_t cpu_number = fsr(&global_cpu_local->cpu_number);
    cpu_local_t *cpu = &cpu_locals[cpu_number];
    thread_identifier_t *run_queue = cpu->run_queue;

    for (size_t i = 0; i < MAX_THREADS; i++) {
        /* If an entry isn't free, it's safe to assume that
         * this entry was just updated by the scheduler
         * on the BSP */
        if (!(cpu->run_queue[i].is_free)) {
            thread_identifier_t t = cpu->run_queue[i];
            size_t pid = t.process_idx;
            size_t tid = t.thread_idx;

            thread_t *next_thread = process_table[pid]->threads[tid];
            uint64_t *pagemap = process_table[pid]->pagemap->pagemap;
            /* Context in RDI, pagemap in RSI */
            ctx_switch(&next_thread->ctx, pagemap);
        } else {
            continue;
        }
    } 
}
