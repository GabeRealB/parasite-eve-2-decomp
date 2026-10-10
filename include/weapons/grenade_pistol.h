#ifndef INCLUDE_WEAPONS_GRENADE_PISTOL_H
#define INCLUDE_WEAPONS_GRENADE_PISTOL_H

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Single-part held Grenade Pistol model attached to the player's weapon hand.
///
/// Geometry and draw commands are shared by its runtime instances; initial
/// transforms are copied into each instance. The Grenade Pistol overlay and
/// its textures must remain loaded until those instances are released.
/// The source and stream stay writable because creation resolves draw handlers
/// in place. Each primitive-buffer half holds two textured Gouraud triangles
/// and 33 textured Gouraud quads, all transformed from the weapon root.
extern TmdSource gGrenadePistolWeaponModel;

/// Single-part held MM1 model attached to the player's weapon hand.
///
/// Instances borrow its geometry and writable command stream; creation copies
/// the read-only rest transform and resolves stream draw handlers in place.
/// Each primitive-buffer half holds four textured Gouraud triangles and 41
/// textured Gouraud quads, all transformed from the weapon root. Keep the MM1
/// overlay and its textures loaded until every referring instance is released.
extern TmdSource gMm1WeaponModel;

/// Single-part Grenade Pistol projectile model used during flight and the blast countdown.
///
/// Eight vertices and eight lighting normals form six textured Gouraud quads;
/// each primitive-buffer half holds those six packets. Instances borrow the
/// geometry and writable command stream, while creation copies the initial
/// root transform and resolves stream draw handlers in place. Keep the Grenade
/// Pistol overlay and its textures loaded until every referring task is released.
extern TmdSource gGrenadePistolShellModel;

/// Single-part MM1 grenade-projectile model shared by its flying shells.
///
/// Instances borrow its geometry and writable command stream; creation copies
/// the read-only rest transform and resolves stream draw handlers in place.
/// Each primitive-buffer half holds six textured Gouraud quads, transformed
/// from the projectile root. Keep the MM1 overlay and its textures loaded
/// until every referring projectile is released.
extern TmdSource gMm1ShellModel;

/// Runs the player's Grenade Pistol ready, fire and recovery phases.
///
/// Requires live player `GameActor` work, native animation slots, equipment slot
/// 1 and the equipped weapon overlay. Phase 0 enters normal actor state 4;
/// phase 2 consumes one primary round and spawns a projectile using the current
/// ammunition index. The shot sets a 40-tick cooldown; recovery ends with the
/// selected character's weapon animation. `GameActor::statePhase` must be 0..3.
void grenadePistolAttackState(Task* playerTask);

/// Runs the same attack phases for the MM1's load, muzzle row and projectile.
///
/// Uses the Grenade Pistol attack contract with the MM1 overlay loaded.
void mm1AttackState(Task* playerTask);

void func_mm1_8011DBD0(Task* arg0);

/// Dispatches the Grenade Pistol projectile's spawn, flight, blast and teardown states.
///
/// Requires task state 0..3 and the matching weapon/gameplay overlays loaded.
/// Spawn owns the `WeaponGrenadeWork` allocation and initializes both collision
/// bodies; flight and blast require it live. Teardown unlinks both bodies and
/// releases the task/work. The projectile's packed spawn argument supplies
/// ammunition and launcher-row selectors to the states. The MM1 build exports
/// its own alias of this dispatcher; its model and launch tuning remain separate.
void grenadePistolShellTask(Task* task);

#endif // INCLUDE_WEAPONS_GRENADE_PISTOL_H
