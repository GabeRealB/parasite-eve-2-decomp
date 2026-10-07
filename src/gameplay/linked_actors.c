#include "linked_actors.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/area_entry.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "damage.h"
#include "ending.h"
#include "gameplay/enemy.h"
#include "hud.h"
#include "hud_sprites.h"
#include "gameplay/scene.h"
#include "world_targets.h"

#include "gameplay/damage.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

/// Scratch-stack workspace for drawing the wireframe of a Parasite Energy area.
///
/// The wireframe is a set of polylines. Each vertex is built in the area's own
/// frame, with the area's axis along negative Y, projected through the GTE and
/// joined to the vertex before it by a flat line. `radius` and `extent` are in
/// world units: the dimensions of an `AttachmentAreaParam` once scaled. Reserve
/// the complete record; nothing in it survives the release.
typedef struct {
    SVECTOR point;            // Vertex being projected, in the area's frame; before that, the centre's offset rotated into view space
    MATRIX  viewRotation;     // Area-to-view rotation loaded into the GTE; only its rotation elements are used
    s32     radius;           // Radius of the area about its axis
    s32     extent;           // Extent of the area along its axis (an ellipsoid's semi-axis, a cylinder's length)
    s32     ringRadius;       // Radius of the ring the vertex lies on; an ellipsoid narrows it with height
    s32     depthCue;         // GTE IR0 depth-cue coefficient of the last projection; stored, never read
    s32     projectionFlags;  // GTE FLAG bits of the last projection; stored, never read
    s32     orderingDepth;    // Quarter camera-space depth of the last projected vertex (0..16383)
    u32     previousScreenXy; // Screen position of the vertex before it, in the same packing
    u32     screenXy;         // Screen position of the last projected vertex (X in bits 0..15, Y in bits 16..31)
    byte    unknown_48[8];    // Never accessed; role unproven
    VECTOR  viewCentre;       // View-space centre of an area that is not centred on the player's root
} _AttachmentAreaWireframeScratch;
STATIC_ASSERT_SIZEOF(_AttachmentAreaWireframeScratch, 0x60);

// The image stores this head alone in the linked_actors BSS subsegment.
WorldTargetNode* gWorldTargetListHead;

/// Target tests allow 100 extra world units on each dimension and below the root.
enum {
    ATTACHMENT_TARGET_ALLOWANCE          = 100,
    ATTACHMENT_TARGET_POSITION_SHIFT     = 4,
    ATTACHMENT_TARGET_SQUARED_AXIS_SHIFT = 8
};

static inline void _hudDrawHpMpValue(s32 x, s32 y, u32 colorRgb, s32 value);

static inline void _hudDrawHpMpLabels(const UiPanel* panel, s32 offsetX, s32 offsetY, u32 colorRgb);

/// Converts the current decimal PE id to its category-2 attachment attack key.
///
/// Hundreds select element 1..6, tens select energy 1..3 and ones select level
/// 1..3, producing rows 1..54. Keep the unsigned digit arithmetic and additive
/// encoding; no lookup or validation occurs.
static inline s32 _attachmentCurrentAttackKey(void)
{
    u16 attachmentId;
    s32 attackKey;

    attachmentId = Gp_StateC08.attachId;
    attackKey    = (attachmentId / 100U - 1) * 9;
    attackKey   += ((attachmentId % 100U) / 10U - 1) * 3;
    attackKey   += attachmentId % 10U;
    attackKey   += WORLD_COLLISION_CONTACT_ATTACK | DAMAGE_PLAYER_ATTACK_ATTACHMENT;
    return attackKey;
}

/// Advances a displayed stat by one point toward its live value, in either direction.
///
/// Borrows one writable widened HP/MP value; it retains no pointer.
static inline void _hudStepDisplayedStat(s32* displayed, s32 live)
{
    if (live < *displayed) {
        *displayed = *displayed - 1;
    } else if (*displayed < live) {
        *displayed = *displayed + 1;
    }
}

void attachmentTargetAll(s32 release)
{
    WorldTargetNode* node;
    Enemy*           enemy;
    Enemy*           contactEnemy;
    s32              attackKey;

    for (node = gWorldTargetListHead; node != NULL; node = node->next) {
        if ((node->state.word & WORLD_TARGET_SCAN_MASK) != WORLD_TARGET_NOT_LOCKABLE) {
            enemy        = GP_NODE_ENEMY(node);
            contactEnemy = enemy;
            if (release == 0) {
                enemy->colorMode |= ENEMY_COLOR_HIT_FLASH;
            } else {
                attackKey = _attachmentCurrentAttackKey();
                attachmentAddTargetContact(contactEnemy, attackKey);
            }
        }
    }
}

/// Sets a ring vertex's XZ offset, preserving its already selected Y coordinate.
///
/// Radius uses game units; `angle` counts 4096 units per turn, with +X at zero
/// and +Z at the positive quarter turn. Products use twelve fractional bits
/// and narrow to signed halfwords. Scratch storage is borrowed for this call.
static __inline__ void _attachmentAreaWireframeSetRingPoint(_AttachmentAreaWireframeScratch* scratch, s32 angle)
{
    enum { ATTACHMENT_AREA_WIREFRAME_TRIG_FRACTION_BITS = 12 };

    scratch->point.vx = (scratch->ringRadius * rcos(angle)) >> ATTACHMENT_AREA_WIREFRAME_TRIG_FRACTION_BITS;
    scratch->point.vz = (scratch->ringRadius * rsin(angle)) >> ATTACHMENT_AREA_WIREFRAME_TRIG_FRACTION_BITS;
}

/// Projects one area vertex and stores its packed screen position and GTE results.
///
/// The caller loads the area-to-view rotation, centre translation and projection
/// parameters. Captures IR0, FLAG and quarter-depth even when projection fails;
/// this helper does not reject or clip vertices. Changes GTE arithmetic state.
static __inline__ void _attachmentAreaWireframeProjectPoint(_AttachmentAreaWireframeScratch* scratch)
{
    gte_ldv0(&scratch->point);
    gte_rtps();
    gte_stsxy(&scratch->screenXy);
    gte_stdp(&scratch->depthCue);
    gte_stflg(&scratch->projectionFlags);
    gte_stszotz(&scratch->orderingDepth);
}

/// Emits a green flat line from the previous projected vertex to the current one.
///
/// Both packed positions and the current vertex's quarter-depth must be set.
/// Requires one LINE_F2 of aligned frame-arena capacity and the current 1024-tag
/// depth table. Depth is shifted and wrapped to that table; no clipping occurs.
/// The GPU borrows the packet until the frame is drawn.
static __inline__ void _attachmentAreaWireframeEmitSegment(const _AttachmentAreaWireframeScratch* scratch)
{
    enum { ATTACHMENT_AREA_WIREFRAME_LINE_COLOR = GPU_PACK_COLOR_WORD(0, 0xC0, 0x40, 0) };
    LINE_F2* line;

    line                              = gGpuPrimCursor;
    gGpuPrimCursor                    = line + 1;
    GPU_PRIMITIVE_COLOR_WORD(line, 0) = ATTACHMENT_AREA_WIREFRAME_LINE_COLOR;
    GPU_PRIMITIVE_XY_WORD(line, 0)    = scratch->previousScreenXy;
    GPU_PRIMITIVE_XY_WORD(line, 1)    = scratch->screenXy;
    setLineF2(line);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->orderingDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            line);
}

void attachmentDrawAreaWireframe(s32 unused, s32 radius, s32 extent, s32 options)
{
    enum {
        ATTACHMENT_AREA_WIREFRAME_ANGLE_BITS               = 12,
        ATTACHMENT_AREA_WIREFRAME_TRIG_SHIFT               = 12,
        ATTACHMENT_AREA_WIREFRAME_SPIN_SHIFT               = 4,
        ATTACHMENT_AREA_WIREFRAME_PROJECTILE_ANCHOR_PART   = 4,
        ATTACHMENT_AREA_WIREFRAME_PROJECTILE_ROOT_Y_OFFSET = 300,
        ATTACHMENT_AREA_WIREFRAME_PROJECTILE_PITCH         = -0x400,
        ATTACHMENT_AREA_WIREFRAME_DOME_TOP_ANGLE           = 0x300,
        ATTACHMENT_AREA_WIREFRAME_CYLINDER_TOP_STEP        = 0x400,
        ATTACHMENT_AREA_WIREFRAME_CYLINDER_HEIGHT_SHIFT    = 10,
        ATTACHMENT_AREA_WIREFRAME_MERIDIAN_COUNT           = 12,
        ATTACHMENT_AREA_WIREFRAME_HEIGHT_STEP              = 0x80,
        ATTACHMENT_AREA_WIREFRAME_MERIDIAN_TWIST_STEP      = 0x73,
        ATTACHMENT_AREA_WIREFRAME_RING_SEGMENTS            = 24,
        ATTACHMENT_AREA_WIREFRAME_RING_COUNT               = 2
    };
    Task*                            playerTask;
    GfxCoord*                        playerRoot;
    GfxCoord*                        projectileAnchor;
    _AttachmentAreaWireframeScratch* scratch;
    s32                              spinAngle;
    s32                              meridianLimit;
    s32                              azimuth;
    s32                              segmentIndex;
    s32                              heightStep;
    s32                              ringIndex;

    playerTask      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch         = SCRATCH_STACK_RESERVE_BLOCK(_AttachmentAreaWireframeScratch);
    playerRoot      = playerTask->extra.tmd->coords;
    scratch->radius = radius;
    scratch->extent = extent;
    spinAngle       = gDisplayState.animFrame << ATTACHMENT_AREA_WIREFRAME_SPIN_SHIFT;
    // Place the centre in view space; the projectile preview follows the player.
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    if (options & ATTACHMENT_AREA_WIREFRAME_PROJECTILE) {
        projectileAnchor  = &playerTask->extra.tmd->coords[ATTACHMENT_AREA_WIREFRAME_PROJECTILE_ANCHOR_PART];
        scratch->point.vx = 0;
        scratch->point.vy = ATTACHMENT_AREA_WIREFRAME_PROJECTILE_ROOT_Y_OFFSET;
        scratch->point.vz = 0;
        _gfxLoadRotSv(&playerRoot->workm, &scratch->point);
        gte_rtv0();
        gte_stsv(&scratch->point);
        scratch->viewCentre.vx = projectileAnchor->workm.t[0] + scratch->point.vx;
        scratch->viewCentre.vy = projectileAnchor->workm.t[1] + scratch->point.vy;
        scratch->viewCentre.vz = projectileAnchor->workm.t[2] + scratch->point.vz;
        gte_SetTransVector(&scratch->viewCentre);
    } else if ((options & ATTACHMENT_AREA_WIREFRAME_AHEAD) == 0) {
        gte_SetTransMatrix(&playerRoot->workm);
    } else {
        options          &= ~ATTACHMENT_AREA_WIREFRAME_AHEAD;
        scratch->point.vx = 0;
        scratch->point.vy = 0;
        scratch->point.vz = radius;
        _gfxLoadRotSv(&playerRoot->workm, &scratch->point);
        gte_rtv0();
        gte_stsv(&scratch->point);
        scratch->viewCentre.vx = playerRoot->workm.t[0] + scratch->point.vx;
        scratch->viewCentre.vy = playerRoot->workm.t[1] + scratch->point.vy;
        scratch->viewCentre.vz = playerRoot->workm.t[2] + scratch->point.vz;
        gte_SetTransVector(&scratch->viewCentre);
    }

    if (options & ATTACHMENT_AREA_WIREFRAME_PROJECTILE) {
        scratch->viewRotation = playerRoot->workm;
        gfxRotMatrixX(&scratch->viewRotation, ATTACHMENT_AREA_WIREFRAME_PROJECTILE_PITCH, GRAPHICS_ROTATION_COMPOSE);
        options &= ~ATTACHMENT_AREA_WIREFRAME_PROJECTILE;
    } else {
        scratch->viewRotation = gGfxViewCoord.workm;
    }
    gte_SetRotMatrix(&scratch->viewRotation);

    meridianLimit = ATTACHMENT_AREA_WIREFRAME_CYLINDER_TOP_STEP;
    if (options == 0) {
        meridianLimit = ATTACHMENT_AREA_WIREFRAME_DOME_TOP_ANGLE;
    }

    // Twist twelve meridians as the display frame advances.
    for (segmentIndex = 0; segmentIndex < ATTACHMENT_AREA_WIREFRAME_MERIDIAN_COUNT; segmentIndex++) {
        azimuth = spinAngle + ((segmentIndex << ATTACHMENT_AREA_WIREFRAME_ANGLE_BITS) / ATTACHMENT_AREA_WIREFRAME_MERIDIAN_COUNT);
        for (heightStep = 0; heightStep <= meridianLimit; azimuth += ATTACHMENT_AREA_WIREFRAME_MERIDIAN_TWIST_STEP, heightStep += ATTACHMENT_AREA_WIREFRAME_HEIGHT_STEP) {
            if (options == 0) {
                scratch->ringRadius = (scratch->radius * rcos(heightStep)) >> ATTACHMENT_AREA_WIREFRAME_TRIG_SHIFT;
                scratch->point.vy   = -(scratch->extent * rsin(heightStep)) >> ATTACHMENT_AREA_WIREFRAME_TRIG_SHIFT;
                _attachmentAreaWireframeSetRingPoint(scratch, azimuth);
            } else {
                scratch->point.vy = -(scratch->extent * heightStep) >> ATTACHMENT_AREA_WIREFRAME_CYLINDER_HEIGHT_SHIFT;
                scratch->point.vx = (scratch->radius * rcos(azimuth)) >> ATTACHMENT_AREA_WIREFRAME_TRIG_SHIFT;
                scratch->point.vz = (scratch->radius * rsin(azimuth)) >> ATTACHMENT_AREA_WIREFRAME_TRIG_SHIFT;
            }
            _attachmentAreaWireframeProjectPoint(scratch);
            if (heightStep > 0) {
                _attachmentAreaWireframeEmitSegment(scratch);
            }
            scratch->previousScreenXy = scratch->screenXy;
        }
    }

    // Close the upper and base boundaries with rings spinning the other way.
    spinAngle = -spinAngle;
    for (ringIndex = 0; ringIndex < ATTACHMENT_AREA_WIREFRAME_RING_COUNT; ringIndex++) {
        if (ringIndex == 0) {
            if (options == 0) {
                scratch->ringRadius = (scratch->radius * rcos(ATTACHMENT_AREA_WIREFRAME_DOME_TOP_ANGLE)) >> ATTACHMENT_AREA_WIREFRAME_TRIG_SHIFT;
                scratch->point.vy   = -(scratch->extent * rsin(ATTACHMENT_AREA_WIREFRAME_DOME_TOP_ANGLE)) >> ATTACHMENT_AREA_WIREFRAME_TRIG_SHIFT;
            } else {
                scratch->ringRadius = scratch->radius;
                scratch->point.vy   = -scratch->extent;
            }
        } else {
            scratch->point.vy   = 0;
            scratch->ringRadius = scratch->radius;
        }
        for (segmentIndex = 0; segmentIndex < ATTACHMENT_AREA_WIREFRAME_RING_SEGMENTS + 1; segmentIndex++) {
            azimuth = spinAngle + ((segmentIndex << ATTACHMENT_AREA_WIREFRAME_ANGLE_BITS) / ATTACHMENT_AREA_WIREFRAME_RING_SEGMENTS);
            _attachmentAreaWireframeSetRingPoint(scratch, azimuth);
            _attachmentAreaWireframeProjectPoint(scratch);
            if (segmentIndex != 0) {
                _attachmentAreaWireframeEmitSegment(scratch);
            }
            scratch->previousScreenXy = scratch->screenXy;
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(_AttachmentAreaWireframeScratch);
}

void attachmentTargetEllipsoid(s32 release, s32 radius, s32 extent, s32 ahead)
{
    SVECTOR*         targetPosition;
    WorldTargetNode* node;
    Enemy*           enemy;
    s32              attackKey;
    s32              extentSquaredScaled;
    s32              radiusSquaredScaled;

    // Draw the unexpanded area, then include the targeting allowance in the scan.
    if (release == 0) {
        if (ahead == 0) {
            attachmentDrawAreaWireframe(0, radius, extent, 0);
        } else {
            attachmentDrawAreaWireframe(0, radius, extent, ATTACHMENT_AREA_WIREFRAME_AHEAD);
        }
    }

    extent             += ATTACHMENT_TARGET_ALLOWANCE;
    radius             += ATTACHMENT_TARGET_ALLOWANCE;
    extentSquaredScaled = (extent * extent) >> ATTACHMENT_TARGET_SQUARED_AXIS_SHIFT;
    radiusSquaredScaled = (radius * radius) >> ATTACHMENT_TARGET_SQUARED_AXIS_SHIFT;
    node                = gWorldTargetListHead;
    SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    targetPosition = SCRATCH_STACK_CURSOR(SVECTOR);

    if (node != NULL) {
        do {
            if ((node->state.word & WORLD_TARGET_SCAN_MASK) != WORLD_TARGET_NOT_LOCKABLE) {
                targetPosition->vx = GP_NODE_ENEMY(node)->playerRelPos.vx;
                targetPosition->vy = GP_NODE_ENEMY(node)->playerRelPos.vy;
                targetPosition->vz = GP_NODE_ENEMY(node)->playerRelPos.vz;
                if (ahead != 0) {
                    targetPosition->vz -= radius;
                }
                if (targetPosition->vy >= -extent && targetPosition->vy < ATTACHMENT_TARGET_ALLOWANCE + 1 && targetPosition->vx >= -radius && targetPosition->vx <= radius && targetPosition->vz >= -radius &&
                    targetPosition->vz <= radius) {
                    // Reduce coordinates before the unsigned, wrapping ellipsoid comparison.
                    targetPosition->vx >>= ATTACHMENT_TARGET_POSITION_SHIFT;
                    targetPosition->vy >>= ATTACHMENT_TARGET_POSITION_SHIFT;
                    targetPosition->vz >>= ATTACHMENT_TARGET_POSITION_SHIFT;
                    if ((u32)(extentSquaredScaled * (targetPosition->vx * targetPosition->vx + targetPosition->vz * targetPosition->vz) + radiusSquaredScaled * (targetPosition->vy * targetPosition->vy)) <=
                        (u32)(extentSquaredScaled * radiusSquaredScaled)) {
                        enemy = GP_NODE_ENEMY(node);
                        if (release == 0) {
                            enemy->colorMode |= ENEMY_COLOR_HIT_FLASH;
                        } else {
                            attackKey = _attachmentCurrentAttackKey();
                            attachmentAddTargetContact(enemy, attackKey);
                        }
                    }
                }
            }
            node = node->next;
        } while (node != NULL);
    }

    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

void attachmentTargetCylinder(s32 release, s32 radius, s32 extent, s32 ahead)
{
    SVECTOR*         targetPosition;
    WorldTargetNode* node;
    Enemy*           enemy;
    s32              attackKey;
    s32              targetY;

    // Draw the unexpanded area, then include the targeting allowance in the scan.
    if (release == 0) {
        if (ahead == 0) {
            attachmentDrawAreaWireframe(0, radius, extent, ATTACHMENT_AREA_WIREFRAME_CYLINDER);
        } else {
            attachmentDrawAreaWireframe(0, radius, extent, ATTACHMENT_AREA_WIREFRAME_CYLINDER | ATTACHMENT_AREA_WIREFRAME_AHEAD);
        }
    }

    radius += ATTACHMENT_TARGET_ALLOWANCE;
    extent += ATTACHMENT_TARGET_ALLOWANCE;
    node    = gWorldTargetListHead;
    SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    targetPosition = SCRATCH_STACK_CURSOR(SVECTOR);

    if (node != NULL) {
        do {
            if ((node->state.word & WORLD_TARGET_SCAN_MASK) != WORLD_TARGET_NOT_LOCKABLE) {
                targetPosition->vx = GP_NODE_ENEMY(node)->playerRelPos.vx;
                targetPosition->vy = GP_NODE_ENEMY(node)->playerRelPos.vy;
                targetPosition->vz = GP_NODE_ENEMY(node)->playerRelPos.vz;
                if (ahead != 0) {
                    targetPosition->vz -= radius;
                }
                targetY = targetPosition->vy;
                if (targetY < ATTACHMENT_TARGET_ALLOWANCE + 1 && targetY >= -extent) {
                    if ((u32)(targetPosition->vx * targetPosition->vx + targetPosition->vz * targetPosition->vz) <= (u32)(radius * radius)) {
                        enemy = GP_NODE_ENEMY(node);
                        if (release == 0) {
                            enemy->colorMode |= ENEMY_COLOR_HIT_FLASH;
                        } else {
                            attackKey = _attachmentCurrentAttackKey();
                            attachmentAddTargetContact(enemy, attackKey);
                        }
                    }
                }
            }
            node = node->next;
        } while (node != NULL);
    }

    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Draws one live HP/MP value as a right-aligned, outlined HUD number.
///
/// Coordinates are screen-centered pixels and `colorRgb` is packed 0xBBGGRR.
/// Negative values display zero; the unsigned decimal formatter saturates at
/// 999,999,999. Uses a private 16-byte digit buffer (at most ten bytes written),
/// medium glyphs and front tag -2. Text/primitive storage belongs to this frame;
/// the stack requests and digits are not retained.
static inline void _hudDrawHpMpValue(s32 x, s32 y, u32 colorRgb, s32 value)
{
    enum { HUD_HP_MP_VALUE_OT_INDEX = -2 };
    u8          digits[0x10];
    TextDrawReq request;

    if (value < 0) {
        value = 0;
    }
    request.x          = x;
    request.y          = y;
    request.otIndex    = HUD_HP_MP_VALUE_OT_INDEX;
    request.colorRgb   = colorRgb;
    request.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    request.alignment  = TEXT_ALIGNMENT_RIGHT;
    request.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&request, textItoaUnsigned(digits, value));
}

/// Draws the HP and MP captions relative to a borrowed UI panel.
///
/// Only the panel's content origin and signed ordering-table index are read.
/// Offsets use screen pixels; `colorRgb` is packed 0xBBGGRR. Small outlined
/// captions use the panel's tag plus one. Requires loaded glyphs and frame
/// arena capacity; retains neither the read-only panel nor stack requests.
static inline void _hudDrawHpMpLabels(const UiPanel* panel, s32 offsetX, s32 offsetY, u32 colorRgb)
{
    TextDrawReq hpRequest;
    TextDrawReq mpRequest;

    hpRequest.x          = panel->contentOriginX.unsignedValue + 4 + offsetX;
    hpRequest.y          = panel->contentOriginY.unsignedValue + 8 + offsetY;
    hpRequest.otIndex    = panel->otIndex.signedValue + 1;
    hpRequest.colorRgb   = colorRgb;
    hpRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    hpRequest.alignment  = TEXT_ALIGNMENT_LEFT;
    hpRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&hpRequest, (const u8*)Gp_StrHP);

    mpRequest.x          = panel->contentOriginX.unsignedValue + 0x2E + offsetX;
    mpRequest.y          = panel->contentOriginY.unsignedValue + 8 + offsetY;
    mpRequest.otIndex    = panel->otIndex.signedValue + 1;
    mpRequest.colorRgb   = colorRgb;
    mpRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    mpRequest.alignment  = TEXT_ALIGNMENT_LEFT;
    mpRequest.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&mpRequest, (const u8*)Gp_StrMP);
}

void hudDrawStatusBlock(const HudState* hud)
{
    enum {
        HUD_STATUS_BLOCK_LEFT                  = -152,
        HUD_STATUS_BLOCK_TOP                   = -100,
        HUD_STATUS_BLOCK_WIDTH                 = 90,
        HUD_STATUS_BLOCK_HEIGHT                = 20,
        HUD_STATUS_BLOCK_SHAKE_SCALE           = 3,
        HUD_STATUS_BLOCK_OT_INDEX              = -2,
        HUD_STATUS_BLOCK_LABEL_PANEL_OT_INDEX  = -3,
        HUD_STATUS_BLOCK_FRAME_OT_INDEX        = -1,
        HUD_STATUS_BLOCK_TEXT_COLOR            = 0x606060,
        HUD_STATUS_BAR_PIXELS                  = 37,
        HUD_STATUS_BAR_HEIGHT                  = 2,
        HUD_STATUS_BAR_REMAINING_COLOR         = GPU_PACK_COLOR_WORD(0x1f, 0x74, 0x01, 0),
        HUD_STATUS_BAR_COST_COLOR              = GPU_PACK_COLOR_WORD(0xff, 0xff, 0, 0),
        HUD_STATUS_BAR_TILE_WORDS              = 3,
        HUD_STATUS_BAR_TILE_CODE               = 0x60,
        HUD_STATUS_BAR_FRAME_CLUT              = 0x3C0B,
        HUD_STATUS_BAR_FRAME_LEFT_U            = 0x98,
        HUD_STATUS_BAR_FRAME_MIDDLE_U          = 0xA0,
        HUD_STATUS_BAR_FRAME_RIGHT_U           = 0xA8,
        HUD_STATUS_BAR_FRAME_TOP_V             = 0x68,
        HUD_STATUS_BAR_FRAME_BOTTOM_V          = 0x70,
        HUD_STATUS_BAR_FRAME_TPAGE             = 0x3E,
        HUD_STATUS_BAR_CAP_WORDS               = 3,
        HUD_STATUS_BAR_RAW_SPRITE8_CODE        = 0x75,
        HUD_STATUS_BAR_QUAD_WORDS              = 9,
        HUD_STATUS_BAR_RAW_QUAD_CODE           = 0x2D,
        HUD_STATUS_BAR_TEXTURE_PAGE_COMMAND    = 0xE100023E,
        HUD_STATUS_BLOCK_EFFECTS_FRAME_STYLE   = 4,
        HUD_STATUS_ICON_STRIDE                 = 13,
        HUD_STATUS_ICON_PIXELS                 = 14,
        HUD_STATUS_ICON_TEXTURE_STRIDE         = 16,
        HUD_STATUS_ICON_FIRST_U                = 0x60,
        HUD_STATUS_ICON_V                      = 0x40,
        HUD_STATUS_ICON_CLUT                   = 0x3C08,
        HUD_STATUS_ICON_WORDS                  = 4,
        HUD_STATUS_ICON_RAW_SPRITE_CODE        = 0x65,
        HUD_STATUS_UNIDENTIFIED_TIMED_EFFECT   = 0x20,
        HUD_STATUS_COMPANION_NO_READOUT_FAMILY = 2
    };
    GameDebugState* debugState;
    s32             pendingMp;
    TILE*           tile;
    SPRT *          hpCapSprite, *mpCapSprite, *statusSprite;
    POLY_FT4 *      hpFrameQuad, *mpFrameQuad;
    DR_TPAGE*       texturePage;
    PlayerStatus*   playerStatus;
    s32             pendingHp;
    s32             y;
    s32             hp;
    s32             mp;
    u32             colorRgb;
    s32             frameStyle;
    s32             iconX, iconY;
    s32             x;
    s32             remainingBarWidth;
    s32             currentBarWidth;
    s32             iconIndex;
    HudHpMp*        displayedStats;

    playerStatus = &gPlayerStatus;
    debugState   = Pad_RemapState;
    pendingHp    = 0;
    pendingMp    = 0;
    if (debugState->hideHud != 0) {
        return;
    }

    // Numeric readouts use live values; the bars chase changes one point per draw.
    _hudStepDisplayedStat(&Gp_HpMpWork.hp, playerStatus->hp);
    displayedStats = &Gp_HpMpWork;
    _hudStepDisplayedStat(&displayedStats->mp, playerStatus->mp);

    x  = HUD_STATUS_BLOCK_LEFT;
    y  = HUD_STATUS_BLOCK_TOP;
    y -= gDisplayState.vramYOffset;
    if (gGameSession->hudShakeY > 0) {
        y -= gGameSession->hudShakeY * HUD_STATUS_BLOCK_SHAKE_SCALE;
    }

    // Show the prospective cast spend against the animated bar totals.
    if (playerStatus->statusFlags & PLAYER_STATUS_BERSERKER) {
        pendingHp = hud->previewCastCost << 1;
    } else {
        pendingMp = hud->previewCastCost;
    }

    colorRgb = HUD_STATUS_BLOCK_TEXT_COLOR;
    hp       = Gp_HpMpWork.hp;
    mp       = Gp_HpMpWork.mp;

    _hudDrawHpMpValue(x + 0x2B, y + 0xA, colorRgb, playerStatus->hp);
    _hudDrawHpMpValue(x + 0x56, y + 0xA, colorRgb, playerStatus->mp);

    {
        UiObject labelPanel;

        labelPanel.panel.otIndex.signedValue          = HUD_STATUS_BLOCK_LABEL_PANEL_OT_INDEX;
        labelPanel.panel.contentOriginX.unsignedValue = 0;
        labelPanel.panel.contentOriginY.unsignedValue = 0;
        labelPanel.panel.state                        = USER_INTERFACE_PANEL_INITIAL;
        _hudDrawHpMpLabels(&labelPanel.panel, x, y, colorRgb);
    }

    // Green is the retained amount; yellow is the pending spend.
    if (hp > 0) {
        if (playerStatus->hpMax > 0) {
            if (hp >= pendingHp) {
                remainingBarWidth = (hp - pendingHp) * HUD_STATUS_BAR_PIXELS / playerStatus->hpMax;
                if (remainingBarWidth >= HUD_STATUS_BAR_PIXELS + 1) {
                    remainingBarWidth = HUD_STATUS_BAR_PIXELS;
                } else if (remainingBarWidth < 0) {
                    remainingBarWidth = 0;
                }
            } else {
                remainingBarWidth = 0;
            }
            currentBarWidth = hp * HUD_STATUS_BAR_PIXELS / playerStatus->hpMax;
            if (currentBarWidth >= HUD_STATUS_BAR_PIXELS + 1) {
                currentBarWidth = HUD_STATUS_BAR_PIXELS;
            }
            if (remainingBarWidth > 0) {
                tile           = gGpuPrimCursor;
                gGpuPrimCursor = tile + 1;
                tile->x0       = x + 5;
                tile->y0       = y + 0xE;
                tile->h        = HUD_STATUS_BAR_HEIGHT;
                setlen(tile, HUD_STATUS_BAR_TILE_WORDS);
                GPU_PRIMITIVE_COLOR_WORD(tile, 0) = HUD_STATUS_BAR_REMAINING_COLOR;
                setcode(tile, HUD_STATUS_BAR_TILE_CODE);
                tile->w = remainingBarWidth;
                addPrim(gGpuCurrentOt + HUD_STATUS_BLOCK_OT_INDEX, tile);
            }
            if (currentBarWidth - remainingBarWidth > 0) {
                tile           = gGpuPrimCursor;
                gGpuPrimCursor = tile + 1;
                {
                    s32 tileX = remainingBarWidth + 5;
                    tile->x0  = x + tileX;
                }
                tile->y0                          = y + 0xE;
                tile->h                           = HUD_STATUS_BAR_HEIGHT;
                GPU_PRIMITIVE_COLOR_WORD(tile, 0) = HUD_STATUS_BAR_COST_COLOR;
                setlen(tile, HUD_STATUS_BAR_TILE_WORDS);
                setcode(tile, HUD_STATUS_BAR_TILE_CODE);
                tile->w = currentBarWidth - remainingBarWidth;
                addPrim(gGpuCurrentOt + HUD_STATUS_BLOCK_OT_INDEX, tile);
            }
        }
    }

    if (playerStatus->mpMax <= 0) {
        remainingBarWidth = 0;
        currentBarWidth   = remainingBarWidth;
    } else {
        if (mp >= pendingMp) {
            remainingBarWidth = (mp - pendingMp) * HUD_STATUS_BAR_PIXELS / playerStatus->mpMax;
            if (remainingBarWidth >= HUD_STATUS_BAR_PIXELS + 1) {
                remainingBarWidth = HUD_STATUS_BAR_PIXELS;
            } else if (remainingBarWidth < 0) {
                remainingBarWidth = 0;
            }
        } else {
            remainingBarWidth = 0;
        }
        currentBarWidth = mp * HUD_STATUS_BAR_PIXELS / playerStatus->mpMax;
        if (currentBarWidth >= HUD_STATUS_BAR_PIXELS + 1) {
            currentBarWidth = HUD_STATUS_BAR_PIXELS;
        }
    }
    if (remainingBarWidth > 0) {
        tile           = gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        tile->x0       = x + 0x30;
        tile->y0       = y + 0xE;
        tile->h        = HUD_STATUS_BAR_HEIGHT;
        setlen(tile, HUD_STATUS_BAR_TILE_WORDS);
        GPU_PRIMITIVE_COLOR_WORD(tile, 0) = HUD_STATUS_BAR_REMAINING_COLOR;
        setcode(tile, HUD_STATUS_BAR_TILE_CODE);
        tile->w = remainingBarWidth;
        addPrim(gGpuCurrentOt + HUD_STATUS_BLOCK_OT_INDEX, tile);
    }
    if (currentBarWidth - remainingBarWidth > 0) {
        tile           = gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        {
            s32 tileX = remainingBarWidth + 0x30;
            tile->x0  = x + tileX;
        }
        tile->y0                          = y + 0xE;
        tile->h                           = HUD_STATUS_BAR_HEIGHT;
        GPU_PRIMITIVE_COLOR_WORD(tile, 0) = HUD_STATUS_BAR_COST_COLOR;
        setlen(tile, HUD_STATUS_BAR_TILE_WORDS);
        setcode(tile, HUD_STATUS_BAR_TILE_CODE);
        tile->w = currentBarWidth - remainingBarWidth;
        addPrim(gGpuCurrentOt + HUD_STATUS_BLOCK_OT_INDEX, tile);
    }

    {
        // The atlas caps use fixed 8-pixel packets in SPRT-sized arena slots.
        s32 capLeft     = x + 4;
        s32 frameTop    = y + 0xB;
        s32 frameClut   = HUD_STATUS_BAR_FRAME_CLUT;
        s32 capRight    = x + 0x23;
        s32 frameBottom = y + 0x13;

        hpCapSprite       = gGpuPrimCursor;
        gGpuPrimCursor    = hpCapSprite + 1;
        hpCapSprite->x0   = capLeft;
        hpCapSprite->y0   = frameTop;
        hpCapSprite->u0   = HUD_STATUS_BAR_FRAME_LEFT_U;
        hpCapSprite->v0   = HUD_STATUS_BAR_FRAME_TOP_V;
        hpCapSprite->clut = frameClut;
        setlen(hpCapSprite, HUD_STATUS_BAR_CAP_WORDS);
        setcode(hpCapSprite, HUD_STATUS_BAR_RAW_SPRITE8_CODE);
        addPrim(gGpuCurrentOt + HUD_STATUS_BLOCK_OT_INDEX, hpCapSprite);

        hpCapSprite       = gGpuPrimCursor;
        gGpuPrimCursor    = hpCapSprite + 1;
        hpCapSprite->x0   = capRight;
        hpCapSprite->y0   = frameTop;
        hpCapSprite->u0   = HUD_STATUS_BAR_FRAME_RIGHT_U;
        hpCapSprite->v0   = HUD_STATUS_BAR_FRAME_TOP_V;
        hpCapSprite->clut = frameClut;
        setlen(hpCapSprite, HUD_STATUS_BAR_CAP_WORDS);
        setcode(hpCapSprite, HUD_STATUS_BAR_RAW_SPRITE8_CODE);
        addPrim(gGpuCurrentOt + HUD_STATUS_BLOCK_OT_INDEX, hpCapSprite);

        hpFrameQuad        = gGpuPrimCursor;
        gGpuPrimCursor     = hpFrameQuad + 1;
        hpFrameQuad->x2    = x + 0xC;
        hpFrameQuad->x0    = x + 0xC;
        hpFrameQuad->x3    = capRight;
        hpFrameQuad->x1    = capRight;
        hpFrameQuad->y1    = frameTop;
        hpFrameQuad->y0    = frameTop;
        hpFrameQuad->y3    = frameBottom;
        hpFrameQuad->y2    = frameBottom;
        hpFrameQuad->u0    = HUD_STATUS_BAR_FRAME_MIDDLE_U;
        hpFrameQuad->v0    = HUD_STATUS_BAR_FRAME_TOP_V;
        hpFrameQuad->u1    = HUD_STATUS_BAR_FRAME_RIGHT_U;
        hpFrameQuad->v1    = HUD_STATUS_BAR_FRAME_TOP_V;
        hpFrameQuad->u2    = HUD_STATUS_BAR_FRAME_MIDDLE_U;
        hpFrameQuad->v2    = HUD_STATUS_BAR_FRAME_BOTTOM_V;
        hpFrameQuad->u3    = HUD_STATUS_BAR_FRAME_RIGHT_U;
        hpFrameQuad->v3    = HUD_STATUS_BAR_FRAME_BOTTOM_V;
        hpFrameQuad->clut  = frameClut;
        hpFrameQuad->tpage = HUD_STATUS_BAR_FRAME_TPAGE;
        setlen(hpFrameQuad, HUD_STATUS_BAR_QUAD_WORDS);
        setcode(hpFrameQuad, HUD_STATUS_BAR_RAW_QUAD_CODE);
        addPrim(gGpuCurrentOt + HUD_STATUS_BLOCK_OT_INDEX, hpFrameQuad);

        mpCapSprite       = gGpuPrimCursor;
        mpCapSprite->x0   = capLeft;
        gGpuPrimCursor    = mpCapSprite + 1;
        mpCapSprite->y0   = frameTop;
        mpCapSprite->u0   = HUD_STATUS_BAR_FRAME_LEFT_U;
        mpCapSprite->v0   = HUD_STATUS_BAR_FRAME_TOP_V;
        mpCapSprite->clut = frameClut;
        mpCapSprite->x0  += 0x2B;
        setlen(mpCapSprite, HUD_STATUS_BAR_CAP_WORDS);
        setcode(mpCapSprite, HUD_STATUS_BAR_RAW_SPRITE8_CODE);
        addPrim(gGpuCurrentOt + HUD_STATUS_BLOCK_OT_INDEX, mpCapSprite);

        mpCapSprite     = gGpuPrimCursor;
        mpCapSprite->x0 = capRight;
        gGpuPrimCursor  = mpCapSprite + 1;
        mpCapSprite->u0 = HUD_STATUS_BAR_FRAME_RIGHT_U;
        mpCapSprite->v0 = HUD_STATUS_BAR_FRAME_TOP_V;
        setlen(mpCapSprite, HUD_STATUS_BAR_CAP_WORDS);
        setcode(mpCapSprite, HUD_STATUS_BAR_RAW_SPRITE8_CODE);
        mpCapSprite->x0  += 0x2B;
        mpCapSprite->y0   = frameTop;
        mpCapSprite->clut = frameClut;
        addPrim(gGpuCurrentOt + HUD_STATUS_BLOCK_OT_INDEX, mpCapSprite);

        mpFrameQuad        = gGpuPrimCursor;
        gGpuPrimCursor     = mpFrameQuad + 1;
        mpFrameQuad->x2    = x + 0x37;
        mpFrameQuad->x0    = x + 0x37;
        mpFrameQuad->x3    = x + 0x4E;
        mpFrameQuad->x1    = x + 0x4E;
        mpFrameQuad->y1    = frameTop;
        mpFrameQuad->y0    = frameTop;
        mpFrameQuad->y3    = frameBottom;
        mpFrameQuad->y2    = frameBottom;
        mpFrameQuad->u0    = HUD_STATUS_BAR_FRAME_MIDDLE_U;
        mpFrameQuad->v0    = HUD_STATUS_BAR_FRAME_TOP_V;
        mpFrameQuad->u1    = HUD_STATUS_BAR_FRAME_RIGHT_U;
        mpFrameQuad->v1    = HUD_STATUS_BAR_FRAME_TOP_V;
        mpFrameQuad->u2    = HUD_STATUS_BAR_FRAME_MIDDLE_U;
        mpFrameQuad->v2    = HUD_STATUS_BAR_FRAME_BOTTOM_V;
        mpFrameQuad->u3    = HUD_STATUS_BAR_FRAME_RIGHT_U;
        mpFrameQuad->v3    = HUD_STATUS_BAR_FRAME_BOTTOM_V;
        mpFrameQuad->clut  = frameClut;
        mpFrameQuad->tpage = HUD_STATUS_BAR_FRAME_TPAGE;
        setlen(mpFrameQuad, HUD_STATUS_BAR_QUAD_WORDS);
        setcode(mpFrameQuad, HUD_STATUS_BAR_RAW_QUAD_CODE);
        addPrim(gGpuCurrentOt + HUD_STATUS_BLOCK_OT_INDEX, mpFrameQuad);
    }

    texturePage    = gGpuPrimCursor;
    gGpuPrimCursor = texturePage + 1;
    setlen(texturePage, 1);
    texturePage->code[0] = HUD_STATUS_BAR_TEXTURE_PAGE_COMMAND;
    addPrim(gGpuCurrentOt + HUD_STATUS_BLOCK_OT_INDEX, texturePage);

    {
        RECT rect;

        rect.x     = x;
        rect.y     = y;
        rect.w     = HUD_STATUS_BLOCK_WIDTH;
        rect.h     = HUD_STATUS_BLOCK_HEIGHT;
        frameStyle = USER_INTERFACE_PANEL_TITLE_STYLE;
        if (playerStatus->statusFlags != 0) {
            frameStyle = HUD_STATUS_BLOCK_EFFECTS_FRAME_STYLE;
        }
        uiDrawRectFrame(&rect, HUD_STATUS_BLOCK_FRAME_OT_INDEX, frameStyle, NULL);
    }

    if (playerStatus->statusFlags != 0) {
        iconX = x;
        iconY = y + HUD_STATUS_BLOCK_HEIGHT;
        {
            // Pack active effects from left to right in their atlas order.
            u16 iconStatusMasks[7] = {
                PLAYER_STATUS_DARKNESS,
                PLAYER_STATUS_PARALYSIS,
                PLAYER_STATUS_POISON,
                PLAYER_STATUS_SILENCE,
                HUD_STATUS_UNIDENTIFIED_TIMED_EFFECT, // Gameplay meaning is unproven
                PLAYER_STATUS_CONFUSION,
                PLAYER_STATUS_BERSERKER,
            };

            for (iconIndex = 0; iconIndex < ARRAY_SIZE(iconStatusMasks); iconIndex++) {
                if (playerStatus->statusFlags & iconStatusMasks[iconIndex]) {
                    statusSprite       = gGpuPrimCursor;
                    gGpuPrimCursor     = statusSprite + 1;
                    statusSprite->x0   = iconX;
                    iconX             += HUD_STATUS_ICON_STRIDE;
                    statusSprite->y0   = iconY;
                    statusSprite->u0   = iconIndex * HUD_STATUS_ICON_TEXTURE_STRIDE + HUD_STATUS_ICON_FIRST_U;
                    statusSprite->w    = HUD_STATUS_ICON_PIXELS;
                    statusSprite->h    = HUD_STATUS_ICON_PIXELS;
                    statusSprite->v0   = HUD_STATUS_ICON_V;
                    statusSprite->clut = HUD_STATUS_ICON_CLUT;
                    setlen(statusSprite, HUD_STATUS_ICON_WORDS);
                    setcode(statusSprite, HUD_STATUS_ICON_RAW_SPRITE_CODE);
                    addPrim(gGpuCurrentOt + HUD_STATUS_BLOCK_OT_INDEX, statusSprite);
                }
            }
        }
        uiQueueTexturePage(HUD_STATUS_BLOCK_OT_INDEX, 0);
    }

    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] != NULL) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType != HUD_STATUS_COMPANION_NO_READOUT_FAMILY) {
            hudDrawHpReadout(0x2D, HUD_STATUS_BLOCK_TOP, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHpMax, HUD_HP_READOUT_COMPANION);
        }
    }
}

void hudDrawRadarMarker(s32 centerX, s32 centerY, s32 markerKind)
{
    enum {
        HUD_RADAR_MARKER_ANCHOR_X                = 6,
        HUD_RADAR_MARKER_ANCHOR_Y                = 8,
        HUD_RADAR_MARKER_CLUT                    = 0x3C0D,
        HUD_RADAR_MARKER_PLAYER_U                = 0xA0,
        HUD_RADAR_MARKER_PLAYER_V                = 0x88,
        HUD_RADAR_MARKER_ENEMY_U                 = 0xA8,
        HUD_RADAR_MARKER_ENEMY_V                 = 0x80,
        HUD_RADAR_MARKER_TARGETED_U              = 0xA0,
        HUD_RADAR_MARKER_TARGETED_V              = 0x80,
        HUD_RADAR_MARKER_PACKET_WORDS            = 3,
        HUD_RADAR_MARKER_RAW_TRANSLUCENT_SPRITE8 = 0x77,
        HUD_RADAR_MARKER_OT_INDEX                = -2
    };
    SPRT_8* marker;
    s32     orderingOffset;

    orderingOffset = 0;
    centerX       -= HUD_RADAR_MARKER_ANCHOR_X;
    marker         = gGpuPrimCursor;
    centerY       -= HUD_RADAR_MARKER_ANCHOR_Y;
    gGpuPrimCursor = marker + 1;
    marker->x0     = centerX;
    marker->y0     = centerY;
    switch (markerKind) {
        case HUD_RADAR_MARKER_PLAYER:
            marker->u0 = HUD_RADAR_MARKER_PLAYER_U;
            marker->v0 = HUD_RADAR_MARKER_PLAYER_V;
            break;
        case HUD_RADAR_MARKER_ENEMY:
            marker->u0 = HUD_RADAR_MARKER_ENEMY_U;
            marker->v0 = HUD_RADAR_MARKER_ENEMY_V;
            break;
        case HUD_RADAR_MARKER_TARGETED:
        default:
            orderingOffset = -1;
            marker->u0     = HUD_RADAR_MARKER_TARGETED_U;
            marker->v0     = HUD_RADAR_MARKER_TARGETED_V;
            break;
    }
    marker->clut = HUD_RADAR_MARKER_CLUT;
    setlen(marker, HUD_RADAR_MARKER_PACKET_WORDS);
    setcode(marker, HUD_RADAR_MARKER_RAW_TRANSLUCENT_SPRITE8);
    addPrim(gGpuCurrentOt + orderingOffset + HUD_RADAR_MARKER_OT_INDEX, marker);
}
