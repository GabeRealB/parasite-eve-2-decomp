#ifndef TMD_H
#define TMD_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

// Types — TMD model lists (src/main/tmd.c; stage fade/MDEC lives in stage.c)

/// Source/model data pointed to by TmdObject::field_10 (see Tmd_Create).
typedef struct _TmdSource {
    /* 0x00 */ s32  field_0;  // one-shot init flag (set 1)
    /* 0x04 */ s32  field_4;  // byte count for aux alloc (calloc size * 2)
    /* 0x08 */ s32  field_8;  // offset into half-buffer base
    /* 0x0C */ byte unknown_C[0x8];
    /* 0x14 */ s32  field_14; // copied to scratch ws
    /* 0x18 */ s32  field_18; // copied to scratch ws
    /* 0x1C */ byte unknown_1C[0x4];
    /* 0x20 */ u32* field_20; // [id, handler_slot, dims, data…] stream
} TmdSource;

/// Node in the Tmd_List linked list (tmd.c TMD/model objects).
typedef struct _TmdObject {
    /* 0x00 */ struct _TmdObject* next;
    /* 0x04 */ byte               unknown_4[0x4];
    /* 0x08 */ void*              field_8;
    /* 0x0C */ u16                field_C;
    /* 0x0E */ s8                 field_E;
    /* 0x0F */ byte               unknown_F;
    /* 0x10 */ TmdSource*         field_10; // source / model data
    /* 0x14 */ u16                field_14; // cleared when buffers alloc
    /* 0x16 */ u16                field_16;
    /* 0x18 */ void*              field_18; // aux buffer (Tmd_AllocBuffers)
    /* 0x1C */ void*              field_1C;
    /* 0x20 */ void*              field_20;
    /* 0x24 */ u8                 field_24;
    /* 0x25 */ u8                 field_25;
    /* 0x26 */ u8                 field_26;
    /* 0x27 */ u8                 field_27;
    /* 0x28 */ byte               unknown_28[0x8];
    /* 0x30 */ s32                field_30;
} TmdObject;
STATIC_ASSERT_SIZEOF(TmdObject, 0x34);

/// Sentinel list head for TmdObject (and similar) intrusive lists.
/// Same layout as TaskNode: next is the first element, prev is the last
/// (or &self when the list is empty). Initialized by Tmd_InitLists.
typedef struct _TmdListHead {
    /* 0x00 */ TmdObject*           next;
    /* 0x04 */ struct _TmdListHead* prev;
} TmdListHead;
STATIC_ASSERT_SIZEOF(TmdListHead, 0x8);

/// Head of the TmdObject linked list used by tmd.c TMD/model helpers.
extern TmdListHead Tmd_List;
/// Second list head initialized alongside Tmd_List by Tmd_InitLists.
extern TmdListHead Tmd_ListAlt;
/// Cleared by Tmd_InitLists during system init.
extern s32 D_80071210;

/// 0x88-byte scratch from G_SCRATCH_HEAD for Tmd_ProcessStream (model path).
typedef struct {
    /* 0x00 */ u8*        field_0;
    /* 0x04 */ u8*        field_4;
    /* 0x08 */ s32        field_8;
    /* 0x0C */ s32        field_C;
    /* 0x10 */ s32*       field_10;
    /* 0x14 */ byte       pad_14[0x4];
    /* 0x18 */ s32        field_18;
    /* 0x1C */ s32        field_1C;
    /* 0x20 */ u32        field_20;
    /* 0x24 */ s32        field_24;
    /* 0x28 */ s32        field_28;
    /* 0x2C */ byte       pad_2C[0x44];
    /* 0x70 */ s16        field_70;
    /* 0x72 */ s16        field_72;
    /* 0x74 */ byte       pad_74[0xC];
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
