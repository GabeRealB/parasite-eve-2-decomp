#include "planar_reflection.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>

#include "common.h"
#include "gte.h"
#include "types.h"

#include "gameplay/actor_render.h"
#include "gameplay/model_objects.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/room_common.h"

/// Scratch block for rebuilding a planar reflection's coordinate frame.
///
/// Reserved on the scratch stack when the view changes, and released before
/// the player's light matrices are copied. A floor mirror uses only
/// `viewYRow`. Any other mirror reflects the view through a plane.
/// `normal` and `planePoint` describe that plane, `refAxis` is the world axis
/// least aligned with the normal, `basis` is the orthonormal frame built from
/// the two, and `reflect` is the reflection matrix copied into the mirror's
/// coordinate frame. That frame's translation is the view translation plus
/// the shift that makes `reflect` fix `planePoint`.
typedef struct {
    SVECTOR viewYRow;      // View matrix Y row, copied and then negated in place (4.12)
    SVECTOR refAxis;       // Unit world axis least aligned with `normal` (4.12; 0x1000 = 1)
    byte    unknown_10[8]; // No recovered access; role unproven
    MATRIX  basis;         // Orthonormal frame whose third column is the plane normal
    MATRIX  reflect;       // Reflection through that plane; only the rotation is used
    SVECTOR normal;        // Plane normal, normalized in place to 4.12
    SVECTOR planePoint;    // Point on the plane, in view-translation units; then rotated by `reflect`
    s16     leastAbs;      // Smallest absolute component of `normal` so far (4.12)
    s16     leastAxis;     // Which component `leastAbs` came from (0 X, 1 Y, 2 Z)
    s16     axisAbs;       // Absolute value of the component being compared (4.12)
    byte    unknown_6E[2]; // No recovered access; role unproven
} _PlanarReflectionFrameScratch;
STATIC_ASSERT_SIZEOF(_PlanarReflectionFrameScratch, 0x70);

/// Player-reflection phases, spawnArg1 modes and shared render settings.
enum {
    PLANAR_REFLECTION_PLAYER_STATE_INIT   = 0,
    PLANAR_REFLECTION_PLAYER_STATE_UPDATE = 1,
    PLANAR_REFLECTION_MODE_FLOOR          = 0,
    PLANAR_REFLECTION_MODE_PLANE          = 1,
    PLANAR_REFLECTION_MODE_COUNT          = 2,
    PLANAR_REFLECTION_COPY_IDLE           = 0,
    PLANAR_REFLECTION_COPY_REQUESTED      = 1,
    PLANAR_REFLECTION_PLAYER_OT_OFFSET    = 31 // Ordering-table entries
};

static void _planarReflectionUpdatePlayer(Task* reflectionTask);

#ifndef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION
#error "Define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION as 0 or 1"
#elif PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION != 0 && PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION != 1
#error "PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION must be 0 or 1"
#elif PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION
#include "planar_reflection_rodata.inc.c"
#endif

/// Creates a player-model reflection and its attachment reflections, then updates it immediately.
///
/// Requires a live player task with a TMD body. The reflection starts bodyless
/// in state 0; spawnArg1.value selects floor mode (0) or room-plane mode (1).
/// Other values, model-attachment failure or work-allocation failure kill it.
/// It owns the cloned body and primary-heap `RoomMirrorWork`, borrows the
/// player's geometry, and becomes a child of the player for teardown.
/// Attachment reflections are children of their live source tasks and borrow
/// this reflection's frame and lighting, so those sources must not outlive it.
/// Success advances to state 1 and calls `_planarReflectionUpdatePlayer`.
static void _planarReflectionInitPlayer(Task* reflectionTask)
{
    enum {
        PLANAR_REFLECTION_PLAYER_TEXTURE_PAGE_OFFSET = 6, // Encoded texture-page displacement, not pixels
        PLANAR_REFLECTION_CACHE_UNSET                = -1 // Forces the first view and weapon refresh
    };
    Task*           playerTask;
    GameActor*      playerActor;
    TmdObject*      reflectionModel;
    GfxCoord*       reflectionRoot;
    RoomMirrorWork* mirrorWork;
    Task*           sourceAttachment;
    Task*           attachmentReflection;
    s32             attachmentIndex;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (modelObjectAttachTmd(reflectionTask, playerTask->extra.tmd->source) == NULL) {
        taskKill(reflectionTask);
        return;
    }
    reflectionModel = reflectionTask->extra.tmd;
    reflectionRoot  = reflectionModel->coords;
    if ((u32)reflectionTask->spawnArg1.value >= (u32)PLANAR_REFLECTION_MODE_COUNT) {
        taskKill(reflectionTask);
        return;
    }
    mirrorWork = memCalloc(sizeof(RoomMirrorWork), 0);
    if (mirrorWork == NULL) {
        taskKill(reflectionTask);
        return;
    }
    reflectionTask->work = mirrorWork;
    // Refresh both packet halves after moving the clone's encoded texture pages.
    reflectionModel->texturePageOffset = PLANAR_REFLECTION_PLAYER_TEXTURE_PAGE_OFFSET;
    tmdBuildBufferHalf(reflectionModel);
    tmdBuildBufferHalf(reflectionModel);
    reflectionModel->flags    = TMD_OBJECT_REVERSE_CULLING;
    reflectionModel->otOffset = PLANAR_REFLECTION_PLAYER_OT_OFFSET;
    if (reflectionTask->spawnArg1.value == PLANAR_REFLECTION_MODE_FLOOR) {
        gGameSession->field_4E = 1;
    }
    reflectionRoot->parent    = &mirrorWork->coord;
    reflectionModel->lightMtx = &mirrorWork->light;
    reflectionModel->colorMtx = &mirrorWork->color;
    taskReparent(playerTask, reflectionTask);
    reflectionTask->state++;
    mirrorWork->viewRebuildStamp = gGfxViewCoord.composeStamp & GRAPHICS_COORD_STAMP_MASK;
    mirrorWork->copyPending      = PLANAR_REFLECTION_COPY_REQUESTED;
    mirrorWork->equippedWeapon   = PLANAR_REFLECTION_CACHE_UNSET;
    reflectionModel->flags      |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    mirrorWork->copyPending      = PLANAR_REFLECTION_COPY_IDLE;
    mirrorWork->viewRebuildStamp = PLANAR_REFLECTION_CACHE_UNSET;
    playerActor                  = playerTask->work;
    for (attachmentIndex = 0; attachmentIndex < ARRAY_SIZE(playerActor->attachmentTasks); attachmentIndex++) {
        sourceAttachment = playerActor->attachmentTasks[attachmentIndex];
        if (sourceAttachment != NULL) {
            attachmentReflection = taskSpawnFromTable(_planarReflectionGetTaskTable(), PLANAR_REFLECTION_TASK_ATTACHMENT, attachmentIndex, reflectionTask);
            if (attachmentReflection != NULL) {
                taskReparent(sourceAttachment, attachmentReflection);
            }
        }
    }
    _planarReflectionUpdatePlayer(reflectionTask);
}

/// Scratch-stack block for where a planar reflection lands on screen.
///
/// The mirror task in Acropolis and in the Shelter/Neo Ark stage reserves one
/// block, projects two samples through one reflected part, and releases the
/// block before copying the player's light matrices. While a scripted event
/// is running the samples are on part 1, 1000 game units either side of its
/// origin. Otherwise they are on the root part: 2000 units along negative
/// local Y, and the origin. Model space is Y-down, so the negative-Y sample
/// is the upper end of an upright part; it is stored as the head and the
/// other sample as the foot.
///
/// `point` is reused for both projections. Each projection overwrites
/// `depthCue` and `projectionFlags`, and the mirror never reads them. Screen
/// Y is then ordered so `screenFoot` is the upper edge and `screenHead` the
/// lower, and each edge is padded by 16 pixels. The swap uses `screenHead.vx`
/// as its temporary. The quad is centred on the foot sample's screen X. Its
/// half-width is half the padded vertical span, clamped to 95 pixels, and
/// `left` through `bottom` is that rectangle clamped to the 320 by 240 frame.
/// The quads are drawn when the rectangle overlaps the mirror's clip
/// rectangle; a shared edge does not count. Otherwise the reflected model is
/// hidden for the frame.
///
/// `orderingDepthFoot` is the quarter-depth the quads are ordered at. In area
/// 2, view 5, a mirror other than index 0 stores the head sample's depth plus
/// 10 there instead. `texturePageX` is the 64-pixel-aligned VRAM X of the
/// 16-bit page those quads sample the off-screen frame copy from.
typedef struct {
    SVECTOR point;             // Sample in the part's local space; X and Z are 0, and the unused halfword is left unchanged
    s32     depthCue;          // GTE IR0 of the latest projection, with 12 fractional bits; written and never read
    s32     projectionFlags;   // GTE FLAG of the latest projection; written and never tested
    s32     orderingDepthFoot; // Foot sample's SZ3 / 4, or the head depth plus 10; the quads' ordering-table depth
    s32     orderingDepthHead; // Head sample's SZ3 / 4; read only to replace the foot depth
    DVECTOR screenFoot;        // Foot sample's screen pixels; vy is ordered to the upper edge, vx centres the quad
    DVECTOR screenHead;        // Head sample's screen pixels; vy is ordered to the lower edge, vx is the swap temporary
    u16     texturePageX;      // 64-pixel-aligned VRAM X of the 16-bit page the quads sample. Two alignment bytes follow
    s32     left;              // Quad left edge, pixels from screen centre, clamped to -160
    s32     right;             // Quad right edge, pixels from screen centre, clamped to 160
    s32     top;               // Quad top edge, pixels from screen centre, clamped to -120
    s32     bottom;            // Quad bottom edge, pixels from screen centre, clamped to 120
} _PlanarReflectionExtentScratch;
STATIC_ASSERT_SIZEOF(_PlanarReflectionExtentScratch, 0x34);

/// Sets the Q12 basis hint to the positive coordinate axis least aligned with the plane normal.
///
/// Borrows a live, word-aligned `frameScratch`. The normal's xyz may use any
/// common scale, but must lie in -32767..32767 so their magnitudes fit the
/// signed-halfword scratch fields. The hint uses the normal's coordinate frame
/// and `ONE` (4096) for 1.0. Equal magnitudes prefer X, then Y, then Z; a zero
/// normal selects X, though it cannot define a plane basis.
///
/// Writes `refAxis` xyz, the minimum magnitude to `leastAbs`, its axis index
/// (0 X, 1 Y, 2 Z) to `leastAxis`, and the absolute Z component to `axisAbs`.
/// Preserves the normal, `refAxis.pad` and the rest of the block. Reserves no
/// scratch storage, changes no GTE state and retains no pointer.
static inline void _planarReflectionChooseReferenceAxis(_PlanarReflectionFrameScratch* frameScratch)
{
    enum {
        PLANAR_REFLECTION_AXIS_X = 0,
        PLANAR_REFLECTION_AXIS_Y = 1,
        PLANAR_REFLECTION_AXIS_Z = 2
    };

    // Keep the first minimum so ties have a stable axis order.
    frameScratch->leastAbs = frameScratch->normal.vx;
    if (frameScratch->leastAbs < 0) {
        frameScratch->leastAbs = -frameScratch->leastAbs;
    }
    frameScratch->leastAxis = PLANAR_REFLECTION_AXIS_X;
    frameScratch->axisAbs   = frameScratch->normal.vy;
    if (frameScratch->axisAbs < 0) {
        frameScratch->axisAbs = -frameScratch->axisAbs;
    }
    if (frameScratch->leastAbs > frameScratch->axisAbs) {
        frameScratch->leastAbs  = frameScratch->axisAbs;
        frameScratch->leastAxis = PLANAR_REFLECTION_AXIS_Y;
    }
    frameScratch->axisAbs = frameScratch->normal.vz;
    if (frameScratch->axisAbs < 0) {
        frameScratch->axisAbs = -frameScratch->axisAbs;
    }
    if (frameScratch->leastAbs > frameScratch->axisAbs) {
        frameScratch->leastAbs  = frameScratch->axisAbs;
        frameScratch->leastAxis = PLANAR_REFLECTION_AXIS_Z;
    }
    // The least-aligned axis gives the basis builder the largest cross product.
    frameScratch->refAxis.vx = 0;
    if (frameScratch->leastAxis == PLANAR_REFLECTION_AXIS_X) {
        frameScratch->refAxis.vx = ONE;
    }
    frameScratch->refAxis.vy = 0;
    if (frameScratch->leastAxis == PLANAR_REFLECTION_AXIS_Y) {
        frameScratch->refAxis.vy = ONE;
    }
    frameScratch->refAxis.vz = 0;
    if (frameScratch->leastAxis == PLANAR_REFLECTION_AXIS_Z) {
        frameScratch->refAxis.vz = ONE;
    }
}

/// Builds a plane-reflection transform with the current view translation.
///
/// `frameScratch->normal` must be nonzero and normalized to Q12 (`ONE` = 1.0).
/// It and `frameScratch->planePoint` use the view parent's coordinate frame; the point and
/// translation use integer game coordinates. The output rotation is
/// B * diag(1, 1, -1) * transpose(B), where B's Z axis is the plane normal.
/// Translation is view.t + point - reflectedPoint. The reflected point is
/// rounded down and saturated to signed halfwords by the GTE.
///
/// Borrows live, word-aligned work and scratch blocks, disjoint from each other
/// and the view matrix. Overwrites the scratch axis-selection
/// fields, reference axis, basis, reflection rotation and plane-point xyz;
/// preserves the normal. Requires an initialized scratch stack with another
/// sizeof(MATRIX) bytes free for the basis calculation. Clobbers GTE arithmetic
/// and rotation state; retains no pointers. The caller manages the containing
/// coordinate's parent and composition stamp. Only `mirrorWork->coord.coord`
/// is written in the work block.
static inline void _planarReflectionBuildPlaneFrame(RoomMirrorWork* mirrorWork, _PlanarReflectionFrameScratch* frameScratch)
{
    _planarReflectionChooseReferenceAxis(frameScratch);

    // Reflect the normal-aligned Z axis, then return to the view parent's axes.
    gfxBuildOrthonormalBasis(&frameScratch->basis, &frameScratch->normal, &frameScratch->refAxis);
    gte_TransposeMatrix(&frameScratch->basis, &frameScratch->reflect);
    frameScratch->reflect.m[2][0] = -frameScratch->reflect.m[2][0];
    frameScratch->reflect.m[2][1] = -frameScratch->reflect.m[2][1];
    frameScratch->reflect.m[2][2] = -frameScratch->reflect.m[2][2];
    gte_MulMatrix0(&frameScratch->basis, &frameScratch->reflect, &frameScratch->reflect);
    // Offset the reflection to keep the plane fixed, then add the view translation.
    mirrorWork->coord.coord      = frameScratch->reflect;
    mirrorWork->coord.coord.t[0] = gGfxViewCoord.coord.t[0] + frameScratch->planePoint.vx;
    mirrorWork->coord.coord.t[1] = gGfxViewCoord.coord.t[1] + frameScratch->planePoint.vy;
    mirrorWork->coord.coord.t[2] = gGfxViewCoord.coord.t[2] + frameScratch->planePoint.vz;
    _gfxRotateSv(&frameScratch->reflect, &frameScratch->planePoint);
    mirrorWork->coord.coord.t[0] -= frameScratch->planePoint.vx;
    mirrorWork->coord.coord.t[1] -= frameScratch->planePoint.vy;
    mirrorWork->coord.coord.t[2] -= frameScratch->planePoint.vz;
}

/// Updates the player's reflected frame, visible pose, lighting and framebuffer compositing.
///
/// Requires a successfully initialized reflection and a live player with the
/// same borrowed TMD source. Weapon changes create equipment reflections with
/// spawnArg1 selectors 2/3; the source equipment tasks own their teardown.
/// A changed view stamp rebuilds the floor or room-plane frame and visibility.
/// Floor mode can queue a 320x240, 16-bit framebuffer copy in that same call;
/// it remains pending while the view or a display-mode request blocks copying.
/// The copy starts at VRAM (320, 256) in Shelter/Neo Ark, otherwise (448, 256).
/// In Acropolis and Shelter/Neo Ark, the screen extent gates drawing and blend quads.
/// Pose copying pauses with scene updates; lighting follows the player every
/// call. The shared source must have at least two parts, and attachment users
/// require parts 8 and 12. GPU packet storage and the current OT are borrowed
/// for the frame. Blend packets use wrapped depth indices 0..1023 plus 16,
/// requiring the normal OT's reserved tail through base-relative index 1039.
static void _planarReflectionUpdatePlayer(Task* reflectionTask)
{
    enum {
        PLANAR_REFLECTION_CAPTURE_X                = 448,
        PLANAR_REFLECTION_NEO_ARK_CAPTURE_X        = 320,
        PLANAR_REFLECTION_CAPTURE_Y                = 256,
        PLANAR_REFLECTION_SCREEN_WIDTH             = 320,
        PLANAR_REFLECTION_SCREEN_HEIGHT            = 240,
        PLANAR_REFLECTION_SCREEN_HALF_WIDTH        = 160,
        PLANAR_REFLECTION_SCREEN_HALF_HEIGHT       = 120,
        PLANAR_REFLECTION_FRAMEBUFFER_Y_STRIDE     = 272,
        PLANAR_REFLECTION_FRAMEBUFFER_PAGE_Y_SHIFT = 8, // Buffer 1's page begins at VRAM Y = 256
        PLANAR_REFLECTION_FRAMEBUFFER_V_SHIFT      = 4, // Its framebuffer begins 16 pixels into that page
        PLANAR_REFLECTION_TEXTURE_16_BIT           = 2,
        PLANAR_REFLECTION_COPY_RIGHT_PAGE_X        = 128,
        PLANAR_REFLECTION_COPY_RIGHT_U             = 32,  // Right half begins at source X = 160
        PLANAR_REFLECTION_CAPTURE_CLEAR_COLOR      = 2,
        PLANAR_REFLECTION_NEUTRAL_TEXTURE_COLOR    = 128, // RGB modulation factor 1.0
        PLANAR_REFLECTION_FORCE_MASK_BIT           = 1,
        PLANAR_REFLECTION_NORMAL_MASK_BIT          = 0,
        PLANAR_REFLECTION_COPY_OT_INDEX            = 1023,
        PLANAR_REFLECTION_OT_DEPTH_MASK            = 0x3FFF, // Shifted quarter-depth window before the four-bit reduction
        PLANAR_REFLECTION_BLEND_DEPTH_BIAS         = 15,     // OT entries subtracted from the model's offset
        PLANAR_REFLECTION_MAX_QUAD_HALF_WIDTH      = 95,
        PLANAR_REFLECTION_FIRST_EQUIPMENT_SLOT     = 2,
        PLANAR_REFLECTION_NEO_ARK_FLOOR_Y_OFFSET   = 155, // Game coordinate units
        PLANAR_REFLECTION_ACROPOLIS_FLOOR_Y_OFFSET = 105,
        PLANAR_REFLECTION_SQUARE_NO_COPY_VIEW      = 15
    };
    RoomMirrorWork*                 mirrorWork;
    PlayerStatus*                   playerStatus;
    TmdObject*                      reflectionModel;
    Task*                           playerTask;
    GameActor*                      playerActor;
    Task*                           sourceEquipment;
    Task*                           equipmentReflection;
    _PlanarReflectionFrameScratch*  frame;
    _PlanarReflectionExtentScratch* extent;
    GfxCoord*                       reflectionParts;
    GfxCoord*                       eventSamplePart;
    DR_AREA*                        drawArea;
    DR_STP*                         maskBitMode;
    DR_OFFSET*                      drawOffset;
    SPRT*                           copySprite;
    DR_TPAGE*                       copyTexturePage;
    TILE*                           captureClear;
    POLY_FT4*                       blendQuad;
    s32                             stage;
    s32                             area;
    s32                             view;
    s32                             captureX;
    s32                             viewRebuildStamp;
    s32                             copyPending;
    s32                             halfWidth;
    s32                             textureX;
    s32                             equipmentIndex;
    s32                             blendMode;
    u32                             partIndex;

    captureX        = PLANAR_REFLECTION_CAPTURE_X;
    mirrorWork      = reflectionTask->work;
    reflectionModel = reflectionTask->extra.tmd;
    stage           = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage;
    area            = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area;
    view            = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
    playerStatus    = &gPlayerStatus;
    if (stage == GAME_STAGE_SHELTER_NEO_ARK) {
        captureX = PLANAR_REFLECTION_NEO_ARK_CAPTURE_X;
    }
    if (mirrorWork->equippedWeapon != playerStatus->weapon) {
        // Equipment replacements keep their reflections under the live source tasks.
        playerActor                = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
        mirrorWork->equippedWeapon = playerStatus->weapon;
        for (equipmentIndex = 0; equipmentIndex < ARRAY_SIZE(playerActor->equipmentTasks); equipmentIndex++) {
            sourceEquipment = playerActor->equipmentTasks[equipmentIndex];
            if (sourceEquipment != NULL) {
                equipmentReflection = taskSpawnFromTable(_planarReflectionGetTaskTable(), PLANAR_REFLECTION_TASK_ATTACHMENT,
                                                         equipmentIndex + PLANAR_REFLECTION_FIRST_EQUIPMENT_SLOT, reflectionTask);
                if (equipmentReflection != NULL) {
                    taskReparent(sourceEquipment, equipmentReflection);
                }
            }
        }
    }
    reflectionModel->flags |= TMD_OBJECT_REVERSE_CULLING;
    viewRebuildStamp        = gGfxViewCoord.composeStamp & GRAPHICS_COORD_STAMP_MASK;
    if (mirrorWork->viewRebuildStamp != viewRebuildStamp) {
        GfxCoord* viewParent;

        // Rebuild only when the view changes; the cached flags survive per-frame clipping.
        mirrorWork->viewRebuildStamp   = viewRebuildStamp;
        viewParent                     = gGfxViewCoord.parent;
        mirrorWork->clipLeft           = -PLANAR_REFLECTION_SCREEN_HALF_WIDTH;
        mirrorWork->clipRight          = PLANAR_REFLECTION_SCREEN_HALF_WIDTH;
        mirrorWork->coord.composeStamp = GRAPHICS_COORD_DIRTY;
        mirrorWork->clipTop            = -PLANAR_REFLECTION_SCREEN_HALF_HEIGHT;
        mirrorWork->clipBottom         = PLANAR_REFLECTION_SCREEN_HALF_HEIGHT;
        frame                          = SCRATCH_STACK_RESERVE_BLOCK(_PlanarReflectionFrameScratch);
        mirrorWork->coord.parent       = viewParent;
        if (reflectionTask->spawnArg1.value == PLANAR_REFLECTION_MODE_FLOOR) {
            mirrorWork->copyPending = PLANAR_REFLECTION_COPY_REQUESTED;
            mirrorWork->coord.coord = gGfxViewCoord.coord;
            frame->viewYRow.vx      = mirrorWork->coord.coord.m[1][0];
            frame->viewYRow.vy      = mirrorWork->coord.coord.m[1][1];
            frame->viewYRow.vz      = mirrorWork->coord.coord.m[1][2];
            gte_lddp(-ONE);
            gte_ldsv(&frame->viewYRow);
            gte_gpf12();
            gte_stsv(&frame->viewYRow);
            mirrorWork->coord.coord.m[1][0] = frame->viewYRow.vx;
            mirrorWork->coord.coord.m[1][1] = frame->viewYRow.vy;
            mirrorWork->coord.coord.m[1][2] = frame->viewYRow.vz;
            if (stage == GAME_STAGE_SHELTER_NEO_ARK) {
                if (area == GAME_AREA_NEO_ARK_OBSERVATORY) {
                    if (view >= 6 && view < 12 && gGameSession->location.loc.room == 2) {
                        mirrorWork->coord.coord.t[1] += PLANAR_REFLECTION_NEO_ARK_FLOOR_Y_OFFSET;
                        reflectionModel->flags       &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
                        mirrorWork->firstBlendMode    = GPU_BLEND_AVERAGE;
                    } else {
                        mirrorWork->copyPending = PLANAR_REFLECTION_COPY_IDLE;
                        reflectionModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                }
            } else if (area == GAME_AREA_ACROPOLIS_SQUARE) {
                reflectionModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                if (view == 9) {
                    mirrorWork->copyPending = PLANAR_REFLECTION_COPY_IDLE;
                }
            } else {
                if (area != GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL) {
                    mirrorWork->coord.coord.t[1] += PLANAR_REFLECTION_ACROPOLIS_FLOOR_Y_OFFSET;
                }
                mirrorWork->firstBlendMode = GPU_BLEND_ADD;
                if ((area == GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL && view == 5) || (area == GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL && (view == 7 || view == 5))) {
                    reflectionModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                } else {
                    reflectionModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
                }
            }
        } else {
            TmdObject* planeModel = reflectionTask->extra.tmd;
            planeModel->flags    &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            if (stage == GAME_STAGE_ACROPOLIS) {
                switch (area) {
                    case GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL:
                        switch (view) {
                            case 2:
                                frame->normal.vx = -ONE;
                                frame->normal.vy = 0;
                                frame->normal.vz = 0;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = -0x1518;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = 0;
                                break;
                            case 3:
                                frame->normal.vx = 0x64;
                                frame->normal.vy = 0;
                                frame->normal.vz = -0x384;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = 0;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = -0x640;
                                break;
                            case 4:
                                mirrorWork->clipLeft  = -0x14;
                                mirrorWork->clipRight = 0x14;
                                frame->normal.vx      = -ONE;
                                frame->normal.vy      = 0;
                                frame->normal.vz      = 0;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = 0x1644;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = 0;
                                break;
                            default:
                                planeModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                                break;
                        }
                        break;
                    case GAME_AREA_ACROPOLIS_SQUARE:
                        switch (view) {
                            case 6:
                                mirrorWork->clipRight = 0x64;
                                mirrorWork->clipLeft  = 0;
                                frame->normal.vx      = -ONE;
                                frame->normal.vy      = 0;
                                frame->normal.vz      = 0;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = 0x1AF4;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = 0;
                                planeModel->otOffset = PLANAR_REFLECTION_PLAYER_OT_OFFSET;
                                break;
                            case 7:
                            case 8:
                                frame->normal.vx = 0;
                                frame->normal.vy = 0;
                                frame->normal.vz = ONE;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = 0;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = 0x14B4;
                                break;
                            default:
                                planeModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                                break;
                        }
                        break;
                    case GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL:
                        switch (view) {
                            case 2:
                            case 5:
                                frame->normal.vx = -ONE;
                                frame->normal.vy = 0;
                                frame->normal.vz = 0;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = 0x170C;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = 0;
                                break;
                            case 4:
                                frame->normal.vx = -ONE;
                                frame->normal.vy = 0;
                                frame->normal.vz = 0;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = -0x1644;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = 0;
                                break;
                            case 3:
                                frame->normal.vx = -0x64;
                                frame->normal.vy = 0;
                                frame->normal.vz = -0x384;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = 0;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = -0x640;
                                break;
                            default:
                                planeModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                                break;
                        }
                        break;
                    default:
                        planeModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                        break;
                }
            } else if (view == 8 || view == 1) {
                frame->normal.vx = -ONE;
                frame->normal.vy = 0;
                frame->normal.vz = 0;
                VectorNormalSS(&frame->normal, &frame->normal);
                frame->planePoint.vx = 0xA38;
                frame->planePoint.vy = 0;
                frame->planePoint.vz = 0;
            } else {
                planeModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            if (!(planeModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
                _planarReflectionBuildPlaneFrame(mirrorWork, frame);
                mirrorWork->firstBlendMode = GPU_BLEND_ADD;
            }
        }
        mirrorWork->objectFlags = reflectionModel->flags;
        SCRATCH_STACK_RELEASE_BLOCK(_PlanarReflectionFrameScratch);
    }

    copyPending = mirrorWork->copyPending;
    if (copyPending == PLANAR_REFLECTION_COPY_REQUESTED && reflectionTask->spawnArg1.value == PLANAR_REFLECTION_MODE_FLOOR && !(area == GAME_AREA_ACROPOLIS_SQUARE && view == PLANAR_REFLECTION_SQUARE_NO_COPY_VIEW) && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
        u16  drawOrigin[2];
        RECT drawRect;

        // OT insertion reverses this sequence: capture off-screen with bit 15 set,
        // then restore the framebuffer draw area and origin. Each sprite copies 160 pixels.
        mirrorWork->copyPending = PLANAR_REFLECTION_COPY_IDLE;
        drawArea                = gGpuPrimCursor;
        gGpuPrimCursor          = (u8*)gGpuPrimCursor + sizeof(DR_AREA);
        drawRect.x              = 0;
        drawRect.y              = gDisplayState.drawBuffer * PLANAR_REFLECTION_FRAMEBUFFER_Y_STRIDE;
        drawRect.w              = PLANAR_REFLECTION_SCREEN_WIDTH;
        drawRect.h              = PLANAR_REFLECTION_SCREEN_HEIGHT;
        SetDrawArea(drawArea, &drawRect);
        addPrim(&gGpuCurrentOt[PLANAR_REFLECTION_COPY_OT_INDEX], drawArea);

        maskBitMode    = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_STP);
        SetDrawStp(maskBitMode, PLANAR_REFLECTION_NORMAL_MASK_BIT);
        addPrim(&gGpuCurrentOt[PLANAR_REFLECTION_COPY_OT_INDEX], maskBitMode);

        drawOffset     = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_OFFSET);
        drawOrigin[0]  = PLANAR_REFLECTION_SCREEN_HALF_WIDTH;
        drawOrigin[1]  = gDisplayState.drawBuffer * PLANAR_REFLECTION_FRAMEBUFFER_Y_STRIDE + PLANAR_REFLECTION_SCREEN_HALF_HEIGHT;
        SetDrawOffset(drawOffset, drawOrigin);
        addPrim(&gGpuCurrentOt[PLANAR_REFLECTION_COPY_OT_INDEX], drawOffset);

        copySprite     = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(SPRT);
        copySprite->x0 = -PLANAR_REFLECTION_SCREEN_HALF_WIDTH;
        copySprite->y0 = -PLANAR_REFLECTION_SCREEN_HALF_HEIGHT;
        copySprite->w  = PLANAR_REFLECTION_SCREEN_HALF_WIDTH;
        copySprite->h  = PLANAR_REFLECTION_SCREEN_HEIGHT;
        copySprite->u0 = 0;
        copySprite->v0 = gDisplayState.drawBuffer << PLANAR_REFLECTION_FRAMEBUFFER_V_SHIFT;
        setSprt(copySprite);
        setShadeTex(copySprite, 1);
        addPrim(&gGpuCurrentOt[PLANAR_REFLECTION_COPY_OT_INDEX], copySprite);

        copyTexturePage = gGpuPrimCursor;
        gGpuPrimCursor  = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
        setDrawTPage(copyTexturePage, 1, 1, getTPage(PLANAR_REFLECTION_TEXTURE_16_BIT, GPU_BLEND_AVERAGE, 0, gDisplayState.drawBuffer << PLANAR_REFLECTION_FRAMEBUFFER_PAGE_Y_SHIFT));
        addPrim(&gGpuCurrentOt[PLANAR_REFLECTION_COPY_OT_INDEX], copyTexturePage);

        copySprite     = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(SPRT);
        copySprite->x0 = 0;
        copySprite->y0 = -PLANAR_REFLECTION_SCREEN_HALF_HEIGHT;
        copySprite->w  = PLANAR_REFLECTION_SCREEN_HALF_WIDTH;
        copySprite->h  = PLANAR_REFLECTION_SCREEN_HEIGHT;
        copySprite->u0 = PLANAR_REFLECTION_COPY_RIGHT_U;
        copySprite->v0 = gDisplayState.drawBuffer << PLANAR_REFLECTION_FRAMEBUFFER_V_SHIFT;
        setSprt(copySprite);
        setShadeTex(copySprite, 1);
        addPrim(&gGpuCurrentOt[PLANAR_REFLECTION_COPY_OT_INDEX], copySprite);

        copyTexturePage = gGpuPrimCursor;
        gGpuPrimCursor  = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
        setDrawTPage(copyTexturePage, 1, 1, getTPage(PLANAR_REFLECTION_TEXTURE_16_BIT, GPU_BLEND_AVERAGE, PLANAR_REFLECTION_COPY_RIGHT_PAGE_X, gDisplayState.drawBuffer << PLANAR_REFLECTION_FRAMEBUFFER_PAGE_Y_SHIFT));
        addPrim(&gGpuCurrentOt[PLANAR_REFLECTION_COPY_OT_INDEX], copyTexturePage);

        captureClear   = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(TILE);
        setTile(captureClear);
        captureClear->x0 = -PLANAR_REFLECTION_SCREEN_HALF_WIDTH;
        captureClear->y0 = -PLANAR_REFLECTION_SCREEN_HALF_HEIGHT;
        captureClear->r0 = captureClear->g0 = PLANAR_REFLECTION_CAPTURE_CLEAR_COLOR;
        captureClear->b0                    = PLANAR_REFLECTION_CAPTURE_CLEAR_COLOR;
        captureClear->w                     = PLANAR_REFLECTION_SCREEN_WIDTH;
        captureClear->h                     = PLANAR_REFLECTION_SCREEN_HEIGHT;
        addPrim(&gGpuCurrentOt[PLANAR_REFLECTION_COPY_OT_INDEX], captureClear);

        maskBitMode    = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_STP);
        SetDrawStp(maskBitMode, PLANAR_REFLECTION_FORCE_MASK_BIT);
        addPrim(&gGpuCurrentOt[PLANAR_REFLECTION_COPY_OT_INDEX], maskBitMode);

        drawOffset     = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_OFFSET);
        drawOrigin[0]  = captureX + PLANAR_REFLECTION_SCREEN_HALF_WIDTH;
        drawOrigin[1]  = PLANAR_REFLECTION_CAPTURE_Y + PLANAR_REFLECTION_SCREEN_HALF_HEIGHT;
        SetDrawOffset(drawOffset, drawOrigin);
        addPrim(&gGpuCurrentOt[PLANAR_REFLECTION_COPY_OT_INDEX], drawOffset);

        drawArea       = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_AREA);
        drawRect.x     = captureX;
        drawRect.y     = PLANAR_REFLECTION_CAPTURE_Y;
        drawRect.w     = PLANAR_REFLECTION_SCREEN_WIDTH;
        drawRect.h     = PLANAR_REFLECTION_SCREEN_HEIGHT;
        SetDrawArea(drawArea, &drawRect);
        addPrim(&gGpuCurrentOt[PLANAR_REFLECTION_COPY_OT_INDEX], drawArea);
    }

    reflectionModel->flags = mirrorWork->objectFlags;
    if (!(reflectionModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && gGameSession->sceneUpdatesPaused == 0) {
        // Both models share a source, so copy exactly the source's local part matrices.
        reflectionParts = reflectionTask->extra.tmd->coords;
        playerTask      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        eventSamplePart = &reflectionParts[1];
        if (playerTask != NULL) {
            TmdObject* sourceModel = playerTask->extra.tmd;
            GfxCoord*  sourceParts = sourceModel->coords;

            reflectionParts->composeStamp = GRAPHICS_COORD_DIRTY;
            partIndex                     = 0;
            if (sourceModel->partCount != 0) {
                MATRIX* sourceMatrix     = &sourceParts->coord;
                MATRIX* reflectionMatrix = &reflectionParts->coord;

                do {
                    *reflectionMatrix = *sourceMatrix;
                    reflectionMatrix  = &PARENT_OF(reflectionMatrix, GfxCoord, coord)[1].coord;
                    sourceMatrix      = &PARENT_OF(sourceMatrix, GfxCoord, coord)[1].coord;
                } while (++partIndex < sourceModel->partCount);
            }
        }
        if (stage == GAME_STAGE_ACROPOLIS || stage == GAME_STAGE_SHELTER_NEO_ARK) {
            enum {
                /// Local-Y distance from part 1 to each sample during a scripted event, in game coordinates.
                PLANAR_REFLECTION_EVENT_SAMPLE_DISTANCE = 0x3E8,
                /// Local Y of the root part's head sample, in game coordinates. The foot sample is the origin.
                PLANAR_REFLECTION_ROOT_HEAD_OFFSET = -0x7D0,
                /// Pixels added beyond each ordered screen edge before the quad is measured.
                PLANAR_REFLECTION_SCREEN_EDGE_PAD = 0x10,
                /// Quarter-depths added to the head sample when it replaces the foot's ordering depth.
                PLANAR_REFLECTION_ORDERING_DEPTH_BIAS = 0xA,
                /// Clears a VRAM X down to the 64-pixel alignment a 16-bit texture page requires.
                PLANAR_REFLECTION_TEXTURE_PAGE_MASK = 0xFFC0
            };

            // Project upper/lower samples in reflected model space before clipping the blend quads.
            extent = SCRATCH_STACK_RESERVE_BLOCK(_PlanarReflectionExtentScratch);
            if (gGameSession->eventState != 0) {
                actorRenderComposeCoord(eventSamplePart);
                gte_SetTransMatrix(&eventSamplePart->workm);
                gte_SetRotMatrix(&eventSamplePart->workm);
                extent->point.vx = 0;
                extent->point.vy = -PLANAR_REFLECTION_EVENT_SAMPLE_DISTANCE;
                extent->point.vz = 0;
                gte_RotTransPers(&extent->point, &extent->screenHead, &extent->depthCue, &extent->projectionFlags,
                                 &extent->orderingDepthHead);
                extent->point.vx = 0;
                extent->point.vy = PLANAR_REFLECTION_EVENT_SAMPLE_DISTANCE;
                extent->point.vz = 0;
                gte_RotTransPers(&extent->point, &extent->screenFoot, &extent->depthCue, &extent->projectionFlags,
                                 &extent->orderingDepthFoot);
            } else {
                actorRenderComposeCoord(reflectionParts);
                gte_SetTransMatrix(&reflectionParts->workm);
                gte_SetRotMatrix(&reflectionParts->workm);
                extent->point.vx = 0;
                extent->point.vy = PLANAR_REFLECTION_ROOT_HEAD_OFFSET;
                extent->point.vz = 0;
                gte_RotTransPers(&extent->point, &extent->screenHead, &extent->depthCue, &extent->projectionFlags,
                                 &extent->orderingDepthHead);
                extent->point.vx = 0;
                extent->point.vy = 0;
                extent->point.vz = 0;
                gte_RotTransPers(&extent->point, &extent->screenFoot, &extent->depthCue, &extent->projectionFlags,
                                 &extent->orderingDepthFoot);
            }
            // Order screen Y so the foot slot is the upper edge. screenHead.vx is the swap temporary.
            if (extent->screenFoot.vy > extent->screenHead.vy) {
                extent->screenHead.vx = extent->screenFoot.vy;
                extent->screenFoot.vy = extent->screenHead.vy;
                extent->screenHead.vy = extent->screenHead.vx;
            }
            extent->screenFoot.vy -= PLANAR_REFLECTION_SCREEN_EDGE_PAD;
            extent->screenHead.vy += PLANAR_REFLECTION_SCREEN_EDGE_PAD;
            // Half the padded vertical span, clamped to 95, is the quad's half-width about the foot's screen X.
            halfWidth = (extent->screenHead.vy - extent->screenFoot.vy) >> 1;
            if (halfWidth >= PLANAR_REFLECTION_MAX_QUAD_HALF_WIDTH + 1) {
                halfWidth = PLANAR_REFLECTION_MAX_QUAD_HALF_WIDTH;
            }
            // Area 2, view 5: mirror 0 forces that maximum; any other mirror orders from the head sample.
            if ((GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_AREA_VIEW_MASK) == GAME_LOCATION_KEY(0, 2, 0, 5)) {
                if (reflectionTask->spawnArg1.value == PLANAR_REFLECTION_MODE_FLOOR) {
                    halfWidth = PLANAR_REFLECTION_MAX_QUAD_HALF_WIDTH;
                } else {
                    extent->orderingDepthFoot = extent->orderingDepthHead + PLANAR_REFLECTION_ORDERING_DEPTH_BIAS;
                }
            }
            extent->left = extent->screenFoot.vx - halfWidth;
            if (extent->left < -PLANAR_REFLECTION_SCREEN_HALF_WIDTH) {
                extent->left = -PLANAR_REFLECTION_SCREEN_HALF_WIDTH;
            }
            extent->right = extent->screenFoot.vx + halfWidth;
            if (extent->right > PLANAR_REFLECTION_SCREEN_HALF_WIDTH) {
                extent->right = PLANAR_REFLECTION_SCREEN_HALF_WIDTH;
            }
            extent->top = extent->screenFoot.vy;
            if (extent->top < -PLANAR_REFLECTION_SCREEN_HALF_HEIGHT) {
                extent->top = -PLANAR_REFLECTION_SCREEN_HALF_HEIGHT;
            }
            extent->bottom = extent->screenHead.vy;
            if (extent->bottom > PLANAR_REFLECTION_SCREEN_HALF_HEIGHT) {
                extent->bottom = PLANAR_REFLECTION_SCREEN_HALF_HEIGHT;
            }
            if (extent->top < mirrorWork->clipBottom && mirrorWork->clipTop < extent->bottom && extent->left < mirrorWork->clipRight &&
                mirrorWork->clipLeft < extent->right) {
                DR_TPAGE* textureMode;

                textureMode          = gGpuPrimCursor;
                textureX             = extent->left + (u16)(captureX + PLANAR_REFLECTION_SCREEN_HALF_WIDTH);
                extent->texturePageX = textureX & PLANAR_REFLECTION_TEXTURE_PAGE_MASK;
                gGpuPrimCursor       = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
                setDrawTPage(textureMode, 0, 1, 0);
                addPrim(&gGpuCurrentOt[(((extent->orderingDepthFoot << gDisplayState.otDepthShift) & PLANAR_REFLECTION_OT_DEPTH_MASK) >> 4) + reflectionModel->otOffset - PLANAR_REFLECTION_BLEND_DEPTH_BIAS],
                        textureMode);
                // The 190-pixel maximum span fits a 256-texel page even after X alignment.
                // Same-depth packets run in reverse insertion order, including the blend layers.
                for (blendMode = mirrorWork->firstBlendMode; blendMode < GPU_BLEND_ADD_QUARTER; blendMode++) {
                    blendQuad      = gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(POLY_FT4);
                    setPolyFT4(blendQuad);
                    setSemiTrans(blendQuad, 1);
                    if (mirrorWork->firstBlendMode == GPU_BLEND_ADD) {
                        setShadeTex(blendQuad, 0);
                        blendQuad->r0 = blendQuad->g0 = blendQuad->b0 = PLANAR_REFLECTION_NEUTRAL_TEXTURE_COLOR;
                    } else {
                        setShadeTex(blendQuad, 1);
                    }
                    blendQuad->x0 = blendQuad->x2 = extent->left;
                    blendQuad->x1 = blendQuad->x3 = extent->right;
                    blendQuad->y0 = blendQuad->y1 = extent->top;
                    blendQuad->y2 = blendQuad->y3 = extent->bottom;
                    blendQuad->tpage              = getTPage(PLANAR_REFLECTION_TEXTURE_16_BIT, blendMode, extent->texturePageX, PLANAR_REFLECTION_CAPTURE_Y);
                    blendQuad->u0 = blendQuad->u2 = blendQuad->x0 + PLANAR_REFLECTION_SCREEN_HALF_WIDTH + captureX - extent->texturePageX;
                    blendQuad->u1 = blendQuad->u3 = blendQuad->x1 + PLANAR_REFLECTION_SCREEN_HALF_WIDTH + captureX - extent->texturePageX;
                    blendQuad->v0 = blendQuad->v1 = blendQuad->y0 + PLANAR_REFLECTION_SCREEN_HALF_HEIGHT;
                    blendQuad->v2 = blendQuad->v3 = blendQuad->y2 + PLANAR_REFLECTION_SCREEN_HALF_HEIGHT;
                    addPrim(&gGpuCurrentOt[(((extent->orderingDepthFoot << gDisplayState.otDepthShift) & PLANAR_REFLECTION_OT_DEPTH_MASK) >> 4) + reflectionModel->otOffset - PLANAR_REFLECTION_BLEND_DEPTH_BIAS],
                            blendQuad);
                }
                textureMode    = gGpuPrimCursor;
                gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
                setDrawTPage(textureMode, 0, 0, 0);
                addPrim(&gGpuCurrentOt[(((extent->orderingDepthFoot << gDisplayState.otDepthShift) & PLANAR_REFLECTION_OT_DEPTH_MASK) >> 4) + reflectionModel->otOffset - PLANAR_REFLECTION_BLEND_DEPTH_BIAS],
                        textureMode);
            } else {
                reflectionModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            SCRATCH_STACK_RELEASE_BLOCK(_PlanarReflectionExtentScratch);
        }
    }

    {
        GfxCoord*  playerParts;
        TmdObject* playerModel;
        GfxCoord*  reflectionRoot;
        MATRIX     lightingRotation;

        // Rotate the player's lighting into the reflection root's orientation, even while paused.
        playerParts       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
        playerModel       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd;
        reflectionRoot    = reflectionTask->extra.tmd->coords;
        mirrorWork->light = *playerModel->lightMtx;
        mirrorWork->color = *playerModel->colorMtx;
        actorRenderComposeCoord(reflectionRoot);
        gte_TransposeMatrix(&reflectionRoot->workm, &lightingRotation);
        gte_MulMatrix0(&playerParts->workm, &lightingRotation, &lightingRotation);
        gte_MulMatrix0(&mirrorWork->light, &lightingRotation, &mirrorWork->light);
    }
}

/// Copies an attachment root's offset with local X negated and marks its composed transform stale.
///
/// Both pointers borrow live root coordinates for the call. The three translation
/// components are signed 32-bit game units; source X must have a representable
/// negation. The reflection root keeps its rotation and parent, which the caller
/// sets separately, and must be composed again before its cached matrix is used.
static inline void _planarReflectionReflectAttachmentOffset(GfxCoord* reflectionRoot, const GfxCoord* sourceRoot)
{
    reflectionRoot->coord.t[0]   = -sourceRoot->coord.t[0];
    reflectionRoot->coord.t[1]   = sourceRoot->coord.t[1];
    reflectionRoot->coord.t[2]   = sourceRoot->coord.t[2];
    reflectionRoot->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Creates and updates a reflected player attachment or equipment model.
///
/// `spawnArg1.value` is a slot in 0..3: 0/1 select attachment tasks, 2/3
/// select equipment tasks. Slots 0/2 attach to reflected player part 12;
/// slots 1/3 attach to part 8, so the player model must contain both parts.
/// `spawnArg2.pointer` borrows the player-reflection task and its
/// `RoomMirrorWork`; `parent` is the live source-model task. Both must outlive
/// this task, including its borrowed coordinate parent and lighting matrices.
/// The task starts bodyless in state 0 and owns the clone after attachment.
/// Its initial root offset is reflected across X in game units; equipment
/// also flips the root rotation with a Q12 (-1, +1, +1) scale. Later frames
/// follow the player reflection's flags, clearing reverse culling for equipment.
static void _planarReflectionAttachmentTask(Task* reflectionTask)
{
    enum {
        PLANAR_REFLECTION_ATTACHMENT_STATE_INIT = 0,
        PLANAR_REFLECTION_FIRST_EQUIPMENT_SLOT  = 2,
        PLANAR_REFLECTION_ATTACHMENT_OT_OFFSET  = 31 // Ordering-table entries, matching the reflected player
    };
    Task*           playerReflection;
    TmdObject*      playerReflectionModel;
    RoomMirrorWork* mirrorWork;
    GfxCoord*       reflectedPart;
    TmdObject*      sourceModel;
    const GfxCoord* sourceRoot;
    TmdObject*      reflectionModel;
    GfxCoord*       reflectionRoot;
    VECTOR          xFlipScale;
    u16             playerReflectionFlags;

    if (reflectionTask->parent == NULL) {
        taskCallExit(reflectionTask);
    }
    playerReflection      = reflectionTask->spawnArg2.pointer;
    reflectedPart         = &playerReflection->extra.tmd->coords[Reflection_Data_8017FC8C[reflectionTask->spawnArg1.value]];
    mirrorWork            = playerReflection->work;
    playerReflectionModel = playerReflection->extra.tmd;
    if (reflectionTask->state == PLANAR_REFLECTION_ATTACHMENT_STATE_INIT) {
        // Borrow the source geometry, but own a new model and both rebuilt packet halves.
        sourceModel = reflectionTask->parent->extra.tmd;
        sourceRoot  = sourceModel->coords;
        if (modelObjectAttachTmd(reflectionTask, sourceModel->source) == NULL) {
            taskCallExit(reflectionTask);
            return;
        }
        reflectionModel                    = reflectionTask->extra.tmd;
        reflectionRoot                     = reflectionModel->coords;
        reflectionModel->texturePageOffset = sourceModel->texturePageOffset;
        tmdBuildBufferHalf(reflectionModel);
        tmdBuildBufferHalf(reflectionModel);
        reflectionModel->flags    = TMD_OBJECT_REVERSE_CULLING;
        reflectionModel->otOffset = PLANAR_REFLECTION_ATTACHMENT_OT_OFFSET;
        reflectionRoot->parent    = reflectedPart;
        reflectionModel->lightMtx = &mirrorWork->light;
        reflectionModel->colorMtx = &mirrorWork->color;
        // Equipment needs another X flip within the already reflected player frame.
        if (reflectionTask->spawnArg1.value >= PLANAR_REFLECTION_FIRST_EQUIPMENT_SLOT) {
            xFlipScale = Reflection_Data_8017D5C4;
            ScaleMatrix(&reflectionRoot->coord, &xFlipScale);
        }
        _planarReflectionReflectAttachmentOffset(reflectionRoot, sourceRoot);
        reflectionTask->state++;
    }
    // Follow mirror visibility while correcting facing for the extra equipment flip.
    reflectionModel        = reflectionTask->extra.tmd;
    playerReflectionFlags  = playerReflectionModel->flags;
    reflectionModel->flags = playerReflectionFlags;
    if (reflectionTask->spawnArg1.value >= PLANAR_REFLECTION_FIRST_EQUIPMENT_SLOT) {
        reflectionModel->flags = playerReflectionFlags & (u16)~TMD_OBJECT_REVERSE_CULLING;
    }
}

/// Dispatches the player reflection's initialization and per-frame states.
///
/// The including overlay supplies the ordinary task entry point. The task's
/// state must be 0 (initialize) or 1 (update); dispatch does not check bounds.
/// Initialization advances the state and performs the first update immediately.
/// Callback entries have the resident `TaskFunc` signature and remain private
/// to the including translation unit.
static inline void _planarReflectionPlayerTask(Task* reflectionTask)
{
    TaskFunc stateHandlers[PLANAR_REFLECTION_PLAYER_STATE_UPDATE + 1] = {
        _planarReflectionInitPlayer,
        _planarReflectionUpdatePlayer,
    };

    stateHandlers[reflectionTask->state](reflectionTask);
}
