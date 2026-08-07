#ifndef UNKNOWN_SYMS_H
#define UNKNOWN_SYMS_H

#include <psyq/libcd.h>
#include <psyq/libspu.h>

#include "common.h"
#include "main/game.h"
#include "main/fs.h"

#define C3D458_HEAP_SIZE        0x3D00
#define C3D458_HEAP_START_MAGIC 0xB25A
#define C3D458_HEAP_MAGIC       0xA52B

#define C5F414_OTAG_ENTRIES  0x440
#define C5F414_OTAG_END_PRIM 0xFFFFFF

// boot.c APIs / F04CF8_* → main/boot.h; Boot_LoadInitialFile → main/fs.h

// Scratchpad image unpacker in boot rodata (func_80010398); reads
// D5B498_8006C22C / Fs_ChunkWritePtr globals, takes no meaningful args.
extern void func_80010398(void);

// E734.c / cdcmd.c — most CdCmd_* APIs live in main/fs.h
extern void CdCmd_StartOverlay(u16 arg0, u16 arg1, u16 arg2);
extern u16  func_8001D82C(void);

// 11E9C.c
extern void Fs_SetupBootLoad(void);
extern void Fs_BootImageMachine(void* arg0, void* arg1);

// 33300.c
extern s32 TextStream_Draw(TextStream* arg0, u8* arg1, s16* arg2, s32 arg3);

// fs.c APIs and Fs_* data → main/fs.h
extern void func_80010024(void);

// 16494.c
extern void Fs_StreamReadyCb(u8 status, u8* result);
extern s32  Cd_InitStateMachine(u32* arg0);
extern void F16494_ResetSpuAttr(void);
extern void CdVol_SetMixMode(s32 arg0);
extern u8   CdVol_GetMixMode(void);
extern void CdVol_CacheFromSpu(void);
extern void CdVol_RegisterCallbacks(void);
extern void CdVol_ClearCallbackSlot(void);
extern s32  Cd_Flush(void);
extern void CdVol_ApplyFromTable(u16 arg0);
extern s32  CdVol_StepDown(void);

// gamemain.c APIs → main/gamemain.h

// 1C034.c — game-flow state handlers
extern void GameFlow_StateByField34(Task* arg0);
extern void Fade_DrawOverlay(s32 r, s32 g, s32 b, s32 mode);
extern void func_80094B90(s32 arg0);
extern void func_8009407C(void);
extern void func_8009FD74(s8 arg0, void* arg1);
extern void Game_ClearSession(void);
extern void GameFlow_InitSystems(void);
extern void GameFlow_SpawnMainWhenReady(Task* arg0);
extern void Game_ClearEd68(void);
extern void GameFlow_DispatchTable(Task* arg0);
extern void func_8002C1D8(void);
// Pad_* → main/pad.h; Task_* → main/task.h
extern TaskFuncTable5    D_800134BC;
extern TaskFuncTable3    D_800134D0;
extern u16               D_80013E88[]; // display width table
extern u16               D_80013E94[]; // display height table
extern TaskFuncTable6    D_80013E98;
extern TaskFuncTable3    D_80013EDC;
extern GBytes4           D_80013F18;
extern TaskFuncTable4    D_80013F1C;
extern UiPanelFuncTable6 D_80013F2C;
extern GBytes6           D_80014124;
extern void              Game_SetPtrSlot(void* arg0, s32 arg1);
extern void*             Game_GetPtrSlot(s32 arg0);
extern void              Task_CountdownCallback(Task* arg0);

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

// 1E6C4.c
extern void func_8002DEC4(void);
extern s32  Text_MeasureGlyphWidth(TextDrawReq* arg0, u8* arg1, u8* arg2);
// Dual SPRT glyph draw: code 0x66 (clut 0x7FFD, colored) + 0x67 (clut 0x7FFE)
// linked at OT[field_4] and OT[field_4+1] respectively.
extern void Text_DrawGlyphDualSprtA(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2);
// Dual SPRT glyph draw: code 0x64 (clut 0x7FFD, colored) + 0x67 (clut 0x7FFF)
// linked at OT[field_4] and OT[field_4+1] respectively.
extern void Text_DrawGlyphDualSprt(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2);
// Dual SPRT glyph draw with DR_TPAGE: code 0x64 (clut 0x7FFD) + tpage 0xE100023F
// + code 0x67 (clut 0x7FFF) + tpage 0xE100025F, all linked at OT[field_4].
extern void Text_DrawGlyphDualSprtTpage(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2);
extern void func_8002E53C(TextDrawReq* arg0, u8* arg1);
extern void Text_MeasureAndCenter(TextDrawReq* arg0, u8* arg1);
// Skip arg1 newline/escape-delimited lines starting at arg0; returns advanced pointer.
extern u8*  Text_SkipLines(u8* arg0, s32 arg1);
extern void Text_DrawGlyphImmediate(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2);
extern void Text_DrawGlyphQueued(TextDrawReq* arg0, FontGlyph* arg1, s32 arg2);
extern void Text_DrawGlyphOt(TextDrawReq* arg0, FontGlyph* arg1);
extern void Text_BootTask(Task* arg0);

// Glyph tables (selected by TextDrawReq.field_C); entries are FontGlyph (0xC each).
extern u8 Font_Glyphs0[];
extern u8 Font_Glyphs1[];
extern u8 Font_Glyphs2[];

// Memcard product-code prefix (12 bytes, e.g. "BASLUS-01042")
extern u8 D_80060DC8[];
// Mc_FileName / Mc_Glyphs* / Mc_BufferSlots / Mc_DefaultChecksumSrc / Mc_PromptTable
// / Mc_SaveData → main/mc.h
// 64-byte character table for random memcard filename body
extern u8 D_80060E08[];
// sum / ones-complement pair written by Mc_WriteDataChecksum(0, ...); adjacent
// halfword D_80072AA8 is compared by Mc_VerifyFirstByteChecksum (also Mc_SaveData + 0x940)
extern s16 D_80072AA4;
extern u16 D_80072AA8;

// 201E0.c
// Extracts one text line (handles escapes/newlines) from *arg0 into arg1; advances *arg0.
// Returns -1 at end of string, 1 on newline, other values for control escapes.
extern s32 Text_ParseLine(u8** arg0, u8* arg1);
// Multi-line text draw (Text_DrawPrompt per line, y += 0xF). Skips draw when UiObject.field_8 == 5.
extern s32 Text_DrawMultiLine(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5,
                              s32 arg6);
extern s32 Text_MeasureWidth(u8* arg0);
// Multi-line text measure: packed (height<<16)|maxWidth, 0xF pixels per line.
extern s32 Text_MeasureMultiLine(u8* arg0);
extern s32 Text_DrawPrompt(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5,
                           s32 arg6);
// Multi-line text draw with line limit (arg7) and scroll offset (arg8).
// Returns 1 if all text drawn within limit, 0 if truncated.
extern s32  Text_DrawMultiLineScroll(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5,
                                     s32 arg6, s32 arg7, s32 arg8);
extern void Text_LoadClutImages(void);
// Builds a memcard save filename into arg0 (prefix + arg1 char + 7 random)
extern void Mc_BuildFileName(u8* arg0, s32 arg1);
// Image data uploaded to VRAM by Text_LoadClutImages
extern u_long D_80060910[];
extern u_long D_800609B0[];
// Prompt / dialog data tables (see McMenu_ConfirmDialog / McMenu_ConfirmDialogAlt / func_80036F18 / func_80036FB8)
extern u8 D_80060A54[];
extern u8 D_80060A58[];
extern u8 D_80060A5C[];
extern u8 D_80060A64[];
// "File Information" string passed to Ui_DrawTitle by McMenu_FileInformation (mcmenu)
extern char D_80013BB4[];

// mc.c
extern void func_800330D8(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

// 32B64.c
extern void CdCmd_EnqueueLoadFile(s32 arg0, s32 arg1, s32 arg2);
extern s32  func_80042500(void);
extern void Prim_DrawLoadingSprt(void);
extern void Snd_ApplyVolumeTable(s32 arg0);

// overlay / high-address
extern void func_800AC688(void);
extern void func_800ACAA8(void); // called from 2F244.c
extern s32  func_800AD284(void);

// 34E98.c
extern UiObject* Ui_SpawnTextBlock(TextBlockDesc* arg0);
extern UiObject* Ui_SpawnFromDesc(UiObjectDesc* arg0, s32 arg1, s32 arg2, s32 arg3,
                                  UiObject* arg4);
extern void      func_80046B34(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, s32 arg6);
extern void      func_80046EEC(void* arg0, void* arg1, s32 arg2);
extern void      Ui_SizeFromText(UiPanel* arg0, u8* arg1, s32 arg2, s32 arg3);
extern void      Ui_TeardownTree(UiObject* arg0, Task* arg1);
extern void      Ui_FreeAndKill(Task* arg0);
extern void      Ui_SetState4(Task* arg0);
extern void      Ui_DrawTextColored(UiPanel* arg0, char* arg1);
extern void      Ui_DrawText(UiPanel* arg0, char* arg1);
extern void      Ui_ClampAnimOrClose(UiPanel* arg0, s32 arg1, s32 arg2);
extern void      Ui_StartCloseAnim(UiPanel* arg0, void* arg1);
extern void      Ui_DrawCaret(UiList* arg0, UiPanel* arg1, s32 arg2);
extern void      Ui_LayoutListPanel(UiList* arg0, UiPanel* arg1);
extern void      Ui_InitList(UiList* arg0, UiMiniObj* arg1);
extern void      Ui_ComputeVisibleRows(UiList* arg0, s32 arg1);
extern void      Ui_UpdateListNoAnim(void* arg0, void* arg1);
extern void      Ui_ComputeVisibleRowsEx(UiList* arg0, UiPanel* arg1, s32 arg2);
extern void      Ui_SmoothCursor(UiMiniObj* arg0, s32 arg1, s32 arg2);
extern s32       Ui_LookupTable(void* arg0, s32 arg1);
extern void      Ui_DrawHBar(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3);
extern void      Ui_DrawVBar(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3);
extern void      Ui_DrawTextUnderline(UiPanel* arg0, s32 arg1, s32 arg2, char* arg3, s32 arg4);
extern void      Ui_DrawTitle(UiPanel* arg0, char* arg1);
extern void      Ui_DrawTextAtLayout(UiPanel* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6);
extern void      Ui_SetListScrollFlag(UiList* arg0, s32 arg1);
extern void      Ui_AllocTile(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5);
extern void      Ui_DrawListHighlight(UiList* arg0, UiPanel* arg1, s32 arg2);
extern void      Ui_LayoutWithMode0(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5);
extern void      Ui_LayoutWithMode1(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5);
extern void      Ui_SetupClip(UiPanel* arg0);
extern void      Ui_ScaleRect(UiPanel* arg0, RECT* arg1, s32 arg2, s32 arg3);
extern void      Ui_LayoutAndClip(UiPanel* arg0);
extern void      Ui_LayoutAndDraw(UiPanel* arg0);
extern void      Ui_LayoutAndDrawAlt(UiPanel* arg0);
extern void      Ui_ComputeAnimRect(UiPanel* arg0, RECT* arg1);
extern void      Ui_AnimOpenStep(UiPanel* arg0, void* arg1);
extern void      Ui_DrawAndCallback(UiPanel* arg0, void* arg1);
extern void      Ui_LayoutDrawAndCallback(UiPanel* arg0, void* arg1);
extern void      Ui_TickAnimCounter(UiPanel* arg0, void* arg1);
extern void      Ui_ClipAndCallback(UiPanel* arg0, void* arg1);
extern void      Ui_WaitCdThenOverlay(Task* arg0);

// 3B458.c
extern s32 GameFlag_GetNibble(s32 arg0);

// 3D458.c
extern void           Spu_WaitDma(void);
extern void           Audio_IrqFrameWork(void);
extern SndBank*       Snd_AllocBank(SndBankPayload* arg0);
extern void           F3D458_ResetHeap(void);
extern void*          F3D458_Malloc(size_t);
extern void           F3D458_Free(void* ptr);
extern void           Snd_FreeBank(SndBank* arg0);
extern SndBank*       Snd_FindBank(u16 arg0);
extern void           Snd_BuildGroupIndex(SndBank* arg0);
extern void           LinInterp_Setup(LinInterp* arg0, s32 arg1, s32 arg2, s32 arg3);
extern s32            LinInterp_Apply(LinInterp* arg0, s32 arg1);
extern void           LinInterp_Step(LinInterp* arg0);
extern void           Spu_ApplyPanVolume(s16* arg0, s16 arg1, s32 arg2);
extern s32            Spu_TimerReentryWork(void);
extern void           AudioTick_Process(void);
extern AudioTickNode* AudioTick_Remove(AudioTickNode* arg0);
extern void           AudioTick_Reset(void);

// 3E48C.c
extern s16      func_8004DE18(void* arg0);
extern void     AsyncCb_Cancel(s32 arg0);
extern void     func_8004E200(void);
extern s32      Spu_AllocVoice(s16* arg0, s32 arg1, s32 arg2);
extern void     Spu_SetVoiceCallbacks(u32 voiceIdx, s32 arg1, s32 arg2);
extern s32      Spu_SetVoiceRange(s32 idx, s32 arg1, s32 arg2);
extern s32      Spu_GetVoiceRef(s8 arg0, SpuVoiceRef* arg1);
extern u8       Spu_GetVoiceStatus(u32 voiceIdx);
extern void     Spu_ClearVoiceCallbacks(u32 voiceIdx);
extern void     Spu_KeyOff(u32 voiceIdx);
extern u16      Spu_CalcVolume(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
extern SndNote* Snd_GetNote(SndBank* arg0, u8 arg1, u8 arg2);
extern void     F3E48C_8004E44C(void);
extern s32      F3E48C_8004E660(u32 voiceIdx);
extern void     F3E48C_QueryReverbVoices(void);
extern void     F3E48C_ConfigSpuReverb(s32 mode);
extern void     F3E48C_SetReverbDepth(s16 depth);
extern void     F3E48C_SetReverbMode(u32 mode);
extern void     F3E48C_EnableVoice(u32 voiceIdx);
extern void     F3E48C_DisableVoice(u32 voiceIdx);
extern bool     F3E48C_ReverbVoiceIsEnabled(u32 voiceIdx);
extern void     F3E48C_ApplyReverbConfig(void);

// 410B0.c
extern void SndEvt_Process(void);
extern void (*SndEvt_Handlers[])(SndEvt*);
/// MIDI status-byte handler used by Midi_DriveTrack / D_800689C4.
typedef u8* (*MidiHandler)(s32, u8*, MidiSong*, MidiTrack*);
/// MIDI status-byte dispatch table for Midi_DriveTrack (0x80..0xF0 → index 0..7).
/// Entries: note-off, note-on, poly-AT, CC, program, pressure, pitch-bend, meta.
extern MidiHandler D_800689C4[];
extern SndEvt*     SndEvt_Alloc(void);
extern s32         SndEvt_EnqueueType1(s32 arg0, s32 arg1);
extern s32         SndEvt_EnqueueType2(s32 arg0, s32 arg1);
extern s32         Midi_IsBusy(s32 arg0);
extern s32         Midi_IsChannelFree(u8 arg0);
extern void        SndEvt_Enqueue(SndEvt* arg0);
extern void        SndEvt_Free(SndEvt* arg0);
extern void        SndEvt_HandleInitSequence(SndEvt* arg0);
extern void        SndEvt_HandleStartFadeOut(SndEvt* arg0);
extern s32         Midi_InitSequence(u8 arg0, u16 arg1);
extern void        SndEvt_HandleFadeOn(SndEvt* arg0);
extern void        SndEvt_HandleFadeOff(SndEvt* arg0);
extern void        SndEvt_HandleSetVolume(SndEvt* arg0);
extern void        SndEvt_HandleAllocVoice(SndEvt* arg0);
extern void        SndEvt_HandleType7(SndEvt* arg0);
extern void        SndEvt_HandleFadeMatchingOn(SndEvt* arg0);
extern void        SndEvt_HandleFadeMatchingOff(SndEvt* arg0);
extern void        SndEvt_HandlePanRamp(SndEvt* arg0);
extern void        Midi_StartFadeOut(u8 arg0, u16 arg1);
extern void        Midi_FadeVolume(u8 arg0, s32 arg1);
extern void        Midi_SetVolumeScale(u8 arg0, u8 arg1);
extern void        Midi_SetMasterVolume(s32 arg0);
extern s32         Midi_GetMasterVolume(void);
extern u8*         Midi_GetSlot(s32 arg0);
extern void*       Midi_GetFixedBuffer(s32 arg0, s32 arg1);
extern s32         SndEvt_EnqueueType5(s32 arg0, s32 arg1);
extern void        SndEvt_EnqueueType5Pending(void);
extern void        SndEvt_FlushType5Pending(void);
extern void        Midi_InitSlot(s32 arg0);
extern void*       Midi_ResolveTrackData(MidiSong* arg0, s32 arg1, u8* arg2);
extern void        Midi_ResetTrackFlags(MidiSong* arg0);
extern void        Midi_KeyOffVoices(MidiSong* arg0);
extern void        Midi_DriveTrack(MidiSong* arg0, MidiTrack* arg1);
extern void        Midi_UpdateVoiceVolumes(MidiSong* arg0);
extern void*       SndLoad_AllocBuffer(s32 arg0, s32 arg1, u32 arg2);
extern s32         SndLoad_LookupMode(s32 arg0, s32 arg1, s32 arg2);
extern s32         Midi_ReadVlq(u8* arg0, u8* arg1);
extern void        Midi_InitChannelTable(s32* arg0);
extern s32         func_800528F0(s32 arg0, s32 arg1);
extern u8*         func_80052488(s32 arg0, u8* arg1, MidiSong* arg2, MidiTrack* arg3);
extern u8*         Midi_KeyOffChannel(s32 arg0, u8* arg1, MidiSong* arg2);
extern u8*         Midi_SetProgram(s32 arg0, u8* arg1, MidiOpcodeCtx* arg2);
extern u8*         Midi_PitchBend(s32 arg0, u8* arg1, MidiSong* arg2);
extern s32         SndLoad_ProcessSector(s32* arg0);
extern s32         SndBank_SetupFromLoad(SndLoadState* arg0);
extern void        func_800530DC(SndLoadState* arg0);
extern void        SndLoad_FromSectorMode8(void* arg0);
extern void        func_80053280(u8 arg0, void* arg1);
extern void        SndLoad_Teardown(void);
extern s32         SndLoad_FeedSector(void* arg0);
extern s32         SndLoad_FeedSectorOrError(void* arg0);
extern s32         SndBank_FinalizeLoad(SndLoadState* arg0);
extern void        SndLoad_Init(s32 arg0, void* arg1);

// 43FFC.c
extern s32  D_800689E4;
extern s32  D_800689E8;
extern s16  D_800689EC;
extern u8   D_800689F0[];
extern s8   D_80082748;
extern s8   D_80082749;
extern s8   D_8008274A;
extern s8   D_8008274B;
extern void Snd_InitFromStage(s32 arg0, s32 arg1);
extern s32  func_80053A20(s32 arg0, s32 arg1);
// Unprototyped: SndVoice_HasActiveId calls with indeterminate a0 (nop in delay slot).
extern s32          SndBank_RemapId();
extern void         Snd_SetMutedVolumes(s32 arg0);
extern s32          SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);
extern void         SndEvt_EnqueueTypeB(s32 arg0, s32 arg1);
extern void         SndBank_SetEnableFlags(s32 arg0, s32 arg1);
extern void         SndVoice_SetPriority(s8 arg0);
extern s32          SndVoice_HasActiveId(void);
extern void         SndEvt_EnqueueTypeD(void);
extern void         SndEvt_EnqueueTypeE(void);
extern void         SndEvt_EnqueueTypeF(void);
extern void         func_800546F4(s32 arg0, u16 arg1);
extern void         SndVoice_StepMasterLevel(void);
extern void         SndVoice_KeyOffMatching(void);
extern s32          SndScript_Exec(SndScript* arg0);
extern void         SndVoice_TickEnvelope(SndVoice* arg0);
extern s32          SndVoice_AllocSlot(s32 arg0, s8 arg1, s8 arg2, s32 arg3, SndVoiceParams* arg4);
extern void         SndVoice_ScanCandidates(SndVoicePick* arg0, u16 arg1, s32 arg2, u16 arg3);
extern s8           func_80055EF8(SndVoicePick* arg0, s16 arg1);
extern void         SndScript_Play(s32 arg0, s8 arg1, s8 arg2, s32 arg3, s32 arg4, SndVoiceParams* arg5);
extern s32          SndVoice_Tick(SndVoice* arg0);
extern void         SndVoice_FadeMatching(s32 arg0, s32 arg1);
extern void         SndVoice_SetPanRamp(s32 arg0, s32 arg1, s32 arg2);
extern void         SndVoice_SetVolumeRamp(s32 arg0, s32 arg1);
extern void         SndVoice_IncRefCount(void);
extern void         SndVoice_TickRefCount(void);
extern void         SndVoice_SetPriorityLevel(s8 arg0);
extern s32          SndVoice_FindById(s32 arg0);
extern void         SndVoice_ApplyMasterVolume(s8 arg0);
extern s8           SndVoice_GetMasterVolume(void);
extern void         SndVoice_Detach(SndVoice* arg0);
extern SndBankSlot* SndBankSlot_Find(u16 arg0, s32 arg1);
extern SndBankSlot* SndBankSlot_Get(s32 arg0);
extern void         SndBankSlot_Free(s32 arg0);
extern SndVoice*    SndVoice_Alloc(s32 arg0);
extern void         SndVoice_Attach(SndVoiceOwner* arg0, SndVoice* arg1);
extern s32          SndScript_TickVoices(SndScript* arg0);
extern void         SndVoice_ScaleVolume(s8 arg0, s8 arg1, SndVoice* arg2, LinInterp* arg3, s16* arg4);
extern void         SndVoice_SetupEnvelope(SndVoice* arg0, s16 arg1, u32 arg2, SndNote* arg3);
extern s32          SndScript_FindOneA(u8* arg0, s16 arg1, SndOneAOut* arg2);
extern void         SndVoice_ClearActive(void);
extern s32          CdAudio_Begin(void);

// 46FE4.c
extern void CdAudio_Init(void);
extern u8   CdAudio_GetState(void);
extern void CdAudio_Tick(void);
extern s32  CdAudio_StartTrack(s32 arg0, s32 arg1);
extern s32  CdAudio_ResetKeepBuffer(s32 arg0);
extern s32  CdAudio_StoreIfNonNull(s32 arg0);
extern s32  CdAudio_JumpToSector(s32 arg0);
extern void CdAudio_SetLocBase(s32 arg0);
extern void CdAudio_AllocVoices(s8* arg0, s8* arg1);
extern s32  CdAudio_SeekAbs(s32 arg0);
extern void CdAudio_StartVolumeRamp(s32 arg0);
extern void CdAudio_JumpWithPitch(s32 arg0, s32 arg1);
extern s32  func_80057BC0(void);
extern void CdAudio_SetLocFlag(void);
extern void CdStream_Start(CdStreamParams* arg0);
extern void CdStream_Stop(void);
extern void CdStream_Drive(void);
extern s16  D_80068A7C[];
extern u8   D_80068A80[];
extern u8   D_80068AF0[];
extern s32  D_80068B18[];
extern s32  D_80068B1C;
extern s16  D_80068B28[];
extern s32  D_80068B2C[];
extern s32  (*D_80068B34[])(void);
extern u16  D_80068BB8[];
extern u16  D_80068C78[];
extern u16  D_80068D78[];
extern u16  D_80068E78[];

// 43FFC.c
extern s32  TaskIdMap_RemapIndex(s32 arg0, s32 arg1, s32 arg2);
extern void SndEvt_EnqueueType7(s32 arg0, s32 arg1);

// 4A6E0.c — CdStream_* / CdReady_* public APIs also in main/cdstream.h
extern void CdStream_Reset(void);
extern void CdStream_ArmSpuIrq(void);
extern void CdStream_SpuIrqHandler(void);
extern void CdStream_SetPitch(s16 arg0);
extern s32  CdStream_IsBusy(void);
extern void CdStream_SetLinkedPitch(s32 arg0);
extern void CdReady_InstallCallback(CdlCB arg0);
extern void CdReady_ClearCallback(void);
extern void CdStream_AbortPhase(u32* arg0);
extern void CdStream_ClearReadySlot(void);
extern void CdStream_MarkEnding(void);
extern s32  CdStream_Flush(void);
extern void CdStream_ConfigureSpuIrq(s32 arg0, u32 arg1);
extern s32  CdStream_InitDisc(u32* arg0);
extern s32  CdReady_Enqueue(CdReadyEntry* arg0);
extern void CdReady_Poll(void);
extern void CdReady_Cancel(s16 arg0);
extern void CdStream_Continue(void);

// F344.c
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
extern s32         func_80179BE4(u16 arg0, u8 arg1, LinInterp* arg2);
extern void        Mdec_SetupBuffers(u8* arg0);
extern s32         StCdIntrFlag;
extern s32         Stream_RestoreAfterLoad(s32 arg0, s32 arg1);
extern void        func_8001F430(void);
extern void        Mdec_UploadSlice(void);
extern void        Mdec_KickStrip(void);
extern void        Mem_AllocAuxWithImages(s16 arg0);
extern void        Stream_ResetRestoreState(void);
extern s16         Stream_FindSlot(u8* arg0, s32 arg1, s32 arg2);
extern s16         Stream_FindSlotByKey(u8* arg0);
extern u32         func_8001F180(u32);
extern s32         func_8001FAE0(s32 arg0, s32 arg1);
extern StreamSlot* Stream_GetSlot(u32 arg0);
extern u16         Stream_GetSlotField1A(u32 arg0);
extern void        Stream_KickDecode(u32 arg0);

// Other
// GameMain_Init / GameMain → main/gamemain.h
// Pad_Init → main/pad.h
extern void GameMain_Loop(void);
extern void Tmd_InitLists(void);
extern void Mc_InitBufferSlots(void);
extern void func_80030AB0(McWork* work);
extern s32  Mc_PromptDialog(Task* arg0, s32 arg1, s32 arg2);
extern s32  Mc_PromptDialogChoice(Task* arg0, s32 arg1, s32 arg2);
extern s32  Mc_PromptDialogSpawn(Task* arg0, s32 arg1, s32 arg2);
extern s32  Mc_PromptDialogFile(Task* arg0, s32 arg1, s32 arg2);
// Mc_InitLib → main/mc.h
extern void  Display_SetMode(s32 arg0);
extern void  Display_SetAutoClear(s32 arg0, s32 arg1, s32 arg2);
extern void  Display_ClampField126(s8 arg0);
extern void  Gpu_InitOtSmall(void);
extern Task* Display_SpawnFromMode(void);
extern void  Display_FlipOtAndDispatch(s32 arg0);
extern void  Display_InvertFramebufferGray(void);
extern void  Stage_InitOtAndSpawn(void);
extern s32   Display_SetFadeRate(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
extern void  Display_SetFadeMax(u8 arg0);
extern s32   Display_InitModeObj(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3);
extern void  func_8004017C(void);
extern void  Mdec_DecodeToVram(void);
extern void  CdCmd_StepVlcRebuild(void);
extern void  Tmd_ProcessStream(TmdObject* arg0);
extern void  Tmd_SetupDraw(TmdObject* arg0);
extern void  Gpu_ResetGraphAndOt(void);
extern void  Tmd_AllocMissingBuffers(void);
extern void  Mc_InitSaveSlotDefaults(void);
extern void  Spu_InitSystem(s32 arg0);
extern void  Spu_Init(void);
extern void  F3D458_ResetHeap(void);
extern long  Spu_TimerCallback(void);
extern void  Snd_ClearBanks(void);
extern void  AsyncCb_Poll(void);
extern void  AsyncCb_Reset(void);
extern void  Spu_InitVoices(void);
extern void  AudioTick_Insert(void*, u32, u32, s32*);
extern void  SndEvt_Reset(void);
extern s32   Midi_InitSystem(u32);
extern s32   Midi_Tick(void);
extern void  Snd_PollAsync(void);
extern void  Snd_RegisterTickCallbacks(void);
extern s32   SndVoice_DriveSlots(void);
extern s32   Snd_ReverbWarmupCb(s32* arg0);
extern s32   Snd_InitBanks(u32);
extern s16   func_800AF89C(u16 arg0, u16 arg1, u16 arg2, s32 arg3);
extern void  func_800AFA44(void);
extern void  func_800B0034(u16 arg0);
extern void  func_800B00C4(void);
extern void  func_8017D6D4(void);
extern void  func_801D4B64(Task* arg0);

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
extern u8 D_80013404[];
// Task_DescBanks → main/task.h
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
extern StageCtx*    Stage_Ctx;
extern TaskDesc     D_8006269C;
/// 0x1C-byte block zeroed by GameMain init; field_8 remaps pad input (Pad_UpdatePort0).
typedef struct _PadRemapState {
    /* 0x00 */ byte unknown_0[0x8];
    /* 0x08 */ s8   field_8;
    /* 0x09 */ byte unknown_9[0x13];
} PadRemapState;
/* size 0x1C — see Mem_Set(Pad_RemapState, 0, 0x1C) */
extern PadRemapState* Pad_RemapState;
extern u8             D_800626E8;
extern u8             D_80062734;
extern u8             D_80062735;
extern u8             D_80062736;
extern u8             D_80062737;
extern u8             D_80062738;
extern u8             D_80062739;
extern GPairU8*       D_8006273C[];
extern GPairU8*       D_80062750[];
extern u8             D_80062764[];
extern u8             D_8006276C[];
extern u8             D_80062934[];
extern u8             D_80062954[];
extern u8             D_800629B0[];
extern u8             D_800629D0[];
extern u8             D_80062A24[];
extern u8             D_80062A44[];
extern u8             D_80062AB4[];
extern u8             D_80062AD4[];
extern u8             D_80062B30[];
extern u8             D_80062B50[];
extern u8             D_80062BA4[];
extern u8             D_80062BC4[];
extern u8             D_80062C20[];
extern u8             D_80062C40[];
extern u8             D_80062C94[];
extern u8             D_80062CB4[];
extern u8             D_80062D08[];
extern u8             D_80062DB0[];
extern u8             D_80062E04[];
extern u8             D_80062E50[];
extern u8             D_80062EA0[];
extern u8             D_80062EEC[];
extern u8             D_80062F34[];
extern u8             D_80062F80[];
extern u8             D_80062FD0[];
extern u8             D_80063018[];
extern u8             D_80063068[];
extern u8             D_800630B0[];
extern s32            D_8006763C[];
extern s32            D_80067648;
extern s32            D_8006764C;
extern UiList         D_80067654;
extern UiObjectDesc   D_80067678;
extern WipUiHolder*   Wip_UiHolder;
extern u8             D_800680A4;
extern s8             D_800680AC[];
extern s8             D_800680BB;
extern u32            D_800680BC;
extern u8             D_80068184;
extern volatile u32   D_800680C0;
extern volatile s32   D_80068B58;
extern volatile u8    D_80068B5C;
extern volatile u8    D_80068B5D;
extern volatile u8    D_80068B5F;
extern volatile u8    D_80068B62;
extern volatile u8    D_80068B63;
extern volatile u8    D_80068B64;
extern volatile u8    D_80068B65;
extern u8             D_80068B66;
extern volatile s16   D_80068B6A;
extern void*          D_80068B6C;
extern s32            D_80068B70;
extern s32            D_80068B74;
extern volatile u16   D_80068B78;
extern CdlLOC         D_800827F8;
extern void           func_80059EE0(void);
extern void           CdStream_ReadyMts(s32 arg0, u8* arg1);
extern void           CdStream_FinishQueueEntry(u32* arg0);
extern s32            func_800AF590(s32 arg0, s32 arg1);
extern void           func_800B0118(s32 arg0, s32 arg1);

// D4CB64_ImgBuffers + all Fs_* data / D5B498_* FS state → main/fs.h

// 4F564
extern GameSession* Game_Session;
extern s32          D_8005ED68;
extern s32          D_8005ED70;
extern s32          D_8005ED74;
extern s32          D_8005ED78;
extern u16          D_8005ED8A;

extern u16* D_8005C36C;
extern u16* D_8005C370;
extern u16* D_8005C374;
extern s32* D_8005DCB4[];

extern s16 Stream_HasActiveLowId(void* arg0);

// 58028
extern u8 D58028_SpuTimerEnabled;

// .bss

// 5B3FC
extern void* D_8006AC00;
extern void* D_8006AC40;

// Task_ActiveList / Task_DefaultList → main/task.h
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
// Display_State → main/display.h
extern u_long*     D_800710A0; // current OT base
extern WipSysFlags Wip_SysFlags;
extern DR_TPAGE*   D_80071190; // primitive buffer cursor

// 61F10
// Immediate-mode SPRT scratch used by Text_DrawGlyphImmediate (DrawPrim path).
extern SPRT D_80071710;
extern s8   D_8007272B;
extern s8   D_8007272D;
// Mc_SaveData → main/mc.h
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
extern s8           D_80072313;
extern u8           D_800733F0[2][0x6C];
extern u8           D_800734C8[2][0xB0];
extern u8           D_80073628[2][0x24];
extern u8           D_80073670[2][0xE4];
extern u8           D_80073838[2][0xA4];
extern u8           D_80073980[0x208];
extern WipSysConfig Wip_SysConfig;

// 61CC0
extern GameSession D61CC0_800714C0;
// Pad_States → main/pad.h

// 64880
extern MATRIX D_80074080;

// 648E0
extern u8               D_800740E0[0x6000];
extern void*            D_8007A0E0;
extern s32              D_8007A0E4;
extern GameOt           Gpu_OrderingTables[2];
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
