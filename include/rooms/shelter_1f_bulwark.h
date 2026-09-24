#ifndef ROOMS_SHELTER_1F_BULWARK_H
#define ROOMS_SHELTER_1F_BULWARK_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1A8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"

/// Event parameters copied to the room's pending event. The controller
/// runs the cap command in field_0 and the stage sound in field_4;
/// field_8 is the game flag checked and set when the event starts.
typedef struct BulwarkEvent {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s16 field_8;
    /* 0xA */ u8  field_A;
} BulwarkEvent;
STATIC_ASSERT_SIZEOF(BulwarkEvent, 0xC);

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

void func_shelter_1f_bulwark_8017DF00(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_1f_bulwark_8017E630(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_1f_bulwark_8017EA5C(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_shelter_1f_bulwark_8017F960(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

extern TaskDesc     D_shelter_1f_bulwark_80180320;
extern TaskDesc     D_shelter_1f_bulwark_80180354;
extern TaskDesc     D_shelter_1f_bulwark_80180360;
extern GpStateBD8   D_shelter_1f_bulwark_80180EC0;
extern GpSaveLoc    D_shelter_1f_bulwark_80180EC4;
extern s8           D_shelter_1f_bulwark_80180ECC;
extern BulwarkEvent D_shelter_1f_bulwark_80180ED0;

#endif // ROOMS_SHELTER_1F_BULWARK_H
