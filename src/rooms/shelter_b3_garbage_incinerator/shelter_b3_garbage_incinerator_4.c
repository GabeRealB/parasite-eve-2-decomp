#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/session.h"
#include "main/task.h"

typedef struct {
    /* 0x00 */ byte pad_0[0x24];
    /* 0x24 */ u16  field_24;
} GarbageIncineratorDraw;

extern SVECTOR D_shelter_b3_garbage_incinerator_80187544[];
extern SVECTOR D_shelter_b3_garbage_incinerator_8018754C[];
extern SVECTOR D_shelter_b3_garbage_incinerator_80187554[];
extern SVECTOR D_shelter_b3_garbage_incinerator_80187564[];
extern SVECTOR D_shelter_b3_garbage_incinerator_80187574[];
extern SVECTOR D_shelter_b3_garbage_incinerator_8018759C[];
extern SVECTOR D_shelter_b3_garbage_incinerator_801875AC[];
extern SVECTOR D_shelter_b3_garbage_incinerator_801875B4[];
extern SVECTOR D_shelter_b3_garbage_incinerator_80187614[];

void func_shelter_b3_garbage_incinerator_80181FC4(SVECTOR* v, s32 arg1, s32 arg2);
void func_shelter_b3_garbage_incinerator_80183E78(SVECTOR* v, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b3_garbage_incinerator_801842A4(SVECTOR* v, s32 arg1, s32 arg2, s32 arg3);

void func_shelter_b3_garbage_incinerator_8018110C(Task* task)
{
    GarbageIncineratorDraw* ctx;
    u32                     mode;
    u8                      view;

    ctx                 = task->spawnArg2;
    Gp_State1C->field_A = 2;
    mode                = Game_Session->field_132;

    if (mode != 0) {
        if (mode < 2 && (Display_State.field_8 & 2) == 0) {
            ctx->field_24 = 0x3C40;
        } else {
            ctx->field_24 = 0x304C;
        }
    } else {
        ctx->field_24 = 0x3C40;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 0x02:
        case 0x16:
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018759C[0], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018759C[1], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018759C[5], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[16], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[17], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[24], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[25], 0x300, 0x400, 0x40);
            break;
        case 0x03:
        case 0x17:
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018759C[0], 0x200, 0x5200);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018759C[1], 0x200, 0x5300);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018759C[2], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018759C[5], 0x200, 0x5200);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018759C[6], 0x200, 0x5300);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018759C[7], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[16], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[17], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[18], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[19], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[20], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[24], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[25], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[26], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[27], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[28], 0x300, 0x400, 0x40);
            break;
        case 0x04:
        case 0x18:
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875AC[0], 0x200, 0x5200);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875AC[1], 0x200, 0x5200);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875AC[2], 0x200, 0x5200);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875AC[6], 0x200, 0x5300);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875AC[7], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[18], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[19], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[20], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[21], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[28], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[29], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[30], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[31], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[32], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[33], 0x300, 0x400, 0x40);
            break;
        case 0x05:
        case 0x19:
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875B4[0], 0x200, 0x5100);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875B4[1], 0x200, 0x5100);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875B4[5], 0x200, 0x5200);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875B4[6], 0x200, 0x5300);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875B4[7], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875B4[10], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875B4[19], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875B4[20], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875B4[28], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875B4[29], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875B4[30], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875B4[31], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875B4[32], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875B4[33], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875B4[34], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875B4[49], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875B4[50], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875B4[51], 0x300, 0x400, 0x40);
            break;
        case 0x06:
        case 0x1A:
        case 0x23:
            func_shelter_b3_garbage_incinerator_801842A4(&D_shelter_b3_garbage_incinerator_80187544[0], 0x200, ctx->field_24, 0x80);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[9], 0x200, 0x3444);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[10], 0x200, 0x3444);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[19], 0x200, 0x5100);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[20], 0x200, 0x5200);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[21], 0x200, 0x5300);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[22], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[24], 0x200, 0x5300);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[25], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[42], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[45], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[46], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[47], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[48], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[49], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[50], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[51], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[63], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[64], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[65], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[66], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[67], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[68], 0x300, 0x400, 0x40);
            break;
        case 0x07:
        case 0x1B:
        case 0x24:
            func_shelter_b3_garbage_incinerator_801842A4(&D_shelter_b3_garbage_incinerator_80187544[0], 0x200, ctx->field_24, 0x80);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[9], 0x200, 0x3444);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[10], 0x200, 0x3444);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[20], 0x200, 0x5100);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[21], 0x200, 0x5200);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[22], 0x200, 0x5300);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[24], 0x200, 0x5200);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[25], 0x200, 0x5300);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[47], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[48], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[49], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[50], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[51], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[52], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[53], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[54], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[55], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[64], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[65], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[66], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[67], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[68], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[69], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[70], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[71], 0x300, 0x400, 0x40);
            break;
        case 0x08:
        case 0x1C:
        case 0x22:
        case 0x25:
            func_shelter_b3_garbage_incinerator_801842A4(&D_shelter_b3_garbage_incinerator_80187544[0], 0x200, ctx->field_24, 0x80);
            func_shelter_b3_garbage_incinerator_801842A4(&D_shelter_b3_garbage_incinerator_80187544[1], 0x200, ctx->field_24, 0x80);
            if (Game_Session->field_135 == 1) {
                func_shelter_b3_garbage_incinerator_801842A4(&D_shelter_b3_garbage_incinerator_80187544[3], 0x200, 0xF63, 0x10C0);
            }
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[20], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[21], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[22], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[24], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[25], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[49], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[50], 0x300, 0x0, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[51], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[52], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[53], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[54], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[55], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[56], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[57], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[67], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[68], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[69], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[70], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[71], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[72], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[73], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187544[74], 0x300, 0x400, 0x40);
            break;
        case 0x09:
        case 0x1D:
            func_shelter_b3_garbage_incinerator_801842A4(&D_shelter_b3_garbage_incinerator_8018754C[0], 0x200, ctx->field_24, 0x80);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018754C[5], 0x280, 0x3040);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018754C[6], 0x200, 0x3444);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018754C[7], 0x200, 0x3444);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018754C[22], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018754C[25], 0x200, 0x5400);
            break;
        case 0x0A:
            func_shelter_b3_garbage_incinerator_801842A4(&D_shelter_b3_garbage_incinerator_80187554[0], 0x180, 0x3F6, 0xC0);
            /* fallthrough */
        case 0x1E:
        case 0x26:
            func_shelter_b3_garbage_incinerator_801842A4(&D_shelter_b3_garbage_incinerator_80187544[0], 0x200, ctx->field_24, 0x80);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[9], 0x200, 0x3444);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[10], 0x200, 0x3444);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[22], 0x200, 0x5400);
            break;
        case 0x0B:
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187564[0], 0x280, 0x44);
            break;
        case 0x0C:
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875AC[0], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875AC[1], 0x200, 0x5300);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875AC[2], 0x200, 0x5200);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875AC[5], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875AC[6], 0x200, 0x5100);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_801875AC[7], 0x200, 0x5100);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[20], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[21], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[28], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[29], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[30], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[31], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[32], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[33], 0x300, 0x200, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[34], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[50], 0x300, 0x100, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_801875AC[51], 0x300, 0x100, 0x40);
            break;
        case 0x0E:
            func_shelter_b3_garbage_incinerator_801842A4(&D_shelter_b3_garbage_incinerator_80187544[0], 0x200, ctx->field_24, 0x80);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[9], 0x200, 0x3444);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[10], 0x200, 0x3444);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[22], 0x200, 0x5400);
            break;
        case 0x0F:
        case 0x1F:
        case 0x27:
            func_shelter_b3_garbage_incinerator_801842A4(&D_shelter_b3_garbage_incinerator_8018754C[0], 0x200, ctx->field_24, 0x80);
            if (Game_Session->field_135 == 1) {
                func_shelter_b3_garbage_incinerator_801842A4(&D_shelter_b3_garbage_incinerator_8018754C[2], 0x200, 0xF63, 0x10C0);
            }
            break;
        case 0x10:
        case 0x20:
            func_shelter_b3_garbage_incinerator_801842A4(&D_shelter_b3_garbage_incinerator_80187544[0], 0x200, ctx->field_24, 0x80);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[9], 0x200, 0x3333);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[10], 0x200, 0x3333);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[20], 0x100, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[21], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[22], 0x300, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[24], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187544[25], 0x300, 0x5400);
            break;
        case 0x11:
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018759C[0], 0x200, 0x5300);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018759C[1], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_8018759C[5], 0x200, 0x5300);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[16], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[17], 0x300, 0x300, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[24], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_8018759C[25], 0x300, 0x300, 0x40);
            break;
        case 0x12:
        case 0x21:
            func_shelter_b3_garbage_incinerator_801842A4(&D_shelter_b3_garbage_incinerator_80187544[0], 0x200, ctx->field_24, 0x80);
            break;
        case 0x13:
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187614[0], 0x200, 0x3400);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187614[35], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187614[36], 0x300, 0x400, 0x40);
            break;
        case 0x15:
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187574[0], 0x280, 0x3400);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187574[1], 0x200, 0x3444);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187574[2], 0x200, 0x3444);
            func_shelter_b3_garbage_incinerator_80181FC4(&D_shelter_b3_garbage_incinerator_80187574[20], 0x200, 0x5400);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187574[56], 0x300, 0x400, 0x40);
            func_shelter_b3_garbage_incinerator_80183E78(&D_shelter_b3_garbage_incinerator_80187574[72], 0x300, 0x400, 0x40);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80181FC4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80182368);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80182AB8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80182F18);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_4", func_shelter_b3_garbage_incinerator_80183364);
