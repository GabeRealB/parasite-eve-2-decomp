#ifndef TMD_H
#define TMD_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

struct Task;

// Types — TMD model lists (src/main/tmd.c; stage fade/MDEC lives in stage.c)

/// One entry of the `TmdSource.skeleton` array: a bone's rest transform and the
/// bone it hangs from.
///
/// The array is the model's rest pose, one bone per part. A part's vertices are
/// authored around its own bone, so the skeleton is what puts the parts of a
/// model back in one place: `Tmd_Create` builds the model's per-part coordinate
/// array from these, and the matrices composed from it are what each part is
/// drawn under.
typedef struct {
    MATRIX local;  // Rest transform, in the space of the bone it hangs from
    s32    parent; // Bone it hangs from, as an index into the same array; its own index at the root
} TmdBone;
STATIC_ASSERT_SIZEOF(TmdBone, 0x24);

/// One model as its package ships it: the vertex and normal arrays, the packet
/// stream that draws its parts, and the bone rest pose that places them.
///
/// A package lays a model out as `[vertices][normals][packet stream][record]`
/// with the record last, and every pointer here is an address into that same
/// package, so a model stays whole at whatever address its package loads. A
/// `TmdObject` points here, and the two divide the work between them: the
/// record carries what shipped, while the buffer the model is decoded into and
/// the per-part coordinate array belong to the object.
///
/// The record is not all read-only. The packet stream is resolved to handlers
/// in place the first time the model is used, and `handlersResolved` is how the
/// record says that has happened.
typedef struct {
    s32      handlersResolved; // Zero as shipped, set once the packet stream has been resolved to handlers
    s32      halfSize;         // Size of one half of the model's buffer in bytes; the object allocates both halves together
    s32      firstRegionSize;  // Size of the first of a half's two prim regions, i.e. the offset the second starts at
    s32      partCount;        // Parts the model is divided into; one bone each
    u32*     partVerts;        // Vertex count per part, summing to the vertex array's length
    SVECTOR* verts;            // Vertices, grouped by part
    SVECTOR* normals;          // Normals, indexed independently of the vertices
    TmdBone* skeleton;         // Rest pose: one bone per part, carrying its parent index
    u32*     stream;           // Packet stream, the drawing instructions for the model's parts
} TmdSource;
STATIC_ASSERT_SIZEOF(TmdSource, 0x24);

/// One node of one of the TMD lists: the link pair a head and every element on
/// it carry as their first two fields.
///
/// A head is a bare node belonging to no element. Its `next` is the first node
/// and its `prev` the last, which is the head itself while the list is empty,
/// so a walk stops on `next` alone and the head is reachable only through
/// `prev`. Linking and unlinking are written against the pair rather than
/// against whichever type the elements are, so an unlink body differs from the
/// next one only in the head it names.
typedef struct _TmdListHead {
    struct _TmdListHead* next; // Following node, or NULL past the last
    struct _TmdListHead* prev; // Preceding node, or the head at the front
} TmdListHead;
STATIC_ASSERT_SIZEOF(TmdListHead, 0x8);

/// An attached model body: an element of `gTmdList` and the object a
/// spawnType-1 `Task` carries in `Task::extra`.
///
/// The object owns what a model needs at run time — the buffer its packet
/// stream is decoded into, the coordinate array that places its parts, the
/// light and colour matrices it is drawn under — while the `TmdSource` it
/// points at owns what the package shipped. `coords` is part of this object's
/// own allocation, laid out directly after it, so the two are one block.
///
/// The buffer holds two halves and the passes alternate between them, so no
/// pass reads the half it writes: `bufferIndex` selects the half in use and
/// each pass flips it.
typedef struct {
    TmdListHead*   next;        // Following node of the list, or NULL past the last
    TmdListHead*   prev;        // Preceding node, or the head at the front
    GsCOORDINATE2* coords;      // Per-part coordinate array, part of this object's own block
    u16            flags;       // State bits (0x4 buffer allocated by whoever created it, 0x8 drawn by the flagged pass, 0x80 hidden)
    s8             otOffset;    // Ordering-table offset the model's primitives are linked at
    byte           unknown_F;
    TmdSource*     source;      // The model as its package shipped it
    u16            bufferIndex; // Which half of the buffer is in use (0/1); each pass flips it
    u16            halfSize;    // Size of one buffer half, cached from the source
    void*          buffer;      // Both buffer halves, allocated together; NULL while there are none
    MATRIX*        lightMtx;    // Light matrix the model is drawn under
    MATRIX*        colorMtx;    // Colour matrix the model is drawn under
    u8             tpage;       // Texture page the model's primitives are offset by
    u8             clut;        // CLUT the model's primitives are offset by, in 64-entry rows
    u8             tpageOffset; // Further texture page offset the handlers that use one add to a primitive
    u8             clutOffset;  // Further CLUT row offset the handlers that use one add, in 64-entry rows
    byte           unknown_28[0x4];
    s32            lightLevel;  // Lighting, 12.4 fixed point (0x1000 fully lit)
    s32            partCount;   // Parts the model is divided into, cached from the source
} TmdObject;
STATIC_ASSERT_SIZEOF(TmdObject, 0x34);

/// Head of the model list: the anchor every attached `TmdObject` hangs from.
///
/// A model is linked here when its task attaches it and unlinked when the task
/// releases it, so the list is the model subsystem's whole view of what is
/// currently loaded. The size, buffer, draw and free passes work from it
/// instead of walking the task list.
extern TmdListHead gTmdList;
/// Head of the 2D-display list: the anchor for the coordinate nodes a task
/// attaches in place of a model.
///
/// A node here carries a single coordinate rather than a model with parts, so
/// the entry that follows the link pair is the coordinate rather than the
/// per-part array. The draw pass refreshes that coordinate for every node on
/// the list before it reaches the models and draws nothing from it; the task
/// that attached the node reads the refreshed matrix. The two lists are saved,
/// emptied and restored together.
extern TmdListHead gTmdDisp2dList;
/// Cleared by Tmd_InitLists during system init.
extern s32 D_80071210;

/// 0x88-byte scratch from G_SCRATCH_HEAD for Tmd_ProcessStream (model path).
typedef struct {
    /* 0x00 */ u8*        field_0;
    /* 0x04 */ u8*        field_4;
    /* 0x08 */ s32        field_8;
    /* 0x0C */ s32        field_C;
    /* 0x10 */ s32*       field_10;
    /* 0x14 */ u_long*    field_14; // OT base (draw path; same slot as TmdScratchDrawBlock)
    /* 0x18 */ s32        field_18;
    /* 0x1C */ s32        field_1C;
    /* 0x20 */ u32        field_20;
    /* 0x24 */ s32        field_24;
    /* 0x28 */ s32        field_28;
    /* 0x2C */ byte       pad_2C[0x44];
    /* 0x70 */ s16        field_70;
    /* 0x72 */ s16        field_72;
    /* 0x74 */ u16        field_74;
    /* 0x76 */ u16        field_76;
    /* 0x78 */ u16        field_78;
    /* 0x7A */ u16        pad_7A;
    /* 0x7C */ u16        field_7C;
    /* 0x7E */ u16        field_7E;
    /* 0x80 */ TmdObject* field_80;
    /* 0x84 */ byte       pad_84[0x4];
} TmdScratchModelBlock;
STATIC_ASSERT_SIZEOF(TmdScratchModelBlock, 0x88);

/// 0x98-byte scratch for Tmd_SetupDraw (draw path).
typedef struct {
    /* 0x00 */ u8*        field_0;
    /* 0x04 */ u8*        field_4;
    /* 0x08 */ s32        field_8;
    /* 0x0C */ s32        field_C;
    /* 0x10 */ void*      field_10;
    /* 0x14 */ u_long*    field_14;
    /* 0x18 */ byte       pad_18[0x38]; // Dispatch stores 0x18/0x1C/0x20/0x2C/0x30
    /* 0x50 */ MATRIX     mat;
    /* 0x70 */ byte       pad_70[0x10];
    /* 0x80 */ TmdObject* field_80;
    /* 0x84 */ s32        field_84;
    /* 0x88 */ byte       pad_88[0x10];
} TmdScratchDrawBlock;
STATIC_ASSERT_SIZEOF(TmdScratchDrawBlock, 0x98);

/// Model-path stream command: Tmd_ProcessStream → handler(ws, flags, stream).
typedef u32* (*TmdModelStreamHandler)(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// Draw-path stream command: Tmd_DispatchStream jalr → handler(ws, flags, stream).
typedef u32* (*TmdDrawStreamHandler)(TmdScratchDrawBlock* ws, s32 flags, u32* stream);

// --- APIs ---
void       Tmd_InitLists(void);
TmdObject* Tmd_Create(TmdSource* src, s32 flags);
void       Tmd_ProcessStream(TmdObject* obj);
void       Tmd_SetupDraw(TmdObject* obj);
void       Tmd_AllocMissingBuffers(void);
s32        Tmd_AllocBuffers(TmdObject* obj);
void       Tmd_FreeBuffers(TmdObject* obj);

/// Early-image handwritten GTE matrix load (src/main/hasm/Tmd_SetupGteMatrices.s).
void Tmd_SetupGteMatrices(TmdScratchDrawBlock* ws, u32 flags, void* stream, TmdObject* node);
/// Walk stream records and jalr each draw handler until terminator -2.
u32* Tmd_DispatchStream(TmdScratchDrawBlock* ws, s32 flags, u32* stream);
void Tmd_DrawFlaggedNodes(TmdObject* node);
void Tmd_DrawActiveNodes(TmdObject* node);

// The per-frame callback of the task that holds the models' buffers, and the
// states it runs in sequence. Each state walks `gTmdList` whole and advances
// the task to the next, so the list is only ever worked from a task that owns
// the pass.
void Tmd_DispatchTask(struct Task* task);
/// Marks every attached model as no longer drawn.
void Tmd_FlagAllNodes(struct Task* task);
/// Releases the buffer of every attached model.
void Tmd_FreeNodeBuffers(struct Task* task);
/// Gives a buffer back to every attached model that has none, then kills the task.
void Tmd_AllocNodeBuffers(struct Task* task);
/// Total bytes the attached models hold in their buffers.
s32 Tmd_SumBufferBytes(void);

// Early-image handlers (src/main/hasm/Tmd_StreamHandler_*.s).
// Same ABI for model and draw scratch (shared offsets 0x18/0x1C/…); declared
// as model-side type for ProcessStream. Draw path is jalr from hasm only.
u32* Tmd_StreamHandler_Default(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Prim32(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Prim30(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Prim3A(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Prim38(TmdScratchModelBlock* ws, s32 flags, u32* stream);

// Early-image handlers in Tmd_StreamHandlers_Ops.s (named by stream opcode).
u32* Tmd_StreamHandler_Op20(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op60(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_OpC0(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op3A(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op38(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op7A(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op78(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_OpC8(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op3B(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op39(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op7B(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op79(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op00(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op40(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op1A(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op18(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op58(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op5A(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op130(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op170(TmdScratchModelBlock* ws, s32 flags, u32* stream);

#endif // TMD_H
