/* The Sucklerceph, the first enemy of actor_104600. It spawns
 * either standing (dormant) or hidden until a message drops it into place from
 * a spawn point (maps 0x27/0x28). While dormant it rocks back and forth. When
 * the player enters its wide sensing sphere (a 0x10000 contact) it wakes,
 * turns toward the player by up to 0x20 a frame and crawls at 0x14 a step,
 * with a randomly timed idle sound. Within 0x320 of the player it swells, its
 * body-part scale growing 0xC8 a frame, and on the fifth frame it dies. The
 * death is picked at random (or forced): either it bursts - hit spheres armed,
 * burst effects, a death script, model hidden - or it slumps and its body is
 * flattened into the floor by a decaying Y scale before the enemy is
 * destroyed. It takes damage and critical kills from player hits (0x20000
 * contacts), is pushed out of walls (0x30000), and can be told by message to
 * collapse away (modes 4/5). A spawn-arg mode picks one of two sound banks and
 * an alternate texture page/CLUT.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_SUCKLERCEPH_H
#define SRC_SHARED_SUCKLERCEPH_H

#include "types.h"

#include "main/coord.h"
#include "main/task_types.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"
#include "gameplay/effects.h"

/// The Sucklerceph's 0x2E4-byte work block, which both of its
/// spawn handlers allocate with `memCalloc` and park in `Task::work`. After the
/// animation context and its three slots come the colour and light matrices the
/// model is pointed at, then four `WorldCollisionBody` bodies, each followed by the
/// `WorldCollisionContact` table its `context.contacts` names.
typedef struct SucklercephWork {
    /* 0x000 */ AnimationContext      context;
    /* 0x014 */ AnimationSlot         slots[3];
    /* 0x08C */ byte                  field_8C[0x30]; // pose buffer handed to func_800B3F84
    /* 0x0BC */ MATRIX                field_BC;       // colour matrix, TmdObject::colorMtx
    /* 0x0DC */ MATRIX                field_DC;       // light matrix, TmdObject::lightMtx
    /* 0x0FC */ WorldCollisionBody    objFC;
    /* 0x11C */ WorldCollisionContact rec11C;
    /* 0x134 */ WorldCollisionBody    obj134;
    /* 0x154 */ WorldCollisionContact rec154[4]; // the body's contact table; also the enemy's `recs`
    /* 0x1B4 */ WorldCollisionBody    obj1B4;
    /* 0x1D4 */ WorldCollisionContact rec1D4;
    /* 0x1EC */ WorldCollisionBody    obj1EC;
    /* 0x20C */ WorldCollisionContact rec20C;
    /* 0x224 */ byte                  pad_224[0x50];
    /* 0x274 */ VECTOR3               field_274; // root translation before the last step
    /* 0x280 */ byte                  pad_280[4];
    /* 0x284 */ EffectSpawnArg        field_284; // hit-effect coordinate and parameters
    /* 0x28C */ MATRIX                field_28C; // root transform saved when the enemy dies
    /* 0x2AC */ s32                   field_2AC; // scale factor of the model's second part
    /* 0x2B0 */ s16                   field_2B0; // heading, stepped 0x20 a frame toward the player
    /* 0x2B2 */ s16                   field_2B2; // reaction state the per-frame dispatch switches on
    /* 0x2B4 */ s16                   field_2B4; // phase of the death sequence
    /* 0x2B6 */ s16                   field_2B6; // frames spent in the death phase
    /* 0x2B8 */ s16                   field_2B8; // animation id the work is playing
    /* 0x2BA */ s16                   field_2BA; // id the two helper slots last saw
    /* 0x2BC */ u16                   field_2BC; // frames spent on the current id
    /* 0x2BE */ s16                   field_2BE; // step length along the facing
    /* 0x2C0 */ byte                  pad_2C0[6];
    /* 0x2C6 */ s16                   field_2C6;
    /* 0x2C8 */ s16                   field_2C8; // live stage: 1 alive, 2 dying
    /* 0x2CA */ s16                   field_2CA; // Y scale folded onto the saved transform
    /* 0x2CC */ s16                   field_2CC;
    /* 0x2CE */ s16                   field_2CE; // remaining hit cooldown
    /* 0x2D0 */ u16                   field_2D0; // frames until the next idle sound
    /* 0x2D2 */ s16                   field_2D2; // non-zero: the animation rebind is suppressed
    /* 0x2D4 */ u16                   field_2D4; // frame or event counter of the dying stages
    /* 0x2D6 */ s16                   field_2D6; // spawn arg's low half; picks the sound set
    /* 0x2D8 */ s16                   field_2D8; // latched once the dormant enemy is touched
    /* 0x2DA */ s16                   field_2DA; // non-zero: the death spawns a final effect
    /* 0x2DC */ s16                   field_2DC; // spawn arg's high half
    /* 0x2DE */ s16                   field_2DE; // fall speed while dropping into place
    /* 0x2E0 */ s16                   field_2E0; // non-zero once the drop has hit something
    /* 0x2E2 */ s16                   field_2E2; // non-zero: the drop has been armed
} SucklercephWork;
STATIC_ASSERT_SIZEOF(SucklercephWork, 0x2E4);

void sucklercephSpawnState(Enemy* arg0, Task* arg1);
void sucklercephReactionDispatch(Task* arg0);
void sucklercephDormantTick(Task* arg0);
void sucklercephAwakeTick(Task* arg0);
void sucklercephContacts(Task* arg0);
void sucklercephTakeDamage(Task* arg0, s32 arg1);
void sucklercephTurnToPlayer(Task* arg0);
void sucklercephDeathState(Enemy* enemy, Task* task);
void sucklercephKill(Task* arg0, u8 arg1);
void sucklercephDropSpawnState(Enemy* arg0, Task* arg1);
void sucklercephDropState(Enemy* arg0, Task* arg1);
void sucklercephDropCollide(Task* arg0);
s32  sucklercephMessage(Task* arg0, s32 arg1, ActorCommand* request);
void sucklercephUpdateState(Enemy* arg0, Task* arg1);
void sucklercephReactionFlags(Task* arg0);
void sucklercephStep(Task* task);
void sucklercephScalePart(Task* arg0, GfxCoord* arg1);
void sucklercephFlatten(Task* arg0);
void sucklercephExit(Task* task);
void sucklercephFallStep(Task* task);

static __inline__ void sucklercephTickAnim(Task* task);

/* Defined by each package. */
void sucklercephAnimate(Task* arg0);
void sucklercephColour(Enemy* arg0, Task* task);
void sucklercephDrawShadow(Task* task);

#endif /* SRC_SHARED_SUCKLERCEPH_H */
