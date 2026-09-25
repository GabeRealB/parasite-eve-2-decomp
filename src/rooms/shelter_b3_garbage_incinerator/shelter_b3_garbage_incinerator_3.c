#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b3_garbage_incinerator.h"

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
void func_shelter_b3_garbage_incinerator_801842A4(SVECTOR* v, u16 arg1, u16 arg2, u16 arg3);
void func_shelter_b3_garbage_incinerator_80182AB8(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_b3_garbage_incinerator_80182F18(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_b3_garbage_incinerator_801837F8(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_b3_garbage_incinerator_80183BE4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);

extern s8       D_8007218A;
extern u8       D_80073BA9;
extern TaskDesc D_80164190;

/// Four halfwords per entry, read as the two floor-level end points of a wall
/// edge: `[0]`/`[1]` for the first corner and `[2]`/`[3]` for the second.
extern u16 D_shelter_b3_garbage_incinerator_8018FBFC[][4];

/// Same layout as `D_shelter_b3_garbage_incinerator_8018FBFC`, for the wall
/// edges built by `func_shelter_b3_garbage_incinerator_8018507C`.
extern u16 D_shelter_b3_garbage_incinerator_8018FBCC[][4];

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

    head               = SCRATCH_HEAD(void);
    SCRATCH_HEAD(void) = head - 0x10;
    block              = SCRATCH_HEAD(RoomDraw13Scratch);

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtps();
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
    SCRATCH_POP_BYTES(0x10);
}

void func_shelter_b3_garbage_incinerator_80182368(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            step;
    s32            level;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (task->spawnArg1 < 0) {
            func_shelter_b3_garbage_incinerator_80182F18(coord, work->index | work->pos.vx, work->scale, work->angle);
        } else {
            func_shelter_b3_garbage_incinerator_80182AB8(coord, work->index | work->pos.vx, work->scale, work->angle);
        }
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1 & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 7;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            task->state  = 1;
            task->state  = task->spawnArg1 < 0 ? 2 : 1;
            work->pos.vx = (task->spawnArg1 >> 16) & 0x7000;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                switch ((task->spawnArg1 >> 24) & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                    case 6:
                        work->move.vy = 0;
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 7:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = ((u32)Gp_LcgState >> 16) & 0xFF;
                        gte_SetRotMatrix(&work->parent->coord);
                        gte_ldv0(&work->move);
                        gte_rtv0();
                        gte_stsv(&work->move);
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            break;
        case 1:
            func_shelter_b3_garbage_incinerator_80182AB8(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0] += work->move.vx;
                coord->coord.t[1] += work->move.vy;
                coord->coord.t[2] += work->move.vz;
                coord->flg         = 0;
                if (((task->spawnArg1 >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 2;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 12) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
        case 2:
            func_shelter_b3_garbage_incinerator_80182F18(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0] += work->move.vx;
                coord->coord.t[1] += work->move.vy;
                coord->coord.t[2] += work->move.vz;
                coord->flg         = 0;
                if (((task->spawnArg1 >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 1;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 10) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

void func_shelter_b3_garbage_incinerator_80182AB8(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    u16              col;
    u16              row;
    s32              u0;
    s32              v0;
    s32              ang;
    s32              ang2;
    u16              bank;
    u32              idx;

    head                                      = SCRATCH_HEAD(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    SCRATCH_HEAD(void)                        = head - 0x1C;
    block                                     = SCRATCH_HEAD(GpFxQuadScratch);
    block->vec.vy                             = arg0->workm.t[1];
    block->vec.vz                             = arg0->workm.t[2];
    idx                                       = arg1;
    idx                                      &= 0xFFF;
    bank                                      = arg1 >> 12;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        if (bank >= 2) {
            prim->clut = 0x428F;
        } else {
            prim->clut = ((bank + 0x10E) << 6) | (idx & 0x3F);
        }
        col = (u16)idx % 5;
        row = (u16)idx / 5;
        ang = arg3;
        u0  = col * 0x30;
        v0  = row * 0x30;
        setUV4(prim, u0, v0 + 0x70, u0 + 0x2F, v0 + 0x70, u0, v0 - 0x61, u0 + 0x2F, v0 - 0x61);
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
    SCRATCH_POP_BYTES(0x1C);
}

void func_shelter_b3_garbage_incinerator_80182F18(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    u16              col;
    u16              row;
    s32              u0;
    s32              v0;
    s32              ang;
    s32              ang2;
    u16              bank;
    u16              vz;

    bank                                      = arg1 >> 12;
    arg1                                     &= 0xFFF;
    head                                      = SCRATCH_HEAD(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    SCRATCH_HEAD(void)                        = head - 0x1C;
    block                                     = SCRATCH_HEAD(GpFxQuadScratch);
    block->vec.vy                             = arg0->workm.t[1];
    block->vec.vz                             = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
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
    SCRATCH_POP_BYTES(0x1C);
}

void func_shelter_b3_garbage_incinerator_80183364(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            kind;
    s32            step;
    s32            state;
    s32            level;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_b3_garbage_incinerator_801837F8(coord, work->index, work->scale, work->angle);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            state        = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->move.vx | (u16)work->move.vy | (u16)work->move.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                kind       = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            return;
        case 1:
            func_shelter_b3_garbage_incinerator_801837F8(coord, work->index, work->scale, work->angle);
            break;
        case 2:
            func_shelter_b3_garbage_incinerator_80183BE4(coord, (u16)work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0] += work->move.vx;
        coord->coord.t[1] += work->move.vy;
        coord->coord.t[2] += work->move.vz;
        coord->flg         = 0;
        work->move.vy     += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a spinning textured sprite at the world position of `arg0`,
/// projected through `GsWSMATRIX`: one semi-transparent `POLY_FT4` whose
/// corners lie `(s16)arg2 * 31` over the depth from the centre, at the angle
/// `arg3` and a quarter turn past it. `arg1` picks the frame, a 32x32 cell
/// in a row of the texture page. Nothing is drawn when the projection flags
/// an error.
void func_shelter_b3_garbage_incinerator_801837F8(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s32              v;
    s32              ang;
    s32              ang2;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = *(u16*)&arg0->workm.t[1];
    vz                                        = *(u16*)&arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ang            = arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2C;
        prim->clut  = 0x43D3;
        u0          = arg1 << 5;
        v           = 0xE0;
        u1          = u0 + 0x1F;
        setUV4(prim, u0, v, u1, v, u0, 0xFF, u1, 0xFF);
        block->dx = (((arg2 * 31) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        ang2      = ang + 0x400;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        block->dx = (((arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x1C);
}

/// Draws a textured billboard at the world position of `arg0`, projected
/// through `GsWSMATRIX`: one semi-transparent axis-aligned `POLY_FT4`, a
/// square of half-side `(s16)arg2 * 55` over the depth, raised so the
/// projected point sits three quarters of the way down it. `arg1` picks the
/// frame, a 56x56 cell in a four-by-two grid of the texture page. Nothing is
/// drawn when the projection flags an error.
void func_shelter_b3_garbage_incinerator_80183BE4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    DisplayState*  ds;
    s32            tex;
    u32            cell;
    s32            u1;
    s32            v0;
    s32            v1;
    s32            sarg;
    s32            t;
    s16            xy;
    u16            vz;

    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = *(u16*)&arg0->workm.t[1];
    vz                                      = *(u16*)&arg0->workm.t[2];
    tex                                     = arg1;
    SOFT_TOUCH_REG(tex);
    *scratch      = block;
    block->vec.vz = vz;
    vec           = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        cell        = (u16)tex;
        prim->clut  = 0x4393;
        tex         = (cell & 3) * 0x38;
        SOFT_BARRIER();
        v0          = ((cell & 7) >> 2) * 0x38;
        u1          = tex + 0x37;
        prim->v0    = v0;
        prim->v1    = v0;
        v1          = v0 + 0x37;
        prim->u1    = u1;
        prim->u3    = u1;
        sarg        = (s16)arg2;
        prim->v2    = v1;
        prim->v3    = v1;
        t           = sarg * 0x38;
        prim->u0    = tex;
        prim->u2    = tex;
        block->step = (t - sarg) / block->otz;
        xy          = *(u16*)&block->sx - *(u16*)&block->step;
        prim->x0 = prim->x2 = xy;
        xy                  = *(u16*)&block->sx + *(u16*)&block->step;
        prim->x1 = prim->x3 = xy;
        xy                  = (*(u16*)&block->sy - *(u16*)&block->step) - (block->step >> 1);
        ds                  = &gDisplayState;
        prim->y0 = prim->y1 = xy;
        xy                  = *(u16*)&block->sy + (block->step >> 1);
        prim->y2 = prim->y3 = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}

void func_shelter_b3_garbage_incinerator_80183E78(SVECTOR* v, s32 arg1, s32 arg2, s32 arg3)
{
    u8*                head;
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    u8                 lvl;
    u8                 r;
    u8                 g;
    u8                 b;

    head               = SCRATCH_HEAD(void);
    SCRATCH_HEAD(void) = head - 0x10;
    block              = SCRATCH_HEAD(RoomDraw13Scratch);

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        lvl           = rsin(gDisplayState.animFrame * (s16)arg3) / 34 + 0x78;
        arg1          = ((s16)arg1 * 64) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        r             = lvl * (((s16)arg2 >> 8) & 0xF) / 15;
        g             = lvl * (((s16)arg2 >> 4) & 0xF) / 15;
        b             = lvl * (arg2 & 0xF) / 15;
        ang           = 0;
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
    SCRATCH_POP_BYTES(0x10);
}

void func_shelter_b3_garbage_incinerator_801842A4(SVECTOR* arg0, u16 arg1, u16 arg2, u16 arg3)
{
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                ua;
    s32                ub;
    s32                uc;
    u16                frame;
    u16                start;
    s32                blend;
    u32                packed;
    u8                 lvl;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        tmp     = SCRATCH_PUSH_BYTES_AT(scratch, 0x14);
        block   = (RoomDraw05Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        frame = gDisplayState.animFrame;
        if (arg3 & 0x1000) {
            start = (frame << 7) & 0xF80;
        } else {
            start = 0;
        }
        arg3 &= 0xFFF;
        if (arg2 & 0xF000) {
            blend   = frame & 1;
            packed  = arg2;
            blend <<= packed >> 12;
            r       = blend + ((packed >> 4) & 0xF0);
            g       = blend + (packed & 0xF0);
            b       = blend + ((arg2 & 0xF) << 4);
        } else {
            lvl = rsin((frame * arg3) & 0xFFF) / 68 - 0x4C;
            r   = lvl * ((arg2 >> 8) & 0xF) / 15;
            g   = lvl * ((arg2 >> 4) & 0xF) / 15;
            b   = lvl * (arg2 & 0xF) / 15;
        }
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;
        for (ang = start; ang < start + 0x1000; ang = t2) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, (u8)r >> 1, (u8)g >> 1, (u8)b >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }

        r = (u8)r >> 1;
        g = (u8)g >> 1;
        b = (u8)b >> 1;
        for (ang = start + 0x200; ang < start + 0x1000; ang = uc) {
            ua             = ang - 0x400;
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ua)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(ua)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            ub       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ub)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(ub)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ub)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(ub)) >> 11);
            uc       = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(uc)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(uc)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x14);
}

void func_shelter_b3_garbage_incinerator_80184D7C(void)
{
}

void func_shelter_b3_garbage_incinerator_80184D84(Task* arg0)
{
    union {
        s32 msg[5];
        struct {
            u8 param1[8];
            u8 param2[8];
        } cd;
    } buf;
    s32 out;
    s32 v;

    switch (arg0->state) {
        case 0:
            v = D_80073BA9;
            if (D_8007218A == 1) {
                v = v + 1;
            } else {
                v = v + 0x22;
            }
            buf.msg[0] = v;
            buf.msg[1] = 1;
            buf.msg[2] = 0;
            buf.msg[3] = 0;
            buf.msg[4] = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)buf.msg, 0);
            arg0->spawnArg2 = Task_SpawnFromTable(&D_80164190, 0, 0, 0);
            arg0->state++;
            return;
        case 1:
            if (Task_PollKill(arg0->spawnArg2, &out) != 0) {
                arg0->state++;
            }
            return;
        case 2:
            arg0->state = 3;
            return;
        case 3:
            buf.cd.param1[2] = 0x22;
            buf.cd.param1[3] = 0;
            buf.cd.param1[0] = 0;
            buf.cd.param2[0] = 0x16;
            buf.cd.param2[1] = 0;
            buf.cd.param2[2] = 0;
            buf.cd.param2[3] = 0;
            CdCmd_Enqueue(0x21, buf.cd.param1, buf.cd.param2);
            taskKill(arg0);
            break;
    }
}

void func_shelter_b3_garbage_incinerator_80184ECC(Task* task)
{
    taskKill(task);
}

void func_shelter_b3_garbage_incinerator_80184EEC(void)
{
    SVECTOR     normal;
    SVECTOR*    normals;
    SVECTOR*    verts;
    GpGridFace* faces;
    s16         i;
    SVECTOR*    np;

    normals = Gp_GridParams->field_4;
    verts   = Gp_GridParams->field_8;
    faces   = Gp_GridParams->field_C;
    if (gGameSession->at4.loc.place == 2) {
        i = 6;
        do {
            verts[i * 4].vx = verts[i * 4 + 2].vx = 13000;
            verts[i * 4].vy = verts[i * 4 + 2].vy = 1000;
            verts[i * 4].vz = verts[i * 4 + 2].vz = -15000;
            verts[i * 4 + 1].vx = verts[i * 4 + 3].vx = 10000;
            verts[i * 4 + 1].vy = verts[i * 4 + 3].vy = 1000;
            verts[i * 4 + 1].vz = verts[i * 4 + 3].vz = -15000;
            verts[i * 4].vy                           = 200;
            verts[i * 4 + 1].vy                       = 200;
            faces[i].verts[1]                         = i * 4 + 1;
            faces[i].verts[0]                         = i * 4;
            faces[i].verts[2]                         = i * 4 + 2;
            faces[i].verts[3]                         = i * 4 + 3;
            faces[i].field_A                          = 1;
            faces[i].field_8                          = i;
            np                                        = &normal;
            normal.vx                                 = 0;
            normal.vy                                 = 0;
            normal.vz                                 = 0x1000;
            VectorNormalSS(np, np);
            normals[i] = normal;
            i++;
        } while (i < 8);
    }
}

void func_shelter_b3_garbage_incinerator_8018507C(void)
{
    SVECTOR     normal;
    SVECTOR*    normals;
    SVECTOR*    verts;
    GpGridFace* faces;
    s16         i;
    SVECTOR*    np;

    i       = 0;
    normals = Gp_GridParams->field_4;
    verts   = Gp_GridParams->field_8;
    faces   = Gp_GridParams->field_C;
    np      = &normal;
    func_shelter_b3_garbage_incinerator_80184EEC();
    do {
        verts[i * 4].vx = verts[i * 4 + 2].vx = D_shelter_b3_garbage_incinerator_8018FBCC[i][0];
        verts[i * 4].vy = verts[i * 4 + 2].vy = 0;
        verts[i * 4].vz = verts[i * 4 + 2].vz = D_shelter_b3_garbage_incinerator_8018FBCC[i][1];
        verts[i * 4 + 1].vx = verts[i * 4 + 3].vx = D_shelter_b3_garbage_incinerator_8018FBCC[i][2];
        verts[i * 4 + 1].vy = verts[i * 4 + 3].vy = 0;
        verts[i * 4 + 1].vz = verts[i * 4 + 3].vz = D_shelter_b3_garbage_incinerator_8018FBCC[i][3];
        verts[i * 4].vy                          -= 400;
        verts[i * 4 + 1].vy                      -= 400;
        faces[i].verts[1]                         = i * 4 + 1;
        faces[i].verts[0]                         = i * 4;
        faces[i].verts[2]                         = i * 4 + 2;
        faces[i].verts[3]                         = i * 4 + 3;
        faces[i].field_A                          = 1;
        faces[i].field_8                          = i;
        normal.vx                                 = D_shelter_b3_garbage_incinerator_8018FBCC[i][3] - D_shelter_b3_garbage_incinerator_8018FBCC[i][1];
        normal.vy                                 = 0;
        normal.vz                                 = D_shelter_b3_garbage_incinerator_8018FBCC[i][0] - D_shelter_b3_garbage_incinerator_8018FBCC[i][2];
        VectorNormalSS(np, np);
        normals[i] = normal;
        i++;
    } while (i < 6);
}

void func_shelter_b3_garbage_incinerator_80185220(void)
{
    SVECTOR     normal;
    SVECTOR*    normals;
    SVECTOR*    verts;
    GpGridFace* faces;
    s16         i;
    SVECTOR*    np;

    i       = 0;
    normals = Gp_GridParams->field_4;
    verts   = Gp_GridParams->field_8;
    faces   = Gp_GridParams->field_C;
    np      = &normal;
    func_shelter_b3_garbage_incinerator_80184EEC();
    do {
        verts[i * 4].vx = verts[i * 4 + 2].vx = D_shelter_b3_garbage_incinerator_8018FBFC[i][0];
        verts[i * 4].vy = verts[i * 4 + 2].vy = 0;
        verts[i * 4].vz = verts[i * 4 + 2].vz = D_shelter_b3_garbage_incinerator_8018FBFC[i][1];
        verts[i * 4 + 1].vx = verts[i * 4 + 3].vx = D_shelter_b3_garbage_incinerator_8018FBFC[i][2];
        verts[i * 4 + 1].vy = verts[i * 4 + 3].vy = 0;
        verts[i * 4 + 1].vz = verts[i * 4 + 3].vz = D_shelter_b3_garbage_incinerator_8018FBFC[i][3];
        verts[i * 4].vy                          -= 400;
        verts[i * 4 + 1].vy                      -= 400;
        faces[i].verts[1]                         = i * 4 + 1;
        faces[i].verts[0]                         = i * 4;
        faces[i].verts[2]                         = i * 4 + 2;
        faces[i].verts[3]                         = i * 4 + 3;
        faces[i].field_8                          = i;
        faces[i].field_A                          = 1;
        normal.vx                                 = D_shelter_b3_garbage_incinerator_8018FBFC[i][3] - D_shelter_b3_garbage_incinerator_8018FBFC[i][1];
        normal.vy                                 = 0;
        normal.vz                                 = D_shelter_b3_garbage_incinerator_8018FBFC[i][0] - D_shelter_b3_garbage_incinerator_8018FBFC[i][2];
        VectorNormalSS(np, np);
        normals[i] = normal;
        i++;
    } while (i < 6);
}

void func_shelter_b3_garbage_incinerator_801853C4(void)
{
    SVECTOR     normal;
    SVECTOR*    normals;
    SVECTOR*    verts;
    GpGridFace* faces;
    s16         i;

    i       = 0;
    normals = Gp_GridParams->field_4;
    verts   = Gp_GridParams->field_8;
    faces   = Gp_GridParams->field_C;
    func_shelter_b3_garbage_incinerator_80184EEC();
    do {
        verts[i * 4].vx = verts[i * 4 + 2].vx = D_shelter_b3_garbage_incinerator_8018FBFC[i][0];
        verts[i * 4].vy = verts[i * 4 + 2].vy = 0;
        verts[i * 4].vz = verts[i * 4 + 2].vz = D_shelter_b3_garbage_incinerator_8018FBFC[i][1];
        verts[i * 4 + 1].vx = verts[i * 4 + 3].vx = D_shelter_b3_garbage_incinerator_8018FBFC[i][2];
        verts[i * 4 + 1].vy = verts[i * 4 + 3].vy = 0;
        verts[i * 4 + 1].vz = verts[i * 4 + 3].vz = D_shelter_b3_garbage_incinerator_8018FBFC[i][3];
        verts[i * 4].vy                          += 1000;
        verts[i * 4 + 1].vy                      += 1000;
        verts[i * 4 + 2].vy                      += 2000;
        verts[i * 4 + 3].vy                      += 2000;
        faces[i].verts[1]                         = i * 4 + 1;
        faces[i].verts[0]                         = i * 4;
        faces[i].verts[2]                         = i * 4 + 2;
        faces[i].verts[3]                         = i * 4 + 3;
        faces[i].field_8                          = i;
        faces[i].field_A                          = 1;
        normal.vx                                 = D_shelter_b3_garbage_incinerator_8018FBFC[i][3] - D_shelter_b3_garbage_incinerator_8018FBFC[i][1];
        normal.vy                                 = 0;
        normal.vz                                 = D_shelter_b3_garbage_incinerator_8018FBFC[i][0] - D_shelter_b3_garbage_incinerator_8018FBFC[i][2];
        VectorNormalSS(&normal, &normal);
        normals[i] = normal;
        i++;
    } while (i < 6);
}

void func_shelter_b3_garbage_incinerator_80185574(void)
{
    func_shelter_b3_garbage_incinerator_80185220();
}
