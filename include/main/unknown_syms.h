#ifndef UNKNOWN_SYMS_H
#define UNKNOWN_SYMS_H

#include "common.h"

extern void func_80014A50(void);
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

// TODO: These seem to be a pointer and size to a second auxilary heap.
// `Mem_SetActiveAuxHeap` switches between these and those in `GAuxHeap`.
// Could be that the game uses different heaps for each based on the kind
// of data that must be allocated.
extern u8*    D_800691F4;
extern size_t D_800691F8;

extern u32  D_8005EC64;
extern u8   D_800680A5;
extern u8   D_800680A4;
extern u32  D_800680BC;
extern u32  D_800680C0;
extern long D_8007E0D0;
extern u32  D_8007108C;
extern u32  D_800710A8;
extern u8   D_8007E0CC;

#endif // UNKNOWN_SYMS_H
