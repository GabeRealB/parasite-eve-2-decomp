#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

/// Cutscene script blob argument of `func_800E8634`.
extern void func_800E8634(s32 arg0, s32 arg1, s32 arg2);

/// Byte at 0x8007272D, written when the garage scene ends.
extern s8 D_8007272D;

/// One entry of the room's 0x98-byte display-object table. Only the flag byte
/// at 0x4A is touched here: bit 6 shows the entry, clearing it hides it.
typedef struct {
    /* 0x00 */ u8 pad_0[0x4A];
    /* 0x4A */ u8 field_4A;
    /* 0x4B */ u8 pad_4B[0x4D];
} DryfieldNightGarageObj;

STATIC_ASSERT_SIZEOF(DryfieldNightGarageObj, 0x98);

/// One entry of the room's vector lists: three coordinates plus padding, eight
/// bytes apart. Only the three coordinates are ever read or written.
typedef struct DryfieldNightGarageVec {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
    /* 0x6 */ s16 pad;
} DryfieldNightGarageVec;
STATIC_ASSERT_SIZEOF(DryfieldNightGarageVec, 0x8);

/// A 12-byte record copied whole, never read field by field.
typedef struct DryfieldNightGarageBlob {
    /* 0x0 */ s8 b[12];
} DryfieldNightGarageBlob;
STATIC_ASSERT_SIZEOF(DryfieldNightGarageBlob, 0xC);

/// A table of pointers into layout data: a four-entry vector list, an
/// eight-entry vector list and four 12-byte records. The room keeps a template
/// and a live copy. Nothing pins where the table ends.
typedef struct DryfieldNightGarageLayout {
    /* 0x0 */ s32                      field_0;
    /* 0x4 */ DryfieldNightGarageVec*  field_4;
    /* 0x8 */ DryfieldNightGarageVec*  field_8;
    /* 0xC */ DryfieldNightGarageBlob* field_C;
} DryfieldNightGarageLayout;

extern TaskDesc               D_8013B11C[];
extern s32                    D_dryfield_night_garage_80182DE0;
extern s32                    D_dryfield_night_garage_80182DE4;
extern TaskDesc               D_dryfield_night_garage_80182C98[];
extern s32                    D_dryfield_night_garage_80182DF8;
extern s32                    D_dryfield_night_garage_801831B8;
extern DryfieldNightGarageObj D_dryfield_night_garage_80186E60[];

/// The layout template and the live copy the reset below restores from it.
extern DryfieldNightGarageLayout D_dryfield_night_garage_80181E40;
extern DryfieldNightGarageLayout D_dryfield_night_garage_80183DD4;

s32 func_800D4D2C(s32 arg0);

s32 func_dryfield_night_garage_80180A64(s32 arg0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage_5", func_dryfield_night_garage_80180414);

/// Resets the live layout lists from the template: the four-entry vector list,
/// the eight-entry list two entries per pass, and the 12-byte records. The
/// eight-entry list is then raised by 0x7D0 on y when `arg0` is nonzero.
void func_dryfield_night_garage_80180604(s32 arg0)
{
    DryfieldNightGarageLayout* dst;
    DryfieldNightGarageLayout* src;
    DryfieldNightGarageVec     d;
    s32                        i;

    dst = &D_dryfield_night_garage_80183DD4;
    src = &D_dryfield_night_garage_80181E40;

    for (i = 0; i < 4; i++) {
        dst->field_4[i].x         = src->field_4[i].x;
        dst->field_4[i].y         = src->field_4[i].y;
        dst->field_4[i].z         = src->field_4[i].z;
        dst->field_8[i * 2].x     = src->field_8[i * 2].x;
        dst->field_8[i * 2].y     = src->field_8[i * 2].y;
        dst->field_8[i * 2].z     = src->field_8[i * 2].z;
        dst->field_8[i * 2 + 1].x = src->field_8[i * 2 + 1].x;
        dst->field_8[i * 2 + 1].y = src->field_8[i * 2 + 1].y;
        dst->field_8[i * 2 + 1].z = src->field_8[i * 2 + 1].z;
        dst->field_C[i]           = src->field_C[i];
    }

    if (arg0 == 0) {
        d.x = 0;
        d.y = 0;
    } else {
        d.x = 0;
        d.y = 0x7D0;
    }
    d.z = 0;

    for (i = 0; i < 8; i++) {
        dst->field_8[i].x += d.x;
        dst->field_8[i].y += d.y;
        dst->field_8[i].z += d.z;
    }
}

void func_dryfield_night_garage_801807E4(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_StartCapSlot((s16)arg0->spawnArg1, 0, 0);
            Gp_DispatchMsg(
                (Task*)func_dryfield_night_garage_80180A64(0), 0x7DB, (s32)&D_dryfield_night_garage_80182DE0, 0);
            goto block_12;
        case 1:
            if (Gp_CapBusy() == 0) {
                func_800D4D2C(0x20);
                goto block_12;
            }
            return;
        case 2:
            Gp_StartCapSlot((s16)arg0->spawnArg1, 0, (s16)(GameFlag_GetNibble(0x107) + 1));
            if (GameFlag_GetNibble(0x107) == 0) {
                GameFlag_SetNibble(0x107, 1);
            }
        block_12:
            arg0->state = arg0->state + 1;
            return;
        case 3:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_MsgPlayerWeapon(1);
            Gp_DispatchMsg(
                (Task*)func_dryfield_night_garage_80180A64(0), 0x7DB, (s32)&D_dryfield_night_garage_80182DE4, 0);
        default:
            taskKill(arg0);
            break;
    }
}
