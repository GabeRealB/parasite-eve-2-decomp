#ifndef ROOMS_SHELTER_B1_STOREROOM_H
#define ROOMS_SHELTER_B1_STOREROOM_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017e4f8.h"

extern const TaskFuncTable3     D_shelter_b1_storeroom_8017D5C4;
extern s32                      D_shelter_b1_storeroom_80184968;
extern RoomsShared8017e4f8Shade D_shelter_b1_storeroom_80184B20[];
extern SVECTOR                  D_shelter_b1_storeroom_80184B34[];
extern SVECTOR                  D_shelter_b1_storeroom_80184B3C;

void func_shelter_b1_storeroom_8017D740(Task* task);
void func_shelter_b1_storeroom_8017D794(Task* task);
void func_shelter_b1_storeroom_8017DBC4(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b1_storeroom_8017E408(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b1_storeroom_8017E7A8(Task* task);
void func_shelter_b1_storeroom_8017F15C(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_shelter_b1_storeroom_8017F4F0(Task* task);
void func_shelter_b1_storeroom_8017F888(Task* task);
void func_shelter_b1_storeroom_8017FA34(GsCOORDINATE2* coord, s16 frame);
void func_shelter_b1_storeroom_801802D8(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_shelter_b1_storeroom_80180C98(Task* task);
void func_shelter_b1_storeroom_80180DCC(Task* task);
void func_shelter_b1_storeroom_8018149C(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_shelter_b1_storeroom_80181830(Task* task);
void func_shelter_b1_storeroom_80181D20(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);
void func_shelter_b1_storeroom_80182118(Task* task);
void func_shelter_b1_storeroom_801823A0(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_shelter_b1_storeroom_801832B8(Task* task);
void func_shelter_b1_storeroom_801834DC(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b1_storeroom_80183760(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* arg3);
void func_shelter_b1_storeroom_80183B84(GsCOORDINATE2* arg0, s32 arg1, u8* arg2);

#endif // ROOMS_SHELTER_B1_STOREROOM_H
