#ifndef GAMEPLAY_3A34_H
#define GAMEPLAY_3A34_H

#include "common.h"

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

void func_800DB72C(void);
void func_800DB900(GpObj* node);
void func_800E0414(GpObj* a, GpObj* b);
void func_800E0540(GpObj* node);
void func_800E0608(GpObj* node, s32 mask, s32 match);
void func_800E06AC(GpObj* node, s32 mask, s32 match);
void func_800E0B08(void);
void func_8010154C(void);

#endif // GAMEPLAY_3A34_H
