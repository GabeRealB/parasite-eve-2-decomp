#ifndef GAMEPLAY_ENEMY_H
#define GAMEPLAY_ENEMY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/areaplace.h"
#include "gameplay/pairsrc.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"

/// One enemy: the work object `Gp_AllocEnemy` allocates and hangs off
/// `Task::spawnArg2`, which the enemy's task frees again when it dies.
///
/// An actor keeps its own state in a work block of its own and publishes here
/// only what the gameplay systems read back: the coordinate and body position
/// it moves the enemy through, its hit points, the placement record its spawn
/// parameters come from, and the parameter record its kind is defined by.
/// Those systems find the enemy through `node` rather than through its task -
/// the lock-on scan, the HP readout and the damage reactions all walk the list
/// it is on - and by `placeKey`, which names the placement it was spawned from
/// and is the enemy's identity for the walkers of its parent's children.
///
/// `Gp_SaveEnemyPose` files the enemy's pose under that key, and `spawnState`
/// beside it says which state the spawn handler resumes the enemy in, so a
/// room the player leaves and re-enters restores its enemies where they were
/// rather than placing them again.
///
/// A landed hit can start a reaction on the enemy (`Gp_ApplyObjKind`): flag 1
/// is a bare request, flag 2 builds up over time and ends after a random
/// delay, and flag 4 deals a fraction of `hpMax` on every tick until it
/// expires. The parameters come from `param`, scaled by a grade the attack
/// carries.
typedef struct GpEnemy {
    Task*        task;          // Owning task; the one whose `spawnArg2` is this object
    MATRIX*      field_4;       // Role unproven: actors store a model part's matrix here, nothing reads it back
    u16          placeKey;      // Key of the placement the enemy was spawned from: area, stage, and that placement's own number in the high nibble
    u16          workType;      // Work type the enemy was spawned as, bank in the high byte and type in the low (0x900 is the plain enemy)
    s32          waitTicks;     // Frames an enemy with no actor body waits before it is torn down
    GpLinkNode   node;          // Lock-on link: the entry the aim scan, HP readout and damage reactions reach the enemy by
    GpCoord*     coord;         // Coordinate the body sits at, usually one of the actor's model parts
    VECTOR3      bodyPos;       // Body position in `coord`'s frame: the point distance and damage-chance rolls measure from
    byte         pad_28[4];
    VECTOR3      playerRelPos;  // `bodyPos` brought to world space and made relative to the player, refreshed each frame; the aim and lock-on scans take their angle and distance from it
    byte         pad_38[4];
    GpAreaPlace* place;         // Placement record behind the enemy's spawn parameters (an actor may publish a table of its own here)
    s16          hp;            // Hit points left; damage subtracts from it and the readout shows it against `hpMax`
    u16          hpMax;         // Hit points the enemy is spawned with; a damage reaction is picked by fractions of it
    byte         pad_44[4];
    u8           field_48;      // Role unproven: every spawn handler clears it, nothing reads it back
    byte         pad_49[2];
    u8           spawnState;    // State the enemy is respawned in: saved with its pose and restored by the spawn handlers
    u8           reactionFlags; // Reactions a landed hit asked for: bits 0-1 stagger the body, bits 2-3 the damage-over-time reaction, cleared as the body consumes them
    u8           field_4D;      // Role unproven: cleared beside `reactionFlags` on spawn, nothing reads it back
    u8           colorMode;     // Colour remap the body is drawn with: current mode in bits 0-1, previous in bits 2-3, bit 7 a pending hit flash
    u8           colorBlend;    // Frames a colour remap change is blended over, in sixteenths; 0 switches at once
    GpPairSrcE*  param;         // Parameter record the enemy's kind is defined by, shared with every enemy of that kind, `NULL` where the kind has none
    GpRec18*     recs;          // The enemy's own contact records; its collision bodies point at the table and the Parasite Energy targeting claims entries in it
    u8           flag2Steps;    // Flag-2 reaction: steps it has built up, each 0x1F frames long, up to the limit `param->flag2Ticks` and its grade allow
    u8           flag4Delay;    // Flag-4 reaction: frames left until its next damage tick, reseeded at random on each tick
    u8           flag4Ticks;    // Flag-4 reaction: damage ticks dealt so far, measured against `param->flag4Ticks`
    u8           flag2Timer;    // Flag-2 reaction: frames into the current step; once the limit is reached, a random countdown to the reaction ending
    u8           flag4Grade;    // Flag-4 reaction: 0-9 row of the scale tables its length and damage are taken from; 0 for an attack without one
    u8           flag2Grade;    // Flag-2 reaction: 0-9 row of the scale table its length is taken from; 0 for an attack without one
    byte         pad_5E[2];
} GpEnemy;
STATIC_ASSERT_SIZEOF(GpEnemy, 0x60);

/// Callback for GpEnemy + Task state handlers (entries in `Gp_EnemyWaitFuncs`).
typedef void (*GpEnemyTaskFunc)(GpEnemy* enemy, Task* task);

/// Fixed-size table of `GpEnemyTaskFunc` callbacks. Copied onto the stack by
/// `Gp_EnemyDispatch` so the call uses a local jump table.
typedef struct {
    GpEnemyTaskFunc funcs[3];
} GpEnemyTaskFuncTable3;

/// Four-entry form of `GpEnemyTaskFuncTable3`, for actors whose dispatcher has
/// an extra state beyond spawn/tick/teardown.
typedef struct {
    GpEnemyTaskFunc funcs[4];
} GpEnemyTaskFuncTable4;

/// Five-entry form of `GpEnemyTaskFuncTable3`, for actors with two extra
/// states beyond spawn/tick/teardown.
typedef struct {
    GpEnemyTaskFunc funcs[5];
} GpEnemyTaskFuncTable5;

/// Overlay of `Task::spawnArg2` for sibling walkers. `field_A` high byte is
/// the work type (`Gp_FindChildType9` / `Gp_ExitChildrenType9` / `Gp_SendMsgType9` match 9;
/// `Gp_FindChildExceptType9` skips 9). `field_8` is the id compared against the search
/// key (`as_u16` / `as_u8`; `Gp_FindWorkById` matches `as_u16` on slot 4's
/// children). `field_3C` is the placement record the children of slot 4 were
/// spawned from, whose `entryId` `Gp_ApplyAreaTmdFlags` matches. Full size unknown.
typedef struct _GpWorkObj {
    /* 0x00 */ Task* field_0; // task owning this slot-4 object
    /* 0x04 */ byte  pad_4[4];
    /* 0x08 */ union {
        u16 as_u16;
        u8  as_u8;
    } field_8;
    /* 0x0A */ u16          field_A;
    /* 0x0C */ byte         pad_C[0x30];
    /* 0x3C */ GpAreaPlace* field_3C;
} GpWorkObj;

#endif // GAMEPLAY_ENEMY_H
