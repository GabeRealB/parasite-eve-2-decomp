#ifndef GAMEPLAY_SCENE_COMBAT_H
#define GAMEPLAY_SCENE_COMBAT_H

#include "gameplay/world_state.h"

/// Scene-wide battle state, actor controls, rewards and enemy-group signals.
///
/// The gameplay overlay owns this storage and resets it on scene loading.
/// Room, actor and PE overlays share it while gameplay is loaded; enemy tasks
/// and scripted encounters hold battle references until they die or retire.
extern SceneCombatState gSceneCombatState;

#endif // GAMEPLAY_SCENE_COMBAT_H
