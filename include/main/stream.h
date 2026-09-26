#ifndef STREAM_H
#define STREAM_H

#include "common.h"

// Types — stream slots (src/main/stream.c)

/// Element of BSS array Stream_Slots (15 entries, total 0x258).
/// Fields inferred from F344.c accessors (Stream_InitFromSlot, Stream_FindSlotByKey, etc.).
typedef struct _StreamSlot {
    /* 0x00 */ s16  field_0;
    /* 0x02 */ byte unknown_2[0x2];
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ u16  field_C;
    /* 0x0E */ u16  field_E;
    /* 0x10 */ u16  field_10;
    /* 0x12 */ u16  field_12;
    /* 0x14 */ u16  field_14;
    /* 0x16 */ u16  field_16;
    /* 0x18 */ u16  field_18;
    /* 0x1A */ u16  field_1A;
    /* 0x1C */ u16  field_1C;
    /* 0x1E */ u16  field_1E;
    /* 0x20 */ byte unknown_20[0x2];
    /* 0x22 */ u16  field_22;
    /* 0x24 */ u16  field_24;
    /* 0x26 */ u16  field_26;
} StreamSlot;
STATIC_ASSERT_SIZEOF(StreamSlot, 0x28);

// Globals
extern StreamSlot Stream_Slots[15];

// --- APIs ---
void        Mdec_ResolveStreamBuffer(u8* arg0);
void        Mdec_BeginDecode(void* arg0);
u32         func_8001F180(u32 arg0);
u16         Stream_RestoreAfterLoad(s32 arg0, s32 arg1);
void        Stream_ResetRestoreState(void);
void        func_80020058(void);
s16         Stream_FindSlot(u8* arg0, s32 arg1, s32 arg2);
s16         Stream_FindSlotByKey(u8* arg0);
StreamSlot* Stream_GetSlot(u32 arg0);
u16         Stream_GetSlotField1A(u32 arg0);
void        Stream_KickDecode(u32 arg0);
s16         Stream_HasActiveLowId(void* arg0);

extern u16 D_8005EAEC;
extern u16 D_8005EAEE;
/// ISO root scan: LBA of the first `.STR` (stream) file, plus a sibling word.
typedef struct {
    s32 sector;
    s32 field_4;
} FsStrInfo;

#ifndef STREAM_C
extern FsStrInfo D_8006AC30;
extern u16       D_8006AC3C;
extern void*     D_8006AC40;
extern void*     D_8006AC44;
extern u_long*   D_8006AC48[];
extern u_long*   D_8006AC50[];
extern u16       D_8006AC58;
extern u16       D_8006AC5A;
extern u16       D_8006AC5C;
extern u16*      D_8006AC60;
extern u16       D_8006AC6C;
#endif
extern s32 StCdIntrFlag;

#endif // STREAM_H
