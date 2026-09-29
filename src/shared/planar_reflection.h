/* Player and held-object reflections with private scale and colour data.
 *
 * PlayerTask is a static-inline implementation. The including overlay defines
 * its ordinary entry point after planar_reflection.inc.c and owns the two-entry
 * task table. Reflection_GetTasks supplies that table as a typed, inline view.
 * Any exported table has an unconditional declaration in the overlay header.
 *
 * REFLECTION_SCALE_IN_CODE is 1 when the scale constant is emitted with code,
 * or 0 when planar_reflection_rodata.inc.c supplies it at an earlier rodata
 * position. This changes placement only; values and behaviour are shared.
 * Include this header in the prologue and planar_reflection_data.inc.c before
 * the overlay's task table at its existing data position.
 */

#ifndef SRC_SHARED_PLANAR_REFLECTION_H
#define SRC_SHARED_PLANAR_REFLECTION_H

#include "main/task_types.h"

/* Interface for the including source. */

static inline void Reflection_PlayerTask(Task* task);

/* The including overlay supplies its task table. */
static inline TaskDesc* Reflection_GetTasks(void);

#endif /* SRC_SHARED_PLANAR_REFLECTION_H */
