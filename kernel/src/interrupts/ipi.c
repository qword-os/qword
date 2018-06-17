#include <ipi.h>
#include <panic.h>
#include <smp.h>
#include <task.h>

void ipi_abort_handler(void) {
    panic("Received inter-processor interrupt ABORT...", 0, 0);
}
