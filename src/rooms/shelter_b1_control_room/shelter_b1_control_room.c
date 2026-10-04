#include "rooms/shelter_b1_control_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

/// How `_ShelterB1ControlRoomMirrorConfig::mode` builds the reflected frame.
enum {
    SHELTER_B1_CONTROL_ROOM_MIRROR_MODE_PLANE = 0, // Reflect through the plane given by `normal` and `offset`
    SHELTER_B1_CONTROL_ROOM_MIRROR_MODE_FLOOR = 1, // Copy the view frame with its second row negated
};

/// `_ShelterB1ControlRoomMirrorConfig::firstBlendMode` value that draws no
/// overlay quads. Any negative value has that effect.
enum { SHELTER_B1_CONTROL_ROOM_MIRROR_NO_OVERLAY = -1 };

/// What the room's mirror task reflects, and how, for the current location.
///
/// The task keeps one of these in its work block and has it refilled from the
/// location key as it starts and again whenever the view moves, so every field
/// describes the view being shown. The reflection is a second copy of
/// `subject`'s model drawn through a mirrored coordinate frame. Over the
/// screen rectangle that copy covers, the task also draws quads textured from
/// a saved copy of the frame buffer, once for each GPU blend mode from
/// `firstBlendMode` up to `GPU_BLEND_SUBTRACT`.
///
/// A floor mirror raises its frame by `normal.vy`, which no floor configuration
/// sets. The one that supplies a height writes it to `offset.vy`, which a floor
/// mirror never reads; whether that was intended is unproven.
typedef struct {
    s32     active;          // 1 while this view shows the reflection; otherwise the copy is hidden
    s32     copyPending;     // 1 requests a frame-buffer copy into the off-screen strip; cleared once queued
    s32     firstBlendMode;  // First `GPU_BLEND_*` mode of the overlay quads; negative draws none
    s32     mode;            // Frame construction (0 plane, 1 floor)
    s32     subjectIsPlayer; // 1 makes every refill select the player task; 0 keeps the location's subject
    s32     stripX;          // VRAM x of the 320x240 off-screen strip at y = 0x100
    s32     disabled;        // 1 where the location's layout has no reflection; the task exits as it starts
    Task*   subject;         // Borrowed task whose model and pose the reflection copies
    SVECTOR normal;          // Plane mirror's unit normal, 4096 = 1.0
    SVECTOR offset;          // Point on the mirror plane, relative to the view frame's position
} _ShelterB1ControlRoomMirrorConfig;
STATIC_ASSERT_SIZEOF(_ShelterB1ControlRoomMirrorConfig, 0x30);

/// Work block the control-room mirror task parks in `Task::work`.
///
/// The task draws another copy of the configured subject's model, parented to
/// `coord`. A floor mirror copies the view frame and negates its second row;
/// a plane mirror reflects the view through the configured plane. The frame is
/// rebuilt only when `viewRebuildStamp` differs from the view's masked
/// composition stamp, so a change of visit parity alone leaves it in place.
/// The stamp starts at -1. Startup and each rebuild refill `cfg` from the location.
typedef struct {
    s32                               viewRebuildStamp; // Masked `GfxCoord::composeStamp` last rebuilt from; -1 rebuilds on the next update
    GfxCoord                          coord;            // Reflected frame the clone's root part is parented to
    MATRIX                            light;            // Light matrix the clone is drawn with
    MATRIX                            color;            // Colour matrix the clone is drawn with
    s16                               clipLeft;         // Left screen edge the overlay may cover, in pixels from centre
    s16                               clipRight;        // Right screen edge, in pixels from centre
    s16                               clipTop;          // Top screen edge, in pixels from centre
    s16                               clipBottom;       // Bottom screen edge, in pixels from centre
    byte                              unknown_9C[4];    // No recovered access; role unproven
    _ShelterB1ControlRoomMirrorConfig cfg;              // Mirror configuration for the current location
} _ShelterB1ControlRoomMirrorWork;
STATIC_ASSERT_SIZEOF(_ShelterB1ControlRoomMirrorWork, 0xD0);

/// Which axis `_ShelterB1ControlRoomMirrorScratch::leastAxis` names while a
/// plane mirror builds its reflected frame. Each projection then overwrites
/// that word with the GTE flag, which is never tested.
enum {
    SHELTER_B1_CONTROL_ROOM_MIRROR_AXIS_X = 0, // The normal's X component is the smallest
    SHELTER_B1_CONTROL_ROOM_MIRROR_AXIS_Y = 1, // The normal's Y component is the smallest
    SHELTER_B1_CONTROL_ROOM_MIRROR_AXIS_Z = 2, // The normal's Z component is the smallest
};

/// Distances and screen limits for the control-room mirror's overlay quad.
///
/// Sample offsets are game coordinates in model space, which is Y-down.
/// Screen values are pixels from the centre of the 320 by 240 frame.
enum {
    SHELTER_B1_CONTROL_ROOM_MIRROR_EVENT_HEAD_OFFSET    = -0x3E8, // Local Y of the head sample on part 1 during a scripted event
    SHELTER_B1_CONTROL_ROOM_MIRROR_EVENT_FOOT_OFFSET    = 0x3E8,  // Local Y of the foot sample on part 1; the opposite side of the head
    SHELTER_B1_CONTROL_ROOM_MIRROR_ROOT_HEAD_OFFSET     = -0x7D0, // Local Y of the root part's head sample; the foot sample is the origin
    SHELTER_B1_CONTROL_ROOM_MIRROR_SCREEN_EDGE_PAD      = 0x10,   // Pixels added beyond each ordered screen edge before the quad is measured
    SHELTER_B1_CONTROL_ROOM_MIRROR_QUAD_HALF_SPAN_LIMIT = 0x60,   // Half-width is kept to one pixel below this span
    SHELTER_B1_CONTROL_ROOM_MIRROR_QUAD_HALF_WIDTH_MAX  = 0x5F,   // Largest half-width of the overlay quad, in pixels
    SHELTER_B1_CONTROL_ROOM_MIRROR_TEXTURE_PAGE_MASK    = 0xFFC0, // 64-pixel alignment a 16-bit texture page requires
    SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_WIDTH     = 0xA0,   // Half of the 320-pixel frame
    SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_HEIGHT    = 0x78,   // Half of the 240-pixel frame
    SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_LEFT           = -SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_WIDTH,
    SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_RIGHT          = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_WIDTH,
    SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_TOP            = -SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_HEIGHT,
    SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_BOTTOM         = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_HEIGHT,
};

/// Scratchpad block the control-room mirror reserves for one frame.
///
/// A plane mirror first builds its reflected frame here. `refAxis` is the
/// coordinate axis least aligned with the plane normal, chosen by comparing
/// absolute components in `leastAbs` and `axisAbs` and recording the axis in
/// `leastAxis`. `basis` is the orthonormal frame from the normal and that
/// axis, `reflect` the reflection matrix derived from it, and `offset` the
/// plane point rotated into that frame and subtracted from the clone's
/// translation. That frame is copied onto the clone, after which `basis` is
/// only a temporary for the clone's light matrix.
///
/// The same block then places the overlay quads. Those fields have the same
/// roles as `_PlanarReflectionExtentScratch` — a local sample, an unread depth
/// cue and GTE flag, head and foot screen points and quarter-depths, the
/// texture-page X and the screen rectangle — packed after the plane basis in
/// a different order, so the two types stay separate. The GTE flag reuses
/// `leastAxis` once the axis has been consumed. During a scripted event the
/// samples are on part 1, 1000 game units either side of its origin; otherwise
/// they are on the root, 2000 units along negative local Y and at the origin.
/// Model space is Y-down, so the negative-Y sample is the upper end and is
/// stored as the head. Screen Y is ordered so `screenFoot` is the upper edge
/// and `screenHead` the lower, and each edge is padded by 16 pixels. The swap
/// uses `screenHead.vx` as its temporary. The quad is centred on the foot
/// sample's screen X. Its half-width is half the padded vertical span, clamped
/// to 95 pixels, and the rectangle is clamped to the frame.
///
/// `unknown_5A` and `unknown_5E` have no recovered access.
typedef struct {
    SVECTOR point;             // Sample in the part's local space; X and Z stay 0, and the unused halfword is left unchanged
    SVECTOR refAxis;           // Unit axis least aligned with the plane normal, 4096 = 1; the hint for the orthonormal frame
    MATRIX  basis;             // Orthonormal frame from the normal and `refAxis`; afterwards a temporary for the clone's light
    MATRIX  reflect;           // Reflection of the view through the plane; copied into the clone's coordinate frame
    SVECTOR offset;            // Plane point rotated into `reflect`, then subtracted from the frame's translation
    s16     leastAbs;          // Smallest absolute normal component seen so far, 4096 = 1
    byte    unknown_5A[2];     // No recovered access; role unproven
    s16     axisAbs;           // Absolute value of the normal component being compared, 4096 = 1
    byte    unknown_5E[2];     // No recovered access; role unproven
    DVECTOR screenFoot;        // Foot sample's screen pixels; vy is ordered to the upper edge, vx centres the quad
    DVECTOR screenHead;        // Head sample's screen pixels; vy is ordered to the lower edge, vx is the swap temporary
    u16     texturePageX;      // 64-pixel-aligned VRAM X of the 16-bit page the quads sample. Two alignment bytes follow
    s32     depthCue;          // GTE IR0 of the latest projection, with 12 fractional bits; written and never read
    s32     leastAxis;         // 0 X, 1 Y, 2 Z while the frame is built; then the unread GTE flag of the latest projection
    s32     orderingDepthFoot; // Foot sample's quarter-depth; the depth the overlay quads are ordered at
    s32     orderingDepthHead; // Head sample's quarter-depth; written and never read
    s32     left;              // Quad left edge, pixels from screen centre, clamped to -160
    s32     right;             // Quad right edge, pixels from screen centre, clamped to 160
    s32     top;               // Quad top edge, pixels from screen centre, clamped to -120
    s32     bottom;            // Quad bottom edge, pixels from screen centre, clamped to 120
} _ShelterB1ControlRoomMirrorScratch;
STATIC_ASSERT_SIZEOF(_ShelterB1ControlRoomMirrorScratch, 0x8C);

extern void             func_actor_150400_80131FB8(void);
extern TaskMessageEntry D_shelter_b1_control_room_80181B94[];
extern EvsCommand       D_actor_150400_80132D70[];
extern EvsCommand       D_actor_150400_80133088[];

static void func_shelter_b1_control_room_8017D600(Task* task, _ShelterB1ControlRoomMirrorConfig* cfg);

s32 func_shelter_b1_control_room_8017ECCC(Task*, s32, s32, s32);
s32 func_shelter_b1_control_room_8017ECD4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b1_control_room_8017ED68(Task*, s32, s32, s32);
s32 func_shelter_b1_control_room_8017EE24(Task*, s32, s32, s32);

void func_shelter_b1_control_room_8017D7B8(Task*);

TaskDesc D_shelter_b1_control_room_80181B88 = { { { TASK_BODY_NONE, 112 } }, func_shelter_b1_control_room_8017D7B8, { .value = 0 } };

TaskMessageEntry D_shelter_b1_control_room_80181B94[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_control_room_8017ECD4 },
    { 5105, func_shelter_b1_control_room_8017ECCC },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b1_control_room_8017EE24 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_control_room_8017ED68 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static inline void _applyMatrixSV(MATRIX* m, SVECTOR* v, SVECTOR* out);
static void        func_shelter_b1_control_room_8017EE2C(Task* arg0);
static void        func_shelter_b1_control_room_8017EEBC(Task* task);

/// Applies `m` to `v` through the GTE and stores the result in `out`.
static inline void _applyMatrixSV(MATRIX* m, SVECTOR* v, SVECTOR* out)
{
    gte_SetRotMatrix(m);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(out);
}

/// Fills in `cfg` for the current area key.
///
/// Every field starts from a default that leaves the mirror inactive, with the
/// player task (`gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)`) as its subject. Three places turn it on,
/// each for a set of views: area 7 of stage 5 while the session is in room 2,
/// area 0x1E of stages 2 and 3, and area 0x12 of stage 4. In stage 4's area the
/// subject becomes the `Gp_LookupSlot4(0)` task instead, and only when the
/// key's `variant` is 0xB; with any other `variant`, `disabled` is set so the
/// mirror task exits on its first frame.
///
/// The `do { } while (0)` is not logic. Its loop notes act as a scheduling
/// barrier: without it, the scheduler would move the shared constant 1 down to
/// its first store, below the key reads.
static void func_shelter_b1_control_room_8017D600(Task* task, _ShelterB1ControlRoomMirrorConfig* cfg)
{
    s32              stage;
    s32              area;
    s32              view;
    GameLocationKey* key;
    s32              one;

    key = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc;
    one = 1;
    do {
        stage = key->stage;
        area  = key->area;
        view  = key->view;
    } while (0);
    cfg->stripX          = 0x1C0;
    cfg->active          = 0;
    cfg->copyPending     = 0;
    cfg->firstBlendMode  = GPU_BLEND_AVERAGE;
    cfg->mode            = one;
    cfg->offset.vy       = 0;
    cfg->subjectIsPlayer = one;
    cfg->disabled        = 0;
    switch (stage) {
        case 5:
            if (area == 7 && (u32)(view - 6) < 6 && gGameSession->location.loc.room == 2) {
                cfg->offset.vy   = 0x9B;
                cfg->active      = 1;
                cfg->copyPending = 1;
            }
            break;
        case 4:
            if (area == 0x12) {
                if (key->variant == 0xB) {
                    cfg->subjectIsPlayer = 0;
                    cfg->subject         = Gp_LookupSlot4(0);
                    if (view == 4 || view == 1) {
                        cfg->normal.vz      = -0x1000;
                        cfg->offset.vz      = -0xABE;
                        cfg->mode           = SHELTER_B1_CONTROL_ROOM_MIRROR_MODE_PLANE;
                        cfg->normal.vx      = 0;
                        cfg->normal.vy      = 0;
                        cfg->offset.vx      = 0;
                        cfg->offset.vy      = 0;
                        cfg->active         = 1;
                        cfg->copyPending    = 1;
                        cfg->stripX         = 0x140;
                        cfg->firstBlendMode = GPU_BLEND_ADD;
                    }
                } else {
                    cfg->disabled = 1;
                }
            }
            break;
        case 2:
        case 3:
            if (area == 0x1E && (view == 8 || view == 1)) {
                cfg->offset.vx      = 0xA38;
                cfg->firstBlendMode = SHELTER_B1_CONTROL_ROOM_MIRROR_NO_OVERLAY;
                cfg->normal.vx      = 0;
                cfg->normal.vz      = 0;
                cfg->offset.vy      = 0;
                cfg->offset.vz      = 0;
                cfg->active         = 1;
                cfg->copyPending    = 1;
            }
            break;
    }
    if (cfg->subjectIsPlayer == 1) {
        cfg->subject = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    }
}

/// Per-frame update of the room's mirror task.
///
/// On the first frame it attaches the subject's TMD source to this task so the
/// clone draws the same model, hangs the clone's root part from the reflected
/// frame and adopts the subject task. When the view's masked rebuild stamp
/// changes it rebuilds the reflected frame, and on request it copies the frame
/// buffer into the off-screen strip. While active it copies the subject's pose
/// and matrices onto the clone, projects the clone to find its screen rectangle
/// and, where that overlaps the clip rectangle, draws quads sampling the strip.
void func_shelter_b1_control_room_8017D7B8(Task* task)
{
    _ShelterB1ControlRoomMirrorWork*    work;
    _ShelterB1ControlRoomMirrorConfig*  cfg;
    _ShelterB1ControlRoomMirrorScratch* scratch;
    TmdObject*                          model;
    TmdObject*                          src;
    TmdObject*                          body;
    GfxCoord*                           parts;
    GfxCoord*                           refPart;
    GfxCoord*                           from;
    GfxCoord*                           to;
    DR_AREA*                            drArea;
    DR_STP*                             drStp;
    DR_OFFSET*                          drOffset;
    SPRT*                               sprt;
    DR_TPAGE*                           tpage;
    TILE*                               tile;
    POLY_FT4*                           poly;
    s32                                 copyPending;
    s32                                 halfWidth;
    s32                                 texX;
    s32                                 texBase;
    GfxCoord*                           sub;
    s32                                 layer;
    u16                                 ofs[2];
    RECT                                rect;

    if (task->state == 0) {
        work = memCalloc(sizeof(_ShelterB1ControlRoomMirrorWork), 0);
        cfg  = &work->cfg;
        if (work == NULL) {
            goto exit;
        }
        task->work = work;
        func_shelter_b1_control_room_8017D600(task, cfg);
        if (cfg->disabled == 1) {
            goto exit;
        }
        body = cfg->subject->extra.tmd;
        if (Gp_AttachTmd(task, body->source) == NULL) {
        exit:
            Task_CallExit(task);
            return;
        }
        model                    = task->extra.tmd;
        parts                    = model->coords;
        model->clutRowOffset     = body->clutRowOffset;
        model->texturePageOffset = body->texturePageOffset;
        tmdProcessStream(model);
        tmdProcessStream(model);
        model->otOffset        = 0x16;
        model->flags           = TMD_OBJECT_REVERSE_CULLING;
        gGameSession->field_4E = 1;
        parts->parent          = &work->coord;
        model->lightMtx        = &work->light;
        model->colorMtx        = &work->color;
        taskReparent(cfg->subject, task);
        work->viewRebuildStamp = -1;
        task->state++;
    }

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_ShelterB1ControlRoomMirrorScratch);
    model   = task->extra.tmd;
    work    = task->work;
    parts   = model->coords;
    cfg     = &work->cfg;
    if (work->viewRebuildStamp != (gGfxViewCoord.composeStamp & GRAPHICS_COORD_STAMP_MASK)) {
        work->viewRebuildStamp = gGfxViewCoord.composeStamp & GRAPHICS_COORD_STAMP_MASK;
        func_shelter_b1_control_room_8017D600(task, cfg);
        if (work->cfg.active == 1) {
            sub                      = gGfxViewCoord.parent;
            work->clipLeft           = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_LEFT;
            work->clipRight          = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_RIGHT;
            work->clipTop            = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_TOP;
            work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
            work->clipBottom         = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_BOTTOM;
            work->coord.parent       = sub;
            if (cfg->mode == SHELTER_B1_CONTROL_ROOM_MIRROR_MODE_FLOOR) {
                work->coord.coord          = gGfxViewCoord.coord;
                work->coord.coord.m[1][0] *= -1;
                work->coord.coord.m[1][1] *= -1;
                work->coord.coord.m[1][2] *= -1;
                work->coord.coord.t[1]    += cfg->normal.vy;
            } else {
                // Select the coordinate axis least aligned with the plane normal.
                scratch->leastAbs = cfg->normal.vx;
                if (scratch->leastAbs < 0) {
                    scratch->leastAbs = -scratch->leastAbs;
                }
                scratch->leastAxis = SHELTER_B1_CONTROL_ROOM_MIRROR_AXIS_X;
                scratch->axisAbs   = cfg->normal.vy;
                if (scratch->axisAbs < 0) {
                    scratch->axisAbs = -scratch->axisAbs;
                }
                if (scratch->leastAbs > scratch->axisAbs) {
                    scratch->leastAbs  = scratch->axisAbs;
                    scratch->leastAxis = SHELTER_B1_CONTROL_ROOM_MIRROR_AXIS_Y;
                }
                scratch->axisAbs = cfg->normal.vz;
                if (scratch->axisAbs < 0) {
                    scratch->axisAbs = -scratch->axisAbs;
                }
                if (scratch->leastAbs > scratch->axisAbs) {
                    scratch->leastAbs  = scratch->axisAbs;
                    scratch->leastAxis = SHELTER_B1_CONTROL_ROOM_MIRROR_AXIS_Z;
                }
                scratch->refAxis.vx = 0;
                if (scratch->leastAxis == SHELTER_B1_CONTROL_ROOM_MIRROR_AXIS_X) {
                    scratch->refAxis.vx = ONE;
                }
                scratch->refAxis.vy = 0;
                if (scratch->leastAxis == SHELTER_B1_CONTROL_ROOM_MIRROR_AXIS_Y) {
                    scratch->refAxis.vy = ONE;
                }
                scratch->refAxis.vz = 0;
                if (scratch->leastAxis == SHELTER_B1_CONTROL_ROOM_MIRROR_AXIS_Z) {
                    scratch->refAxis.vz = ONE;
                }
                Gfx_OrthonormalBasis(&scratch->basis, &cfg->normal, &scratch->refAxis);
                gte_TransposeMatrix(&scratch->basis, &scratch->reflect);
                scratch->reflect.m[2][0] = -scratch->reflect.m[2][0];
                scratch->reflect.m[2][1] = -scratch->reflect.m[2][1];
                scratch->reflect.m[2][2] = -scratch->reflect.m[2][2];
                gte_MulMatrix0(&scratch->basis, &scratch->reflect, &scratch->reflect);
                work->coord.coord      = scratch->reflect;
                work->coord.coord.t[0] = gGfxViewCoord.coord.t[0] + cfg->offset.vx;
                work->coord.coord.t[1] = gGfxViewCoord.coord.t[1] + cfg->offset.vy;
                work->coord.coord.t[2] = gGfxViewCoord.coord.t[2] + cfg->offset.vz;
                _applyMatrixSV(&scratch->reflect, &cfg->offset, &scratch->offset);
                work->coord.coord.t[0] -= scratch->offset.vx;
                work->coord.coord.t[1] -= scratch->offset.vy;
                work->coord.coord.t[2] -= scratch->offset.vz;
            }
        }
    }

    copyPending = cfg->copyPending;
    if (copyPending == 1 && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
        drArea         = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_AREA);
        rect.x         = 0;
        rect.y         = gDisplayState.drawBuffer * 0x110;
        rect.w         = 0x140;
        rect.h         = 0xF0;
        SetDrawArea(drArea, &rect);
        addPrim(&gGpuCurrentOt[0x3FF], drArea);

        drStp          = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_STP);
        SetDrawStp(drStp, 0);
        addPrim(&gGpuCurrentOt[0x3FF], drStp);

        drOffset       = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_OFFSET);
        ofs[0]         = 0xA0;
        ofs[1]         = gDisplayState.drawBuffer * 0x110 + 0x78;
        SetDrawOffset(drOffset, ofs);
        addPrim(&gGpuCurrentOt[0x3FF], drOffset);

        sprt           = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(SPRT);
        sprt->x0       = -0xA0;
        sprt->y0       = -0x78;
        sprt->w        = 0xA0;
        sprt->h        = 0xF0;
        sprt->u0       = 0;
        sprt->v0       = gDisplayState.drawBuffer << 4;
        setlen(sprt, 4);
        setcode(sprt, 0x65);
        addPrim(&gGpuCurrentOt[0x3FF], sprt);

        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
        setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0, gDisplayState.drawBuffer << 8));
        addPrim(&gGpuCurrentOt[0x3FF], tpage);

        sprt           = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(SPRT);
        sprt->x0       = 0;
        sprt->y0       = -0x78;
        sprt->w        = 0xA0;
        sprt->h        = 0xF0;
        sprt->u0       = 0x20;
        sprt->v0       = gDisplayState.drawBuffer << 4;
        setlen(sprt, 4);
        setcode(sprt, 0x65);
        addPrim(&gGpuCurrentOt[0x3FF], sprt);

        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
        setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0x80, gDisplayState.drawBuffer << 8));
        addPrim(&gGpuCurrentOt[0x3FF], tpage);

        tile           = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(TILE);
        setlen(tile, 3);
        setcode(tile, 0x60);
        tile->x0 = -0xA0;
        tile->y0 = -0x78;
        tile->r0 = tile->g0 = 2;
        tile->b0            = 2;
        tile->w             = 0x140;
        tile->h             = 0xF0;
        addPrim(&gGpuCurrentOt[0x3FF], tile);

        drStp          = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_STP);
        SetDrawStp(drStp, 1);
        addPrim(&gGpuCurrentOt[0x3FF], drStp);

        drOffset       = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_OFFSET);
        ofs[0]         = cfg->stripX + 0xA0;
        ofs[1]         = 0x178;
        SetDrawOffset(drOffset, ofs);
        addPrim(&gGpuCurrentOt[0x3FF], drOffset);

        drArea         = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_AREA);
        rect.x         = cfg->stripX;
        rect.y         = 0x100;
        rect.w         = 0x140;
        rect.h         = 0xF0;
        SetDrawArea(drArea, &rect);
        addPrim(&gGpuCurrentOt[0x3FF], drArea);

        cfg->copyPending = 0;
    }

    if (cfg->active == 1) {
        src           = cfg->subject->extra.tmd;
        refPart       = &task->extra.tmd->coords[1];
        from          = src->coords;
        model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->light   = *src->lightMtx;
        work->color   = *src->colorMtx;
        // basis is scratch from here: the reflected frame, when one was built, already sits on the clone.
        actorRenderComposeCoord(parts);
        gte_TransposeMatrix(&parts->workm, &scratch->basis);
        gte_MulMatrix0(&from->workm, &scratch->basis, &scratch->basis);
        gte_MulMatrix0(&work->light, &scratch->basis, &work->light);
        parts->composeStamp = GRAPHICS_COORD_DIRTY;
        to                  = parts;
        for (layer = 0; layer < (u32)src->partCount; layer++) {
            to->coord = from->coord;
            to++;
            from++;
        }
        if (cfg->firstBlendMode >= 0) {
            // Project a head sample and a foot sample. depthCue, and the GTE flag written over leastAxis, are not read.
            if (gGameSession->eventState != 0) {
                actorRenderComposeCoord(refPart);
                gte_SetTransMatrix(&refPart->workm);
                gte_SetRotMatrix(&refPart->workm);
                scratch->point.vx = 0;
                scratch->point.vy = SHELTER_B1_CONTROL_ROOM_MIRROR_EVENT_HEAD_OFFSET;
                scratch->point.vz = 0;
                gte_RotTransPers(&scratch->point, &scratch->screenHead, &scratch->depthCue, &scratch->leastAxis,
                                 &scratch->orderingDepthHead);
                scratch->point.vx = 0;
                scratch->point.vy = SHELTER_B1_CONTROL_ROOM_MIRROR_EVENT_FOOT_OFFSET;
                scratch->point.vz = 0;
                gte_RotTransPers(&scratch->point, &scratch->screenFoot, &scratch->depthCue, &scratch->leastAxis,
                                 &scratch->orderingDepthFoot);
            } else {
                actorRenderComposeCoord(parts);
                gte_SetTransMatrix(&parts->workm);
                gte_SetRotMatrix(&parts->workm);
                scratch->point.vx = 0;
                scratch->point.vy = SHELTER_B1_CONTROL_ROOM_MIRROR_ROOT_HEAD_OFFSET;
                scratch->point.vz = 0;
                gte_RotTransPers(&scratch->point, &scratch->screenHead, &scratch->depthCue, &scratch->leastAxis,
                                 &scratch->orderingDepthHead);
                scratch->point.vx = 0;
                scratch->point.vy = 0;
                scratch->point.vz = 0;
                gte_RotTransPers(&scratch->point, &scratch->screenFoot, &scratch->depthCue, &scratch->leastAxis,
                                 &scratch->orderingDepthFoot);
            }
            // Order screen Y so the foot slot is the upper edge. screenHead.vx holds the swap.
            if (scratch->screenFoot.vy > scratch->screenHead.vy) {
                scratch->screenHead.vx = scratch->screenFoot.vy;
                scratch->screenFoot.vy = scratch->screenHead.vy;
                scratch->screenHead.vy = scratch->screenHead.vx;
            }
            scratch->screenFoot.vy -= SHELTER_B1_CONTROL_ROOM_MIRROR_SCREEN_EDGE_PAD;
            scratch->screenHead.vy += SHELTER_B1_CONTROL_ROOM_MIRROR_SCREEN_EDGE_PAD;
            halfWidth               = (scratch->screenHead.vy - scratch->screenFoot.vy) >> 1;
            if (halfWidth >= SHELTER_B1_CONTROL_ROOM_MIRROR_QUAD_HALF_SPAN_LIMIT) {
                halfWidth = SHELTER_B1_CONTROL_ROOM_MIRROR_QUAD_HALF_WIDTH_MAX;
            }
            scratch->left = scratch->screenFoot.vx - halfWidth;
            if (scratch->left < SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_LEFT) {
                scratch->left = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_LEFT;
            }
            scratch->right = scratch->screenFoot.vx + halfWidth;
            if (scratch->right > SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_RIGHT) {
                scratch->right = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_RIGHT;
            }
            scratch->top = scratch->screenFoot.vy;
            if (scratch->top < SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_TOP) {
                scratch->top = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_TOP;
            }
            scratch->bottom = scratch->screenHead.vy;
            if (scratch->bottom > SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_BOTTOM) {
                scratch->bottom = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_BOTTOM;
            }
            if (scratch->top < work->clipBottom && work->clipTop < scratch->bottom && scratch->left < work->clipRight &&
                work->clipLeft < scratch->right) {
                DR_TPAGE* mode;

                mode                  = gGpuPrimCursor;
                texBase               = cfg->stripX + SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_WIDTH;
                texX                  = scratch->left + texBase;
                scratch->texturePageX = texX & SHELTER_B1_CONTROL_ROOM_MIRROR_TEXTURE_PAGE_MASK;
                gGpuPrimCursor        = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
                setDrawTPage(mode, 0, 1, 0);
                addPrim(&gGpuCurrentOt[(((scratch->orderingDepthFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + model->otOffset - 15],
                        mode);
                for (layer = cfg->firstBlendMode; layer <= GPU_BLEND_SUBTRACT; layer++) {
                    poly           = gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(POLY_FT4);
                    setPolyFT4(poly);
                    setSemiTrans(poly, 1);
                    if (cfg->firstBlendMode == GPU_BLEND_ADD) {
                        setShadeTex(poly, 0);
                        poly->r0 = poly->g0 = poly->b0 = 0x80;
                    } else {
                        setShadeTex(poly, 1);
                    }
                    poly->x0 = poly->x2 = scratch->left;
                    poly->x1 = poly->x3 = scratch->right;
                    poly->y0 = poly->y1 = scratch->top;
                    poly->y2 = poly->y3 = scratch->bottom;
                    poly->tpage         = getTPage(2, layer, scratch->texturePageX, 0x100);
                    poly->u0 = poly->u2 = poly->x0 + SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_WIDTH + cfg->stripX - scratch->texturePageX;
                    poly->u1 = poly->u3 = poly->x1 + SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_WIDTH + cfg->stripX - scratch->texturePageX;
                    poly->v0 = poly->v1 = poly->y0 + SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_HEIGHT;
                    poly->v2 = poly->v3 = poly->y2 + SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_HEIGHT;
                    addPrim(&gGpuCurrentOt[(((scratch->orderingDepthFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + model->otOffset - 15],
                            poly);
                }
                mode           = gGpuPrimCursor;
                gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
                setDrawTPage(mode, 0, 0, 0);
                addPrim(&gGpuCurrentOt[(((scratch->orderingDepthFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + model->otOffset - 15],
                        mode);
            }
        }
    } else {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_ShelterB1ControlRoomMirrorScratch);
}

s32 func_shelter_b1_control_room_8017ECCC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_control_room_8017ECD4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->areaId != GAME_AREA_SHELTER_B1_ACCESS_TUNNEL) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_B1_CONTROL_ROOM_TUNNEL_DOOR_UNLOCKED) != 0) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 0;
    }
    Gp_SetNibbleIf(in->flagId, 2);
    Gp_RunCapCmd1(1);
    return 0;
}

s32 func_shelter_b1_control_room_8017ED68(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 3:
            if (gameFlagGetNibble(GAME_FLAG_CONTROL_ROOM_RETURN_TAKEN) != 0) {
                Gp_RunCapCmd1(6);
            } else {
                Gp_RunCapCmd1(3);
            }
            break;
        case 4:
            if (gameFlagGetNibble(GAME_FLAG_CONTROL_ROOM_RETURN_TAKEN) != 0) {
                Gp_RunCapCmd1(7);
            } else {
                Gp_RunCapCmd1(4);
            }
            break;
        case 5:
            if (gameFlagGetNibble(GAME_FLAG_CONTROL_ROOM_RETURN_TAKEN) != 0) {
                Gp_RunCapCmd1(8);
            } else {
                Gp_RunCapCmd1(5);
            }
            break;
        case 9:
            if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) == 6) {
                Gp_RunCapCmd1(9);
            }
            break;
    }
    return 0;
}

s32 func_shelter_b1_control_room_8017EE24(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

static void func_shelter_b1_control_room_8017EE2C(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_control_room_80181B94;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == 0xB) {
        func_actor_150400_80131FB8();
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 9) {
            func_800E8634(D_actor_150400_80132D70, 0, D_actor_150400_80133088);
        }
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Idle state of the room task: does nothing, and only reserves a 0x10-byte
/// stack frame.
static void func_shelter_b1_control_room_8017EEBC(Task* task)
{
    char pad[0x10];
}

/// States of the room task `func_shelter_b1_control_room_8017EECC`: the setup
/// state `func_shelter_b1_control_room_8017EE2C`, the idle state
/// `func_shelter_b1_control_room_8017EEBC`, then `taskKill`.
static const TaskFuncTable3 D_shelter_b1_control_room_8017D5C4 = {
    { func_shelter_b1_control_room_8017EE2C, func_shelter_b1_control_room_8017EEBC, taskKill },
};

/// The room task: runs the handler for its state from a stack copy of
/// `D_shelter_b1_control_room_8017D5C4`.
void func_shelter_b1_control_room_8017EECC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_control_room_8017D5C4;
    sp.funcs[task->state](task);
}
