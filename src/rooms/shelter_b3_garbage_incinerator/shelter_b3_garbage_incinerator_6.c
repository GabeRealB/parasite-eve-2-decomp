#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgte.h>

#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern s32 Gp_LcgState;

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

    ctx                        = task->spawnArg2;
    Gp_State1C->roomEffectMode = 2;
    mode                       = gGameSession->field_132;

    if (mode != 0) {
        if (mode < 2 && (gDisplayState.animFrame & 2) == 0) {
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
            if (gGameSession->field_135 == 1) {
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
            if (gGameSession->field_135 == 1) {
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

void func_shelter_b3_garbage_incinerator_80181FC4(SVECTOR* v, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    head                    = *(void**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = head - 0x10;
    block                   = (RoomDraw13Scratch*)*(void**)G_SCRATCH_HEAD;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1          = ((s16)arg1 * 64) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        ang           = 0;
        packed        = arg2 << 16;
        blend         = (gDisplayState.animFrame & 1) << (packed >> 28);
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend + tr;
        g             = blend + tg;
        b             = blend + ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_6", func_shelter_b3_garbage_incinerator_80182368);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_6", func_shelter_b3_garbage_incinerator_80182AB8);

void func_shelter_b3_garbage_incinerator_80182F18(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*                head;
    RoomDraw19Scratch* block;
    POLY_FT4*          prim;
    u16                col;
    u16                row;
    s32                u0;
    s32                v0;
    s32                ang;
    s32                ang2;
    u16                bank;
    u16                vz;

    bank                                        = arg1 >> 12;
    arg1                                       &= 0xFFF;
    head                                        = *(u8**)G_SCRATCH_HEAD;
    ((RoomDraw19Scratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    *(void**)G_SCRATCH_HEAD                     = head - 0x1C;
    block                                       = *(RoomDraw19Scratch**)G_SCRATCH_HEAD;
    block->vec.vy                               = arg0->workm.t[1];
    block->vec.vz                               = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw19Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw19Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw19Scratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2C;
        prim->clut  = bank ? 0x428F : 0x43D0;
        col         = arg1 % 5;
        row         = arg1 / 5;
        ang         = arg3;
        u0          = col * 0x30;
        v0          = row * 0x30;
        setUV4(prim, u0, v0 - 0x80, u0 + 0x2F, v0 - 0x80, u0, v0 - 0x51, u0 + 0x2F, v0 - 0x51);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(u8**)G_SCRATCH_HEAD += 0x1C;
}

void func_shelter_b3_garbage_incinerator_80183364(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            kind;
    s32            step;
    s32            state;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        Room_Draw40(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->field_28 = step;
            work->field_22 = 0;
            state          = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->field_10.vx | (u16)work->field_10.vy | (u16)work->field_10.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->field_2A = level;
                kind           = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->field_2A = 0;
                        break;
                    case 1:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->field_10.vx = work->field_18;
                        work->field_10.vy = work->field_1A;
                        work->field_10.vz = work->field_1C;
                        break;
                }
                vec = &work->field_10;
                VectorNormalSS(vec, vec);
                gte_lddp(work->field_2A);
                gte_ldsv(vec);
                gte_gpf12_real();
                gte_stsv(vec);
            } else {
                work->field_2A = 0x40;
            }
            return;
        case 1:
            Room_Draw40(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            break;
        case 2:
            Room_Draw41(coord, work->field_20, (s16)work->field_24);
            break;
        default:
            return;
    }
    if ((s16)work->field_2A != 0) {
        coord->coord.t[0] += work->field_10.vx;
        coord->coord.t[1] += work->field_10.vy;
        coord->coord.t[2] += work->field_10.vz;
        coord->flg         = 0;
        work->field_10.vy += 6;
    }
    if (((s16)work->field_22 % (s16)work->field_28) == 0) {
        work->field_20++;
        if ((s16)work->field_20 >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}
