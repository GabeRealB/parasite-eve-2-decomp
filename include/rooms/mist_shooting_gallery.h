#ifndef INCLUDE_ROOMS_MIST_SHOOTING_GALLERY_H
#define INCLUDE_ROOMS_MIST_SHOOTING_GALLERY_H

#include "common.h"

#include "main/tmd_types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/ui_types.h"

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

extern TaskDesc D_mist_shooting_gallery_80185384[3];

extern TaskDesc D_mist_shooting_gallery_801856B8[2];

extern GpAreaVariant D_mist_shooting_gallery_8018DF74[12];

extern UiObjectDesc D_mist_shooting_gallery_80185000;

extern UiObjectDesc D_mist_shooting_gallery_80184F70;

// mist_shooting_gallery
extern GpRoomObjRec D_mist_shooting_gallery_801853A8[];

extern u8* D_mist_shooting_gallery_801853B8[];

extern GpViewCountRec D_mist_shooting_gallery_801853BC[];

/// Light collection and per-view ambient minima for the gallery's single room.
///
/// Indexed by the 1-based room ID minus one; only room 1 is valid. The mutable
/// `lights` pointer initially selects the default collection and may switch to
/// the alternate collection. Both share `ambientTable` for views 1..18.
/// The table and its borrowed data are valid only while the room overlay is loaded.
extern WorldCoordRoomLighting gMistShootingGalleryRoomLightingTable[1];

extern GpWarpRec D_mist_shooting_gallery_801853C8[];

extern GpViewRec D_mist_shooting_gallery_8018998C[];

extern SpriteView D_mist_shooting_gallery_8018BD10[];

extern WorldCollisionSurfaceProperties* D_mist_shooting_gallery_8018E09C[];

void func_mist_shooting_gallery_80180390(s32 arg0);

s32 func_mist_shooting_gallery_80180B34(s32 unused);

void func_mist_shooting_gallery_801811C0(s16 arg0);

void func_mist_shooting_gallery_801848B4(void);

void func_mist_shooting_gallery_80184954(void);

void func_mist_shooting_gallery_8017DCAC(s32 mode);

s32 func_mist_shooting_gallery_8017F95C(s32 unused);

void func_mist_shooting_gallery_8017FBD8(void);

void func_mist_shooting_gallery_801811EC(Task* unused);

void func_mist_shooting_gallery_80182064(Task* task);

void func_mist_shooting_gallery_8018018C(Task* task);

extern TmdSource gMistShootingGalleryModel093FC;

extern TmdSource gMistShootingGalleryModel095EC;

extern TmdSource gMistShootingGalleryModel097DC;

extern TmdSource gMistShootingGalleryModel099CC;

extern TmdSource gMistShootingGalleryModel09BBC;

extern TmdSource gMistShootingGalleryModel09DAC;

extern TmdSource gMistShootingGalleryModel09F9C;

extern TmdSource gMistShootingGalleryModel0A18C;

extern TmdSource gMistShootingGalleryModel0A37C;

extern TmdSource gMistShootingGalleryModel0A56C;

extern TmdSource gMistShootingGalleryModel0A81C;

extern TmdSource gMistShootingGalleryModel0AB30;

extern TmdSource gMistShootingGalleryModel0AC94;

extern TmdSource gMistShootingGalleryModel0ADF8;

extern TmdSource gMistShootingGalleryModel0AF5C;

extern TmdSource gMistShootingGalleryModel0B0D0;

extern TmdSource gMistShootingGalleryModel0B2C0;

extern TmdSource gMistShootingGalleryModel0B4B0;

extern TmdSource gMistShootingGalleryModel0B6A0;

#endif // INCLUDE_ROOMS_MIST_SHOOTING_GALLERY_H
