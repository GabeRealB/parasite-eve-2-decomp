#ifndef TMD_H
#define TMD_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

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

struct _TmdListHead;

/// An attached model body: an element of `gTmdList` and the object a
/// spawnType-1 `Task` carries in `Task::extra`. `Gp_AttachTmd` /
/// `Gp_AttachTmdFlags` store the `Tmd_Create` result straight into
/// `Task::extra` and set `Task::spawnType = 1`, `Task_Kill`'s type-1 path ORs
/// 0x80 into `field_C` of that same pointer, and `Gp_FreeTmd` releases it. The
/// spawnType-2 alternative (`Gp_AttachDisp2d`) is a different object,
/// `GpDisp2d`, sharing only the `next` / `prev` / `firstRegionSize` / `field_C` prefix.
/// This type was also modelled separately as `GameActorExt` (0x24, `firstRegionSize` as
/// `s32*`, 0x0E..0x17 padded over); that duplicate is gone.
///
/// `Tmd_Create` allocates the header and the per-part coordinate array as one
/// `Mem_Calloc(partCount * 0x50 + 0x34, 0)` block, then points `firstRegionSize` at
/// `(u8*)this + 0x34`, so `firstRegionSize` is this object's own trailing
/// `GsCOORDINATE2[field_30]`. The init loop writes each element's `coord`
/// (+0x04, 0x20 bytes copied from the matching `TmdBone.local`), clears `flg` (+0x00)
/// and links `sub` (+0x4C) to the parent part or `Gfx_ViewCoord`, which is why
/// every caller walks `firstRegionSize` with a 0x50 stride and why `Display_SpawnFromMode`
/// can clear the root `flg` through it.
///
/// `field_C` starts at 0x80 and gains bit 0x4 when `Tmd_Create`'s `flags & 1`.
/// `Task_Kill` ORs 0x80 (type-1 deferred kill); `Gp_WaitItemFlag2` writes 8 on
/// first run and clears bit 0x8 before `Task_CallExit`. A non-NULL `normals`
/// lets `Gp_UpdateActorColor` rebuild the color matrix even when
/// `gGameSession->field_65 == 1` (unless bit 0x80 of `field_C` is set).
/// `field_1C` / `stream` are the light and color matrices `Tmd_SetupDraw`
/// loads (`GsLIGHTWSMATRIX` / `D_80074080` by default, `Gp_DefaultMtx` /
/// `Gp_DefaultMtx2` after `Gp_BindDefaultMtx`).
typedef struct _TmdObject {
    /* 0x00 */ struct _TmdObject*   next;
    /* 0x04 */ struct _TmdListHead* prev;    // Previous node, or the head sentinel
    /* 0x08 */ GsCOORDINATE2*       field_8; // trailing per-part coord array, `this + 0x34`
    /* 0x0C */ u16                  field_C;
    /* 0x0E */ s8                   field_E;
    /* 0x0F */ byte                 unknown_F;
    /* 0x10 */ TmdSource*           field_10; // source / model data
    /* 0x14 */ u16                  field_14; // cleared when buffers alloc
    /* 0x16 */ u16                  field_16;
    /* 0x18 */ void*                field_18; // aux buffer (Tmd_AllocBuffers)
    /* 0x1C */ MATRIX*              field_1C; // light matrix
    /* 0x20 */ MATRIX*              field_20; // color matrix
    /* 0x24 */ u8                   field_24; // texture page, Tmd_ProcessStream ws.field_70
    /* 0x25 */ u8                   field_25; // CLUT row, ws.field_72 = field_25 << 6
    /* 0x26 */ u8                   field_26;
    /* 0x27 */ u8                   field_27;
    /* 0x28 */ byte                 unknown_28[0x4];
    /* 0x2C */ s32                  field_2C; // lighting intensity (>> 5 into RGB)
    /* 0x30 */ s32                  field_30; // part count, copied from TmdSource.partCount
} TmdObject;
STATIC_ASSERT_SIZEOF(TmdObject, 0x34);

/// Sentinel at the head of an intrusive list whose elements carry their own
/// `next` and `prev` as their first two fields. `next` is the first element,
/// NULL when the list is empty; `prev` is the last element, or the sentinel
/// itself when it is empty.
typedef struct _TmdListHead {
    /* 0x00 */ TmdObject*           next;
    /* 0x04 */ struct _TmdListHead* prev;
} TmdListHead;
STATIC_ASSERT_SIZEOF(TmdListHead, 0x8);

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
/// A node here carries the model list's `next` / `prev` prefix and a single
/// coordinate rather than a model with parts. The draw pass refreshes that
/// coordinate for every node on the list before it reaches the models, and
/// draws nothing from it; the task that attached the node reads the refreshed
/// matrix. The two lists are saved, emptied and restored together.
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
void       Tmd_ProcessStream(TmdObject* arg0);
void       Tmd_SetupDraw(TmdObject* arg0);
void       Tmd_AllocMissingBuffers(void);
s32        Tmd_AllocBuffers(TmdObject* arg0);
void       Tmd_FreeBuffers(TmdObject* arg0);

/// Early-image handwritten GTE matrix load (src/main/hasm/Tmd_SetupGteMatrices.s).
void Tmd_SetupGteMatrices(TmdScratchDrawBlock* ws, u32 flags, void* stream, TmdObject* node);
/// Walk stream records and jalr each draw handler until terminator -2.
u32* Tmd_DispatchStream(TmdScratchDrawBlock* ws, s32 flags, u32* stream);
void Tmd_DrawFlaggedNodes(TmdObject* node);
void Tmd_DrawActiveNodes(TmdObject* node);

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
