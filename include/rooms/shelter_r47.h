#ifndef ROOMS_SHELTER_R47_H
#define ROOMS_SHELTER_R47_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// Sparse view of the adjacent records at 0x80187960 and 0x80187C0C.
/// The room's 0x13EF message handler toggles bit 0x40 at offset 0x4A.
typedef struct ShelterR47Object {
    /* 0x000 */ u8 pad_0[0x4A];
    /* 0x04A */ u8 field_4A;
    /* 0x04B */ u8 pad_4B[0x261];
} ShelterR47Object;
STATIC_ASSERT_SIZEOF(ShelterR47Object, 0x2AC);

extern s32              D_801350BC;
extern s32              D_801359D4;
extern s32              D_8014152C;
extern s32              D_80141C1C;
extern s32              D_80143494;
extern TaskDesc         D_shelter_r47_80186F70;
extern TaskDesc         D_shelter_r47_80187020;
extern TaskDesc         D_shelter_r47_80187618;
extern ShelterR47Object D_shelter_r47_80187960[];
extern Task*            D_shelter_r47_8018A690;

s32 func_shelter_r47_8017FE84(s32 arg0, s32 arg1, RoomEventMsg* arg2);

#endif // ROOMS_SHELTER_R47_H
