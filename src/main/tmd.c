#include "main/tmd.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "tmd.h"
#include "main/tmd_types.h"

#include "actors/actor_403600.h"

#include "gameplay/model_lighting.h"
#include "gameplay/model_objects.h"

/// Callback for one TMD stream record during packet construction or drawing.
///
/// The caller sets `scratch->opcode`, `scratch->elemCount` and
/// `scratch->elemStride` (in u32 words). `elements` starts just after the
/// three-word record header; the handler returns the word after the payload,
/// leaving any group or stream terminator for the caller to consume.
/// `objectFlags` contains `TmdObject.flags` when drawing and is zero when
/// constructing packets; record flags are part of `scratch->opcode` instead.
///
/// Scratch, elements, geometry and primitive buffers are borrowed for the call.
/// Handlers may change scratch cursors and counters and must use only the fields
/// initialized by their pass. Resolution stores draw callbacks in the stream;
/// packet construction selects its callbacks directly from the opcode.
typedef u32* (*_TmdModelStreamHandler)(TmdStreamWorkspace* scratch, s32 objectFlags, u32* elements);

/// One four-byte TMD command-stream word, viewed as data or a resolved draw callback.
///
/// Each command has an opcode, a callback slot and packed dimensions, followed
/// by its payload. The dimensions' high 16 bits count elements; the low 16 bits
/// give each element's stride in u32 words. Only a command's second word uses
/// `drawHandler`: resolution overwrites it before drawing, and packet construction
/// skips it. Opcodes, dimensions, payload and the single-word `TMD_STREAM_GROUP_END`
/// and `TMD_STREAM_END` markers use `dataWord`.
///
/// This is the resolver's view of the borrowed `TmdSource.stream` word storage.
/// The stream must be word-aligned, writable and contain complete records through
/// `TMD_STREAM_END`; its descriptor supplies no length for bounds checking.
typedef union {
    u32                    dataWord;    // Opcode, marker, packed count/word stride or payload word
    _TmdModelStreamHandler drawHandler; // Resolved callback in a command's second word; ignored during packet construction
} _TmdStreamWord;
STATIC_ASSERT_SIZEOF(_TmdStreamWord, 4);

/// Scratch-stack block reserved while one model's stream is drawn.
///
/// The stream workspace comes first, so the block's address is the workspace
/// handed to the group walk and to every draw callback. The reservation is
/// 16 bytes longer than the bare `TmdStreamWorkspace` that packet construction
/// reserves. No draw-pass code reads or writes that tail.
typedef struct {
    TmdStreamWorkspace workspace;        // Callback state for the draw walk; set up before the first group
    byte               unknown_88[0x10]; // Reserved with the workspace but never accessed; purpose and subdivision unproven
} _TmdDrawScratch;
STATIC_ASSERT_SIZEOF(_TmdDrawScratch, 0x98);

enum {
    /// Initial state of a shared TMD source whose draw-handler slots need resolution.
    ///
    /// Zero in `TmdSource.handlersResolved` makes creation overwrite each command's
    /// handler slot using its opcode and the current location. The stream must be
    /// writable and well formed; group markers have no handler slot. Reaching
    /// `TMD_STREAM_END`, including at entry to an empty stream, changes the state
    /// to `TMD_SOURCE_HANDLERS_RESOLVED`. Nonzero states skip this pass, so later
    /// creations sharing the source reuse the handlers selected on the first pass.
    TMD_SOURCE_HANDLERS_UNRESOLVED = 0,
    /// Completion value for resolving a shared source's draw-handler slots.
    ///
    /// Stored in `TmdSource.handlersResolved` after reaching `TMD_STREAM_END`,
    /// including an empty stream. Later creations sharing the source reuse
    /// these callbacks. Resolution precedes object and primitive-buffer allocation.
    TMD_SOURCE_HANDLERS_RESOLVED = 1
};

/// Number of primitive-buffer halves in one allocated block.
///
/// Each half holds `bufferHalfBytes` bytes. Allocation and the attached-buffer
/// total both cover every half. A pass selects one half and then toggles
/// `nextBufferHalf` between the first and the second, so the count is two.
enum { TMD_BUFFER_HALF_COUNT = 2 };

/// Number of cached vertex depths in the draw pass's CPU-stack table.
enum { TMD_DRAW_VERTEX_DEPTH_COUNT = 1024 };

/// Left shift from a signed CLUT row count to an encoded GPU CLUT-word displacement.
///
/// `getClut` stores the row at bit 6 and the column, in 16-pixel steps, in
/// bits 0..5. Shifting a row count by 6 gives the amount added to a primitive's
/// CLUT word, and that addend's column bits are clear. An s8 row count covers
/// -8192..8128, which fits in the s16 displacement.
enum { TMD_ENCODED_CLUT_ROW_SHIFT = 6 };

/// Stream opcode families and modifiers consumed by primitive-buffer construction.
///
/// Modifiers select distinct serialized records, not TmdObject.flags. The actor
/// draw variants resolve to different actor-overlay callbacks but use the base
/// family's constructor and element layout. Unlisted combinations fall back
/// to skipping their payload; combining bits does not make a supported opcode.
enum {
    TMD_STREAM_G3_ONE_NORMAL       = 0x00,
    TMD_STREAM_F3                  = 0x04,
    TMD_STREAM_GT3_ONE_NORMAL      = 0x18,
    TMD_STREAM_FT3                 = 0x1C,
    TMD_STREAM_G3_CORNER_NORMALS   = 0x20,
    TMD_STREAM_GT3_ELEMENT_COLOR   = 0x30,
    TMD_STREAM_GT3_CORNER_NORMALS  = 0x38,
    TMD_STREAM_G4_ONE_NORMAL       = 0x40,
    TMD_STREAM_F4                  = 0x44,
    TMD_STREAM_GT4_ONE_NORMAL      = 0x58,
    TMD_STREAM_FT4                 = 0x5C,
    TMD_STREAM_G4_CORNER_NORMALS   = 0x60,
    TMD_STREAM_GT4_ELEMENT_COLOR   = 0x70,
    TMD_STREAM_GT4_CORNER_NORMALS  = 0x78,
    TMD_STREAM_GT4_UNLIT           = 0x156,
    TMD_STREAM_PRE_XFORM           = 0x01,
    TMD_STREAM_SEMI_TRANS          = 0x02,
    TMD_STREAM_CORNER_COLORS       = 0x100,
    TMD_STREAM_LAYERED_TEXTURE     = 0x4000,
    TMD_STREAM_ACTOR_DRAW_VARIANT1 = 0x8000,
    TMD_STREAM_ACTOR_DRAW_VARIANT2 = 0x10000,
    TMD_STREAM_ACTOR_DRAW_VARIANT3 = 0x20000
};

static const TaskFuncTable3 Tmd_TaskStates;

static void _tmdResolveSourceDrawHandlers(TmdSource* source);

static void Tmd_SetupDraw(TmdObject* obj);

static s32 _tmdSumAttachedBufferBytes(void);

static void _tmdEnableSourceLayeredTextures(const TmdSource* source);

static void _tmdHideAttachedModelsTask(Task* task);

static void _tmdFreeAttachedBuffersTask(Task* task);

static const TaskFuncTable3 Tmd_TaskStates = { {
    _tmdHideAttachedModelsTask,
    _tmdFreeAttachedBuffersTask,
    taskKill,
} };

/// Resolves and caches the shared source's per-record draw callbacks in place.
///
/// Only a zero `handlersResolved` runs the walk; completion stores one, even
/// for an empty stream. Each complete opcode chooses a callback for the command's
/// second word. Unknown combinations use `tmdSkipStreamRecord`. Packed dimensions
/// give an unsigned element count and stride in u32 words; group markers occupy
/// one word without a callback slot. The writable, word-aligned stream must end
/// with `TMD_STREAM_END` at entry or after a group marker; no length is available
/// to check record headers, payload extents or the final marker.
///
/// Layered records use offset textures only in Dryfield by day's toilet, unlike
/// construction's parking-lot-or-toilet gate. Reusing a resolved source keeps
/// the first choice across later creations and location changes. Actor variants
/// retain callbacks in the actor403600 overlay, which must be resident at draw.
/// No callback is invoked, no packet capacity is reserved and no scratch is
/// initialized here: drawing may consume slots construction skips, including
/// 0x22, 0x62, 0x122 and 0x162. Source capacities must also cover that draw extent.
static void _tmdResolveSourceDrawHandlers(TmdSource* source)
{
    // Projection records scatter results into pre-transformed packet slots.
    enum {
        TMD_STREAM_XFORM_VERTS_ELEMENT_COLOR = 0xC0,
        TMD_STREAM_XFORM_VERTS_UNLIT         = 0xC4,
        TMD_STREAM_XFORM_VERTS               = 0xC8
    };

    _TmdStreamWord*        stream;
    u32                    opcode;
    u32                    dimensions;
    u32                    elementStrideWords;
    u32                    elementCount;
    _TmdModelStreamHandler handler;
    s32                    useOffsetLayer;
    u32                    stageAreaDifference;

    /// Advances past packed dimensions and their count-times-word-stride payload.
    ///
    /// Captures dimensions, elementStrideWords and elementCount in this walk.
    /// Evaluates cursor three times; require a side-effect-free _TmdStreamWord*
    /// lvalue at complete dimensions/payload words. Leaves the next word unconsumed.
#define TMD_RESOLVE_SKIP_PAYLOAD(cursor)                        \
    do {                                                        \
        dimensions = (cursor)->dataWord;                        \
        (cursor)++;                                             \
        elementStrideWords = dimensions & 0xFFFF;               \
        elementCount       = dimensions >> 16;                  \
        (cursor)          += elementCount * elementStrideWords; \
    } while (0)

    stream = (_TmdStreamWord*)source->stream;
    if (source->handlersResolved == TMD_SOURCE_HANDLERS_UNRESOLVED) {
        stageAreaDifference = GAME_LOCATION_WORD(gGameSession->location.loc);
        stageAreaDifference = (stageAreaDifference & GAME_LOCATION_STAGE_AREA_MASK) ^ GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 0, 0);
        useOffsetLayer      = stageAreaDifference < 1;
        goto readOpcode;

        for (;;) {
            switch (opcode) {
                case TMD_STREAM_G3_CORNER_NORMALS:
                case TMD_STREAM_G3_CORNER_NORMALS | TMD_STREAM_SEMI_TRANS:
                    handler = tmdDrawStreamPrimG3CornerNormals;
                    break;
                case TMD_STREAM_G4_CORNER_NORMALS:
                case TMD_STREAM_G4_CORNER_NORMALS | TMD_STREAM_SEMI_TRANS:
                    handler = tmdDrawStreamPrimG4CornerNormals;
                    break;
                case TMD_STREAM_XFORM_VERTS_ELEMENT_COLOR:
                    handler = tmdXformStreamVertsElemColor;
                    break;
                case TMD_STREAM_XFORM_VERTS_UNLIT:
                    handler = tmdXformStreamVertsUnlit;
                    break;
                case TMD_STREAM_F3 | TMD_STREAM_PRE_XFORM:
                    handler = tmdDrawStreamPrimF3PreXform;
                    break;
                case TMD_STREAM_F4 | TMD_STREAM_PRE_XFORM:
                    handler = tmdDrawStreamPrimF4PreXform;
                    break;
                case TMD_STREAM_G3_CORNER_NORMALS | TMD_STREAM_PRE_XFORM:
                case TMD_STREAM_G3_CORNER_NORMALS | TMD_STREAM_PRE_XFORM | TMD_STREAM_CORNER_COLORS:
                    handler = tmdDrawStreamPrimG3PreXform;
                    break;
                case TMD_STREAM_G4_CORNER_NORMALS | TMD_STREAM_PRE_XFORM:
                case TMD_STREAM_G4_CORNER_NORMALS | TMD_STREAM_PRE_XFORM | TMD_STREAM_CORNER_COLORS:
                    handler = tmdDrawStreamPrimG4PreXform;
                    break;
                case TMD_STREAM_GT3_CORNER_NORMALS:
                    handler = tmdDrawStreamGt3;
                    break;
                case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_ACTOR_DRAW_VARIANT1:
                    handler = actor403600DrawStreamGt3BottomFade;
                    break;
                case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_ACTOR_DRAW_VARIANT2:
                    handler = actor403600DrawStreamGt3TopDisplace;
                    break;
                case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_ACTOR_DRAW_VARIANT3:
                    handler = actor403600DrawStreamGt3PlaneClamp;
                    break;
                case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_SEMI_TRANS:
                    handler = tmdDrawStreamGt3SemiTrans;
                    break;
                case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_SEMI_TRANS | TMD_STREAM_ACTOR_DRAW_VARIANT2:
                    handler = actor403600DrawStreamGt3TopDisplaceSemiTrans;
                    break;
                case TMD_STREAM_GT4_CORNER_NORMALS:
                    handler = tmdDrawStreamGt4;
                    break;
                case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_ACTOR_DRAW_VARIANT1:
                    handler = actor403600DrawStreamGt4BottomFade;
                    break;
                case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_ACTOR_DRAW_VARIANT2:
                    handler = actor403600DrawStreamGt4TopDisplace;
                    break;
                case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_ACTOR_DRAW_VARIANT3:
                    handler = actor403600DrawStreamGt4PlaneClamp;
                    break;
                case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_SEMI_TRANS:
                    handler = tmdDrawStreamGt4SemiTrans;
                    break;
                case TMD_STREAM_XFORM_VERTS:
                    handler = tmdXformStreamVerts;
                    break;
                case TMD_STREAM_XFORM_VERTS | TMD_STREAM_LAYERED_TEXTURE:
                    handler = tmdXformStreamVertsEnvLayer;
                    if (useOffsetLayer != 0) {
                        handler = tmdXformStreamVertsOffsetLayer;
                    }
                    break;
                case TMD_STREAM_XFORM_VERTS | TMD_STREAM_ACTOR_DRAW_VARIANT3:
                    handler = actor403600XformStreamVertsPlaneClamp;
                    break;
                case TMD_STREAM_GT3_ELEMENT_COLOR | TMD_STREAM_PRE_XFORM:
                case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_PRE_XFORM:
                case TMD_STREAM_GT3_ELEMENT_COLOR | TMD_STREAM_PRE_XFORM | TMD_STREAM_CORNER_COLORS:
                    handler = tmdDrawStreamPrimGt3PreXform;
                    break;
                case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_PRE_XFORM | TMD_STREAM_ACTOR_DRAW_VARIANT1:
                    handler = actor403600DrawStreamGt3PreXformBottomFade;
                    break;
                case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_PRE_XFORM | TMD_STREAM_SEMI_TRANS:
                    handler = tmdDrawStreamPrimGt3PreXformSemiTrans;
                    break;
                case TMD_STREAM_GT4_ELEMENT_COLOR | TMD_STREAM_PRE_XFORM:
                case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_PRE_XFORM:
                case TMD_STREAM_GT4_ELEMENT_COLOR | TMD_STREAM_PRE_XFORM | TMD_STREAM_CORNER_COLORS:
                    handler = tmdDrawStreamPrimGt4PreXform;
                    break;
                case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_PRE_XFORM | TMD_STREAM_ACTOR_DRAW_VARIANT1:
                    handler = actor403600DrawStreamGt4PreXformBottomFade;
                    break;
                case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_PRE_XFORM | TMD_STREAM_SEMI_TRANS:
                    handler = tmdDrawStreamPrimGt4PreXformSemiTrans;
                    break;
                case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_PRE_XFORM | TMD_STREAM_LAYERED_TEXTURE:
                    handler = tmdDrawStreamPrimGt3PreXformEnvLayer;
                    if (useOffsetLayer != 0) {
                        handler = tmdDrawStreamPrimGt3PreXformOffsetLayer;
                    }
                    break;
                case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_PRE_XFORM | TMD_STREAM_LAYERED_TEXTURE:
                    handler = tmdDrawStreamPrimGt4PreXformEnvLayer;
                    if (useOffsetLayer != 0) {
                        handler = tmdDrawStreamPrimGt4PreXformOffsetLayer;
                    }
                    break;
                case TMD_STREAM_G3_ONE_NORMAL:
                    handler = tmdDrawStreamPrimG3;
                    break;
                case TMD_STREAM_G4_ONE_NORMAL:
                    handler = tmdDrawStreamPrimG4;
                    break;
                case TMD_STREAM_GT3_ONE_NORMAL:
                    handler = tmdDrawStreamPrimGt3OneNormal;
                    break;
                case TMD_STREAM_GT3_ONE_NORMAL | TMD_STREAM_SEMI_TRANS:
                    handler = tmdDrawStreamPrimGt3OneNormalSemiTrans;
                    break;
                case TMD_STREAM_GT4_ONE_NORMAL:
                    handler = tmdDrawStreamPrimGt4OneNormal;
                    break;
                case TMD_STREAM_GT4_ONE_NORMAL | TMD_STREAM_SEMI_TRANS:
                    handler = tmdDrawStreamPrimGt4OneNormalSemiTrans;
                    break;
                case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_LAYERED_TEXTURE:
                    handler = tmdDrawStreamPrimGt4EnvLayer;
                    if (useOffsetLayer != 0) {
                        handler = tmdDrawStreamPrimGt4OffsetLayer;
                    }
                    break;
                case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_LAYERED_TEXTURE:
                    handler = tmdDrawStreamPrimGt3EnvLayer;
                    if (useOffsetLayer != 0) {
                        handler = tmdDrawStreamPrimGt3OffsetLayer;
                    }
                    break;
                case TMD_STREAM_G3_CORNER_NORMALS | TMD_STREAM_CORNER_COLORS:
                    handler = tmdDrawStreamPrimG3CornerColors;
                    break;
                case TMD_STREAM_G3_CORNER_NORMALS | TMD_STREAM_SEMI_TRANS | TMD_STREAM_CORNER_COLORS:
                    handler = tmdDrawStreamPrimG3CornerColorsSemiTrans;
                    break;
                case TMD_STREAM_G4_CORNER_NORMALS | TMD_STREAM_CORNER_COLORS:
                    handler = tmdDrawStreamPrimG4CornerColors;
                    break;
                case TMD_STREAM_G4_CORNER_NORMALS | TMD_STREAM_SEMI_TRANS | TMD_STREAM_CORNER_COLORS:
                    handler = tmdDrawStreamPrimG4CornerColorsSemiTrans;
                    break;
                case TMD_STREAM_FT3:
                    handler = tmdDrawStreamPrimFt3;
                    break;
                case TMD_STREAM_FT3 | TMD_STREAM_SEMI_TRANS:
                    handler = tmdDrawStreamPrimFt3SemiTrans;
                    break;
                case TMD_STREAM_FT4:
                    handler = tmdDrawStreamPrimFt4;
                    break;
                case TMD_STREAM_FT4 | TMD_STREAM_SEMI_TRANS:
                    handler = tmdDrawStreamPrimFt4SemiTrans;
                    break;
                case TMD_STREAM_GT3_ELEMENT_COLOR:
                    handler = tmdDrawStreamPrimGt3ElemColor;
                    break;
                case TMD_STREAM_GT3_ELEMENT_COLOR | TMD_STREAM_CORNER_COLORS:
                    handler = tmdDrawStreamPrimGt3CornerColors;
                    break;
                case TMD_STREAM_GT4_ELEMENT_COLOR:
                    handler = tmdDrawStreamPrimGt4ElemColor;
                    break;
                case TMD_STREAM_GT4_ELEMENT_COLOR | TMD_STREAM_CORNER_COLORS:
                    handler = tmdDrawStreamPrimGt4CornerColors;
                    break;
                case TMD_STREAM_GT4_UNLIT:
                    handler = tmdDrawStreamPrimGt4Unlit;
                    break;
                case TMD_STREAM_F3:
                    handler = tmdDrawStreamPrimF3;
                    break;
                case TMD_STREAM_F4:
                    handler = tmdDrawStreamPrimF4;
                    break;
                default:
                    handler = tmdSkipStreamRecord;
                    break;
            }

            // Resolve the draw slot, then skip the count * stride payload words.
            stream++;
            stream->drawHandler = handler;
            stream++;
            TMD_RESOLVE_SKIP_PAYLOAD(stream);
            opcode = stream->dataWord;

            while (1) {
                if (opcode != TMD_STREAM_GROUP_END) {
                    break;
                }
                stream++;
            readOpcode:
                opcode = stream->dataWord;
                // Terminator at entry, or after a group marker. Leave the word in place.
                if (opcode == TMD_STREAM_END) {
                    goto done;
                }
            }
        }
    done:
        source->handlersResolved = TMD_SOURCE_HANDLERS_RESOLVED;
    }
}

#undef TMD_RESOLVE_SKIP_PAYLOAD

void tmdBuildBufferHalf(TmdObject* model)
{
    TmdStreamWorkspace*    workspace;
    TmdSource*             source;
    u32*                   stream;
    u32                    opcode;
    _TmdModelStreamHandler handler;
    s32                    useOffsetLayer;
    u8*                    bufferBase;
    u32                    stageAreaKey;
    TmdStreamWorkspace*    reservedWorkspace;
    TmdStreamWorkspace*    scratchEnd;

    useOffsetLayer                           = 0;
    source                                   = model->source;
    scratchEnd                               = SCRATCH_STACK_CURSOR(TmdStreamWorkspace);
    stream                                   = source->stream;
    stageAreaKey                             = GAME_LOCATION_WORD(gGameSession->location.loc);
    reservedWorkspace                        = scratchEnd - 1;
    stageAreaKey                            &= GAME_LOCATION_STAGE_AREA_MASK;
    SCRATCH_STACK_CURSOR(TmdStreamWorkspace) = reservedWorkspace;
    if ((stageAreaKey == GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 0, 0)) ||
        (stageAreaKey == GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 0, 0))) {
        useOffsetLayer = 1;
    }
    workspace = reservedWorkspace;
    // Select one half; pre-transformed packets precede directly transformed packets.
    workspace->obj       = model;
    bufferBase           = model->buffer;
    workspace->primWrite = bufferBase;
    if (model->nextBufferHalf != 0) {
        workspace->primWrite = bufferBase + model->bufferHalfBytes;
    }
    workspace->preXformWrite     = workspace->primWrite;
    workspace->primWrite         = workspace->primWrite + source->preXformRegionBytes;
    model->nextBufferHalf       ^= 1;
    workspace->verts             = model->source->verts;
    workspace->normals           = model->source->normals;
    workspace->texturePageOffset = model->texturePageOffset;
    // Scale the byte representation, then sign-extend it to an encoded CLUT displacement.
    workspace->encodedClutOffset = (s32)((u32)(u8)model->clutRowOffset << 24) >> (24 - TMD_ENCODED_CLUT_ROW_SHIFT);
    goto readOpcode;

    for (;;) {
        switch (opcode) {
            case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_LAYERED_TEXTURE:
                handler = tmdBuildStreamGt3LayeredBase;
                if (useOffsetLayer != 0) {
                    handler = tmdBuildStreamGt3OffsetLayer;
                }
                break;
            case TMD_STREAM_GT3_CORNER_NORMALS:
            case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_SEMI_TRANS:
            case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_ACTOR_DRAW_VARIANT1:
            case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_ACTOR_DRAW_VARIANT2:
            case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_SEMI_TRANS | TMD_STREAM_ACTOR_DRAW_VARIANT2:
            case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_ACTOR_DRAW_VARIANT3:
                handler = tmdBuildStreamGt3;
                break;
            case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_LAYERED_TEXTURE:
                handler = tmdBuildStreamGt4LayeredBase;
                if (useOffsetLayer != 0) {
                    handler = tmdBuildStreamGt4OffsetLayer;
                }
                break;
            case TMD_STREAM_GT4_CORNER_NORMALS:
            case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_SEMI_TRANS:
            case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_ACTOR_DRAW_VARIANT1:
            case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_ACTOR_DRAW_VARIANT2:
            case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_ACTOR_DRAW_VARIANT3:
                handler = tmdBuildStreamGt4;
                break;
            case TMD_STREAM_GT3_ELEMENT_COLOR | TMD_STREAM_PRE_XFORM:
            case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_PRE_XFORM:
            case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_PRE_XFORM | TMD_STREAM_SEMI_TRANS:
            case TMD_STREAM_GT3_ELEMENT_COLOR | TMD_STREAM_PRE_XFORM | TMD_STREAM_CORNER_COLORS:
            case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_PRE_XFORM | TMD_STREAM_ACTOR_DRAW_VARIANT1:
                handler = tmdBuildStreamGt3PreXform;
                break;
            case TMD_STREAM_GT4_ELEMENT_COLOR | TMD_STREAM_PRE_XFORM:
            case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_PRE_XFORM:
            case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_PRE_XFORM | TMD_STREAM_SEMI_TRANS:
            case TMD_STREAM_GT4_ELEMENT_COLOR | TMD_STREAM_PRE_XFORM | TMD_STREAM_CORNER_COLORS:
            case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_PRE_XFORM | TMD_STREAM_ACTOR_DRAW_VARIANT1:
                handler = tmdBuildStreamGt4PreXform;
                break;
            case TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_PRE_XFORM | TMD_STREAM_LAYERED_TEXTURE:
                handler = tmdBuildStreamGt3PreXformEnvLayer;
                if (useOffsetLayer != 0) {
                    handler = tmdBuildStreamGt3PreXformOffsetLayer;
                }
                break;
            case TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_PRE_XFORM | TMD_STREAM_LAYERED_TEXTURE:
                handler = tmdBuildStreamGt4PreXformEnvLayer;
                if (useOffsetLayer != 0) {
                    handler = tmdBuildStreamGt4PreXformOffsetLayer;
                }
                break;
            case TMD_STREAM_GT3_ONE_NORMAL:
            case TMD_STREAM_GT3_ONE_NORMAL | TMD_STREAM_SEMI_TRANS:
                handler = tmdBuildStreamGt3OneNormal;
                break;
            case TMD_STREAM_GT4_ONE_NORMAL:
            case TMD_STREAM_GT4_ONE_NORMAL | TMD_STREAM_SEMI_TRANS:
                handler = tmdBuildStreamGt4OneNormal;
                break;
            case TMD_STREAM_FT3:
            case TMD_STREAM_FT3 | TMD_STREAM_SEMI_TRANS:
                handler = modelLightingStreamPrimFt3;
                break;
            case TMD_STREAM_FT4:
            case TMD_STREAM_FT4 | TMD_STREAM_SEMI_TRANS:
                handler = modelLightingStreamPrimFt4;
                break;
            case TMD_STREAM_GT3_ELEMENT_COLOR:
                handler = tmdBuildStreamGt3ElemColor;
                break;
            case TMD_STREAM_GT3_ELEMENT_COLOR | TMD_STREAM_CORNER_COLORS:
                handler = tmdBuildStreamGt3CornerColors;
                break;
            case TMD_STREAM_GT4_ELEMENT_COLOR:
                handler = tmdBuildStreamGt4ElemColor;
                break;
            case TMD_STREAM_GT4_ELEMENT_COLOR | TMD_STREAM_CORNER_COLORS:
                handler = tmdBuildStreamGt4CornerColors;
                break;
            case TMD_STREAM_GT4_UNLIT:
                handler = tmdBuildStreamGt4Unlit;
                break;
            case TMD_STREAM_F3:
                handler = modelLightingStreamPrimF3;
                break;
            case TMD_STREAM_F4:
                handler = modelLightingStreamPrimF4;
                break;
            case TMD_STREAM_F3 | TMD_STREAM_PRE_XFORM:
                handler = modelLightingStreamPrimF3PreXform;
                break;
            case TMD_STREAM_F4 | TMD_STREAM_PRE_XFORM:
                handler = modelLightingStreamPrimF4PreXform;
                break;
            case TMD_STREAM_G4_ONE_NORMAL:
            case TMD_STREAM_G4_CORNER_NORMALS:
            case TMD_STREAM_G4_CORNER_NORMALS | TMD_STREAM_CORNER_COLORS:
            case TMD_STREAM_G4_ONE_NORMAL | TMD_STREAM_LAYERED_TEXTURE:
            case TMD_STREAM_G4_CORNER_NORMALS | TMD_STREAM_LAYERED_TEXTURE:
            case TMD_STREAM_G4_CORNER_NORMALS | TMD_STREAM_CORNER_COLORS | TMD_STREAM_LAYERED_TEXTURE:
                handler = modelLightingReserveStreamPrimG4;
                break;
            default:
                handler = tmdSkipStreamRecord;
                break;
            case TMD_STREAM_G3_ONE_NORMAL:
            case TMD_STREAM_G3_CORNER_NORMALS:
            case TMD_STREAM_G3_CORNER_NORMALS | TMD_STREAM_CORNER_COLORS:
            case TMD_STREAM_G3_ONE_NORMAL | TMD_STREAM_LAYERED_TEXTURE:
            case TMD_STREAM_G3_CORNER_NORMALS | TMD_STREAM_LAYERED_TEXTURE:
            case TMD_STREAM_G3_CORNER_NORMALS | TMD_STREAM_CORNER_COLORS | TMD_STREAM_LAYERED_TEXTURE:
                handler = modelLightingReserveStreamPrimG3;
                break;
        }

        // Ignore the resolved draw slot and decode little-endian count/word stride.
        workspace->opcode     = *stream;
        stream               += 2;
        workspace->elemStride = ((u16*)stream)[0];
        workspace->elemCount  = ((u16*)stream)[1];
        stream               += 1;
        stream                = handler(workspace, 0, stream);
        opcode                = *stream;

        while (1) {
            if (opcode != TMD_STREAM_GROUP_END) {
                break;
            }
            stream++;
        readOpcode:
            opcode = *stream;
            // Terminator at entry, or after a group marker. Leave the word in place.
            if (opcode == TMD_STREAM_END) {
                goto done;
            }
        }
    }
done:
    SCRATCH_STACK_RELEASE_BLOCK(TmdStreamWorkspace);
}

/// Initializes persistent packet data in both halves, preserving the half selector.
///
/// `model` must own a live auxiliary-heap block and have nextBufferHalf 0 or 1.
/// Each build consumes the selected half and toggles the selector; two builds
/// restore it. The caller chooses the initial half before calling. Stream,
/// source capacities and scratch-stack requirements are those of
/// `tmdBuildBufferHalf`; this helper neither allocates nor enables drawing.
static inline void _tmdInitializeBufferHalves(TmdObject* model)
{
    tmdBuildBufferHalf(model);
    tmdBuildBufferHalf(model);
}

TmdObject* tmdCreateModel(TmdSource* source, s32 bufferFlags)
{
    TmdAllocation* allocation;
    TmdObject*     model;
    GfxCoord*      coord;
    const TmdBone* bone;
    u32            partIndex;
    void*          buffer = NULL;

    // Resolve shared draw slots before either allocation, including failure paths.
    _tmdResolveSourceDrawHandlers(source);
    allocation = memCalloc((source->partCount * sizeof(allocation->coords[0])) + sizeof(*allocation), false);
    model      = allocation != NULL ? &allocation->object : NULL;
    if (model != NULL) {
        model->flags             = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        model->partCount         = source->partCount;
        model->coords            = allocation->coords;
        model->nextBufferHalf    = 0;
        coord                    = model->coords;
        model->bufferHalfBytes   = source->bufferHalfBytes;
        model->lightMtx          = &GsLIGHTWSMATRIX;
        model->colorMtx          = &D_80074080;
        model->texturePageOffset = 0;
        model->clutRowOffset     = 0;
        model->source            = source;
        bone                     = source->skeleton;
        // Copy the initial pose; self-parented roots attach to the view coordinate.
        for (partIndex = 0; partIndex < (u32)model->partCount; partIndex++) {
            coord->coord = bone->local;
            if (bone->parentIndex != partIndex) {
                coord->parent = &model->coords[bone->parentIndex];
            } else {
                coord->parent = &gGfxViewCoord;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord++;
            bone++;
        }
        model->buffer = NULL;
        if (bufferFlags == 0) {
            buffer = memCalloc(source->bufferHalfBytes * TMD_BUFFER_HALF_COUNT, true);
            if (buffer != NULL) {
                model->buffer = buffer;
                _tmdInitializeBufferHalves(model);
            }
        } else if (bufferFlags & TMD_CREATE_SKIP_AUTO_BUFFER) {
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        }
    }
    return model;
}

static void Tmd_SetupDraw(TmdObject* obj)
{
    s32              vertexDepths[TMD_DRAW_VERTEX_DEPTH_COUNT];
    _TmdDrawScratch* scratchEnd;
    _TmdDrawScratch* scratch;
    u32*             stream;
    u32              flags;
    void*            bufptr;
    s32              disp;
    u_long*          ot;
    TmdSource*       p;
    s32              e;
    s32*             depthTable;
    SVECTOR*         normals;

    {
        TmdSource* p;

        p                               = obj->source;
        scratchEnd                      = SCRATCH_STACK_CURSOR(_TmdDrawScratch);
        stream                          = p->stream;
        disp                            = gDisplayState.otDepthShift;
        scratch                         = scratchEnd - 1;
        scratch->workspace.obj          = obj;
        scratch->workspace.otDepthShift = disp;
    }
    bufptr                                = obj->buffer;
    scratch->workspace.primWrite          = bufptr;
    SCRATCH_STACK_CURSOR(_TmdDrawScratch) = scratch;
    if (obj->nextBufferHalf != 0) {
        scratch->workspace.primWrite = (u8*)bufptr + obj->bufferHalfBytes;
    }
    scratch->workspace.preXformWrite = scratch->workspace.primWrite;
    scratch->workspace.primWrite     = scratch->workspace.primWrite + obj->source->preXformRegionBytes;
    obj->nextBufferHalf             ^= 1;
    scratch->workspace.verts         = obj->source->verts;
    ot                               = gGpuCurrentOt;
    p                                = obj->source;
    normals                          = p->normals;
    scratch->workspace.ot            = ot;
    scratch->workspace.normals       = normals;
    e                                = obj->otOffset;
    depthTable                       = vertexDepths;
    scratch->workspace.szTable       = depthTable;
    scratch->workspace.ot            = ot + e;

    gte_SetColorMatrix(obj->colorMtx);
    gte_ldbkdir(obj->colorMtx->t[0], obj->colorMtx->t[1], obj->colorMtx->t[2]);

    flags = obj->flags;
    // Remove the view rotation before combining the light directions with each part.
    gte_TransposeMatrix(&gGfxViewCoord.workm, &scratch->workspace.viewLightRotation);

    gte_SetRotMatrix(obj->lightMtx);
    gte_ldclmv(&scratch->workspace.viewLightRotation[0][0]);
    gte_rtir();
    gte_stclmv(&scratch->workspace.viewLightRotation[0][0]);
    gte_ldclmv(&scratch->workspace.viewLightRotation[0][1]);
    gte_rtir();
    gte_stclmv(&scratch->workspace.viewLightRotation[0][1]);
    gte_ldclmv(&scratch->workspace.viewLightRotation[0][2]);
    gte_rtir();
    gte_stclmv(&scratch->workspace.viewLightRotation[0][2]);

    tmdDrawModelStream(&scratch->workspace, flags, stream, obj);

    SCRATCH_STACK_RELEASE_BLOCK(_TmdDrawScratch);
}

void tmdFreePrimitiveBuffer(TmdObject* model)
{
    if (model->buffer != NULL) {
        memFreeFromHeap(model->buffer, true);
        model->buffer = NULL;
    }
}

s32 tmdAllocPrimitiveBuffer(TmdObject* model)
{
    s32   allocated;
    void* buffer;

    allocated = false;
    if (model->buffer == NULL) {
        buffer        = memCalloc(model->source->bufferHalfBytes * TMD_BUFFER_HALF_COUNT, true);
        model->buffer = buffer;
        if (buffer != NULL) {
            model->nextBufferHalf = 0;
            _tmdInitializeBufferHalves(model);
            allocated = true;
        }
    }
    return allocated;
}

/// Sums both primitive-buffer halves of attached models that currently own buffers.
///
/// Counts source-declared payload capacity in bytes, excluding heap metadata
/// and model/coordinate allocations. Sources must remain unchanged while their
/// objects live, and the attached-list total must fit s32. No current caller.
static s32 _tmdSumAttachedBufferBytes(void)
{
    TmdObject* model;
    s32        totalBytes;

    totalBytes = 0;
    model      = PARENT_OF(gTmdList.next, TmdObject, link);
    while (model != NULL) {
        if (model->buffer != NULL) {
            totalBytes += model->source->bufferHalfBytes * TMD_BUFFER_HALF_COUNT;
        }
        model = PARENT_OF(model->link.next, TmdObject, link);
    }
    return totalBytes;
}

/// Converts ordinary directly transformed GT3/GT4 records to layered texture records.
///
/// Rewrites only 0x38 to 0x4038 and 0x78 to 0x4078 in the borrowed writable
/// stream. Payloads, dimensions and cached draw-handler slots are unchanged.
/// Call before handler resolution and allocate capacity for two packets per
/// converted element; this routine neither resets resolution nor resizes buffers.
/// Groups must end with TMD_STREAM_GROUP_END before the final TMD_STREAM_END.
/// There is no stored stream length or current caller.
static void _tmdEnableSourceLayeredTextures(const TmdSource* source)
{
    u32* stream;
    u32  opcode;
    u32  dimensions;
    u32  elementStrideWords;
    u32  groupEnd;

    /// Skips a dimensions word and its count-times-word-stride payload.
    ///
    /// Captures this function's dimensions and elementStrideWords locals.
    /// Evaluates cursor three times; it must be a side-effect-free u32* lvalue
    /// addressing complete dimensions and payload words in the same stream.
#define TMD_SKIP_STREAM_PAYLOAD(cursor)                      \
    do {                                                     \
        dimensions         = *(cursor);                      \
        elementStrideWords = dimensions & 0xFFFF;            \
        (cursor)++;                                          \
        (cursor) += (dimensions >> 16) * elementStrideWords; \
    } while (0)

    stream = source->stream;
    // A stream whose first word is the terminator has no groups to rewrite.
    if (*stream != TMD_STREAM_END) {
        groupEnd = TMD_STREAM_GROUP_END;
        do {
            if (*stream != groupEnd) {
                do {
                    opcode = *stream;
                    if (opcode == (TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_PRE_XFORM | TMD_STREAM_SEMI_TRANS)) {
                        goto skipPayload;
                    }
                    if (opcode <= (u32)(TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_SEMI_TRANS | TMD_STREAM_PRE_XFORM)) {
                        if (opcode == TMD_STREAM_GT3_CORNER_NORMALS) {
                            goto convertTriangle;
                        }
                        goto skipOtherPayload;
                    }
                    if (opcode == (TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_PRE_XFORM)) {
                        goto skipPayload;
                    }
                    if (opcode >= (u32)(TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_SEMI_TRANS)) {
                        goto skipPayload;
                    }
                    if (opcode == TMD_STREAM_GT4_CORNER_NORMALS) {
                        goto convertQuad;
                    }
                    goto skipOtherPayload;

                convertTriangle:
                    *stream = (TMD_STREAM_GT3_CORNER_NORMALS | TMD_STREAM_LAYERED_TEXTURE);
                    goto skipPayload;
                convertQuad:
                    *stream = (TMD_STREAM_GT4_CORNER_NORMALS | TMD_STREAM_LAYERED_TEXTURE);
                skipPayload:
                    stream += 2;
                    goto readDimensions;
                skipOtherPayload:
                    stream += 2;
                readDimensions:
                    TMD_SKIP_STREAM_PAYLOAD(stream);
                } while (*stream != TMD_STREAM_GROUP_END);
            }
            stream++;
        } while (*stream != TMD_STREAM_END);
    }
#undef TMD_SKIP_STREAM_PAYLOAD
}

/// Hides every attached model, then advances the buffer-release task to its next state.
///
/// State 0 of Tmd_DispatchTask sets only active-pass exclusion. Buffers and
/// flagged-pass selection remain intact until the subsequent release state.
static void _tmdHideAttachedModelsTask(Task* task)
{
    TmdObject* model;

    model = PARENT_OF(gTmdList.next, TmdObject, link);
    while (model != NULL) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        model         = PARENT_OF(model->link.next, TmdObject, link);
    }
    task->state++;
}

/// Releases all attached primitive buffers, then advances the task to its kill state.
///
/// State 1 follows the hide state on the next dispatch. Models and coordinates
/// stay attached; GPU use of these buffers must already have finished.
static void _tmdFreeAttachedBuffersTask(Task* task)
{
    TmdObject* model;

    model = PARENT_OF(gTmdList.next, TmdObject, link);
    while (model != NULL) {
        if (model->buffer != NULL) {
            memFreeFromHeap(model->buffer, true);
            model->buffer = NULL;
        }
        model = PARENT_OF(model->link.next, TmdObject, link);
    }
    task->state++;
}

void Tmd_DispatchTask(Task* task)
{
    TaskFuncTable3 sp;

    sp = Tmd_TaskStates;
    sp.funcs[task->state](task);
}

void Gpu_ResetGraphAndOt(void)
{
    TmdObject* node;

    node = PARENT_OF(gTmdList.next, TmdObject, link);
    ResetGraph(1);
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    while (node != NULL) {
        if (node->buffer != NULL) {
            node->buffer = NULL;
        }
        node = PARENT_OF(node->link.next, TmdObject, link);
    }
}

void Tmd_AllocMissingBuffers(void)
{
    TmdObject* node;
    void*      mem;

    node = PARENT_OF(gTmdList.next, TmdObject, link);
    memInitAuxHeap();
    CdCmd_SetupMdecBuffers();
    while (node != NULL) {
        if (node->buffer == NULL) {
            if (!(node->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
                mem = memCalloc(node->source->bufferHalfBytes * TMD_BUFFER_HALF_COUNT, 1);
                if (mem != NULL) {
                    node->buffer         = mem;
                    node->nextBufferHalf = 0;
                    tmdBuildBufferHalf(node);
                    tmdBuildBufferHalf(node);
                }
            }
        }
        node = PARENT_OF(node->link.next, TmdObject, link);
    }
}

void tmdRestoreAttachedBuffersTask(Task* task)
{
    TmdObject* model;
    void*      buffer;

    model = PARENT_OF(gTmdList.next, TmdObject, link);
    while (model != NULL) {
        if (model->buffer == NULL) {
            buffer = memCalloc(model->source->bufferHalfBytes * TMD_BUFFER_HALF_COUNT, true);
            if (buffer != NULL) {
                model->buffer         = buffer;
                model->nextBufferHalf = 0;
                // Only a newly allocated model becomes eligible for active drawing.
                model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
                tmdBuildBufferHalf(model);
                tmdBuildBufferHalf(model);
            }
        }
        model = PARENT_OF(model->link.next, TmdObject, link);
    }
    taskKill(task);
}

void Tmd_DrawFlaggedNodes(TmdObject* node)
{
    while (node != NULL) {
        if (node->flags & TMD_OBJECT_FLAGGED_PASS) {
            if (node->buffer != NULL) {
                Tmd_SetupDraw(node);
            }
        }
        node = PARENT_OF(node->link.next, TmdObject, link);
    }
}

void Tmd_DrawActiveNodes(TmdObject* node)
{
    while (node != NULL) {
        if (!(node->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
            if (node->buffer != NULL) {
                Tmd_SetupDraw(node);
            }
        }
        node = PARENT_OF(node->link.next, TmdObject, link);
    }
}
