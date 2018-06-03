#ifndef __IPI_H__
#define __IPI_H__

#define IPI_BASE 0x40
#define IPI_ABORT (IPI_BASE + 0)
#define IPI_SCHEDULER (IPI_BASE + 1)

void ipi_abort(void);

void scheduler_ipi(void);

void ipi_abort_handler(void);

#endif
