#ifndef GAMEPLAY_3A34_H
#define GAMEPLAY_3A34_H

#include "common.h"

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

extern GpObj* D_80115570;
extern GpObj* D_80115574;
extern GpObj* D_80115578;
extern GpObj* D_8011557C;
extern GpObj* D_80115580;
extern GpObj* D_80115588;
extern GpObj* D_8011558C;
extern GpObj* D_80115590;
extern s32    D_80115424;

void func_800D9CC8(Task* arg0);
void func_800DAB38(GpLinkNode* node);
void func_800DB72C(void);
void func_800DB900(GpObj* node);
void func_800E0414(GpObj* a, GpObj* b);
void func_800E0540(GpObj* node);
void func_800E0608(GpObj* node, s32 mask, s32 match);
void func_800E06AC(GpObj* node, s32 mask, s32 match);
s32  func_800E076C(void);
void func_800E0B08(void);
void func_800E1638(GpObj* node);
void func_8010154C(void);

#endif // GAMEPLAY_3A34_H
