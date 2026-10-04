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
 * Each package states which enemy it builds before including this header:
 * MAGGOT_CATERPILLAR_KIND is MAGGOT (actor_02600) or CATERPILLAR
 * (actor_05500); the parameters below follow from it.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_MAGGOT_CATERPILLAR_H
#define SRC_SHARED_MAGGOT_CATERPILLAR_H

#define MAGGOT      1
#define CATERPILLAR 2
#ifndef MAGGOT_CATERPILLAR_KIND
#error "define MAGGOT_CATERPILLAR_KIND (MAGGOT or CATERPILLAR) before including maggot_caterpillar.h"
#endif

/* Per kind: the type id (in its collision keys, 0x30000 | id); the flag the
 * spawn stores in `field_3C0`, which the dying sequence reads back for the id
 * and which keeps the Caterpillar from spraying; and the player distances at
 * which it wakes, drops from its ambush, and pounces. */
#if MAGGOT_CATERPILLAR_KIND == MAGGOT
#define MAGGOT_CATERPILLAR_ID             0x1A
#define MAGGOT_CATERPILLAR_IS_CATERPILLAR 0
#define MAGGOT_CATERPILLAR_WAKE_RANGE     0x9C4
#define MAGGOT_CATERPILLAR_AMBUSH_RANGE   0x7D0
#define MAGGOT_CATERPILLAR_POUNCE_RANGE   0x9C4
#else
#define MAGGOT_CATERPILLAR_ID             0x37
#define MAGGOT_CATERPILLAR_IS_CATERPILLAR 1
#define MAGGOT_CATERPILLAR_WAKE_RANGE     0x7D0
#define MAGGOT_CATERPILLAR_AMBUSH_RANGE   0x5DC
#define MAGGOT_CATERPILLAR_POUNCE_RANGE   0x8FC
#endif

#include "types.h"

#include "actors/actor.h"

#include "main/task_types.h"

/// Work block of the enemy whose code both actor_05500 and actor_02600 carry,
/// kept at `Task::work`. Animation setup fills the context and eight slots;
/// the projectile task has its own smaller collision-work allocation.
typedef struct MaggotCaterpillarWork {
    ActorAnimRig8         rig; // Playback storage of the model's parts; slots 1 to 7 are driven
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
    WorldCollisionDelta delta;
    VECTOR              normal;
    VECTOR              local;
    SVECTOR             rot;
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

void maggotCaterpillarWaitState(Task* arg0);
void maggotCaterpillarAimState(Task* arg0);
void maggotCaterpillarAmbushState(Task* actor);
void maggotCaterpillarRoamState(Task* arg0);
void maggotCaterpillarSpawn(Enemy* ctx, Task* actor);
void maggotCaterpillarResolveContacts(Task* arg0);
void maggotCaterpillarPuffTask(Task* arg0);
void maggotCaterpillarTask(Task* arg0);
void maggotCaterpillarRunBehaviour(Task* arg0);
void maggotCaterpillarTickAnim(Task* arg0);
void maggotCaterpillarUpdateColor(Task* arg0);

static inline void maggotCaterpillarTickAnimInline(Task* task);

/// Stores the yaw that `coord`'s frame faces in `work->field_3A2`, then
/// rebuilds the frame's rotation as a level turn half a revolution away from
/// it, with `rot` holding the angles.
#define MAGGOT_CATERPILLAR_TURN_AROUND(work, coord, rot)                                    \
    do {                                                                                    \
        (work)->field_3A2 = ratan2((coord)->coord.m[0][2], (coord)->coord.m[2][2]) & 0xFFF; \
        (rot)->vx         = 0;                                                              \
        (rot)->vy         = (u16)(work)->field_3A2 + 0x800;                                 \
        (rot)->vz         = 0;                                                              \
        RotMatrix((rot), &(coord)->coord);                                                  \
    } while (0)

/// How far the origin of `coord`'s frame lies inside contact `rec`, clamped at
/// zero, into `out`. `delta` receives the offset from the contact point to
/// the origin.
#define MAGGOT_CATERPILLAR_CONTACT_OVERLAP(out, coord, rec, delta)                                 \
    do {                                                                                           \
        s32 offX;                                                                                  \
        s32 offY;                                                                                  \
        s32 offZ;                                                                                  \
        s32 clamped;                                                                               \
        offX              = (coord)->workm.t[0] - (rec).point.vx;                                  \
        (delta).vector.vx = offX;                                                                  \
        offY              = (coord)->workm.t[1] - (rec).point.vy;                                  \
        (delta).vector.vy = offY;                                                                  \
        offZ              = (coord)->workm.t[2] - (rec).point.vz;                                  \
        (delta).vector.vz = offZ;                                                                  \
        (out)             = (rec).distance - SquareRoot0(offX * offX + offY * offY + offZ * offZ); \
        clamped           = (out);                                                                 \
        if ((out) <= 0) {                                                                          \
            clamped = 0;                                                                           \
        }                                                                                          \
        (out) = clamped;                                                                           \
    } while (0)

/// Normalises `delta` into `unit` and expresses the direction in the frame of
/// the collision grid, into `out`.
#define MAGGOT_CATERPILLAR_GRID_DIRECTION(delta, unit, out)                      \
    do {                                                                         \
        VectorNormal(&(delta)->vector, (unit));                                  \
        ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, (unit), (out)); \
    } while (0)

/// Sets `work->field_3CE` when contact `rec` is a body, or a face of the
/// collision grid whose normal has no vertical component.
#define MAGGOT_CATERPILLAR_NOTE_BLOCKING_CONTACT(work, rec)                                         \
    do {                                                                                            \
        if ((((rec).key.value & 0xFFFF0000) == 0x10000) ||                                          \
            ((((rec).key.value & 0xFFFF0000) == 0x100000) && ((rec).response.direction.vy == 0))) { \
            (work)->field_3CE = 1;                                                                  \
        }                                                                                           \
    } while (0)

#endif /* SRC_SHARED_MAGGOT_CATERPILLAR_H */
