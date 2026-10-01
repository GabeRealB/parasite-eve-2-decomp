/* The Skull Stalker (actor_104600's second enemy, actor_207200): 1 HP, no
 * attack, and a pulsing light: a
 * light blend ramps 0..0x12 and back, and at the top it switches the model to
 * lit mode 2 and becomes targetable. It idles on animation 1 and rolls random
 * waits between pulses. When the player touches it (a 0x10000 contact) it sets
 * a global alarm byte, switches to its agitated animation with a repeating
 * cry, and arms the battle state. Any damaging hit kills it with sparks and a
 * hit sound; a zero-damage hit only applies the id's side effect. Dying, it is
 * pressed flat into the floor by a decaying Y scale and destroyed after 0x3D
 * frames. A placement mode picks one of two sound sets and an alternate
 * texture page/CLUT.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_SKULL_STALKER_H
#define SRC_SHARED_SKULL_STALKER_H

#include "types.h"

#include "main/task_types.h"
#include "main/session_types.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"

/// The Skull Stalker's 0x2B0-byte work block, allocated by its
/// spawn state and parked in `Task::work`. It carries three `WorldCollisionBody` bodies:
/// the first points its `context.capsule` at the `WorldCollisionCapsule` after it, the other
/// two point at their own `WorldCollisionContact` tables.
typedef struct SkullStalkerWork {
    /* 0x000 */ AnimationContext      context;
    /* 0x014 */ AnimationSlot         slots[3];
    /* 0x08C */ byte                  field_8C[0x30]; // pose buffer handed to func_800B3F84
    /* 0x0BC */ MATRIX                field_BC;       // colour matrix, TmdObject::colorMtx
    /* 0x0DC */ MATRIX                field_DC;       // light matrix, TmdObject::lightMtx
    /* 0x0FC */ WorldCollisionBody    field_FC;
    /* 0x11C */ WorldCollisionCapsule field_11C;
    /* 0x134 */ WorldCollisionContact field_134[1];
    /* 0x14C */ WorldCollisionBody    field_14C;
    /* 0x16C */ WorldCollisionContact field_16C[1];
    /* 0x184 */ WorldCollisionBody    field_184;
    /* 0x1A4 */ WorldCollisionContact field_1A4[4]; // the enemy's `recs`
    /* 0x204 */ byte                  pad_204[0x50];
    /* 0x254 */ s32                   field_254;    // position restored when the push-back conflicts
    /* 0x258 */ s32                   field_258;
    /* 0x25C */ s32                   field_25C;
    /* 0x260 */ byte                  pad_260[4];
    /* 0x264 */ MATRIX                field_264; // root transform the dying enemy refolds
    /* 0x284 */ byte                  pad_284[2];
    /* 0x286 */ s16                   field_286; // reaction state
    /* 0x288 */ s16                   field_288; // non-zero once the death has unlinked the bodies
    /* 0x28A */ s16                   field_28A; // frames spent in the current state
    /* 0x28C */ s16                   field_28C; // animation id the work is playing
    /* 0x28E */ s16                   field_28E; // id the two helper slots last saw
    /* 0x290 */ s16                   field_290; // frames spent on the current id
    /* 0x292 */ s16                   field_292;
    /* 0x294 */ byte                  pad_294[6];
    /* 0x29A */ s16                   field_29A;
    /* 0x29C */ byte                  pad_29C[4];
    /* 0x2A0 */ s16                   field_2A0; // Y scale folded onto the saved transform
    /* 0x2A2 */ byte                  pad_2A2[2];
    /* 0x2A4 */ s16                   field_2A4; // light blend, 0..0x12
    /* 0x2A6 */ s16                   field_2A6; // non-zero: the blend is rising
    /* 0x2A8 */ s16                   field_2A8; // frames until the next blend turn
    /* 0x2AA */ s16                   field_2AA; // latched by a hit
    /* 0x2AC */ s16                   field_2AC; // placement mode; picks the sound set
    /* 0x2AE */ byte                  pad_2AE[2];
} SkullStalkerWork;
STATIC_ASSERT_SIZEOF(SkullStalkerWork, 0x2B0);

void skullStalkerSpawnState(Enemy* arg0, Task* arg1);
void skullStalkerIdleTick(Task* arg0);
void skullStalkerHits(Task* arg0);
void skullStalkerDeathState(Enemy* arg0, Task* arg1);
void skullStalkerUpdateState(Enemy* arg0, Task* arg1);
void skullStalkerReactionFlags(Task* arg0);
void skullStalkerReactionDispatch(Task* task);
void skullStalkerLightRamp(Task* task);
void skullStalkerFlatten(Task* arg0);
void skullStalkerExit(Task* task);

/* Defined by each package, as an include of sucklerceph_colour.inc.c. */
void skullStalkerColour(Enemy* arg0, Task* task);

static inline void skullStalkerTickAnim(Task* task);

void skullStalkerTask(Task* arg0);
void skullStalkerAnimate(Task* arg0);

#endif /* SRC_SHARED_SKULL_STALKER_H */
