#ifndef SHELTER_R47_PRIVATE_H
#define SHELTER_R47_PRIVATE_H

#include "main/ui_types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

typedef struct {
    u16 clutX;
    u16 clutY;
    s16 x;
    s16 y;
    u8  u;
    u8  v;
    u8  w;
    u8  h;
} ShelterR47SpritePart;

extern ShelterR47SpritePart* D_shelter_r47_8018729C[];

extern SVECTOR D_shelter_r47_80187624[10];

// Callbacks referenced by the overlay's shared data tables.
void func_shelter_r47_80183234(Task *);
void func_shelter_r47_80185214(Task *);
void func_shelter_r47_8018580C(Task *);

// Callbacks referenced by the overlay's shared data tables.
void func_shelter_r47_8017D86C(UiList *, UiObject *);
void func_shelter_r47_8017E038(UiList *, UiObject *);
void func_shelter_r47_8017EA50(Task *);
void func_shelter_r47_8017EEFC(Task *);
void func_shelter_r47_8017F0BC(Task *);
void func_shelter_r47_8017F2B0(UiList *, UiObject *);
void func_shelter_r47_8017F394(UiList *, UiObject *);
void func_shelter_r47_8017F45C(UiList *, UiObject *);
void func_shelter_r47_8017F524(UiList *, UiObject *);
void func_shelter_r47_8017F628(Task *);
s32 func_shelter_r47_8017FE84(Task *, s32, RoomEventMsg *, GpMessageArg);
s32 func_shelter_r47_801801DC(Task *, s32, s32, GpMessageArg);
void func_shelter_r47_80180324(Task *);
void func_shelter_r47_80180540(Task *);
s32 func_shelter_r47_801805D0(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_shelter_r47_801805D8(Task *, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_shelter_r47_8018061C(Task *, s32, s32, GpMessageArg);
void func_shelter_r47_80180650(Task *);
void func_shelter_r47_80180714(Task *);
void func_shelter_r47_8018080C(Task *);
void func_shelter_r47_801808D4(Task *);
void func_shelter_r47_80182B18(Task *);

#endif // SHELTER_R47_PRIVATE_H
