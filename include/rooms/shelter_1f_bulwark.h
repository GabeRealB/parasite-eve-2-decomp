#ifndef ROOMS_SHELTER_1F_BULWARK_H
#define ROOMS_SHELTER_1F_BULWARK_H

#include "common.h"

#include "gameplay/1A8.h"
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

extern TaskDesc     D_shelter_1f_bulwark_80180320;
extern TaskDesc     D_shelter_1f_bulwark_80180354;
extern GpSaveLoc    D_shelter_1f_bulwark_80180EC4;
extern s8           D_shelter_1f_bulwark_80180ECC;
extern BulwarkEvent D_shelter_1f_bulwark_80180ED0;

#endif // ROOMS_SHELTER_1F_BULWARK_H
