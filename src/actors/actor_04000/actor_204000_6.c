#include "common.h"
#include "actors/actor_104000.h"
#include "gameplay/3A34.h"

/// Task-like caller whose `field_30` indexes the `Actor04000_D0C6EC` state table.
typedef struct Actor204000Task {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  field_30;
} Actor204000Task;

/// Payload of message 0x7DA, sent to the `gameGetPtrSlot(4)` task.
typedef struct Actor204000Msg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor204000Msg7DA;

extern u8           D_8007216C;
extern Actor104000* Actor04000_D0C710[2];
extern Actor104000* Actor04000_D0C718[6];
extern void         (*Actor04000_D0C6EC[])(Actor204000Task*);

void Actor04000_Fn06EA8(Actor204000Task* arg0)
{
    Actor204000Msg7DA msg;
    s16               i;

    for (i = 0; i < 6; i++) {
        Actor04000_D0C718[i] = NULL;
    }
    Actor04000_D0C710[1] = NULL;
    msg.field_0          = 3;
    msg.field_1          = 0x10;
    Actor04000_D0C710[0] = NULL;
    msg.field_2          = 1;
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
    arg0->field_30++;
}

void Actor04000_Fn06F54(Actor204000Task* arg0)
{
    s16 i;

    if (Gp_StateF0.field_0 == 1) {
        for (i = 0; i < 6; i++) {
            if (Actor04000_D0C718[i] != NULL) {
                Actor04000_D0C718[i]->field_20->field_14 = 0;
            }
        }
        arg0->field_30++;
    }
    if (D_8007216C == 5) {
        for (i = 0; i < 6; i++) {
            if (Actor04000_D0C718[i] != NULL) {
                Gp_ArmStateF0(1);
                return;
            }
        }
    }
}

void Actor04000_Fn0703C(Actor204000Task* arg0)
{
    Actor04000_D0C6EC[arg0->field_30](arg0);
}
