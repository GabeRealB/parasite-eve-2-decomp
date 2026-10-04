#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/actor_contacts.h"

/// One collision sphere of the actor: a body and the contact table it records
/// into.
///
/// The work block holds four. Three ride model parts and are pair-tested
/// against other bodies while the actor is active, and some states also test
/// one of them against the room grid; the fourth sits on the root coordinate
/// and is only ever grid-tested. `contacts` is the table that body alone
/// fills: the hit handler scans the part spheres' tables for an attack
/// contact, the movement states resolve push-back from a table's contacts,
/// and each frame's update ends by resetting every table's occupied entries.
/// The spawn handler links each body and the task's exit callback unlinks it.
typedef struct {
    WorldCollisionBody    body;        // Sphere linked into the world's body list; `coord` is the part it rides
    WorldCollisionContact contacts[5]; // The body's own contact table
} _Actor403000CollisionSphere;
STATIC_ASSERT_SIZEOF(_Actor403000CollisionSphere, 0x98);

/// Per-actor work block for the `actor_403000` overlay.
///
/// `func_actor_403000_801343B8` allocates it with `memCalloc(0xFDC, 0)` and
/// parks it in the `Task::work` slot (0x1C) -- the same slot `DesertChaserWork`
/// and `Actor403100Work` use for the same job, so the overlay reaches its state
/// through the task and not through the actor. `field_0` is the animation state
/// the per-frame handler switches on and `field_2` its frame counter, both
/// restarted together by the message handlers; `yaw` is the heading
/// `func_actor_403000_8013D364` reads back from the root coordinate;
/// `field_AC6` is the
/// requested animation id, written by the handlers and read back by the
/// handlers that tick the current state.
///
/// `field_AC0` is the playback state `func_actor_403000_80133AF8` switches on
/// (1 is the running state, whose per-frame branch compares `field_AC4`, the
/// animation in progress, against `field_AC6`) and `field_AC2` the frame
/// counter it advances; `field_ACA` is the clip id that tick copies into each
/// display node, so `func_actor_403000_8013D850` starting the actor rewrites
/// `field_AC6` and `field_ACA` together. `field_6` is the per-frame tick that
/// same function bumps, and `field_60` (slot 1's `flags`) watches
/// `ANIMATION_SLOT_SETTLED` to tell that the animation it asked for is holding
/// its boundary pose.
typedef struct Actor403000Work {
    /* 0x000 */ s16                         field_0;
    /* 0x002 */ s16                         field_2;
    /* 0x004 */ s16                         field_4;
    /* 0x006 */ u16                         field_6;
    /* 0x008 */ s16                         field_8;
    /* 0x00A */ byte                        pad_A[0x2];
    /* 0x00C */ s16                         yaw;
    /* 0x00E */ byte                        pad_E[0x6];
    /* 0x014 */ AnimationContext            anim;
    /* 0x028 */ AnimationSlot               slots[24];
    /* 0x3E8 */ byte                        pad_3E8[0x180];
    /* 0x568 */ AnimationContext            blendAnim;
    /* 0x57C */ AnimationSlot               blendSlots[24];
    /* 0x93C */ byte                        pad_93C[0x184];
    /* 0xAC0 */ s16                         field_AC0;
    /* 0xAC2 */ s16                         field_AC2;
    /* 0xAC4 */ s16                         field_AC4;
    /* 0xAC6 */ s16                         field_AC6;
    /* 0xAC8 */ u16                         field_AC8;
    /* 0xACA */ s16                         field_ACA;
    /* 0xACC */ s16                         field_ACC;
    /* 0xACE */ s16                         field_ACE;
    /* 0xAD0 */ s16                         field_AD0;
    /* 0xAD2 */ s16                         field_AD2;
    /* 0xAD4 */ s16                         field_AD4;
    /* 0xAD6 */ s16                         field_AD6;
    /* 0xAD8 */ s16                         field_AD8;
    /* 0xADA */ s16                         field_ADA;
    /* 0xADC */ s16                         field_ADC;
    /* 0xADE */ s16                         field_ADE;
    /* 0xAE0 */ s16                         field_AE0;
    /* 0xAE2 */ s16                         field_AE2;
    /* 0xAE4 */ s16                         field_AE4;
    /* 0xAE6 */ s16                         field_AE6;
    /* 0xAE8 */ u8                          field_AE8;
    /* 0xAE9 */ u8                          field_AE9;
    /* 0xAEA */ u8                          field_AEA;
    /* 0xAEB */ u8                          field_AEB;
    /* 0xAEC */ byte                        pad_AEC[4];
    /* 0xAF0 */ u32                         field_AF0;
    /* 0xAF4 */ byte                        pad_AF4[0x5C];
    /* 0xB50 */ _Actor403000CollisionSphere objB50;
    /* 0xBE8 */ _Actor403000CollisionSphere objBE8;
    /* 0xC80 */ _Actor403000CollisionSphere objC80;
    /* 0xD18 */ _Actor403000CollisionSphere objD18;
    /* 0xDB0 */ WorldCollisionBody          objDB0;
    /* 0xDD0 */ WorldCollisionCapsule       recDD0;
    /// Record table `func_actor_403000_8013D48C` scans: the same five-entry
    /// `WorldCollisionContact` run each `_Actor403000CollisionSphere` carries, here standing on its own
    /// after the four spheres. `func_actor_403000_8013C864` hands it back to
    /// `Gp_ClearRec18Occupied` twice, and the scan reads a record's `key`
    /// the way the shared hit-record walkers do -- 0 means the run has ended,
    /// high half 0x10 is the kind that counts as present.
    /* 0xDE8 */ WorldCollisionContact records[5];
    /* 0xE60 */ WorldCollisionBody    objE60;
    /* 0xE80 */ WorldCollisionCapsule recE80;
    /* 0xE98 */ WorldCollisionContact recordsE98[5];
    /* 0xF10 */ MATRIX                field_F10;
    /// The second of the two default matrices the spawn handler binds to the
    /// display object -- `&work->field_F10` and this one are what it writes to
    /// `TmdObject::lightMtx` / `colorMtx` -- so it is a `MATRIX` whether or not
    /// the animation that drives it is running. `func_actor_403000_8013D72C`
    /// zeroes this one field by field, last element first.
    /* 0xF30 */ MATRIX field_F30;
    /* 0xF50 */ byte   pad_F50[0x20];
    /// Frames left before the next hit is taken (`Gp_GetIdParam2` of the last
    /// hit id), counted down by `func_actor_403000_80134F44`.
    /* 0xF70 */ s16  field_F70;
    /* 0xF72 */ byte pad_F72[0x2];
    /// Horizontal target the display object is eased toward while it sits
    /// below the player (`func_actor_403000_8013BDE0`).
    /* 0xF74 */ s16  field_F74;
    /* 0xF76 */ s16  field_F76;
    /* 0xF78 */ s16  field_F78;
    /* 0xF7A */ byte pad_F7A[0x2];
    /* 0xF7C */ s16  field_F7C;
    /* 0xF7E */ s16  field_F7E;
    /* 0xF80 */ s16  field_F80;
    /* 0xF82 */ byte pad_F82[0x2];
    /* 0xF84 */ s16  field_F84;
    /* 0xF86 */ s16  field_F86;
    /// Bitmask of the four trigger points in `D_actor_403000_80158D64` last
    /// latched from `GameFlag_GetNibble(0xE2)` by `func_actor_403000_80134E00`.
    /* 0xF88 */ s16            field_F88;
    /* 0xF8A */ byte           pad_F8A[0x2];
    /* 0xF8C */ s16            field_F8C;
    /* 0xF8E */ byte           pad_F8E[0x2];
    /* 0xF90 */ void*          field_F90;
    /* 0xF94 */ s32            field_F94;
    /* 0xF98 */ s32            field_F98;
    /* 0xF9C */ s32            field_F9C;
    /* 0xFA0 */ s32            field_FA0;
    /* 0xFA4 */ u8             field_FA4;
    /* 0xFA5 */ u8             field_FA5;
    /* 0xFA6 */ u8             field_FA6;
    /* 0xFA7 */ byte           pad_FA7[0x1];
    /* 0xFA8 */ EffectSpawnArg field_FA8; // `func_800FDB18` argument record
    /* 0xFB0 */ SVECTOR        field_FB0;
    /* 0xFB8 */ s16            field_FB8;
    /* 0xFBA */ s16            field_FBA;
    /* 0xFBC */ s16            field_FBC;
    /* 0xFBE */ byte           pad_FBE[0x2];
    /* 0xFC0 */ s16            field_FC0;
    /* 0xFC2 */ s16            field_FC2;
    /* 0xFC4 */ byte           pad_FC4[0x4];
    /* 0xFC8 */ s16            field_FC8;
    /* 0xFCA */ s16            field_FCA;
    /* 0xFCC */ u16            field_FCC;
    /* 0xFCE */ byte           pad_FCE[0x3];
    /* 0xFD1 */ s8             field_FD1;
    /* 0xFD2 */ s8             field_FD2;
    /* 0xFD3 */ s8             field_FD3;
    /* 0xFD4 */ s8             field_FD4;
    /* 0xFD5 */ s8             field_FD5;
    /* 0xFD6 */ u8             field_FD6;
    /* 0xFD7 */ s8             field_FD7;
    /* 0xFD8 */ s8             field_FD8;
    /* 0xFD9 */ u8             field_FD9;
    /* 0xFDA */ u8             field_FDA;
    /* 0xFDB */ byte           pad_FDB[0x1];
} Actor403000Work;
STATIC_ASSERT_SIZEOF(Actor403000Work, 0xFDC);

/// Event record `func_actor_403000_801324EC` dispatches on: `w[0]` is the
/// event kind (only 0x204 is handled) and `w[1]` its sub-code, and the first
/// three bytes are also copied raw into `Actor403000Work::field_FA4`..`field_FA6`.
/// Same shape as `Actor401300Event`.
typedef union Actor403000Event {
    u8  b[3];
    u16 w[2];
} Actor403000Event;

/// 0x34-byte scratch from the scratch stack used by
/// `func_actor_403000_80134F44`: `pos` and `id` are the first damage record
/// found on the four hit tables, `d`/`dist` the player's offset from the model
/// and its length, `rel` the hit position relative to the model, `damage` the
/// amount taken and `angle` the wrapped heading of the hit.
typedef struct Actor403000DamageScratch {
    /* 0x00 */ VECTOR  d;
    /* 0x10 */ SVECTOR rel;
    /* 0x18 */ SVECTOR pos;
    /* 0x20 */ s32     id;
    /* 0x24 */ u32     damage;
    /* 0x28 */ s32     dist;
    /* 0x2C */ s16     angle;
    /* 0x2E */ byte    pad_2E[0x6];
} Actor403000DamageScratch;
STATIC_ASSERT_SIZEOF(Actor403000DamageScratch, 0x34);

/// 0x28-byte scratch from the scratch stack used by
/// `func_actor_403000_801384E8`: `dir` holds the display object's first
/// matrix column, normalised and scaled down into the push vector.
typedef struct Actor403000PushScratch {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR dir;
    /* 0x18 */ byte    pad_18[0x10];
} Actor403000PushScratch;
STATIC_ASSERT_SIZEOF(Actor403000PushScratch, 0x28);

/// 0x28-byte scratch from the scratch stack used by
/// `func_actor_403000_801386E8`: `d` is the player's offset from the model and
/// `dist` its length, `target` the camera target relative to the model,
/// `angle` the clamped turn and `ret` the reply to message 0x3F9.
typedef struct Actor403000LungeScratch {
    /* 0x00 */ VECTOR  d;
    /* 0x10 */ SVECTOR target;
    /* 0x18 */ s32     dist;
    /* 0x1C */ byte    pad_1C[0x4];
    /* 0x20 */ s16     angle;
    /* 0x22 */ s16     ret;
    /* 0x24 */ byte    pad_24[0x4];
} Actor403000LungeScratch;
STATIC_ASSERT_SIZEOF(Actor403000LungeScratch, 0x28);

/// 0x28-byte scratch from the scratch stack used by
/// `func_actor_403000_80137084`: `d` is the camera target's offset from the
/// model and `dist` its length, `target` the same offset for the heading,
/// `angle` the clamped turn, `cell` / `playerCell` the waypoint-grid cells.
typedef struct Actor403000ChaseScratch {
    /* 0x00 */ VECTOR  d;
    /* 0x10 */ SVECTOR target;
    /* 0x18 */ s32     dist;
    /* 0x1C */ byte    pad_1C[0x4];
    /* 0x20 */ s16     angle;
    /* 0x22 */ byte    pad_22[0x2];
    /* 0x24 */ s8      cell;
    /* 0x25 */ s8      playerCell;
    /* 0x26 */ byte    pad_26[0x2];
} Actor403000ChaseScratch;
STATIC_ASSERT_SIZEOF(Actor403000ChaseScratch, 0x28);

/// 0x28-byte scratch from the scratch stack used by
/// `func_actor_403000_801377C8`: `d`/`dist` the player's offset from the model
/// and its length, `target` the camera target relative to the model,
/// `playerYaw`/`aimYaw` the player's facing and the reversed heading to the
/// camera target, `angle` the wrapped turn, `ret` the reply to message 0x3F9
/// and `cell`/`playerCell` the waypoint-grid cells.
typedef struct Actor403000GrabScratch {
    /* 0x00 */ VECTOR  d;
    /* 0x10 */ SVECTOR target;
    /* 0x18 */ s32     dist;
    /* 0x1C */ s16     playerYaw;
    /* 0x1E */ s16     aimYaw;
    /* 0x20 */ s16     angle;
    /* 0x22 */ s16     ret;
    /* 0x24 */ s8      cell;
    /* 0x25 */ s8      playerCell;
    /* 0x26 */ byte    pad_26[0x2];
} Actor403000GrabScratch;
STATIC_ASSERT_SIZEOF(Actor403000GrabScratch, 0x28);

/// 0x14-byte scratch from the scratch stack used by
/// `func_actor_403000_8013B238`: `vec` is the waypoint relative to the
/// coordinate and later the scaled matrix columns, `index` the waypoint picked
/// from `base` plus `field_FD5`, `angle` the wrapped heading error.
typedef struct Actor403000AimScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s16     index;
    /* 0x0A */ byte    pad_A[0x2];
    /* 0x0C */ s16     angle;
    /* 0x0E */ s16     base;
    /* 0x10 */ byte    pad_10[0x4];
} Actor403000AimScratch;
STATIC_ASSERT_SIZEOF(Actor403000AimScratch, 0x14);

/// Scratch frame used by `func_actor_403000_8013B74C` for the drop target,
/// heading error, player-message result and waypoint-grid cell.
typedef struct Actor403000DropScratch {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR target;
    /* 0x18 */ byte    pad_18[0x8];
    /* 0x20 */ s16     angle;
    /* 0x22 */ s16     ret;
    /* 0x24 */ byte    pad_24;
    /* 0x25 */ s8      base;
    /* 0x26 */ byte    pad_26[0x2];
} Actor403000DropScratch;
STATIC_ASSERT_SIZEOF(Actor403000DropScratch, 0x28);

/// 0x14-byte scratch from the scratch stack used by
/// `func_actor_403000_8013C2D4`: `facing` and `base` are the player's and the
/// actor's waypoint-grid cells, `index` the waypoint picked from `base` plus
/// `field_FD1`, `vec` it relative to the coordinate and `angle` the clamped turn.
typedef struct Actor403000SeekScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s16     index;
    /* 0x0A */ byte    pad_A[0x2];
    /* 0x0C */ s16     angle;
    /* 0x0E */ s16     base;
    /* 0x10 */ s16     facing;
    /* 0x12 */ byte    pad_12[0x2];
} Actor403000SeekScratch;
STATIC_ASSERT_SIZEOF(Actor403000SeekScratch, 0x14);

/// 0xC-byte scratch from the scratch stack used by
/// `func_actor_403000_80134204`: `index` picks the next waypoint out of
/// `D_actor_403000_80158CE0`, `target` is it relative to the coordinate and
/// `turn` the +1/-1 steering result.
typedef struct Actor403000TurnScratch {
    /* 0x00 */ SVECTOR target;
    /* 0x08 */ s8      index;
    /* 0x09 */ s8      turn;
    /* 0x0A */ byte    pad_A[0x2];
} Actor403000TurnScratch;
STATIC_ASSERT_SIZEOF(Actor403000TurnScratch, 0xC);

/// 0xC-byte scratch stack block `func_actor_403000_80133FC0` takes: the
/// player's position relative to the model, then the wrapped facing error.
typedef struct Actor403000FacingScratch {
    /* 0x00 */ SVECTOR target;
    /* 0x08 */ s16     angle;
    /* 0x0A */ byte    pad_A[0x2];
} Actor403000FacingScratch;
STATIC_ASSERT_SIZEOF(Actor403000FacingScratch, 0xC);

typedef union Actor403000Sxy {
    s32     w;
    DVECTOR v;
} Actor403000Sxy;
STATIC_ASSERT_SIZEOF(Actor403000Sxy, 0x4);

/// 0xB4-byte trail scratch block: a parented coordinate, its view-space origin,
/// and the current/previous projected point used to draw the trail segments.
/// `func_actor_403000_80132AE0` returns it to the scratch stack;
/// `func_actor_403000_801330D4` only updates the point history and keeps it.
typedef struct Actor403000TrailScratch {
    /* 0x00 */ GfxCoord       coord;
    /* 0x50 */ byte           pad_50[0x18];
    /* 0x68 */ SVECTOR        pos;
    /* 0x70 */ Actor403000Sxy sxy;
    /* 0x74 */ Actor403000Sxy prevSxy;
    /* 0x78 */ s32            p;
    /* 0x7C */ s32            flag;
    /* 0x80 */ s32            otz;
    /* 0x84 */ byte           pad_84[0x4];
    /* 0x88 */ s32            prevFlag;
    /* 0x8C */ SVECTOR        normal;
    /* 0x94 */ byte           pad_94[0x20];
} Actor403000TrailScratch;
STATIC_ASSERT_SIZEOF(Actor403000TrailScratch, 0xB4);

/// 0x38-byte scratch stack block `func_actor_403000_8013C864` takes each
/// frame: `d` and `dist` are the player's offset from the model and its length
/// (even frames), `to`/`from` the two world positions handed to `func_800E0308`
/// as the line-of-sight segment (odd frames), `ofs` the flare offset passed to
/// `func_actor_403000_801327B0`.
typedef struct Actor403000UpdateScratch {
    /* 0x00 */ VECTOR  d;
    /* 0x10 */ byte    pad_10[0x8];
    /* 0x18 */ SVECTOR to;
    /* 0x20 */ SVECTOR from;
    /* 0x28 */ SVECTOR ofs;
    /* 0x30 */ s32     dist;
    /* 0x34 */ byte    pad_34[0x4];
} Actor403000UpdateScratch;
STATIC_ASSERT_SIZEOF(Actor403000UpdateScratch, 0x38);

/// The animation-state handlers `func_actor_403000_8013C864` copies onto its
/// stack and calls through, indexed by `Actor403000Work::field_0`.
typedef struct Actor403000StateTable {
    TaskFunc funcs[35];
} Actor403000StateTable;
static const Actor403000StateTable D_actor_403000_80131F44;

/// Waypoint grid for `func_actor_403000_80134204`: two rows of five indices
/// (row by `coord.t[2]`, column by `coord.t[0]` band), each one less than the
/// `D_actor_403000_80158CE0` entry it selects.
extern u8 D_actor_403000_80158D48[];

/// The push message `func_actor_403000_801384E8` keeps resending to the
/// player.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    GameActorMoveBy value;
    u8              retained[12];
} Actor403000Storage8DB0;
STATIC_ASSERT_SIZEOF(Actor403000Storage8DB0, 32);

/// The grab message `func_actor_403000_801386E8` sends as 0x3E9.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    ActorTransform value;
    u8             retained[8];
} Actor403000Storage8D90;
STATIC_ASSERT_SIZEOF(Actor403000Storage8D90, 32);

/// Pairs of hit-effect vectors `func_actor_403000_80134910` picks from by
/// turn magnitude; `pad` indexes the display object's coordinate parts.
extern SVECTOR D_actor_403000_80158C48[];

/// The overlay's pose table: 8-byte records of three halfwords at 0x0/0x2/0x4
/// plus padding, i.e. `SVECTOR`s. Indexed by the low signed halfword of the
/// caller's id -- `func_actor_403000_8013ACBC` scales a byte id by 8 into it
/// the same way -- so a record's `vx`/`vy`/`vz` are the vector an actor's
/// handlers copy out of it. Lives in the overlay's trailing data region.
extern SVECTOR D_actor_403000_80158CE0[];

/// Four trigger points (`vx`/`vz` used) `func_actor_403000_80134E00` measures
/// the display object against, one per bit of `GameFlag_GetNibble(0xE2)`.
extern SVECTOR D_actor_403000_80158D64[];

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

/// Turn joint `coord` by `yaw` about Y in view space, keeping it expressed in
/// its parent's frame.

/// Tick the work block's animation playback: state `field_AC0` 1 advances
/// `field_AC2` until it catches up with the requested `field_AC6`, copying
/// `field_ACA` into the four display nodes' clip slot as it goes.
static void func_actor_403000_80133AF8(Task* arg0);

static s32 func_actor_403000_80134204(GfxCoord* coord);

/// Step `coord` by the movement the first `count` records of `recs` resolve
/// to; returns whether the actor moved on X or Z.

/// Copy `placement` onto the actor's root coordinate (Y then X then Z) and
/// cache the resulting heading in `Actor403000Work::yaw`.
s32 func_actor_403000_8013D364(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);

/// Latch the requested animation and restart the animation state machine.
s32 func_actor_403000_8013D464(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3);

/// Report whether the work block's five-entry record run holds a live entry:
/// the walk stops at the first empty `key` and answers 1 if any record it
/// passed carried the 0x10 kind bits.
static s16 func_actor_403000_8013D48C(Task* task);

/// `Task::exitCallback` installed by the spawn handler, for the teardown path
/// where the enemy was created: hand the four collision spheres' bodies back to
/// `Gp_UnlinkObj`, drop the enemy's `recs` slot, then let `enemyDestroy`
/// free the enemy and the task.
static void func_actor_403000_8013D4F4(Task* task);

/// Copy the `vx`/`vy`/`vz` of record `arg1` of the pose table into `arg0`.
static void func_actor_403000_8013D564(SVECTOR* arg0, s32 arg1);

static void func_actor_403000_8013D5F8(Task* arg0);

/// Per-frame update for the actor once its work block exists: on the frame
/// `field_4` is set, reinstate the display object's buffers and restart the
/// animation state machine on clip 0x10, then tick `field_6` and the playback
/// state, and when `field_60` has `ANIMATION_SLOT_SETTLED`, raise
/// `field_FD2`/`field_FD3` and move the state machine to state 2.
static void func_actor_403000_8013D850(Task* arg0);

/// Per-frame countdown: on the frame `field_4` is set, reload the `field_6`
/// tick from a fresh `gRandomLcgState` draw masked to 0xA..0x19, then decrement
/// it. When the tick underflows and the enemy still has HP left
/// (`Enemy::hp`), the animation state `field_0` is set to 0x13.
static void func_actor_403000_8013D910(Task* arg0);

extern EnemyParams   D_actor_403000_8013DA00;
extern EnemyParams   D_actor_403000_8013DA10;
extern AnimationSet* D_actor_403000_80158B50[46];
extern AnimationSet* D_actor_403000_80158C08[8];
extern AnimationSet* D_actor_403000_80158C28[8];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_403000_80158CA8[7];
// Preserve the unreferenced zero tail; fields versus alignment is unresolved.
typedef struct {
    GameActorButtonPressHold value;
    u8                       retained[8];
} Actor403000DelayStorage;
STATIC_ASSERT_SIZEOF(Actor403000DelayStorage, 32);
extern Actor403000Storage8D90 D_actor_403000_80158D90;

extern Actor403000Storage8DB0 D_actor_403000_80158DB0;

extern Actor403000DelayStorage D_actor_403000_80158DD0;

/// Trail history `func_actor_403000_801330D4` shifts down one slot per call,
/// storing the newest position in slot 0.
extern SVECTOR D_actor_403000_80158DF0[18];
extern s8      D_actor_403000_80158364[];

extern ActorCommand D_actor_403000_80158D8C;

/// Integer part of the last movement step `ActorContact_PushContact`
/// applied to the actor's root coordinate.
extern SVECTOR ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

static void func_actor_403000_80132AE0(GfxCoord* coord);
static void func_actor_403000_80134F44(Task* arg0);
static void func_actor_403000_8013D72C(Task* arg0);
static void func_actor_403000_8013D648(Task* arg0);
static void func_actor_403000_801377C8(Task* arg0);
static void func_actor_403000_8013B74C(Task* arg0);

static AnimationSet _gActor403000Animation139B8;
static AnimationSet _gActor403000Animation14AB4;
static AnimationSet _gActor403000Animation15528;
static AnimationSet _gActor403000Animation1607C;
static AnimationSet _gActor403000Animation16A1C;
static AnimationSet _gActor403000Animation17038;
static AnimationSet _gActor403000Animation17694;
static AnimationSet _gActor403000Animation189B4;
static AnimationSet _gActor403000Animation19984;
static AnimationSet _gActor403000Animation1A69C;
static AnimationSet _gActor403000Animation1B000;
static AnimationSet _gActor403000Animation1BABC;
static AnimationSet _gActor403000Animation1C69C;
static AnimationSet _gActor403000Animation1CE64;
static AnimationSet _gActor403000Animation1D9E8;
static AnimationSet _gActor403000Animation1E058;
static AnimationSet _gActor403000Animation1EAC4;
static AnimationSet _gActor403000Animation20714;
static AnimationSet _gActor403000Animation21A1C;
static AnimationSet _gActor403000Animation22500;
static AnimationSet _gActor403000Animation229E8;
static AnimationSet _gActor403000Animation23230;
static AnimationSet _gActor403000Animation239CC;
static AnimationSet _gActor403000Animation241D8;
static AnimationSet _gActor403000Animation249EC;
static AnimationSet _gActor403000Animation24BD0;
static AnimationSet _gActor403000Animation250C4;
static AnimationSet _gActor403000Animation25484;
static AnimationSet _gActor403000Animation25B24;
static AnimationSet _gActor403000Animation26334;
static AnimationSet _gActor403000Animation2651C;
static TmdSource    _gActor403000BlizzardChaserBody;
s32                 func_actor_403000_801324EC(Task*, s32, Actor403000Event*, s32);
s32                 func_actor_403000_8013D268(Task*, s32, s32, s32);
s32                 func_actor_403000_8013D324(Task*, s32, s32, s32);
s32                 func_actor_403000_8013D364(Task* task, s32 msgId, ActorTransform* placement, s32 arg3);
s32                 func_actor_403000_8013D464(Task*, s32, AnimationPlayRequest*, s32);
static void         func_actor_403000_8013D59C(Task*);
s32                 func_actor_403000_8013D260(Task*, s32, s32, s32);

DamageAttack D_actor_403000_8013D9E0[4] = {
    { 30, 7 },
    { 20, 7 },
    { 20, 7 },
    { 25, 0 },
};

DamageAttack D_actor_403000_8013D9F0[4] = {
    { 20, 3 },
    { 12, 7 },
    { 12, 7 },
    { 18, 0 },
};

EnemyParams D_actor_403000_8013DA00 = { D_actor_403000_8013D9E0, 500, 300, 200, 10, 100, 6, 100, 0 };

EnemyParams D_actor_403000_8013DA10 = { D_actor_403000_8013D9F0, 2500, 500, 300, 30, 100, 3, 100, 20 };

static TmdBone _gActor403000BlizzardChaserBodySkeleton[24] = {
#include "assets/blizzard_chaser_body_skeleton.inc"
};

static u32 _gActor403000BlizzardChaserBodyPartVerts[24] = {
#include "assets/blizzard_chaser_body_partVerts.inc"
};

static SVECTOR _gActor403000BlizzardChaserBodyVerts[398] = {
#include "assets/blizzard_chaser_body_verts.inc"
};

static SVECTOR _gActor403000BlizzardChaserBodyNormals[410] = {
#include "assets/blizzard_chaser_body_normals.inc"
};

static u32 _gActor403000BlizzardChaserBodyStream[5889] = {
#include "assets/blizzard_chaser_body_stream.inc"
};

static TmdSource _gActor403000BlizzardChaserBody = {
    0,
    29692,
    10264,
    24,
    _gActor403000BlizzardChaserBodyPartVerts,
    _gActor403000BlizzardChaserBodyVerts,
    _gActor403000BlizzardChaserBodyNormals,
    _gActor403000BlizzardChaserBodySkeleton,
    _gActor403000BlizzardChaserBodyStream,
};

static AnimationPackedPose _gActor403000Animation139B8Bank1[6] = {
#include "assets/actor_403000_animation_139B8_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation139B8Bank4[73] = {
#include "assets/actor_403000_animation_139B8_bank4.inc"
};

static AnimationRecord _gActor403000Animation139B8Records[189] = {
#include "assets/actor_403000_animation_139B8_records.inc"
};

static u16 _gActor403000Animation139B8Indices[24] = {
#include "assets/actor_403000_animation_139B8_indices.inc"
};

static AnimationSet _gActor403000Animation139B8 = {
    _gActor403000Animation139B8Records,
    _gActor403000Animation139B8Indices,
    { NULL, _gActor403000Animation139B8Bank1, NULL, NULL, _gActor403000Animation139B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation14AB4Bank1[49] = {
#include "assets/actor_403000_animation_14AB4_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation14AB4Bank4[361] = {
#include "assets/actor_403000_animation_14AB4_bank4.inc"
};

static AnimationRecord _gActor403000Animation14AB4Records[557] = {
#include "assets/actor_403000_animation_14AB4_records.inc"
};

static u16 _gActor403000Animation14AB4Indices[24] = {
#include "assets/actor_403000_animation_14AB4_indices.inc"
};

static AnimationSet _gActor403000Animation14AB4 = {
    _gActor403000Animation14AB4Records,
    _gActor403000Animation14AB4Indices,
    { NULL, _gActor403000Animation14AB4Bank1, NULL, NULL, _gActor403000Animation14AB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation15528Bank1[28] = {
#include "assets/actor_403000_animation_15528_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation15528Bank4[211] = {
#include "assets/actor_403000_animation_15528_bank4.inc"
};

static AnimationRecord _gActor403000Animation15528Records[352] = {
#include "assets/actor_403000_animation_15528_records.inc"
};

static u16 _gActor403000Animation15528Indices[24] = {
#include "assets/actor_403000_animation_15528_indices.inc"
};

static AnimationSet _gActor403000Animation15528 = {
    _gActor403000Animation15528Records,
    _gActor403000Animation15528Indices,
    { NULL, _gActor403000Animation15528Bank1, NULL, NULL, _gActor403000Animation15528Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1607CBank1[28] = {
#include "assets/actor_403000_animation_1607C_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1607CBank4[257] = {
#include "assets/actor_403000_animation_1607C_bank4.inc"
};

static AnimationRecord _gActor403000Animation1607CRecords[362] = {
#include "assets/actor_403000_animation_1607C_records.inc"
};

static u16 _gActor403000Animation1607CIndices[24] = {
#include "assets/actor_403000_animation_1607C_indices.inc"
};

static AnimationSet _gActor403000Animation1607C = {
    _gActor403000Animation1607CRecords,
    _gActor403000Animation1607CIndices,
    { NULL, _gActor403000Animation1607CBank1, NULL, NULL, _gActor403000Animation1607CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation16A1CBank1[14] = {
#include "assets/actor_403000_animation_16A1C_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation16A1CBank4[192] = {
#include "assets/actor_403000_animation_16A1C_bank4.inc"
};

static AnimationRecord _gActor403000Animation16A1CRecords[360] = {
#include "assets/actor_403000_animation_16A1C_records.inc"
};

static u16 _gActor403000Animation16A1CIndices[24] = {
#include "assets/actor_403000_animation_16A1C_indices.inc"
};

static AnimationSet _gActor403000Animation16A1C = {
    _gActor403000Animation16A1CRecords,
    _gActor403000Animation16A1CIndices,
    { NULL, _gActor403000Animation16A1CBank1, NULL, NULL, _gActor403000Animation16A1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation17038Bank1[10] = {
#include "assets/actor_403000_animation_17038_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation17038Bank4[142] = {
#include "assets/actor_403000_animation_17038_bank4.inc"
};

static AnimationRecord _gActor403000Animation17038Records[197] = {
#include "assets/actor_403000_animation_17038_records.inc"
};

static u16 _gActor403000Animation17038Indices[24] = {
#include "assets/actor_403000_animation_17038_indices.inc"
};

static AnimationSet _gActor403000Animation17038 = {
    _gActor403000Animation17038Records,
    _gActor403000Animation17038Indices,
    { NULL, _gActor403000Animation17038Bank1, NULL, NULL, _gActor403000Animation17038Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation17694Bank1[14] = {
#include "assets/actor_403000_animation_17694_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation17694Bank4[140] = {
#include "assets/actor_403000_animation_17694_bank4.inc"
};

static AnimationRecord _gActor403000Animation17694Records[203] = {
#include "assets/actor_403000_animation_17694_records.inc"
};

static u16 _gActor403000Animation17694Indices[24] = {
#include "assets/actor_403000_animation_17694_indices.inc"
};

static AnimationSet _gActor403000Animation17694 = {
    _gActor403000Animation17694Records,
    _gActor403000Animation17694Indices,
    { NULL, _gActor403000Animation17694Bank1, NULL, NULL, _gActor403000Animation17694Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation189B4Bank1[39] = {
#include "assets/actor_403000_animation_189B4_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation189B4Bank4[474] = {
#include "assets/actor_403000_animation_189B4_bank4.inc"
};

static AnimationRecord _gActor403000Animation189B4Records[611] = {
#include "assets/actor_403000_animation_189B4_records.inc"
};

static u16 _gActor403000Animation189B4Indices[24] = {
#include "assets/actor_403000_animation_189B4_indices.inc"
};

static AnimationSet _gActor403000Animation189B4 = {
    _gActor403000Animation189B4Records,
    _gActor403000Animation189B4Indices,
    { NULL, _gActor403000Animation189B4Bank1, NULL, NULL, _gActor403000Animation189B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation19984Bank1[26] = {
#include "assets/actor_403000_animation_19984_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation19984Bank4[401] = {
#include "assets/actor_403000_animation_19984_bank4.inc"
};

static AnimationRecord _gActor403000Animation19984Records[511] = {
#include "assets/actor_403000_animation_19984_records.inc"
};

static u16 _gActor403000Animation19984Indices[24] = {
#include "assets/actor_403000_animation_19984_indices.inc"
};

static AnimationSet _gActor403000Animation19984 = {
    _gActor403000Animation19984Records,
    _gActor403000Animation19984Indices,
    { NULL, _gActor403000Animation19984Bank1, NULL, NULL, _gActor403000Animation19984Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1A69CBank1[21] = {
#include "assets/actor_403000_animation_1A69C_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1A69CBank4[325] = {
#include "assets/actor_403000_animation_1A69C_bank4.inc"
};

static AnimationRecord _gActor403000Animation1A69CRecords[428] = {
#include "assets/actor_403000_animation_1A69C_records.inc"
};

static u16 _gActor403000Animation1A69CIndices[24] = {
#include "assets/actor_403000_animation_1A69C_indices.inc"
};

static AnimationSet _gActor403000Animation1A69C = {
    _gActor403000Animation1A69CRecords,
    _gActor403000Animation1A69CIndices,
    { NULL, _gActor403000Animation1A69CBank1, NULL, NULL, _gActor403000Animation1A69CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1B000Bank1[24] = {
#include "assets/actor_403000_animation_1B000_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1B000Bank4[220] = {
#include "assets/actor_403000_animation_1B000_bank4.inc"
};

static AnimationRecord _gActor403000Animation1B000Records[287] = {
#include "assets/actor_403000_animation_1B000_records.inc"
};

static u16 _gActor403000Animation1B000Indices[24] = {
#include "assets/actor_403000_animation_1B000_indices.inc"
};

static AnimationSet _gActor403000Animation1B000 = {
    _gActor403000Animation1B000Records,
    _gActor403000Animation1B000Indices,
    { NULL, _gActor403000Animation1B000Bank1, NULL, NULL, _gActor403000Animation1B000Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1BABCBank1[23] = {
#include "assets/actor_403000_animation_1BABC_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1BABCBank4[258] = {
#include "assets/actor_403000_animation_1BABC_bank4.inc"
};

static AnimationRecord _gActor403000Animation1BABCRecords[338] = {
#include "assets/actor_403000_animation_1BABC_records.inc"
};

static u16 _gActor403000Animation1BABCIndices[24] = {
#include "assets/actor_403000_animation_1BABC_indices.inc"
};

static AnimationSet _gActor403000Animation1BABC = {
    _gActor403000Animation1BABCRecords,
    _gActor403000Animation1BABCIndices,
    { NULL, _gActor403000Animation1BABCBank1, NULL, NULL, _gActor403000Animation1BABCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1C69CBank1[23] = {
#include "assets/actor_403000_animation_1C69C_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1C69CBank4[296] = {
#include "assets/actor_403000_animation_1C69C_bank4.inc"
};

static AnimationRecord _gActor403000Animation1C69CRecords[373] = {
#include "assets/actor_403000_animation_1C69C_records.inc"
};

static u16 _gActor403000Animation1C69CIndices[24] = {
#include "assets/actor_403000_animation_1C69C_indices.inc"
};

static AnimationSet _gActor403000Animation1C69C = {
    _gActor403000Animation1C69CRecords,
    _gActor403000Animation1C69CIndices,
    { NULL, _gActor403000Animation1C69CBank1, NULL, NULL, _gActor403000Animation1C69CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1CE64Bank1[11] = {
#include "assets/actor_403000_animation_1CE64_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1CE64Bank4[184] = {
#include "assets/actor_403000_animation_1CE64_bank4.inc"
};

static AnimationRecord _gActor403000Animation1CE64Records[259] = {
#include "assets/actor_403000_animation_1CE64_records.inc"
};

static u16 _gActor403000Animation1CE64Indices[24] = {
#include "assets/actor_403000_animation_1CE64_indices.inc"
};

static AnimationSet _gActor403000Animation1CE64 = {
    _gActor403000Animation1CE64Records,
    _gActor403000Animation1CE64Indices,
    { NULL, _gActor403000Animation1CE64Bank1, NULL, NULL, _gActor403000Animation1CE64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1D9E8Bank1[16] = {
#include "assets/actor_403000_animation_1D9E8_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1D9E8Bank4[288] = {
#include "assets/actor_403000_animation_1D9E8_bank4.inc"
};

static AnimationRecord _gActor403000Animation1D9E8Records[379] = {
#include "assets/actor_403000_animation_1D9E8_records.inc"
};

static u16 _gActor403000Animation1D9E8Indices[24] = {
#include "assets/actor_403000_animation_1D9E8_indices.inc"
};

static AnimationSet _gActor403000Animation1D9E8 = {
    _gActor403000Animation1D9E8Records,
    _gActor403000Animation1D9E8Indices,
    { NULL, _gActor403000Animation1D9E8Bank1, NULL, NULL, _gActor403000Animation1D9E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1E058Bank1[7] = {
#include "assets/actor_403000_animation_1E058_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1E058Bank4[122] = {
#include "assets/actor_403000_animation_1E058_bank4.inc"
};

static AnimationRecord _gActor403000Animation1E058Records[247] = {
#include "assets/actor_403000_animation_1E058_records.inc"
};

static u16 _gActor403000Animation1E058Indices[24] = {
#include "assets/actor_403000_animation_1E058_indices.inc"
};

static AnimationSet _gActor403000Animation1E058 = {
    _gActor403000Animation1E058Records,
    _gActor403000Animation1E058Indices,
    { NULL, _gActor403000Animation1E058Bank1, NULL, NULL, _gActor403000Animation1E058Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation1EAC4Bank1[15] = {
#include "assets/actor_403000_animation_1EAC4_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation1EAC4Bank4[272] = {
#include "assets/actor_403000_animation_1EAC4_bank4.inc"
};

static AnimationRecord _gActor403000Animation1EAC4Records[328] = {
#include "assets/actor_403000_animation_1EAC4_records.inc"
};

static u16 _gActor403000Animation1EAC4Indices[24] = {
#include "assets/actor_403000_animation_1EAC4_indices.inc"
};

static AnimationSet _gActor403000Animation1EAC4 = {
    _gActor403000Animation1EAC4Records,
    _gActor403000Animation1EAC4Indices,
    { NULL, _gActor403000Animation1EAC4Bank1, NULL, NULL, _gActor403000Animation1EAC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation20714Bank1[47] = {
#include "assets/actor_403000_animation_20714_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation20714Bank4[726] = {
#include "assets/actor_403000_animation_20714_bank4.inc"
};

static AnimationRecord _gActor403000Animation20714Records[923] = {
#include "assets/actor_403000_animation_20714_records.inc"
};

static u16 _gActor403000Animation20714Indices[24] = {
#include "assets/actor_403000_animation_20714_indices.inc"
};

static AnimationSet _gActor403000Animation20714 = {
    _gActor403000Animation20714Records,
    _gActor403000Animation20714Indices,
    { NULL, _gActor403000Animation20714Bank1, NULL, NULL, _gActor403000Animation20714Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation21A1CBank1[49] = {
#include "assets/actor_403000_animation_21A1C_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation21A1CBank4[476] = {
#include "assets/actor_403000_animation_21A1C_bank4.inc"
};

static AnimationRecord _gActor403000Animation21A1CRecords[575] = {
#include "assets/actor_403000_animation_21A1C_records.inc"
};

static u16 _gActor403000Animation21A1CIndices[20] = {
#include "assets/actor_403000_animation_21A1C_indices.inc"
};

static AnimationSet _gActor403000Animation21A1C = {
    _gActor403000Animation21A1CRecords,
    _gActor403000Animation21A1CIndices,
    { NULL, _gActor403000Animation21A1CBank1, NULL, NULL, _gActor403000Animation21A1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation22500Bank1[20] = {
#include "assets/actor_403000_animation_22500_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation22500Bank4[283] = {
#include "assets/actor_403000_animation_22500_bank4.inc"
};

static AnimationRecord _gActor403000Animation22500Records[334] = {
#include "assets/actor_403000_animation_22500_records.inc"
};

static u16 _gActor403000Animation22500Indices[20] = {
#include "assets/actor_403000_animation_22500_indices.inc"
};

static AnimationSet _gActor403000Animation22500 = {
    _gActor403000Animation22500Records,
    _gActor403000Animation22500Indices,
    { NULL, _gActor403000Animation22500Bank1, NULL, NULL, _gActor403000Animation22500Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation229E8Bank1[10] = {
#include "assets/actor_403000_animation_229E8_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation229E8Bank4[110] = {
#include "assets/actor_403000_animation_229E8_bank4.inc"
};

static AnimationRecord _gActor403000Animation229E8Records[154] = {
#include "assets/actor_403000_animation_229E8_records.inc"
};

static u16 _gActor403000Animation229E8Indices[20] = {
#include "assets/actor_403000_animation_229E8_indices.inc"
};

static AnimationSet _gActor403000Animation229E8 = {
    _gActor403000Animation229E8Records,
    _gActor403000Animation229E8Indices,
    { NULL, _gActor403000Animation229E8Bank1, NULL, NULL, _gActor403000Animation229E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation23230Bank1[21] = {
#include "assets/actor_403000_animation_23230_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation23230Bank4[193] = {
#include "assets/actor_403000_animation_23230_bank4.inc"
};

static AnimationRecord _gActor403000Animation23230Records[254] = {
#include "assets/actor_403000_animation_23230_records.inc"
};

static u16 _gActor403000Animation23230Indices[20] = {
#include "assets/actor_403000_animation_23230_indices.inc"
};

static AnimationSet _gActor403000Animation23230 = {
    _gActor403000Animation23230Records,
    _gActor403000Animation23230Indices,
    { NULL, _gActor403000Animation23230Bank1, NULL, NULL, _gActor403000Animation23230Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation239CCBank1[20] = {
#include "assets/actor_403000_animation_239CC_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation239CCBank4[172] = {
#include "assets/actor_403000_animation_239CC_bank4.inc"
};

static AnimationRecord _gActor403000Animation239CCRecords[235] = {
#include "assets/actor_403000_animation_239CC_records.inc"
};

static u16 _gActor403000Animation239CCIndices[20] = {
#include "assets/actor_403000_animation_239CC_indices.inc"
};

static AnimationSet _gActor403000Animation239CC = {
    _gActor403000Animation239CCRecords,
    _gActor403000Animation239CCIndices,
    { NULL, _gActor403000Animation239CCBank1, NULL, NULL, _gActor403000Animation239CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation241D8Bank1[15] = {
#include "assets/actor_403000_animation_241D8_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation241D8Bank4[206] = {
#include "assets/actor_403000_animation_241D8_bank4.inc"
};

static AnimationRecord _gActor403000Animation241D8Records[244] = {
#include "assets/actor_403000_animation_241D8_records.inc"
};

static u16 _gActor403000Animation241D8Indices[20] = {
#include "assets/actor_403000_animation_241D8_indices.inc"
};

static AnimationSet _gActor403000Animation241D8 = {
    _gActor403000Animation241D8Records,
    _gActor403000Animation241D8Indices,
    { NULL, _gActor403000Animation241D8Bank1, NULL, NULL, _gActor403000Animation241D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation249ECBank1[14] = {
#include "assets/actor_403000_animation_249EC_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation249ECBank4[207] = {
#include "assets/actor_403000_animation_249EC_bank4.inc"
};

static AnimationRecord _gActor403000Animation249ECRecords[248] = {
#include "assets/actor_403000_animation_249EC_records.inc"
};

static u16 _gActor403000Animation249ECIndices[20] = {
#include "assets/actor_403000_animation_249EC_indices.inc"
};

static AnimationSet _gActor403000Animation249EC = {
    _gActor403000Animation249ECRecords,
    _gActor403000Animation249ECIndices,
    { NULL, _gActor403000Animation249ECBank1, NULL, NULL, _gActor403000Animation249ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation24BD0Bank1[2] = {
#include "assets/actor_403000_animation_24BD0_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation24BD0Bank4[21] = {
#include "assets/actor_403000_animation_24BD0_bank4.inc"
};

static AnimationRecord _gActor403000Animation24BD0Records[72] = {
#include "assets/actor_403000_animation_24BD0_records.inc"
};

static u16 _gActor403000Animation24BD0Indices[24] = {
#include "assets/actor_403000_animation_24BD0_indices.inc"
};

static AnimationSet _gActor403000Animation24BD0 = {
    _gActor403000Animation24BD0Records,
    _gActor403000Animation24BD0Indices,
    { NULL, _gActor403000Animation24BD0Bank1, NULL, NULL, _gActor403000Animation24BD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation250C4Bank1[8] = {
#include "assets/actor_403000_animation_250C4_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation250C4Bank4[103] = {
#include "assets/actor_403000_animation_250C4_bank4.inc"
};

static AnimationRecord _gActor403000Animation250C4Records[168] = {
#include "assets/actor_403000_animation_250C4_records.inc"
};

static u16 _gActor403000Animation250C4Indices[24] = {
#include "assets/actor_403000_animation_250C4_indices.inc"
};

static AnimationSet _gActor403000Animation250C4 = {
    _gActor403000Animation250C4Records,
    _gActor403000Animation250C4Indices,
    { NULL, _gActor403000Animation250C4Bank1, NULL, NULL, _gActor403000Animation250C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation25484Bank1[6] = {
#include "assets/actor_403000_animation_25484_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation25484Bank4[78] = {
#include "assets/actor_403000_animation_25484_bank4.inc"
};

static AnimationRecord _gActor403000Animation25484Records[122] = {
#include "assets/actor_403000_animation_25484_records.inc"
};

static u16 _gActor403000Animation25484Indices[24] = {
#include "assets/actor_403000_animation_25484_indices.inc"
};

static AnimationSet _gActor403000Animation25484 = {
    _gActor403000Animation25484Records,
    _gActor403000Animation25484Indices,
    { NULL, _gActor403000Animation25484Bank1, NULL, NULL, _gActor403000Animation25484Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation25B24Bank1[10] = {
#include "assets/actor_403000_animation_25B24_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation25B24Bank4[156] = {
#include "assets/actor_403000_animation_25B24_bank4.inc"
};

static AnimationRecord _gActor403000Animation25B24Records[216] = {
#include "assets/actor_403000_animation_25B24_records.inc"
};

static u16 _gActor403000Animation25B24Indices[24] = {
#include "assets/actor_403000_animation_25B24_indices.inc"
};

static AnimationSet _gActor403000Animation25B24 = {
    _gActor403000Animation25B24Records,
    _gActor403000Animation25B24Indices,
    { NULL, _gActor403000Animation25B24Bank1, NULL, NULL, _gActor403000Animation25B24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation26334Bank1[13] = {
#include "assets/actor_403000_animation_26334_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation26334Bank4[171] = {
#include "assets/actor_403000_animation_26334_bank4.inc"
};

static AnimationRecord _gActor403000Animation26334Records[284] = {
#include "assets/actor_403000_animation_26334_records.inc"
};

static u16 _gActor403000Animation26334Indices[24] = {
#include "assets/actor_403000_animation_26334_indices.inc"
};

static AnimationSet _gActor403000Animation26334 = {
    _gActor403000Animation26334Records,
    _gActor403000Animation26334Indices,
    { NULL, _gActor403000Animation26334Bank1, NULL, NULL, _gActor403000Animation26334Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403000Animation2651CBank1[2] = {
#include "assets/actor_403000_animation_2651C_bank1.inc"
};

static AnimationPackedRotation _gActor403000Animation2651CBank4[22] = {
#include "assets/actor_403000_animation_2651C_bank4.inc"
};

static AnimationRecord _gActor403000Animation2651CRecords[72] = {
#include "assets/actor_403000_animation_2651C_records.inc"
};

static u16 _gActor403000Animation2651CIndices[24] = {
#include "assets/actor_403000_animation_2651C_indices.inc"
};

static AnimationSet _gActor403000Animation2651C = {
    _gActor403000Animation2651CRecords,
    _gActor403000Animation2651CIndices,
    { NULL, _gActor403000Animation2651CBank1, NULL, NULL, _gActor403000Animation2651CBank4, NULL, NULL, NULL },
};

s8 D_actor_403000_80158364[2028] = {
    0,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    8,
    8,
    8,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    7,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    7,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    7,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

AnimationSet* D_actor_403000_80158B50[46] = {
    &_gActor403000Animation139B8,
    &_gActor403000Animation14AB4,
    &_gActor403000Animation15528,
    &_gActor403000Animation1607C,
    &_gActor403000Animation16A1C,
    &_gActor403000Animation17038,
    &_gActor403000Animation17694,
    &_gActor403000Animation189B4,
    &_gActor403000Animation19984,
    &_gActor403000Animation1A69C,
    &_gActor403000Animation1B000,
    &_gActor403000Animation1BABC,
    &_gActor403000Animation1C69C,
    &_gActor403000Animation1CE64,
    &_gActor403000Animation1D9E8,
    &_gActor403000Animation1E058,
    &_gActor403000Animation1EAC4,
    &_gActor403000Animation20714,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gActor403000Animation24BD0,
    &_gActor403000Animation250C4,
    &_gActor403000Animation25484,
    &_gActor403000Animation25B24,
    &_gActor403000Animation2651C,
    &_gActor403000Animation26334,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

AnimationSet* D_actor_403000_80158C08[8] = {
    NULL,
    &_gActor403000Animation21A1C,
    &_gActor403000Animation22500,
    &_gActor403000Animation23230,
    &_gActor403000Animation241D8,
    &_gActor403000Animation229E8,
    &_gActor403000Animation249EC,
    NULL,
};

AnimationSet* D_actor_403000_80158C28[8] = {
    NULL,
    &_gActor403000Animation21A1C,
    &_gActor403000Animation22500,
    &_gActor403000Animation239CC,
    &_gActor403000Animation249EC,
    &_gActor403000Animation229E8,
    &_gActor403000Animation249EC,
    NULL,
};

SVECTOR D_actor_403000_80158C48[12] = {
    { 60, -12, 30, 2 },
    { -50, -130, 29, 2 },
    { 20, -70, 25, 2 },
    { -30, -65, 25, 2 },
    { 60, -120, 30, 2 },
    { 20, -20, -5, 2 },
    { -15, -50, 0, 2 },
    { 2, 10, -15, 2 },
    { 14, 0, 0, 7 },
    { 25, 0, 0, 2 },
    { -14, 0, 0, 9 },
    { -25, 0, 0, 2 },
};

TaskMessageEntry D_actor_403000_80158CA8[7] = {
    { 2015, func_actor_403000_8013D260 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_403000_8013D268 },
    { ACTOR_MESSAGE_IS_PRESENT, func_actor_403000_8013D324 },
    { ACTOR_MESSAGE_PLACE, func_actor_403000_8013D364 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_403000_801324EC },
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_403000_8013D464 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_actor_403000_80158CE0[13] = {
    { 0x445C, 2, 2100, 0 },
    { 0x33C2, 2, 2100, 0 },
    { 8900, 2, 2100, 0 },
    { 5100, 2, 2100, 0 },
    { 500, 2, 2100, 0 },
    { 500, 2, 6700, 0 },
    { 5100, 2, 6700, 0 },
    { 8900, 2, 6700, 0 },
    { 0x33C2, 2, 6700, 0 },
    { 0x445C, 2, 6700, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
};

u8 D_actor_403000_80158D48[16] = {
    0,
    1,
    2,
    3,
    4,
    9,
    8,
    7,
    6,
    5,
    0,
    0,
    1,
    0,
    3,
    2,
};

TaskDesc D_actor_403000_80158D58 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_403000_8013D59C, { .model = &_gActor403000BlizzardChaserBody } };

SVECTOR D_actor_403000_80158D64[4] = {
    { 4550, 0, 310, 0 },
    { 0x3566, 0, 330, 0 },
    { 4340, 0, 8710, 0 },
    { 0x3552, 0, 8710, 0 },
};

SVECTOR ActorContact_ScratchPosition = { 0 };

ActorCommand D_actor_403000_80158D8C = { 0 };

Actor403000Storage8D90 D_actor_403000_80158D90;

Actor403000Storage8DB0 D_actor_403000_80158DB0;

Actor403000DelayStorage D_actor_403000_80158DD0;

SVECTOR D_actor_403000_80158DF0[18];

static void                func_actor_403000_801327B0(GfxCoord* coord, SVECTOR* pos, s32 arg2);
static void                func_actor_403000_801330D4(GfxCoord* parent);
static void                func_actor_403000_801332E8(Task* arg0);
static void                func_actor_403000_80133444(Task* arg0);
static void                func_actor_403000_801336B4(Task* arg0);
static s32                 func_actor_403000_801337E0(Task* arg0, Actor403000Work* work);
static s32                 func_actor_403000_80133FC0(Task* arg0, s16 arg1, s16 arg2);
static void                func_actor_403000_801343B8(Enemy* arg0, Task* arg1);
static void                func_actor_403000_80134910(Task* arg0, s16 arg1, s32 arg2);
static s32                 func_actor_403000_80134E00(Task* arg0);
static inline s32          func_actor_403000_FindHit(SVECTOR* pos, WorldCollisionContact* records);
static inline s16          func_actor_403000_WrapAngle(s16 angle);
static inline void         func_actor_403000_PlaySound(Task* arg0, Enemy* enemy, s32 id);
static void                func_actor_403000_80135F08(Task* arg0);
static __inline__ void     Actor403000_FaceScale(GfxCoord* coord, s16 sy);
static void                func_actor_403000_8013603C(Task* arg0);
static void                func_actor_403000_801365D0(Task* arg0);
static void                func_actor_403000_80136B14(Task* arg0);
static void                func_actor_403000_80136D68(Task* arg0);
static __inline__ void     Actor403000_ScaleVec(SVECTOR* v, u16 k);
static __inline__ SVECTOR* Actor403000_PushVec(void);
static __inline__ void     Actor403000_PopVec(void);
static inline s8           Actor403000_Cell(GfxCoord* coord);
static void                func_actor_403000_80137084(Task* arg0);
static void                func_actor_403000_801384E8(Task* arg0);
static void                func_actor_403000_801386E8(Task* arg0);
static void                func_actor_403000_80138DB0(Task* arg0);
static void                func_actor_403000_801399A0(Task* arg0);
static void                func_actor_403000_80139AE0(Task* arg0);
static void                func_actor_403000_8013A08C(Task* arg0);
static void                func_actor_403000_8013A678(Task* arg0);
static void                func_actor_403000_8013ACBC(Task* arg0);
static void                func_actor_403000_8013B238(Task* arg0);
static __inline__ s32      Actor403000_Outside(SVECTOR* v, s32 r);
static void                func_actor_403000_8013BDE0(Task* arg0);
static void                func_actor_403000_8013C050(Task* arg0);
static void                func_actor_403000_8013C2D4(Task* arg0);
static void                func_actor_403000_8013C864(Enemy* arg0, Task* arg1);
static s32                 func_actor_403000_8013D98C(s32 arg0);

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

s32 func_actor_403000_801324EC(Task* arg0, s32 arg1, Actor403000Event* arg2, s32 arg3)
{
    Actor403000Work* work  = arg0->work;
    Enemy*           enemy = arg0->spawnArg2.pointer;

    work->field_FA4 = arg2->b[0];
    work->field_FA5 = arg2->b[1];
    work->field_FA6 = arg2->b[2];
    if (arg2->w[0] == 0x204) {
        switch (arg2->w[1]) {
            case 0:
                enemy->hp     = 0;
                work->field_0 = 0;
                return 1;
            case 1:
                Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags = 0;
                enemy->param         = &D_actor_403000_8013DA00;
                enemy->hp            = D_actor_403000_8013DA00.hpMax;
                work->field_AC6      = 0x18;
                work->field_0        = 1;
                work->field_2        = -1;
                return 1;
            case 2:
                Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags = 0;
                enemy->param         = &D_actor_403000_8013DA00;
                enemy->hp            = D_actor_403000_8013DA00.hpMax;
                work->field_AC6      = 0x19;
                work->field_0        = 1;
                work->field_2        = -1;
                return 1;
            case 3:
                enemy->hp = 0;
                Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags = 0;
                work->field_AC6      = 0x1A;
                work->field_0        = 1;
                work->field_2        = -1;
                enemy->reactionFlags = 0;
                enemy->hp            = D_actor_403000_8013DA10.hpMax;
                enemy->param         = &D_actor_403000_8013DA10;
                return 1;
            case 4:
                Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags = 0;
                work->field_AC6      = 0x1B;
                work->field_0        = 1;
                work->field_2        = -1;
                return 1;
            case 5:
                work->field_0                      = 0x15;
                work->field_2                      = -1;
                arg0->extra.tmd->texturePageOffset = 2;
                arg0->extra.tmd->clutRowOffset     = 4;
                return 1;
            case 6:
                Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags = 0;
                enemy->hp            = D_actor_403000_8013DA10.hpMax;
                enemy->param         = &D_actor_403000_8013DA10;
                work->field_AC6      = 0x19;
                work->field_0        = 1;
                work->field_2        = -1;
                return 1;
            case 7:
                arg0->extra.tmd->texturePageOffset = 2;
                arg0->extra.tmd->clutRowOffset     = 4;
                work->field_0                      = 0x14;
                work->field_2                      = -1;
                return 1;
            case 10:
                Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                enemy->reactionFlags = 0;
                work->field_AC6      = 0x18;
                work->field_0        = 1;
                work->field_2        = -1;
                return 1;
            case 11:
                work->field_0   = 4;
                work->field_2   = -1;
                work->field_FD3 = -1;
                work->field_FD2 = 1;
                work->field_FD5 = 1;
                if (arg0->extra.tmd->texturePageOffset == 2) {
                    enemy->reactionFlags = 0;
                    enemy->hp            = D_actor_403000_8013DA10.hpMax;
                    enemy->param         = &D_actor_403000_8013DA10;
                }
                return 1;
            case 12:
                arg0->extra.tmd->texturePageOffset = 2;
                arg0->extra.tmd->clutRowOffset     = 4;
                work->field_0                      = 0x16;
                work->field_2                      = -1;
                return 1;
            case 13:
                work->field_0                      = 0x17;
                work->field_2                      = -1;
                arg0->extra.tmd->texturePageOffset = 2;
                arg0->extra.tmd->clutRowOffset     = 4;
                return 1;
        }
    }
    return 0;
}

static void func_actor_403000_801327B0(GfxCoord* coord, SVECTOR* pos, s32 arg2)
{
    s32       sxy;
    s32       flag;
    s32       otz;
    POLY_G3*  prim;
    DR_TPAGE* dr;
    s32       radius;
    s32       i;
    u16       x;
    u16       y;

    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    gte_SetRotMatrix(&coord->workm);
    gte_SetTransMatrix(&coord->workm);
    gte_ldv0(pos);
    gte_rtps();
    gte_stsxy(&sxy);
    gte_stflg(&flag);
    gte_stszotz(&otz);
    if (flag >= 0) {
        x               = sxy;
        y               = sxy >> 16;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        radius          = (s32)(((gRandomLcgState >> 16) & 0xF) + 0x1E) * 0x160 / (otz * 4);
        for (i = 0; i < 8; i++) {
            prim = gGpuPrimCursor;
            // Preserve the textured-triangle-sized reservation for this gouraud packet.
            gGpuPrimCursor = (u8*)prim + sizeof(POLY_GT3);
            setPolyG3(prim);
            setRGB0(prim, 0xFF, 0x60, 0x60);
            setRGB1(prim, 0xF, 8, 8);
            setRGB2(prim, 0x2F, 8, 8);
            prim->x0 = x;
            prim->y0 = y;
            setSemiTrans(prim, 1);
            prim->x1 = x + ((rsin(i << 9) * radius) >> 12);
            prim->y1 = y + ((rcos(i << 9) * radius) >> 12);
            prim->x2 = x + ((rsin(i * 0x200 + 0x200) * radius) >> 12);
            prim->y2 = y + ((rcos(i * 0x200 + 0x200) * radius) >> 12);
            addPrim(&gGpuCurrentOt[(otz - 6) >> 4], prim);
            dr = gGpuPrimCursor;
            // Preserve the draw-mode-sized reservation for this texture-page packet.
            gGpuPrimCursor = (u8*)dr + sizeof(DR_MODE);
            setDrawTPage(dr, 0, 0, 0x2A);
            addPrim(&gGpuCurrentOt[(otz - 6) >> 4], dr);
        }
    }
}

static void func_actor_403000_80132AE0(GfxCoord* parent)
{
    Actor403000TrailScratch* scratch;
    MATRIX*                  m;
    GfxCoord*                walker;
    SVECTOR*                 pos;
    s16                      i;
    POLY_FT4*                prim;
    SVECTOR*                 n;

    SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor403000TrailScratch));
    scratch = SCRATCH_STACK_CURSOR(Actor403000TrailScratch);
    for (i = 0; i < 17; i++) {
        D_actor_403000_80158DF0[17 - i] = D_actor_403000_80158DF0[16 - i];
    }
    m                                        = &scratch->coord.coord;
    MATRIX_PAIR(&scratch->coord.coord, 0, 0) = 0x1000;
    MATRIX_PAIR(m, 0, 2)                     = 0;
    MATRIX_PAIR(m, 1, 1)                     = 0x1000;
    MATRIX_PAIR(m, 2, 0)                     = 0;
    m->m[2][2]                               = 0x1000;
    scratch->coord.coord.t[0]                = -0x3C;
    scratch->coord.coord.t[1]                = -0x28;
    scratch->coord.coord.t[2]                = 0x12C;
    scratch->coord.parent                    = parent;
    scratch->coord.composeStamp              = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&scratch->coord);
    walker          = &scratch->coord;
    pos             = &scratch->pos;
    scratch->pos.vz = 0;
    scratch->pos.vy = 0;
    scratch->pos.vx = 0;
    {
        SVECTOR local;
        VECTOR  result;
        s32     flag;

        local.vx = 0;
        local.vy = pos->vy;
        local.vz = pos->vz;
        while (1) {
            if (walker->parent == NULL)
                break;
            if (walker != &gGfxViewCoord) {
                gte_SetTransMatrix(&walker->coord);
                gte_SetRotMatrix(&walker->coord);
                gte_ldv0(&local);
                gte_rtv0tr();
                gte_stlvnl(&result);
                gte_stflg(&flag);
                local.vx = result.vx;
                local.vy = result.vy;
                local.vz = result.vz;
                walker   = walker->parent;
                continue;
            }
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            break;
        }
    }
    D_actor_403000_80158DF0[0].vx = scratch->pos.vx;
    D_actor_403000_80158DF0[0].vy = scratch->pos.vy;
    D_actor_403000_80158DF0[0].vz = scratch->pos.vz;
    for (i = 0; i < 18; i++) {
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&D_actor_403000_80158DF0[i]);
        gte_rtps();
        gte_stsxy(&scratch->sxy.w);
        gte_stdp(&scratch->p);
        gte_stflg(&scratch->flag);
        gte_stszotz(&scratch->otz);
        if (i == 0 || scratch->flag < 0) {
            scratch->prevSxy.w = scratch->sxy.w;
            scratch->prevFlag  = scratch->flag;
            continue;
        }
        n                  = &scratch->normal;
        scratch->normal.vz = 0;
        scratch->normal.vx = scratch->sxy.v.vy - scratch->prevSxy.v.vy;
        scratch->normal.vy = scratch->prevSxy.v.vx - scratch->sxy.v.vx;
        VectorNormalSS(n, n);
        if (scratch->otz > 0) {
            gte_lddp((gDisplayState.screenDistance * 50 / scratch->otz) >> 2);
            gte_ldsv(n);
            gte_gpf12();
            gte_stsv(n);
        }
        prim->x2 = scratch->sxy.v.vx + scratch->normal.vx;
        prim->y2 = scratch->sxy.v.vy + scratch->normal.vy;
        prim->x3 = scratch->sxy.v.vx - scratch->normal.vx;
        prim->y3 = scratch->sxy.v.vy - scratch->normal.vy;
        if (scratch->prevFlag >= 0) {
            if (i != 1) {
                POLY_FT4* prev = prim - 1;

                GPU_PRIMITIVE_XY_WORD(prim, 0) = GPU_PRIMITIVE_XY_WORD(prev, 2);
                GPU_PRIMITIVE_XY_WORD(prim, 1) = GPU_PRIMITIVE_XY_WORD(prev, 3);
            } else {
                prim->x0 = scratch->prevSxy.v.vx + scratch->normal.vx;
                prim->y0 = scratch->prevSxy.v.vy + scratch->normal.vy;
                prim->x1 = scratch->prevSxy.v.vx - scratch->normal.vx;
                prim->y1 = scratch->prevSxy.v.vy - scratch->normal.vy;
            }
            prim->u2                          = 4;
            prim->u0                          = 4;
            prim->u3                          = 5;
            prim->u1                          = 5;
            prim->v1                          = 7;
            prim->v0                          = 7;
            prim->v3                          = 8;
            prim->v2                          = 8;
            prim->tpage                       = 0x3F;
            prim->clut                        = 0x3C51;
            GPU_PRIMITIVE_COLOR_WORD(prim, 0) = ((17 - i) * 4) & 0xFF;
            setlen(prim, 9);
            prim->code = 0x2E;
            addPrim(&gGpuCurrentOt[(((u32)(scratch->otz - 10) << gDisplayState.otDepthShift) >> 4) & 0x3FF], prim);
        }
        scratch->prevSxy.w = scratch->sxy.w;
        scratch->prevFlag  = scratch->flag;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor403000TrailScratch));
}

static void func_actor_403000_801330D4(GfxCoord* parent)
{
    Actor403000TrailScratch* scratch;
    MATRIX*                  m;
    GfxCoord*                walker;
    SVECTOR*                 pos;
    s16                      i;

    SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor403000TrailScratch));
    scratch = SCRATCH_STACK_CURSOR(Actor403000TrailScratch);
    for (i = 0; i < 17; i++) {
        D_actor_403000_80158DF0[17 - i] = D_actor_403000_80158DF0[16 - i];
    }
    /* Identity, written as three words and a short through a second pointer. */
    m                                        = &scratch->coord.coord;
    MATRIX_PAIR(&scratch->coord.coord, 0, 0) = 0x1000;
    MATRIX_PAIR(m, 0, 2)                     = 0;
    MATRIX_PAIR(m, 1, 1)                     = 0x1000;
    MATRIX_PAIR(m, 2, 0)                     = 0;
    m->m[2][2]                               = 0x1000;
    scratch->coord.coord.t[0]                = -0x3C;
    scratch->coord.coord.t[1]                = -0x28;
    scratch->coord.parent                    = parent;
    scratch->coord.coord.t[2]                = 0x12C;
    scratch->coord.composeStamp              = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&scratch->coord);
    walker          = &scratch->coord;
    pos             = &scratch->pos;
    scratch->pos.vz = 0;
    scratch->pos.vy = 0;
    scratch->pos.vx = 0;
    {
        SVECTOR local;
        VECTOR  result;
        s32     flag;

        local.vx = 0;
        local.vy = pos->vy;
        local.vz = pos->vz;
        while (1) {
            if (walker->parent == NULL)
                break;
            if (walker != &gGfxViewCoord) {
                gte_SetTransMatrix(&walker->coord);
                gte_SetRotMatrix(&walker->coord);
                gte_ldv0(&local);
                gte_rtv0tr();
                gte_stlvnl(&result);
                gte_stflg(&flag);
                local.vx = result.vx;
                local.vy = result.vy;
                local.vz = result.vz;
                walker   = walker->parent;
                continue;
            }
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            break;
        }
    }
    D_actor_403000_80158DF0[0].vx = scratch->pos.vx;
    D_actor_403000_80158DF0[0].vy = scratch->pos.vy;
    D_actor_403000_80158DF0[0].vz = scratch->pos.vz;
}

static void func_actor_403000_801332E8(Task* arg0)
{
    Actor403000Work* work;
    s16              target;
    s16              orig;
    s32              diff;

    work   = arg0->work;
    orig   = work->field_ADA;
    target = orig;
    if (orig > 700) {
        target = 700;
    }
    if (orig < -700) {
        target = -700;
    }
    if (work->field_AE2 < target) {
        if (target - work->field_AE2 > 64) {
            work->field_AE2 += 64;
        } else {
            work->field_AE2 = target;
        }
    }
    if (target < work->field_AE2) {
        diff = work->field_AE2 - target;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff > 64) {
            work->field_AE2 -= 64;
        } else {
            work->field_AE2 = target;
        }
    }
    gfxRotMatrixZ(&arg0->extra.tmd->coords[22].coord, work->field_AE2 / 2, GRAPHICS_ROTATION_REPLACE);
    arg0->extra.tmd->coords[22].composeStamp = GRAPHICS_COORD_DIRTY;
    gfxRotMatrixZ(&arg0->extra.tmd->coords[23].coord, work->field_AE2 * 3 / 4, GRAPHICS_ROTATION_REPLACE);
    arg0->extra.tmd->coords[23].composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_actor_403000_80133444(Task* arg0)
{
    Actor403000Work* work;
    s16              target;
    s16              orig;
    s32              diff;
    s32              delta;

    work = arg0->work;
    if (work->field_AE0 < work->field_AE6 && (diff = abs(work->field_AE0 - work->field_AE6)) >= 8 && work->field_ADC > -0x280) {
        if (diff >= 24) {
            if (work->field_ADC > 0) {
                work->field_ADC = -32;
            } else {
                work->field_ADC -= 32;
            }
        } else {
            if (work->field_ADC > 0) {
                work->field_ADC = -2;
            } else {
                work->field_ADC -= 2;
            }
        }
    } else if (work->field_AE0 > work->field_AE6 && (diff = abs(work->field_AE0 - work->field_AE6)) >= 8 && work->field_ADC < 0x280) {
        if (diff >= 24) {
            if (work->field_ADC < 0) {
                work->field_ADC = 32;
            } else {
                work->field_ADC += 32;
            }
        } else {
            if (work->field_ADC < 0) {
                work->field_ADC = 2;
            } else {
                work->field_ADC += 2;
            }
        }
    } else {
        work->field_ADC = 0;
    }
    target          = work->field_ADC;
    work->field_AE6 = work->field_AE0;
    orig            = target;
    if (orig > 640) {
        target = 640;
    }
    if (orig < -640) {
        target = -640;
    }
    if (work->field_AE4 < target) {
        if (target - work->field_AE4 > 48) {
            work->field_AE4 += 48;
        } else {
            work->field_AE4 = target;
        }
    }
    if (target < work->field_AE4) {
        delta = work->field_AE4 - target;
        if (delta < 0) {
            delta = -delta;
        }
        if (delta > 48) {
            work->field_AE4 -= 48;
        } else {
            work->field_AE4 = target;
        }
    }
    gfxRotMatrixZ(&arg0->extra.tmd->coords[6].coord, work->field_AE4 / 2, GRAPHICS_ROTATION_COMPOSE);
    arg0->extra.tmd->coords[6].composeStamp = GRAPHICS_COORD_DIRTY;
    gfxRotMatrixZ(&arg0->extra.tmd->coords[7].coord, work->field_AE4 * 3 / 4, GRAPHICS_ROTATION_COMPOSE);
    arg0->extra.tmd->coords[7].composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_actor_403000_801336B4(Task* arg0)
{
    AnimationPose     pose;
    AnimationPose     blendPose;
    AnimationContext* anim;
    s16               weight;
    s16               i;
    Actor403000Work*  work;

    work   = (Actor403000Work*)arg0->work;
    weight = work->field_AD4;
    anim   = &work->anim;
    for (i = 1; i < 0x18; i++) {
        if (i < 0xB) {
            work->blendSlots[i].rate = work->field_AD2;
            work->slots[i].rate      = (work->field_ACA - 3);
            animationTickSlotPose(anim, i, &pose, 0);
            animationTickSlotPose(&work->blendAnim, i, &blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].rate = (work->field_ACA - 3);
            animationTickSlot(&work->anim, i);
        }
    }
}

static s32 func_actor_403000_801337E0(Task* arg0, Actor403000Work* work)
{
    s32 ret;

    ret = 0;
    if (work->field_AF0 == (work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
        return ret;
    }
    switch ((s16)(work->field_AC6 - 1)) {
        case 0:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x21 && work->field_AF0 < 0x21) {
                ret = 0x401E0002;
            }
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x2C && work->field_AF0 < 0x2C) {
                ret = 0x401E0001;
            }
            break;
        case 8:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 5 && work->field_AF0 < 5) {
                ret = 0x401E0002;
            }
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0xA && work->field_AF0 < 0xA) {
                ret = 0x401E0001;
            }
            break;
        case 13:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0xF && work->field_AF0 < 0xF) {
                ret = 0x401E000E;
            }
            break;
        case 1:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x1D && work->field_AF0 < 0x1D) {
                ret = 0x401E0003;
            }
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x17 && work->field_AF0 < 0x17) {
                ret = 0x401E0004;
            }
            break;
        case 11:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x13 && work->field_AF0 < 0x13) {
                ret = 0x401E0008;
            }
        case 10:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x12 && work->field_AF0 < 0x12) {
                ret = 0x401E0007;
            }
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0xF && work->field_AF0 < 0xF) {
                ret = 0x401E000C;
            }
            break;
        case 6:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0x15 && work->field_AF0 < 0x15) {
                ret = 0x401E0009;
            }
            break;
        case 16:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 8 && work->field_AF0 < 8) {
                ret = 0x401E000B;
            }
            break;
        case 7:
            if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 0xD && work->field_AF0 < 0xD) {
                ret = 0x401E000D;
            }
            break;
    }
    work->field_AF0 = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    return ret;
}

static void func_actor_403000_80133AF8(Task* arg0)
{
    Actor403000Work* seekWork;
    Actor403000Work* resetWork;
    Actor403000Work* turnWork;
    Actor403000Work* secondaryWork;
    Actor403000Work* tickWork;
    Actor403000Work* work;
    u32              table;
    s32              index;
    s32              animation;
    s32              updatedTurn;
    s16              currentTurn;
    s16              thirdAngle;
    s16              state;
    s32              currentAngle;
    s32              targetAngle;
    s16              angle;
    s32              seekSlotIndex;
    s32              resetSlotIndex;
    s32              secondarySlotIndex;
    s32              tickSlotIndex;
    s32              signedTurn;
    s32              sound;
    s32              resetIndex;
    s32              secondaryIndex;
    s32              tickIndex;
    s32              seekIndex;
    s32              delta;
    AnimationSlot*   tickSlot;
    AnimationSlot*   seekSlot;
    AnimationSlot*   resetSlot;
    AnimationSlot*   secondarySlot;
    s32              pan;
    s32              currentAngleBits;
    u16              originalTurn;
    s32              targetAngleBits;
    u16              updatedTurnBits;
    s16              clampedAngle;
    s32              targetTurn;

    work  = (Actor403000Work*)arg0->work;
    state = work->field_AC0;
    if (state == 1) {
        if (work->field_AC4 != work->field_AC6) {
            seekWork  = work;
            seekIndex = 1;
            table     = (u32)D_actor_403000_80158364;
            seekSlot  = work->slots;
            do {
                seekSlotIndex    = seekIndex;
                seekSlot[1].rate = seekWork->field_ACA;
                animation        = seekWork->field_AC6;
                seekSlot        += 1;
                index            = seekWork->field_AC4 * 0x2D;
                animationSeekSlotWithBlend(&seekWork->anim, seekSlotIndex, (s16)(animation), 0, (s32) * (s8*)((animation + index) + table));
                seekIndex += 1;
            } while (seekIndex < 0x18);
            seekWork->field_AC4 = seekWork->field_AC6;
        }
        work->field_AC0 = 3;
        work->field_AC8 = 0;
        memFillBytes(work->pad_AEC, 0U, 0x60U);
    } else if (state == 2) {
        resetWork  = work;
        resetIndex = 1;
        resetSlot  = work->slots;
        do {
            resetSlotIndex    = resetIndex;
            resetSlot[1].rate = resetWork->field_ACA;
            resetSlot        += 1;
            animationResetSlot(&resetWork->anim, resetSlotIndex, resetWork->field_AC6);
            resetIndex += 1;
        } while (resetIndex < 0x18);
        resetWork->field_AC4 = resetWork->field_AC6;
        work->field_AC0      = 3;
        work->field_AC8      = 0U;
        memFillBytes(work->pad_AEC, 0U, 0x60U);
    }
    if (work->field_ACE == 2) {
        secondaryWork            = (Actor403000Work*)arg0->work;
        secondaryIndex           = 1;
        secondarySlot            = secondaryWork->slots;
        secondaryWork->field_AD2 = 0x20;
        secondaryWork->field_AD4 = 0x800;
        do {
            secondarySlotIndex    = secondaryIndex;
            secondarySlot[1].rate = secondaryWork->field_AD2;
            secondarySlot        += 1;
            animationResetSlot(&secondaryWork->blendAnim, secondarySlotIndex, secondaryWork->field_AD0);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x18);
        work->field_ACE = 3;
    }
    work->field_AC8 = (u16)(work->field_AC8 + 1);
    if (work->field_AC2 == 0) {
        tickWork  = (Actor403000Work*)arg0->work;
        tickIndex = 1;
        tickSlot  = tickWork->slots;
        do {
            tickSlotIndex    = tickIndex;
            tickSlot[1].rate = tickWork->field_ACA;
            animationTickSlot(&tickWork->anim, tickSlotIndex);
            tickSlot  += 1;
            tickIndex += 1;
        } while (tickIndex < 0x18);
    } else {
        func_actor_403000_801336B4(arg0);
        if (work->blendSlots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
            work->field_AC2 = 0;
        }
    }
    targetAngle      = work->field_AD8;
    currentAngle     = work->field_AE0;
    targetAngleBits  = (u16)work->field_AD8;
    currentAngleBits = (u16)work->field_AE0;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x72) {
            work->field_AE0 = currentAngleBits + 0x71;
        } else {
            goto block_26;
        }
    } else if ((currentAngle - targetAngle) >= 0x72) {
        work->field_AE0 = currentAngleBits - 0x71;
    } else {
    block_26:
        work->field_AE0 = targetAngleBits;
    }
    if ((s8)work->field_AE8 == 1) {
        angle        = work->field_AE0;
        clampedAngle = angle;
        if (angle >= 0x501) {
            clampedAngle = 0x500;
        }
        if (angle < -0x500) {
            clampedAngle = -0x500;
        }
        thirdAngle = (s16)clampedAngle / 3;
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[2], thirdAngle);
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[3], thirdAngle);
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[4], (s16)clampedAngle / 2);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if ((s8)work->field_AE9 == 1) {
        func_actor_403000_80133444(arg0);
    }
    if ((s8)work->field_AEA == 1) {
        turnWork     = (Actor403000Work*)arg0->work;
        targetTurn   = (u16)turnWork->field_AD6;
        originalTurn = targetTurn;
        if ((s16)targetTurn >= 0x201) {
            targetTurn = 0x200;
        }
        if ((s16)originalTurn < -0x200) {
            targetTurn = -0x200;
        }
        signedTurn  = (s16)targetTurn;
        currentTurn = turnWork->field_ADE;
        if (currentTurn < signedTurn) {
            if ((signedTurn - currentTurn) >= 0xD) {
                turnWork->field_ADE = (s16)((u16)turnWork->field_ADE + 0xC);
            } else {
                turnWork->field_ADE = (s16)targetTurn;
            }
        }
        updatedTurn     = turnWork->field_ADE;
        updatedTurnBits = (u16)turnWork->field_ADE;
        if ((s16)targetTurn < updatedTurn) {
            delta = updatedTurn - (s16)targetTurn;
            if (delta < 0) {
                delta = -delta;
            }
            if (delta >= 0xD) {
                turnWork->field_ADE = (s16)(updatedTurnBits - 0xC);
            } else {
                turnWork->field_ADE = (s16)targetTurn;
            }
        }
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[10], (s16)((s32)(u16)turnWork->field_ADE * -1));
        arg0->extra.tmd->coords[10].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if ((s8)work->field_AEB == 1) {
        func_actor_403000_801332E8(arg0);
    }
    sound = func_actor_403000_801337E0(arg0, work);
    if (sound != 0) {
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
}

static s32 func_actor_403000_80133FC0(Task* arg0, s16 arg1, s16 arg2)
{
    GfxCoord*                 coord;
    Actor403000FacingScratch* scratch;
    s16                       angle;
    s32                       mag;
    GfxCoord*                 coord2;

    if (arg1 == arg2) {
        return 1;
    }
    switch (arg1) {
        case 0:
            if (arg2 == 9) {
                goto calc;
            }
            if (arg2 < 4) {
                goto calc;
            }
            return 0;
        case 1:
        case 2:
        case 3:
            if (arg2 < 5) {
                goto calc;
            }
            return 0;
        case 4:
            if (arg2 >= 6) {
                return 0;
            }
            if (arg2 != 0) {
                goto calc;
            }
            return 0;
        case 5:
            if (arg2 < 4) {
                return 0;
            }
            if (arg2 != 9) {
                goto calc;
            }
            return 0;
        case 6:
        case 7:
        case 8:
            if (arg2 >= 5) {
                goto calc;
            }
            return 0;
        case 9:
        default:
            if (arg2 >= 6) {
                goto calc;
            }
            if (arg2 != 0) {
                return 0;
            }
            break;
    }
calc:
    scratch            = SCRATCH_STACK_RESERVE_BLOCK(Actor403000FacingScratch);
    coord              = arg0->extra.tmd->coords;
    scratch->target.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    scratch->target.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    scratch->target.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    coord2             = arg0->extra.tmd->coords;
    angle              = ratan2(scratch->target.vx, scratch->target.vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    scratch->angle = mag = angle;
    if ((mag < 0 ? -mag : mag) < 0x200) {
        SCRATCH_STACK_RELEASE_BLOCK(Actor403000FacingScratch);
        return 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000FacingScratch);
    return 0;
}

static s32 func_actor_403000_80134204(GfxCoord* arg0)
{
    GfxCoord*               coord;
    Actor403000TurnScratch* scratch;
    SVECTOR*                table;
    SVECTOR*                v;
    s32                     x;
    s32                     z;
    s8                      col;
    s8                      row;
    s16                     angle;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(Actor403000TurnScratch);
    coord   = arg0;
    x       = coord->coord.t[0];
    z       = coord->coord.t[2];
    col     = 4;
    if (x >= 0xD48) {
        col = 3;
        if (x >= 0x1A90) {
            col = 2;
            if (x >= 0x2AF8) {
                col = x < 0x3C8C;
            }
        }
    }
    row            = z >= 0x1068;
    scratch->index = D_actor_403000_80158D48[col + row * 5] + 1;
    if (scratch->index == 10) {
        scratch->index = 0;
    }
    table               = D_actor_403000_80158CE0;
    v                   = &table[scratch->index];
    scratch->target.vx  = v->vx;
    scratch->target.vy  = v->vy;
    scratch->target.vz  = v->vz;
    scratch->target.vx -= coord->coord.t[0];
    scratch->target.vz -= coord->coord.t[2];
    angle               = ratan2(scratch->target.vx, scratch->target.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    if (angle < 0x400) {
        scratch->turn = 1;
    } else {
        scratch->turn = -1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000TurnScratch);
    return scratch->turn;
}

static void func_actor_403000_801343B8(Enemy* arg0, Task* arg1)
{
    SVECTOR                dir;
    VECTOR                 pos;
    Actor403000Work*       work;
    TmdObject*             obj;
    GfxCoord*              coord;
    WorldCollisionBody*    node;
    WorldCollisionBody*    node2;
    WorldCollisionBody*    node3;
    AnimationSet**         animSrc;
    WorldCollisionContact* records;
    WorldCollisionContact* records2;
    SVECTOR*               dirp;
    WorldCollisionContact* firstRec;
    Actor403000Work*       idWork;
    TmdObject*             tmd;

    obj        = arg1->extra.tmd;
    coord      = obj->coords;
    arg1->work = (work = memCalloc(0xFDCU, false));
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->exitCallback = func_actor_403000_8013D4F4;
    idWork             = (Actor403000Work*)arg1->work;
    tmd                = arg1->extra.tmd;
    tmd->lightMtx      = &idWork->field_F10;
    tmd->colorMtx      = &idWork->field_F30;
    arg0->field_4      = &arg1->extra.tmd->coords->coord;
    arg0->field_48     = 0;
    arg0->bodyPos.vx   = 0;
    arg0->bodyPos.vy   = 0;
    arg0->bodyPos.vz   = 0;
    arg0->coord        = &arg1->extra.tmd->coords[2];
    Gp_LinkNode(&arg0->node);
    animSrc             = D_actor_403000_80158B50;
    work->field_FCA     = 1;
    arg0->reactionFlags = 0;
    arg0->hp            = D_actor_403000_8013DA00.hpMax;
    arg0->param         = &D_actor_403000_8013DA00;
    arg0->recs          = (firstRec = work->objB50.contacts);
    animationInitContext(&((Actor403000Work*)work)->anim, animSrc, obj,
                         (u8(*)[ANIMATION_POSE_BUFFER_BYTES])((Actor403000Work*)work)->pad_3E8, ((Actor403000Work*)work)->slots);
    animationInitContext(&((Actor403000Work*)work)->blendAnim, animSrc, obj,
                         (u8(*)[ANIMATION_POSE_BUFFER_BYTES])((Actor403000Work*)work)->pad_93C, ((Actor403000Work*)work)->blendSlots);
    work->field_AC0 = 2;
    work->field_AC2 = 0;
    work->field_AC6 = 0;
    work->field_AE0 = 0;
    work->field_AD8 = 0;
    work->field_ACC = 0x10;
    work->field_ACA = 0x10;
    work->field_AEB = 1;
    work->field_AEA = 1;
    work->field_AE9 = 1;
    work->field_AE8 = 1;
    func_actor_403000_80133AF8(arg1);
    work->objD18.body.context.contacts = work->objD18.contacts;
    work->objD18.body.coord            = coord;
    work->objD18.body.pos.vx           = 0;
    work->objD18.body.pos.vy           = -0x11C;
    work->objD18.body.pos.vz           = 0;
    work->objD18.body.key              = 0x30001;
    work->objD18.body.radius           = 0x12C;
    work->objD18.body.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->objD18.body);
    work->recDD0.ends[0].vy      = -0x180;
    work->recDD0.ends[1].vy      = -0x180;
    work->recDD0.ends[1].vz      = 0x2BC;
    work->objDB0.context.capsule = &work->recDD0;
    work->objDB0.key             = 0x30001;
    work->recDD0.ends[0].vx      = 0;
    work->recDD0.ends[0].vz      = 0;
    work->recDD0.ends[1].vx      = 0;
    work->recDD0.end0Radius      = 0x12C;
    work->recDD0.end1Radius      = 0x12C;
    work->recDD0.contacts = records = work->records;
    work->objDB0.coord              = coord;
    work->objDB0.pos.vx             = 0;
    work->objDB0.pos.vy             = 0;
    work->objDB0.pos.vz             = 0;
    work->objDB0.radius             = 0;
    work->objDB0.flags              = WORLD_COLLISION_BODY_CAPSULE;
    work->objD18.body.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
    Gp_LinkObj(2, &work->objDB0);
    work->recE80.ends[0].vz = -0x3E8;
    work->recE80.ends[1].vz = 0x190;
    work->recE80.end0Radius = 0x200;
    work->recE80.end1Radius = 0x200;
    work->recE80.ends[0].vx = 0;
    work->recE80.ends[0].vy = 0;
    work->recE80.ends[1].vx = 0;
    work->recE80.ends[1].vy = 0;
    work->recE80.contacts = records2 = work->recordsE98;
    work->objDB0.flags              |= WORLD_COLLISION_BODY_GRID_ENABLED;
    work->objE60.coord               = &arg1->extra.tmd->coords[5];
    work->objE60.context.capsule     = &work->recE80;
    work->objE60.pos.vx              = 0;
    work->objE60.pos.vy              = 0;
    work->objE60.pos.vz              = 0;
    work->objE60.key                 = 0x3001E;
    work->objE60.radius              = 0;
    work->objE60.flags               = WORLD_COLLISION_BODY_CAPSULE;
    Gp_LinkObj(2, &work->objE60);
    work->objE60.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(records, 5, 0);
    Gp_InitRec18Table(records2, 5, 0);
    Gp_InitRec18Table(work->objD18.body.context.contacts, ARRAY_SIZE(work->objD18.contacts), 0);
    node                   = &work->objB50.body;
    node->coord            = &arg1->extra.tmd->coords[1];
    node->context.contacts = firstRec;
    node->pos.vx           = 0;
    node->pos.vy           = 0;
    node->pos.vz           = 0;
    node->key              = 0x3001E;
    node->radius           = 0x3E8;
    node->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->objB50.body);
    node->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(node->context.contacts, ARRAY_SIZE(work->objB50.contacts), 0);
    node2                   = &work->objBE8.body;
    node2->coord            = &arg1->extra.tmd->coords[15];
    node2->context.contacts = work->objBE8.contacts;
    node2->pos.vx           = 0;
    node2->pos.vy           = 0;
    node2->pos.vz           = 0;
    node2->key              = 0x3001E;
    node2->radius           = 0x320;
    node2->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->objBE8.body);
    node2->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(node2->context.contacts, ARRAY_SIZE(work->objBE8.contacts), 0);
    node3                   = &work->objC80.body;
    node3->coord            = &arg1->extra.tmd->coords[4];
    node3->context.contacts = work->objC80.contacts;
    node3->pos.vx           = 0;
    node3->pos.vy           = 0;
    node3->pos.vz           = 0;
    node3->key              = 0x3001E;
    node3->radius           = 0x320;
    node3->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->objC80.body);
    node3->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(node3->context.contacts, ARRAY_SIZE(work->objC80.contacts), 0);
    work->objBE8.body.pos.vx = 0;
    work->objBE8.body.pos.vy = 0;
    work->objBE8.body.pos.vz = -0x100;
    work->field_FC0          = 0;
    work->field_F88          = GameFlag_GetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED);
    gfxReadMatrixZAxis(&arg1->extra.tmd->coords->coord, &dir);
    dir.vy = 0;
    dirp   = &dir;
    VectorNormalSS(dirp, dirp);
    gte_lddp(0x1388);
    gte_ldsv(dirp);
    gte_gpf12();
    gte_stsv(dirp);
    work->field_F90     = D_actor_403000_80158C08;
    work->field_F94     = 1;
    work->field_F9C     = 3;
    work->field_F98     = 0;
    work->field_FA0     = 1;
    work->field_F8C     = 0;
    arg1->msgTable      = D_actor_403000_80158CA8;
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    work->field_FD2 = -1;
    work->field_FD3 = -1;
    work->field_0   = 0xB;
    arg1->state++;
}

static void func_actor_403000_80134910(Task* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*         scratch;
    Actor403000Work* work;
    EffectSpawnArg*  eff;
    s32              mag;

    scratch = (SCRATCH_STACK_CURSOR(SVECTOR) -= 2);
    mag     = (arg1 >= 0) ? arg1 : -arg1;
    work    = arg0->work;
    if (mag < 0x200) {
        switch ((s32)((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3) {
            case 0:
                scratch[0] = D_actor_403000_80158C48[0];
                scratch[1] = D_actor_403000_80158C48[3];
                break;
            case 1:
                scratch[0] = D_actor_403000_80158C48[1];
                scratch[1] = D_actor_403000_80158C48[2];
                break;
            case 2:
                scratch[0] = D_actor_403000_80158C48[2];
                scratch[1] = D_actor_403000_80158C48[0];
                break;
            case 3:
                scratch[0] = D_actor_403000_80158C48[3];
                scratch[1] = D_actor_403000_80158C48[1];
                break;
            default:
                scratch[0] = D_actor_403000_80158C48[4];
                break;
        }
    } else if (mag > 0x600) {
        switch ((s32)((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 2) {
            case 0:
                scratch[0] = D_actor_403000_80158C48[5];
                scratch[1] = D_actor_403000_80158C48[6];
                break;
            case 1:
                scratch[0] = D_actor_403000_80158C48[6];
                scratch[1] = D_actor_403000_80158C48[7];
                break;
            default:
                scratch[0] = D_actor_403000_80158C48[7];
                scratch[1] = D_actor_403000_80158C48[5];
                break;
        }
    } else if (arg1 > 0) {
        scratch[0] = D_actor_403000_80158C48[8];
        scratch[1] = D_actor_403000_80158C48[9];
    } else {
        scratch[0] = D_actor_403000_80158C48[10];
        scratch[1] = D_actor_403000_80158C48[11];
    }
    work->field_FA8.coord      = &arg0->extra.tmd->coords[scratch[0].pad];
    work->field_FA8.spawnArgLo = 0x500;
    work->field_FA8.spawnArgHi = 3;
    eff                        = &work->field_FA8;
    func_800FDB18((u16)Gp_GetIdParam1(arg2), &arg0->extra.tmd->coords[scratch[0].pad], &scratch[0], eff);
    work->field_FA8.coord      = &arg0->extra.tmd->coords[scratch[1].pad];
    work->field_FA8.spawnArgLo = 0x400;
    work->field_FA8.spawnArgHi = 2;
    func_800FDB18((u16)Gp_GetIdParam1(arg2), &arg0->extra.tmd->coords[scratch[1].pad], &scratch[1], eff);
    SCRATCH_STACK_CURSOR(SVECTOR) += 2;
}

static s32 func_actor_403000_80134E00(Task* arg0)
{
    Actor403000Work* work;
    s16              flags;
    s16              i;
    VECTOR           d;

    work  = arg0->work;
    flags = GameFlag_GetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED);
    if (flags == work->field_F88) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        if (((flags >> i) & 1) && !((work->field_F88 >> i) & 1)) {
            d.vx = arg0->extra.tmd->coords->coord.t[0] - D_actor_403000_80158D64[i].vx;
            d.vz = arg0->extra.tmd->coords->coord.t[2] - D_actor_403000_80158D64[i].vz;
            if (SquareRoot0(d.vx * d.vx + d.vz * d.vz) < 3000) {
                work->field_F88 = flags;
                return 1;
            }
        }
    }
    work->field_F88 = flags;
    return 0;
}

static inline s32 func_actor_403000_FindHit(SVECTOR* pos, WorldCollisionContact* records)
{
    s16 i;
    for (i = 0; i < 5; i++) {
        if (!records[i].key.value)
            break;
        if ((records[i].key.value & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].point.vx;
            pos->vy = records[i].point.vy;
            pos->vz = records[i].point.vz;
            return records[i].key.value;
        }
    }
    return 0;
}

static inline s16 func_actor_403000_WrapAngle(s16 angle)
{
    if (angle < 0) {
        while (1) {
            if (angle >= -0x800)
                break;
            angle += 0x1000;
        }
    } else {
        while (1) {
            if (angle <= 0x800)
                break;
            angle -= 0x1000;
        }
    }
    return angle;
}

static inline void func_actor_403000_PlaySound(Task* arg0, Enemy* enemy, s32 id)
{
    s32 sound;
    s32 pan;

    sound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
    pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
}

static void func_actor_403000_80134F44(Task* arg0)
{
    Task*                     player;
    Enemy*                    enemy;
    PlayerStatus*             config;
    Actor403000Work*          work;
    Actor403000DamageScratch* head;
    Actor403000DamageScratch* scratch;
    s32                       yaw;
    s32                       dx;
    s32                       dy;
    s32                       dz;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    enemy  = arg0->spawnArg2.pointer;
    config = &gPlayerStatus;
    work   = arg0->work;
    if (enemy->hp > 0) {
        if (work->field_F70 > 0) {
            work->field_F70--;
            return;
        }
        head        = SCRATCH_STACK_CURSOR(Actor403000DamageScratch);
        scratch     = (SCRATCH_STACK_CURSOR(Actor403000DamageScratch) = head - 1);
        scratch->id = func_actor_403000_FindHit(&scratch->pos, work->objB50.contacts);
        if (scratch->id == 0) {
            scratch->id = func_actor_403000_FindHit(&scratch->pos, work->objBE8.contacts);
        }
        if (scratch->id == 0) {
            scratch->id = func_actor_403000_FindHit(&scratch->pos, work->objC80.contacts);
        }
        if (scratch->id == 0) {
            scratch->id = func_actor_403000_FindHit(&scratch->pos, work->recordsE98);
        }
        if (work->field_FDA != 0) {
            work->field_FDA--;
            switch (work->field_FDA % 30) {
                case 0:
                    Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, &arg0->extra.tmd->coords[15], 0x800001FF, NULL);
                    break;
                case 8:
                    Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, &arg0->extra.tmd->coords[23], 0x800001FF, NULL);
                    break;
                case 19:
                    Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, &arg0->extra.tmd->coords[11], 0x800001FF, NULL);
                    break;
            }
        }
        if ((s16)func_actor_403000_80134E00(arg0) != 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_FDA = ((gRandomLcgState >> 16) & 0x1F) + 0xA0;
            if (work->field_0 != 0x10 && work->field_0 != 0x12 && !(work->field_0 == 0x13 && (s16)work->field_6 >= 0x13)) {
                if (work->field_0 != 0xF && !(work->field_0 == 0xC && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) && work->field_0 != 0xD && work->field_FC0 != 1) {
                    work->field_0 = 0x11;
                }
            }
            scratch->id     = 0;
            scratch->damage = 400;
            work->field_AE0 = 0;
            work->field_AD8 = 0;
            func_800E2C78(enemy, scratch->id, scratch->damage, 0);
            func_800DA6E8(&enemy->node, scratch->damage, 0);
            enemy->hp -= scratch->damage;
            if (enemy->hp <= 0 && gPlayerStatus.hp <= 0) {
                enemy->hp = 1;
            }
            if ((work->field_0 == 0xC && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) || work->field_0 == 0xD || work->field_0 == 0xF || work->field_FC0 == 1) {
                if (enemy->hp <= 0) {
                    enemy->hp = 1;
                }
            }
            if (enemy->hp <= 0) {
                if ((work->field_0 == 0x13 && (s16)work->field_6 < 0x12) || work->field_0 == 0x12 || work->field_0 == 0x10 || work->field_0 == 0x18) {
                    work->field_F8C = 1;
                    work->field_0   = 0x14;
                } else {
                    work->field_0 = 0x11;
                }
                if (arg0->extra.tmd->texturePageOffset == 0) {
                    func_actor_403000_PlaySound(arg0, enemy, 0x401E0011);
                } else {
                    func_actor_403000_PlaySound(arg0, enemy, 0x401E0012);
                }
            } else {
                func_actor_403000_PlaySound(arg0, enemy, 0x401E0005);
            }
        }
        if (scratch->id != 0) {
            if (work->field_0 == 3) {
                work->field_FD6 = 1;
            }
            work->field_F70 = Gp_GetIdParam2(scratch->id);
            switch (Gp_GetIdParam0(scratch->id) & 0xFFFF) {
                case 0:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    if (work->field_0 == 3 || work->field_0 == 0xE) {
                        work->field_0   = 2;
                        work->field_2   = -1;
                        work->field_FD2 = work->field_FD3;
                        work->field_FD3 = -work->field_FD3;
                        break;
                    }
                    if ((u16)(work->field_0 - 0x10) >= 3 && work->field_0 != 0x18 && work->field_FC0 != 1 && !(work->field_0 == 0x13 && (s16)work->field_6 < 0x12)) {
                        work->field_AD0 = 0xD;
                        work->field_AC2 = 1;
                        work->field_ACE = 2;
                    }
                    break;
                case 4:
                    if (work->field_0 == 0x10 || work->field_0 == 0x12 || work->field_0 == 0x18 || (work->field_0 == 0x13 && (s16)work->field_6 < 0x12)) {
                        work->field_0 = 0x18;
                        work->field_2 = -1;
                        break;
                    }
                    if (work->field_0 != 0xF && !(work->field_0 == 0xC && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) && work->field_0 != 0x11 && work->field_0 != 0xD && work->field_FC0 != 1) {
                        work->field_0 = 0x11;
                    }
                    break;
                case 2:
                    if (work->field_0 != 0xF && !(work->field_0 == 0xC && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) && work->field_0 != 0x11 && work->field_0 != 0xD && work->field_FC0 != 1) {
                        Gp_SetObjFlag2(enemy, scratch->id, 0);
                        if (work->field_0 == 0x10 || work->field_0 == 0x12 || work->field_0 == 0x18 || (work->field_0 == 0x13 && (s16)work->field_6 < 0x12)) {
                            work->field_0 = 0x18;
                            work->field_2 = -1;
                            break;
                        }
                        work->field_0 = 0x11;
                    }
                    break;
                case 3:
                    Gp_SetObjFlag4(enemy, scratch->id, 0);
                    break;
                case 1:
                    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    if (work->field_0 == 0x10 || work->field_0 == 0x12 || work->field_0 == 0x18 || (work->field_0 == 0x13 && (s16)work->field_6 < 0x12)) {
                        work->field_0 = 0x18;
                        work->field_2 = -1;
                        break;
                    }
                    if (work->field_0 != 0xF && !(work->field_0 == 0xC && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) && work->field_0 != 0x11 && work->field_0 != 0xD && work->field_FC0 != 1) {
                        work->field_AC2 = 1;
                        work->field_AD0 = 0xD;
                        work->field_ACE = 2;
                        if (work->field_0 == 3 || work->field_0 == 0xE) {
                            work->field_0   = 2;
                            work->field_2   = -1;
                            work->field_FD2 = work->field_FD3;
                            work->field_FD3 = -work->field_FD3;
                        }
                    }
                    break;
            }
            dx              = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
            scratch->d.vx   = dx;
            dy              = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
            scratch->d.vy   = dy;
            dz              = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
            scratch->d.vz   = dz;
            scratch->dist   = SquareRoot0(dx * dx + dy * dy + dz * dz);
            scratch->damage = Gp_ComputeDamage(scratch->id, scratch->dist, 0, 0);
            if (Gp_RollEnemyChance(enemy, scratch->id, 0) != 0) {
                scratch->damage *= 4;
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[2], 0, NULL);
            }
            scratch->rel.vx = scratch->pos.vx - arg0->extra.tmd->coords->workm.t[0];
            scratch->rel.vy = scratch->pos.vy - arg0->extra.tmd->coords->workm.t[1];
            scratch->rel.vz = scratch->pos.vz - arg0->extra.tmd->coords->workm.t[2];
            yaw             = ratan2(scratch->rel.vx, scratch->rel.vz);
            scratch->angle  = yaw - ratan2(-arg0->extra.tmd->coords->workm.m[2][0], arg0->extra.tmd->coords->workm.m[2][2]);
            scratch->angle  = func_actor_403000_WrapAngle(scratch->angle);
            func_actor_403000_80134910(arg0, scratch->angle, scratch->id);
            work->field_AE0 = 0;
            work->field_AD8 = 0;
            func_800E2C78(enemy, scratch->id, scratch->damage, 0);
            func_800DA6E8(&enemy->node, scratch->damage, 0);
            enemy->hp -= scratch->damage;
            if ((work->field_0 == 0xC && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) || work->field_0 == 0xD || work->field_0 == 0xF || work->field_FC0 == 1) {
                if (enemy->hp <= 0) {
                    enemy->hp = 1;
                }
            }
            if (enemy->hp <= 0) {
                if ((work->field_0 == 0x13 && (s16)work->field_6 < 0x12) || work->field_0 == 0x12 || work->field_0 == 0x10 || work->field_0 == 0x18) {
                    work->field_F8C = 1;
                    work->field_0   = 0x14;
                } else {
                    work->field_0 = 0x11;
                }
                D_actor_403000_80158D8C.context.loc.stage = 9;
                D_actor_403000_80158D8C.context.loc.area  = 1;
                D_actor_403000_80158D8C.command           = 3;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403000_80158D8C, ACTOR_COMMAND_MESSAGE_APPLY);
                if (arg0->extra.tmd->texturePageOffset == 0) {
                    func_actor_403000_PlaySound(arg0, enemy, 0x401E0011);
                } else {
                    func_actor_403000_PlaySound(arg0, enemy, 0x401E0012);
                }
            } else {
                func_actor_403000_PlaySound(arg0, enemy, 0x401E0005);
            }
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            scratch->damage = Gp_TickObjFlag4(enemy);
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            if (scratch->damage != 0) {
                scratch->damage >>= 2;
                func_800DA6E8(&enemy->node, scratch->damage, 0);
                enemy->hp -= scratch->damage;
                if ((work->field_0 == 0xC && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) || work->field_0 == 0xD || work->field_0 == 0xF || work->field_FC0 == 1) {
                    if (enemy->hp <= 0) {
                        enemy->hp = 1;
                    }
                }
                if (enemy->hp <= 0) {
                    if (work->field_0 != 0x12 && work->field_0 != 0x18 && work->field_0 != 0x11 && work->field_0 != 0x10) {
                        work->field_0 = 0x11;
                    } else {
                        work->field_F8C = 1;
                        work->field_0   = 0x14;
                    }
                } else {
                    if ((work->field_0 == 0x13 && (s16)work->field_6 < 0x12) || work->field_0 == 0x12 || work->field_0 == 0x18 || work->field_0 == 0x10) {
                        work->field_0 = 0x18;
                        work->field_2 = -1;
                    } else if (work->field_0 != 0x11 && work->field_0 != 0xF && !(work->field_0 == 0xC && arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) && work->field_0 != 0xD) {
                        work->field_AC2 = 1;
                        work->field_AD0 = 0xD;
                        work->field_ACE = 2;
                    }
                }
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(Actor403000DamageScratch);
    }
}

static void func_actor_403000_80135F08(Task* arg0)
{
    Actor403000Work* work;
    Enemy*           obj;
    TmdObject*       tmd;
    u32              seed;

    work = arg0->work;
    obj  = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        tmd             = arg0->extra.tmd;
        work->field_FCA = 0;
        tmd->flags      = 0;
        Tmd_AllocBuffers(tmd);
        work->field_ACA          = 0x10;
        work->field_AC6          = 0xF;
        work->field_AC0          = 2;
        work->field_6            = 0;
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_403000_80133AF8(arg0);
    if (work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED)) {
        seed            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState = seed;
        work->field_6   = (seed >> 0x10) & 0x1F;
    }
    if ((s16)work->field_6 > 0) {
        work->field_6--;
        work->field_ACA = 0;
    } else {
        work->field_ACA = 0x10;
    }
    if ((Gp_TickObjFlag2(obj) == 1) || (obj->hp <= 0)) {
        obj->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->field_0       = 0x12;
    }
}

static __inline__ void Actor403000_FaceScale(GfxCoord* coord, s16 sy)
{
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* scratch;

    head                                       = SCRATCH_STACK_CURSOR(ActorScaleRotScratch);
    scratch                                    = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = scratch;
    scratch->angle                             = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&scratch->m, scratch->angle, 1);
    scratch->scale.vx = 0x1000;
    scratch->scale.vy = sy;
    scratch->scale.vz = 0x1000;
    ScaleMatrix(&scratch->m, &scratch->scale);
    coord->coord.m[0][0] = head[-1].m.m[0][0];
    coord->coord.m[0][1] = scratch->m.m[0][1];
    coord->coord.m[0][2] = scratch->m.m[0][2];
    coord->coord.m[1][0] = scratch->m.m[1][0];
    coord->coord.m[1][1] = scratch->m.m[1][1];
    coord->coord.m[1][2] = scratch->m.m[1][2];
    coord->coord.m[2][0] = scratch->m.m[2][0];
    coord->coord.m[2][1] = scratch->m.m[2][1];
    coord->coord.m[2][2] = scratch->m.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
}

static void func_actor_403000_8013603C(Task* arg0)
{
    Actor403000Work* work;
    Enemy*           enemy;
    TmdObject*       tmd;
    s16              t;

    work  = arg0->work;
    tmd   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        tmd->flags = 0;
        Tmd_AllocBuffers(tmd);
        work->field_FCA          = 1;
        work->objD18.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        Gp_ClearNodeSlots(&enemy->node);
        work->field_6 = 0;
        Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
    }
    if (work->field_F8C == 1 && Gp_StateC08.mode != work->field_F8C && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
        work->field_F8C = 0;
    }
    if ((s16)work->field_6 <= 0x1000) {
        work->field_6++;
        if ((s16)work->field_6 % 5 == 0 && (s16)work->field_6 < 130) {
            switch ((s16)((s16)((s16)work->field_6 / 5) % 4)) {
                case 0:
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[1], 0x12600, NULL);
                    break;
                case 1:
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[4], 0x22400, NULL);
                    break;
                case 2:
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[1], 0x32600, NULL);
                    break;
                case 3:
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[18], 0x12500, NULL);
                    break;
            }
        }
        switch ((s16)work->field_6) {
            case 1:
                arg0->extra.tmd->flags = 0;
                Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                break;
            case 0x76:
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[1], 2, NULL);
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[4], 1, NULL);
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[18], 1, NULL);
                break;
            case 0x78:
                arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                arg0->extra.tmd->texturePageOffset = 2;
                arg0->extra.tmd->clutRowOffset     = 4;
                break;
            case 0x88:
                arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
        t = work->field_6;
        if (t > 0x64) {
            if ((t - 0x64) * 0x3C < 0x1000) {
                Actor403000_FaceScale(arg0->extra.tmd->coords, 0x1000 - (t - 0x64) * 0x6B);
            } else {
                Actor403000_FaceScale(arg0->extra.tmd->coords, 0);
            }
        } else {
            Actor403000_FaceScale(arg0->extra.tmd->coords, 0x1000);
        }
    }
}

static void func_actor_403000_801365D0(Task* arg0)
{
    Actor403000Work* work;
    Enemy*           enemy;
    TmdObject*       tmd;
    s16              t;

    work  = arg0->work;
    tmd   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        tmd->flags = 0;
        Tmd_AllocBuffers(tmd);
        work->field_FCA          = 1;
        work->objD18.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        Gp_ClearNodeSlots(&enemy->node);
        work->field_6 = 0;
        Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
    }
    if ((s16)work->field_6 <= 0x1000) {
        work->field_6++;
        if ((s16)work->field_6 % 5 == 0 && (s16)work->field_6 < 100) {
            switch ((s16)((s16)((s16)work->field_6 / 5) % 4)) {
                case 0:
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[1], 0x12600, NULL);
                    break;
                case 1:
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[4], 0x22400, NULL);
                    break;
                case 2:
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[1], 0x32600, NULL);
                    break;
                case 3:
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[18], 0x12500, NULL);
                    break;
            }
        }
        switch ((s16)work->field_6) {
            case 1:
                arg0->extra.tmd->flags = 0;
                Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                break;
            case 0x58:
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[1], 2, NULL);
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[4], 1, NULL);
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[18], 1, NULL);
                break;
            case 0x5A:
                arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                arg0->extra.tmd->texturePageOffset = 2;
                arg0->extra.tmd->clutRowOffset     = 4;
                break;
            case 0x6A:
                arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
        t = work->field_6;
        if (t > 0x46) {
            if ((t - 0x46) * 0x3C < 0x1000) {
                Actor403000_FaceScale(arg0->extra.tmd->coords, 0x1000 - (t - 0x46) * 0x6B);
            } else {
                Actor403000_FaceScale(arg0->extra.tmd->coords, 0);
            }
        } else {
            Actor403000_FaceScale(arg0->extra.tmd->coords, 0x1000);
        }
    }
}

static void func_actor_403000_80136B14(Task* arg0)
{
    Actor403000Work* work;
    Enemy*           enemy;
    TmdObject*       tmd;

    work  = arg0->work;
    tmd   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        tmd->flags = 0;
        Tmd_AllocBuffers(tmd);
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
        enemy->reactionFlags          = 0;
        work->field_ACA               = 0x10;
        work->field_AC6               = 0x1C;
        work->field_AC0               = 2;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_FCA               = 1;
        Gp_ClearNodeSlots(&enemy->node);
        arg0->extra.tmd->otOffset = 8;
        work->field_6             = 0;
    }
    func_actor_403000_80133AF8(arg0);
    if ((s16)work->field_6 < 0x28) {
        work->field_6++;
    }
    switch ((s16)work->field_6) {
        case 2:
            arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[1], 2, NULL);
            break;
        case 5:
            arg0->extra.tmd->coords[12].composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(&arg0->extra.tmd->coords[12]);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[12], 1, NULL);
            break;
        case 15:
            arg0->extra.tmd->coords[16].composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(&arg0->extra.tmd->coords[16]);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[16], 1, NULL);
            break;
        case 30:
            arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[1], 2, NULL);
            arg0->extra.tmd->otOffset = 0;
            break;
    }
}

static void func_actor_403000_80136D68(Task* arg0)
{
    Actor403000Work* work;
    Enemy*           enemy;
    TmdObject*       tmd;

    work  = arg0->work;
    tmd   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        tmd->flags = 0;
        Tmd_AllocBuffers(tmd);
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
        enemy->reactionFlags          = 0;
        work->field_ACA               = 0x10;
        work->field_AC6               = 0x1C;
        work->field_AC0               = 2;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_FCA               = 1;
        Gp_ClearNodeSlots(&enemy->node);
        arg0->extra.tmd->otOffset = 8;
        work->field_6             = 0;
    }
    func_actor_403000_80133AF8(arg0);
    if ((s16)work->field_6 < 300) {
        work->field_6++;
        switch ((s16)work->field_6 % 24) {
            case 10:
            case 13:
            case 18:
            case 21:
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[1], 0x01032600, NULL);
                break;
            default:
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[(s16)((s16)work->field_6 % 24)], 0x01032600, NULL);
                break;
        }
    }
    switch ((s16)work->field_6) {
        case 2:
            arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
            break;
        case 5:
            arg0->extra.tmd->coords[12].composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(&arg0->extra.tmd->coords[12]);
            break;
        case 15:
            arg0->extra.tmd->coords[16].composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(&arg0->extra.tmd->coords[16]);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[16], 1, NULL);
            break;
        case 30:
            arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[1], 2, NULL);
            arg0->extra.tmd->otOffset = 0;
            break;
    }
}

static __inline__ void Actor403000_ScaleVec(SVECTOR* v, u16 k)
{
    gte_lddp(k);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
}

static __inline__ SVECTOR* Actor403000_PushVec(void)
{
    SVECTOR* head;

    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    SCRATCH_STACK_CURSOR(SVECTOR) = head - 1;
    return head - 1;
}

static __inline__ void Actor403000_PopVec(void)
{
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

static inline s8 Actor403000_Cell(GfxCoord* coord)
{
    s32 x;
    s32 z;
    s8  col;
    s8  row;
    s32 cell;

    x = coord->coord.t[0];
    z = coord->coord.t[2];
    if (x < 0xD48) {
        col = 4;
    } else if (x < 0x1A90) {
        col = 3;
    } else if (x < 0x2AF8) {
        col = 2;
    } else {
        col = x < 0x3C8C;
    }
    row  = z >= 0x1068;
    cell = (s8)D_actor_403000_80158D48[col + row * 5];
    return cell;
}

/// Turn toward the camera target (state 4) and, once facing it, walk at it
/// (state 2): hand off to state 8 or 7 by distance, or to state 4 with a
/// fresh `field_FD3` direction when the heading error grows past 0x300.
static void func_actor_403000_80137084(Task* arg0)
{
    Actor403000Work*         work;
    Task*                    player;
    Actor403000ChaseScratch* scratch;
    Actor403000ChaseScratch* head;
    GfxCoord*                coord;
    GfxCoord*                pos;
    GfxCoord*                rot;
    GfxCoord*                pos2;
    GfxCoord*                rot2;
    SVECTOR*                 dir;
    SVECTOR*                 t;
    s16                      angle;
    s32                      mag;
    s16                      diff;
    s32                      dist;
    s8                       sign;
    PlayerStatus*            wip;
    TmdObject*               tmd;

    work                                          = arg0->work;
    player                                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    wip                                           = &gPlayerStatus;
    head                                          = SCRATCH_STACK_CURSOR(Actor403000ChaseScratch);
    SCRATCH_STACK_CURSOR(Actor403000ChaseScratch) = head - 1;
    scratch                                       = head - 1;
    if (work->field_4 != 0) {
        tmd             = arg0->extra.tmd;
        work->field_FCA = 0;
        tmd->flags      = 0;
        Tmd_AllocBuffers(tmd);
        work->objB50.body.radius = 0x3E8;
        work->field_AC0          = 1;
        work->field_ACA          = 0x10;
        work->field_AC6          = 4;
        work->field_AC2          = 0;
        work->field_AD6          = 0;
        work->field_6            = 0;
        work->field_8            = 0;
        work->field_FC2          = 0;
        work->recDD0.ends[1].vz  = 0x384;
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    func_actor_403000_80133AF8(arg0);
    if (work->field_AC6 == 4) {
        t     = &scratch->target;
        pos   = arg0->extra.tmd->coords;
        t->vx = wip->coordMtx->t[0] - pos->coord.t[0];
        t->vy = wip->coordMtx->t[1] - pos->coord.t[1];
        t->vz = wip->coordMtx->t[2] - pos->coord.t[2];
        rot   = arg0->extra.tmd->coords;
        angle = ratan2(t->vx, t->vz) - ratan2(-rot->coord.m[2][0], rot->coord.m[2][2]);
        if (angle < 0) {
        loop_neg:
            if (angle < -0x800) {
                angle += 0x1000;
                goto loop_neg;
            }
        } else {
        loop_pos:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto loop_pos;
            }
        }
        scratch->angle = mag = angle;
        work->field_AD8      = mag;
        if (scratch->angle > 0x40) {
            scratch->angle = 0x40;
        } else if (scratch->angle < -0x40) {
            scratch->angle = -0x40;
        }
        scratch->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->angle, 1);
        if (ABS(work->field_AD8) < 0x80) {
            work->field_AC6 = 2;
            work->field_AC0 = 1;
        }
        ActorContact_PushContact(arg0->extra.tmd->coords, work->objD18.contacts, ARRAY_SIZE(work->objD18.contacts));
    }
    if (work->field_AC6 == 2) {
        ActorContact_PushContact(arg0->extra.tmd->coords, work->objD18.contacts, ARRAY_SIZE(work->objD18.contacts));
        t     = &scratch->target;
        pos2  = arg0->extra.tmd->coords;
        t->vx = gPlayerStatus.coordMtx->t[0] - pos2->coord.t[0];
        t->vy = gPlayerStatus.coordMtx->t[1] - pos2->coord.t[1];
        t->vz = gPlayerStatus.coordMtx->t[2] - pos2->coord.t[2];
        rot2  = arg0->extra.tmd->coords;
        angle = ratan2(t->vx, t->vz) - ratan2(-rot2->coord.m[2][0], rot2->coord.m[2][2]);
        if (angle < 0) {
        loop_neg2:
            if (angle < -0x800) {
                angle += 0x1000;
                goto loop_neg2;
            }
        } else {
        loop_pos2:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto loop_pos2;
            }
        }
        scratch->angle = mag = angle;
        work->field_AD8      = mag;
        coord                = arg0->extra.tmd->coords;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
            dir = Actor403000_PushVec();
            gfxReadMatrixZAxis(&coord->coord, dir);
            VectorNormalSS(dir, dir);
            Actor403000_ScaleVec(dir, 300);
            coord->coord.t[0]  += dir->vx;
            coord->coord.t[1]  += dir->vy;
            coord->coord.t[2]  += dir->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Actor403000_PopVec();
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        scratch->d.vx                         = wip->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
        scratch->d.vy                         = 0;
        scratch->d.vz                         = wip->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
        scratch->dist = dist = SquareRoot0(scratch->d.vx * scratch->d.vx + scratch->d.vy * scratch->d.vy + scratch->d.vz * scratch->d.vz);
        dist                -= 0x1964;
        if (dist < 0) {
            dist = -dist;
        }
        if (dist < 0x1F4 && ABS(scratch->angle) < 0x200) {
            work->field_0 = 8;
        }
        if (scratch->dist < 0x1770 && ABS(scratch->angle) < 0x200) {
            work->field_0 = 7;
        }
        if (ABS(work->field_AD8) > 0x300) {
            scratch->playerCell = Actor403000_Cell(player->extra.tmd->coords);
            scratch->cell       = Actor403000_Cell(arg0->extra.tmd->coords);
            work->field_0       = 4;
            diff                = scratch->cell - scratch->playerCell;
            if (diff < -5) {
                goto neg;
            }
            if (diff < 0) {
                goto pos;
            }
            if (diff < 5) {
            neg:
                sign = -1;
            } else {
            pos:
                sign = 1;
            }
            work->field_FD5 = work->field_FD3 = -sign;
        }
        if (scratch->angle > 0x40) {
            scratch->angle = 0x40;
        } else if (scratch->angle < -0x40) {
            scratch->angle = -0x40;
        }
        scratch->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->angle, 1);
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000ChaseScratch);
}

/// Grab approach (animation 2 then 0xB): on the frame `field_4` is set, record
/// the player's offset; walk forward until the player is within 0xDAC, then
/// switch to the lunge clip with a per-frame step of a fifteenth of the distance.
/// On frame 10 of the lunge, if a hit record is live and the player accepts
/// message 0x3F8, snap the model in front of the player and send the grab
/// (front or back by the facing difference). `ANIMATION_SLOT_SETTLED` in `field_60` ends the
/// lunge in state 4 with a fresh `field_FD3` direction.
static void func_actor_403000_801377C8(Task* arg0)
{
    Actor403000Work*        work;
    Task*                   player;
    Enemy*                  enemy;
    Actor403000GrabScratch* scratch;
    Actor403000GrabScratch* head;
    GfxCoord*               coord;
    GfxCoord*               coord2;
    GfxCoord*               pos;
    GfxCoord*               rot;
    SVECTOR*                dir;
    SVECTOR*                t;
    SVECTOR*                v;
    SVECTOR*                dirA;
    SVECTOR*                dirB;
    Task*                   task;
    GameActor*              pw;
    WorldCollisionContact*  recs;
    s16                     angle;
    s16                     step;
    s16                     i;
    s16                     diff;
    s32                     found;
    s32                     value;
    s32                     mag;
    s8                      sign;
    s16                     cell;

    work                                         = arg0->work;
    player                                       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                                         = SCRATCH_STACK_CURSOR(Actor403000GrabScratch);
    SCRATCH_STACK_CURSOR(Actor403000GrabScratch) = head - 1;
    enemy                                        = arg0->spawnArg2.pointer;
    scratch                                      = head - 1;
    if (work->field_4 != 0) {
        work->field_F74 = player->extra.tmd->coords->coord.t[0] - arg0->extra.tmd->coords->coord.t[0];
        work->field_F76 = player->extra.tmd->coords->coord.t[1] - arg0->extra.tmd->coords->coord.t[1];
        work->field_F78 = player->extra.tmd->coords->coord.t[2] - arg0->extra.tmd->coords->coord.t[2];
        work->field_AC6 = 2;
        work->field_AC0 = 1;
        work->field_6   = 0;
    }
    if (work->field_AC6 == 2) {
        scratch->d.vx = player->extra.tmd->coords->coord.t[0] - arg0->extra.tmd->coords->coord.t[0];
        scratch->d.vy = 0;
        scratch->d.vz = player->extra.tmd->coords->coord.t[2] - arg0->extra.tmd->coords->coord.t[2];
        scratch->dist = SquareRoot0(scratch->d.vx * scratch->d.vx + scratch->d.vy * scratch->d.vy + scratch->d.vz * scratch->d.vz);
        if (scratch->dist < 0xDAC) {
            work->field_AC6       = 0xB;
            work->field_AC0       = 2;
            *(s16*)&work->field_6 = -1;
            work->field_F86       = scratch->dist / 15;
        }
    }
    if (work->field_AC6 == 0xB) {
        if ((s16)work->field_6 == 0xA) {
            t     = &scratch->target;
            pos   = arg0->extra.tmd->coords;
            t->vx = gPlayerStatus.coordMtx->t[0] - pos->coord.t[0];
            t->vy = gPlayerStatus.coordMtx->t[1] - pos->coord.t[1];
            t->vz = gPlayerStatus.coordMtx->t[2] - pos->coord.t[2];
            rot   = arg0->extra.tmd->coords;
            angle = ratan2(t->vx, t->vz) - ratan2(-rot->coord.m[2][0], rot->coord.m[2][2]);
            if (angle < 0) {
                for (;;) {
                    if (angle >= -0x800) {
                        goto wrapped;
                    }
                    angle += 0x1000;
                }
            } else {
                for (;;) {
                    if (angle <= 0x800) {
                        goto wrapped;
                    }
                    angle -= 0x1000;
                }
            }
        wrapped:
            scratch->angle = mag = angle;
            if (ABS(mag) < 0x400) {
                recs = work->objC80.contacts;
                for (i = 0; i < ARRAY_SIZE(work->objC80.contacts); i++) {
                    value = recs[i].key.value;
                    if (value == 0) {
                        break;
                    }
                    if ((value & 0xFFFF0000) == 0x10000) {
                        found = 1;
                        goto done;
                    }
                }
                found = 0;
            done:
                if (found != 0 && enemy->hp > 0 && TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_403000_80158DD0.value, 0) == 0) {
                    work->field_0      = 6;
                    work->field_FC0    = 1;
                    work->field_F74    = player->extra.tmd->coords->coord.t[0] - arg0->extra.tmd->coords->coord.t[0];
                    work->field_F76    = player->extra.tmd->coords->coord.t[1] - arg0->extra.tmd->coords->coord.t[1];
                    work->field_F78    = player->extra.tmd->coords->coord.t[2] - arg0->extra.tmd->coords->coord.t[2];
                    scratch->playerYaw = ratan2(-gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][0], gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][2]);
                    t                  = &scratch->target;
                    pos                = arg0->extra.tmd->coords;
                    t->vx              = gPlayerStatus.coordMtx->t[0] - pos->coord.t[0];
                    t->vy              = gPlayerStatus.coordMtx->t[1] - pos->coord.t[1];
                    t->vz              = gPlayerStatus.coordMtx->t[2] - pos->coord.t[2];
                    scratch->aimYaw    = ratan2(scratch->target.vx, scratch->target.vz) + 0x800;
                    angle              = scratch->aimYaw;
                    if (angle < 0) {
                        for (;;) {
                            if (angle >= -0x800) {
                                goto wrapped2;
                            }
                            angle += 0x1000;
                        }
                    } else {
                        for (;;) {
                            if (angle <= 0x800) {
                                goto wrapped2;
                            }
                            angle -= 0x1000;
                        }
                    }
                wrapped2:
                    scratch->aimYaw = mag = angle;
                    mag                  -= scratch->playerYaw;
                    if (ABS(mag) < 0x400) {
                        work->field_F90 = D_actor_403000_80158C08;
                        work->field_F94 = 3;
                        work->field_F98 = 0;
                        work->field_F9C = 0;
                        work->field_FA0 = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_F90, 0);
                        dir = &scratch->target;
                        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, dir);
                        VectorNormalSS(dir, dir);
                        gte_lddp(-0x546);
                        gte_ldsv(dir);
                        gte_gpf12();
                        gte_stsv(dir);
                        arg0->extra.tmd->coords->coord.t[0]   = player->extra.tmd->coords->coord.t[0] + scratch->target.vx;
                        arg0->extra.tmd->coords->coord.t[1]   = player->extra.tmd->coords->coord.t[1] + scratch->target.vy;
                        arg0->extra.tmd->coords->coord.t[2]   = player->extra.tmd->coords->coord.t[2] + scratch->target.vz;
                        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                        D_actor_403000_80158D90.value.pos.vx  = player->extra.tmd->coords->coord.t[0];
                        D_actor_403000_80158D90.value.pos.vy  = player->extra.tmd->coords->coord.t[1];
                        D_actor_403000_80158D90.value.pos.vz  = player->extra.tmd->coords->coord.t[2];
                        D_actor_403000_80158D90.value.rot.vx  = 0;
                        D_actor_403000_80158D90.value.rot.vy  = ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]) - 0x500;
                        D_actor_403000_80158D90.value.rot.vz  = 0;
                        TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &D_actor_403000_80158D90.value, 0);
                    } else {
                        work->field_F90 = D_actor_403000_80158C28;
                        work->field_F94 = 3;
                        work->field_F98 = 0;
                        work->field_F9C = 0;
                        work->field_FA0 = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_F90, 0);
                        dir = &scratch->target;
                        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, dir);
                        VectorNormalSS(dir, dir);
                        gte_lddp(-0x546);
                        gte_ldsv(dir);
                        gte_gpf12();
                        gte_stsv(dir);
                        arg0->extra.tmd->coords->coord.t[0]   = player->extra.tmd->coords->coord.t[0] + scratch->target.vx;
                        arg0->extra.tmd->coords->coord.t[1]   = player->extra.tmd->coords->coord.t[1] + scratch->target.vy;
                        arg0->extra.tmd->coords->coord.t[2]   = player->extra.tmd->coords->coord.t[2] + scratch->target.vz;
                        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                        D_actor_403000_80158D90.value.pos.vx  = player->extra.tmd->coords->coord.t[0];
                        D_actor_403000_80158D90.value.pos.vy  = player->extra.tmd->coords->coord.t[1];
                        D_actor_403000_80158D90.value.pos.vz  = player->extra.tmd->coords->coord.t[2];
                        D_actor_403000_80158D90.value.rot.vx  = 0;
                        D_actor_403000_80158D90.value.rot.vy  = ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]) + 0x400;
                        D_actor_403000_80158D90.value.rot.vz  = 0;
                        TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &D_actor_403000_80158D90.value, 0);
                    }
                    task         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    scratch->ret = taskMessageDispatch(task, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 1), 0);
                    if (scratch->ret == 1) {
                        pw                              = (GameActor*)player->work;
                        gGameSession->deathFadeFrames   = 0x28;
                        gGameSession->deathRestartDelay = 0x28;
                        pw->state                       = 0xA;
                    }
                }
            }
        }
    }
    if (work->field_AC6 == 2) {
        coord = arg0->extra.tmd->coords;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
            dirA = Actor403000_PushVec();
            gfxReadMatrixZAxis(&coord->coord, dirA);
            VectorNormalSS(dirA, dirA);
            Actor403000_ScaleVec(dirA, 300);
            coord->coord.t[0]  += dirA->vx;
            coord->coord.t[1]  += dirA->vy;
            coord->coord.t[2]  += dirA->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Actor403000_PopVec();
        }
    }
    if (work->field_AC6 == 0xB && (s16)work->field_6 < 0xE) {
        coord2 = arg0->extra.tmd->coords;
        step   = work->field_F86;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
            dirB = Actor403000_PushVec();
            v    = dirB;
            if (step != 0) {
                gfxReadMatrixZAxis(&coord2->coord, dirB);
                VectorNormalSS(dirB, dirB);
                Actor403000_ScaleVec(v, step);
                coord2->coord.t[0]  += dirB->vx;
                coord2->coord.t[1]  += dirB->vy;
                coord2->coord.t[2]  += dirB->vz;
                coord2->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            Actor403000_PopVec();
        }
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->objD18.contacts, ARRAY_SIZE(work->objD18.contacts)) == 1 && work->field_AC6 == 2 && (s16)work->field_6 >= 0x10) {
        work->field_0   = 4;
        work->field_FD5 = -work->field_FD3;
    }
    func_actor_403000_80133AF8(arg0);
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if (work->field_AC6 == 0xB) {
            scratch->playerCell = Actor403000_Cell(player->extra.tmd->coords);
            cell                = Actor403000_Cell(arg0->extra.tmd->coords);
            scratch->cell       = cell;
            diff                = scratch->cell - scratch->playerCell;
            if (diff < -5) {
                goto neg1;
            }
            if (diff < 0) {
                goto pos1;
            }
            if (diff < 5) {
            neg1:
                sign = -1;
            } else {
            pos1:
                sign = 1;
            }
            work->field_FD3 = sign;
            diff            = scratch->cell - scratch->playerCell;
            if (diff < -5) {
                goto neg2;
            }
            if (diff < 0) {
                goto pos2;
            }
            if (diff < 5) {
            neg2:
                sign = -1;
            } else {
            pos2:
                sign = 1;
            }
            work->field_FD5 = work->field_FD3 = -sign;
            work->field_0                     = 4;
        }
    }
    work->field_6++;
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000GrabScratch);
}

/// Per-frame push: on the frame `field_4` is set, turn the display object's
/// first matrix column into a short push vector and play the enemy's sound,
/// then send it to the player as `GAME_ACTOR_MESSAGE_MOVE_BY` for the first 0x28 frames.
/// `ANIMATION_SLOT_REACHED_BOUNDARY` in `field_60` moves the state machine to 4 and flips `field_FD3`.
static void func_actor_403000_801384E8(Task* arg0)
{
    Actor403000Work*        work;
    Enemy*                  enemy;
    Task*                   player;
    Actor403000PushScratch* scratch;
    s32                     sound;
    s32                     pan;
    s32                     ret;

    work                                         = arg0->work;
    player                                       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch                                      = SCRATCH_STACK_CURSOR(Actor403000PushScratch) - 1;
    SCRATCH_STACK_CURSOR(Actor403000PushScratch) = scratch;
    if (work->field_4 != 0) {
        enemy         = arg0->spawnArg2.pointer;
        work->field_6 = 0;
        Gfx_MatrixCol0(&arg0->extra.tmd->coords->coord, &scratch->dir);
        VectorNormalSS(&scratch->dir, &scratch->dir);
        gte_lddp(0x55);
        gte_ldsv(&scratch->dir);
        gte_gpf12();
        gte_stsv(&scratch->dir);
        D_actor_403000_80158DB0.value.displacement.vx   = scratch->dir.vx;
        D_actor_403000_80158DB0.value.displacement.vy   = 0;
        D_actor_403000_80158DB0.value.displacement.vz   = scratch->dir.vz;
        D_actor_403000_80158DB0.value.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        D_actor_403000_80158DB0.value.keepControl       = 1;
        sound                                           = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 7;
        pan                                             = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->field_6 < 0x28) {
        ret = TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &D_actor_403000_80158DB0.value, 0);
        if (ret == 1) {
            D_actor_403000_80158DB0.value.displacement.vx   = 0;
            D_actor_403000_80158DB0.value.displacement.vy   = 0;
            D_actor_403000_80158DB0.value.displacement.vz   = 0;
            D_actor_403000_80158DB0.value.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
            D_actor_403000_80158DB0.value.keepControl       = ret;
        }
    }
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_0   = 4;
        work->field_FD3 = work->field_FD5 = work->field_FD2 = -work->field_FD3;
    }
    func_actor_403000_80133AF8(arg0);
    work->field_6++;
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000PushScratch);
}

/// Lunge toward the player: on the frame `field_4` is set, record the player's
/// position and turn the distance to it into the per-frame step `field_F84`;
/// for frames 5..24 push the display object along its third matrix column by
/// that step, for frames 5..14 turn it toward the camera target by at most
/// 0x40, and on frame 0x17 send the grab messages if a hit record is live.
static void func_actor_403000_801386E8(Task* arg0)
{
    Actor403000Work*         work;
    Task*                    player;
    Enemy*                   enemy;
    Actor403000LungeScratch* scratch;
    Actor403000LungeScratch* head;
    GfxCoord*                coord;
    SVECTOR*                 dir;
    Task*                    task;
    s16                      step;
    s16                      angle;
    s16                      i;
    s32                      found;
    s32                      value;
    s32                      dist;
    s32                      mag;
    SVECTOR*                 t;
    GfxCoord*                coord2;
    GfxCoord*                coord3;
    WorldCollisionContact*   recs;
    GameActor*               pw;

    work                                          = arg0->work;
    player                                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                                          = SCRATCH_STACK_CURSOR(Actor403000LungeScratch);
    SCRATCH_STACK_CURSOR(Actor403000LungeScratch) = head - 1;
    enemy                                         = arg0->spawnArg2.pointer;
    scratch                                       = head - 1;
    if (work->field_4 != 0) {
        work->field_F7C = player->extra.tmd->coords->coord.t[0];
        work->field_F7E = player->extra.tmd->coords->coord.t[1];
        work->field_F80 = player->extra.tmd->coords->coord.t[2];
        scratch->d.vx   = player->extra.tmd->coords->coord.t[0] - arg0->extra.tmd->coords->coord.t[0];
        scratch->d.vy   = 0;
        scratch->d.vz   = player->extra.tmd->coords->coord.t[2] - arg0->extra.tmd->coords->coord.t[2];
        scratch->dist = dist     = SquareRoot0(scratch->d.vx * scratch->d.vx + scratch->d.vy * scratch->d.vy + scratch->d.vz * scratch->d.vz);
        work->field_AC6          = 7;
        work->field_AC0          = 1;
        work->field_6            = 0;
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->objC80.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_F84          = (dist - 900) / 20;
    }
    if (work->field_6 >= 5 && work->field_6 < 25) {
        coord = arg0->extra.tmd->coords;
        step  = work->field_F84;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
            dir = Actor403000_PushVec();
            if (step != 0) {
                gfxReadMatrixZAxis(&coord->coord, dir);
                VectorNormalSS(dir, dir);
                Actor403000_ScaleVec(dir, step);
                coord->coord.t[0]  += dir->vx;
                coord->coord.t[1]  += dir->vy;
                coord->coord.t[2]  += dir->vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            Actor403000_PopVec();
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if ((u16)(work->field_6 - 5) < 10) {
        t      = &scratch->target;
        coord3 = arg0->extra.tmd->coords;
        t->vx  = gPlayerStatus.coordMtx->t[0] - coord3->coord.t[0];
        t->vy  = gPlayerStatus.coordMtx->t[1] - coord3->coord.t[1];
        t->vz  = gPlayerStatus.coordMtx->t[2] - coord3->coord.t[2];
        coord2 = arg0->extra.tmd->coords;
        angle  = ratan2(t->vx, t->vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
        if (angle < 0) {
        loop_neg:
            if (angle < -0x800) {
                angle += 0x1000;
                goto loop_neg;
            }
        } else {
        loop_pos:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto loop_pos;
            }
        }
        scratch->angle = mag = angle;
        if (scratch->angle > 0x40) {
            scratch->angle = 0x40;
        } else if (scratch->angle < -0x40) {
            scratch->angle = -0x40;
        }
        scratch->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->angle, 1);
    }
    recs = work->recordsE98;
    if ((s16)work->field_6 == 0x17) {
        for (i = 0; i < 5; i++) {
            value = recs[i].key.value;
            if (value == 0) {
                break;
            }
            if ((value & 0xFFFF0000) == 0x10000) {
                found = 1;
                goto done;
            }
        }
        found = 0;
    done:
        if (found != 0 && enemy->hp > 0 && TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_403000_80158DD0.value, 0) == 0) {
            D_actor_403000_80158D90.value.pos.vx = player->extra.tmd->coords->coord.t[0];
            D_actor_403000_80158D90.value.pos.vy = player->extra.tmd->coords->coord.t[1];
            D_actor_403000_80158D90.value.pos.vz = player->extra.tmd->coords->coord.t[2];
            D_actor_403000_80158D90.value.rot.vx = 0;
            D_actor_403000_80158D90.value.rot.vy = ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]) - 0x400;
            D_actor_403000_80158D90.value.rot.vz = 0;
            TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &D_actor_403000_80158D90.value, 0);
            task         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            scratch->ret = taskMessageDispatch(task, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 0), 0);
            if (scratch->ret == 1) {
                pw                              = (GameActor*)player->work;
                gGameSession->deathFadeFrames   = 0x1C;
                gGameSession->deathRestartDelay = 0x1E;
                pw->state                       = 0xA;
            }
            work->field_0   = 9;
            work->field_F90 = D_actor_403000_80158C08;
            work->field_F94 = 1;
            work->field_F98 = 0;
            work->field_F9C = 3;
            work->field_FA0 = 1;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_F90, 0);
            work->field_FC0 = 1;
        }
    }
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->field_0   = 2;
        work->field_FD2 = work->field_FD3;
        work->field_FD3 = -work->field_FD3;
    }
    func_actor_403000_80133AF8(arg0);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->objD18.contacts, ARRAY_SIZE(work->objD18.contacts)) == 0) {
        ActorContact_PushContact(arg0->extra.tmd->coords, work->objC80.contacts, ARRAY_SIZE(work->objC80.contacts));
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000LungeScratch);
    work->field_6++;
}

/// Lunge-and-shove (animation 0x11): on the frame `field_4` is set, place the
/// model a fixed distance short of the player along its facing, turn it toward
/// the camera target and start the hit effect; for the first ten frames step it
/// forward and push the player with `GAME_ACTOR_MESSAGE_MOVE_BY`, turn for the first six, and
/// on frame 0x29 aim a sideways push that frames 0x2C..0x35 keep resending.
/// `ANIMATION_SLOT_REACHED_BOUNDARY` in `field_60` moves the state machine to 4 with a fresh `field_FD3`.
static void func_actor_403000_80138DB0(Task* arg0)
{
    Actor403000Work*         work;
    Task*                    player;
    Actor403000ChaseScratch* scratch;
    Actor403000ChaseScratch* head;
    GfxCoord*                coord;
    GfxCoord*                rot;
    GfxCoord*                pos;
    GfxCoord*                pos2;
    SVECTOR*                 dir;
    SVECTOR*                 t1;
    SVECTOR*                 vp;
    SVECTOR*                 t2;
    SVECTOR*                 t3;
    SVECTOR*                 t4;
    SVECTOR                  v;
    s16                      angle;
    s16                      diff;
    s32                      dist;
    s32                      ret;
    s8                       sign;
    s8                       sign2;
    s16                      cell;

    work                                          = arg0->work;
    player                                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                                          = SCRATCH_STACK_CURSOR(Actor403000ChaseScratch);
    SCRATCH_STACK_CURSOR(Actor403000ChaseScratch) = head - 1;
    scratch                                       = head - 1;
    if (work->field_4 != 0) {
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_F7C          = player->extra.tmd->coords->coord.t[0];
        work->field_F7E          = player->extra.tmd->coords->coord.t[1];
        work->field_F80          = player->extra.tmd->coords->coord.t[2];
        scratch->d.vx            = player->extra.tmd->coords->coord.t[0] - arg0->extra.tmd->coords->coord.t[0];
        scratch->d.vy            = 0;
        scratch->d.vz            = player->extra.tmd->coords->coord.t[2] - arg0->extra.tmd->coords->coord.t[2];
        scratch->dist = dist = SquareRoot0(scratch->d.vx * scratch->d.vx + scratch->d.vy * scratch->d.vy + scratch->d.vz * scratch->d.vz);
        work->field_AC6      = 0x11;
        work->field_AC0      = 2;
        work->field_6        = 0;
        work->field_F84      = (dist - 900) / 20;
        t1                   = &scratch->target;
        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, t1);
        VectorNormalSS(t1, t1);
        Actor403000_ScaleVec(t1, 0x41A);
        arg0->extra.tmd->coords->coord.t[0] = player->extra.tmd->coords->coord.t[0] - scratch->target.vx;
        arg0->extra.tmd->coords->coord.t[1] = player->extra.tmd->coords->coord.t[1] - scratch->target.vy;
        arg0->extra.tmd->coords->coord.t[2] = player->extra.tmd->coords->coord.t[2] - scratch->target.vz;
        pos                                 = arg0->extra.tmd->coords;
        t1->vx                              = gPlayerStatus.coordMtx->t[0] - pos->coord.t[0];
        t1->vy                              = gPlayerStatus.coordMtx->t[1] - pos->coord.t[1];
        t1->vz                              = gPlayerStatus.coordMtx->t[2] - pos->coord.t[2];
        rot                                 = arg0->extra.tmd->coords;
        angle                               = ratan2(t1->vx, t1->vz) - ratan2(-rot->coord.m[2][0], rot->coord.m[2][2]);
        if (angle < 0) {
        loop_neg:
            if (angle < -0x800) {
                angle += 0x1000;
                goto loop_neg;
            }
        } else {
        loop_pos:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto loop_pos;
            }
        }
        scratch->angle  = angle;
        scratch->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->angle, 1);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        t2                                    = &scratch->target;
        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, t2);
        VectorNormalSS(t2, t2);
        Actor403000_ScaleVec(t2, 0x96);
        D_actor_403000_80158DB0.value.displacement.vx   = scratch->target.vx;
        D_actor_403000_80158DB0.value.displacement.vy   = 0;
        D_actor_403000_80158DB0.value.displacement.vz   = scratch->target.vz;
        D_actor_403000_80158DB0.value.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        D_actor_403000_80158DB0.value.keepControl       = 1;
        Gp_SpawnPadLerp(5, 0xFF, 0x80);
        work->field_FA8.coord      = &player->extra.tmd->coords[3];
        work->field_FA8.spawnArgLo = 0x500;
        work->field_FA8.spawnArgHi = 3;
        func_800FDB18(Gp_GetIdParam1(0x100F) & 0xFFFF, &player->extra.tmd->coords[3], 0, &work->field_FA8);
    }
    if ((s16)work->field_6 < 10) {
        coord = arg0->extra.tmd->coords;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
            dir = Actor403000_PushVec();
            gfxReadMatrixZAxis(&coord->coord, dir);
            VectorNormalSS(dir, dir);
            Actor403000_ScaleVec(dir, 0x78);
            coord->coord.t[0]  += dir->vx;
            coord->coord.t[1]  += dir->vy;
            coord->coord.t[2]  += dir->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Actor403000_PopVec();
        }
        v.vz = 0;
        v.vy = 0;
        v.vx = 0;
        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, &v);
        vp = &v;
        VectorNormalSS(vp, vp);
        Actor403000_ScaleVec(vp, 0x546);
        v.vx = v.vx + arg0->extra.tmd->coords->coord.t[0] - player->extra.tmd->coords->coord.t[0];
        v.vy = 0;
        v.vz = v.vz + arg0->extra.tmd->coords->coord.t[2] - player->extra.tmd->coords->coord.t[2];
        VectorNormalSS(vp, vp);
        Actor403000_ScaleVec(vp, 0x96);
        D_actor_403000_80158DB0.value.displacement.vy   = 0;
        D_actor_403000_80158DB0.value.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        D_actor_403000_80158DB0.value.keepControl       = 1;
        D_actor_403000_80158DB0.value.displacement.vx   = v.vx;
        D_actor_403000_80158DB0.value.displacement.vz   = v.vz;
        TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &D_actor_403000_80158DB0.value, 0);
    }
    if ((s16)work->field_6 < 6) {
        t3     = &scratch->target;
        pos2   = arg0->extra.tmd->coords;
        t3->vx = gPlayerStatus.coordMtx->t[0] - pos2->coord.t[0];
        t3->vy = gPlayerStatus.coordMtx->t[1] - pos2->coord.t[1];
        t3->vz = gPlayerStatus.coordMtx->t[2] - pos2->coord.t[2];
        rot    = arg0->extra.tmd->coords;
        angle  = ratan2(t3->vx, t3->vz) - ratan2(-rot->coord.m[2][0], rot->coord.m[2][2]);
        if (angle < 0) {
        loop_neg2:
            if (angle < -0x800) {
                angle += 0x1000;
                goto loop_neg2;
            }
        } else {
        loop_pos2:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto loop_pos2;
            }
        }
        scratch->angle  = angle;
        scratch->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->angle, 1);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if ((s16)work->field_6 == 0x29) {
        t4 = &scratch->target;
        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, t4);
        VectorNormalSS(t4, t4);
        Actor403000_ScaleVec(t4, 0x21);
        D_actor_403000_80158DB0.value.displacement.vx = scratch->target.vx;
        D_actor_403000_80158DB0.value.displacement.vy = 0;
        D_actor_403000_80158DB0.value.displacement.vz = scratch->target.vz;
        Gfx_MatrixCol0(&arg0->extra.tmd->coords->coord, t4);
        VectorNormalSS(t4, t4);
        Actor403000_ScaleVec(t4, 0x7D);
        D_actor_403000_80158DB0.value.displacement.vx  += scratch->target.vx;
        D_actor_403000_80158DB0.value.displacement.vz  += scratch->target.vz;
        D_actor_403000_80158DB0.value.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        D_actor_403000_80158DB0.value.keepControl       = 1;
    }
    if ((u16)(work->field_6 - 0x2C) < 10) {
        ret = TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &D_actor_403000_80158DB0.value, 0);
        if (ret == 1) {
            D_actor_403000_80158DB0.value.displacement.vx   = 0;
            D_actor_403000_80158DB0.value.displacement.vy   = 0;
            D_actor_403000_80158DB0.value.displacement.vz   = 0;
            D_actor_403000_80158DB0.value.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
            D_actor_403000_80158DB0.value.keepControl       = ret;
        }
    }
    if ((s16)work->field_6 == 0x39) {
        work->field_F90 = D_actor_403000_80158C08;
        work->field_F94 = 2;
        work->field_F98 = 0;
        work->field_F9C = 0;
        work->field_FA0 = 1;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_F90, 0);
    }
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        scratch->playerCell = Actor403000_Cell(player->extra.tmd->coords);
        cell                = Actor403000_Cell(arg0->extra.tmd->coords);
        scratch->cell       = cell;
        diff                = (s8)cell - scratch->playerCell;
        if (diff < -5) {
            goto neg;
        }
        if (diff < 0) {
            goto pos;
        }
        if (diff < 5) {
        neg:
            sign = -1;
        } else {
        pos:
            sign = 1;
        }
        work->field_FD3 = sign;
        diff            = scratch->cell - scratch->playerCell;
        if (diff < -5) {
            goto neg2;
        }
        if (diff < 0) {
            goto pos2;
        }
        if (diff < 5) {
        neg2:
            sign2 = -1;
        } else {
        pos2:
            sign2 = 1;
        }
        work->field_FD5 = work->field_FD3 = -sign2;
        work->field_0                     = 4;
    }
    func_actor_403000_80133AF8(arg0);
    if ((s16)++work->field_6 < 10) {
        ActorContact_PushContact(arg0->extra.tmd->coords, work->objD18.contacts, ARRAY_SIZE(work->objD18.contacts));
        ActorContact_PushContact(arg0->extra.tmd->coords, work->objC80.contacts, ARRAY_SIZE(work->objC80.contacts));
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000ChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_actor_403000_801399A0(Task* arg0)
{
    Actor403000Work* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags   = 0;
        work->objB50.body.radius = 0x3E8;
        work->field_AC0          = 1;
        work->field_AC6          = 0xE;
        work->field_ACA          = 0x10;
        work->field_FCA          = 0;
        work->field_AE0          = 0;
        work->field_AD8          = 0;
        work->field_AD6          = 0;
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->objB50.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->objD18.contacts, ARRAY_SIZE(work->objD18.contacts)) == 0) {
        ActorContact_PushContact(arg0->extra.tmd->coords, work->objB50.contacts, ARRAY_SIZE(work->objB50.contacts));
    }
    func_actor_403000_80133AF8(arg0);
    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) && work->field_AC6 == 0xE) {
        work->objB50.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp > 0) {
            if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
                work->field_0 = 0x10;
            } else {
                work->field_0 = 0x12;
            }
        } else {
            work->field_F8C = 1;
            work->field_0   = 0x14;
        }
    }
}

/// Waypoint-grid cell under `coord`: column by `coord.t[0]` band, row by
/// `coord.t[2]`, as `func_actor_403000_80134204` computes it inline.
static void func_actor_403000_80139AE0(Task* arg0)
{
    Actor403000Work*        work;
    Task*                   player;
    Actor403000SeekScratch* scratch;
    TmdObject*              obj;
    GfxCoord*               coord;
    SVECTOR*                table;
    SVECTOR*                v;
    s16                     angle;
    s16                     diff;
    s32                     mag;
    s8                      base;
    s8                      dir;

    work    = arg0->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(Actor403000SeekScratch);
    if (work->field_4 != 0) {
        obj             = arg0->extra.tmd;
        work->field_FCA = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->objB50.body.radius = 0x3E8;
        work->field_AC0          = 1;
        work->field_ACA          = 0x10;
        work->field_AC2          = 0;
        work->field_AC6          = 2;
        work->field_AD6          = 0;
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_403000_80133AF8(arg0);
        work->field_6           = 0;
        work->field_8           = 0;
        work->field_FC2         = 0;
        work->recDD0.ends[1].vz = 0x320;
        scratch->facing         = Actor403000_Cell(player->extra.tmd->coords);
        scratch->base           = Actor403000_Cell(arg0->extra.tmd->coords);
        diff                    = scratch->base - scratch->facing;
        if (diff < -5) {
            goto neg;
        }
        if (diff < 0) {
            goto pos;
        }
        if (diff < 5) {
        neg:
            dir = -1;
        } else {
        pos:
            dir = 1;
        }
        work->field_FD1 = dir;
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->objD18.contacts, ARRAY_SIZE(work->objD18.contacts));
    scratch->facing = Actor403000_Cell(player->extra.tmd->coords);
    base            = Actor403000_Cell(arg0->extra.tmd->coords);
    scratch->base   = base;
    if (func_actor_403000_80133FC0(arg0, base, scratch->facing) << 16) {
        work->field_0 = 5;
    }
    scratch->index = scratch->base + work->field_FD1;
    if (scratch->index != -1) {
        if (scratch->index == 10) {
            scratch->index = 0;
        }
    } else {
        scratch->index = 9;
    }
    table            = D_actor_403000_80158CE0;
    v                = &table[scratch->index];
    scratch->vec.vx  = v->vx;
    scratch->vec.vy  = v->vy;
    scratch->vec.vz  = v->vz;
    scratch->vec.vx -= arg0->extra.tmd->coords->coord.t[0];
    scratch->vec.vy  = 0;
    scratch->vec.vz -= arg0->extra.tmd->coords->coord.t[2];
    coord            = arg0->extra.tmd->coords;
    angle            = ratan2(scratch->vec.vx, scratch->vec.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    mag             = angle;
    scratch->angle  = mag;
    work->field_AD8 = mag;
    func_actor_403000_80133AF8(arg0);
    if (scratch->angle > 0x30) {
        scratch->angle = 0x30;
    }
    if (scratch->angle < -0x30) {
        scratch->angle = -0x30;
    }
    scratch->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->angle, 1);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorMoveForward(arg0->extra.tmd->coords, 0x12C);
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000SeekScratch);
}

/// Waypoint-ring variant of `func_actor_403000_80139AE0`: on entry pick the
/// ring direction from the player/model cell difference, then steer (clamped to
/// 0x40 per frame) toward the next waypoint and step forward 0x12C; switch to 5
/// once `func_actor_403000_80133FC0` allows it after 60 frames.
static void func_actor_403000_8013A08C(Task* arg0)
{
    Actor403000Work*        work;
    Task*                   player;
    Actor403000SeekScratch* scratch;
    TmdObject*              obj;
    GfxCoord*               coord;
    SVECTOR*                table;
    SVECTOR*                v;
    s16                     angle;
    s16                     diff;
    s32                     mag;
    s32                     cell;
    s8                      base;
    s8                      dir;

    work    = arg0->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(Actor403000SeekScratch);
    if (work->field_4 != 0) {
        obj             = arg0->extra.tmd;
        work->field_FCA = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->objB50.body.radius = 0x3E8;
        work->field_AC0          = 1;
        work->field_ACA          = 0x10;
        work->field_AC2          = 0;
        work->field_AC6          = 2;
        work->field_AD6          = 0;
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_403000_80133AF8(arg0);
        work->field_6           = 0;
        work->field_8           = 0;
        work->field_FC2         = 0;
        work->recDD0.ends[1].vz = 0x320;
        scratch->facing         = Actor403000_Cell(player->extra.tmd->coords);
        cell                    = Actor403000_Cell(arg0->extra.tmd->coords);
        scratch->base           = cell;
        if (cell != scratch->facing) {
            diff = cell - scratch->facing;
            if (diff < -5) {
                goto neg;
            }
            if (diff < 0) {
                goto pos;
            }
            if (diff < 5) {
            neg:
                dir = -1;
            } else {
            pos:
                dir = 1;
            }
        } else {
            dir = work->field_FD1;
        }
        work->field_FD1 = -dir;
        work->field_6   = 0;
    }
    work->field_6++;
    ActorContact_PushContact(arg0->extra.tmd->coords, work->objD18.contacts, ARRAY_SIZE(work->objD18.contacts));
    scratch->facing = Actor403000_Cell(player->extra.tmd->coords);
    base            = Actor403000_Cell(arg0->extra.tmd->coords);
    scratch->base   = base;
    if ((func_actor_403000_80133FC0(arg0, base, scratch->facing) << 16) && (s16)work->field_6 > 0x3C) {
        work->field_0 = 5;
    }
    scratch->index = scratch->base + work->field_FD1;
    if (scratch->index != -1) {
        if (scratch->index == 10) {
            scratch->index = 0;
        }
    } else {
        scratch->index = 9;
    }
    table            = D_actor_403000_80158CE0;
    v                = &table[scratch->index];
    scratch->vec.vx  = v->vx;
    scratch->vec.vy  = v->vy;
    scratch->vec.vz  = v->vz;
    scratch->vec.vx -= arg0->extra.tmd->coords->coord.t[0];
    scratch->vec.vy  = 0;
    scratch->vec.vz -= arg0->extra.tmd->coords->coord.t[2];
    coord            = arg0->extra.tmd->coords;
    angle            = ratan2(scratch->vec.vx, scratch->vec.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    mag             = angle;
    scratch->angle  = mag;
    work->field_AD8 = mag;
    func_actor_403000_80133AF8(arg0);
    if (scratch->angle > 0x40) {
        scratch->angle = 0x40;
    }
    if (scratch->angle < -0x40) {
        scratch->angle = -0x40;
    }
    scratch->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->angle, 1);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorMoveForward(arg0->extra.tmd->coords, 0x12C);
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000SeekScratch);
}

/// Waypoint-ring patrol toward a goal waypoint: on entry derive the goal from
/// the player's cell and, if unset, the ring direction `field_FD2`; every frame
/// on reaching the goal pick the next state (3, 11 or 15) from a random roll,
/// then steer (clamped to 0x40) toward the next waypoint and step forward 0x12C.
static void func_actor_403000_8013A678(Task* arg0)
{
    Actor403000Work*        work;
    Task*                   player;
    Actor403000SeekScratch* scratch;
    TmdObject*              obj;
    GfxCoord*               coord;
    SVECTOR*                table;
    SVECTOR*                v;
    s16                     angle;
    s16                     diff;
    s32                     mag;
    s16                     r;
    s8                      base;
    s8                      dir;
    s8                      goal;

    work    = arg0->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(Actor403000SeekScratch);
    if (work->field_4 != 0) {
        obj             = arg0->extra.tmd;
        work->field_FCA = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->objB50.body.radius = 0x3E8;
        work->field_AC0          = 1;
        work->field_ACA          = 0x10;
        work->field_AC2          = 0;
        work->field_AC6          = 2;
        work->field_AD6          = 0;
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_403000_80133AF8(arg0);
        work->field_6           = 0;
        work->field_8           = 0;
        work->field_FC2         = 0;
        work->recDD0.ends[1].vz = 0x320;
        scratch->facing         = Actor403000_Cell(player->extra.tmd->coords);
        scratch->base           = Actor403000_Cell(arg0->extra.tmd->coords);
        switch ((u8)scratch->facing) {
            case 0:
            case 1:
            case 2:
                goal = 5;
                break;
            case 3:
            case 4:
                goal = 9;
                break;
            case 5:
            case 6:
            case 7:
                goal = 0;
                break;
            case 8:
            case 9:
                goal = 4;
                break;
            default:
                goal = -1;
                break;
        }
        work->field_FD4 = goal;
        if (work->field_FD2 == 0) {
            diff = scratch->base - scratch->facing;
            if (diff < -5) {
                goto neg;
            }
            if (diff < 0) {
                goto pos;
            }
            if (diff < 5) {
            neg:
                dir = -1;
            } else {
            pos:
                dir = 1;
            }
            work->field_FD2 = -dir;
        }
    }
    work->field_6++;
    ActorContact_PushContact(arg0->extra.tmd->coords, work->objD18.contacts, ARRAY_SIZE(work->objD18.contacts));
    base          = Actor403000_Cell(arg0->extra.tmd->coords);
    scratch->base = base;
    if (base == work->field_FD4) {
        if ((s8)work->field_FD6 == 1) {
            work->field_0 = 0xF;
        } else {
            r = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF;
            if (arg0->extra.tmd->texturePageOffset == 2) {
                if (r <= 0) {
                    work->field_0 = 3;
                } else if (r < 7) {
                    work->field_0 = 0xF;
                } else {
                    work->field_0 = 0xB;
                }
            } else if (r < 7) {
                work->field_0 = 3;
            } else if (r < 0xB) {
                work->field_0 = 0xF;
            } else {
                work->field_0 = 0xB;
            }
        }
    }
    scratch->index = scratch->base + work->field_FD2;
    if (scratch->index != -1) {
        if (scratch->index == 10) {
            scratch->index = 0;
        }
    } else {
        scratch->index = 9;
    }
    table            = D_actor_403000_80158CE0;
    v                = &table[scratch->index];
    scratch->vec.vx  = v->vx;
    scratch->vec.vy  = v->vy;
    scratch->vec.vz  = v->vz;
    scratch->vec.vx -= arg0->extra.tmd->coords->coord.t[0];
    scratch->vec.vy  = 0;
    scratch->vec.vz -= arg0->extra.tmd->coords->coord.t[2];
    coord            = arg0->extra.tmd->coords;
    angle            = ratan2(scratch->vec.vx, scratch->vec.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    mag             = angle;
    scratch->angle  = mag;
    work->field_AD8 = mag;
    func_actor_403000_80133AF8(arg0);
    if (scratch->angle > 0x40) {
        scratch->angle = 0x40;
    }
    if (scratch->angle < -0x40) {
        scratch->angle = -0x40;
    }
    scratch->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->angle, 1);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorMoveForward(arg0->extra.tmd->coords, 0x12C);
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000SeekScratch);
}

/// Walk the waypoint ring: on the entry frame snap the model onto its cell's
/// waypoint and face the neighbour in the `field_FD3` direction; every frame
/// finish (state 2) on reaching the player's cell, give up (14) after 300
/// frames, or switch to 5 once `func_actor_403000_80133FC0` allows it.
static void func_actor_403000_8013ACBC(Task* arg0)
{
    Actor403000Work*        work;
    Task*                   player;
    Actor403000SeekScratch* scratch;
    TmdObject*              obj;
    GfxCoord*               coord;
    SVECTOR*                table;
    SVECTOR*                v;
    s16                     angle;
    s16                     index;
    s8                      base;
    SVECTOR*                last;

    work    = arg0->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(Actor403000SeekScratch);
    if (work->field_4 != 0) {
        obj             = arg0->extra.tmd;
        work->field_FCA = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->objB50.body.radius = 0x3E8;
        work->field_AC0          = 1;
        work->field_ACA          = 0x10;
        work->field_AC2          = 0;
        work->field_AC6          = 4;
        work->field_AD6          = 0;
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_403000_80133AF8(arg0);
        base                                  = Actor403000_Cell(arg0->extra.tmd->coords);
        scratch->base                         = base;
        table                                 = D_actor_403000_80158CE0;
        v                                     = &table[base];
        scratch->vec.vx                       = v->vx;
        scratch->vec.vy                       = v->vy;
        scratch->vec.vz                       = v->vz;
        arg0->extra.tmd->coords->coord.t[0]   = scratch->vec.vx;
        arg0->extra.tmd->coords->coord.t[1]   = scratch->vec.vy;
        arg0->extra.tmd->coords->coord.t[2]   = scratch->vec.vz;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (work->field_FD3 == 1) {
            if (scratch->base + 1 >= 10) {
                scratch->vec.vx = table[0].vx;
                scratch->vec.vy = table[0].vy;
                scratch->vec.vz = table[0].vz;
            } else {
                index           = scratch->base + 1;
                scratch->vec.vx = table[index].vx;
                scratch->vec.vy = table[index].vy;
                scratch->vec.vz = table[index].vz;
            }
        } else {
            if (scratch->base - 1 < 0) {
                last            = &table[9];
                scratch->vec.vx = last->vx;
                scratch->vec.vy = last->vy;
                scratch->vec.vz = last->vz;
            } else {
                index           = scratch->base - 1;
                scratch->vec.vx = table[index].vx;
                scratch->vec.vy = table[index].vy;
                scratch->vec.vz = table[index].vz;
            }
        }
        scratch->vec.vx -= arg0->extra.tmd->coords->coord.t[0];
        scratch->vec.vy  = 0;
        scratch->vec.vz -= arg0->extra.tmd->coords->coord.t[2];
        coord            = arg0->extra.tmd->coords;
        angle            = ratan2(scratch->vec.vx, scratch->vec.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (angle < 0) {
        loop_neg:
            if (angle < -0x800) {
                angle += 0x1000;
                goto loop_neg;
            }
        } else {
        loop_pos:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto loop_pos;
            }
        }
        scratch->angle  = angle;
        work->field_AD8 = 0;
        scratch->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->angle, 1);
        work->field_6           = 0;
        work->field_8           = 0;
        work->field_FC2         = 0;
        work->recDD0.ends[1].vz = 0x320;
        scratch->facing         = Actor403000_Cell(player->extra.tmd->coords);
    }
    work->field_6++;
    scratch->facing = Actor403000_Cell(player->extra.tmd->coords);
    scratch->base   = Actor403000_Cell(arg0->extra.tmd->coords);
    func_actor_403000_80133AF8(arg0);
    if ((s16)work->field_6 > 300) {
        work->field_0 = 14;
    } else if (scratch->facing == scratch->base) {
        work->field_0   = 2;
        work->field_FD2 = work->field_FD3;
        work->field_FD3 = -work->field_FD3;
    } else if (func_actor_403000_80133FC0(arg0, scratch->base, scratch->facing) << 16) {
        if ((s16)work->field_6 > 60) {
            work->field_0 = 5;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000SeekScratch);
}

/// Turn toward the next waypoint: on the entry frame pick it from the grid
/// offset by `field_FD5`, set state 2 if it is within a quarter turn, and fold
/// the display object's scaled first and third matrix columns into the drift
/// `field_FB0`; every frame re-aim by `field_FC8` and apply that drift.
static void func_actor_403000_8013B238(Task* arg0)
{
    Actor403000Work*       work;
    Actor403000AimScratch* scratch;
    GfxCoord*              coord;
    s32                    x;
    s32                    z;
    s8                     col;
    s32                    b;
    s8                     row;
    TmdObject*             obj;
    SVECTOR*               v;
    SVECTOR*               table;
    s32                    mag;
    Actor403000AimScratch* head;
    s16                    angle;

    head                                        = SCRATCH_STACK_CURSOR(Actor403000AimScratch);
    work                                        = arg0->work;
    SCRATCH_STACK_CURSOR(Actor403000AimScratch) = head - 1;
    scratch                                     = head - 1;
    if (work->field_4 != 0) {
        obj             = arg0->extra.tmd;
        work->field_FCA = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->objB50.body.radius = 0x3E8;
        work->field_AC0          = 1;
        work->field_ACA          = 0x10;
        work->field_AC2          = 0;
        work->field_AC6          = 9;
        work->field_AD6          = 0;
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        x                        = arg0->extra.tmd->coords->coord.t[0];
        z                        = arg0->extra.tmd->coords->coord.t[2];
        if (x < 0xD48) {
            col = 4;
        } else if (x < 0x1A90) {
            col = 3;
        } else if (x < 0x2AF8) {
            col = 2;
        } else {
            col = x < 0x3C8C;
        }
        row            = z >= 0x1068;
        b              = (s8)D_actor_403000_80158D48[col + row * 5];
        scratch->base  = b;
        scratch->index = scratch->base + work->field_FD5;
        if (scratch->index >= 10) {
            scratch->index -= 10;
        } else if (scratch->index < 0) {
            scratch->index += 10;
        }
        table            = D_actor_403000_80158CE0;
        v                = &table[scratch->index];
        scratch->vec.vx  = v->vx;
        scratch->vec.vy  = v->vy;
        scratch->vec.vz  = v->vz;
        scratch->vec.vx -= arg0->extra.tmd->coords->coord.t[0];
        scratch->vec.vy -= arg0->extra.tmd->coords->coord.t[1];
        scratch->vec.vz -= arg0->extra.tmd->coords->coord.t[2];
        coord            = arg0->extra.tmd->coords;
        angle            = ratan2(scratch->vec.vx, scratch->vec.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (angle < 0) {
        loop_neg:
            if (angle < -0x800) {
                angle += 0x1000;
                goto loop_neg;
            }
        } else {
        loop_pos:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto loop_pos;
            }
        }
        mag            = angle;
        scratch->angle = mag;
        if (ABS(mag) < 0x400) {
            work->field_0 = 2;
        }
        if (scratch->angle < 0) {
            scratch->angle += 0x1000;
        }
        Gfx_MatrixCol0(&arg0->extra.tmd->coords->coord, &scratch->vec);
        VectorNormalSS(&scratch->vec, &scratch->vec);
        gte_lddp(0x1B);
        gte_ldsv(&scratch->vec);
        gte_gpf12();
        gte_stsv(&scratch->vec);
        work->field_FB0 = scratch->vec;
        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, &scratch->vec);
        VectorNormalSS(&scratch->vec, &scratch->vec);
        gte_lddp(-0x29);
        gte_ldsv(&scratch->vec);
        gte_gpf12();
        gte_stsv(&scratch->vec);
        work->field_FC8     = scratch->angle / 36;
        work->field_FB0.vx += scratch->vec.vx;
        work->field_FB0.vy += scratch->vec.vy;
        work->field_FB0.vz += scratch->vec.vz;
        work->field_AEB     = 0;
        work->field_AEA     = 0;
        work->field_AE9     = 0;
        work->field_AE8     = 0;
        work->field_6       = 0;
    }
    work->field_6++;
    ActorContact_PushContact(arg0->extra.tmd->coords, work->objD18.contacts, ARRAY_SIZE(work->objD18.contacts));
    scratch->angle = work->field_FC8 + ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->angle, 1);
    arg0->extra.tmd->coords->coord.t[0]  += work->field_FB0.vx;
    arg0->extra.tmd->coords->coord.t[1]  += work->field_FB0.vy;
    arg0->extra.tmd->coords->coord.t[2]  += work->field_FB0.vz;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_403000_80133AF8(arg0);
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_0 = 2;
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000AimScratch);
}

static __inline__ s32 Actor403000_Outside(SVECTOR* v, s32 r)
{
    OverlayRangeScratch* s;
    OverlayRangeScratch* head;
    head                                      = SCRATCH_STACK_CURSOR(OverlayRangeScratch);
    s                                         = head - 1;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = s;
    s->dx                                     = v->vx;
    s->dz                                     = v->vz;
    s->r                                      = r;
    s->dx                                    *= s->dx;
    s->dz                                    *= s->dz;
    s->r                                     *= s->r;
    SCRATCH_STACK_CURSOR(OverlayRangeScratch) = head;
    return (s->dx + s->dz) >= s->r;
}

static void func_actor_403000_8013B74C(Task* arg0)
{
    Actor403000Work*        work;
    Task*                   player;
    Enemy*                  enemy;
    Actor403000DropScratch* scratch;
    Actor403000DropScratch* head;
    GfxCoord*               coord;
    GfxCoord*               coord2;
    SVECTOR*                t;
    Task*                   task;
    s16                     b;
    GameActor*              pw;
    s32                     frame;
    s16                     angle;
    s32                     mag;
    work   = arg0->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    enemy  = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_F74                     = player->extra.tmd->coords->coord.t[0];
        work->field_F76                     = player->extra.tmd->coords->coord.t[1];
        work->field_F78                     = player->extra.tmd->coords->coord.t[2];
        arg0->extra.tmd->coords->coord.t[0] = 0x2134;
        arg0->extra.tmd->coords->coord.t[1] = player->extra.tmd->coords->coord.t[1] - 0x1518;
        arg0->extra.tmd->coords->coord.t[2] = 0x1194;
        work->field_AC6                     = 8;
        work->field_AC0                     = 2;
        work->field_6                       = 0;
        work->field_ACA                     = 0;
        work->field_FCA                     = 0;
        scratch                             = SCRATCH_STACK_RESERVE_BLOCK(Actor403000DropScratch);
        b                                   = Actor403000_Cell(player->extra.tmd->coords);
        scratch->base                       = b;
        switch (scratch->base) {
            case 0:
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x800, 1);
                break;

            case 1:
            case 2:
            case 3:
            case 4:
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x400, 1);
                break;

            case 5:
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0, 1);
                break;

            case 6:
            case 7:
            case 8:
            case 9:
            default:
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x400, 1);
                break;
        }

        SCRATCH_STACK_RELEASE_BLOCK(Actor403000DropScratch);
    }
    if (work->field_FD9 != 0) {
        work->field_FD9--;
        return;
    }
    head    = SCRATCH_STACK_CURSOR(Actor403000DropScratch);
    scratch = (SCRATCH_STACK_CURSOR(Actor403000DropScratch) = head - 1);
    frame   = (s16)work->field_6;
    if (frame == 10) {
        work->field_ACA    = 0x10;
        scratch->target.vx = work->field_F74 - player->extra.tmd->coords->coord.t[0];
        scratch->target.vy = work->field_F76 - player->extra.tmd->coords->coord.t[1];
        scratch->target.vz = work->field_F78 - player->extra.tmd->coords->coord.t[2];
        if (!Actor403000_Outside(&scratch->target, 1000) && enemy->hp > 0 &&
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_403000_80158DD0.value, 0) == 0) {
            task         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            scratch->ret = taskMessageDispatch(task, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 3), 0);
            if (scratch->ret == 1) {
                pw                              = (GameActor*)player->work;
                gGameSession->deathFadeFrames   = 0x28;
                gGameSession->deathRestartDelay = 0x28;
                pw->state                       = frame;
            }
            work->field_0   = 0xD;
            work->field_F90 = D_actor_403000_80158C08;
            work->field_F94 = 5;
            work->field_F98 = 0;
            work->field_F9C = 0;
            work->field_FA0 = 1;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_F90, 0);
            work->field_FC0 = 1;
        }
    }
    if (arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) {
        arg0->extra.tmd->coords->coord.t[1] += 0x12C;
    }
    if ((s16)work->field_6 >= 4) {
        arg0->extra.tmd->coords->coord.t[0] += (work->field_F74 - arg0->extra.tmd->coords->coord.t[0]) >> 2;
        arg0->extra.tmd->coords->coord.t[2] += (work->field_F78 - arg0->extra.tmd->coords->coord.t[2]) >> 2;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_403000_80133AF8(arg0);
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        t      = &scratch->target;
        coord2 = arg0->extra.tmd->coords;
        t->vx  = gPlayerStatus.coordMtx->t[0] - coord2->coord.t[0];
        t->vy  = gPlayerStatus.coordMtx->t[1] - coord2->coord.t[1];
        t->vz  = gPlayerStatus.coordMtx->t[2] - coord2->coord.t[2];
        coord  = arg0->extra.tmd->coords;
        angle  = ratan2(t->vx, t->vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (angle < 0) {
        loop_neg:
            if (angle < (-0x800)) {
                angle += 0x1000;
                goto loop_neg;
            }

        } else {
        loop_pos:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto loop_pos;
            }
        }
        scratch->angle = (mag = angle);
        if (ABS(mag) > 0x300) {
            work->field_0   = 4;
            work->field_FD3 = (work->field_FD2 = (work->field_FD5 = -func_actor_403000_80134204(arg0->extra.tmd->coords)));
        } else {
            work->field_0   = 2;
            work->field_FD3 = -func_actor_403000_80134204(arg0->extra.tmd->coords);
            work->field_FD2 = (work->field_FD5 = func_actor_403000_80134204(arg0->extra.tmd->coords));
        }
    }
    if ((s16)work->field_6 >= 2) {
        work->field_ACA = 0x10;
    }
    work->field_6++;
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000DropScratch);
}

/// Ease the display object up toward the player and across to
/// `field_F74`/`field_F78`, turn the player's third matrix column into the
/// push vector for the first 8 frames, and once `ANIMATION_SLOT_REACHED_BOUNDARY` is set in `field_60`
/// after frame 0xB move the state machine to 4.
static void func_actor_403000_8013BDE0(Task* arg0)
{
    Actor403000Work*        work;
    Task*                   player;
    Actor403000PushScratch* scratch;
    Enemy*                  enemy;
    s32                     sound;
    s32                     pan;

    work                                         = arg0->work;
    player                                       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch                                      = SCRATCH_STACK_CURSOR(Actor403000PushScratch) - 1;
    SCRATCH_STACK_CURSOR(Actor403000PushScratch) = scratch;
    if (work->field_4 != 0) {
        enemy           = arg0->spawnArg2.pointer;
        work->field_FCA = 0;
        work->field_6   = 0;
        sound           = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 7;
        pan             = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (arg0->extra.tmd->coords->coord.t[1] < player->extra.tmd->coords->coord.t[1]) {
        arg0->extra.tmd->coords->coord.t[1] += 0x12C;
        arg0->extra.tmd->coords->coord.t[0] += (work->field_F74 - arg0->extra.tmd->coords->coord.t[0]) >> 2;
        arg0->extra.tmd->coords->coord.t[2] += (work->field_F78 - arg0->extra.tmd->coords->coord.t[2]) >> 2;
    }
    if ((s16)work->field_6 < 8) {
        gfxReadMatrixZAxis(&player->extra.tmd->coords->coord, &scratch->dir);
        VectorNormalSS(&scratch->dir, &scratch->dir);
        gte_lddp(-0x2A);
        gte_ldsv(&scratch->dir);
        gte_gpf12();
        gte_stsv(&scratch->dir);
    }
    D_actor_403000_80158DB0.value.displacement.vx   = scratch->dir.vx;
    D_actor_403000_80158DB0.value.displacement.vy   = 0;
    D_actor_403000_80158DB0.value.displacement.vz   = scratch->dir.vz;
    D_actor_403000_80158DB0.value.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
    D_actor_403000_80158DB0.value.keepControl       = 1;
    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && (s16)work->field_6 >= 0xB) {
        work->field_0   = 4;
        work->field_FD3 = work->field_FD2 = work->field_FD5 = -func_actor_403000_80134204(arg0->extra.tmd->coords);
    }
    func_actor_403000_80133AF8(arg0);
    work->field_6++;
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000PushScratch);
}

static void func_actor_403000_8013C050(Task* arg0)
{
    Actor403000Work* work;
    Task*            player;
    Enemy*           enemy;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;

    work   = arg0->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    enemy  = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_FD6                     = 0;
        arg0->extra.tmd->coords->coord.t[0] = 0x2134;
        arg0->extra.tmd->coords->coord.t[1] = player->extra.tmd->coords->coord.t[1] - 0x1518;
        arg0->extra.tmd->coords->coord.t[2] = 0x1194;
        work->field_FCA                     = 1;
        work->field_AC6                     = 8;
        work->field_AC0                     = 2;
        work->field_6                       = 0;
        work->field_8                       = 0;
        work->field_ACA                     = 0;
        func_actor_403000_80133AF8(arg0);
    }
    if ((s16)work->field_6 > 0x3C) {
        work->field_FD9 = 0x14;
        sound           = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401E0010;
        pan             = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->field_0 = 0xC;
    }
    if ((s16)work->field_6 > 0x64) {
        if (work->field_FB8 == arg0->extra.tmd->coords->coord.t[0] &&
            work->field_FBA == arg0->extra.tmd->coords->coord.t[1] &&
            work->field_FBC == arg0->extra.tmd->coords->coord.t[2]) {
            work->field_8++;
        } else {
            work->field_8 = 0;
        }
        if (work->field_8 > 0x1E) {
            work->field_FD9 = 0x14;
            sound2          = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401E0010;
            pan2            = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            work->field_0 = 0xC;
        }
        work->field_FB8 = arg0->extra.tmd->coords->coord.t[0];
        work->field_FBA = arg0->extra.tmd->coords->coord.t[1];
        work->field_FBC = arg0->extra.tmd->coords->coord.t[2];
    }
    work->field_6++;
}

/// Seek the next waypoint: on the entry frame restart the animation and latch
/// the steering from `func_actor_403000_80134204`; every frame compare the
/// player's and the actor's grid cells, turn at most 8 units toward the chosen
/// waypoint and step forward.
static void func_actor_403000_8013C2D4(Task* arg0)
{
    Actor403000Work*        work;
    Task*                   player;
    Actor403000SeekScratch* scratch;
    TmdObject*              obj;
    GfxCoord*               coord;
    SVECTOR*                table;
    SVECTOR*                v;
    s16                     angle;
    s32                     mag;
    s8                      base;

    work    = arg0->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(Actor403000SeekScratch);
    if (work->field_4 != 0) {
        obj             = arg0->extra.tmd;
        work->field_FCA = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->objB50.body.radius = 0x3E8;
        work->field_AC0          = 1;
        work->field_ACA          = 0x10;
        work->field_AC2          = 0;
        work->field_AC6          = 1;
        work->field_AD6          = 0;
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_403000_80133AF8(arg0);
        work->field_6           = 0;
        work->field_8           = 0;
        work->field_FC2         = 0;
        work->recDD0.ends[1].vz = 0x320;
        scratch->facing         = Actor403000_Cell(player->extra.tmd->coords);
        scratch->base           = Actor403000_Cell(arg0->extra.tmd->coords);
        work->field_FD1         = func_actor_403000_80134204(arg0->extra.tmd->coords);
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->objD18.contacts, ARRAY_SIZE(work->objD18.contacts));
    scratch->facing = Actor403000_Cell(player->extra.tmd->coords);
    base            = Actor403000_Cell(arg0->extra.tmd->coords);
    scratch->base   = base;
    if (func_actor_403000_80133FC0(arg0, base, scratch->facing) << 16) {
        work->field_0 = 5;
    }
    scratch->index = scratch->base + work->field_FD1;
    if (scratch->index != -1) {
        if (scratch->index == 10) {
            scratch->index = 0;
        }
    } else {
        scratch->index = 9;
    }
    table            = D_actor_403000_80158CE0;
    v                = &table[scratch->index];
    scratch->vec.vx  = v->vx;
    scratch->vec.vy  = v->vy;
    scratch->vec.vz  = v->vz;
    scratch->vec.vx -= arg0->extra.tmd->coords->coord.t[0];
    scratch->vec.vy  = 0;
    scratch->vec.vz -= arg0->extra.tmd->coords->coord.t[2];
    coord            = arg0->extra.tmd->coords;
    angle            = ratan2(scratch->vec.vx, scratch->vec.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    mag             = angle;
    scratch->angle  = mag;
    work->field_AD8 = mag;
    func_actor_403000_80133AF8(arg0);
    if (scratch->angle > 8) {
        scratch->angle = 8;
    }
    if (scratch->angle < -8) {
        scratch->angle = -8;
    }
    scratch->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, scratch->angle, 1);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorMoveForward(arg0->extra.tmd->coords, 0x16);
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000SeekScratch);
}

static const Actor403000StateTable D_actor_403000_80131F44 = {
    {
        func_actor_403000_8013D5F8,
        func_actor_403000_8013D72C,
        func_actor_403000_8013A678,
        func_actor_403000_8013ACBC,
        func_actor_403000_8013B238,
        func_actor_403000_80137084,
        func_actor_403000_801384E8,
        func_actor_403000_801377C8,
        func_actor_403000_801386E8,
        func_actor_403000_80138DB0,
        func_actor_403000_8013A08C,
        func_actor_403000_80139AE0,
        func_actor_403000_8013B74C,
        func_actor_403000_8013BDE0,
        func_actor_403000_8013C2D4,
        func_actor_403000_8013C050,
        func_actor_403000_80135F08,
        func_actor_403000_801399A0,
        func_actor_403000_8013D910,
        func_actor_403000_8013D850,
        func_actor_403000_8013603C,
        func_actor_403000_80136B14,
        func_actor_403000_801365D0,
        func_actor_403000_80136D68,
        func_actor_403000_8013D648,
    }
};

static void func_actor_403000_8013C864(Enemy* arg0, Task* arg1)
{
    VECTOR3                   pos;
    Actor403000Work*          work;
    Task*                     player;
    Actor403000UpdateScratch* scratch;
    Actor403000StateTable     states;
    PlayerStatus*             config;
    u32                       sound;
    s32                       pan;

    work                                  = arg1->work;
    player                                = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    config                                = &gPlayerStatus;
    states                                = D_actor_403000_80131F44;
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
    pos.vx = arg1->extra.tmd->coords->workm.t[0];
    pos.vy = arg1->extra.tmd->coords->workm.t[1];
    pos.vz = arg1->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(arg0, (VECTOR*)&pos, 0, 0);
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->field_0 != 0x16 && work->field_0 != 0x14 && work->field_0 != 0) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->field_0 != 0x16 && work->field_0 != 0x14 && work->field_0 != 0) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            Gp_ClearRec18Occupied(work->objD18.contacts);
            Gp_ClearRec18Occupied(work->objB50.contacts);
            Gp_ClearRec18Occupied(work->objBE8.contacts);
            Gp_ClearRec18Occupied(work->objC80.contacts);
            Gp_ClearRec18Occupied(work->records);
            Gp_ClearRec18Occupied(work->recordsE98);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Gp_ClearRec18Occupied(work->objD18.contacts);
            Gp_ClearRec18Occupied(work->objB50.contacts);
            Gp_ClearRec18Occupied(work->objBE8.contacts);
            Gp_ClearRec18Occupied(work->objC80.contacts);
            Gp_ClearRec18Occupied(work->records);
            Gp_ClearRec18Occupied(work->recordsE98);
            return;
    }
    scratch = SCRATCH_STACK_RESERVE_BLOCK(Actor403000UpdateScratch);
    if (config->hp > 0) {
        func_actor_403000_80134F44(arg1);
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    states.funcs[work->field_0](arg1);
    if (work->field_0 != 0x16 && work->field_0 != 0x14 && work->field_0 != 0x15 && work->field_0 != 0) {
        work->objB50.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->objBE8.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->objC80.body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->objE60.flags      |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->objB50.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->objBE8.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->objC80.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->objE60.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if (work->field_FCA == 0) {
        if (work->field_FD8 == 1 && work->field_FD7 == 1) {
            arg0->node.state.parts.flags = (WORLD_TARGET_HIDE_HP | WORLD_TARGET_KEEP_SCANNED);
        } else {
            arg0->node.state.parts.flags = (WORLD_TARGET_HIDE_HP | WORLD_TARGET_KEEP_SCANNED | WORLD_TARGET_NOT_LOCKABLE);
            Gp_ClearNodeSlots(&arg0->node);
        }
    }
    if (!(gDisplayState.animFrame & 1)) {
        scratch->d.vx = player->extra.tmd->coords->coord.t[0] - arg1->extra.tmd->coords->coord.t[0];
        scratch->d.vy = player->extra.tmd->coords->coord.t[1] - arg1->extra.tmd->coords->coord.t[1];
        scratch->d.vz = player->extra.tmd->coords->coord.t[2] - arg1->extra.tmd->coords->coord.t[2];
        scratch->dist = SquareRoot0(scratch->d.vx * scratch->d.vx + scratch->d.vy * scratch->d.vy + scratch->d.vz * scratch->d.vz);
        if (scratch->dist > 9000) {
            work->field_FD8 = 0;
        } else {
            work->field_FD8 = 1;
        }
    } else {
        scratch->to.vx   = player->extra.tmd->coords->workm.t[0];
        scratch->to.vy   = player->extra.tmd->coords->workm.t[1];
        scratch->to.vz   = player->extra.tmd->coords->workm.t[2];
        scratch->from.vx = arg1->extra.tmd->coords->workm.t[0];
        scratch->from.vy = arg1->extra.tmd->coords->workm.t[1];
        scratch->from.vz = arg1->extra.tmd->coords->workm.t[2];
        if (func_800E0308(&scratch->from, &scratch->to) != 1) {
            work->field_FD7 = 1;
        } else {
            work->field_FD7 = 0;
        }
    }
    if (work->field_FC0 == 1) {
        work->field_FCC++;
        if (work->field_F90 != Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]) {
            switch (work->field_F94) {
                case 1:
                    if (work->field_FCC == 42) {
                        Gp_SpawnPadLerp(0xC, 0xFF, 0x80);
                    }
                    if (work->field_FCC == 52) {
                        sound   = arg0->placeKey;
                        sound >>= 12;
                        sound <<= 8;
                        sound  |= 0x401E000A;
                        pan     = worldCoordGetOriginAudioPan(arg1->extra.tmd->coords) << 24;
                        pan   >>= 24;
                        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
                    }
                    break;
                case 3:
                    if (work->field_FCC == 4) {
                        Gp_SpawnPadLerp(0xC, 0x58, 0xFF);
                    }
                    if (work->field_FCC == 15) {
                        sound   = arg0->placeKey;
                        sound >>= 12;
                        sound <<= 8;
                        sound  |= 0x401E000A;
                        pan     = worldCoordGetOriginAudioPan(arg1->extra.tmd->coords) << 24;
                        pan   >>= 24;
                        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
                    }
                    break;
                case 5:
                    if (work->field_FCC == 1) {
                        Gp_SpawnPadLerp(0xC, 0x58, 0xFF);
                    }
                    if (work->field_FCC == 10) {
                        sound   = arg0->placeKey;
                        sound >>= 12;
                        sound <<= 8;
                        sound  |= 0x401E000A;
                        pan     = worldCoordGetOriginAudioPan(arg1->extra.tmd->coords) << 24;
                        pan   >>= 24;
                        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
                    }
                    break;
                case 2:
                case 4:
                case 6:
                    break;
            }
        }
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            work->field_FCC = 0;
            if (work->field_F90 != Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]) {
                switch (work->field_F94) {
                    case 1:
                        work->field_F94 = 2;
                        work->field_F98 = 1;
                        work->field_F9C = 0;
                        work->field_FA0 = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_F90, 0);
                        break;
                    case 3:
                        work->field_F94 = 4;
                        work->field_F98 = 1;
                        work->field_F9C = 0;
                        work->field_FA0 = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_F90, 0);
                        break;
                    case 5:
                        work->field_F94 = 6;
                        work->field_F98 = 1;
                        work->field_F9C = 0;
                        work->field_FA0 = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_F90, 0);
                        break;
                    case 2:
                    case 4:
                    case 6:
                        work->field_F90 = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon];
                        work->field_F94 = 7;
                        work->field_F9C = 0x10;
                        work->field_F98 = 0;
                        work->field_FA0 = 1;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->field_F90, 0);
                        break;
                }
            } else {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
                work->field_FC0 = 0;
            }
        }
    }
    scratch->ofs.vx = -60;
    scratch->ofs.vy = -40;
    scratch->ofs.vz = 300;
    if (work->field_0 != 0x16 && work->field_0 != 0x14 && work->field_0 != 0x15) {
        func_actor_403000_801327B0(&arg1->extra.tmd->coords[4], &scratch->ofs, 0);
        func_actor_403000_80132AE0(&arg1->extra.tmd->coords[4]);
    } else {
        func_actor_403000_801330D4(&arg1->extra.tmd->coords[4]);
    }
    Gp_ClearRec18Occupied(work->objD18.contacts);
    Gp_ClearRec18Occupied(work->objB50.contacts);
    Gp_ClearRec18Occupied(work->objBE8.contacts);
    Gp_ClearRec18Occupied(work->objC80.contacts);
    Gp_ClearRec18Occupied(work->records);
    Gp_ClearRec18Occupied(work->recordsE98);
    if (player->extra.tmd->coords->coord.t[1] > 3) {
        player->extra.tmd->coords->coord.t[1] = 3;
    }
    if (player->extra.tmd->coords->coord.t[2] > 0x2260) {
        player->extra.tmd->coords->coord.t[2] = 0x2260;
    }
    if (player->extra.tmd->coords->coord.t[2] < -400) {
        player->extra.tmd->coords->coord.t[2] = -400;
    }
    if (player->extra.tmd->coords->coord.t[0] < 250) {
        player->extra.tmd->coords->coord.t[0] = 250;
    }
    if (player->extra.tmd->coords->coord.t[0] > 0x477C) {
        player->extra.tmd->coords->coord.t[0] = 0x477C;
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor403000UpdateScratch);
}

/// The enemy's three state handlers - spawn, per-frame tick and teardown -
/// indexed by `Task::state`.
static const EnemyTaskFuncTable3 D_actor_403000_80132004 = {
    {
        func_actor_403000_801343B8,
        func_actor_403000_8013C864,
        enemyDestroy,
    },
};

s32 func_actor_403000_8013D260(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
}

/// Handler for message 0x7D5 in the actor's message table. `arg2` picks the
/// display mode: 0 hides the model (flag 0x80) and 1 shows it again, both
/// re-running `Tmd_AllocBuffers`; 2 sets `TMD_OBJECT_SKIP_AUTO_BUFFER` on top of the current flags
/// and 3 replaces them with just `TMD_OBJECT_SKIP_AUTO_BUFFER`. Every mode but 1 resets the animation
/// state `field_0`. `arg1` is unused.
s32 func_actor_403000_8013D268(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject*       obj;
    Actor403000Work* work;

    obj  = task->extra.tmd;
    work = (Actor403000Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            break;
        case 2:
            obj->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Handler for message 0x7D6: returns 1 while the enemy still has hit points
/// or its model is shown (flag 0x80 clear), 0 once it is dead and hidden.
s32 func_actor_403000_8013D324(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (((Enemy*)task->spawnArg2.pointer)->hp > 0) {
        goto return_one;
    }

    if ((task->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) != 0) {
        return 0;
    }

return_one:
    return 1;
}

/// Handler for message 0x7D4: place the model's root coordinate at
/// `placement` - translation, then rotation about Y, X and Z - and cache the
/// resulting heading in `Actor403000Work::yaw`.
s32 func_actor_403000_8013D364(Task* task, s32 arg1, ActorTransform* placement, s32 arg3)
{
    GfxCoord*        coord;
    Actor403000Work* work;

    work                                = (Actor403000Work*)task->work;
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 1);
    gfxRotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = task->extra.tmd->coords;
    work->yaw                             = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    return 1;
}

s32 func_actor_403000_8013D464(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    Actor403000Work* work = (Actor403000Work*)task->work;

    work->field_AC6 = msg->animationId;
    work->field_0   = 1;
    work->field_2   = -1;
    return 0;
}

static s16 func_actor_403000_8013D48C(Task* task)
{
    Actor403000Work* work  = (Actor403000Work*)task->work;
    s16              found = 0;
    s16              i;
    s32              value;

    for (i = 0; i < 5; i++) {
        value = work->records[i].key.value;
        if (value == 0) {
            break;
        }
        if ((value & 0xFFFF0000) == 0x100000) {
            found = 1;
        }
    }
    return found;
}

static void func_actor_403000_8013D4F4(Task* task)
{
    Actor403000Work* work  = (Actor403000Work*)task->work;
    Enemy*           enemy = (Enemy*)task->spawnArg2.pointer;

    if (work != NULL) {
        Gp_UnlinkObj(&work->objB50.body);
        Gp_UnlinkObj(&work->objBE8.body);
        Gp_UnlinkObj(&work->objC80.body);
        Gp_UnlinkObj(&work->objD18.body);
        enemy->recs = 0;
    }
    enemyDestroy(enemy, task);
}

static void func_actor_403000_8013D564(SVECTOR* arg0, s32 arg1)
{
    arg0->vx = D_actor_403000_80158CE0[(s16)arg1].vx;
    arg0->vy = D_actor_403000_80158CE0[(s16)arg1].vy;
    arg0->vz = D_actor_403000_80158CE0[(s16)arg1].vz;
}

/// The enemy task's per-frame entry: runs the handler for the task's current
/// state - spawn, tick or teardown - from a stack copy of the state table.
static void func_actor_403000_8013D59C(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_403000_80132004;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_403000_8013D5F8(Task* arg0)
{
    TmdObject*       obj;
    Actor403000Work* work;
    Enemy*           enemy;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                      = arg0->extra.tmd;
        enemy                    = arg0->spawnArg2.pointer;
        work->field_FCA          = 1;
        obj->flags              |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->objD18.body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->hp                = 0;
    }
}

static void func_actor_403000_8013D648(Task* arg0)
{
    TmdObject*       obj;
    Actor403000Work* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj                      = arg0->extra.tmd;
        work->field_FCA          = 0;
        obj->flags               = 0;
        work->field_ACA          = 0x30;
        work->field_AC6          = 0xF;
        work->field_AC0          = 2;
        work->field_6            = 0;
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    work->field_6++;
    func_actor_403000_80133AF8(arg0);
    if ((work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED)) || (s16)work->field_6 >= 5) {
        if (enemy->hp > 0) {
            if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
                work->field_0 = 0x10;
            } else {
                work->field_0 = 0x12;
            }
        } else {
            work->field_F8C = 1;
            work->field_0   = 0x14;
        }
    }
}

static void func_actor_403000_8013D72C(Task* arg0)
{
    Enemy*           enemy;
    Actor403000Work* work;
    TmdObject*       obj;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj             = arg0->extra.tmd;
        enemy           = arg0->spawnArg2.pointer;
        work->field_FCA = 1;
        Gp_ClearNodeSlots(&enemy->node);
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->field_ACA          = 0x10;
        work->field_AC0          = 2;
        work->field_AD6          = 0;
        work->field_AD8          = 0;
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_403000_80133AF8(arg0);
        return;
    }
    func_actor_403000_80133AF8(arg0);
    if (work->field_FA6 == 0xA) {
        work->field_F30.t[2]    = 0;
        work->field_F30.t[1]    = 0;
        work->field_F30.t[0]    = 0;
        work->field_F30.m[2][2] = 0;
        work->field_F30.m[2][1] = 0;
        work->field_F30.m[2][0] = 0;
        work->field_F30.m[1][2] = 0;
        work->field_F30.m[1][1] = 0;
        work->field_F30.m[1][0] = 0;
        work->field_F30.m[0][2] = 0;
        work->field_F30.m[0][1] = 0;
        work->field_F30.m[0][0] = 0;
    }
    if (work->field_FA6 == 4 && work->field_AC6 == 0x1B && (work->slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
        work->field_AC6 = 0x1D;
        work->field_AC0 = 2;
    }
}

static void func_actor_403000_8013D850(Task* arg0)
{
    Actor403000Work* work;
    TmdObject*       obj;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj             = arg0->extra.tmd;
        work->field_FCA = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->objB50.body.radius = 0x3E8;
        work->field_AC0          = 1;
        work->field_ACA          = 0x10;
        work->field_AC2          = 0;
        work->field_AC6          = 0x10;
        work->objD18.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_403000_80133AF8(arg0);
        work->field_6 = 0;
    }
    work->field_6++;
    func_actor_403000_80133AF8(arg0);
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->field_FD3 = 1;
        work->field_FD2 = 1;
        work->field_0   = 2;
    }
}

static void func_actor_403000_8013D910(Task* arg0)
{
    Actor403000Work* work;
    Enemy*           enemy;
    u32              rng;
    s16              timer;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        work->field_6   = ((rng >> 16) & 0xF) + 0xA;
    }
    timer         = work->field_6 - 1;
    work->field_6 = timer;
    if (timer < 0 && enemy->hp > 0) {
        work->field_0 = 0x13;
    }
}

static s32 func_actor_403000_8013D98C(s32 arg0)
{
    switch (arg0 & 0xFF) {
        case 0:
        case 1:
        case 2:
            return 5;
        case 3:
        case 4:
            return 9;
        case 5:
        case 6:
        case 7:
            return 0;
        case 8:
        case 9:
            return 4;
        default:
            return -1;
    }
}
