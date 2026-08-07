#ifndef CDAUDIO_H
#define CDAUDIO_H

#include "common.h"

// Types — CD audio player (src/main/cdaudio.c)

/// CD audio player phase machine (CdAudio_Phase). Driven by switches in 46FE4.c.
typedef struct _CdAudioPhase {
    /* 0x0 */ u8 field_0; // primary phase
    /* 0x1 */ u8 field_1; // sub-phase
    /* 0x2 */ u8 field_2; // sub-phase / gate
    /* 0x3 */ u8 field_3; // stream sub-phase
    /* 0x4 */ u8 field_4; // seek / load sub-phase
    /* 0x5 */ u8 field_5; // control / abort flag
} CdAudioPhase;
STATIC_ASSERT_SIZEOF(CdAudioPhase, 0x6);
/// BSS object CdAudio_Loc (size 0x10). Sector / pitch state for CD audio player.
typedef struct _CdAudioLoc {
    /* 0x0 */ u8  field_0; // status
    /* 0x1 */ u8  field_1; // ready / done flag
    /* 0x2 */ u16 field_2; // pitch-related
    /* 0x4 */ s32 field_4; // sector / position
    /* 0x8 */ s32 field_8; // SPU / buffer param
    /* 0xC */ s32 field_C; // sector position
} CdAudioLoc;
STATIC_ASSERT_SIZEOF(CdAudioLoc, 0x10);
/// BSS object CdAudio_Tbl (size 0x18). CD/audio stream state for 46FE4.c.
/// field_C is a base pointer into a halfword table; CdAudio_LoadSectorEntry indexes it
/// with ((packed >> 14) & 0x3FC) / 2 (4-byte stride, low halfword of each slot).
typedef struct _CdAudioTbl {
    /* 0x00 */ u8   field_0;
    /* 0x01 */ u8   field_1;
    /* 0x02 */ u8   field_2; // index into CdAudio_TblEntries
    /* 0x03 */ u8   pad_3;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ u16* field_C;  // halfword table base (CdAudio_LoadSectorEntry)
    /* 0x10 */ s32  field_10; // transfer / SpuWrite param
    /* 0x14 */ s32  field_14;
} CdAudioTbl;
STATIC_ASSERT_SIZEOF(CdAudioTbl, 0x18);
/// BSS object CdAudio_Ctl (size 0x14). CD stream control for 46FE4.c.
typedef struct _CdAudioCtl {
    /* 0x00 */ s32 field_0;  // busy / retry counter
    /* 0x04 */ s32 field_4;  // secondary counter
    /* 0x08 */ u8  field_8;  // phase mirror
    /* 0x09 */ u8  field_9;
    /* 0x0A */ s8  field_A;  // error code (-1 / -2)
    /* 0x0B */ u8  field_B;
    /* 0x0C */ s32 field_C;  // countdown
    /* 0x10 */ s32 field_10; // control flag
} CdAudioCtl;
STATIC_ASSERT_SIZEOF(CdAudioCtl, 0x14);
/// 4-byte entry pointed to by CdAudio_TblEntries (see CdAudio_PrepareNextEntry).
/// Indexed by CdAudio_Tbl.field_2; field_3 is compared across adjacent entries.
typedef struct _CdAudioTblEntry {
    /* 0x0 */ u8 pad[3];
    /* 0x3 */ u8 field_3; // compared across adjacent entries for span
} CdAudioTblEntry;
STATIC_ASSERT_SIZEOF(CdAudioTblEntry, 0x4);
/// BSS block covering CdAudio_Loc (0x10) + D_800827B0 (0x4). Immediately precedes
/// LinInterp_CdStream; used when codegen holds &LinInterp_CdStream and reaches back 0x14 bytes.
typedef struct _CdAudioLocEx {
    /* 0x00 */ u8  field_0;
    /* 0x01 */ u8  field_1;
    /* 0x02 */ u16 field_2;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} CdAudioLocEx;
STATIC_ASSERT_SIZEOF(CdAudioLocEx, 0x14);
/// Extended view of the CdAudio_Loc BSS block for SPU voice indices at +0x3E/+0x3F
/// (used by CdAudio_DrivePhase0 / CdAudio_DriveRead). The zero-init in CdAudio_Init covers
/// 0x44 bytes from CdAudio_Loc, so these offsets sit inside that block.
typedef struct _CdAudioVoices {
    /* 0x00 */ u8 pad[0x3E];
    /* 0x3E */ s8 field_3E;
    /* 0x3F */ s8 field_3F;
} CdAudioVoices;
STATIC_ASSERT_SIZEOF(CdAudioVoices, 0x40);

// --- APIs (from unknown_syms) ---
s32  CdAudio_Begin(void);
void CdAudio_Init(void);
u8   CdAudio_GetState(void);
void CdAudio_Tick(void);
s32  CdAudio_StartTrack(s32 arg0, s32 arg1);
s32  CdAudio_ResetKeepBuffer(s32 arg0);
s32  CdAudio_StoreIfNonNull(s32 arg0);
s32  CdAudio_JumpToSector(s32 arg0);
void CdAudio_SetLocBase(s32 arg0);
void CdAudio_AllocVoices(s8* arg0, s8* arg1);
s32  CdAudio_SeekAbs(s32 arg0);
void CdAudio_StartVolumeRamp(s32 arg0);
void CdAudio_JumpWithPitch(s32 arg0, s32 arg1);
void CdAudio_SetLocFlag(void);

#endif // CDAUDIO_H
