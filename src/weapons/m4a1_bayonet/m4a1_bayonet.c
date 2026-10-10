#include "weapons/m4a1_bayonet.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "m4a1_bayonet_private.h"
#include "weapons/weapon.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/blade_trail.h"
#include "gameplay/animation.h"

/// Far ribbon endpoint in integer weapon-part coordinates, `(0, 0x300, 0x40)`.
///
/// The trail's persistent endpoint uses this translation relative to the held weapon.
static SVECTOR _gM4a1BayonetTrailTipOffset[1] = { { 0, 0x0300, 0x0040, 0 } };

/// Near ribbon endpoint translation, `(0, 0x180, 0x40)`, in integer coordinate units.
///
/// Recording applies it in the weapon-part frame; seeding applies the same offset
/// relative to the far endpoint, preserving the trail's initial pose.
static const SVECTOR _gM4a1BayonetTrailNearOffset __attribute__((section(".data"))) = { 0, 0x0180, 0x0040, 0 };

/// Stores a composed endpoint as a world-space trail pose independent of the weapon.
///
/// `composedEndpoint->workm.t` must be a current view-space position, and
/// `gGfxViewCoord.workm` the current world-to-view transform. Copies the
/// complete endpoint cache into `historyFrame->workm` and removes the view
/// transform into `historyFrame->coord`, with the persistent view node as
/// parent. Later composition follows camera movement without following the
/// weapon. Rotation is copied and rebased too; the ribbon uses only translation.
/// Rotation elements use ONE (4096) for 1.0; translations are signed 32-bit
/// game coordinates.
///
/// The view rotation must be orthonormal. The caller-owned nodes must be
/// word-aligned and disjoint; the destination must also be disjoint from the
/// view node. Reads only the source's composed cache and retains no pointer to
/// it. Leaves the destination's composition stamp and parameters untouched;
/// the caller must mark it dirty before recomposing. Requires an initialized
/// scratch stack with 48 free bytes disjoint from both arguments, released
/// before return. Changes GTE rotation, translation and arithmetic state.
static inline void _m4a1BayonetStoreTrailFrame(GfxCoord* historyFrame, const GfxCoord* composedEndpoint)
{
    historyFrame->parent = &gGfxViewCoord;
    historyFrame->workm  = composedEndpoint->workm;
    gte_SetRotMatrix(&composedEndpoint->workm);
    gte_SetTransMatrix(&composedEndpoint->workm);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &historyFrame->workm, &historyFrame->coord);
}

void m4a1BayonetTrailTask(Task* task)
{
    enum {
        M4A1_BAYONET_TRAIL_SEED           = 0,
        M4A1_BAYONET_TRAIL_RECORD         = 1,
        M4A1_BAYONET_TRAIL_LIFETIME_TICKS = 13
    };
    EffectWork*    effectWork;
    GfxCoord*      tipCoord;
    GfxCoord*      historyFrame;
    GfxCoord       nearEndpoint;
    s32            effectControl;
    const SVECTOR* nearOffset;
    s32            offsetX;
    s32            offsetY;
    s32            offsetZ;
    s32            historyIndex;
    s32            keepTask;

    effectWork    = task->spawnArg2.pointer;
    tipCoord      = task->extra.coordBody->coord;
    effectControl = gRoomEffectState->effectControl;
    if (effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        effectWork->age++;
        switch (task->state) {
            case M4A1_BAYONET_TRAIL_SEED:
                tipCoord->parent       = effectWork->parent;
                tipCoord->coord.t[0]   = _gM4a1BayonetTrailTipOffset[0].vx;
                tipCoord->coord.t[1]   = _gM4a1BayonetTrailTipOffset[0].vy;
                tipCoord->coord.t[2]   = _gM4a1BayonetTrailTipOffset[0].vz;
                tipCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(tipCoord);
                task->state = M4A1_BAYONET_TRAIL_RECORD;

                // Only endpoint translations feed the ribbon; the temporary rotation is left uninitialized.
                // The seed chains the second endpoint beneath the tip; recording uses the weapon parent.
                // This retained one-past access reads the adjacent near-offset bytes in the image,
                // but is outside the C array's bounds. A bounded pair still needs rematching.
                offsetX                   = _gM4a1BayonetTrailTipOffset[1].vx;
                nearOffset                = &_gM4a1BayonetTrailTipOffset[1];
                offsetY                   = nearOffset->vy;
                offsetZ                   = nearOffset->vz;
                nearEndpoint.parent       = tipCoord;
                nearEndpoint.composeStamp = GRAPHICS_COORD_DIRTY;
                nearEndpoint.coord.t[0]   = offsetX;
                nearEndpoint.coord.t[1]   = offsetY;
                nearEndpoint.coord.t[2]   = offsetZ;
                actorRenderComposeCoord(&nearEndpoint);

                // Seed every slot with the same world pose, detached from the moving weapon.
                for (historyIndex = 0; historyIndex < ARRAY_SIZE(gBladeTrailBase); historyIndex++) {
                    historyFrame = &gBladeTrailBase[historyIndex];
                    _m4a1BayonetStoreTrailFrame(historyFrame, tipCoord);

                    historyFrame = &gBladeTrailTip[historyIndex];
                    _m4a1BayonetStoreTrailFrame(historyFrame, &nearEndpoint);
                }
                break;
            case M4A1_BAYONET_TRAIL_RECORD:
                tipCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(tipCoord);

                nearEndpoint.parent       = effectWork->parent;
                nearEndpoint.composeStamp = GRAPHICS_COORD_DIRTY;
                nearEndpoint.coord.t[0]   = _gM4a1BayonetTrailNearOffset.vx;
                nearEndpoint.coord.t[1]   = _gM4a1BayonetTrailNearOffset.vy;
                nearEndpoint.coord.t[2]   = _gM4a1BayonetTrailNearOffset.vz;
                actorRenderComposeCoord(&nearEndpoint);

                historyFrame = &gBladeTrailBase[effectWork->age & (ARRAY_SIZE(gBladeTrailBase) - 1)];
                _m4a1BayonetStoreTrailFrame(historyFrame, tipCoord);

                historyFrame = &gBladeTrailTip[effectWork->age & (ARRAY_SIZE(gBladeTrailTip) - 1)];
                _m4a1BayonetStoreTrailFrame(historyFrame, &nearEndpoint);

                // Recompose all retained world poses against the current view before drawing.
                for (historyIndex = 0; historyIndex < ARRAY_SIZE(gBladeTrailBase); historyIndex++) {
                    historyFrame               = &gBladeTrailBase[historyIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                    historyFrame               = &gBladeTrailTip[historyIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                }
                _bladeTrailDraw(effectWork->age & (ARRAY_SIZE(gBladeTrailBase) - 1), BLADE_TRAIL_TINT_BLUE_WHITE);
                break;
        }
        keepTask = effectWork->age < M4A1_BAYONET_TRAIL_LIFETIME_TICKS;
    } else {
        keepTask = effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN;
    }
    if (!keepTask) {
        effectKillTask(effectWork, task);
    }
}

#define BLADE_TRAIL_BASE_HISTORY gBladeTrailBase
#define BLADE_TRAIL_TIP_HISTORY  gBladeTrailTip
#include "../../shared/blade_trail_draw.inc.c"
#undef BLADE_TRAIL_BASE_HISTORY
#undef BLADE_TRAIL_TIP_HISTORY

/// Identity root pose for the held rifle and bayonet; model instances copy this transform.
static const TmdBone _gM4a1BayonetWeaponModelSkeleton[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_model_01130_skeleton.inc"
};

/// Vertex count of the held weapon's single rigid part.
static const u32 _gM4a1BayonetWeaponModelPartVerts[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_model_01130_partVerts.inc"
};

/// Held rifle and bayonet vertices in integer coordinates local to the weapon part.
static SVECTOR _gM4a1BayonetWeaponModelVerts[] = {
#include "assets/m4a1_bayonet_model_01130_verts.inc"
};

/// Part-local lighting normals, indexed independently of the held weapon's vertices.
static SVECTOR _gM4a1BayonetWeaponModelNormals[] = {
#include "assets/m4a1_bayonet_model_01130_normals.inc"
};

/// Writable commands for six textured Gouraud triangles and 58 quads, then group and stream markers.
///
/// Model creation resolves the draw-handler slots in place.
static u32 _gM4a1BayonetWeaponModelStream[] = {
#include "assets/m4a1_bayonet_model_01130_stream.inc"
};

TmdSource gM4a1BayonetWeaponModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (6 * sizeof(POLY_GT3)) + (58 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gM4a1BayonetWeaponModelSkeleton),
    .partVertexCounts    = _gM4a1BayonetWeaponModelPartVerts,
    .verts               = _gM4a1BayonetWeaponModelVerts,
    .normals             = _gM4a1BayonetWeaponModelNormals,
    .skeleton            = _gM4a1BayonetWeaponModelSkeleton,
    .stream              = _gM4a1BayonetWeaponModelStream,
};

/// Idle translation and Euler poses in encoding-1 three-word entries, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_019F0_bank1.inc"
};

/// Idle packed Euler poses in encoding-4 words, with each angle step equal to 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_019F0_bank4.inc"
};

/// Idle keyframe durations in normal-rate frames, pose-word offsets and track controls.
static const AnimationRecord _gM4a1BayonetIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_019F0_records.inc"
};

/// Nineteen idle track starts as absolute record indices, plus the exporter's trailing halfword.
static const u16 _gM4a1BayonetIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_019F0_indices.inc"
};

/// Standing-idle pose for the M4A1 Bayonet player animation bank.
///
/// Nineteen tracks repeat their held poses; bank slots 1, 21, 22 and 41 share this set.
/// Records, indices and pose banks are read-only in their serialized data positions
/// and must stay loaded while any playback slot borrows the clip.
static AnimationSet _gM4a1BayonetIdleAnimation = {
    .records           = _gM4a1BayonetIdleAnimationRecords,
    .trackStartIndices = _gM4a1BayonetIdleAnimationIndices,
    .poseBanks         = { NULL, _gM4a1BayonetIdleAnimationBank1, NULL, NULL, _gM4a1BayonetIdleAnimationBank4, NULL, NULL, NULL },
};

/// Unarmed-walk translation and Euler poses in encoding-1 entries, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetUnarmedWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_02094_bank1.inc"
};

/// Unarmed-walk packed Euler poses in encoding-4 words, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetUnarmedWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_02094_bank4.inc"
};

/// Unarmed-walk keyframe durations in normal-rate frames, pose-word offsets and track controls.
static const AnimationRecord _gM4a1BayonetUnarmedWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_02094_records.inc"
};

/// Nineteen unarmed-walk track starts as absolute record indices, plus the exporter's trailing halfword.
static const u16 _gM4a1BayonetUnarmedWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_02094_indices.inc"
};

/// Forward walking clip while the player's held weapon model is absent.
///
/// Bank slot 19 selects nineteen looping tracks from the M4A1 Bayonet package.
/// Records, indices and pose banks are read-only in their serialized data positions
/// and must stay loaded while any playback slot borrows the clip.
static AnimationSet _gM4a1BayonetUnarmedWalkAnimation = {
    .records           = _gM4a1BayonetUnarmedWalkAnimationRecords,
    .trackStartIndices = _gM4a1BayonetUnarmedWalkAnimationIndices,
    .poseBanks         = { NULL, _gM4a1BayonetUnarmedWalkAnimationBank1, NULL, NULL, _gM4a1BayonetUnarmedWalkAnimationBank4, NULL, NULL, NULL },
};

/// Negative-turn translation and Euler poses in encoding-1 entries, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetNegativeTurnAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_028F4_bank1.inc"
};

/// Negative-turn packed Euler poses in encoding-4 words, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetNegativeTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_028F4_bank4.inc"
};

/// Negative-turn keyframe durations in normal-rate frames, pose-word offsets and track controls.
static const AnimationRecord _gM4a1BayonetNegativeTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_028F4_records.inc"
};

/// Nineteen negative-turn track starts as absolute record indices, plus the exporter's trailing halfword.
static const u16 _gM4a1BayonetNegativeTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_028F4_indices.inc"
};

/// Stationary turn toward decreasing yaw for the M4A1 Bayonet player animation bank.
///
/// Manual and scripted negative turns select bank slot 5 and its nineteen looping tracks.
/// Records, indices and pose banks are read-only in their serialized data positions
/// and must stay loaded while any playback slot borrows the clip.
static AnimationSet _gM4a1BayonetNegativeTurnAnimation = {
    .records           = _gM4a1BayonetNegativeTurnAnimationRecords,
    .trackStartIndices = _gM4a1BayonetNegativeTurnAnimationIndices,
    .poseBanks         = { NULL, _gM4a1BayonetNegativeTurnAnimationBank1, NULL, NULL, _gM4a1BayonetNegativeTurnAnimationBank4, NULL, NULL, NULL },
};

/// Positive-turn translation and Euler poses in encoding-1 entries, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetPositiveTurnAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_03158_bank1.inc"
};

/// Positive-turn packed Euler poses in encoding-4 words, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetPositiveTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_03158_bank4.inc"
};

/// Positive-turn keyframe durations in normal-rate frames, pose-word offsets and track controls.
static const AnimationRecord _gM4a1BayonetPositiveTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_03158_records.inc"
};

/// Nineteen positive-turn track starts as absolute record indices, plus the exporter's trailing halfword.
static const u16 _gM4a1BayonetPositiveTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_03158_indices.inc"
};

/// Stationary turn toward increasing yaw for the M4A1 Bayonet player animation bank.
///
/// Manual positive and scripted nonnegative turns select bank slot 6 and its nineteen looping tracks.
/// Records, indices and pose banks are read-only in their serialized data positions
/// and must stay loaded while any playback slot borrows the clip.
static AnimationSet _gM4a1BayonetPositiveTurnAnimation = {
    .records           = _gM4a1BayonetPositiveTurnAnimationRecords,
    .trackStartIndices = _gM4a1BayonetPositiveTurnAnimationIndices,
    .poseBanks         = { NULL, _gM4a1BayonetPositiveTurnAnimationBank1, NULL, NULL, _gM4a1BayonetPositiveTurnAnimationBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation0346CBank1[3] = {
#include "assets/m4a1_bayonet_animation_0346C_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation0346CBank4[69] = {
#include "assets/m4a1_bayonet_animation_0346C_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation0346CRecords[99] = {
#include "assets/m4a1_bayonet_animation_0346C_records.inc"
};

static u16 _gM4a1BayonetAnimation0346CIndices[20] = {
#include "assets/m4a1_bayonet_animation_0346C_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation0346C = {
    _gM4a1BayonetAnimation0346CRecords,
    _gM4a1BayonetAnimation0346CIndices,
    { NULL, _gM4a1BayonetAnimation0346CBank1, NULL, NULL, _gM4a1BayonetAnimation0346CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation03BC8Bank1[14] = {
#include "assets/m4a1_bayonet_animation_03BC8_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation03BC8Bank4[156] = {
#include "assets/m4a1_bayonet_animation_03BC8_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation03BC8Records[253] = {
#include "assets/m4a1_bayonet_animation_03BC8_records.inc"
};

static u16 _gM4a1BayonetAnimation03BC8Indices[20] = {
#include "assets/m4a1_bayonet_animation_03BC8_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation03BC8 = {
    _gM4a1BayonetAnimation03BC8Records,
    _gM4a1BayonetAnimation03BC8Indices,
    { NULL, _gM4a1BayonetAnimation03BC8Bank1, NULL, NULL, _gM4a1BayonetAnimation03BC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation04350Bank1[16] = {
#include "assets/m4a1_bayonet_animation_04350_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation04350Bank4[167] = {
#include "assets/m4a1_bayonet_animation_04350_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation04350Records[247] = {
#include "assets/m4a1_bayonet_animation_04350_records.inc"
};

static u16 _gM4a1BayonetAnimation04350Indices[20] = {
#include "assets/m4a1_bayonet_animation_04350_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation04350 = {
    _gM4a1BayonetAnimation04350Records,
    _gM4a1BayonetAnimation04350Indices,
    { NULL, _gM4a1BayonetAnimation04350Bank1, NULL, NULL, _gM4a1BayonetAnimation04350Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation04624Bank1[6] = {
#include "assets/m4a1_bayonet_animation_04624_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation04624Bank4[52] = {
#include "assets/m4a1_bayonet_animation_04624_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation04624Records[91] = {
#include "assets/m4a1_bayonet_animation_04624_records.inc"
};

static u16 _gM4a1BayonetAnimation04624Indices[20] = {
#include "assets/m4a1_bayonet_animation_04624_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation04624 = {
    _gM4a1BayonetAnimation04624Records,
    _gM4a1BayonetAnimation04624Indices,
    { NULL, _gM4a1BayonetAnimation04624Bank1, NULL, NULL, _gM4a1BayonetAnimation04624Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation049B4Bank1[7] = {
#include "assets/m4a1_bayonet_animation_049B4_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation049B4Bank4[73] = {
#include "assets/m4a1_bayonet_animation_049B4_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation049B4Records[114] = {
#include "assets/m4a1_bayonet_animation_049B4_records.inc"
};

static u16 _gM4a1BayonetAnimation049B4Indices[20] = {
#include "assets/m4a1_bayonet_animation_049B4_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation049B4 = {
    _gM4a1BayonetAnimation049B4Records,
    _gM4a1BayonetAnimation049B4Indices,
    { NULL, _gM4a1BayonetAnimation049B4Bank1, NULL, NULL, _gM4a1BayonetAnimation049B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation04E3CBank1[9] = {
#include "assets/m4a1_bayonet_animation_04E3C_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation04E3CBank4[104] = {
#include "assets/m4a1_bayonet_animation_04E3C_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation04E3CRecords[139] = {
#include "assets/m4a1_bayonet_animation_04E3C_records.inc"
};

static u16 _gM4a1BayonetAnimation04E3CIndices[20] = {
#include "assets/m4a1_bayonet_animation_04E3C_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation04E3C = {
    _gM4a1BayonetAnimation04E3CRecords,
    _gM4a1BayonetAnimation04E3CIndices,
    { NULL, _gM4a1BayonetAnimation04E3CBank1, NULL, NULL, _gM4a1BayonetAnimation04E3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation05038Bank1[3] = {
#include "assets/m4a1_bayonet_animation_05038_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation05038Bank4[22] = {
#include "assets/m4a1_bayonet_animation_05038_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation05038Records[76] = {
#include "assets/m4a1_bayonet_animation_05038_records.inc"
};

static u16 _gM4a1BayonetAnimation05038Indices[20] = {
#include "assets/m4a1_bayonet_animation_05038_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation05038 = {
    _gM4a1BayonetAnimation05038Records,
    _gM4a1BayonetAnimation05038Indices,
    { NULL, _gM4a1BayonetAnimation05038Bank1, NULL, NULL, _gM4a1BayonetAnimation05038Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation05310Bank1[6] = {
#include "assets/m4a1_bayonet_animation_05310_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation05310Bank4[57] = {
#include "assets/m4a1_bayonet_animation_05310_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation05310Records[87] = {
#include "assets/m4a1_bayonet_animation_05310_records.inc"
};

static u16 _gM4a1BayonetAnimation05310Indices[20] = {
#include "assets/m4a1_bayonet_animation_05310_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation05310 = {
    _gM4a1BayonetAnimation05310Records,
    _gM4a1BayonetAnimation05310Indices,
    { NULL, _gM4a1BayonetAnimation05310Bank1, NULL, NULL, _gM4a1BayonetAnimation05310Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation055B4Bank1[4] = {
#include "assets/m4a1_bayonet_animation_055B4_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation055B4Bank4[55] = {
#include "assets/m4a1_bayonet_animation_055B4_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation055B4Records[82] = {
#include "assets/m4a1_bayonet_animation_055B4_records.inc"
};

static u16 _gM4a1BayonetAnimation055B4Indices[20] = {
#include "assets/m4a1_bayonet_animation_055B4_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation055B4 = {
    _gM4a1BayonetAnimation055B4Records,
    _gM4a1BayonetAnimation055B4Indices,
    { NULL, _gM4a1BayonetAnimation055B4Bank1, NULL, NULL, _gM4a1BayonetAnimation055B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation057B4Bank1[3] = {
#include "assets/m4a1_bayonet_animation_057B4_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation057B4Bank4[23] = {
#include "assets/m4a1_bayonet_animation_057B4_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation057B4Records[76] = {
#include "assets/m4a1_bayonet_animation_057B4_records.inc"
};

static u16 _gM4a1BayonetAnimation057B4Indices[20] = {
#include "assets/m4a1_bayonet_animation_057B4_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation057B4 = {
    _gM4a1BayonetAnimation057B4Records,
    _gM4a1BayonetAnimation057B4Indices,
    { NULL, _gM4a1BayonetAnimation057B4Bank1, NULL, NULL, _gM4a1BayonetAnimation057B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation05B08Bank1[8] = {
#include "assets/m4a1_bayonet_animation_05B08_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation05B08Bank4[68] = {
#include "assets/m4a1_bayonet_animation_05B08_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation05B08Records[101] = {
#include "assets/m4a1_bayonet_animation_05B08_records.inc"
};

static u16 _gM4a1BayonetAnimation05B08Indices[20] = {
#include "assets/m4a1_bayonet_animation_05B08_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation05B08 = {
    _gM4a1BayonetAnimation05B08Records,
    _gM4a1BayonetAnimation05B08Indices,
    { NULL, _gM4a1BayonetAnimation05B08Bank1, NULL, NULL, _gM4a1BayonetAnimation05B08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation05DBCBank1[5] = {
#include "assets/m4a1_bayonet_animation_05DBC_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation05DBCBank4[55] = {
#include "assets/m4a1_bayonet_animation_05DBC_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation05DBCRecords[83] = {
#include "assets/m4a1_bayonet_animation_05DBC_records.inc"
};

static u16 _gM4a1BayonetAnimation05DBCIndices[20] = {
#include "assets/m4a1_bayonet_animation_05DBC_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation05DBC = {
    _gM4a1BayonetAnimation05DBCRecords,
    _gM4a1BayonetAnimation05DBCIndices,
    { NULL, _gM4a1BayonetAnimation05DBCBank1, NULL, NULL, _gM4a1BayonetAnimation05DBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation060DCBank1[6] = {
#include "assets/m4a1_bayonet_animation_060DC_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation060DCBank4[66] = {
#include "assets/m4a1_bayonet_animation_060DC_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation060DCRecords[96] = {
#include "assets/m4a1_bayonet_animation_060DC_records.inc"
};

static u16 _gM4a1BayonetAnimation060DCIndices[20] = {
#include "assets/m4a1_bayonet_animation_060DC_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation060DC = {
    _gM4a1BayonetAnimation060DCRecords,
    _gM4a1BayonetAnimation060DCIndices,
    { NULL, _gM4a1BayonetAnimation060DCBank1, NULL, NULL, _gM4a1BayonetAnimation060DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation068A0Bank1[18] = {
#include "assets/m4a1_bayonet_animation_068A0_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation068A0Bank4[184] = {
#include "assets/m4a1_bayonet_animation_068A0_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation068A0Records[239] = {
#include "assets/m4a1_bayonet_animation_068A0_records.inc"
};

static u16 _gM4a1BayonetAnimation068A0Indices[20] = {
#include "assets/m4a1_bayonet_animation_068A0_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation068A0 = {
    _gM4a1BayonetAnimation068A0Records,
    _gM4a1BayonetAnimation068A0Indices,
    { NULL, _gM4a1BayonetAnimation068A0Bank1, NULL, NULL, _gM4a1BayonetAnimation068A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation07B38Bank1[29] = {
#include "assets/m4a1_bayonet_animation_07B38_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation07B38Bank4[450] = {
#include "assets/m4a1_bayonet_animation_07B38_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation07B38Records[633] = {
#include "assets/m4a1_bayonet_animation_07B38_records.inc"
};

static u16 _gM4a1BayonetAnimation07B38Indices[20] = {
#include "assets/m4a1_bayonet_animation_07B38_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation07B38 = {
    _gM4a1BayonetAnimation07B38Records,
    _gM4a1BayonetAnimation07B38Indices,
    { NULL, _gM4a1BayonetAnimation07B38Bank1, NULL, NULL, _gM4a1BayonetAnimation07B38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation086B0Bank1[12] = {
#include "assets/m4a1_bayonet_animation_086B0_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation086B0Bank4[266] = {
#include "assets/m4a1_bayonet_animation_086B0_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation086B0Records[412] = {
#include "assets/m4a1_bayonet_animation_086B0_records.inc"
};

static u16 _gM4a1BayonetAnimation086B0Indices[20] = {
#include "assets/m4a1_bayonet_animation_086B0_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation086B0 = {
    _gM4a1BayonetAnimation086B0Records,
    _gM4a1BayonetAnimation086B0Indices,
    { NULL, _gM4a1BayonetAnimation086B0Bank1, NULL, NULL, _gM4a1BayonetAnimation086B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation08DCCBank1[9] = {
#include "assets/m4a1_bayonet_animation_08DCC_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation08DCCBank4[144] = {
#include "assets/m4a1_bayonet_animation_08DCC_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation08DCCRecords[264] = {
#include "assets/m4a1_bayonet_animation_08DCC_records.inc"
};

static u16 _gM4a1BayonetAnimation08DCCIndices[20] = {
#include "assets/m4a1_bayonet_animation_08DCC_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation08DCC = {
    _gM4a1BayonetAnimation08DCCRecords,
    _gM4a1BayonetAnimation08DCCIndices,
    { NULL, _gM4a1BayonetAnimation08DCCBank1, NULL, NULL, _gM4a1BayonetAnimation08DCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation0924CBank1[6] = {
#include "assets/m4a1_bayonet_animation_0924C_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation0924CBank4[107] = {
#include "assets/m4a1_bayonet_animation_0924C_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation0924CRecords[143] = {
#include "assets/m4a1_bayonet_animation_0924C_records.inc"
};

static u16 _gM4a1BayonetAnimation0924CIndices[20] = {
#include "assets/m4a1_bayonet_animation_0924C_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation0924C = {
    _gM4a1BayonetAnimation0924CRecords,
    _gM4a1BayonetAnimation0924CIndices,
    { NULL, _gM4a1BayonetAnimation0924CBank1, NULL, NULL, _gM4a1BayonetAnimation0924CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation09424Bank1[3] = {
#include "assets/m4a1_bayonet_animation_09424_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation09424Bank4[32] = {
#include "assets/m4a1_bayonet_animation_09424_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation09424Records[57] = {
#include "assets/m4a1_bayonet_animation_09424_records.inc"
};

static u16 _gM4a1BayonetAnimation09424Indices[20] = {
#include "assets/m4a1_bayonet_animation_09424_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation09424 = {
    _gM4a1BayonetAnimation09424Records,
    _gM4a1BayonetAnimation09424Indices,
    { NULL, _gM4a1BayonetAnimation09424Bank1, NULL, NULL, _gM4a1BayonetAnimation09424Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation09988Bank1[11] = {
#include "assets/m4a1_bayonet_animation_09988_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation09988Bank4[125] = {
#include "assets/m4a1_bayonet_animation_09988_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation09988Records[167] = {
#include "assets/m4a1_bayonet_animation_09988_records.inc"
};

static u16 _gM4a1BayonetAnimation09988Indices[20] = {
#include "assets/m4a1_bayonet_animation_09988_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation09988 = {
    _gM4a1BayonetAnimation09988Records,
    _gM4a1BayonetAnimation09988Indices,
    { NULL, _gM4a1BayonetAnimation09988Bank1, NULL, NULL, _gM4a1BayonetAnimation09988Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation09B7CBank1[3] = {
#include "assets/m4a1_bayonet_animation_09B7C_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation09B7CBank4[20] = {
#include "assets/m4a1_bayonet_animation_09B7C_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation09B7CRecords[76] = {
#include "assets/m4a1_bayonet_animation_09B7C_records.inc"
};

static u16 _gM4a1BayonetAnimation09B7CIndices[20] = {
#include "assets/m4a1_bayonet_animation_09B7C_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation09B7C = {
    _gM4a1BayonetAnimation09B7CRecords,
    _gM4a1BayonetAnimation09B7CIndices,
    { NULL, _gM4a1BayonetAnimation09B7CBank1, NULL, NULL, _gM4a1BayonetAnimation09B7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation09FFCBank1[8] = {
#include "assets/m4a1_bayonet_animation_09FFC_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation09FFCBank4[105] = {
#include "assets/m4a1_bayonet_animation_09FFC_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation09FFCRecords[139] = {
#include "assets/m4a1_bayonet_animation_09FFC_records.inc"
};

static u16 _gM4a1BayonetAnimation09FFCIndices[20] = {
#include "assets/m4a1_bayonet_animation_09FFC_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation09FFC = {
    _gM4a1BayonetAnimation09FFCRecords,
    _gM4a1BayonetAnimation09FFCIndices,
    { NULL, _gM4a1BayonetAnimation09FFCBank1, NULL, NULL, _gM4a1BayonetAnimation09FFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation0A1D8Bank1[2] = {
#include "assets/m4a1_bayonet_animation_0A1D8_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation0A1D8Bank4[17] = {
#include "assets/m4a1_bayonet_animation_0A1D8_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation0A1D8Records[76] = {
#include "assets/m4a1_bayonet_animation_0A1D8_records.inc"
};

static u16 _gM4a1BayonetAnimation0A1D8Indices[20] = {
#include "assets/m4a1_bayonet_animation_0A1D8_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation0A1D8 = {
    _gM4a1BayonetAnimation0A1D8Records,
    _gM4a1BayonetAnimation0A1D8Indices,
    { NULL, _gM4a1BayonetAnimation0A1D8Bank1, NULL, NULL, _gM4a1BayonetAnimation0A1D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation0A9C0Bank1[14] = {
#include "assets/m4a1_bayonet_animation_0A9C0_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation0A9C0Bank4[189] = {
#include "assets/m4a1_bayonet_animation_0A9C0_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation0A9C0Records[255] = {
#include "assets/m4a1_bayonet_animation_0A9C0_records.inc"
};

static u16 _gM4a1BayonetAnimation0A9C0Indices[20] = {
#include "assets/m4a1_bayonet_animation_0A9C0_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation0A9C0 = {
    _gM4a1BayonetAnimation0A9C0Records,
    _gM4a1BayonetAnimation0A9C0Indices,
    { NULL, _gM4a1BayonetAnimation0A9C0Bank1, NULL, NULL, _gM4a1BayonetAnimation0A9C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation0B468Bank1[19] = {
#include "assets/m4a1_bayonet_animation_0B468_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation0B468Bank4[269] = {
#include "assets/m4a1_bayonet_animation_0B468_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation0B468Records[336] = {
#include "assets/m4a1_bayonet_animation_0B468_records.inc"
};

static u16 _gM4a1BayonetAnimation0B468Indices[20] = {
#include "assets/m4a1_bayonet_animation_0B468_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation0B468 = {
    _gM4a1BayonetAnimation0B468Records,
    _gM4a1BayonetAnimation0B468Indices,
    { NULL, _gM4a1BayonetAnimation0B468Bank1, NULL, NULL, _gM4a1BayonetAnimation0B468Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation0C3CCBank1[26] = {
#include "assets/m4a1_bayonet_animation_0C3CC_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation0C3CCBank4[416] = {
#include "assets/m4a1_bayonet_animation_0C3CC_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation0C3CCRecords[471] = {
#include "assets/m4a1_bayonet_animation_0C3CC_records.inc"
};

static u16 _gM4a1BayonetAnimation0C3CCIndices[20] = {
#include "assets/m4a1_bayonet_animation_0C3CC_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation0C3CC = {
    _gM4a1BayonetAnimation0C3CCRecords,
    _gM4a1BayonetAnimation0C3CCIndices,
    { NULL, _gM4a1BayonetAnimation0C3CCBank1, NULL, NULL, _gM4a1BayonetAnimation0C3CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation0C798Bank1[6] = {
#include "assets/m4a1_bayonet_animation_0C798_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation0C798Bank4[80] = {
#include "assets/m4a1_bayonet_animation_0C798_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation0C798Records[125] = {
#include "assets/m4a1_bayonet_animation_0C798_records.inc"
};

static u16 _gM4a1BayonetAnimation0C798Indices[20] = {
#include "assets/m4a1_bayonet_animation_0C798_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation0C798 = {
    _gM4a1BayonetAnimation0C798Records,
    _gM4a1BayonetAnimation0C798Indices,
    { NULL, _gM4a1BayonetAnimation0C798Bank1, NULL, NULL, _gM4a1BayonetAnimation0C798Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation0D4D8Bank1[23] = {
#include "assets/m4a1_bayonet_animation_0D4D8_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation0D4D8Bank4[346] = {
#include "assets/m4a1_bayonet_animation_0D4D8_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation0D4D8Records[413] = {
#include "assets/m4a1_bayonet_animation_0D4D8_records.inc"
};

static u16 _gM4a1BayonetAnimation0D4D8Indices[20] = {
#include "assets/m4a1_bayonet_animation_0D4D8_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation0D4D8 = {
    _gM4a1BayonetAnimation0D4D8Records,
    _gM4a1BayonetAnimation0D4D8Indices,
    { NULL, _gM4a1BayonetAnimation0D4D8Bank1, NULL, NULL, _gM4a1BayonetAnimation0D4D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation0DC18Bank1[13] = {
#include "assets/m4a1_bayonet_animation_0DC18_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation0DC18Bank4[182] = {
#include "assets/m4a1_bayonet_animation_0DC18_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation0DC18Records[223] = {
#include "assets/m4a1_bayonet_animation_0DC18_records.inc"
};

static u16 _gM4a1BayonetAnimation0DC18Indices[20] = {
#include "assets/m4a1_bayonet_animation_0DC18_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation0DC18 = {
    _gM4a1BayonetAnimation0DC18Records,
    _gM4a1BayonetAnimation0DC18Indices,
    { NULL, _gM4a1BayonetAnimation0DC18Bank1, NULL, NULL, _gM4a1BayonetAnimation0DC18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation0E648Bank1[18] = {
#include "assets/m4a1_bayonet_animation_0E648_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation0E648Bank4[266] = {
#include "assets/m4a1_bayonet_animation_0E648_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation0E648Records[312] = {
#include "assets/m4a1_bayonet_animation_0E648_records.inc"
};

static u16 _gM4a1BayonetAnimation0E648Indices[20] = {
#include "assets/m4a1_bayonet_animation_0E648_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation0E648 = {
    _gM4a1BayonetAnimation0E648Records,
    _gM4a1BayonetAnimation0E648Indices,
    { NULL, _gM4a1BayonetAnimation0E648Bank1, NULL, NULL, _gM4a1BayonetAnimation0E648Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation0EF00Bank1[18] = {
#include "assets/m4a1_bayonet_animation_0EF00_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation0EF00Bank4[201] = {
#include "assets/m4a1_bayonet_animation_0EF00_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation0EF00Records[283] = {
#include "assets/m4a1_bayonet_animation_0EF00_records.inc"
};

static u16 _gM4a1BayonetAnimation0EF00Indices[20] = {
#include "assets/m4a1_bayonet_animation_0EF00_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation0EF00 = {
    _gM4a1BayonetAnimation0EF00Records,
    _gM4a1BayonetAnimation0EF00Indices,
    { NULL, _gM4a1BayonetAnimation0EF00Bank1, NULL, NULL, _gM4a1BayonetAnimation0EF00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation0F6A0Bank1[16] = {
#include "assets/m4a1_bayonet_animation_0F6A0_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation0F6A0Bank4[157] = {
#include "assets/m4a1_bayonet_animation_0F6A0_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation0F6A0Records[263] = {
#include "assets/m4a1_bayonet_animation_0F6A0_records.inc"
};

static u16 _gM4a1BayonetAnimation0F6A0Indices[20] = {
#include "assets/m4a1_bayonet_animation_0F6A0_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation0F6A0 = {
    _gM4a1BayonetAnimation0F6A0Records,
    _gM4a1BayonetAnimation0F6A0Indices,
    { NULL, _gM4a1BayonetAnimation0F6A0Bank1, NULL, NULL, _gM4a1BayonetAnimation0F6A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1BayonetAnimation10074Bank1[25] = {
#include "assets/m4a1_bayonet_animation_10074_bank1.inc"
};

static AnimationPackedRotation _gM4a1BayonetAnimation10074Bank4[223] = {
#include "assets/m4a1_bayonet_animation_10074_bank4.inc"
};

static AnimationRecord _gM4a1BayonetAnimation10074Records[311] = {
#include "assets/m4a1_bayonet_animation_10074_records.inc"
};

static u16 _gM4a1BayonetAnimation10074Indices[20] = {
#include "assets/m4a1_bayonet_animation_10074_indices.inc"
};

static AnimationSet _gM4a1BayonetAnimation10074 = {
    _gM4a1BayonetAnimation10074Records,
    _gM4a1BayonetAnimation10074Indices,
    { NULL, _gM4a1BayonetAnimation10074Bank1, NULL, NULL, _gM4a1BayonetAnimation10074Bank4, NULL, NULL, NULL },
};

AnimationBank D_m4a1_bayonet_8012D25C = { { {
    NULL,
    &_gM4a1BayonetIdleAnimation,
    &_gM4a1BayonetAnimation0EF00,
    &_gM4a1BayonetAnimation0F6A0,
    &_gM4a1BayonetAnimation10074,
    &_gM4a1BayonetNegativeTurnAnimation,
    &_gM4a1BayonetPositiveTurnAnimation,
    &_gM4a1BayonetAnimation0DC18,
    &_gM4a1BayonetAnimation0E648,
    &_gM4a1BayonetAnimation0A1D8,
    &_gM4a1BayonetAnimation0C798,
    &_gM4a1BayonetAnimation0D4D8,
    &_gM4a1BayonetAnimation0B468,
    &_gM4a1BayonetAnimation0A9C0,
    &_gM4a1BayonetAnimation0C3CC,
    &_gM4a1BayonetAnimation0C3CC,
    &_gM4a1BayonetAnimation05DBC,
    &_gM4a1BayonetAnimation060DC,
    &_gM4a1BayonetAnimation068A0,
    &_gM4a1BayonetUnarmedWalkAnimation,
    &_gM4a1BayonetAnimation0C3CC,
    &_gM4a1BayonetIdleAnimation,
    &_gM4a1BayonetIdleAnimation,
    &_gM4a1BayonetAnimation07B38,
    &_gM4a1BayonetAnimation08DCC,
    &_gM4a1BayonetAnimation086B0,
    &_gM4a1BayonetAnimation04E3C,
    &_gM4a1BayonetAnimation05038,
    &_gM4a1BayonetAnimation05310,
    &_gM4a1BayonetAnimation055B4,
    &_gM4a1BayonetAnimation057B4,
    &_gM4a1BayonetAnimation05B08,
    &_gM4a1BayonetAnimation0924C,
    &_gM4a1BayonetAnimation09424,
    &_gM4a1BayonetAnimation0924C,
    &_gM4a1BayonetAnimation09424,
    &_gM4a1BayonetAnimation03BC8,
    &_gM4a1BayonetAnimation04350,
    &_gM4a1BayonetAnimation049B4,
    &_gM4a1BayonetAnimation04624,
    &_gM4a1BayonetAnimation0346C,
    &_gM4a1BayonetIdleAnimation,
    &_gM4a1BayonetAnimation09988,
    &_gM4a1BayonetAnimation09B7C,
    &_gM4a1BayonetAnimation09FFC,
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
    NULL,
    NULL,
} } };

GfxCoord gBladeTrailBase[8] = { 0 };
GfxCoord gBladeTrailTip[8]  = { 0 };
