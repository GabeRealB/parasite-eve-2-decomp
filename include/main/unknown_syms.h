#ifndef UNKNOWN_SYMS_H
#define UNKNOWN_SYMS_H

#include "common.h"
#include "main/game.h"

// 4CF8.c
extern void func_80014A50(void);

// E734.c
extern void func_8002D248(GStruct0Node* node);

// C37C.c
extern void func_8001D3F8(void);

// 1C034.c
extern void      func_8002D248(GStruct0Node* node);
extern void      func_8002D25C(GStruct0Node* node);
extern GStruct2* func_8002D304(u32 idx1, u32 idx2);
extern GStruct2* func_8002D32C(GStruct2* base, u32 idx);
extern void      func_8002D444(GStruct0* state);
extern void      func_8002D474(GStruct0* state);

// 43FFC.c
extern s32 func_80056700(void);

// 46FE4.c
extern void func_800574BC(void);

// Other
extern u32  func_8001F180(u32);
extern void func_8002043C(u32 arg0);
extern void func_800271D4(void);
extern void func_8002604C(void);
extern void func_80028664(void);
extern void func_8002785C(void);
extern void func_800303AC(void);
extern void func_80033BBC(void);
extern void func_8004CC58(s32 arg0);
extern void func_8004CFC8(void);
extern void func_8004D5A8(void);
extern long func_8004D7D4(void);
extern void func_8004D0A0(void);
extern void func_8004D88C(void);
extern void func_8004DDF0(void);
extern void func_8004DF10(void);
extern void func_8004D460(void*, u32, u32, s32*);
extern s32* func_8004D5D8(u32);
extern void func_8004E788(u32 arg0);
extern void func_800509B4(void);
extern void func_80050D20(u32);
extern void func_80053E68(void);
extern void func_80053F60(void);
extern void func_80053FF4(u32);

// .data
extern u32       D_8005EC64;
extern GStruct2* D_8005EF74[];
extern u8        D_800680A5;
extern u8        D_800680A4;
extern u32       D_800680BC;
extern u32       D_800680C0;

// .bss

extern GStruct1      D_80070F68; // 0x80070F68 - 0x800710A0
extern u32           D_800710A8;
extern GStruct0Node* D_800716D8;
extern GStruct0Node  D_800716E0;
extern GStruct0Node  D_8007A110;
extern u8            D_8007E0CC;
extern long          D_8007E0D0;

// 648E0
extern volatile GStruct4 D_80082798;

#endif // UNKNOWN_SYMS_H
