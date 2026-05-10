#ifndef UNKNOWN_SYMS_H
#define UNKNOWN_SYMS_H

#include <psyq/libspu.h>

#include "common.h"
#include "main/game.h"

#define C3D458_HEAP_SIZE        0x3D00
#define C3D458_HEAP_START_MAGIC 0xB25A
#define C3D458_HEAP_MAGIC       0xA52B

// 4CF8.c
extern void func_800148A0(void);
extern void func_800148EC(void);
extern void func_80014A50(void);
extern void func_80014A98(s32 mode);

// E734.c
extern void func_8002D248(GStruct0Node* node);

// C37C.c
extern void func_8001D3F8(void);

// 16494.c
extern void F16494_ResetSpuAttr(void);

// 1C034.c
extern void      func_8002D248(GStruct0Node* node);
extern void      func_8002D25C(GStruct0Node* node);
extern GStruct2* func_8002D304(u32 idx1, u32 idx2);
extern GStruct2* func_8002D32C(GStruct2* base, u32 idx);
extern void      func_8002D444(GStruct0* state);
extern void      func_8002D474(GStruct0* state);

// 3D458.c
extern void  F3D458_ResetHeap(void);
extern void* F3D458_Malloc(size_t);
extern void  F3D458_Free(void* ptr);
extern void  F3D458_8004D88C(void);

// 3E48C.c
extern void F3E48C_ConfigSpuReverb(s32 mode);

// 43FFC.c
extern s32 func_80056700(void);

// 46FE4.c
extern void func_800574BC(void);

// Other
extern u32  func_8001F180(u32);
extern void func_8002043C(u32 arg0);
extern void func_800271D4(void);
extern void func_80028664(void);
extern void func_8002785C(void);
extern void func_800303AC(void);
extern void func_80033BBC(void);
extern void func_8004CC58(s32 arg0);
extern void func_8004CFC8(void);
extern void F3D458_ResetHeap(void);
extern long func_8004D7D4(void);
extern void func_8004D0A0(void);
extern void func_8004DDF0(void);
extern void func_8004DF10(void);
extern void func_8004D460(void*, u32, u32, s32*);
extern void func_800509B4(void);
extern void func_80050D20(u32);
extern void func_80053E68(void);
extern void func_80053F60(void);
extern void func_80053FF4(u32);

// .data
extern u32       D_8005EC64;
extern GStruct2* D_8005EF74[];
extern u8        D_800680A4;
extern u32       D_800680BC;
extern u32       D_800680C0;

// 58028
extern u8 D58028_SpuTimerEnabled;

// .bss

extern GStruct0Node* D_800716D8;
extern GStruct0Node  D_800716E0;
extern GStruct0Node  D_8007A110;
extern u8            D_8007E0CC;

// 5B498
extern SpuCommonAttr D5B498_SpuAttr;
extern s16           D5B498_8006EBF0;

// 5F414
extern GStruct1 D_80070F68; // 0x80070F68 - 0x800710A0

// 61CC0
extern GStruct5 D_800710A8;

// 648E0
extern HeapBlockHeader*  D648E0_HeapStart;
extern u8                D648E0_HeapBuffer[C3D458_HEAP_SIZE];
extern GStruct8          D648E0_8007E0B0;
extern u32               D648E0_8007E0C8;
extern long              D648E0_SpuTimerED;
extern GStruct7          D648E0_SpuReverbCfg;
extern volatile GStruct4 D_80082798;

#endif // UNKNOWN_SYMS_H
