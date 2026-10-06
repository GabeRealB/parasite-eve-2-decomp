#ifndef GAMEPLAY_PRIVATE_SCENE_COMBAT_H
#define GAMEPLAY_PRIVATE_SCENE_COMBAT_H

/// Resets battle accounting, actor controls and enemy-group signals for a new scene.
///
/// Clears the idle battle's signals, holds, rewards and group coordination, and
/// enables actor updates. Training uses normal difficulty; other scenes use the
/// live save's game mode, with a cleared normal save selecting the replay row.
/// Call during scene loading before enemy tasks begin taking battle holds.
void sceneResetCombatState(void);

#endif // GAMEPLAY_PRIVATE_SCENE_COMBAT_H
