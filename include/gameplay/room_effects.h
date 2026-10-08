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
/// overlay is loaded. The controller clears these bindings at startup; zero
/// means the room has none.
extern s32 gRoomEffectSparkEmitterId;

/// Energy Ball balls currently in flight; `_attachmentIsCastBlocked` refuses a new
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

/// Spawns and places a counted effect task with an owned, zeroed `EffectWork`.
///
/// Requires a live `gRoomEffectState`. `effectId` packs a task bank (0..14) in
/// bits 16..30 and its unchecked descriptor index in bits 0..15; index zero
/// returns NULL. Bit 31 bypasses the ordinary limit of 129 live effects, but
/// the new effect is still counted. The descriptor must allocate either one
/// coordinate body or a TMD model with at least one part coordinate. Callback
/// code and borrowed model data must stay loaded for the effect's lifetime.
/// `spawnArg` is copied unchanged into `Task::spawnArg1` for that callback.
///
/// `offset` supplies word-aligned signed 16-bit game-coordinate XYZ, or NULL for zero, and
/// is copied into `EffectWork::pos`. A non-NULL `parentCoord` supplies the
/// placement's orientation and origin; its matrices and borrowed parent chain
/// must be initialized and writable for composition. A supplied nonzero-stamped
/// cache must include an initialized rotation. The offset is transformed from
/// that coordinate's local space. Without a placement coordinate, the offset
/// is transformed by `GsWSMATRIX`. The effect's root coordinate is parented to
/// `gGfxViewCoord` and composed before return, rather than attached to the
/// supplied coordinate's hierarchy.
///
/// The work retains `parentCoord` (or the view coordinate) and the original
/// `offset` pointer, including NULL. These pointers are borrowed; any callback
/// that follows them requires their storage to remain live. Some callers pass
/// temporary scratch storage, so retained addresses alone do not prove lifetime.
/// Returns borrowed work valid until effect teardown, or NULL on rejection or
/// allocation failure. A failed work allocation tears down the new task. The
/// installed exit callback frees the work and decrements the count before task
/// teardown; callbacks with additional resources must release those themselves.
EffectWork* effectSpawn(s32 effectId, GfxCoord* parentCoord, TaskSpawnArg spawnArg, SVECTOR* offset);

/// Hit-effect recipes selected by attack-property tables, separate from packed effect-task ids.
///
/// SPLATTER and SPLATTER_ALT run the same recipe. CONTROL_E3 selects the
/// bank-6 E3 handler; its further visual role is unproven. Kind 0, 14 and
/// other unlisted values perform no spawn.
enum {
    EFFECT_HIT_KIND_WEAPON_PUFF      = 1,
    EFFECT_HIT_KIND_TINTED_PUFF      = 2,
    EFFECT_HIT_KIND_BLAST            = 3,
    EFFECT_HIT_KIND_PARTICLE_EMITTER = 4,
    EFFECT_HIT_KIND_SPLATTER         = 5,
    EFFECT_HIT_KIND_SPARK_AND_PUFFS  = 6,
    EFFECT_HIT_KIND_SPARK_BURST      = 7,
    EFFECT_HIT_KIND_DENSE_PUFFS      = 8,
    EFFECT_HIT_KIND_SPLATTER_ALT     = 9,
    EFFECT_HIT_KIND_CONTROL_E3       = 10,
    EFFECT_HIT_KIND_BLAST_WITH_SOUND = 11,
    EFFECT_HIT_KIND_APOBIOSIS_SHARD  = 12,
    EFFECT_HIT_KIND_LIFE_DRAIN_MOTES = 13,
    EFFECT_HIT_KIND_HAMMER_FLASH     = 15,
    EFFECT_HIT_KIND_WEAPON_BLAST     = 16,
};

/// Dispatches a damage-hit effect recipe using call placement and an optional spawn record.
///
/// `effectKind` narrows to 16 bits before dispatch. Requires a live player
/// task/work and effect controller even for a recipe that spawns nothing.
/// `localOffset` is an optional signed XYZ offset in the chosen coordinate's
/// space; placement and borrowed-pointer lifetimes follow `effectSpawn`.
///
/// A NULL record selects the shared default {coord, 512, 1}, replacing its
/// coordinate on every call. An explicit record with NULL coord is filled from
/// the call or the view node; a NULL call coordinate reuses that record's node.
/// With both present they stay distinct: some recipes use the record node,
/// others the call node. The record is mutated but its address is not retained.
///
/// Recipes 3..5, 7, 9, 11 and 15 pack the signed record halves into a spawn word;
/// 10 forces its high half to 1; 16 does so only for alternate fire. Puff/mote
/// recipes use the high half as a count (8 triples it); 12 reads neither half.
/// Recipe 11 ignores the offset and requires attachment id's decimal suffix
/// in 1..3 for its sound table. Return is void; individual spawn failures are ignored.
void effectSpawnHit(s32 effectKind, GfxCoord* coord, SVECTOR* localOffset, EffectSpawnArg* spawnRecord);

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

/// Draws one additive, unmodulated cell of the eight-frame spinning billboard strip.
///
/// `coord` supplies a composed translation in the input space of `GsWSMATRIX`;
/// its rotation is unused and its position narrows to signed 16-bit coordinates.
/// `frame` selects a 32-by-32 texel cell at V=24..55; GPU UV bytes wrap it
/// modulo eight. `packedAnglePalette` holds a palette index 0..5 in bits 12..15
/// and a screen rotation in bits 0..11, in 4096 units per turn. At angle zero
/// the first corner is above the centre; increasing angles turn clockwise.
///
/// `size` is a signed sizing numerator: size * 31 / (SZ3 / 4 + 1) is the
/// screen-space half-diagonal in pixels before Q12 rotation. Division truncates
/// toward zero; intermediate products must fit s32. A negative GTE FLAG drops
/// the sprite. Borrows inputs for this call, reserves/releases scratch storage,
/// and appends one `POLY_FT4` to the unchecked frame arena when accepted.
/// Inputs must stay clear of that storage; the packet lives through GPU drawing.
void effectDrawSpinningBillboard(const GfxCoord* coord, u16 frame, s16 size, u16 packedAnglePalette);

/// Draws one additive, grayscale-modulated cell of a four-frame billboard strip.
///
/// `coord` supplies a composed translation in the input space of `GsWSMATRIX`;
/// rotation is unused and XYZ narrows to signed 16-bit game-coordinate units.
/// `frame` wraps modulo four. `packedSizeBank` holds a texture bank in bits
/// 12..15 and an unsigned sizing numerator in bits 0..11. Each bank advances
/// U by 96 texels; each frame spans 24 by 24 texels at V=0..23. UV bytes wrap
/// modulo 256. `packedBrightnessPalette` holds a palette index 0..5 in bits
/// 12..15 and an RGB modulation byte in bits 0..7; bits 8..11 are ignored.
/// Modulation 128 preserves texture brightness; palette colours are retained.
///
/// The quad stays aligned to the screen axes, with half-size
/// (packedSizeBank & 0xFFF) * 23 / (SZ3 / 4 + 1) pixels, truncated downward.
/// A negative GTE FLAG drops the sprite. Borrows inputs for this call,
/// reserves/releases scratch storage, and appends one `POLY_FT4` to the
/// unchecked frame arena when accepted. Inputs must stay clear of that
/// storage; the packet lives through GPU drawing.
void effectDrawModulatedBillboard(const GfxCoord* coord, u16 frame, u16 packedSizeBank, u16 packedBrightnessPalette);

/// Draws a raised additive band, coloured at its smaller ring and black at its wider rim.
///
/// Sixteen Gouraud quads join local XY rings: radius `innerRadius` at Z=256,
/// and radius (s16)(innerRadius + 256) at Z=0, in game-coordinate units.
/// `coord` must supply a composed rotation and translation in `GsWSMATRIX`'s
/// input space. Rotated vertices plus translation narrow to signed 16 bits;
/// `rgb` supplies three readable colour bytes. Signed radii are retained.
///
/// Each segment rejects a negative GTE FLAG after projecting its last three
/// corners and sorts by its last corner's SZ3 / 4 + 1. Reserves/releases one
/// scratch block and appends up to sixteen `POLY_G4`/`DR_TPAGE` pairs to the
/// unchecked frame arena. Inputs are borrowed for this call and must stay
/// clear of that storage; packets live through GPU drawing. Leaves additive
/// draw mode with dithering enabled after an accepted segment.
void effectDrawRaisedGlowBand(const GfxCoord* coord, s16 innerRadius, const u8* rgb);

/// Draws an additive local-XZ band, coloured at its inner edge and black at its outer edge.
///
/// Sixteen Gouraud quads join radii `innerRadius` and (s16)(innerRadius + width)
/// at local Y=0, in game-coordinate units. Radii and width are signed; the sum
/// must fit s32 before narrowing. `coord` must supply a composed rotation and
/// translation in `GsWSMATRIX`'s input space. Rotated vertices plus translation
/// narrow to signed 16 bits; translation additions must fit s32.
/// `rgb` supplies three readable colour bytes.
///
/// Each segment rejects a negative GTE FLAG after projecting its last three
/// corners and sorts by its last corner's SZ3 / 4 + 1. Reserves/releases one
/// scratch block and appends up to sixteen `POLY_G4`/`DR_TPAGE` pairs to the
/// unchecked frame arena. Inputs are borrowed for this call and must stay
/// clear of that storage; packets live through GPU drawing. Leaves additive
/// draw mode with dithering enabled after an accepted segment.
void effectDrawInnerGlowBand(const GfxCoord* coord, s16 innerRadius, s32 width, const u8* rgb);

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

/// Requests cancellation of ordinary and parasite-energy effects on the next update.
///
/// Requires a live `gRoomEffectState`. Requests coalesce in its pending flags;
/// the controller publishes cancellation through both control fields for one
/// update and clears the pending flags. Each effect applies its own teardown
/// policy when it observes the published control value.
void roomEffectRequestCancelAll(void);

/// Requests parasite-energy effect cancellation on the next room-effect update.
///
/// Requires a live `gRoomEffectState`. Pending requests coalesce; the
/// controller publishes PE cancellation for one update and clears the request.
/// Each affected effect applies its own teardown policy.
void roomEffectRequestCancelPe(void);

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
