#ifndef ROOMS_MIST_SHOOTING_GALLERY_H
#define ROOMS_MIST_SHOOTING_GALLERY_H

#include "gameplay/area.h"

#include "main/ui_types.h"

#include "types.h"

#include "common.h"

#include "main/task_types.h"

/// Per-run state of the Mist shooting gallery mini-game: a 0x24-byte
/// `memCalloc` allocation that `func_mist_shooting_gallery_80182B1C` stores at
/// `Task::work` of the gallery's controller task, which it also publishes in
/// `D_mist_shooting_gallery_8018E0C4` so the round scripts can reach it without
/// a task pointer. `difficulty` is seeded from the low nibble of the task's
/// `spawnArg1` and gates the scoring rules (`< 3` and `< 2` branches).
typedef struct MistShootingGalleryWork {
    /* 0x00 */ u16  field_00;
    /* 0x02 */ u16  field_02;
    /* 0x04 */ u16  field_04;
    /* 0x06 */ u16  field_06;
    /* 0x08 */ s16  field_08;
    /* 0x0A */ s16  field_0A;
    /* 0x0C */ s16  field_0C;
    /* 0x0E */ u8   field_0E;
    /* 0x0F */ byte pad_0F[0xD];
    /* 0x1C */ u8   difficulty;
    /* 0x1D */ u8   field_1D;
    /* 0x1E */ u8   field_1E;
    /* 0x1F */ u8   field_1F;
    /* 0x20 */ u8   field_20;
    /* 0x21 */ u8   field_21;
    /* 0x22 */ u8   field_22;
    /* 0x23 */ byte pad_23[0x1];
} MistShootingGalleryWork;
STATIC_ASSERT_SIZEOF(MistShootingGalleryWork, 0x24);

extern Task* D_mist_shooting_gallery_8018E0C4;

s32 func_mist_shooting_gallery_80184470(s32 score);
s32 func_mist_shooting_gallery_80184970(s32 arg0);

void func_mist_shooting_gallery_80180390(s32 arg0);

s32 func_mist_shooting_gallery_80180B34(s32 unused);

void func_mist_shooting_gallery_801811C0(s16 arg0);

void func_mist_shooting_gallery_801848B4(void);

void func_mist_shooting_gallery_80184954(void);

extern TaskDesc D_mist_shooting_gallery_80185384[3];

extern TaskDesc D_mist_shooting_gallery_801856B8[2];

void func_mist_shooting_gallery_8017DCAC(s32 mode);

s32 func_mist_shooting_gallery_8017F95C(s32 unused);

extern GpAreaVariant D_mist_shooting_gallery_8018DF74[12];
extern UiObjectDesc D_mist_shooting_gallery_80185000;

void func_mist_shooting_gallery_8017FBD8(void);

extern UiObjectDesc D_mist_shooting_gallery_80184F70;

void func_mist_shooting_gallery_801811EC(Task* unused);

void func_mist_shooting_gallery_80182064(Task* task);

#endif // ROOMS_MIST_SHOOTING_GALLERY_H
