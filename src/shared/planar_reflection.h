/* Player and held-object reflections with private scale and colour data.
 *
 * PlayerTask is a static-inline implementation. The including overlay defines
 * its ordinary entry point after planar_reflection.inc.c and owns the two-entry
 * task table. Reflection_GetTasks supplies that table as a typed, inline view.
 * Any exported table has an unconditional declaration in the overlay header.
 *
 * Each room defines `PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION` as 0
 * or 1 before including this header and keeps it defined through the shared
 * implementation, then undefines it. 1 includes `planar_reflection_rodata.inc.c`
 * at the implementation's position; 0 requires the room to include that file
 * once at its earlier rodata position, before the implementation. Both modes
 * define the same private Q12 X-reflection scale vector in rodata. The selection
 * preserves each room's rodata ordering.
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
