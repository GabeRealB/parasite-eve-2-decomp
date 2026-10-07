/* Telephone/save menu and play-data, weapon and PE statistics panels.
 *
 * _telephoneMenuTask is a static-inline implementation called by an ordinary overlay
 * entry point. Private functions and data have the same names in every TU.
 * TELEPHONE_TITLE_BYTES preserves the 12-byte title allocation, including its
 * terminator and two retained trailing bytes; visible text is shared.
 *
 * Include this header in the prologue and telephone_data.inc.c at the data
 * position. Include telephone.inc.c, the overlay menu wrapper, then
 * telephone_panels.inc.c to preserve function and interleaved rodata order.
 */

#ifndef SRC_SHARED_TELEPHONE_H
#define SRC_SHARED_TELEPHONE_H

#include "main/task_types.h"

/* Interface for the including source. */

static inline void _telephoneMenuTask(Task* task);

#endif /* SRC_SHARED_TELEPHONE_H */
