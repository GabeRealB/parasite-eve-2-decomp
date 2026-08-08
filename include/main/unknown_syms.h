#ifndef UNKNOWN_SYMS_H
#define UNKNOWN_SYMS_H

// Declarations still without a module home:
//   - unmatched / overlay funcs (func_800*, func_801*)
//   - BSS/data symbols not yet filed under a module header
//
// Named APIs live in main/{fs,mc,task,pad,sound,ui,text,cdaudio,cdstream,
// stream,display,tmd,stage,session,gameflow,gameflag,mem,boot,gamemain}.h
// via main/game.h. Prefer those headers; do not re-add Module_ protos here.

#include <psyq/libcd.h>
#include <psyq/libspu.h>

#include "common.h"
#include "main/game.h"
#include "main/fs.h"
#include "main/cdstream.h"

#define C3D458_HEAP_SIZE        0x3D00
#define C3D458_HEAP_START_MAGIC 0xB25A
#define C3D458_HEAP_MAGIC       0xA52B

#define C5F414_OTAG_ENTRIES  0x440
#define C5F414_OTAG_END_PRIM 0xFFFFFF

extern u16 func_8001D82C(void);

// gameflow (src/main/gameflow.c)
extern void              func_80094B90(s32 arg0);
extern void              func_8009407C(void);
extern void              func_8009FD74(s8 arg0, void* arg1);
extern void              func_8002C1D8(void);
extern TaskFuncTable5    D_800134BC;
extern TaskFuncTable3    D_800134D0;
extern u16               D_80013E88[]; // display width table
extern u16               D_80013E94[]; // display height table
extern TaskFuncTable6    D_80013E98;
extern TaskFuncTable3    D_80013EDC;
extern GBytes4           D_80013F18;
extern TaskFuncTable4    D_80013F1C;
extern UiPanelFuncTable6 D_80013F2C;
extern const GBytes6     D_80014124;

// Dynamically loaded (BSS region)
extern void  func_80097AC0(GpuOtBuf* arg0);
extern void  func_8009850C(GpuOtBuf* arg0);
extern void  func_800A8DC0(s32 arg0);
extern void* func_80099098(Task* task);
extern void* func_80099170(Task* task, s32 arg1, s32 arg2);
extern void  func_800991DC(void* arg0);
extern void  func_80099214(void* arg0);
extern void  func_80099258(void* arg0);
extern void  func_80099290(void* arg0);
extern s32   D_8005ED8C;
extern void  func_800E1A6C(void* arg0);
extern void  func_801011D0(s32* arg0, s32 arg1, s32 arg2, void* arg3);

// text draw (src/main/textdraw.c)
extern void func_8002DEC4(void);
// Dual SPRT glyph draw: code 0x66 (clut 0x7FFD, colored) + 0x67 (clut 0x7FFE)
// linked at OT[field_4] and OT[field_4+1] respectively.
// Dual SPRT glyph draw: code 0x64 (clut 0x7FFD, colored) + 0x67 (clut 0x7FFF)
// linked at OT[field_4] and OT[field_4+1] respectively.
// Dual SPRT glyph draw with DR_TPAGE: code 0x64 (clut 0x7FFD) + tpage 0xE100023F
// + code 0x67 (clut 0x7FFF) + tpage 0xE100025F, all linked at OT[field_4].
extern void func_8002E53C(TextDrawReq* arg0, u8* arg1);

// Glyph tables (selected by TextDrawReq.field_C); entries are FontGlyph (0xC each).
extern u8 Font_Glyphs0[];
extern u8 Font_Glyphs1[];
extern u8 Font_Glyphs2[];

// Memcard product-code prefix (12 bytes, e.g. "BASLUS-01042")
extern u8 D_80060DC8[];
// 64-byte character table for random memcard filename body
extern u8 D_80060E08[];
// sum / ones-complement pair written by Mc_WriteDataChecksum(0, ...); adjacent
// halfword D_80072AA8 is compared by Mc_VerifyFirstByteChecksum (also Mc_SaveData + 0x940)
extern s16 D_80072AA4;
extern u16 D_80072AA8;

// text util (src/main/textutil.c)
// Extracts one text line (handles escapes/newlines) from *arg0 into arg1; advances *arg0.
// Returns -1 at end of string, 1 on newline, other values for control escapes.
// Multi-line text draw (Text_DrawPrompt per line, y += 0xF). Skips draw when UiObject.field_8 == 5.
// Multi-line text measure: packed (height<<16)|maxWidth, 0xF pixels per line.
// Multi-line text draw with line limit (arg7) and scroll offset (arg8).
// Returns 1 if all text drawn within limit, 0 if truncated.
// Builds a memcard save filename into arg0 (prefix + arg1 char + 7 random)
// Image data uploaded to VRAM by Text_LoadClutImages
extern u_long D_80060910[];
extern u_long D_800609B0[];
// Prompt / dialog data tables (see McMenu_ConfirmDialog / McMenu_ConfirmDialogAlt / McMenu_ConfirmYes / McMenu_ConfirmNo)
extern u8 D_80060A54[];
extern u8 D_80060A58[];
extern u8 D_80060A5C[];
extern u8 D_80060A64[];
// "File Information" string passed to Ui_DrawTitle by McMenu_FileInformation (mcmenu)
extern char D_80013BB4[];

// memcard extras (src/main/mc.c)
extern void func_800330D8(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

// stage load helpers (src/main/loadui.c)
extern s32 func_80042500(void);

// overlay / high-address
extern void func_800AC688(void);
extern void func_800ACAA8(void); // called from 2F244.c
extern s32  func_800AD284(void);

// UI (src/main/ui.c)
extern void func_80046B34(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, s32 arg6);
extern void func_80046EEC(void* arg0, void* arg1, s32 arg2);

// SPU voices (src/main/spu.c)
extern s16  func_8004DE18(void* arg0);
extern void func_8004E200(void);

// SndEvt / MIDI (src/main/sndevt.c)
extern void (*SndEvt_Handlers[])(SndEvt*);
/// MIDI status-byte handler used by Midi_DriveTrack / D_800689C4.
typedef u8* (*MidiHandler)(s32, u8*, MidiSong*, MidiTrack*);
/// MIDI status-byte dispatch table for Midi_DriveTrack (0x80..0xF0 → index 0..7).
/// Entries: note-off, note-on, poly-AT, CC, program, pressure, pitch-bend, meta.
extern MidiHandler D_800689C4[];
extern u8*         func_80052488(s32 arg0, u8* arg1, MidiSong* arg2, MidiTrack* arg3);

// SndScript / SndVoice (src/main/sndscript.c)
extern volatile s32 D_800689E4;
extern volatile s32 D_800689E8;
extern s16          D_800689EC;
extern u8           D_800689F0[];
extern s8           D_80082748;
extern s8           D_80082749;
extern s8           D_8008274A;
extern s8           D_8008274B;
// Unprototyped: SndVoice_HasActiveId calls with indeterminate a0 (nop in delay slot).
extern s8 func_80055EF8(SndVoicePick* arg0, s16 arg1);

// CdAudio (src/main/cdaudio.c)
extern s32 func_80057BC0(void);
extern s16 D_80068A7C[];
extern u8  D_80068A80[];
extern u8  D_80068AF0[];
extern s32 D_80068B18[];
extern s32 D_80068B1C;
extern s16 D_80068B28[];
extern s32 D_80068B2C[];
extern s32 (*D_80068B34[])(void);
extern u16 D_80068BB8[];
extern u16 D_80068C78[];
extern u16 D_80068D78[];
extern u16 D_80068E78[];

// stream / MDEC (src/main/stream.c)
extern u16      D_8005EAEC;
extern u16      D_8005EAEE;
extern s32      D_8006AC08;
extern u16      D_8006AC0C;
extern u16      D_8006AC0E;
extern u16      D_8006AC10;
extern u16      D_8006AC12;
extern u16      D_8006AC14;
extern u16      D_8006AC16;
extern u16      D_8006AC18;
extern u16      D_8006AC1A;
extern u16      D_8006AC1C;
extern u16      D_8006AC1E;
extern u16      D_8006AC28;
extern s32      D_8006AC24;
extern u16      D_8006AC3C;
extern u_short* D_8006AC38;
extern void*    D_8006AC44;
extern u_long*  D_8006AC48[];
extern u_long*  D_8006AC50[];
extern u16      D_8006AC58;
extern u16      D_8006AC5A;
extern s16      D_8006AC5C;
extern u16*     D_8006AC60;
extern void*    D_8006AC64;
extern u_long*  D_8006AC68;
extern u16      D_8006AC6C;
extern void     func_80179954(void* arg0);
extern void     func_80179988(void* arg0);
extern void     func_801799BC(void* arg0);
/// Overlay helper used by Midi_UpdateVoiceVolumes when field_1 == 0x4F and D_80082120 == 5.
extern s32  func_80179BE4(u16 arg0, u8 arg1, LinInterp* arg2);
extern s32  StCdIntrFlag;
extern void func_8001F430(void);
extern u32  func_8001F180(u32);
extern s32  func_8001FAE0(s32 arg0, s32 arg1);

// Other
extern void func_80030AB0(McWork* work);
extern s16  func_800AF89C(u16 arg0, u16 arg1, u16 arg2, s32 arg3);
extern void func_800AFA44(void);
extern void func_800B0034(u16 arg0);
extern void func_800B00C4(void);
extern void func_8017D6D4(void);
extern void func_801D4B64(Task* arg0);

// .data
extern u32          D_8005EC64;
extern s32          D_8005EC68;
extern s32          D_8005EC6C;
extern volatile u32 D_8005EC70;
// Written by the VSync callback (Display_VSyncCallback); read by the draw path (Display_FrameFlipDraw).
extern volatile s32 D_8005EC74;
// Cleared/set by the draw path; read by the VSync callback for lag accounting.
extern volatile s32 D_8005EC78;
// Display/CD busy flags shared with the VSync path (GameMain_ShowLoading / GameMain_Loop).
extern volatile s32 D_8005EC80;
// Immediate-mode TILE / DR_TPAGE scratch for the "now loading" overlay.
extern TILE     D_8006EC18;
extern DR_TPAGE D_8006EC28;
// "NOW LOADING" (or similar) string drawn by GameMain_ShowLoading.
extern u8           D_80013404[];
extern char         D_80013B64[]; // "Select"
extern s8           D_800138BC[]; // "99999999"
extern s8           D_800138C8[]; // "0"
extern s8           D_800138CC[]; // "999999999"
extern UiList       D_8006116C;
extern UiList       D_80061194;
extern UiList       D_8006125C;
extern UiList       D_80061284;
extern UiList       D_800612AC;
extern UiObjectDesc D_800608F4[];
extern u8           D_80061170;
extern UiObjectDesc D_800611C8[];
extern UiObjectDesc D_80061200[];
extern UiObjectDesc D_8006121C[];
extern UiObjectDesc D_800612D0[];
extern TaskDesc     D_8006268C;
extern TaskDesc     D_8006269C;
extern u8           D_800626E8;
extern u8           D_80062734;
extern u8           D_80062735;
extern u8           D_80062736;
extern u8           D_80062737;
extern u8           D_80062738;
extern u8           D_80062739;
extern GPairU8*     D_8006273C[];
extern GPairU8*     D_80062750[];
extern u8           D_80062764[];
extern u8           D_8006276C[];
extern u8           D_80062934[];
extern u8           D_80062954[];
extern u8           D_800629B0[];
extern u8           D_800629D0[];
extern u8           D_80062A24[];
extern u8           D_80062A44[];
extern u8           D_80062AB4[];
extern u8           D_80062AD4[];
extern u8           D_80062B30[];
extern u8           D_80062B50[];
extern u8           D_80062BA4[];
extern u8           D_80062BC4[];
extern u8           D_80062C20[];
extern u8           D_80062C40[];
extern u8           D_80062C94[];
extern u8           D_80062CB4[];
extern u8           D_80062D08[];
extern u8           D_80062DB0[];
extern u8           D_80062E04[];
extern u8           D_80062E50[];
extern u8           D_80062EA0[];
extern u8           D_80062EEC[];
extern u8           D_80062F34[];
extern u8           D_80062F80[];
extern u8           D_80062FD0[];
extern u8           D_80063018[];
extern u8           D_80063068[];
extern u8           D_800630B0[];
extern s32          D_8006763C[];
extern s32          D_80067648;
extern s32          D_8006764C;
extern UiList       D_80067654;
extern UiObjectDesc D_80067678;
extern WipUiHolder* Wip_UiHolder;
extern u8           D_800680A4;
extern s8           D_800680AC[];
extern s8           D_800680BB;
extern u32          D_800680BC;
extern u8           D_80068184;
extern volatile u32 D_800680C0;
extern volatile s32 D_80068B58;
extern volatile u8  D_80068B5C;
extern volatile u8  D_80068B5D;
extern volatile u8  D_80068B5F;
extern volatile u8  D_80068B62;
extern volatile u8  D_80068B63;
extern volatile u8  D_80068B64;
extern volatile u8  D_80068B65;
extern u8           D_80068B66;
extern volatile s16 D_80068B6A;
extern void*        D_80068B6C;
extern s32          D_80068B70;
extern s32          D_80068B74;
extern volatile u16 D_80068B78;
extern CdlLOC       D_800827F8;
extern void         func_80059EE0(void);
extern s32          func_800AF590(s32 arg0, s32 arg1);
extern void         func_800B0118(s32 arg0, s32 arg1);

// 4F564
extern s32 D_8005ED68;
extern s32 D_8005ED70;
extern s32 D_8005ED74;
extern s32 D_8005ED78;
extern u16 D_8005ED8A;

extern u16* D_8005C36C;
extern u16* D_8005C370;
extern u16* D_8005C374;
extern s32* D_8005DCB4[];

// 58028
extern u8 D58028_SpuTimerEnabled;

// 5B3FC
extern void* D_8006AC00;
extern void* D_8006AC40;

extern TaskNode      D_8007A110;
extern s32           D_8007A118;
extern s32           D_8007A358;
extern u16           D_8007A35C;
extern u16           D_8007A35E;
extern void*         D_8007A360;
extern u8*           D_8007A364; // resolved decode base buffer (Mdec_ResolveStreamBuffer)
extern CdCmd58Entry* D_8007A368; // matched CdCmd_Queue.field_58 entry
extern u8            D_8007A392;
extern u8            D_8007A394;
extern s16           D_8007A396;
extern u8            D_8007A398;
extern s16           D_8007A39A;
extern u16           D_8007A39C;
extern u8            D_8007E0CC;
extern s32           D_8007E0D4;
extern SndBank       Snd_Banks[];

// Stream slot table living next to FS bss (F344); not pure FS API.
extern StreamSlot   Stream_Slots[15];
extern u16          D_8006EBB8;
extern s8           D_8006EBBA;
extern s16          D_8006EBF2;
extern volatile s32 D_8006EBF4;

// 5F414
// Snapshot of Display_State.field_100 / field_103 taken before a draw; read from
// the VSync callback path (Display_VSyncCallback → Display_FlipDraw), so volatile.
extern volatile u8 D_8006EC30;
extern u_long      D5F414_OrderingTables[2 * C5F414_OTAG_ENTRIES];
extern volatile u8 D_80070E38;
// Identity-matrix storage for GsCOORDINATE2.coord (parent at symbol - 4).
extern MATRIX   D_80070E44;
extern MATRIX   D_80070E94;
extern u8*      D_80070EE0; // primitive buffer cursor
extern GpuOtBuf Gpu_OtBuffers[2];
extern MATRIX   D_80070F14;
// VSync countdown; written/read by Display_VSyncCallback (VSync callback).
extern volatile s32 D_80070F64;
extern u_long*      D_800710A0; // current OT base
extern DR_TPAGE*    D_80071190; // primitive buffer cursor

// 61F10
// Immediate-mode SPRT scratch used by Text_DrawGlyphImmediate (DrawPrim path).
extern SPRT D_80071710;
extern s8   D_8007272B;
extern s8   D_8007272D;
// Alias of Mc_SaveData.field_4 (offset 0x4).
extern u8 D_8007216C;
// Alias of Mc_SaveData.field_21 (offset 0x21).
extern s8 D_80072189;
// Alias of Mc_SaveData byte at offset 0x23 (lb in CdCmd_ProcessPhase1).
extern s8 D_8007218B;
// Alias of Mc_SaveData.field_1a9 (offset 0x1A9); loaded with lb in audio setup.
extern s8 D_80072311;
// Alias of Mc_SaveData.field_1aa (offset 0x1AA).
extern u8 D_80072312;
// Alias of Mc_SaveData.field_1ab (offset 0x1AB); loaded with lb in Ui_InitList.
extern s8 D_80072313;
extern u8 D_800733F0[2][0x6C];
extern u8 D_800734C8[2][0xB0];
extern u8 D_80073628[2][0x24];
extern u8 D_80073670[2][0xE4];
extern u8 D_80073838[2][0xA4];
extern u8 D_80073980[0x208];

// 61CC0
extern GameSession D61CC0_800714C0;

// 64880
extern MATRIX D_80074080;

// 648E0
extern u8               D_800740E0[0x6000];
extern void*            D_8007A0E0;
extern s32              D_8007A0E4;
extern GsOT             Gpu_OrderingTables[2];
extern u_long           D_8007A120[0x80];
extern HeapBlockHeader* D648E0_HeapStart;
extern u8               D648E0_HeapBuffer[C3D458_HEAP_SIZE];
extern AudioTickNode    AudioTick_List;
extern u32              AudioTick_Enabled;
extern long             D648E0_SpuTimerED;
extern AsyncCbQueue     AsyncCb_Queue;
extern AsyncCbEntry     AsyncCb_Entries[];
extern SpuVoiceState    Spu_VoiceState;
extern SpuLVoiceTable   Spu_LVoiceTable;
extern SpuVoiceRange    Spu_VoiceRanges[];
extern u32              D648E0_8007EBA8;
extern u32              D648E0_8007EBAC;
extern u32              D648E0_8007EBB0;
extern SpuReverbConfig  D648E0_SpuReverbCfg;
extern s32              SndEvt_Lock;
extern SndEvt*          SndEvt_Head;
extern SndEvt*          SndEvt_Tail;
extern SndEvt           SndEvt_Pool[0x40];
extern u8               D_8007F2F0;
extern MidiSong         Midi_Song;
extern u8               D_8007F8E0[0x2800];
extern u8               D_8007E0F8[];
extern u8               D_8007E158;
extern u8               D_8007E258;
/// Volume scale byte used by Midi_UpdateVoiceVolumes when MidiSong::field_1 == 0x5A.
extern u8                     D_80068A4A;
extern u8                     D_80068A54[];
extern SndBankInitEntry       Snd_BankInitTable[];
extern s32                    D_80068A78;
extern s32                    D_800820E0;
extern s16                    D_800820E4;
extern u8                     D_800820E8;
extern s8                     D_800820E9;
extern SndLoadState           SndLoad_State;
extern u8                     D_800820F3;
extern volatile u8            D_80082120;
extern volatile u8            D_80082121;
extern volatile u8            D_80082122;
extern volatile s32           D_80082124;
extern volatile s32           D_80082128;
extern volatile u8            D_8008212C;
extern volatile s32           D_80082130;
extern volatile s8            D_80082134;
extern volatile u8            D_80082135;
extern volatile u8            D_80082136;
extern u8                     D_80082138[0x10];
extern SndBankSlot            SndBank_Slots[16];
extern SndScript              SndScript_Slots[8];
extern s32                    D_80082548[0x80];
extern volatile s32           D_8008274C;
extern volatile s32           D_80082750;
extern u8                     D_80082754;
extern volatile CdAudioTbl    CdAudio_Tbl;
extern volatile s32           D_80082770;
extern s32                    D_80082778;
extern volatile u8            D_8008277C;
extern volatile CdAudioCtl    CdAudio_Ctl;
extern CdAudioTblEntry*       CdAudio_TblEntries;
extern volatile CdAudioPhase  CdAudio_Phase;
extern volatile CdAudioLoc    CdAudio_Loc;
extern u8                     D_800827B0[];
extern LinInterp              LinInterp_CdStream;
extern CdStreamParams         CdStream_Params;
extern volatile u8            D_800827E4;
extern volatile u16           D_80082808;
extern volatile u16           D_80082810;
extern volatile CdStreamState CdStream_State;
extern CdStreamChannels       CdStream_Channels;
extern volatile CdReadyQueue  CdReady_Queue;
extern u8                     D_80094C8C[0x2E34];
extern u8                     D_80725C54[];

#endif // UNKNOWN_SYMS_H
