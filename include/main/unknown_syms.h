#ifndef UNKNOWN_SYMS_H
#define UNKNOWN_SYMS_H

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

// E734.c
extern s32 func_8001E2D4(void);
extern s32 func_8001E6AC(s32 arg0, s32 arg1);
// cdcmd.c (CdCmd_* → main/fs.h)
extern void func_8001BB7C(void);
extern void func_8001BE60(void);
extern void func_8001C0D4(void);
extern void func_8001C620(void);
extern s32  func_8001C970(void);
extern void func_8001CA70(void);
extern u16  func_8001CDF0(void);
extern void func_8001CEFC(void);
extern u16  func_8001D0E8(void);
extern u16  func_8001D344(void);
extern u16  func_8001D37C(s16 arg0);
extern void func_8001D39C(void);
extern void func_8001D498(void);
extern void func_8001D534(u16 arg0, u16 arg1, u16 arg2);
extern void func_8001D588(void);
extern void func_8001D628(void);
extern void func_8001D6B8(void);
extern u16  func_8001D82C(void);
extern void func_8001D990(s32 cmd, u8* paramA, u8* paramB);
extern void func_8001DB84(void);

// 11E9C.c
extern void func_8002207C(void);
extern void func_8002226C(void*, void*);

// fs.c APIs and Fs_* data → main/fs.h
extern void func_80010024(void);

// 16494.c
extern void func_80025C94(u8 status, u8* result);
extern void func_80025DD8(void);
extern void F16494_ResetSpuAttr(void);
extern void func_800260B0(s32 arg0);
extern u8   func_80026138(void);
extern void func_80026148(void);
extern void func_80026178(void);
extern void func_800261C8(void);
extern s32  func_800261D4(void);
extern void func_80026218(u16 arg0);
extern s32  func_800262A8(void);

// gamemain.c APIs → main/gamemain.h

// 1C034.c — game-flow state handlers
extern void func_8002BA9C(s32 r, s32 g, s32 b, s32 mode);
extern void func_8002BB9C(void);
extern void func_8002BBC8(void);
extern void func_8002BE0C(Task* arg0);
extern void func_8002BFD4(void);
extern void func_8002C028(Task* arg0);
extern void func_8002C1D8(void);
// Pad_* → main/pad.h; Task_* → main/task.h
extern TaskFuncTable5 D_800134BC;
extern TaskFuncTable3 D_800134D0;
extern TaskFuncTable6 D_80013E98;
extern TaskFuncTable3 D_80013EDC;
extern GBytes4        D_80013F18;
extern TaskFuncTable4 D_80013F1C;
extern GFunc30Table6  D_80013F2C;
extern GBytes6        D_80014124;
extern void           func_8002D214(void* arg0, s32 arg1);
extern void*          func_8002D22C(s32 arg0);
extern void           func_8002D6EC(Task* arg0);

// Dynamically loaded (BSS region)
extern void  func_80097AC0(GStruct35* arg0);
extern void  func_8009850C(GStruct35* arg0);
extern void* func_80099098(Task* task);
extern void* func_80099170(Task* task, s32 arg1, s32 arg2);
extern void  func_800991DC(void* arg0);
extern void  func_80099214(void* arg0);
extern void  func_80099290(void* arg0);
extern s32   D_8005ED8C;
extern void  func_800E1A6C(void* arg0);
extern void  func_801011D0(s32* arg0, s32 arg1, s32 arg2, void* arg3);

// 1E6C4.c
extern void func_8002DEC4(void);
extern s32  func_8002DECC(GStruct38* arg0, u8* arg1, u8* arg2);
extern void func_8002E53C(GStruct38* arg0, u8* arg1);
extern void func_8002EDFC(GStruct38* arg0, u8* arg1);
extern void func_8002F5E4(GStruct38* arg0, GStruct68* arg1, s32 arg2);
extern void func_8002F69C(GStruct38* arg0, GStruct68* arg1, s32 arg2);
extern void func_8002F798(GStruct38* arg0, GStruct68* arg1);
extern void func_8002F98C(Task* arg0);

// Glyph tables (selected by GStruct38.field_C); entries are GStruct68 (0xC each).
extern u8 D_8005EFB0[];
extern u8 D_8005FA30[];
extern u8 D_800604B0[];

// Memcard product-code prefix (12 bytes, e.g. "BASLUS-01042")
extern u8 D_80060DC8[];
// Mc_FileName / Mc_Glyphs* / Mc_BufferSlots / Mc_DefaultChecksumSrc / Mc_PromptTable
// / Mc_SaveData → main/mc.h
// 64-byte character table for random memcard filename body
extern u8 D_80060E08[];
// sum / ones-complement pair written by Mc_WriteDataChecksum(0, ...); adjacent
// halfword D_80072AA8 is compared by func_80033D88 (also Mc_SaveData + 0x940)
extern s16 D_80072AA4;
extern u16 D_80072AA8;

// 201E0.c
// Extracts one text line (handles escapes/newlines) from *arg0 into arg1; advances *arg0.
// Returns -1 at end of string, 1 on newline, other values for control escapes.
extern s32 func_8002F9E0(u8** arg0, u8* arg1);
// Multi-line text draw (func_8002FDCC per line, y += 0xF). Skips draw when UiObject.field_8 == 5.
extern s32 func_8002FB84(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5,
                         s32 arg6);
extern s32 func_8002FCBC(u8* arg0);
// Multi-line text measure: packed (height<<16)|maxWidth, 0xF pixels per line.
extern s32  func_8002FD08(u8* arg0);
extern s32  func_8002FDCC(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5,
                          s32 arg6);
extern void func_80030074(void);
// Builds a memcard save filename into arg0 (prefix + arg1 char + 7 random)
extern void func_800300EC(u8* arg0, s32 arg1);
// Image data uploaded to VRAM by func_80030074
extern u_long D_80060910[];
extern u_long D_800609B0[];
// Prompt / dialog data tables (see func_80036D98 / func_80036E78 / func_80036F18 / func_80036FB8)
extern u8 D_80060A54[];
extern u8 D_80060A58[];
extern u8 D_80060A5C[];
extern u8 D_80060A64[];
// "File Information" string passed to func_80048E38 by func_80036CF0 (mcmenu)
extern char D_80013BB4[];

// mc.c
extern void func_800330D8(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

// 32B64.c
extern void func_80042364(s32 arg0, s32 arg1, s32 arg2);
extern s32  func_80042500(void);
extern void func_800429C8(s32 arg0);

// overlay / high-address
extern void func_800AC688(void);
extern void func_800ACAA8(void); // called from 2F244.c
extern s32  func_800AD284(void);

// 34E98.c
extern UiObject* func_800486F0(UiObjectDesc* arg0, s32 arg1, s32 arg2, s32 arg3,
                               UiObject* arg4);
extern void      func_80046B34(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, s32 arg6);
extern void      func_80046EEC(void* arg0, void* arg1, s32 arg2);
extern void      func_80048560(void* arg0, void* arg1, s32 arg2, s32 arg3);
extern void      func_80048838(UiObject* arg0, Task* arg1);
extern void      func_800488B8(Task* arg0);
extern void      func_800488F8(Task* arg0);
extern void      func_80047D90(GStruct30* arg0, char* arg1);
extern void      func_80047F40(GStruct30* arg0, char* arg1);
extern void      func_80048904(GStruct30* arg0, s32 arg1, s32 arg2);
extern void      func_80048964(GStruct30* arg0, void* arg1);
extern void      func_80046830(UiList* arg0, GStruct30* arg1);
extern void      func_800489A0(UiList* arg0, GStruct20* arg1);
extern void      func_80048AEC(UiList* arg0, s32 arg1);
extern void      func_80048C10(void* arg0, void* arg1);
extern void      func_80048C30(UiList* arg0, GStruct30* arg1, s32 arg2);
extern void      func_80048D58(GStruct20* arg0, s32 arg1, s32 arg2);
extern s32       func_80048E10(void* arg0, s32 arg1);
extern void      func_80047A0C(GStruct30* arg0, s32 arg1, s32 arg2, s32 arg3);
extern void      func_80047B24(GStruct30* arg0, s32 arg1, s32 arg2, s32 arg3);
extern void      func_80047C40(GStruct30* arg0, s32 arg1, s32 arg2, char* arg3, s32 arg4);
extern void      func_80048E38(GStruct30* arg0, char* arg1);
extern void      func_80048F88(GStruct30* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6);
extern void      func_8004917C(UiList* arg0, s32 arg1);
extern void      func_800491AC(GStruct30* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5);
extern void      func_80046DEC(UiList* arg0, GStruct30* arg1, s32 arg2);
extern void      func_80049288(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5);
extern void      func_800492B8(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5);
extern void      func_800457F8(GStruct30* arg0);
extern void      func_80045A3C(GStruct30* arg0, RECT* arg1, s32 arg2, s32 arg3);
extern void      func_80045B24(GStruct30* arg0);
extern void      func_80045D24(GStruct30* arg0);
extern void      func_80045F24(GStruct30* arg0);
extern void      func_80049478(GStruct30* arg0, RECT* arg1);
extern void      func_80049554(GStruct30* arg0, void* arg1);
extern void      func_800495B4(GStruct30* arg0, void* arg1);
extern void      func_8004965C(GStruct30* arg0, void* arg1);
extern void      func_8004969C(GStruct30* arg0, void* arg1);
extern void      func_800497F4(GStruct30* arg0, void* arg1);
extern void      func_80049A8C(Task* arg0);

// 3B458.c
extern s32 func_8004ACAC(s32 arg0);

// 3D458.c
extern void       func_8004CFE8(void);
extern void       func_8004D008(void);
extern GStruct42* func_8004CE28(GStruct34Payload* arg0);
extern void       F3D458_ResetHeap(void);
extern void*      F3D458_Malloc(size_t);
extern void       F3D458_Free(void* ptr);
extern void       func_8004D0F0(GStruct42* arg0);
extern GStruct42* func_8004D150(u16 arg0);
extern void       func_8004D19C(GStruct42* arg0);
extern void       func_8004D200(GStruct55* arg0, s32 arg1, s32 arg2, s32 arg3);
extern s32        func_8004D298(GStruct55* arg0, s32 arg1);
extern void       func_8004D2EC(GStruct55* arg0);
extern void       func_8004D35C(s16* arg0, s16 arg1, s32 arg2);
extern s32        func_8004D820(void);
extern void       func_8004D8BC(void);
extern GStruct8*  func_8004D94C(GStruct8* arg0);
extern void       F3D458_8004D88C(void);

// 3E48C.c
extern s16        func_8004DE18(void* arg0);
extern void       func_8004DEBC(s32 arg0);
extern void       func_8004E200(void);
extern s32        func_8004E060(s16* arg0, s32 arg1, s32 arg2);
extern void       func_8004E560(u32 voiceIdx, s32 arg1, s32 arg2);
extern s32        func_8004E5A0(s32 idx, s32 arg1, s32 arg2);
extern s32        func_8004E5C4(s8 arg0, GStruct48* arg1);
extern u8         func_8004E6A4(u32 voiceIdx);
extern void       func_8004E580(u32 voiceIdx);
extern void       func_8004E71C(u32 voiceIdx);
extern u16        func_8004E9D8(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
extern GStruct41* func_8004EA60(GStruct42* arg0, u8 arg1, u8 arg2);
extern void       F3E48C_8004E44C(void);
extern s32        F3E48C_8004E660(u32 voiceIdx);
extern void       F3E48C_QueryReverbVoices(void);
extern void       F3E48C_ConfigSpuReverb(s32 mode);
extern void       F3E48C_SetReverbDepth(s16 depth);
extern void       F3E48C_SetReverbMode(u32 mode);
extern void       F3E48C_EnableVoice(u32 voiceIdx);
extern void       F3E48C_DisableVoice(u32 voiceIdx);
extern bool       F3E48C_ReverbVoiceIsEnabled(u32 voiceIdx);
extern void       F3E48C_ApplyReverbConfig(void);

// 410B0.c
extern void       func_800508B0(void);
extern void       (*D_80068984[])(GStruct16*);
extern GStruct16* func_800509F4(void);
extern s32        func_800512BC(s32 arg0, s32 arg1);
extern s32        func_8005132C(s32 arg0, s32 arg1);
extern s32        func_800514F8(s32 arg0);
extern void       func_80050A38(GStruct16* arg0);
extern void       func_80050A90(GStruct16* arg0);
extern void       func_80050AB4(GStruct16* arg0);
extern void       func_80050AE0(GStruct16* arg0);
extern void       func_80050E3C(u8 arg0, u16 arg1);
extern void       func_80050B0C(GStruct16* arg0);
extern void       func_80050B30(GStruct16* arg0);
extern void       func_80050B54(GStruct16* arg0);
extern void       func_80050B80(GStruct16* arg0);
extern void       func_80050BBC(GStruct16* arg0);
extern void       func_80050BE8(GStruct16* arg0);
extern void       func_80050C0C(GStruct16* arg0);
extern void       func_80050C30(GStruct16* arg0);
extern void       func_800515C0(u8 arg0, u16 arg1);
extern void       func_8005166C(u8 arg0, s32 arg1);
extern void       func_80051744(u8 arg0, u8 arg1);
extern void       func_800517B4(s32 arg0);
extern s32        func_800517F8(void);
extern u8*        func_80051808(s32 arg0);
extern void*      func_80051850(s32 arg0, s32 arg1);
extern s32        func_80051460(s32 arg0, s32 arg1);
extern void       func_80051888(void);
extern void       func_800518E0(void);
extern void       func_80051964(s32 arg0);
extern void*      func_80051A2C(GStruct36* arg0, s32 arg1, u8* arg2);
extern void       func_80051AB8(GStruct36* arg0);
extern void       func_80051AF0(GStruct36* arg0);
extern void       func_80051BB0(GStruct36* arg0, GStruct36Entry* arg1);
extern void       func_80051DF4(GStruct36* arg0);
extern void*      func_80053548(s32 arg0, s32 arg1, u32 arg2);
extern s32        func_8005287C(u8* arg0, u8* arg1);
extern void       func_800528BC(s32* arg0);
extern s32        func_800528F0(s32 arg0, s32 arg1);
extern u8*        func_80052488(s32 arg0, u8* arg1, GStruct36* arg2, GStruct36Entry* arg3);
extern u8*        func_800528F8(s32 arg0, u8* arg1, GStruct36* arg2);
extern u8*        func_800529BC(s32 arg0, u8* arg1, GStruct22* arg2);
extern u8*        func_800529D8(s32 arg0, u8* arg1, GStruct36* arg2);
extern s32        func_80052B30(void* arg0);
extern s32        func_80052F80(GStruct34* arg0);
extern void       func_800530DC(GStruct34* arg0);
extern void       func_8005325C(void* arg0);
extern void       func_80053280(u8 arg0, void* arg1);
extern void       func_800532CC(void);
extern s32        func_8005333C(void* arg0);
extern s32        func_80053414(void* arg0);
extern s32        func_80053448(GStruct34* arg0);
extern void       func_8005363C(s32 arg0, void* arg1);

// 43FFC.c
extern s32  D_800689E4;
extern s32  D_800689E8;
extern s16  D_800689EC;
extern u8   D_800689F0[];
extern s8   D_80082748;
extern s8   D_80082749;
extern s8   D_8008274A;
extern s8   D_8008274B;
extern void func_800537FC(s32 arg0, s32 arg1);
extern s32  func_80053A20(s32 arg0, s32 arg1);
// Unprototyped: func_8005462C calls with indeterminate a0 (nop in delay slot).
extern s32        func_80053F00();
extern void       func_80053FA0(s32 arg0);
extern s32        func_8005414C(s32 arg0, s32 arg1, s32 arg2);
extern void       func_800544B8(s32 arg0, s32 arg1);
extern void       func_8005454C(s32 arg0, s32 arg1);
extern void       func_80054608(s8 arg0);
extern s32        func_8005462C(void);
extern void       func_8005468C(void);
extern void       func_800546C0(void);
extern void       func_800546F4(s32 arg0, u16 arg1);
extern void       func_80054F1C(void);
extern void       func_80055678(GStruct43* arg0);
extern s32        func_800558E8(s32 arg0, s8 arg1, s8 arg2, s32 arg3, GStruct67* arg4);
extern void       func_80054D58(GStruct66* arg0, u16 arg1, s32 arg2, u16 arg3);
extern s8         func_80055EF8(GStruct66* arg0, s16 arg1);
extern void       func_80055F70(s32 arg0, s8 arg1, s8 arg2, s32 arg3, s32 arg4, GStruct67* arg5);
extern s32        func_80056308(GStruct43* arg0);
extern void       func_800559BC(s32 arg0, s32 arg1);
extern void       func_80055A9C(s32 arg0, s32 arg1, s32 arg2);
extern void       func_80055B70(s32 arg0, s32 arg1);
extern void       func_80055C00(void);
extern void       func_80055C8C(void);
extern void       func_80055D78(s8 arg0);
extern s32        func_80055DAC(s32 arg0);
extern void       func_80055DFC(s8 arg0);
extern s8         func_80055EE8(void);
extern void       func_80056068(GStruct43* arg0);
extern GStruct31* func_80056104(u16 arg0, s32 arg1);
extern GStruct31* func_800561C0(s32 arg0);
extern void       func_800561EC(s32 arg0);
extern GStruct43* func_80056240(s32 arg0);
extern void       func_800562B4(GStruct57* arg0, GStruct43* arg1);
extern s32        func_800563B4(GStruct54* arg0);
extern void       func_800565B8(GStruct43* arg0, s16 arg1, u32 arg2, GStruct41* arg3);
extern s32        func_8005664C(u8* arg0, s16 arg1, GStruct59* arg2);
extern void       func_800566A4(void);
extern s32        func_80056700(void);

// 46FE4.c
extern void func_800574BC(void);
extern u8   func_80057554(void);
extern void func_80057564(void);
extern s32  func_800577AC(s32 arg0, s32 arg1);
extern s32  func_80057824(s32 arg0);
extern s32  func_80057894(s32 arg0);
extern s32  func_800578E4(s32 arg0);
extern void func_8005791C(s32 arg0);
extern void func_800579A0(s8* arg0, s8* arg1);
extern s32  func_80057A88(s32 arg0);
extern void func_80057B24(s32 arg0);
extern void func_80057B88(s32 arg0, s32 arg1);
extern s32  func_80057BC0(void);
extern void func_80057D24(void);
extern void func_8005842C(void);
extern void func_80059348(void);
extern s16  D_80068A7C[];
extern u8   D_80068A80[];
extern u8   D_80068AF0[];
extern s32  D_80068B1C;
extern s16  D_80068B28[];
extern s32  D_80068B2C[];
extern s32  (*D_80068B34[])(void);
extern u16  D_80068BB8[];
extern u16  D_80068C78[];
extern u16  D_80068D78[];
extern u16  D_80068E78[];

// 43FFC.c
extern void func_800542D0(s32 arg0, s32 arg1);

// 4A6E0.c
extern void func_8005B6EC(void);
extern void func_8005B78C(void);
extern void func_8005B830(void);
extern void func_8005B84C(s16 arg0);
extern s32  func_8005BB4C(void);
extern void func_8005BBB0(s32 arg0);

// F344.c
extern u16        D_8005EAEC;
extern u16        D_8005EAEE;
extern s32        D_8006AC08;
extern u16        D_8006AC0C;
extern u16        D_8006AC0E;
extern u16        D_8006AC10;
extern u16        D_8006AC12;
extern u16        D_8006AC14;
extern u16        D_8006AC16;
extern u16        D_8006AC18;
extern u16        D_8006AC1A;
extern u16        D_8006AC1C;
extern u16        D_8006AC1E;
extern u16        D_8006AC28;
extern u16        D_8006AC3C;
extern u_short*   D_8006AC38;
extern u_long*    D_8006AC48[];
extern u_long*    D_8006AC50[];
extern u16        D_8006AC58;
extern u16        D_8006AC5A;
extern u_long*    D_8006AC68;
extern u16        D_8006AC6C;
extern s32        StCdIntrFlag;
extern s32        func_8001EF9C(s32 arg0, s32 arg1);
extern s32        func_8001F2FC(s32 arg0);
extern void       func_8001F430(void);
extern void       func_8001F6B8(void);
extern void       func_8001F854(void);
extern void       func_80020298(s16 arg0);
extern void       func_80020388(void);
extern s16        func_8001EDC8(u8* arg0, s32 arg1, s32 arg2);
extern s16        func_8001EED8(u8* arg0);
extern u32        func_8001F180(u32);
extern GStruct24* func_80020278(u32 arg0);
extern u16        func_80020414(u32 arg0);
extern void       func_8002043C(u32 arg0);

// Other
// func_800271D4 / GameMain → main/gamemain.h
// Pad_Init → main/pad.h
extern void func_8002785C(void);
extern void func_80028718(void);
extern void func_800303AC(void);
extern s32  func_800304AC(Task* arg0, s32 arg1, s32 arg2);
extern s32  func_8003062C(Task* arg0, s32 arg1, s32 arg2);
extern s32  func_800307AC(Task* arg0, s32 arg1, s32 arg2);
extern s32  func_8003092C(Task* arg0, s32 arg1, s32 arg2);
// Mc_InitLib → main/mc.h
extern void  func_8003DB48(s32 arg0);
extern void  func_8003DE14(s32 arg0, s32 arg1, s32 arg2);
extern void  func_8003DE78(s8 arg0);
extern void  func_8003E6E4(void);
extern Task* func_8003EE68(void);
extern void  func_8003F450(s32 arg0);
extern void  func_8003F5A4(void);
extern void  func_8003F690(void);
extern s32   func_8003F86C(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
extern void  func_8003FA3C(u8 arg0);
extern s32   func_8003FB70(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3);
extern void  func_8004017C(void);
extern void  func_800405E0(void);
extern void  func_80040820(void);
extern void  func_800410F0(GStruct27* arg0);
extern void  func_800418C0(GStruct27* arg0);
extern void  func_80041E4C(void);
extern void  func_80041EB4(void);
extern void  func_8004C4D0(void);
extern void  func_8004CC58(s32 arg0);
extern void  func_8004CFC8(void);
extern void  F3D458_ResetHeap(void);
extern long  func_8004D7D4(void);
extern void  func_8004D0A0(void);
extern void  func_8004DC8C(void);
extern void  func_8004DDF0(void);
extern void  func_8004DF10(void);
extern void  func_8004D460(void*, u32, u32, s32*);
extern void  func_800509B4(void);
extern s32   func_80050D20(u32);
extern s32   func_800510D4(void);
extern void  func_80053E48(void);
extern void  func_80053E68(void);
extern void  func_80054938(void);
extern s32   func_80053F60(s32* arg0);
extern s32   func_80053FF4(u32);
extern s16   func_800AF89C(u16 arg0, u16 arg1, u16 arg2, s32 arg3);
extern void  func_800AFA44(void);
extern void  func_800B00C4(void);
extern void  func_8017D6D4(void);
extern void  func_801D4B64(Task* arg0);

// .data
extern u32          D_8005EC64;
extern s32          D_8005EC68;
extern s32          D_8005EC6C;
extern volatile u32 D_8005EC70;
extern s32          D_8005EC74;
extern s32          D_8005EC78;
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
extern UiObjectDesc D_800612D0[];
extern TaskDesc     D_8006268C;
extern GStruct17*   D_80062698;
extern TaskDesc     D_8006269C;
extern void*        D_800626A8;
extern u8           D_800626E8;
extern u8           D_80062734;
extern u8           D_80062736;
extern u8           D_80062737;
extern u8           D_80062738;
extern u8           D_80062739;
extern GPairU8*     D_8006273C[];
extern u8           D_80062764[];
extern u8           D_80062934[];
extern u8           D_80062954[];
extern u8           D_800629B0[];
extern u8           D_800629D0[];
extern u8           D_80062A24[];
extern u8           D_80062A44[];
extern u8           D_80062B30[];
extern u8           D_80062B50[];
extern u8           D_80062BA4[];
extern u8           D_80062BC4[];
extern u8           D_80062D08[];
extern u8           D_80062DB0[];
extern u8           D_80062E04[];
extern u8           D_80062E50[];
extern u8           D_80062F80[];
extern u8           D_80063068[];
extern u8           D_800630B0[];
extern s32          D_8006763C[];
extern s32          D_80067648;
extern s32          D_8006764C;
extern UiList       D_80067654;
extern GStruct29*   D_80067694;
extern u8           D_800680A4;
extern s8           D_800680AC[];
extern s8           D_800680BB;
extern u32          D_800680BC;
extern u8           D_80068184;
extern volatile u32 D_800680C0;
extern volatile s32 D_80068B58;
extern volatile u8  D_80068B5C;
extern volatile u8  D_80068B63;
extern volatile s16 D_80068B6A;
extern void         func_80059EE0(void);
extern void         func_8005BA8C(u32* arg0);
extern void         func_800B0118(s32 arg0, s32 arg1);

// D4CB64_ImgBuffers + all Fs_* data / D5B498_* FS state → main/fs.h

// 4F564
extern GStruct14* D4F564_8005ED64;
extern s32        D_8005ED68;
extern s32        D_8005ED70;
extern s32        D_8005ED74;
extern s32        D_8005ED78;
extern u16        D_8005ED8A;

extern u16* D_8005C36C;

extern s16 func_80020394(void* arg0);

// 58028
extern u8 D58028_SpuTimerEnabled;

// .bss

// 5B3FC
extern void* D_8006AC00;
extern u16   D_8006AC04;
extern void* D_8006AC40;

// Task_ActiveList / Task_DefaultList → main/task.h
extern TaskNode  D_8007A110;
extern s32       D_8007A118;
extern s32       D_8007A358;
extern u16       D_8007A35C;
extern u16       D_8007A35E;
extern void*     D_8007A360;
extern u8        D_8007A394;
extern s16       D_8007A396;
extern u8        D_8007A398;
extern s16       D_8007A39A;
extern u8        D_8007E0CC;
extern s32       D_8007E0D4;
extern GStruct42 D_8007E0D8[];

// Stream slot table living next to FS bss (F344); not pure FS API.
extern GStruct24    D_8006D4F0[15];
extern s8           D_8006EBBA;
extern s16          D_8006EBF2;
extern volatile s32 D_8006EBF4;

// 5F414
// Snapshot of Display_State.field_100 / field_103 taken before a draw; read from
// the VSync callback path (func_80027498 → func_8002731C), so volatile.
extern volatile u8 D_8006EC30;
extern u_long      D5F414_OrderingTables[2 * C5F414_OTAG_ENTRIES];
extern volatile u8 D_80070E38;
// Identity-matrix storage for GsCOORDINATE2.coord (parent at symbol - 4).
extern MATRIX    D_80070E44;
extern MATRIX    D_80070E94;
extern u8*       D_80070EE0; // primitive buffer cursor
extern GStruct35 D_80070EE8[2];
extern MATRIX    D_80070F14;
// VSync countdown; written/read by func_80027498 (VSync callback).
extern volatile s32 D_80070F64;
// Display_State → main/display.h
extern u_long*   D_800710A0; // current OT base
extern GStruct5  D_800710A8;
extern DR_TPAGE* D_80071190; // primitive buffer cursor

// 61F10
// Immediate-mode SPRT scratch used by func_8002F5E4 (DrawPrim path).
extern SPRT D_80071710;
// Mc_SaveData → main/mc.h
// Alias of Mc_SaveData.field_21 (offset 0x21).
extern s8 D_80072189;
// Alias of Mc_SaveData.field_1a9 (offset 0x1A9); loaded with lb in audio setup.
extern s8 D_80072311;
// Alias of Mc_SaveData.field_1aa (offset 0x1AA).
extern u8        D_80072312;
extern u8        D_800733F0[2][0x6C];
extern u8        D_800734C8[2][0xB0];
extern u8        D_80073628[2][0x24];
extern u8        D_80073670[2][0xE4];
extern u8        D_80073838[2][0xA4];
extern u8        D_80073980[0x208];
extern GStruct40 D_80073B88;

// 61CC0
extern GStruct14 D61CC0_800714C0;
// Pad_States → main/pad.h

// 64880
extern MATRIX D_80074080;

// 648E0
extern u8                 D_800740E0[0x6000];
extern void*              D_8007A0E0;
extern s32                D_8007A0E4;
extern GStruct50          D_8007A0E8[2];
extern u_long             D_8007A120[0x80];
extern HeapBlockHeader*   D648E0_HeapStart;
extern u8                 D648E0_HeapBuffer[C3D458_HEAP_SIZE];
extern GStruct8           D648E0_8007E0B0;
extern u32                D648E0_8007E0C8;
extern long               D648E0_SpuTimerED;
extern GStruct51Queue     D_8007E2E0;
extern GStruct51          D_8007E2E4[];
extern GStruct9           D648E0_8007E338;
extern GStruct10          D648E0_8007E518;
extern GStruct26          D_8007EB98[];
extern u32                D648E0_8007EBA8;
extern u32                D648E0_8007EBAC;
extern u32                D648E0_8007EBB0;
extern GStruct7           D648E0_SpuReverbCfg;
extern s32                D_8007EBE0;
extern GStruct16*         D_8007EBE4;
extern GStruct16*         D_8007EBE8;
extern GStruct16          D_8007EBF0[0x40];
extern u8                 D_8007F2F0;
extern GStruct36          D_8007F300;
extern u8                 D_8007F8E0[0x2800];
extern u8                 D_8007E0F8[];
extern u8                 D_8007E158;
extern u8                 D_8007E258;
extern u8                 D_80068A54[];
extern GStruct75          D_80068A60[];
extern s32                D_80068A78;
extern s32                D_800820E0;
extern s16                D_800820E4;
extern u8                 D_800820E8;
extern s8                 D_800820E9;
extern GStruct34          D_800820F0;
extern u8                 D_800820F3;
extern volatile u8        D_80082120;
extern volatile u8        D_80082121;
extern volatile u8        D_80082122;
extern volatile s32       D_80082124;
extern volatile s32       D_80082128;
extern volatile u8        D_8008212C;
extern volatile s32       D_80082130;
extern volatile s8        D_80082134;
extern volatile u8        D_80082135;
extern volatile u8        D_80082136;
extern u8                 D_80082138[0x10];
extern GStruct31          D_80082148[16];
extern GStruct54          D_80082248[8];
extern s32                D_80082548[0x80];
extern volatile s32       D_8008274C;
extern volatile s32       D_80082750;
extern u8                 D_80082754;
extern volatile GStruct39 D_80082758;
extern volatile s32       D_80082770;
extern s32                D_80082778;
extern volatile u8        D_8008277C;
extern volatile GStruct44 D_80082780;
extern GStruct52*         D_80082794;
extern volatile GStruct4  D_80082798;
extern volatile GStruct18 D_800827A0;
extern GStruct55          D_800827B4;
extern volatile u8        D_800827E4;
extern volatile u16       D_80082808;
extern volatile u16       D_80082810;
extern volatile GStruct19 D_80082818;
extern GStruct74          D_80082870;
extern volatile GStruct32 D_800828F0;
extern u8                 D_80094C8C[0x2E34];
extern u8                 D_80725C54[];

#endif // UNKNOWN_SYMS_H
