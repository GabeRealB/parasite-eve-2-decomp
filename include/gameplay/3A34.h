#ifndef GAMEPLAY_3A34_H
#define GAMEPLAY_3A34_H

#include "common.h"

#include "gameplay/3FB8.h"
#include "main/task.h"

/// Singly-linked node unlinked by `func_800DAB38` / linked by `func_800DABEC`.
/// `field_6 == 1` means the node is on the `D_80115268` list.
typedef struct _GpLinkNode {
    /* 0x00 */ struct _GpLinkNode* next;
    /* 0x04 */ u8                  field_4;
    /* 0x05 */ u8                  field_5;
    /* 0x06 */ u8                  field_6;
} GpLinkNode;
STATIC_ASSERT_SIZEOF(GpLinkNode, 0x8);

/// Linked object used as a list head/node by the 3A34 pair/filter helpers.
/// `next` is at 0x0 and `flags` is at 0x1E; full object size is not known yet.
typedef struct _GpObj {
    /* 0x00 */ struct _GpObj* next;
    /* 0x04 */ byte           pad_4[0x1A];
    /* 0x1E */ u16            flags;
} GpObj;

/// Object whose word at 0x38 is returned by `func_800D9788`. Adjacent light
/// helpers load 0x38/0x3C/0x40 as three s32s and take `&obj->field_38` as a
/// `VECTOR*`.
typedef struct _GpObj38 {
    /* 0x00 */ byte pad_0[0x38];
    /* 0x38 */ s32  field_38;
} GpObj38;
STATIC_ASSERT_SIZEOF(GpObj38, 0x3C);

/// Object whose flags byte at 0x4C is OR'd by `func_800E3008`. Nearby
/// helpers treat 0x4C as a flag field (bits 0x1, 0x2, 0x4).
typedef struct _GpObj4C {
    /* 0x00 */ byte pad_0[0x4C];
    /* 0x4C */ u8   field_4C;
} GpObj4C;
STATIC_ASSERT_SIZEOF(GpObj4C, 0x4D);

/// Global at `D_801153F0`. `field_0` is a state byte (1 if first set by
/// `func_800DB4E0`; 2 when the last `field_6` ref is released). `field_6`
/// is a u16 refcount incremented by `func_800DB53C` and decremented by
/// `func_800DB558` / `func_800DB630` / `func_800DB6B4`. Nearby helpers also
/// touch bytes at 0x1..0x3 and words at 0x8 / 0xC / 0x10.
typedef struct _GpStateF0 {
    /* 0x00 */ u8   field_0;
    /* 0x01 */ byte pad_1[5];
    /* 0x06 */ u16  field_6;
} GpStateF0;
STATIC_ASSERT_SIZEOF(GpStateF0, 0x8);

extern GpStateF0 D_801153F0;
extern u8        D_801153F3;
extern GpObj* D_80115570;
extern GpObj* D_80115574;
extern GpObj* D_80115578;
extern GpObj* D_8011557C;
extern GpObj* D_80115580;
extern GpObj* D_80115588;
extern GpObj* D_8011558C;
extern GpObj* D_80115590;
extern s32    D_80115424;

s32   func_800D9788(GpObj38* arg0);
void  func_800D9CC8(Task* arg0);
void  func_800D9DFC(void);
void  func_800DAB38(GpLinkNode* node);
void* func_800DA2A0(GpActorWork* arg0, VECTOR3* pos, s32 arg2);
void* func_800DAD54(GpActorWork* arg0);
void  func_800DB4E0(void);
void  func_800DB530(s32 arg0);
void  func_800DB53C(void);
void  func_800DB72C(void);
void func_800DB900(GpObj* node);
void func_800E0414(GpObj* a, GpObj* b);
void func_800E0540(GpObj* node);
void func_800E0608(GpObj* node, s32 mask, s32 match);
void func_800E06AC(GpObj* node, s32 mask, s32 match);
s32  func_800E076C(void);
void func_800E0B08(void);
void func_800E1638(GpObj* node);
void func_800E3008(GpObj4C* arg0);
void func_8010154C(void);

#endif // GAMEPLAY_3A34_H
