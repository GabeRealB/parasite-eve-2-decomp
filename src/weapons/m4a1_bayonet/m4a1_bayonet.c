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

/// Pose-bank indices selected by the player clips' keyframe encodings.
enum {
    M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

/// Item-use translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetItemUseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_0346C_bank1.inc"
};

/// Item-use packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetItemUseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_0346C_bank4.inc"
};

/// Item-use keyframe durations in normal-rate frames, pose-word offsets and stop controls.
static const AnimationRecord _gM4a1BayonetItemUseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_0346C_records.inc"
};

/// Nineteen item-use track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetItemUseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_0346C_indices.inc"
};

/// Scripted item-use gesture in M4A1 Bayonet player bank slot 40.
///
/// All nineteen tracks stop at their last pose. Playback uses child tracks 1..18;
/// the read-only records, indices and poses must stay loaded while borrowed.
static AnimationSet _gM4a1BayonetItemUseAnimation = {
    .records           = _gM4a1BayonetItemUseAnimationRecords,
    .trackStartIndices = _gM4a1BayonetItemUseAnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetItemUseAnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetItemUseAnimationBank4,
    },
};

/// Stair-ascent translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetStairAscentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_03BC8_bank1.inc"
};

/// Stair-ascent packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetStairAscentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_03BC8_bank4.inc"
};

/// Stair-ascent keyframes, footstep cues and loop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1BayonetStairAscentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_03BC8_records.inc"
};

/// Nineteen stair-ascent track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetStairAscentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_03BC8_indices.inc"
};

/// Repeating scripted stair ascent in M4A1 Bayonet player bank slot 36.
///
/// Nineteen tracks loop; track 1's footstep cues gate the scripted strides.
/// The read-only records, indices and poses must stay loaded while borrowed.
static AnimationSet _gM4a1BayonetStairAscentAnimation = {
    .records           = _gM4a1BayonetStairAscentAnimationRecords,
    .trackStartIndices = _gM4a1BayonetStairAscentAnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetStairAscentAnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetStairAscentAnimationBank4,
    },
};

/// Stair-descent translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetStairDescentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_04350_bank1.inc"
};

/// Stair-descent packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetStairDescentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_04350_bank4.inc"
};

/// Stair-descent keyframes, footstep cues and loop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1BayonetStairDescentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_04350_records.inc"
};

/// Nineteen stair-descent track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetStairDescentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_04350_indices.inc"
};

/// Repeating scripted stair descent in M4A1 Bayonet player bank slot 37.
///
/// Nineteen tracks loop; track 1's footstep cues gate the scripted strides.
/// The read-only records, indices and poses must stay loaded while borrowed.
static AnimationSet _gM4a1BayonetStairDescentAnimation = {
    .records           = _gM4a1BayonetStairDescentAnimationRecords,
    .trackStartIndices = _gM4a1BayonetStairDescentAnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetStairDescentAnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetStairDescentAnimationBank4,
    },
};

/// Odd-flight ascent-finish translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetStairAscentFinishOddAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_04624_bank1.inc"
};

/// Odd-flight ascent-finish packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetStairAscentFinishOddAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_04624_bank4.inc"
};

/// Odd-flight ascent-finish keyframes, footstep cue and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1BayonetStairAscentFinishOddAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_04624_records.inc"
};

/// Nineteen odd-flight ascent-finish track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetStairAscentFinishOddAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_04624_indices.inc"
};

/// Final ascent stride for an odd-length stair flight in M4A1 Bayonet bank slot 39.
///
/// The initial stair count selects this stopping clip, with a three-frame blend.
/// Track 1 carries the final footstep cue; all nineteen tracks stop at their last pose.
/// The read-only records, indices and poses must stay loaded while borrowed.
static AnimationSet _gM4a1BayonetStairAscentFinishOddAnimation = {
    .records           = _gM4a1BayonetStairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gM4a1BayonetStairAscentFinishOddAnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetStairAscentFinishOddAnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetStairAscentFinishOddAnimationBank4,
    },
};

/// Even-flight ascent-finish translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetStairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_049B4_bank1.inc"
};

/// Even-flight ascent-finish packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetStairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_049B4_bank4.inc"
};

/// Even-flight ascent-finish keyframes, footstep cue and stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1BayonetStairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_049B4_records.inc"
};

/// Nineteen even-flight ascent-finish track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetStairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_049B4_indices.inc"
};

/// Final ascent stride for an even-length stair flight in M4A1 Bayonet bank slot 38.
///
/// The initial stair count selects this stopping clip, with a three-frame blend.
/// Track 1 carries the final footstep cue; all nineteen tracks stop at their last pose.
/// The read-only records, indices and poses must stay loaded while borrowed.
static AnimationSet _gM4a1BayonetStairAscentFinishEvenAnimation = {
    .records           = _gM4a1BayonetStairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gM4a1BayonetStairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetStairAscentFinishEvenAnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetStairAscentFinishEvenAnimationBank4,
    },
};

/// Family-0 PE release translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetParasiteEnergyReleaseFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_04E3C_bank1.inc"
};

/// Family-0 PE release packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetParasiteEnergyReleaseFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_04E3C_bank4.inc"
};

/// Family-0 PE release keyframe durations in normal-rate frames, pose-word offsets and stop controls.
static const AnimationRecord _gM4a1BayonetParasiteEnergyReleaseFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_04E3C_records.inc"
};

/// Nineteen family-0 PE release track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetParasiteEnergyReleaseFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_04E3C_indices.inc"
};

/// Parasite Energy release for clip family 0 in M4A1 Bayonet player bank slot 26.
///
/// Plays as the attachment countdown begins, before the charge and cast clips.
/// Family 0 also serves item attachments whose tens digit is 1. All nineteen
/// tracks stop at their last pose; the read-only records, indices and poses
/// must stay loaded while borrowed.
static AnimationSet _gM4a1BayonetParasiteEnergyReleaseFamily0Animation = {
    .records           = _gM4a1BayonetParasiteEnergyReleaseFamily0AnimationRecords,
    .trackStartIndices = _gM4a1BayonetParasiteEnergyReleaseFamily0AnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetParasiteEnergyReleaseFamily0AnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetParasiteEnergyReleaseFamily0AnimationBank4,
    },
};

/// Family-0 PE charge translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetParasiteEnergyChargeFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05038_bank1.inc"
};

/// Family-0 PE charge packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetParasiteEnergyChargeFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05038_bank4.inc"
};

/// Family-0 PE charge keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1BayonetParasiteEnergyChargeFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05038_records.inc"
};

/// Nineteen family-0 PE charge track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetParasiteEnergyChargeFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05038_indices.inc"
};

/// Parasite Energy charge loop for clip family 0 in M4A1 Bayonet player bank slot 27.
///
/// Runs after the family-0 release until the attachment countdown reaches zero
/// or the action is cancelled. Each of nineteen tracks repeats two one-frame
/// keyframes; the read-only records, indices and poses must stay loaded while borrowed.
static AnimationSet _gM4a1BayonetParasiteEnergyChargeFamily0Animation = {
    .records           = _gM4a1BayonetParasiteEnergyChargeFamily0AnimationRecords,
    .trackStartIndices = _gM4a1BayonetParasiteEnergyChargeFamily0AnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetParasiteEnergyChargeFamily0AnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetParasiteEnergyChargeFamily0AnimationBank4,
    },
};

/// Family-0 PE cast translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetParasiteEnergyCastFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05310_bank1.inc"
};

/// Family-0 PE cast packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetParasiteEnergyCastFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05310_bank4.inc"
};

/// Family-0 PE cast keyframe durations in normal-rate frames, pose-word offsets and stop controls.
static const AnimationRecord _gM4a1BayonetParasiteEnergyCastFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05310_records.inc"
};

/// Nineteen family-0 PE cast track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetParasiteEnergyCastFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05310_indices.inc"
};

/// Parasite Energy cast for clip family 0 in M4A1 Bayonet player bank slot 28.
///
/// Starts when the family-0 attachment countdown reaches zero. All nineteen
/// tracks stop at their final pose; the read-only records, indices and poses
/// must stay loaded while borrowed.
static AnimationSet _gM4a1BayonetParasiteEnergyCastFamily0Animation = {
    .records           = _gM4a1BayonetParasiteEnergyCastFamily0AnimationRecords,
    .trackStartIndices = _gM4a1BayonetParasiteEnergyCastFamily0AnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetParasiteEnergyCastFamily0AnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetParasiteEnergyCastFamily0AnimationBank4,
    },
};

/// Family-1 PE release translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetParasiteEnergyReleaseFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_055B4_bank1.inc"
};

/// Family-1 PE release packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetParasiteEnergyReleaseFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_055B4_bank4.inc"
};

/// Family-1 PE release keyframe durations in normal-rate frames, pose-word offsets and stop controls.
static const AnimationRecord _gM4a1BayonetParasiteEnergyReleaseFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_055B4_records.inc"
};

/// Nineteen family-1 PE release track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetParasiteEnergyReleaseFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_055B4_indices.inc"
};

/// Parasite Energy release for clip family 1 in M4A1 Bayonet player bank slot 29.
///
/// Begins the attachment countdown before the charge and cast clips. Family 1
/// serves spell IDs below 300 with a tens digit other than 3 and item IDs at
/// least 601 with a tens digit other than 1. All nineteen tracks stop at their
/// final pose; the read-only records, indices and poses must stay loaded while borrowed.
static AnimationSet _gM4a1BayonetParasiteEnergyReleaseFamily1Animation = {
    .records           = _gM4a1BayonetParasiteEnergyReleaseFamily1AnimationRecords,
    .trackStartIndices = _gM4a1BayonetParasiteEnergyReleaseFamily1AnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetParasiteEnergyReleaseFamily1AnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetParasiteEnergyReleaseFamily1AnimationBank4,
    },
};

/// Family-1 PE charge translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetParasiteEnergyChargeFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_057B4_bank1.inc"
};

/// Family-1 PE charge packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetParasiteEnergyChargeFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_057B4_bank4.inc"
};

/// Family-1 PE charge keyframes and loop/stop controls; durations count normal-rate frames.
static const AnimationRecord _gM4a1BayonetParasiteEnergyChargeFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_057B4_records.inc"
};

/// Nineteen family-1 PE charge track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetParasiteEnergyChargeFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_057B4_indices.inc"
};

/// Parasite Energy charge loop for clip family 1 in M4A1 Bayonet player bank slot 30.
///
/// Runs after the family-1 release until the attachment countdown reaches zero
/// or the action is cancelled. Each of nineteen tracks repeats two one-frame
/// keyframes; the read-only records, indices and poses must stay loaded while borrowed.
static AnimationSet _gM4a1BayonetParasiteEnergyChargeFamily1Animation = {
    .records           = _gM4a1BayonetParasiteEnergyChargeFamily1AnimationRecords,
    .trackStartIndices = _gM4a1BayonetParasiteEnergyChargeFamily1AnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetParasiteEnergyChargeFamily1AnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetParasiteEnergyChargeFamily1AnimationBank4,
    },
};

/// Family-1 PE cast translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetParasiteEnergyCastFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05B08_bank1.inc"
};

/// Family-1 PE cast packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetParasiteEnergyCastFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05B08_bank4.inc"
};

/// Family-1 PE cast keyframe durations in normal-rate frames, pose-word offsets and stop controls.
static const AnimationRecord _gM4a1BayonetParasiteEnergyCastFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05B08_records.inc"
};

/// Nineteen family-1 PE cast track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetParasiteEnergyCastFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05B08_indices.inc"
};

/// Parasite Energy cast for clip family 1 in M4A1 Bayonet player bank slot 31.
///
/// Starts when the family-1 attachment countdown reaches zero. All nineteen
/// tracks stop at their final pose; the read-only records, indices and poses
/// must stay loaded while borrowed.
static AnimationSet _gM4a1BayonetParasiteEnergyCastFamily1Animation = {
    .records           = _gM4a1BayonetParasiteEnergyCastFamily1AnimationRecords,
    .trackStartIndices = _gM4a1BayonetParasiteEnergyCastFamily1AnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetParasiteEnergyCastFamily1AnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetParasiteEnergyCastFamily1AnimationBank4,
    },
};

/// Part-4 hit reaction translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetPart4HitReactionAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05DBC_bank1.inc"
};

/// Part-4 hit reaction packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetPart4HitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05DBC_bank4.inc"
};

/// Part-4 hit reaction keyframe durations in normal-rate frames, pose-word offsets and stop controls.
static const AnimationRecord _gM4a1BayonetPart4HitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05DBC_records.inc"
};

/// Nineteen part-4 hit reaction track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetPart4HitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_05DBC_indices.inc"
};

/// Hit reaction for the model-part-4 collision sphere in M4A1 Bayonet player bank slot 16.
///
/// Hit region 1 selects this clip with a three-frame blend across child slots
/// 1..18. All nineteen tracks stop at their final pose; the read-only records,
/// indices and poses must stay loaded while borrowed.
static AnimationSet _gM4a1BayonetPart4HitReactionAnimation = {
    .records           = _gM4a1BayonetPart4HitReactionAnimationRecords,
    .trackStartIndices = _gM4a1BayonetPart4HitReactionAnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetPart4HitReactionAnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetPart4HitReactionAnimationBank4,
    },
};

/// Other-body hit reaction translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetOtherBodyHitReactionAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_bayonet_animation_060DC_bank1.inc"
};

/// Other-body hit reaction packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetOtherBodyHitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_060DC_bank4.inc"
};

/// Other-body hit reaction keyframe durations in normal-rate frames, pose-word offsets and stop controls.
static const AnimationRecord _gM4a1BayonetOtherBodyHitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_060DC_records.inc"
};

/// Nineteen other-body hit reaction track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetOtherBodyHitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_060DC_indices.inc"
};

/// Hit reaction for collision regions other than model part 4 in M4A1 Bayonet player bank slot 17.
///
/// Blends child slots 1..18 over three normal-rate frames, leaving the root
/// slot unchanged. All nineteen tracks stop at their final pose; the read-only
/// records, indices and word-aligned poses must stay loaded while borrowed.
static AnimationSet _gM4a1BayonetOtherBodyHitReactionAnimation = {
    .records           = _gM4a1BayonetOtherBodyHitReactionAnimationRecords,
    .trackStartIndices = _gM4a1BayonetOtherBodyHitReactionAnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetOtherBodyHitReactionAnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetOtherBodyHitReactionAnimationBank4,
    },
};

/// Death clip translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetDeathAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_bayonet_animation_068A0_bank1.inc"
};

/// Death clip packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetDeathAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_068A0_bank4.inc"
};

/// Death clip keyframe durations in normal-rate frames, pose-word offsets and stop controls.
static const AnimationRecord _gM4a1BayonetDeathAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_068A0_records.inc"
};

/// Nineteen death clip track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetDeathAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_068A0_indices.inc"
};

/// Player death clip in M4A1 Bayonet player bank slot 18.
///
/// Zero HP stops movement and turning and selects this clip on child slots
/// 1..18, resetting or blending for the requested duration. The root slot is
/// unchanged. All nineteen tracks stop at their final pose; the read-only
/// records, indices and word-aligned poses must stay loaded while borrowed.
static AnimationSet _gM4a1BayonetDeathAnimation = {
    .records           = _gM4a1BayonetDeathAnimationRecords,
    .trackStartIndices = _gM4a1BayonetDeathAnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetDeathAnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetDeathAnimationBank4,
    },
};

/// High-HP idle translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetIdleHighHpAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_bayonet_animation_07B38_bank1.inc"
};

/// High-HP idle packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetIdleHighHpAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_07B38_bank4.inc"
};

/// High-HP idle keyframes and loop/stop controls; durations count normal-rate frames and pose offsets count words.
static const AnimationRecord _gM4a1BayonetIdleHighHpAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_07B38_records.inc"
};

/// Nineteen high-HP idle track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetIdleHighHpAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_07B38_indices.inc"
};

/// Long-idle loop for HP above half maximum in M4A1 Bayonet player bank slot 23.
///
/// Selected after 300 direction-free locomotion ticks with a five-frame blend
/// on child slots 1..18. The half-maximum threshold truncates down; equality
/// selects the lower HP band. The root slot is unchanged. All nineteen tracks
/// loop; the read-only records, indices and word-aligned poses must stay loaded
/// while borrowed.
static AnimationSet _gM4a1BayonetIdleHighHpAnimation = {
    .records           = _gM4a1BayonetIdleHighHpAnimationRecords,
    .trackStartIndices = _gM4a1BayonetIdleHighHpAnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetIdleHighHpAnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetIdleHighHpAnimationBank4,
    },
};

/// Low-HP idle and paralysis translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetIdleLowHpAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_bayonet_animation_086B0_bank1.inc"
};

/// Low-HP idle and paralysis packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetIdleLowHpAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_086B0_bank4.inc"
};

/// Low-HP idle and paralysis keyframes and loop/stop controls; durations count normal-rate frames and pose offsets count words.
static const AnimationRecord _gM4a1BayonetIdleLowHpAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_086B0_records.inc"
};

/// Nineteen low-HP idle and paralysis track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetIdleLowHpAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_086B0_indices.inc"
};

/// Long-idle and paralysis loop in M4A1 Bayonet player bank slot 25.
///
/// HP at or below a quarter maximum selects this clip after 300 direction-free
/// locomotion ticks with a five-frame blend; the threshold truncates down.
/// Paralysis selects it regardless of HP with a six-frame blend. Both uses
/// play child slots 1..18 and leave the root slot unchanged. All nineteen tracks
/// loop; the read-only records, indices and word-aligned poses must stay loaded
/// while borrowed.
static AnimationSet _gM4a1BayonetIdleLowHpAnimation = {
    .records           = _gM4a1BayonetIdleLowHpAnimationRecords,
    .trackStartIndices = _gM4a1BayonetIdleLowHpAnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetIdleLowHpAnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetIdleLowHpAnimationBank4,
    },
};

/// Mid-HP idle translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetIdleMidHpAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_bayonet_animation_08DCC_bank1.inc"
};

/// Mid-HP idle packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetIdleMidHpAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_08DCC_bank4.inc"
};

/// Mid-HP idle keyframes and loop/stop controls; durations count normal-rate frames and pose offsets count words.
static const AnimationRecord _gM4a1BayonetIdleMidHpAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_08DCC_records.inc"
};

/// Nineteen mid-HP idle track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetIdleMidHpAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_08DCC_indices.inc"
};

/// Long-idle loop for HP above a quarter and at most half maximum in M4A1 Bayonet player bank slot 24.
///
/// Selected after 300 direction-free locomotion ticks with a five-frame blend
/// on child slots 1..18. Both maximum-HP thresholds truncate down. The root slot
/// is unchanged. All nineteen tracks loop; the read-only records, indices and
/// word-aligned poses must stay loaded while borrowed.
static AnimationSet _gM4a1BayonetIdleMidHpAnimation = {
    .records           = _gM4a1BayonetIdleMidHpAnimationRecords,
    .trackStartIndices = _gM4a1BayonetIdleMidHpAnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetIdleMidHpAnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetIdleMidHpAnimationBank4,
    },
};

/// Scripted presentation entry translation and Euler poses, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1BayonetScriptedPresentationEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_bayonet_animation_0924C_bank1.inc"
};

/// Scripted presentation entry packed Euler poses, with angle steps of 1/512 turn.
static const AnimationPackedRotation _gM4a1BayonetScriptedPresentationEntryAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_0924C_bank4.inc"
};

/// Scripted presentation entry keyframe durations in normal-rate frames, pose-word offsets and stop controls.
static const AnimationRecord _gM4a1BayonetScriptedPresentationEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_0924C_records.inc"
};

/// Nineteen scripted presentation entry track starts as absolute record indices, plus the retained export halfword.
static const u16 _gM4a1BayonetScriptedPresentationEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_bayonet_animation_0924C_indices.inc"
};

/// Scripted presentation entry clip in M4A1 Bayonet player bank slots 32 and 34.
///
/// A zero presentation variant selects slot 32, played on child slots 1..18
/// before a weapon CAP event or room command; slot 34 holds the same descriptor.
/// The root slot is unchanged. All nineteen tracks stop at their final pose;
/// the read-only records, indices and word-aligned poses must stay loaded while
/// borrowed. The specific visual gesture is unproven.
static AnimationSet _gM4a1BayonetScriptedPresentationEntryAnimation = {
    .records           = _gM4a1BayonetScriptedPresentationEntryAnimationRecords,
    .trackStartIndices = _gM4a1BayonetScriptedPresentationEntryAnimationIndices,
    .poseBanks         = {
        [M4A1_BAYONET_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1BayonetScriptedPresentationEntryAnimationBank1,
        [M4A1_BAYONET_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1BayonetScriptedPresentationEntryAnimationBank4,
    },
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
    &_gM4a1BayonetPart4HitReactionAnimation,
    &_gM4a1BayonetOtherBodyHitReactionAnimation,
    &_gM4a1BayonetDeathAnimation,
    &_gM4a1BayonetUnarmedWalkAnimation,
    &_gM4a1BayonetAnimation0C3CC,
    &_gM4a1BayonetIdleAnimation,
    &_gM4a1BayonetIdleAnimation,
    &_gM4a1BayonetIdleHighHpAnimation,
    &_gM4a1BayonetIdleMidHpAnimation,
    &_gM4a1BayonetIdleLowHpAnimation,
    &_gM4a1BayonetParasiteEnergyReleaseFamily0Animation,
    &_gM4a1BayonetParasiteEnergyChargeFamily0Animation,
    &_gM4a1BayonetParasiteEnergyCastFamily0Animation,
    &_gM4a1BayonetParasiteEnergyReleaseFamily1Animation,
    &_gM4a1BayonetParasiteEnergyChargeFamily1Animation,
    &_gM4a1BayonetParasiteEnergyCastFamily1Animation,
    &_gM4a1BayonetScriptedPresentationEntryAnimation,
    &_gM4a1BayonetAnimation09424,
    &_gM4a1BayonetScriptedPresentationEntryAnimation,
    &_gM4a1BayonetAnimation09424,
    &_gM4a1BayonetStairAscentAnimation,
    &_gM4a1BayonetStairDescentAnimation,
    &_gM4a1BayonetStairAscentFinishEvenAnimation,
    &_gM4a1BayonetStairAscentFinishOddAnimation,
    &_gM4a1BayonetItemUseAnimation,
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
