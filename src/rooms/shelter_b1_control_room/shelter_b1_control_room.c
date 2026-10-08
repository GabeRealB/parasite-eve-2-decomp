#include "rooms/shelter_b1_control_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "actors/actor_150400.h"

#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/areas.h"
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

extern TaskMessageEntry D_shelter_b1_control_room_80181B94[];
extern EvsCommand       D_actor_150400_80132D70[];
extern EvsCommand       D_actor_150400_80133088[];

static void _shelterB1ControlRoomConfigureMirror(Task* unusedTask, _ShelterB1ControlRoomMirrorConfig* config);

static s32 _shelterB1ControlRoomRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 secondArg);
s32        func_shelter_b1_control_room_8017ECD4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32        func_shelter_b1_control_room_8017ED68(Task*, s32, s32, s32);
static s32 _shelterB1ControlRoomIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg);

static void _shelterB1ControlRoomMirrorTask(Task* task);

TaskDesc D_shelter_b1_control_room_80181B88 = { { { TASK_BODY_NONE, 112 } }, _shelterB1ControlRoomMirrorTask, { .value = 0 } };

TaskMessageEntry D_shelter_b1_control_room_80181B94[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_control_room_8017ECD4 },
    { 5105, _shelterB1ControlRoomRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1ControlRoomIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_control_room_8017ED68 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void func_shelter_b1_control_room_8017EE2C(Task* arg0);
static void _shelterB1ControlRoomIdleState(Task* task);

/// Rotates a signed short vector by the matrix's Q12 rotation coefficients.
///
/// Ignores matrix translation and writes only output XYZ, with the GTE's
/// signed-halfword saturation. Input and output may alias; their fourth
/// halfwords are unused and the output's is preserved. Requires readable,
/// word-aligned matrix and input storage and a writable output vector.
/// Borrows all storage for the call and changes GTE state.
static inline void _gfxRotateShortVector(const MATRIX* rotation, const SVECTOR* input, SVECTOR* output)
{
    gte_SetRotMatrix(rotation);
    gte_ldv0(input);
    gte_rtv0();
    gte_stsv(output);
}

/// Resets the mirror's activity gates and default floor/player selection.
///
/// Borrows the writable configuration; floorMode is 1. Unused plane members
/// and a retained subject are left intact until the location selects them.
static inline void _shelterB1ControlRoomResetMirrorConfig(_ShelterB1ControlRoomMirrorConfig* config, s32 floorMode)
{
    enum { SHELTER_B1_CONTROL_ROOM_MIRROR_DEFAULT_STRIP_X = 448 };

    config->stripX          = SHELTER_B1_CONTROL_ROOM_MIRROR_DEFAULT_STRIP_X;
    config->active          = 0;
    config->copyPending     = 0;
    config->firstBlendMode  = GPU_BLEND_AVERAGE;
    config->mode            = floorMode;
    config->offset.vy       = 0;
    config->subjectIsPlayer = floorMode;
    config->disabled        = 0;
}

/// Selects the mirror's subject, plane and framebuffer strip for the saved location.
///
/// Borrows writable configuration, initially zeroed or retained from the previous
/// view. Resets its gates but leaves unused plane components intact. Enables
/// the observatory's room-2 floor mirror in views 6..11, motel-room-6 reflections
/// in views 1/8, or the control room's placed-actor mirror in variant 11 and
/// views 1/4. Other control-room variants disable the task. Subjects are borrowed
/// live model tasks. Normal components use Q12; offsets use game coordinates
/// and strip X is in VRAM pixels. The floor's configured offset Y is not read
/// by floor-frame construction; preserve that behavior.
static void _shelterB1ControlRoomConfigureMirror(Task* unusedTask, _ShelterB1ControlRoomMirrorConfig* config)
{
    enum { SHELTER_B1_CONTROL_ROOM_MIRROR_ACTOR_STRIP_X          = 320,
           SHELTER_B1_CONTROL_ROOM_MIRROR_ACTOR_VARIANT          = 11,
           SHELTER_B1_CONTROL_ROOM_MIRROR_NORMAL_ONE             = 4096,
           SHELTER_B1_CONTROL_ROOM_MIRROR_PLANE_Z                = -2750,
           SHELTER_B1_CONTROL_ROOM_MIRROR_MOTEL_OFFSET_X         = 2616,
           SHELTER_B1_CONTROL_ROOM_MIRROR_OBSERVATORY_OFFSET_Y   = 155,
           SHELTER_B1_CONTROL_ROOM_MIRROR_OBSERVATORY_FIRST_VIEW = 6,
           SHELTER_B1_CONTROL_ROOM_MIRROR_OBSERVATORY_VIEW_COUNT = 6 };
    s32                    stage;
    s32                    area;
    s32                    view;
    const GameLocationKey* location;
    s32                    one;

    location = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc;
    one      = 1;
    // Retain the grouped key reads; flattening them changes the matching schedule.
    do {
        stage = location->stage;
        area  = location->area;
        view  = location->view;
    } while (0);
    _shelterB1ControlRoomResetMirrorConfig(config, one);
    switch (stage) {
        case GAME_STAGE_SHELTER_NEO_ARK:
            if (area == GAME_AREA_NEO_ARK_OBSERVATORY && (u32)(view - SHELTER_B1_CONTROL_ROOM_MIRROR_OBSERVATORY_FIRST_VIEW) < SHELTER_B1_CONTROL_ROOM_MIRROR_OBSERVATORY_VIEW_COUNT && gGameSession->location.loc.room == 2) {
                config->offset.vy   = SHELTER_B1_CONTROL_ROOM_MIRROR_OBSERVATORY_OFFSET_Y;
                config->active      = 1;
                config->copyPending = 1;
            }
            break;
        case GAME_STAGE_MINE_SHELTER:
            if (area == GAME_AREA_SHELTER_B1_CONTROL_ROOM) {
                if (location->variant == SHELTER_B1_CONTROL_ROOM_MIRROR_ACTOR_VARIANT) {
                    config->subjectIsPlayer = 0;
                    config->subject         = sceneFindPlacedActor(0);
                    if (view == 4 || view == 1) {
                        config->normal.vz      = -SHELTER_B1_CONTROL_ROOM_MIRROR_NORMAL_ONE;
                        config->offset.vz      = SHELTER_B1_CONTROL_ROOM_MIRROR_PLANE_Z;
                        config->mode           = SHELTER_B1_CONTROL_ROOM_MIRROR_MODE_PLANE;
                        config->normal.vx      = 0;
                        config->normal.vy      = 0;
                        config->offset.vx      = 0;
                        config->offset.vy      = 0;
                        config->active         = 1;
                        config->copyPending    = 1;
                        config->stripX         = SHELTER_B1_CONTROL_ROOM_MIRROR_ACTOR_STRIP_X;
                        config->firstBlendMode = GPU_BLEND_ADD;
                    }
                } else {
                    config->disabled = 1;
                }
            }
            break;
        case GAME_STAGE_DRYFIELD:
        case GAME_STAGE_DRYFIELD_NIGHT:
            if (area == GAME_AREA_DRYFIELD_MOTEL_ROOM_6 && (view == 8 || view == 1)) {
                config->offset.vx      = SHELTER_B1_CONTROL_ROOM_MIRROR_MOTEL_OFFSET_X;
                config->firstBlendMode = SHELTER_B1_CONTROL_ROOM_MIRROR_NO_OVERLAY;
                config->normal.vx      = 0;
                config->normal.vz      = 0;
                config->offset.vy      = 0;
                config->offset.vz      = 0;
                config->active         = 1;
                config->copyPending    = 1;
            }
            break;
    }
    if (config->subjectIsPlayer == 1) {
        config->subject = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    }
}

/// Chooses a positive Q12 axis least aligned with the mirror plane normal.
///
/// Borrows readable configuration and distinct writable scratch storage.
/// Writes `refAxis` at scale 4096 = 1.0 without changing the normal.
/// Absolute values narrow to s16 after negation, including -32768's wrap;
/// strict comparisons give X, then Y, then Z priority on ties.
static inline void _shelterB1ControlRoomChooseMirrorAxis(const _ShelterB1ControlRoomMirrorConfig* config, _ShelterB1ControlRoomMirrorScratch* scratch)
{
    scratch->leastAbs = config->normal.vx;
    if (scratch->leastAbs < 0) {
        scratch->leastAbs = -scratch->leastAbs;
    }
    scratch->leastAxis = SHELTER_B1_CONTROL_ROOM_MIRROR_AXIS_X;
    scratch->axisAbs   = config->normal.vy;
    if (scratch->axisAbs < 0) {
        scratch->axisAbs = -scratch->axisAbs;
    }
    if (scratch->leastAbs > scratch->axisAbs) {
        scratch->leastAbs  = scratch->axisAbs;
        scratch->leastAxis = SHELTER_B1_CONTROL_ROOM_MIRROR_AXIS_Y;
    }
    scratch->axisAbs = config->normal.vz;
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
}

/// Builds a plane-reflection transform around the configured plane point.
///
/// Borrows live view state, work with its configuration, and disjoint scratch.
/// The nonzero normal and basis use Q12; the offset uses whole parent-coordinate
/// units. Translation is the view position plus offset minus reflected offset;
/// reflecting the offset narrows with GTE signed-halfword saturation. Requires
/// initialized GTE and scratch-stack state. Leaves composition invalidation and
/// parenting to the caller, retains no pointer and changes GTE state.
static inline void _shelterB1ControlRoomBuildMirrorPlaneFrame(_ShelterB1ControlRoomMirrorWork* work, const _ShelterB1ControlRoomMirrorConfig* config, _ShelterB1ControlRoomMirrorScratch* scratch)
{
    // Reflect the normal axis in an orthonormal frame, then return to parent axes.
    _shelterB1ControlRoomChooseMirrorAxis(config, scratch);
    gfxBuildOrthonormalBasis(&scratch->basis, &config->normal, &scratch->refAxis);
    gte_TransposeMatrix(&scratch->basis, &scratch->reflect);
    scratch->reflect.m[2][0] = -scratch->reflect.m[2][0];
    scratch->reflect.m[2][1] = -scratch->reflect.m[2][1];
    scratch->reflect.m[2][2] = -scratch->reflect.m[2][2];
    gte_MulMatrix0(&scratch->basis, &scratch->reflect, &scratch->reflect);
    // Keep the plane point fixed relative to the current view position.
    work->coord.coord      = scratch->reflect;
    work->coord.coord.t[0] = gGfxViewCoord.coord.t[0] + config->offset.vx;
    work->coord.coord.t[1] = gGfxViewCoord.coord.t[1] + config->offset.vy;
    work->coord.coord.t[2] = gGfxViewCoord.coord.t[2] + config->offset.vz;
    _gfxRotateShortVector(&scratch->reflect, &config->offset, &scratch->offset);
    work->coord.coord.t[0] -= scratch->offset.vx;
    work->coord.coord.t[1] -= scratch->offset.vy;
    work->coord.coord.t[2] -= scratch->offset.vz;
}

/// Maintains the location-selected model reflection and its framebuffer overlay.
///
/// State 0 owns a primary-heap work block and cloned TMD body, released by task
/// teardown; allocation, disabled-location or attachment failure calls its exit.
/// The configured subject must be live with the same model part count and is
/// reparented to this task. Later ticks borrow its model, pose and light matrices.
/// Rebuilds the reflection when the masked view-composition stamp changes and
/// queues a requested 320x240 framebuffer copy while display mode is idle.
/// Active mirrors draw the clone and clip a sampled overlay to the frame;
/// inactive ones hide the clone. Requires live view, GPU packets/OT and scratch
/// stack, and part 1 when projecting a scripted event. Uses 4096-unit rotation
/// coefficients, whole game-coordinate translations and centre-relative pixels.
static void _shelterB1ControlRoomMirrorTask(Task* task)
{
    enum {
        SHELTER_B1_CONTROL_ROOM_MIRROR_INITIALIZE         = 0,
        SHELTER_B1_CONTROL_ROOM_MIRROR_CACHE_UNSET        = -1,
        SHELTER_B1_CONTROL_ROOM_MIRROR_MODEL_OT_OFFSET    = 22,
        SHELTER_B1_CONTROL_ROOM_MIRROR_OVERLAY_OT_BIAS    = -15,
        SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_WIDTH        = 320,
        SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HEIGHT       = 240,
        SHELTER_B1_CONTROL_ROOM_MIRROR_DRAW_BUFFER_STRIDE = 272,
        SHELTER_B1_CONTROL_ROOM_MIRROR_STRIP_Y            = 256,
        SHELTER_B1_CONTROL_ROOM_MIRROR_COPY_OT_SLOT       = 1023,
        SHELTER_B1_CONTROL_ROOM_MIRROR_TEXTURE_16BIT      = 2,
        SHELTER_B1_CONTROL_ROOM_MIRROR_COPY_SPRITE_CODE   = 0x65,
        SHELTER_B1_CONTROL_ROOM_MIRROR_CLEAR_TILE_CODE    = 0x60,
        SHELTER_B1_CONTROL_ROOM_MIRROR_CLEAR_SHADE        = 2,
        SHELTER_B1_CONTROL_ROOM_MIRROR_TEXTURE_SHADE_ONE  = 128,
    };
    _ShelterB1ControlRoomMirrorWork*    work;
    _ShelterB1ControlRoomMirrorConfig*  config;
    _ShelterB1ControlRoomMirrorScratch* scratch;
    TmdObject*                          cloneModel;
    TmdObject*                          subjectModel;
    TmdObject*                          subjectBody;
    GfxCoord*                           cloneRoot;
    GfxCoord*                           cloneEventPart;
    GfxCoord*                           subjectPart;
    GfxCoord*                           clonePart;
    DR_AREA*                            drawArea;
    DR_STP*                             drawMask;
    DR_OFFSET*                          drawOffset;
    SPRT*                               copySprite;
    DR_TPAGE*                           copyTexturePage;
    TILE*                               stripClear;
    POLY_FT4*                           overlayQuad;
    s32                                 copyPending;
    s32                                 halfWidth;
    s32                                 textureX;
    s32                                 stripScreenOriginX;
    GfxCoord*                           viewParent;
    s32                                 loopIndex;
    u16                                 drawOrigin[2];
    RECT                                rect;

    if (task->state == SHELTER_B1_CONTROL_ROOM_MIRROR_INITIALIZE) {
        work = memCalloc(sizeof(*work), 0);
        if (work == NULL) {
            taskCallExit(task);
            return;
        }
        config     = &work->cfg;
        task->work = work;
        _shelterB1ControlRoomConfigureMirror(task, config);
        if (config->disabled == 1) {
            taskCallExit(task);
            return;
        }
        subjectBody = config->subject->extra.tmd;
        if (modelObjectAttachTmd(task, subjectBody->source) == NULL) {
            taskCallExit(task);
            return;
        }
        cloneModel                    = task->extra.tmd;
        cloneRoot                     = cloneModel->coords;
        cloneModel->clutRowOffset     = subjectBody->clutRowOffset;
        cloneModel->texturePageOffset = subjectBody->texturePageOffset;
        tmdBuildBufferHalf(cloneModel);
        tmdBuildBufferHalf(cloneModel);
        cloneModel->otOffset   = SHELTER_B1_CONTROL_ROOM_MIRROR_MODEL_OT_OFFSET;
        cloneModel->flags      = TMD_OBJECT_REVERSE_CULLING;
        gGameSession->field_4E = 1;
        cloneRoot->parent      = &work->coord;
        cloneModel->lightMtx   = &work->light;
        cloneModel->colorMtx   = &work->color;
        taskReparent(config->subject, task);
        work->viewRebuildStamp = SHELTER_B1_CONTROL_ROOM_MIRROR_CACHE_UNSET;
        task->state++;
    }

    scratch    = SCRATCH_STACK_RESERVE_BLOCK(_ShelterB1ControlRoomMirrorScratch);
    cloneModel = task->extra.tmd;
    work       = task->work;
    cloneRoot  = cloneModel->coords;
    config     = &work->cfg;
    // A configuration refill and mirrored frame follow each view rebuild.
    if (work->viewRebuildStamp != (gGfxViewCoord.composeStamp & GRAPHICS_COORD_STAMP_MASK)) {
        work->viewRebuildStamp = gGfxViewCoord.composeStamp & GRAPHICS_COORD_STAMP_MASK;
        _shelterB1ControlRoomConfigureMirror(task, config);
        if (work->cfg.active == 1) {
            viewParent               = gGfxViewCoord.parent;
            work->clipLeft           = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_LEFT;
            work->clipRight          = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_RIGHT;
            work->clipTop            = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_TOP;
            work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
            work->clipBottom         = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_BOTTOM;
            work->coord.parent       = viewParent;
            if (config->mode == SHELTER_B1_CONTROL_ROOM_MIRROR_MODE_FLOOR) {
                work->coord.coord          = gGfxViewCoord.coord;
                work->coord.coord.m[1][0] *= -1;
                work->coord.coord.m[1][1] *= -1;
                work->coord.coord.m[1][2] *= -1;
                work->coord.coord.t[1]    += config->normal.vy;
            } else {
                _shelterB1ControlRoomBuildMirrorPlaneFrame(work, config, scratch);
            }
        }
    }

    // Packets execute in reverse insertion order: enter the strip, clear/copy, restore drawing.
    copyPending = config->copyPending;
    if (copyPending == 1 && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
        drawArea       = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_AREA);
        rect.x         = 0;
        rect.y         = gDisplayState.drawBuffer * SHELTER_B1_CONTROL_ROOM_MIRROR_DRAW_BUFFER_STRIDE;
        rect.w         = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_WIDTH;
        rect.h         = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HEIGHT;
        SetDrawArea(drawArea, &rect);
        addPrim(&gGpuCurrentOt[SHELTER_B1_CONTROL_ROOM_MIRROR_COPY_OT_SLOT], drawArea);

        drawMask       = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_STP);
        SetDrawStp(drawMask, 0);
        addPrim(&gGpuCurrentOt[SHELTER_B1_CONTROL_ROOM_MIRROR_COPY_OT_SLOT], drawMask);

        drawOffset     = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_OFFSET);
        drawOrigin[0]  = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_WIDTH;
        drawOrigin[1]  = gDisplayState.drawBuffer * SHELTER_B1_CONTROL_ROOM_MIRROR_DRAW_BUFFER_STRIDE + SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_HEIGHT;
        SetDrawOffset(drawOffset, drawOrigin);
        addPrim(&gGpuCurrentOt[SHELTER_B1_CONTROL_ROOM_MIRROR_COPY_OT_SLOT], drawOffset);

        copySprite     = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(SPRT);
        copySprite->x0 = -SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_WIDTH;
        copySprite->y0 = -SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_HEIGHT;
        copySprite->w  = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_WIDTH;
        copySprite->h  = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HEIGHT;
        copySprite->u0 = 0;
        copySprite->v0 = gDisplayState.drawBuffer << 4;
        setlen(copySprite, sizeof(*copySprite) / sizeof(copySprite->tag) - 1);
        setcode(copySprite, SHELTER_B1_CONTROL_ROOM_MIRROR_COPY_SPRITE_CODE);
        addPrim(&gGpuCurrentOt[SHELTER_B1_CONTROL_ROOM_MIRROR_COPY_OT_SLOT], copySprite);

        copyTexturePage = gGpuPrimCursor;
        gGpuPrimCursor  = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
        setDrawTPage(copyTexturePage, 1, 1, getTPage(SHELTER_B1_CONTROL_ROOM_MIRROR_TEXTURE_16BIT, 0, 0, gDisplayState.drawBuffer << 8));
        addPrim(&gGpuCurrentOt[SHELTER_B1_CONTROL_ROOM_MIRROR_COPY_OT_SLOT], copyTexturePage);

        copySprite     = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(SPRT);
        copySprite->x0 = 0;
        copySprite->y0 = -SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_HEIGHT;
        copySprite->w  = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_WIDTH;
        copySprite->h  = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HEIGHT;
        copySprite->u0 = 0x20;
        copySprite->v0 = gDisplayState.drawBuffer << 4;
        setlen(copySprite, sizeof(*copySprite) / sizeof(copySprite->tag) - 1);
        setcode(copySprite, SHELTER_B1_CONTROL_ROOM_MIRROR_COPY_SPRITE_CODE);
        addPrim(&gGpuCurrentOt[SHELTER_B1_CONTROL_ROOM_MIRROR_COPY_OT_SLOT], copySprite);

        copyTexturePage = gGpuPrimCursor;
        gGpuPrimCursor  = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
        setDrawTPage(copyTexturePage, 1, 1, getTPage(SHELTER_B1_CONTROL_ROOM_MIRROR_TEXTURE_16BIT, 0, 0x80, gDisplayState.drawBuffer << 8));
        addPrim(&gGpuCurrentOt[SHELTER_B1_CONTROL_ROOM_MIRROR_COPY_OT_SLOT], copyTexturePage);

        stripClear     = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(TILE);
        setlen(stripClear, sizeof(*stripClear) / sizeof(stripClear->tag) - 1);
        setcode(stripClear, SHELTER_B1_CONTROL_ROOM_MIRROR_CLEAR_TILE_CODE);
        stripClear->x0 = -SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_WIDTH;
        stripClear->y0 = -SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_HEIGHT;
        stripClear->r0 = stripClear->g0 = SHELTER_B1_CONTROL_ROOM_MIRROR_CLEAR_SHADE;
        stripClear->b0                  = SHELTER_B1_CONTROL_ROOM_MIRROR_CLEAR_SHADE;
        stripClear->w                   = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_WIDTH;
        stripClear->h                   = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HEIGHT;
        addPrim(&gGpuCurrentOt[SHELTER_B1_CONTROL_ROOM_MIRROR_COPY_OT_SLOT], stripClear);

        drawMask       = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_STP);
        SetDrawStp(drawMask, 1);
        addPrim(&gGpuCurrentOt[SHELTER_B1_CONTROL_ROOM_MIRROR_COPY_OT_SLOT], drawMask);

        drawOffset     = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_OFFSET);
        drawOrigin[0]  = config->stripX + SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_WIDTH;
        drawOrigin[1]  = (SHELTER_B1_CONTROL_ROOM_MIRROR_STRIP_Y + SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_HEIGHT);
        SetDrawOffset(drawOffset, drawOrigin);
        addPrim(&gGpuCurrentOt[SHELTER_B1_CONTROL_ROOM_MIRROR_COPY_OT_SLOT], drawOffset);

        drawArea       = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_AREA);
        rect.x         = config->stripX;
        rect.y         = SHELTER_B1_CONTROL_ROOM_MIRROR_STRIP_Y;
        rect.w         = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_WIDTH;
        rect.h         = SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HEIGHT;
        SetDrawArea(drawArea, &rect);
        addPrim(&gGpuCurrentOt[SHELTER_B1_CONTROL_ROOM_MIRROR_COPY_OT_SLOT], drawArea);

        config->copyPending = 0;
    }

    if (config->active == 1) {
        subjectModel       = config->subject->extra.tmd;
        cloneEventPart     = &task->extra.tmd->coords[1];
        subjectPart        = subjectModel->coords;
        cloneModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->light        = *subjectModel->lightMtx;
        work->color        = *subjectModel->colorMtx;
        // basis is scratch subjectPart here: the reflected frame, when one was built, already sits on the clone.
        actorRenderComposeCoord(cloneRoot);
        gte_TransposeMatrix(&cloneRoot->workm, &scratch->basis);
        gte_MulMatrix0(&subjectPart->workm, &scratch->basis, &scratch->basis);
        gte_MulMatrix0(&work->light, &scratch->basis, &work->light);
        cloneRoot->composeStamp = GRAPHICS_COORD_DIRTY;
        clonePart               = cloneRoot;
        for (loopIndex = 0; loopIndex < (u32)subjectModel->partCount; loopIndex++) {
            clonePart->coord = subjectPart->coord;
            clonePart++;
            subjectPart++;
        }
        if (config->firstBlendMode >= 0) {
            // Project a head sample and a foot sample. depthCue, and the GTE flag written over leastAxis, are not read.
            if (gGameSession->eventState != 0) {
                actorRenderComposeCoord(cloneEventPart);
                gte_SetTransMatrix(&cloneEventPart->workm);
                gte_SetRotMatrix(&cloneEventPart->workm);
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
                actorRenderComposeCoord(cloneRoot);
                gte_SetTransMatrix(&cloneRoot->workm);
                gte_SetRotMatrix(&cloneRoot->workm);
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
                DR_TPAGE* textureModePacket;

                textureModePacket     = gGpuPrimCursor;
                stripScreenOriginX    = config->stripX + SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_WIDTH;
                textureX              = scratch->left + stripScreenOriginX;
                scratch->texturePageX = textureX & SHELTER_B1_CONTROL_ROOM_MIRROR_TEXTURE_PAGE_MASK;
                gGpuPrimCursor        = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
                setDrawTPage(textureModePacket, 0, 1, 0);
                addPrim(&gGpuCurrentOt[(((scratch->orderingDepthFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + cloneModel->otOffset + SHELTER_B1_CONTROL_ROOM_MIRROR_OVERLAY_OT_BIAS],
                        textureModePacket);
                for (loopIndex = config->firstBlendMode; loopIndex <= GPU_BLEND_SUBTRACT; loopIndex++) {
                    overlayQuad    = gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(POLY_FT4);
                    setPolyFT4(overlayQuad);
                    setSemiTrans(overlayQuad, 1);
                    if (config->firstBlendMode == GPU_BLEND_ADD) {
                        setShadeTex(overlayQuad, 0);
                        overlayQuad->r0 = overlayQuad->g0 = overlayQuad->b0 = SHELTER_B1_CONTROL_ROOM_MIRROR_TEXTURE_SHADE_ONE;
                    } else {
                        setShadeTex(overlayQuad, 1);
                    }
                    overlayQuad->x0 = overlayQuad->x2 = scratch->left;
                    overlayQuad->x1 = overlayQuad->x3 = scratch->right;
                    overlayQuad->y0 = overlayQuad->y1 = scratch->top;
                    overlayQuad->y2 = overlayQuad->y3 = scratch->bottom;
                    overlayQuad->tpage                = getTPage(SHELTER_B1_CONTROL_ROOM_MIRROR_TEXTURE_16BIT, loopIndex, scratch->texturePageX, SHELTER_B1_CONTROL_ROOM_MIRROR_STRIP_Y);
                    overlayQuad->u0 = overlayQuad->u2 = overlayQuad->x0 + SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_WIDTH + config->stripX - scratch->texturePageX;
                    overlayQuad->u1 = overlayQuad->u3 = overlayQuad->x1 + SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_WIDTH + config->stripX - scratch->texturePageX;
                    overlayQuad->v0 = overlayQuad->v1 = overlayQuad->y0 + SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_HEIGHT;
                    overlayQuad->v2 = overlayQuad->v3 = overlayQuad->y2 + SHELTER_B1_CONTROL_ROOM_MIRROR_FRAME_HALF_HEIGHT;
                    addPrim(&gGpuCurrentOt[(((scratch->orderingDepthFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + cloneModel->otOffset + SHELTER_B1_CONTROL_ROOM_MIRROR_OVERLAY_OT_BIAS],
                            overlayQuad);
                }
                textureModePacket = gGpuPrimCursor;
                gGpuPrimCursor    = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
                setDrawTPage(textureModePacket, 0, 0, 0);
                addPrim(&gGpuCurrentOt[(((scratch->orderingDepthFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + cloneModel->otOffset + SHELTER_B1_CONTROL_ROOM_MIRROR_OVERLAY_OT_BIAS],
                        textureModePacket);
            }
        }
    } else {
        cloneModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_ShelterB1ControlRoomMirrorScratch);
}

/// Refuses every key-item use in the control room with the item menu cannot-use reply.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`. `itemId` is the selected collected-item
/// ID. All arguments are ignored; no inventory state changes.
static s32 _shelterB1ControlRoomRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 func_shelter_b1_control_room_8017ECD4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    if (in->areaId != GAME_AREA_SHELTER_B1_ACCESS_TUNNEL) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_B1_CONTROL_ROOM_TUNNEL_DOOR_UNLOCKED) != 0) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 0;
    }
    gameFlagSetNibbleIfPresent(in->flagId, 2);
    capRunCommandWithTransition(1);
    return 0;
}

s32 func_shelter_b1_control_room_8017ED68(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 3:
            if (gameFlagGetNibble(GAME_FLAG_CONTROL_ROOM_RETURN_TAKEN) != 0) {
                capRunCommandWithTransition(6);
            } else {
                capRunCommandWithTransition(3);
            }
            break;
        case 4:
            if (gameFlagGetNibble(GAME_FLAG_CONTROL_ROOM_RETURN_TAKEN) != 0) {
                capRunCommandWithTransition(7);
            } else {
                capRunCommandWithTransition(4);
            }
            break;
        case 5:
            if (gameFlagGetNibble(GAME_FLAG_CONTROL_ROOM_RETURN_TAKEN) != 0) {
                capRunCommandWithTransition(8);
            } else {
                capRunCommandWithTransition(5);
            }
            break;
        case 9:
            if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) == 6) {
                capRunCommandWithTransition(9);
            }
            break;
    }
    return 0;
}

/// Ignores the control room direction actions and returns zero.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`. The request is borrowed during
/// synchronous dispatch, neither read nor retained. All other arguments are
/// unused; no event starts and the task state stays unchanged.
static s32 _shelterB1ControlRoomIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg)
{
    return 0;
}

static void func_shelter_b1_control_room_8017EE2C(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_control_room_80181B94;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == 0xB) {
        actor150400SpawnSlidingModels();
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 9) {
            evsStartScriptWithSkip(D_actor_150400_80132D70, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_150400_80133088);
        }
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Keeps the control room task available for messages in state 1.
///
/// Leaves its state and message table intact; room teardown is owned by the caller.
static void _shelterB1ControlRoomIdleState(Task* task)
{
    // Retain the target idle callback's otherwise unused stack frame.
    char reservedStack[0x10];
}

/// States of the room task `func_shelter_b1_control_room_8017EECC`: the setup
/// state `func_shelter_b1_control_room_8017EE2C`, the idle state
/// `_shelterB1ControlRoomIdleState`, then `taskKill`.
static const TaskFuncTable3 D_shelter_b1_control_room_8017D5C4 = {
    { func_shelter_b1_control_room_8017EE2C, _shelterB1ControlRoomIdleState, taskKill },
};

/// The room task: runs the handler for its state from a stack copy of
/// `D_shelter_b1_control_room_8017D5C4`.
void func_shelter_b1_control_room_8017EECC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_control_room_8017D5C4;
    sp.funcs[task->state](task);
}
