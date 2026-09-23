#include "common.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/rooms_shared_8017e690.h"

extern void func_shelter_b4_reservoir_8017E068(void);
void        func_shelter_b4_reservoir_8017E8E4(void);
void        func_shelter_b4_reservoir_8017E8EC(Task* task);

extern s16 D_shelter_b4_reservoir_80184F80;

typedef struct {
    u8  _pad[6];
    s16 field_6;
} _Unk80184F90;

extern s8           D_8007217B;
extern s16          D_shelter_b4_reservoir_80184F82;
extern _Unk80184F90 D_shelter_b4_reservoir_80184F90;
extern u8*          D_shelter_b4_reservoir_80187630;

void func_shelter_b4_reservoir_8017EA00(Task* task);
void func_shelter_b4_reservoir_8017EE04(Task* task);
void func_shelter_b4_reservoir_8017F23C(Task* task);
void func_shelter_b4_reservoir_8017F674(Task* task);

void func_shelter_b4_reservoir_8017E8E4(void)
{
}

void func_shelter_b4_reservoir_8017E8EC(Task* task)
{
    _Unk80184F90* p = &D_shelter_b4_reservoir_80184F90;

    if (D_8007217B == 0) {
        D_shelter_b4_reservoir_80187630 = (u8*)D_8005C374 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_shelter_b4_reservoir_80187630 = (u8*)D_8005C370 + gDisplayState.otBuffer * 0xC000;
    }
    if (gGameSession->at4.loc.view != 0xA) {
        p->field_6 = 0x2328 - (((D_shelter_b4_reservoir_80184F80 + 0x7D0) * 0x31) >> 5);
        func_shelter_b4_reservoir_8017EA00(task);
        func_shelter_b4_reservoir_8017EE04(task);
        func_shelter_b4_reservoir_8017F23C(task);
        return;
    }
    if (D_shelter_b4_reservoir_80184F82 < -0x708) {
        D_shelter_b4_reservoir_80184F82 = -0x708;
    } else if (D_shelter_b4_reservoir_80184F82 > 0) {
        D_shelter_b4_reservoir_80184F82 = 0;
    }
    func_shelter_b4_reservoir_8017F674(task);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_3", func_shelter_b4_reservoir_8017EA00);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_3", func_shelter_b4_reservoir_8017EE04);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_3", func_shelter_b4_reservoir_8017F23C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_3", func_shelter_b4_reservoir_8017F674);

void func_shelter_b4_reservoir_8017FADC(Task* task)
{
    TaskFunc states[2] = { RoomsShared8017e690, func_shelter_b4_reservoir_8017E8EC };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b4_reservoir_80184F80;
}
