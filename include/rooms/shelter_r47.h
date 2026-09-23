#ifndef ROOMS_SHELTER_R47_H
#define ROOMS_SHELTER_R47_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_80181228.h"

/// Sparse view of the adjacent records at 0x80187960 and 0x80187C0C.
/// The room's 0x13EF message handler toggles bit 0x40 at offset 0x4A.
typedef struct ShelterR47Object {
    /* 0x000 */ u8 pad_0[0x4A];
    /* 0x04A */ u8 field_4A;
    /* 0x04B */ u8 pad_4B[0x261];
} ShelterR47Object;
STATIC_ASSERT_SIZEOF(ShelterR47Object, 0x2AC);

/// Scratch state of the room's second cap script: the task family whose state
/// table is `D_shelter_r47_8017D7DC` (dispatcher `func_shelter_r47_80185214`).
/// `memCalloc(0x30)` in its state-0 entry `func_shelter_r47_8018431C`, stored
/// at `Task::work`.
typedef struct {
    /* 0x00 */ u8           pad_0[4];
    /* 0x04 */ RoomHotspot* hotspots; ///< table hit-tested against the action cursor
    /* 0x08 */ u8           pad_8[2];
    /* 0x0A */ s16          field_A;  ///< width of the first quad drawn by `func_shelter_r47_80183B84`; eases toward `field_E`
    /* 0x0C */ s16          field_C;  ///< height of that quad; eases toward `field_10`
    /* 0x0E */ s16          field_E;
    /* 0x10 */ s16          field_10;
    /* 0x12 */ s16          field_12; ///< width of the second quad drawn by `func_shelter_r47_80183B84`; eases toward `field_16`
    /* 0x14 */ s16          field_14; ///< height of that quad; eases toward `field_18`
    /* 0x16 */ s16          field_16;
    /* 0x18 */ s16          field_18;
    /* 0x1A */ s16          field_1A; ///< id of the confirmed hotspot
    /* 0x1C */ s16          field_1C;
    /* 0x1E */ s16          field_1E; ///< target that `field_20` eases toward by a quarter of the gap a frame
    /* 0x20 */ s16          field_20; ///< x of the sprite drawn by `func_shelter_r47_80183FF4`
    /* 0x22 */ u16          fade;     ///< fade-to-black ramp: +0x10 a frame, clamped at 0xFF
    /* 0x24 */ u8           pad_24[2];
    /* 0x26 */ u16          field_26; ///< frames the first quad has been fully open; zeroed while it grows
    /* 0x28 */ u8           field_28; ///< `promptKind` of the confirmed hotspot
    /* 0x29 */ u8           field_29; ///< low byte of `Mc_SaveData.at4.loc.view` saved on entry
    /* 0x2A */ s8           field_2A;
    /* 0x2B */ s8           field_2B; ///< non-zero holds the prompt off
    /* 0x2C */ s8           field_2C; ///< countdown; a sound plays as it reaches zero
    /* 0x2D */ u8           pad_2D[3];
} ShelterR47State2;
STATIC_ASSERT_SIZEOF(ShelterR47State2, 0x30);

extern s8                     D_80114C12;
extern s32                    D_801350BC;
extern s32                    D_801359D4;
extern s32                    D_8014152C;
extern s32                    D_80141C1C;
extern s32                    D_80141D9C;
extern s32                    D_80142A74;
extern s32                    D_80142C24;
extern s32                    D_801432FC;
extern s32                    D_80143494;
extern TaskDesc               D_shelter_r47_80186F70;
extern TaskDesc               D_shelter_r47_80186F94;
extern TaskDesc               D_shelter_r47_80187020;
extern TaskDesc               D_shelter_r47_80187618;
extern ShelterR47Object       D_shelter_r47_80187960[];
extern ShelterR47Object       D_shelter_r47_80187C0C;
extern Task*                  D_shelter_r47_8018A690;
extern RoomsShared80181228Rec D_shelter_r47_8018A698;

void func_shelter_r47_80183210(void);
void func_shelter_r47_80183B84(Task* task);
void func_shelter_r47_80183E24(void);
void func_shelter_r47_80183F0C(void);
void func_shelter_r47_80183FF4(Task* task, s16 arg1);
void func_shelter_r47_80184124(Task* task, s16 arg1);
void func_shelter_r47_801851B8(Task* task);
s32  func_shelter_r47_8017FE84(s32 arg0, s32 arg1, RoomEventMsg* arg2);

#endif // ROOMS_SHELTER_R47_H
