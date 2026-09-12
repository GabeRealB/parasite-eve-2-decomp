#include "common.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/pad.h"
#include "main/stage.h"
#include "gameplay/3CD8.h"
#include "main/display.h"

typedef struct {
    u8  field_0;
    u8  field_1;
    u8  _pad2[0x2];
    u8  field_4;
    u8  _pad5[0x3];
    s32 field_8;
} DumpingHoleSpawnElem;

extern DumpingHoleSpawnElem* D_shelter_b3_dumping_hole_8018F4BC;
extern s16                   D_shelter_b3_dumping_hole_8018F4C6;

extern void func_shelter_b3_dumping_hole_8017FD9C(s32 arg0, s32 arg1);
extern void func_shelter_b3_dumping_hole_8017FE10(void);
extern void func_shelter_b3_dumping_hole_80181C8C(void);
void        func_shelter_b3_dumping_hole_80181F80(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void        func_shelter_b3_dumping_hole_80182AA0(void);

typedef struct {
    u8  pad_00[0x2A];
    u16 field_2A;
    u8  pad_2C[0x4];
    s32 field_30;
    s32 field_34;
} Sub81A48;

void func_shelter_b3_dumping_hole_80181A48(Task* arg0)
{
    Sub81A48* s = (Sub81A48*)arg0;

    switch (s->field_30) {
        case 0:
            s->field_34  = 3;
            s->field_2A  = 8;
            s->field_30 += 1;
            break;
        case 1:
            if ((s16)(s->field_2A -= 1) < 0) {
                s->field_30 += 1;
            }
            Display_ClampField126(s->field_34);
            s->field_34 = -s->field_34;
            break;
        default:
            Display_ClampField126(0);
            Task_Kill(arg0);
            break;
    }
}

void func_shelter_b3_dumping_hole_80181B04(s16 arg0)
{
    func_shelter_b3_dumping_hole_8017FD9C(
        (s32) & ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8[1], arg0);
}

void func_shelter_b3_dumping_hole_80181B44(void)
{
    func_shelter_b3_dumping_hole_8017FE10();
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", func_shelter_b3_dumping_hole_80181B64);

void func_shelter_b3_dumping_hole_80181C8C(void)
{
    if (D_shelter_b3_dumping_hole_8018F4BC == NULL) {
        return;
    }
    if (D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8 == -1) {
        return;
    }
    if (Gp_CapBusy() != 0) {
        return;
    }
    func_shelter_b3_dumping_hole_80181F80(
        D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8, 0x80, 1,
        D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_0 |
            ((D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_1 & 0x10)
             << 4));
    if (D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_4 & 1) {
        return;
    }
    func_shelter_b3_dumping_hole_80182AA0();
}

typedef struct {
    /* 0x00 */ char magic[0x8];
    /* 0x08 */ s32  field_8;
    /* 0x0C */ s32  field_C;
    /* 0x10 */ s32  field_10;
} Reloc80181D68Hdr;

typedef struct {
    /* 0x00 */ u8  pad[0x8];
    /* 0x08 */ s32 field_8;
} Reloc80181D68Entry1;

typedef struct {
    /* 0x00 */ s16                 count;
    /* 0x02 */ u8                  pad[0xE];
    /* 0x10 */ Reloc80181D68Entry1 entries[1];
} Reloc80181D68Table1;

typedef struct {
    /* 0x00 */ s32 count;
    /* 0x04 */ s32 entries[1];
} Reloc80181D68Table2;

extern char D_shelter_b3_dumping_hole_8017D650[];
extern s32  D_shelter_b3_dumping_hole_8018F4B8;
extern s32  D_shelter_b3_dumping_hole_8018F4B4;

s32 func_shelter_b3_dumping_hole_80181D68(s32 arg0)
{
    Reloc80181D68Hdr*    hdr = (Reloc80181D68Hdr*)arg0;
    Reloc80181D68Entry1* r;
    s32*                 q;
    s32                  n1;
    s32                  n2;
    s32                  i;

    if (strncmp((char*)hdr, D_shelter_b3_dumping_hole_8017D650, 3) != 0) {
        return 0;
    }
    if (hdr->field_8 > 0) {
        hdr->field_8  += (s32)hdr;
        hdr->field_C  += (s32)hdr;
        hdr->field_10 += (s32)hdr;
        n1             = ((Reloc80181D68Table1*)hdr->field_C)->count;
        r              = &((Reloc80181D68Table1*)hdr->field_C)->entries[0];
        for (i = 0; i < n1; i++) {
            if (r->field_8 != -1) {
                r->field_8 += (s32)hdr;
            } else {
                r++;
            }
            r++;
        }
        n2 = ((Reloc80181D68Table2*)hdr->field_10)->count;
        q  = &((Reloc80181D68Table2*)hdr->field_10)->entries[0];
        for (i = 0; i < n2; i++) {
            if (*q != 0) {
                *q += (s32)hdr;
            }
            q++;
        }
    }
    D_shelter_b3_dumping_hole_8018F4B8 = hdr->field_8;
    D_shelter_b3_dumping_hole_8018F4B4 = hdr->field_10 + 4;
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", RoomsShared801830f0Sub);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", func_shelter_b3_dumping_hole_80181F80);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", func_shelter_b3_dumping_hole_801829B4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", func_shelter_b3_dumping_hole_80182AA0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", func_shelter_b3_dumping_hole_80182C24);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", func_shelter_b3_dumping_hole_80182D34);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", func_shelter_b3_dumping_hole_80182E50);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", func_shelter_b3_dumping_hole_80182F18);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", func_shelter_b3_dumping_hole_80182FD0);

void func_shelter_b3_dumping_hole_80183024(Task* arg0)
{
    if ((arg0->spawnArg1 -= 1) <= 0) {
        Task_Kill(arg0);
    }
    func_shelter_b3_dumping_hole_80181C8C();
}

void func_shelter_b3_dumping_hole_80183060(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            arg0->state = 1;
            break;
        case 1:
            arg0->spawnArg1 -= 1;
            if (arg0->spawnArg1 <= 0 || Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
                Task_Kill(arg0);
                Stage_SetEndingFlag();
            }
            break;
    }
    func_shelter_b3_dumping_hole_80181C8C();
}
