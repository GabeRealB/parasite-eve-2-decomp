#ifndef COMBAT_LIGHT_COMBAT_LIGHT_H
#define COMBAT_LIGHT_COMBAT_LIGHT_H

#include <engine/task_types.h>

/// The Combat Light: a flash, a light and a beam of six cones that fade over a few frames, applying the item at once;
/// a held attachment or any pause of the room's PE stops its sound and frees it.
void combatLightEffectTask(Task* task);

#endif // COMBAT_LIGHT_COMBAT_LIGHT_H
