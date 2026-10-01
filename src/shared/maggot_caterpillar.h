/* The code the Maggot (actor_102600) and the Caterpillar (actor_105500) share;
 * the code sees the work block as MaggotCaterpillarWork. It waits until the player
 * comes near, then either drops from above on a line, drawn fading grey, or
 * makes a scripted entrance. After that it turns toward the player and
 * chooses an attack. Close up it pounces forward, stepping along a per-frame
 * stride table; if the pounce hits something it bounces back. If it is not
 * burning and the player is not blinded, it instead sprays a burst of short-
 * lived puff projectiles, each a growing textured sprite. A type-7 hit sets it
 * burning: it gives off an effect from two body nodes, plays a periodic sound
 * and its pounce does elemental damage. Timed status hits damage it, a type-2
 * hit stuns it, and when it dies it collapses, squashes flat and fades,
 * leaving a husk model lit with the room's texture page.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_MAGGOT_CATERPILLAR_H
#define SRC_SHARED_MAGGOT_CATERPILLAR_H

#include "types.h"

#include "actors/actor.h"

#include "main/task_types.h"

/// Work block of the enemy whose code both actor_05500 and actor_02600 carry,
/// kept at `Task::work`. Animation setup fills the context and eight slots;
/// the projectile task has its own smaller collision-work allocation.
typedef struct MaggotCaterpillarWork {
    AnimationContext      anim;
    AnimationSlot         slots[8];
    byte                  field_154[0x80];
    MATRIX                field_1D4;
    MATRIX                field_1F4;
    WorldCollisionBody    field_214;
    WorldCollisionContact field_234[4];
    WorldCollisionBody    field_294;
    WorldCollisionContact field_2B4[2];
    WorldCollisionBody    field_2E4;
    WorldCollisionContact field_304[1];
    WorldCollisionBody    field_31C;
    WorldCollisionContact field_33C[1];
    EffectSpawnArg        field_354;
    VECTOR3               field_35C;
    byte                  pad_368[4];
    TaskDesc*             field_36C;
    MATRIX                field_370;
    s16                   field_390;
    s16                   field_392;
    s16                   field_394;
    u16                   field_396;
    s16                   field_398;
    s16                   field_39A;
    s16                   field_39C;
    s16                   field_39E;
    s16                   field_3A0;
    s16                   field_3A2;
    s16                   field_3A4;
    s16                   field_3A6;
    s16                   field_3A8;
    s16                   field_3AA;
    u16                   field_3AC;
    byte                  pad_3AE[2];
    s16                   field_3B0;
    s16                   field_3B2;
    s16                   field_3B4;
    s16                   field_3B6;
    byte                  pad_3B8[2];
    s16                   field_3BA;
    s16                   field_3BC;
    s16                   field_3BE;
    s16                   field_3C0;
    s16                   field_3C2;
    s16                   field_3C4;
    s16                   field_3C6;
    s16                   field_3C8;
    s16                   field_3CA;
    s16                   field_3CC;
    s16                   field_3CE;
    s16                   field_3D0;
    s16                   field_3D2;
} MaggotCaterpillarWork;
STATIC_ASSERT_SIZEOF(MaggotCaterpillarWork, 0x3D4);

/// Scratch-pad block for projecting one end of the enemy's line primitives:
/// the point, its screen position and the depth the line is sorted at.
typedef struct MaggotCaterpillarLineScratch {
    s32     unused[4];
    SVECTOR position;
    s32     screen;
    s32     depth;
} MaggotCaterpillarLineScratch;
STATIC_ASSERT_SIZEOF(MaggotCaterpillarLineScratch, 0x20);

/// Scratch-pad block of that enemy's push-back: the deltas the collision walk
/// resolves, their normal and its image in grid space, and the rotation the
/// actor is re-aimed with.
typedef struct MaggotCaterpillarHitScratch {
    GpDeltaScratch delta;
    VECTOR         normal;
    VECTOR         local;
    SVECTOR        rot;
} MaggotCaterpillarHitScratch;
STATIC_ASSERT_SIZEOF(MaggotCaterpillarHitScratch, 0x38);

void maggotCaterpillarSprayState(Task* arg0);
void maggotCaterpillarPounceState(Task* arg0);
void maggotCaterpillarHurtState(Task* arg0);
void maggotCaterpillarEntranceState(Task* arg0);
void maggotCaterpillarBurnStep(Task* arg0);
void maggotCaterpillarTurnStep(Task* arg0);
void maggotCaterpillarDyingState(Enemy* arg0, Task* arg1);
void maggotCaterpillarPuffTick(Enemy* arg0, Task* arg1);
void maggotCaterpillarDrawPuff(Task* actor, s32 frame);
void maggotCaterpillarDrawThread(Task* actor);
void maggotCaterpillarTick(Enemy* arg0, Task* arg1);
void maggotCaterpillarApplyStatus(Task* arg0);
void maggotCaterpillarStunState(Task* arg0);
void maggotCaterpillarMoveStep(Task* arg0);
void maggotCaterpillarDrawShadow(Task* arg0);
void maggotCaterpillarSquash(Task* arg0);
void maggotCaterpillarSpawnHusk(Task* actor);
void maggotCaterpillarShrinkNode2(Task* actor);
void maggotCaterpillarPuffSetup(Enemy* enemy, Task* task);

/* Defined by each package. */
void maggotCaterpillarResolveContacts(Task* arg0);
void maggotCaterpillarRunBehaviour(Task* arg0);
void maggotCaterpillarTickAnim(Task* arg0);
void maggotCaterpillarUpdateColor(Task* arg0);

static inline void maggotCaterpillarTickAnimInline(Task* task);

#endif /* SRC_SHARED_MAGGOT_CATERPILLAR_H */
