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

#define C12D18_800256F4_ARG_0 0x0
#define C12D18_800256F4_ARG_2 0x2

#define D5B498_8006C228_FF -1

// 4CF8.c
extern void func_800144F8(s32 arg0, s32 arg1);
extern void F04CF8_800148A0(void);
extern void F04CF8_800148EC(void);
extern void F04CF8_80014A50(void);
extern void F04CF8_80014A98(s32 mode);
extern void func_80014C2C(void);
extern bool F04CF8_StageCdfIsAvailable(u32 stageIdx);

// E734.c
extern void func_8002D248(GStruct0Node* node);

// C37C.c
extern void func_8001BB7C(void);
extern void func_8001BE60(void);
extern void func_8001C0D4(void);
extern void func_8001C620(void);
extern void func_8001C970(void);
extern void func_8001CA70(void);
extern u16  func_8001CDF0(void);
extern void func_8001CEFC(void);
extern u16  func_8001D0E8(void);
extern void func_8001D2B0(s32 arg0, u8* arg1, u8* arg2);
extern u16  func_8001D344(void);
extern u16  func_8001D37C(s16 arg0);
extern void func_8001D588(void);
extern void func_8001D628(void);
extern void func_8001D6B8(void);
extern void func_8001D6FC(s32 arg0, u8* arg1, u8* arg2);
extern u16  func_8001D82C(void);
extern void func_8001DB84(void);
extern void F0C37C_ClearD80068FA0(void);

// 11E9C.c
extern void func_8002207C(void);
extern void func_8002226C(void*, void*);

// 12D18.c
extern void func_80010024(void);
extern void F12D18_80022518(void);
extern void F12D18_8002252C(u8* arg0, s16 arg1);
extern void F12D18_80022598(void);
extern void F12D18_800225D4(void);
extern void func_80022BD0(u8 status, u8* result);
extern u8   func_80022CF0(void);
extern u8   func_800231A8(void);
extern void F12D18_InitStage0TablesCb(u8 status, u8* result);
extern void F12D18_InitFolderTable(void);
extern void F12D18_InitStage0Tables(void);
extern void F12D18_ClearDiskError(void);
extern void F12D18_80024EC0(void);
extern u8   F12D18_WaitDiskSwap(void);
extern void F12D18_ReadSector2(s32 sector, s32 arg1, u8* arg2, u8 arg3);
extern void F12D18_ReadSector(s32 sector);
extern void F12D18_WaitDiskReset(s8 withSectHdr);
extern void F12D18_SeekToPos(s32 sector);
extern void F12D18_80025580(u8 status, u8* result);
extern void F12D18_8002563C(u8 status, u8* result);
extern void F12D18_SeekToPosCb(u8 status, u8* result);
extern void F12D18_800256F4(u8 arg0);
extern void F12D18_ContinueDrawing(u_long* ot);
extern u8*  func_800257A4(void);
extern void F12D18_800257B0(void);
extern void F12D18_StopCd(void);

// 16494.c
extern void func_80025C94(u8 status, u8* result);
extern void F16494_ResetSpuAttr(void);
extern void func_800260B0(s32 arg0);

// 179D4.c
extern void func_80027498(void);
extern void func_800280F4(s32);
extern void func_800281D4(void);
extern void F179D4_ClearOTag(s16 tableIdx);

// 1C034.c
extern void      func_8002BB9C(void);
extern void      func_8002BBC8(void);
extern void      func_8002BE0C(GStruct0* arg0);
extern void      func_8002BFD4(void);
extern s32       func_8002C868(s32 arg0, s32 arg1, s32 arg2);
extern void      func_8002C9B0(s32 arg0);
extern void      func_8002C9E0(s32 arg0);
extern void      func_8002CCB8(GStruct0* arg0);
extern void      func_8002CB04(GStruct2* arg0, s32 arg1, s32 arg2, GStruct0Node* arg3);
extern void      func_8002CFA0(GStruct2* arg0, s32 arg1, s32 arg2, s32 arg3);
extern void      func_8002CFDC(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
extern void      func_8002D248(GStruct0Node* node);
extern void      func_8002D25C(GStruct0Node* node);
extern GStruct2* func_8002D304(u32 idx1, u32 idx2);
extern GStruct2* func_8002D32C(GStruct2* base, u32 idx);
extern void      func_8002D0A4(GStruct0* arg0);
extern void      func_8002D214(void* arg0, s32 arg1);
extern void*     func_8002D22C(s32 arg0);
extern void      func_8002D428(void);
extern void      func_8002D444(GStruct0* state);
extern void      func_8002D474(GStruct0* state);

// 1E6C4.c
extern void func_8002F98C(GStruct0* arg0);

// 201E0.c
extern void func_80030074(void);

// 32B64.c
extern void func_800429C8(s32 arg0);

// 34E98.c
extern void func_80046B34(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, s32 arg6);
extern void func_80046EEC(void* arg0, void* arg1, s32 arg2);
extern void func_80048560(void* arg0, void* arg1, s32 arg2, s32 arg3);
extern void func_800488B8(GStruct0* arg0);
extern void func_800488F8(GStruct0* arg0);
extern void func_80048904(GStruct30* arg0, s32 arg1, s32 arg2);
extern void func_80048964(GStruct30* arg0);
extern void func_80048C10(void* arg0, void* arg1);
extern void func_80048D58(void* arg0, s32 arg1, s32 arg2);
extern void func_80049288(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5);
extern void func_800492B8(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5);
extern void func_80045D24(GStruct30* arg0);
extern void func_80049554(GStruct30* arg0, void* arg1);
extern void func_800495B4(GStruct30* arg0, void* arg1);
extern void func_8004965C(GStruct30* arg0, void* arg1);
extern void func_800497F4(GStruct30* arg0, void* arg1);
extern void func_80049A8C(GStruct0* arg0);

// 3D458.c
extern void  func_8004CFE8(void);
extern void  F3D458_ResetHeap(void);
extern void* F3D458_Malloc(size_t);
extern void  F3D458_Free(void* ptr);
extern void  func_8004D0F0(void* arg0);
extern s32   func_8004D820(void);
extern void  func_8004D8BC(void);
extern void  F3D458_8004D88C(void);

// 3E48C.c
extern void func_8004E200(void);
extern s32  func_8004E060(s16* arg0, s32 arg1, s32 arg2);
extern void F3E48C_8004E44C(void);
extern s32  F3E48C_8004E660(u32 voiceIdx);
extern void F3E48C_QueryReverbVoices(void);
extern void F3E48C_ConfigSpuReverb(s32 mode);
extern void F3E48C_SetReverbDepth(s16 depth);
extern void F3E48C_SetReverbMode(u32 mode);
extern void F3E48C_EnableVoice(u32 voiceIdx);
extern void F3E48C_DisableVoice(u32 voiceIdx);
extern bool F3E48C_ReverbVoiceIsEnabled(u32 voiceIdx);
extern void F3E48C_ApplyReverbConfig(void);

// 410B0.c
extern GStruct16* func_800509F4(void);
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
extern void*      func_80051850(s32 arg0, s32 arg1);
extern void       func_80051888(void);
extern void*      func_80053548(s32 arg0, s32 arg1, u32 arg2);
extern s32        func_800528F0(s32 arg0, s32 arg1);
extern u8*        func_800529BC(s32 arg0, u8* arg1, GStruct22* arg2);
extern void       func_8005325C(void* arg0);
extern void       func_800532CC(void);
extern s32        func_80053414(void* arg0);
extern void       func_8005363C(s32 arg0, void* arg1);

// 43FFC.c
extern s16  D_800689EC;
extern s8   D_80082748;
extern void func_800537FC(s32 arg0, s32 arg1);
extern s32  func_80053A20(s32 arg0, s32 arg1);
extern s32  func_80053F00();
extern void func_80053FA0(s32 arg0);
extern void func_8005454C(s32 arg0, s32 arg1);
extern void func_80054608(s8 arg0);
extern s32  func_8005462C(void);
extern void func_8005468C(void);
extern void func_800546C0(void);
extern void func_800546F4(s32 arg0, u16 arg1);
extern void func_80054F1C(void);
extern void func_800558E8(s32 arg0, s8 arg1, s8 arg2, s32 arg3, s32 arg4);
extern void func_800559BC(s32 arg0, s32 arg1);
extern void func_80055A9C(s32 arg0, s8 arg1, s8 arg2);
extern void func_80055B70(s32 arg0, u8 arg1);
extern void func_80055C00(void);
extern void func_80055C8C(void);
extern void func_80055D78(s8 arg0);
extern s32  func_80055DAC(s32 arg0);
extern void func_80055DFC(s8 arg0);
extern s8         func_80055EE8(void);
extern GStruct31* func_800561C0(s32 arg0);
extern void       func_800561EC(s32 arg0);
extern void       func_800566A4(void);
extern s32        func_80056700(void);

// 46FE4.c
extern void func_800574BC(void);
extern u8   func_80057554(void);
extern s32  func_800577AC(s32 arg0, s32 arg1);
extern s32  func_80057824(s32 arg0);
extern s32  func_80057894(s32 arg0);
extern s32  func_800578E4(s32 arg0);
extern void func_8005791C(s32 arg0);
extern void func_800579A0(s8* arg0, s8* arg1);
extern s32  func_80057A88(s32 arg0);
extern void func_80057B88(s32 arg0, s32 arg1);
extern s32  func_80057BC0(void);
extern void func_80057D24(void);
extern u8   D_80068A80[];
extern s16  D_80068B28[];

// 43FFC.c
extern void func_800542D0(s32 arg0, s32 arg1);

// 4A6E0.c
extern void func_8005B830(void);
extern s32  func_8005BB4C(void);
extern void func_8005BBB0(s32 arg0);

// F344.c
extern s16       D_8006AC28;
extern void      func_80020388(void);
extern u32       func_8001F180(u32);
extern GStruct24* func_80020278(u32 arg0);
extern u16       func_80020414(u32 arg0);
extern void      func_8002043C(u32 arg0);

// Other
extern void func_800271D4(void);
extern void func_80028664(void);
extern void func_8002785C(void);
extern void func_80028718(void);
extern void func_800303AC(void);
extern s32  func_800304AC(GStruct0* arg0, s32 arg1, s32 arg2);
extern void func_80033BBC(void);
extern void func_8003DB48(s32 arg0);
extern void func_8003DE14(s32 arg0, s32 arg1, s32 arg2);
extern void func_8003DE78(s8 arg0);
extern void func_8003E6E4(void);
extern void func_8003EE68(void);
extern void func_8003F450(s32 arg0);
extern void func_800410F0(GStruct27* arg0);
extern void func_8004CC58(s32 arg0);
extern void func_8004CFC8(void);
extern void F3D458_ResetHeap(void);
extern long func_8004D7D4(void);
extern void func_8004D0A0(void);
extern void func_8004DC8C(void);
extern void func_8004DDF0(void);
extern void func_8004DF10(void);
extern void func_8004D460(void*, u32, u32, s32*);
extern void func_800509B4(void);
extern void func_80050D20(u32);
extern void func_80053E48(void);
extern void func_80053E68(void);
extern s32 func_80053F60(s32* arg0);
extern void func_80053FF4(u32);
extern void func_800AFA44(void);
extern void func_800B00C4(void);
extern void func_8017D6D4(void);
extern void func_801D4B64(GStruct0* arg0);

// .data
extern u32          D_8005EC64;
extern volatile u32 D_8005EC70;
extern GStruct2*    D_8005EF74[];
extern GStruct17*   D_80062698;
extern GStruct2     D_8006269C;
extern void*        D_800626A8;
extern u8           D_80062734;
extern s32          D_8006763C[];
extern s32          D_80067648;
extern s32          D_8006764C;
extern GStruct29*   D_80067694;
extern u8           D_800680A4;
extern s8           D_800680AC[];
extern u32          D_800680BC;
extern volatile u32 D_800680C0;
extern u8           D_80068B5C;

// 4CB64
extern GStruct11* D4CB64_ImgBuffers;

// 4F564
extern GStruct14* D4F564_8005ED64;
extern s32        D_8005ED68;

// 58028
extern u8 D58028_SpuTimerEnabled;

// .bss

// 5B3FC
extern u16 D_8006AC04;

extern GStruct0Node* D_800716D8;
extern GStruct0Node  D_800716E0;
extern GStruct0Node  D_8007A110;
extern u8            D_8007E0CC;

// 5B498
extern s16            D5B498_8006AC98;
extern u16            D5B498_8006AC9A;
extern u16            D5B498_8006AC9C;
extern void*          D5B498_8006ACAC;
extern void*          D5B498_8006ACB0;
extern GStruct15      D5B498_8006ACB8;
extern s16            D5B498_8006ACC0;
extern u8             D5B498_8006ACC8;
extern s32            D5B498_Stage0HdrSect;
extern s32            D5B498_SeekPos;
extern u16            D5B498_8006ACD4;
extern RECT           D5B498_ImageRect;
extern GStruct13      D5B498_8006ACE8[0x1F];
extern u8             D5B498_8006ADE0;
extern u8             D5B498_8006ADE1;
extern u8             D5B498_8006ADF4;
extern u16            D5B498_Stage0FileSect0[0x30];
extern u16            D5B498_Stage0FileSect5[0x40];
extern FsCdfFileSmall D5B498_Stage0FileTable3[0x1e];
extern u8             D5B498_Stage0FileTable3Len;
extern FsCdfFileSmall D5B498_Stage0FileTable4[0x46];
extern u8             D5B498_Stage0FileTable4Len;
extern FsCdfFileSmall D5B498_Stage0FileTable1[0x3c];
extern u8             D5B498_Stage0FileTable1Len;
extern FsCdfFileSmall D5B498_Stage0FileTable2[0x160];
extern u16            D5B498_Stage0FileTable2Len;
extern FsCdfFile      D5B498_Stage0FileTable[0x10e];
extern u16            D5B498_Stage0FileTableLen;
extern u32            D5B498_Stage0FileSect90[0x8];
extern FsCdfFolder    D5B498_FolderTable[50];
extern u16            D5B498_FolderTableLen;
extern s32            D5B498_ReqCdSector;
extern u8             D5B498_8006C228;
extern u8*            D5B498_8006C22C;
extern u8             D5B498_8006C230;
extern u8             D5B498_8006C231;
extern u8             D5B498_8006C232;
extern u8             D5B498_8006C233;
extern u8             D5B498_8006C234;
extern FsSector       D5B498_CdSectorBuffer;
extern u8*            D5B498_8006D4D8;
extern u8             D5B498_8006D4E0[0x10];
extern GStruct24      D_8006D4F0[15];
extern u16            D5B498_8006D748;
extern void*          D5B498_8006D850;
extern s32            D5B498_8006D854;
extern u16            D5B498_8006D858;
extern u16            D5B498_8006D85A;
extern u8             D5B498_CdfEndFlag;
extern u_long         D5B498_8006D870[0x460];
extern s32            D5B498_StageSectors[FS_CDF_STAGE_COUNT];
extern s32            D5B498_CurrVBlank;
extern s32            D5B498_CurrCdSector;
extern u8             D5B498_CdErrorCount;
extern u16            D5B498_8006EA1A;
extern FsCdfStream    D5B498_Streams[0xa];
extern u16            D5B498_8006EBB0;
extern s8             D_8006EBBA;
extern SpuCommonAttr  D5B498_SpuAttr;
extern s16            D5B498_8006EBF0;
extern s16            D_8006EBF2;
extern volatile s32   D_8006EBF4;

// 5F414
extern u_long    D5F414_OrderingTables[2 * C5F414_OTAG_ENTRIES];
extern GStruct35 D_80070EE8[2];
extern GStruct1  D_80070F68; // 0x80070F68 - 0x800710A0
extern GStruct5  D_800710A8;

// 61F10
extern GStruct23 D_80072168;

// 61CC0
extern GStruct14            D61CC0_800714C0;
extern volatile GStruct25   D_80071620[2];

// 64880
extern MATRIX D_80074080;

// 648E0
extern u8                D_800740E0[0x6000];
extern void*             D_8007A0E0;
extern s32               D_8007A0E4;
extern HeapBlockHeader*  D648E0_HeapStart;
extern u8                D648E0_HeapBuffer[C3D458_HEAP_SIZE];
extern GStruct8          D648E0_8007E0B0;
extern u32               D648E0_8007E0C8;
extern long              D648E0_SpuTimerED;
extern s32               D_8007E2E0[0x15];
extern GStruct9          D648E0_8007E338;
extern GStruct10         D648E0_8007E518;
extern GStruct26         D_8007EB98[];
extern u32               D648E0_8007EBA8;
extern u32               D648E0_8007EBAC;
extern u32               D648E0_8007EBB0;
extern GStruct7          D648E0_SpuReverbCfg;
extern u8                D_8007F2F0;
extern u8                D_8007F8E0[0x2800];
extern u8                D_8007E0F8[];
extern u8                D_8007E158;
extern u8                D_8007E258;
extern u8                 D_80068A54[];
extern u8                 D_800820E8;
extern u8                 D_800820E9;
extern GStruct34          D_800820F0;
extern volatile u8        D_80082120;
extern volatile u8        D_80082121;
extern volatile u8        D_80082122;
extern volatile s32       D_80082124;
extern volatile s32       D_80082128;
extern u8                 D_8008212C;
extern volatile s32       D_80082130;
extern volatile s8        D_80082134;
extern volatile u8        D_80082135;
extern volatile u8        D_80082136;
extern u8                 D_80082138[0x10];
extern GStruct31          D_80082148[16];
extern volatile s32       D_8008274C;
extern volatile GStruct4  D_80082798;
extern volatile GStruct18 D_800827A0;
extern volatile GStruct19 D_80082818;
extern volatile GStruct32 D_800828F0;
extern u8                 D_80094C8C[0x2E34];
extern u8                 D_80725C54[];

#endif // UNKNOWN_SYMS_H
