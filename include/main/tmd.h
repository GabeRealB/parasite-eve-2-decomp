#ifndef TMD_H
#define TMD_H

#include "common.h"

// Types — TMD model lists (src/main/tmd.c)

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

/// Node in the Tmd_List linked list (2F244.c TMD/model objects).
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
    /* 0x26 */ byte               unknown_26[0xA];
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

/// Head of the TmdObject linked list used by 2F244.c TMD/model helpers.
extern TmdListHead Tmd_List;
/// Second list head initialized alongside Tmd_List by Tmd_InitLists.
extern TmdListHead Tmd_ListAlt;

// --- APIs ---
void Tmd_ProcessStream(TmdObject* arg0);
void Tmd_SetupDraw(TmdObject* arg0);
void Tmd_AllocMissingBuffers(void);

/// Early-image handwritten GTE matrix load (src/main/hasm/Tmd_SetupGteMatrices.s).
/// `scratch` is the Tmd_SetupDraw scratch block (0x98 bytes).
void Tmd_SetupGteMatrices(void* scratch, u32 flags, void* stream, TmdObject* node);
/// Early-image handwritten stream walker (src/main/hasm/Tmd_DispatchStream.s).
void Tmd_DispatchStream(void* scratch, u32 flags, void* stream);

#endif // TMD_H
