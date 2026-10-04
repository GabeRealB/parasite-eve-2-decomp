#include "main/tmd.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

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

enum {
    TMD_CREATE_SKIP_AUTO_BUFFER = 1 // Creation bit: defer allocation and skip missing-buffer recovery
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

static const TaskFuncTable3 Tmd_TaskStates;

static void Tmd_InitSourceStream(TmdSource* src);

static void Tmd_SetupDraw(TmdObject* obj);

/// Total bytes the attached models hold in their buffers.
static s32 Tmd_SumBufferBytes(void);

static void Tmd_RewriteOpcodes(TmdSource* src);

/// Excludes every attached model from the active draw pass.
static void Tmd_FlagAllNodes(Task* task);

/// Releases the buffer of every attached model.
static void Tmd_FreeNodeBuffers(Task* task);

u32* func_actor_403600_80136224(TmdStreamWorkspace* ws, s32 flags, u32* stream);

u32* func_actor_403600_80136500(TmdStreamWorkspace* ws, s32 flags, u32* stream);

u32* func_actor_403600_8013685C(TmdStreamWorkspace* ws, s32 flags, u32* stream);

u32* func_actor_403600_80136C00(TmdStreamWorkspace* ws, s32 flags, u32* stream);

u32* func_actor_403600_8013700C(TmdStreamWorkspace* ws, s32 flags, u32* stream);

u32* func_actor_403600_80137300(TmdStreamWorkspace* ws, s32 flags, u32* stream);

u32* func_actor_403600_801375F8(TmdStreamWorkspace* ws, s32 flags, u32* stream);

u32* func_actor_403600_801379B4(TmdStreamWorkspace* ws, s32 flags, u32* stream);

u32* func_actor_403600_80138004(TmdStreamWorkspace* ws, s32 flags, u32* stream);

u32* func_actor_403600_801386EC(TmdStreamWorkspace* ws, s32 flags, u32* stream);

static const TaskFuncTable3 Tmd_TaskStates = { {
    Tmd_FlagAllNodes,
    Tmd_FreeNodeBuffers,
    taskKill,
} };

static void Tmd_InitSourceStream(TmdSource* src)
{
    _TmdStreamWord*        stream;
    u32                    id;
    u32                    dims;
    _TmdModelStreamHandler handler;
    s32                    flag;
    u32                    locationDifference;

    stream = (_TmdStreamWord*)src->stream;
    if (src->handlersResolved == TMD_SOURCE_HANDLERS_UNRESOLVED) {
        locationDifference = GAME_LOCATION_WORD(gGameSession->location.loc);
        locationDifference = (locationDifference & GAME_LOCATION_STAGE_AREA_MASK) ^ GAME_LOCATION_KEY(2, 16, 0, 0);
        flag               = locationDifference < 1;
        goto read_id;

        for (;;) {
            switch (id) {
                case 0x20:
                case 0x22:
                    handler = tmdDrawStreamPrimG3CornerNormals;
                    break;
                case 0x60:
                case 0x62:
                    handler = tmdDrawStreamPrimG4CornerNormals;
                    break;
                case 0xC0:
                    handler = tmdXformStreamVertsElemColor;
                    break;
                case 0xC4:
                    handler = gpXformStreamVertsUnlit;
                    break;
                case 5:
                    handler = tmdDrawStreamPrimF3PreXform;
                    break;
                case 0x45:
                    handler = tmdDrawStreamPrimF4PreXform;
                    break;
                case 0x21:
                case 0x121:
                    handler = tmdDrawStreamPrimG3PreXform;
                    break;
                case 0x61:
                case 0x161:
                    handler = tmdDrawStreamPrimG4PreXform;
                    break;
                case 0x38:
                    handler = tmdDrawStreamGt3;
                    break;
                case 0x8038:
                    handler = func_actor_403600_80136224;
                    break;
                case 0x10038:
                    handler = func_actor_403600_8013700C;
                    break;
                case 0x20038:
                    handler = func_actor_403600_801379B4;
                    break;
                case 0x3A:
                    handler = tmdDrawStreamGt3SemiTrans;
                    break;
                case 0x1003A:
                    handler = func_actor_403600_80137300;
                    break;
                case 0x78:
                    handler = tmdDrawStreamGt4;
                    break;
                case 0x8078:
                    handler = func_actor_403600_8013685C;
                    break;
                case 0x10078:
                    handler = func_actor_403600_801375F8;
                    break;
                case 0x20078:
                    handler = func_actor_403600_80138004;
                    break;
                case 0x7A:
                    handler = tmdDrawStreamGt4SemiTrans;
                    break;
                case 0xC8:
                    handler = tmdXformStreamVerts;
                    break;
                case 0x40C8:
                    handler = func_8009AF90;
                    if (flag != 0) {
                        handler = gpXformStreamVertsOffsetLayer;
                    }
                    break;
                case 0x200C8:
                    handler = func_actor_403600_801386EC;
                    break;
                case 0x31:
                case 0x39:
                case 0x131:
                    handler = tmdDrawStreamPrimGt3PreXform;
                    break;
                case 0x8039:
                    handler = func_actor_403600_80136500;
                    break;
                case 0x3B:
                    handler = tmdDrawStreamPrimGt3PreXformSemiTrans;
                    break;
                case 0x71:
                case 0x79:
                case 0x171:
                    handler = tmdDrawStreamPrimGt4PreXform;
                    break;
                case 0x8079:
                    handler = func_actor_403600_80136C00;
                    break;
                case 0x7B:
                    handler = tmdDrawStreamPrimGt4PreXformSemiTrans;
                    break;
                case 0x4039:
                    handler = tmdDrawStreamPrimGt3PreXformEnvLayer;
                    if (flag != 0) {
                        handler = tmdDrawStreamPrimGt3PreXformOffsetLayer;
                    }
                    break;
                case 0x4079:
                    handler = tmdDrawStreamPrimGt4PreXformEnvLayer;
                    if (flag != 0) {
                        handler = tmdDrawStreamPrimGt4PreXformOffsetLayer;
                    }
                    break;
                case 0:
                    handler = tmdDrawStreamPrimG3;
                    break;
                case 0x40:
                    handler = tmdDrawStreamPrimG4;
                    break;
                case 0x18:
                    handler = tmdDrawStreamPrimGt3OneNormal;
                    break;
                case 0x1A:
                    handler = tmdDrawStreamPrimGt3OneNormalSemiTrans;
                    break;
                case 0x58:
                    handler = tmdDrawStreamPrimGt4OneNormal;
                    break;
                case 0x5A:
                    handler = tmdDrawStreamPrimGt4OneNormalSemiTrans;
                    break;
                case 0x4078:
                    handler = func_8009C414;
                    if (flag != 0) {
                        handler = gpDrawStreamPrimGt4OffsetLayer;
                    }
                    break;
                case 0x4038:
                    handler = func_8009B500;
                    if (flag != 0) {
                        handler = gpDrawStreamPrimGt3OffsetLayer;
                    }
                    break;
                case 0x120:
                    handler = func_8009E048;
                    break;
                case 0x122:
                    handler = func_8009E274;
                    break;
                case 0x160:
                    handler = func_8009E4A0;
                    break;
                case 0x162:
                    handler = gpDrawStreamPrimG4CornerColorsSemiTrans;
                    break;
                case 0x1C:
                    handler = func_8009D388;
                    break;
                case 0x1E:
                    handler = func_8009DCB8;
                    break;
                case 0x5C:
                    handler = func_8009D518;
                    break;
                case 0x5E:
                    handler = func_8009DE48;
                    break;
                case 0x30:
                    handler = gpDrawStreamPrimGt3ElemColor;
                    break;
                case 0x130:
                    handler = tmdDrawStreamPrimGt3CornerColors;
                    break;
                case 0x70:
                    handler = gpDrawStreamPrimGt4ElemColor;
                    break;
                case 0x170:
                    handler = tmdDrawStreamPrimGt4CornerColors;
                    break;
                case 0x156:
                    handler = func_8009D718;
                    break;
                case 4:
                    handler = func_8009DB00;
                    break;
                case 0x44:
                    handler = func_8009D900;
                    break;
                default:
                    handler = tmdSkipStreamRecord;
                    break;
            }

            // Resolve the draw slot, then skip the count * stride payload words.
            stream++;
            stream->drawHandler = handler;
            stream++;
            dims = stream->dataWord;
            stream++;
            id      = dims & 0xFFFF;
            stream += (dims >> 16) * id;
            id      = stream->dataWord;

            while (1) {
                if (id != TMD_STREAM_GROUP_END) {
                    break;
                }
                stream++;
            read_id:
                id = stream->dataWord;
                // Terminator at entry, or after a group marker. Leave the word in place.
                if (id == TMD_STREAM_END) {
                    goto done;
                }
            }
        }
    done:
        src->handlersResolved = TMD_SOURCE_HANDLERS_RESOLVED;
    }
}

void tmdProcessStream(TmdObject* obj)
{
    TmdStreamWorkspace*    ws;
    TmdSource*             src;
    u32*                   stream;
    u32                    id;
    _TmdModelStreamHandler handler;
    s32                    flag;
    void*                  buf;
    u32                    stageAreaKey;
    TmdStreamWorkspace*    head;
    TmdStreamWorkspace*    tmp;

    flag                                     = 0;
    src                                      = obj->source;
    tmp                                      = SCRATCH_STACK_CURSOR(TmdStreamWorkspace);
    stream                                   = src->stream;
    stageAreaKey                             = GAME_LOCATION_WORD(gGameSession->location.loc);
    head                                     = tmp - 1;
    stageAreaKey                            &= GAME_LOCATION_STAGE_AREA_MASK;
    SCRATCH_STACK_CURSOR(TmdStreamWorkspace) = head;
    if ((stageAreaKey == GAME_LOCATION_KEY(2, 15, 0, 0)) || (stageAreaKey == GAME_LOCATION_KEY(2, 16, 0, 0))) {
        flag = 1;
    }
    ws = head;

    ws->obj       = obj;
    buf           = obj->buffer;
    ws->primWrite = buf;
    if (obj->nextBufferHalf != 0) {
        ws->primWrite = (u8*)buf + obj->bufferHalfBytes;
    }
    ws->preXformWrite     = ws->primWrite;
    ws->primWrite         = ws->primWrite + src->preXformRegionBytes;
    obj->nextBufferHalf  ^= 1;
    ws->verts             = obj->source->verts;
    ws->normals           = obj->source->normals;
    ws->texturePageOffset = obj->texturePageOffset;
    // Place the signed row count at bit 6 of the encoded CLUT word.
    ws->encodedClutOffset = obj->clutRowOffset << TMD_ENCODED_CLUT_ROW_SHIFT;
    goto read_id;

    for (;;) {
        switch (id) {
            case 0x4038:
                handler = tmdBuildStreamGt3LayeredBase;
                if (flag != 0) {
                    handler = tmdBuildStreamGt3OffsetLayer;
                }
                break;
            case 0x38:
            case 0x3A:
            case 0x8038:
            case 0x10038:
            case 0x1003A:
            case 0x20038:
                handler = tmdBuildStreamGt3;
                break;
            case 0x4078:
                handler = tmdBuildStreamGt4LayeredBase;
                if (flag != 0) {
                    handler = tmdBuildStreamGt4OffsetLayer;
                }
                break;
            case 0x78:
            case 0x7A:
            case 0x8078:
            case 0x10078:
            case 0x20078:
                handler = tmdBuildStreamGt4;
                break;
            case 0x31:
            case 0x39:
            case 0x3B:
            case 0x131:
            case 0x8039:
                handler = tmdBuildStreamGt3PreXform;
                break;
            case 0x71:
            case 0x79:
            case 0x7B:
            case 0x171:
            case 0x8079:
                handler = gpStreamPrimGt4PreXform;
                break;
            case 0x4039:
                handler = tmdBuildStreamGt3PreXformEnvLayer;
                if (flag != 0) {
                    handler = tmdBuildStreamGt3PreXformOffsetLayer;
                }
                break;
            case 0x4079:
                handler = gpStreamPrimGt4PreXformLayer;
                if (flag != 0) {
                    handler = gpStreamPrimGt4PreXformOffsetLayer;
                }
                break;
            case 0x18:
            case 0x1A:
                handler = tmdBuildStreamGt3OneNormal;
                break;
            case 0x58:
            case 0x5A:
                handler = gpStreamPrimGt4OneNormal;
                break;
            case 0x1C:
            case 0x1E:
                handler = modelLightingStreamPrimFt3;
                break;
            case 0x5C:
            case 0x5E:
                handler = modelLightingStreamPrimFt4;
                break;
            case 0x30:
                handler = tmdBuildStreamGt3ElemColor;
                break;
            case 0x130:
                handler = tmdBuildStreamGt3CornerColors;
                break;
            case 0x70:
                handler = tmdBuildStreamGt4ElemColor;
                break;
            case 0x170:
                handler = tmdBuildStreamGt4CornerColors;
                break;
            case 0x156:
                handler = gpStreamPrimGt4Unlit;
                break;
            case 4:
                handler = modelLightingStreamPrimF3;
                break;
            case 0x44:
                handler = modelLightingStreamPrimF4;
                break;
            case 5:
                handler = modelLightingStreamPrimF3PreXform;
                break;
            case 0x45:
                handler = modelLightingStreamPrimF4PreXform;
                break;
            case 0x40:
            case 0x60:
            case 0x160:
            case 0x4040:
            case 0x4060:
            case 0x4160:
                handler = modelLightingReserveStreamPrimG4;
                break;
            default:
                handler = tmdSkipStreamRecord;
                break;
            case 0:
            case 0x20:
            case 0x120:
            case 0x4000:
            case 0x4020:
            case 0x4120:
                handler = modelLightingReserveStreamPrimG3;
                break;
        }

        ws->opcode     = *stream;
        stream        += 2;
        ws->elemStride = ((u16*)stream)[0];
        ws->elemCount  = ((u16*)stream)[1];
        stream        += 1;
        stream         = handler(ws, 0, stream);
        id             = *stream;

        while (1) {
            if (id != TMD_STREAM_GROUP_END) {
                break;
            }
            stream++;
        read_id:
            id = *stream;
            // Terminator at entry, or after a group marker. Leave the word in place.
            if (id == TMD_STREAM_END) {
                goto done;
            }
        }
    }
done:
    SCRATCH_STACK_RELEASE_BLOCK(TmdStreamWorkspace);
}

TmdObject* Tmd_Create(TmdSource* src, s32 bufferFlags)
{
    TmdAllocation* allocation;
    TmdObject*     obj;
    GfxCoord*      coord;
    const TmdBone* bone;
    u32            partIndex;
    void*          buffer = NULL;

    Tmd_InitSourceStream(src);
    allocation = memCalloc((src->partCount * sizeof(allocation->coords[0])) + sizeof(*allocation), 0);
    obj        = allocation != NULL ? &allocation->object : NULL;
    if (obj != NULL) {
        obj->flags             = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        obj->partCount         = src->partCount;
        obj->coords            = allocation->coords;
        obj->nextBufferHalf    = 0;
        coord                  = obj->coords;
        obj->bufferHalfBytes   = src->bufferHalfBytes;
        obj->lightMtx          = &GsLIGHTWSMATRIX;
        obj->colorMtx          = &D_80074080;
        obj->texturePageOffset = 0;
        obj->clutRowOffset     = 0;
        obj->source            = src;
        bone                   = src->skeleton;
        // Copy the initial pose; self-parented roots attach to the view coordinate.
        for (partIndex = 0; partIndex < (u32)obj->partCount; partIndex++) {
            coord->coord = bone->local;
            if (bone->parentIndex != partIndex) {
                coord->parent = &obj->coords[bone->parentIndex];
            } else {
                coord->parent = &gGfxViewCoord;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord++;
            bone++;
        }
        obj->buffer = NULL;
        if (bufferFlags == 0) {
            buffer = memCalloc(src->bufferHalfBytes * TMD_BUFFER_HALF_COUNT, 1);
            if (buffer != NULL) {
                obj->buffer = buffer;
                // Initialize both buffer halves before the first draw.
                tmdProcessStream(obj);
                tmdProcessStream(obj);
            }
        } else if (bufferFlags & TMD_CREATE_SKIP_AUTO_BUFFER) {
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        }
    }
    return obj;
}

static void Tmd_SetupDraw(TmdObject* obj)
{
    s32              vertexDepths[TMD_DRAW_VERTEX_DEPTH_COUNT];
    _TmdDrawScratch* scratchEnd;
    _TmdDrawScratch* scratch;
    void*            stream;
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

    Tmd_SetupGteMatrices(&scratch->workspace, flags, stream, obj);

    SCRATCH_STACK_RELEASE_BLOCK(_TmdDrawScratch);
}

void Tmd_FreeBuffers(TmdObject* obj)
{
    if (obj->buffer != NULL) {
        memFreeFromHeap(obj->buffer, true);
        obj->buffer = NULL;
    }
}

s32 Tmd_AllocBuffers(TmdObject* obj)
{
    s32   result;
    void* mem;

    result = 0;
    if (obj->buffer == NULL) {
        mem         = memCalloc(obj->source->bufferHalfBytes * TMD_BUFFER_HALF_COUNT, 1);
        obj->buffer = mem;
        if (mem != NULL) {
            obj->nextBufferHalf = 0;
            tmdProcessStream(obj);
            tmdProcessStream(obj);
            result = 1;
        }
    }
    return result;
}

/// Total bytes the attached models hold in their buffers.
static s32 Tmd_SumBufferBytes(void)
{
    TmdObject* node;
    s32        result;

    result = 0;
    node   = PARENT_OF(gTmdList.next, TmdObject, link);
    while (node != NULL) {
        if (node->buffer != NULL) {
            result += node->source->bufferHalfBytes * TMD_BUFFER_HALF_COUNT;
        }
        node = PARENT_OF(node->link.next, TmdObject, link);
    }
    return result;
}

static void Tmd_RewriteOpcodes(TmdSource* src)
{
    u32* stream;
    u32  id;
    u32  dims;
    u32  lo;
    u32  groupEnd;

    stream = src->stream;
    // A stream whose first word is the terminator has no groups to rewrite.
    if (*stream != TMD_STREAM_END) {
        groupEnd = TMD_STREAM_GROUP_END;
        do {
            if (*stream != groupEnd) {
                do {
                    id = *stream;
                    if (id == 0x3B) {
                        goto case_advance;
                    }
                    if (id < 0x3CU) {
                        if (id == 0x38) {
                            goto case_38;
                        }
                        goto default_advance;
                    }
                    if (id == 0x79) {
                        goto case_advance;
                    }
                    if (id >= 0x7AU) {
                        goto case_advance;
                    }
                    if (id == 0x78) {
                        goto case_78;
                    }
                    goto default_advance;

                case_38:
                    *stream = 0x4038;
                    goto case_advance;
                case_78:
                    *stream = 0x4078;
                case_advance:
                    stream += 2;
                    goto after;
                default_advance:
                    stream += 2;
                after:
                    dims = *stream;
                    lo   = dims & 0xFFFF;
                    stream++;
                    stream += (dims >> 16) * lo;
                } while (*stream != TMD_STREAM_GROUP_END);
            }
            stream++;
        } while (*stream != TMD_STREAM_END);
    }
}

/// Excludes every attached model from the active draw pass.
static void Tmd_FlagAllNodes(Task* task)
{
    TmdObject* node;

    node = PARENT_OF(gTmdList.next, TmdObject, link);
    while (node != NULL) {
        node->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        node         = PARENT_OF(node->link.next, TmdObject, link);
    }
    task->state++;
}

/// Releases the buffer of every attached model.
static void Tmd_FreeNodeBuffers(Task* task)
{
    TmdObject* node;

    node = PARENT_OF(gTmdList.next, TmdObject, link);
    while (node != NULL) {
        if (node->buffer != NULL) {
            memFreeFromHeap(node->buffer, true);
            node->buffer = NULL;
        }
        node = PARENT_OF(node->link.next, TmdObject, link);
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
    Mem_InitAux();
    CdCmd_SetupMdecBuffers();
    while (node != NULL) {
        if (node->buffer == NULL) {
            if (!(node->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
                mem = memCalloc(node->source->bufferHalfBytes * TMD_BUFFER_HALF_COUNT, 1);
                if (mem != NULL) {
                    node->buffer         = mem;
                    node->nextBufferHalf = 0;
                    tmdProcessStream(node);
                    tmdProcessStream(node);
                }
            }
        }
        node = PARENT_OF(node->link.next, TmdObject, link);
    }
}

void Tmd_AllocNodeBuffers(Task* task)
{
    TmdObject* node;
    void*      mem;

    node = PARENT_OF(gTmdList.next, TmdObject, link);
    while (node != NULL) {
        if (node->buffer == NULL) {
            mem = memCalloc(node->source->bufferHalfBytes * TMD_BUFFER_HALF_COUNT, 1);
            if (mem != NULL) {
                node->buffer         = mem;
                node->nextBufferHalf = 0;
                node->flags         &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
                tmdProcessStream(node);
                tmdProcessStream(node);
            }
        }
        node = PARENT_OF(node->link.next, TmdObject, link);
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
