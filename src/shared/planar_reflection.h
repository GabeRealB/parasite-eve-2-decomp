/* Player and held-object reflections with private scale and colour data.
 *
 * `_planarReflectionPlayerTask` is a static-inline implementation. The including
 * overlay defines its ordinary entry point after planar_reflection.inc.c and
 * owns the two-entry task table. `_planarReflectionGetTaskTable` borrows that
 * table without copying it.
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

static inline void _planarReflectionPlayerTask(Task* reflectionTask);

/// Descriptor slot for reflected player attachment and equipment models.
enum { PLANAR_REFLECTION_TASK_ATTACHMENT = 1 };

/* The including overlay supplies its task table. */
static inline TaskDesc* _planarReflectionGetTaskTable(void);

#endif /* SRC_SHARED_PLANAR_REFLECTION_H */
