#include "rooms/shelter_b4_lower_sewer.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "types.h"

#include "shelter_b4_lower_sewer_private.h"

#include "gameplay/actor_render.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

/// GPU draw mode and ordering-tag bias for this room's water quad packets.
enum {
    SHELTER_B4_LOWER_SEWER_WATER_OT_BIAS = 1,
    SHELTER_B4_LOWER_SEWER_WATER_TPAGE   = getTPage(0, GPU_BLEND_SUBTRACT, 640, 0),
};

static void _shelterB4LowerSewerInitializeWater(Task* task);
static void _shelterB4LowerSewerDrawWater(Task* task);

static s32  _shelterB4LowerSewerRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32  _shelterB4LowerSewerResolveRoomVariant(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _shelterB4LowerSewerIgnoreCommand(Task* task, s32 messageId, s32 command, s32 commandArg);
static s32  _shelterB4LowerSewerIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused);
static void _shelterB4LowerSewerWaterTask(Task* task);

enum {
    SHELTER_B4_LOWER_SEWER_MESSAGE_IGNORED       = 0,
    SHELTER_B4_LOWER_SEWER_WAVE_PHASE_STEP_SHIFT = 9, // 512 of 4096 angle units per segment
    SHELTER_B4_LOWER_SEWER_WAVE_PHASE_PER_FRAME  = 16,
    SHELTER_B4_LOWER_SEWER_WAVE_AMPLITUDE_SHIFT  = 6, // Q12 sine to signed-halfword -64..64 world units
    SHELTER_B4_LOWER_SEWER_WATER_EDGE_GREEN      = 0x20,
    SHELTER_B4_LOWER_SEWER_WATER_EDGE_BLUE       = 0x80,
    SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS = 0x20
};

/// Room-message dispatch for Mine/Shelter transitions, key-item refusals and ignored room actions/commands.
///
/// Four id-selected handlers followed by TASK_MESSAGE_TABLE_END. The room
/// controller borrows the table for synchronous dispatch; keep the overlay
/// loaded while that controller can receive messages.
static TaskMessageEntry _gShelterB4LowerSewerRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB4LowerSewerResolveRoomVariant },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB4LowerSewerRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB4LowerSewerIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB4LowerSewerIgnoreCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s16 gShelterB4LowerSewerWaterY = -1700;

/// Bodyless water-surface task recipe started after the reservoir event.
///
/// Runs at priority `TASK_PRIORITY_ACTOR` with zero spawn arguments. State 0 prepares a borrowed
/// actor-load buffer; state 1 draws the surfaces. Keep the room and buffer
/// live through drawing and GPU consumption; this single recipe has no END.
static TaskDesc _gShelterB4LowerSewerWaterTaskDesc = {
    { { TASK_BODY_NONE, TASK_PRIORITY_ACTOR } }, _shelterB4LowerSewerWaterTask, { .value = 0 }
};

/// World-unit rectangle list for the water with two strips and a waving X seam.
///
/// One drawable rectangle spans 23400 by 2900 units, followed by
/// WATER_SURFACE_LIST_END. Both strips use the shared undisplaced water Y.
static RoomCompactWaterSurface _gShelterB4LowerSewerXWaveSurfaces[] = {
    { -0x28A0, 0, 0x5B68, 2900, 0 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

/// World-unit rectangle list for the eight-segment water strip with a waving far edge.
///
/// One drawable rectangle spans 3600 by 1450 units, followed by
/// WATER_SURFACE_LIST_END. Its near edge uses the shared undisplaced water Y.
static RoomCompactWaterSurface _gShelterB4LowerSewerSingleStripWaterSurfaces[] = {
    { -0x32C8, 0, 3600, 1450, 0 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

static void _shelterB4LowerSewerInitializeRoom(Task* task);
static void _shelterB4LowerSewerIdleRoom(Task* task);
static void _shelterB4LowerSewerDrawXWaveStrips(Task* task);
static void _shelterB4LowerSewerDrawXWaveStrip(Task* task);

/// Refuses every key-item use in the lower sewer without changing game state.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; `itemId` is the selected collected-item
/// ID and the unused second payload is zero. Returns the item menu's refused reply.
static s32 _shelterB4LowerSewerRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Accepts a room transition and resolves its destination from Mine/Shelter progress.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`. Borrows a readable eight-byte request
/// and writable reply through dispatch; they may be the same record. Copies
/// the complete request before resolving its room. Query-only requests preserve
/// the supplied destination. The map overlay must remain loaded. Always returns 1.
static s32 _shelterB4LowerSewerResolveRoomVariant(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_B4_LOWER_SEWER_ROOM_EVENT_ACCEPTED = 1 };
    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return SHELTER_B4_LOWER_SEWER_ROOM_EVENT_ACCEPTED;
}

/// Ignores lower-sewer CAP room commands and returns zero without side effects.
///
/// `ROOM_MESSAGE_COMMAND` supplies an integer command selector and command
/// argument. Neither word is consumed, and callers discard the reply.
static s32 _shelterB4LowerSewerIgnoreCommand(Task* task, s32 messageId, s32 command, s32 commandArg)
{
    return SHELTER_B4_LOWER_SEWER_MESSAGE_IGNORED;
}

/// Ignores lower-sewer direction-trigger actions and returns zero without side effects.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` supplies a borrowed request and a zero
/// second payload. The request is neither read nor retained; the reply is discarded.
static s32 _shelterB4LowerSewerIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused)
{
    return SHELTER_B4_LOWER_SEWER_MESSAGE_IGNORED;
}

/// Registers the lower-sewer room and starts its water after the reservoir event.
///
/// Called once in state 0. Borrows the room task and installs its message table
/// in `GAME_TASK_SLOT_ROOM`; a nonzero reservoir-completion flag spawns the
/// water task. Advances to the idle state even when water task creation fails.
static void _shelterB4LowerSewerInitializeRoom(Task* task)
{
    task->msgTable = _gShelterB4LowerSewerRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) != 0) {
        taskSpawnFromTable(&_gShelterB4LowerSewerWaterTaskDesc, 0, 0, 0);
    }
    task->state = task->state + 1;
}

/// Keeps the initialized lower-sewer room task live without per-frame work.
static void _shelterB4LowerSewerIdleRoom(Task* task)
{
}

/// Lower-sewer room-controller states: 0 register/conditionally start water, 1 idle, 2 kill.
///
/// The dispatcher copies all three callbacks before indexing by `Task::state`.
/// The selector must be in 0..2 and the room overlay must remain loaded.
static const TaskFuncTable3 _gShelterB4LowerSewerRoomTaskStates = {
    { _shelterB4LowerSewerInitializeRoom, _shelterB4LowerSewerIdleRoom, taskKill }
};

void shelterB4LowerSewerRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gShelterB4LowerSewerRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

/// Draws the main water rectangle as two X-running strips with a waving grey seam.
///
/// The terminated list supplies signed world-unit geometry. Each strip has 32
/// quads with blue outer edges and a seam displaced by -64..64 Y units; phase
/// scrolls -16 of 4096 angle units per display frame. Signed width/32 and
/// depth/2 divisions truncate before narrowing to world-coordinate halfwords.
/// Appends at most 0xC00 bytes of subtractive quad and draw-mode packets for
/// this list. Requires a word-aligned cursor and storage reserved through GPU
/// consumption. Borrows one `WaterQuadScratch` and overwrites GTE state.
/// Projection rejects quads with a negative GTE flag word. `task` is unused.
static void _shelterB4LowerSewerDrawXWaveStrips(Task* task)
{
    enum { SHELTER_B4_LOWER_SEWER_WAVE_SEGMENTS_PER_STRIP = 32 };
    SVECTOR                        vertex0, vertex1, vertex2, vertex3;
    long                           screenXY0, screenXY1, screenXY2, screenXY3;
    long                           projectionScale, projectionFlags;
    const RoomCompactWaterSurface* surface;
    WaterQuadScratch*              scratchTop;
    WaterQuadScratch*              strip;
    s32                            wavePhase;
    POLY_G4*                       quad;
    DR_MODE*                       drawMode;
    s32                            depth;
    s32                            segment;

    /// Queues the completed quad and its subtractive mode in the same depth bucket.
    ///
    /// Captures `quad`, `depth`, `drawMode`, the byte cursor and display state.
    /// Takes no arguments; invoke only in a braced caller context. The one-tag bias
    /// follows depth quantization. Prepending the mode last makes the GPU
    /// consume it before the quad. Requires a word-aligned byte cursor with
    /// sizeof(DR_MODE) bytes available through GPU completion. The casts switch
    /// between packet and byte-arena views. Undefined after both drawers.
#define SHELTER_B4_LOWER_SEWER_QUEUE_WATER_QUAD()                                                     \
    addPrim((GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(depth) + SHELTER_B4_LOWER_SEWER_WATER_OT_BIAS), quad); \
    drawMode                              = (DR_MODE*)gShelterB4LowerSewerWaterPacketCursor;          \
    gShelterB4LowerSewerWaterPacketCursor = (u8*)(drawMode + 1);                                      \
    setDrawTPage(drawMode, 0, 0, SHELTER_B4_LOWER_SEWER_WATER_TPAGE);                                 \
    addPrim((GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(depth) + SHELTER_B4_LOWER_SEWER_WATER_OT_BIAS), drawMode)

    surface                    = _gShelterB4LowerSewerXWaveSurfaces;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    scratchTop                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    wavePhase                  = -(gDisplayState.animFrame * SHELTER_B4_LOWER_SEWER_WAVE_PHASE_PER_FRAME);
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchTop - 1;
    strip                                  = scratchTop - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    strip->y = gShelterB4LowerSewerWaterY;
    // Subdivide along X; the displaced Z edge joins the two strips.
    for (; surface->listMarker != WATER_SURFACE_LIST_END; surface++) {
        strip->dx = surface->width / SHELTER_B4_LOWER_SEWER_WAVE_SEGMENTS_PER_STRIP;
        strip->dz = surface->depth / 2;
        strip->x  = surface->x;
        strip->z  = surface->z;
        // First strip: blue outer edge to the grey displaced seam.
        for (segment = 0; segment < SHELTER_B4_LOWER_SEWER_WAVE_SEGMENTS_PER_STRIP; segment++) {
            vertex0.vx     = strip->x + strip->dx * segment;
            vertex0.vy     = strip->y;
            vertex0.vz     = strip->z;
            vertex1.vx     = strip->x + strip->dx * (segment + 1);
            vertex1.vy     = strip->y;
            vertex1.vz     = strip->z;
            strip->yOffset = (u32)rsin(wavePhase + (segment << SHELTER_B4_LOWER_SEWER_WAVE_PHASE_STEP_SHIFT)) >> SHELTER_B4_LOWER_SEWER_WAVE_AMPLITUDE_SHIFT;
            vertex2.vx     = strip->x + strip->dx * segment;
            vertex2.vy     = strip->y + strip->yOffset;
            vertex2.vz     = strip->z + strip->dz;
            strip->yOffset = (u32)rsin(wavePhase + ((segment + 1) << SHELTER_B4_LOWER_SEWER_WAVE_PHASE_STEP_SHIFT)) >> SHELTER_B4_LOWER_SEWER_WAVE_AMPLITUDE_SHIFT;
            vertex3.vx     = strip->x + strip->dx * (segment + 1);
            vertex3.vy     = strip->y + strip->yOffset;
            vertex3.vz     = strip->z + strip->dz;
            depth          = RotTransPers4(&vertex0, &vertex1, &vertex2, &vertex3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &projectionScale, &projectionFlags);
            if (projectionFlags >= 0) {
                quad                                  = (POLY_G4*)gShelterB4LowerSewerWaterPacketCursor;
                gShelterB4LowerSewerWaterPacketCursor = (u8*)(quad + 1);
                setPolyG4(quad);
                setSemiTrans(quad, 1);
                GPU_PRIMITIVE_XY_WORD(quad, 0) = screenXY0;
                GPU_PRIMITIVE_XY_WORD(quad, 1) = screenXY1;
                GPU_PRIMITIVE_XY_WORD(quad, 2) = screenXY2;
                GPU_PRIMITIVE_XY_WORD(quad, 3) = screenXY3;
                quad->r0                       = 0;
                quad->g0                       = SHELTER_B4_LOWER_SEWER_WATER_EDGE_GREEN;
                quad->b0                       = SHELTER_B4_LOWER_SEWER_WATER_EDGE_BLUE;
                quad->r1                       = 0;
                quad->g1                       = SHELTER_B4_LOWER_SEWER_WATER_EDGE_GREEN;
                quad->b1                       = SHELTER_B4_LOWER_SEWER_WATER_EDGE_BLUE;
                quad->r2                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                quad->g2                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                quad->b2                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                quad->r3                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                quad->g3                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                quad->b3                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                SHELTER_B4_LOWER_SEWER_QUEUE_WATER_QUAD();
            }
        }
        // Second strip: the shared displaced seam to the other blue edge.
        for (segment = 0; segment < SHELTER_B4_LOWER_SEWER_WAVE_SEGMENTS_PER_STRIP; segment++) {
            strip->yOffset = (u32)rsin(wavePhase + (segment << SHELTER_B4_LOWER_SEWER_WAVE_PHASE_STEP_SHIFT)) >> SHELTER_B4_LOWER_SEWER_WAVE_AMPLITUDE_SHIFT;
            vertex0.vx     = strip->x + strip->dx * segment;
            vertex0.vy     = strip->y + strip->yOffset;
            vertex0.vz     = strip->z + strip->dz;
            strip->yOffset = (u32)rsin(wavePhase + ((segment + 1) << SHELTER_B4_LOWER_SEWER_WAVE_PHASE_STEP_SHIFT)) >> SHELTER_B4_LOWER_SEWER_WAVE_AMPLITUDE_SHIFT;
            vertex1.vx     = strip->x + strip->dx * (segment + 1);
            vertex1.vy     = strip->y + strip->yOffset;
            vertex1.vz     = strip->z + strip->dz;
            vertex2.vx     = strip->x + strip->dx * segment;
            vertex2.vy     = strip->y;
            vertex2.vz     = strip->z + strip->dz * 2;
            vertex3.vx     = strip->x + strip->dx * (segment + 1);
            vertex3.vy     = strip->y;
            vertex3.vz     = strip->z + strip->dz * 2;
            depth          = RotTransPers4(&vertex0, &vertex1, &vertex2, &vertex3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &projectionScale, &projectionFlags);
            if (projectionFlags >= 0) {
                quad                                  = (POLY_G4*)gShelterB4LowerSewerWaterPacketCursor;
                gShelterB4LowerSewerWaterPacketCursor = (u8*)(quad + 1);
                setPolyG4(quad);
                setSemiTrans(quad, 1);
                GPU_PRIMITIVE_XY_WORD(quad, 0) = screenXY0;
                GPU_PRIMITIVE_XY_WORD(quad, 1) = screenXY1;
                GPU_PRIMITIVE_XY_WORD(quad, 2) = screenXY2;
                GPU_PRIMITIVE_XY_WORD(quad, 3) = screenXY3;
                quad->r2                       = 0;
                quad->g2                       = SHELTER_B4_LOWER_SEWER_WATER_EDGE_GREEN;
                quad->b2                       = SHELTER_B4_LOWER_SEWER_WATER_EDGE_BLUE;
                quad->r3                       = 0;
                quad->g3                       = SHELTER_B4_LOWER_SEWER_WATER_EDGE_GREEN;
                quad->b3                       = SHELTER_B4_LOWER_SEWER_WATER_EDGE_BLUE;
                quad->r0                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                quad->g0                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                quad->b0                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                quad->r1                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                quad->g1                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                quad->b1                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                SHELTER_B4_LOWER_SEWER_QUEUE_WATER_QUAD();
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
}

/// Draws the second water rectangle as one X-running strip with a waving far edge.
///
/// The terminated list supplies signed world-unit geometry. Eight quads span
/// the rectangle; the blue near edge stays flat and the grey far edge is
/// displaced by -64..64 Y units. Phase scrolls -16 of 4096 angle units per
/// display frame. Signed width/8 truncates before narrowing to a halfword.
/// Appends at most 0x180 bytes to the initialized word-aligned packet cursor;
/// storage must remain reserved through GPU consumption. Borrows one
/// `WaterQuadScratch` and overwrites GTE state. Projection rejects quads with
/// a negative GTE flag word. `task` is unused.
static void _shelterB4LowerSewerDrawXWaveStrip(Task* task)
{
    enum { SHELTER_B4_LOWER_SEWER_WAVE_SEGMENTS_PER_STRIP = 8 };
    SVECTOR                        vertex0, vertex1, vertex2, vertex3;
    long                           screenXY0, screenXY1, screenXY2, screenXY3;
    long                           projectionScale, projectionFlags;
    s32                            wavePhase;
    WaterQuadScratch*              scratchTop;
    WaterQuadScratch*              strip;
    const RoomCompactWaterSurface* surface;
    POLY_G4*                       quad;
    DR_MODE*                       drawMode;
    s32                            depth;
    s32                            segment;

    surface                    = _gShelterB4LowerSewerSingleStripWaterSurfaces;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    scratchTop                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    wavePhase                  = -(gDisplayState.animFrame * SHELTER_B4_LOWER_SEWER_WAVE_PHASE_PER_FRAME);
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchTop - 1;
    strip                                  = scratchTop - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    strip->y = gShelterB4LowerSewerWaterY;
    // Keep the near Z edge flat and displace the far edge along each X segment.
    for (; surface->listMarker != WATER_SURFACE_LIST_END; surface++) {
        strip->dx = surface->width / SHELTER_B4_LOWER_SEWER_WAVE_SEGMENTS_PER_STRIP;
        strip->dz = surface->depth;
        strip->x  = surface->x;
        strip->z  = surface->z;
        for (segment = 0; segment < SHELTER_B4_LOWER_SEWER_WAVE_SEGMENTS_PER_STRIP; segment++) {
            vertex0.vx     = strip->x + strip->dx * segment;
            vertex0.vy     = strip->y;
            vertex0.vz     = strip->z;
            vertex1.vx     = strip->x + strip->dx * (segment + 1);
            vertex1.vy     = strip->y;
            vertex1.vz     = strip->z;
            strip->yOffset = (u32)rsin(wavePhase + (segment << SHELTER_B4_LOWER_SEWER_WAVE_PHASE_STEP_SHIFT)) >> SHELTER_B4_LOWER_SEWER_WAVE_AMPLITUDE_SHIFT;
            vertex2.vx     = strip->x + strip->dx * segment;
            vertex2.vy     = strip->y + strip->yOffset;
            vertex2.vz     = strip->z + strip->dz;
            strip->yOffset = (u32)rsin(wavePhase + ((segment + 1) << SHELTER_B4_LOWER_SEWER_WAVE_PHASE_STEP_SHIFT)) >> SHELTER_B4_LOWER_SEWER_WAVE_AMPLITUDE_SHIFT;
            vertex3.vx     = strip->x + strip->dx * (segment + 1);
            vertex3.vy     = strip->y + strip->yOffset;
            vertex3.vz     = strip->z + strip->dz;
            depth          = RotTransPers4(&vertex0, &vertex1, &vertex2, &vertex3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &projectionScale, &projectionFlags);
            if (projectionFlags >= 0) {
                quad                                  = (POLY_G4*)gShelterB4LowerSewerWaterPacketCursor;
                gShelterB4LowerSewerWaterPacketCursor = (u8*)(quad + 1);
                setPolyG4(quad);
                setSemiTrans(quad, 1);
                GPU_PRIMITIVE_XY_WORD(quad, 0) = screenXY0;
                GPU_PRIMITIVE_XY_WORD(quad, 1) = screenXY1;
                GPU_PRIMITIVE_XY_WORD(quad, 2) = screenXY2;
                GPU_PRIMITIVE_XY_WORD(quad, 3) = screenXY3;
                quad->r0                       = 0;
                quad->g0                       = SHELTER_B4_LOWER_SEWER_WATER_EDGE_GREEN;
                quad->b0                       = SHELTER_B4_LOWER_SEWER_WATER_EDGE_BLUE;
                quad->r1                       = 0;
                quad->g1                       = SHELTER_B4_LOWER_SEWER_WATER_EDGE_GREEN;
                quad->b1                       = SHELTER_B4_LOWER_SEWER_WATER_EDGE_BLUE;
                quad->r2                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                quad->g2                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                quad->b2                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                quad->r3                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                quad->g3                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                quad->b3                       = SHELTER_B4_LOWER_SEWER_WATER_SEAM_BRIGHTNESS;
                SHELTER_B4_LOWER_SEWER_QUEUE_WATER_QUAD();
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
#undef SHELTER_B4_LOWER_SEWER_QUEUE_WATER_QUAD
}

/// Runs the lower sewer's water state and publishes its undisplaced world Y.
///
/// Requires a live task with state 0 or 1 and unused actor-load storage reserved
/// for water until GPU consumption. State 0 initializes without drawing; state
/// 1 draws both rectangles each tick. Body and spawn arguments are unused.
static void _shelterB4LowerSewerWaterTask(Task* task)
{
    TaskFunc states[] = { _shelterB4LowerSewerInitializeWater, _shelterB4LowerSewerDrawWater };

    states[task->state](task);
    gGameSession->waterY = gShelterB4LowerSewerWaterY;
}

/// Prepares the unused actor-load buffer for water packets and enters the drawing state.
///
/// A saved companion selects buffer 1; without one, buffer 2 is reused. Previous
/// actor data in that buffer must no longer be needed. Clears its session marker,
/// whose nonzero meaning is unproven. Requires state 0; this tick does not draw.
static void _shelterB4LowerSewerInitializeWater(Task* task)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    task->state = task->state + 1;
}

/// Resets the water packet cursor to the current display half of a borrowed actor buffer.
///
/// A saved companion selects buffer 1, otherwise buffer 2. Requires otBuffer
/// 0 or 1 and previous actor data no longer needed. Each word-aligned 0xC000-byte
/// half must remain reserved until GPU consumption. Does not clear its bytes.
static inline void _shelterB4LowerSewerResetWaterPacketCursor(void)
{
    enum { SHELTER_B4_LOWER_SEWER_WATER_PACKET_HALF_BYTES = 0xC000 };
    // These are byte offsets into reusable arenas, rather than object members.
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gShelterB4LowerSewerWaterPacketCursor = gFsActorBuffer2Base + gDisplayState.otBuffer * SHELTER_B4_LOWER_SEWER_WATER_PACKET_HALF_BYTES;
    } else {
        gShelterB4LowerSewerWaterPacketCursor = gFsActorBuffer1Base + gDisplayState.otBuffer * SHELTER_B4_LOWER_SEWER_WATER_PACKET_HALF_BYTES;
    }
}

/// Draws both water lists into the current display half of the borrowed actor buffer.
///
/// Requires initialized water state and `otBuffer` 0 or 1. A saved companion
/// selects buffer 1, otherwise buffer 2. Each word-aligned 0xC000-byte half
/// remains reserved until GPU consumption; both rectangles use at most 0xD80
/// bytes together. `task` is forwarded only for the drawers' task signatures.
static void _shelterB4LowerSewerDrawWater(Task* task)
{
    // Reset once so the second rectangle appends after the main rectangle.
    _shelterB4LowerSewerResetWaterPacketCursor();
    _shelterB4LowerSewerDrawXWaveStrips(task);
    _shelterB4LowerSewerDrawXWaveStrip(task);
}
