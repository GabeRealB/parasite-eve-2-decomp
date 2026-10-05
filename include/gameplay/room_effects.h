#ifndef GAMEPLAY_ROOM_EFFECTS_H
#define GAMEPLAY_ROOM_EFFECTS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/effect_ids.h"
#include "gameplay/effects.h"
#include "gameplay/scene.h"

#include "main/coord.h"
#include "main/display_types.h"
#include "main/task_types.h"

/// Effect ids each room installs for the shared effects enemies spawn.
///
/// A room's init task stores the `EFFECT_*` id of its own copy of each shared
/// room-visual-effects or water task, so an enemy's effect works whichever room
/// overlay is loaded; `Gp_InitState1C` clears them. Zero means the room has none.
extern s32 gRoomEffectSparkEmitterId;

/// Energy Ball balls currently in flight; `Gp_CheckAttachThreshold` refuses a new
/// Energy Ball cast while three are.
extern s32 gEnergyBallInFlightCount;

/// Rising mote; fireball embers (actor_00300, actor_02400, actor_105100) and the spark emitter's particles.
extern s32 gRoomEffectMoteId;

/// Twin-trail beam; the beam-sword golems' sword trail (actor_02000, actor_02300).
extern s32 gRoomEffectTwinTrailId;

/// Spark flying from a player joint into the glow disc.
extern s32 gRoomEffectFlyingSparkId;

/// Glow disc of the Amoeba (actor_02400).
extern s32 gRoomEffectGlowDiscId;

/// Water spray, spawned where an actor breaks the surface at `GameSession.waterY`.
extern s32 gRoomEffectWaterSprayId;

/// Burst where the Brain Stinger's fireball ends.
extern s32 gRoomEffectOrangeBurstId;

/// The room-effect controller's live `RoomEffectState`.
///
/// Allocated on the primary heap when the controller starts and stored both
/// here and as that task's work. Gameplay and the room, actor, weapon and PE
/// overlays borrow it for the controller's lifetime. Callers do not test it
/// for `NULL`, and destroying the task does not clear it.
extern RoomEffectState* gRoomEffectState;

/// Halo the Brain Stinger casts with.
extern s32 gRoomEffectHaloId;

/// Water ripple spawned with `gRoomEffectWaterSprayId` at the surface.
extern s32 gRoomEffectWaterRippleId;

/// Sparks where a grenade-launcher golem's shot ends (actor_05600, actor_05700).
extern s32 gRoomEffectSparkBurstId;

/// Burst where the Amoeba's projectile ends.
extern s32 gRoomEffectOrangeBurst2Id;

/// Flash of a golem's silence scream (actor_02300, actor_05700).
extern s32 gRoomEffectFlashId;

extern TaskDesc D_8010FC2C[];

/// Projects a composed coordinate's view-space position onto room geometry along world +Y.
///
/// Probes a 4096-game-unit segment using the current composed view rotation.
/// Input XYZ and the rotated endpoint narrow to signed 16-bit game units.
/// Returns 1 on a hit, otherwise 0 with `hitCoord` unchanged. On success, replaces
/// its cached translation, derives its local matrix using its existing cached
/// rotation, reparents it to `gGfxViewCoord` and composes it. Source and output
/// may alias. The output's cached rotation is read and must already be initialized.
/// Inputs must remain clear of the initialized scratch stack and the nested
/// collision query's storage. Changes GTE state and retains no pointers.
s32 worldCollisionProjectGroundCoord(const GfxCoord* sourceCoord, GfxCoord* hitCoord);

/// Projects a view-space point onto room geometry along world +Y.
///
/// Reads only the source's three signed 32-bit game-unit components and probes
/// 4096 units using the current composed view rotation. Source XYZ and the
/// endpoint narrow to signed 16 bits. On a hit, writes only XYZ to `hitPoint`,
/// promoting the signed intersection to 32 bits, and returns hit Y minus source
/// Y in view space; a zero displacement returns 1. A miss returns 0 and leaves
/// the output unchanged. Thus every nonzero result is a hit, including negative
/// displacements. Source and output may alias. Both must remain clear of the
/// initialized scratch stack and nested query storage. Changes GTE state;
/// retains no pointers and does not read or write an SDK VECTOR pad word.
s32 worldCollisionProjectGroundPoint(const VECTOR3* sourcePoint, VECTOR3* hitPoint);

/// Attenuates a ground shadow's shade by the signed view-Y probe displacement.
///
/// `halfSize` is the shadow's half-size in game units; `baseShade` is its
/// grayscale intensity. Computes baseShade * (2 * halfSize) / viewYDisplacement
/// with signed word arithmetic. A zero displacement returns 0 (unmodulated);
/// values above 255 saturate to 255, and a nonzero displacement that rounds to
/// zero returns -1 (disabled). Negative results otherwise remain unchanged.
/// Pass the ground-point probe result narrowed to s16; this is view Y, not a
/// camera-independent world height.
s32 effectGetGroundShadowShade(s16 halfSize, s16 baseShade, s16 viewYDisplacement);

/// Records the player's most recently played animation sound cue.
///
/// `cueIndex` is 0 for animation cue 2 or 1 for cue 1. Stores cueIndex + 1,
/// narrowed to the controller's signed halfword; zero is reserved for no cue.
/// Requires a live `gRoomEffectState`; records the cue without playing sound.
void roomEffectRecordAnimationSoundCue(s32 cueIndex);

/// Spawns a counted effect task and its `EffectWork`.
/// `arg0` packs the `Task_Spawn` bank in bits 16..30 and the type in the low
/// 16 bits; a negative `arg0` bypasses the ordinary spawn limit (129) in
/// `RoomEffectState::effectCount`. `arg1` is stored in `EffectWork::parent`;
/// NULL stores the view coordinate there. The effect's own coordinate is
/// parented to `gGfxViewCoord` either way, after the offset is rotated into
/// it. `arg2` becomes `Task::spawnArg1`. `arg3` is an optional offset: NULL is
/// read as a zero vector, the components are copied into `EffectWork::pos`,
/// and the original pointer, NULL included, is stored in `EffectWork::field_C`.
/// Returns the work object, or `NULL`.
EffectWork* Gp_SpawnEff(s32 arg0, GfxCoord* arg1, TaskSpawnArg arg2, SVECTOR* arg3);

/// Draws a semitransparent full-screen colour tint over the current frame.
///
/// `rgb` supplies three readable bytes; `blendMode`'s low two bits select
/// `GPU_BLEND_*`. Covers the centred 320 by 240 pixel viewport, compensating
/// for `vramYOffset`, at sorting depth 16 scaled by `otDepthShift`. Prepends a
/// dithered blend command ahead of the quad. Consumes one `POLY_F4` and one
/// `DR_TPAGE` from the word-aligned frame arena without a capacity check;
/// packets borrow it until GPU drawing completes, and draw mode persists.
void effectDrawScreenTint(const u8* rgb, s32 blendMode);

/// Draws an additive camera-facing Gouraud band, black inside and coloured outside.
///
/// `centreCoord` must have a composed view-space cached translation; its rotation
/// is unused. `rgb` supplies three readable bytes. `innerRadius` and `width` each
/// narrow independently to s16; screen radii are innerRadius * 64 / (OTZ + 1)
/// and (innerRadius + width) * 64 / (OTZ + 1), with signed word arithmetic.
/// The complete band uses sixteen untextured quads.
/// Rejects a negative GTE projection flag. Reserves/relinquishes scratch storage
/// and appends sixteen `POLY_G4`/`DR_TPAGE` pairs to the unchecked frame arena;
/// packets live through GPU drawing. Inputs must stay clear of that storage.
void effectDrawOuterGlowBand(const GfxCoord* centreCoord, s32 innerRadius, s32 width, const u8* rgb);

/// Draws an additive camera-facing Gouraud disc, coloured at its centre and black at its rim.
///
/// `centreCoord` supplies a composed view-space cached translation; rotation is
/// unused. `radius` narrows to s16, then scales to radius * 64 / (OTZ + 1) pixels.
/// `rgb` supplies three readable bytes. Eight untextured quads cover sixteen fan
/// triangles. Rejects a negative GTE projection flag.
/// Reserves/relinquishes scratch storage and appends eight `POLY_G4`/`DR_TPAGE`
/// pairs to the unchecked frame arena; packets live through GPU drawing. Inputs
/// must stay clear of that storage. Leaves additive draw mode active.
void effectDrawGouraudDisc(const GfxCoord* centreCoord, s32 radius, const u8* rgb);

/// Draws one textured, additive `POLY_FT4` billboard at `arg0`'s projected
/// position. `arg1` is the animation frame (U origin `arg1 * 32`, the sprite
/// is 0x20 x 0x20 at V 0x18), `arg2` the radius (scaled by 31 and divided by
/// the projected OTZ) and `arg3` packs the sprite's CLUT index
/// (`Gp_QuadClutX`) in its top nibble and the spin angle in its low 12 bits,
/// so the quad's corners sit at `angle` and `angle + 0x400`.
void Gp_DrawFxQuad(GfxCoord* arg0, u16 arg1, s16 arg2, u16 arg3);

/// Draws a grayscale, semitransparent textured billboard at the projected
/// world position. `arg1 & 3` selects the 24-pixel animation frame, `arg2`
/// packs the texture bank in its top nibble and radius in its low 12 bits,
/// and `arg3` packs the CLUT index in its top nibble and brightness in its
/// low byte.
void func_800EB6E8(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3);

void Gp_DrawBand(GfxCoord* arg0, s16 arg1, u8* arg2);

void Gp_DrawBandEx(GfxCoord* arg0, s16 arg1, s32 arg2, u8* arg3);

/// Ends one counted effect by freeing its work and performing default task teardown.
///
/// `task` must be live and counted in the initialized `gRoomEffectState`. Call
/// once per effect. `effectWork` is an effect-specific allocation, normally held
/// in `Task::spawnArg2`; it must be `NULL` or the original pointer to a live
/// primary-heap block. The count decreases even when `effectWork` is `NULL`.
///
/// Release nested resources and unlink external nodes first. `effectWork` must
/// not also be owned through `Task::work`, which default teardown frees separately.
/// The work is released before child exit handlers run. This calls `taskKill`
/// directly, bypassing this task's replacement `exitCallback`; its teardown
/// requirements and deferred or immediate body/task release rules apply.
/// The spawn-argument pointer is left unchanged after release.
void effectKillTask(void* effectWork, Task* task);

void Gp_PulseState1C(void);

/// Enables semitransparency and queues the blend mode for an untextured primitive.
///
/// `primitive` must be an initialized, writable polygon, line or rectangle packet
/// already linked into the current depth ordering table at the same `depth`.
/// `depth` is the sorting depth before `gDisplayState.otDepthShift`, not a tag
/// index; the scaled value wraps to one of the 1024 depth tags. The low two bits
/// of `blendMode` select a `GPU_BLEND_*` mode.
///
/// Consumes `sizeof(DR_TPAGE)` bytes from `gGpuPrimCursor` without a capacity
/// check and prepends that command at the depth slot, ahead of the primitive.
/// It enables dithering, prohibits drawing into the displayed area and selects
/// the fixed 4-bit texture page at (640, 0). The draw mode remains active until
/// replaced. Both packets borrow the frame arena until GPU drawing completes.
void gpuSetPrimitiveBlendMode(void* primitive, s32 blendMode, s32 depth);

#endif // GAMEPLAY_ROOM_EFFECTS_H
