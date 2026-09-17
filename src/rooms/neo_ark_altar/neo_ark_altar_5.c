#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/neo_ark_altar.h"

#include <psyq/inline_c.h>

extern TaskDesc RoomsShared8018397cDesc;

extern u8 D_8007216D;

extern NeoArkAltarTile D_neo_ark_altar_8017F014[];

extern s16 D_neo_ark_altar_801800AC;
extern s16 D_neo_ark_altar_801800AE;
extern s16 D_neo_ark_altar_801800B0[];

void func_neo_ark_altar_8017E658(SVECTOR* p0, SVECTOR* p1, SVECTOR* p2, SVECTOR* p3);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017DC40);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017DF0C);

/// Altar state 0: gates the wall sprites of the current view's record on game
/// flag 0xD9 and resets the altar's work area. The switch state written to
/// `D_neo_ark_altar_801800AE` is 6 while the flag is clear, 0 otherwise; the
/// six sprite commands reached through `rec[3]` / `rec[6]` / `rec[4]` are
/// skipped (1) or linked (0) to match, and the 17 halfwords at
/// `D_neo_ark_altar_801800B0` are cleared for `func_neo_ark_altar_8017E260`.
void func_neo_ark_altar_8017E148(void)
{
    GameSessionFrom4* sess;
    GpSprtRec*        rec;
    GpSprtCmd*        cmd;
    s32               i;

    sess = (GameSessionFrom4*)&Game_Session->field_4;
    rec  = Gp_SprtTables[sess->field_3 - 1][0].field_0[sess->field_2 - 1];
    if (GameFlag_GetNibble(0xD9) == 0) {
        cmd                      = rec[3].field_4;
        cmd[1].field_4           = 1;
        cmd                      = rec[6].field_4;
        cmd[1].field_4           = 1;
        cmd                      = rec[4].field_4;
        cmd[1].field_4           = 0;
        cmd[2].field_4           = 1;
        cmd[3].field_4           = 1;
        cmd[4].field_4           = 1;
        cmd[5].field_4           = 1;
        cmd[6].field_4           = 1;
        D_neo_ark_altar_801800AE = 6;
    } else {
        cmd                      = rec[3].field_4;
        cmd[1].field_4           = 0;
        cmd                      = rec[6].field_4;
        cmd[1].field_4           = 0;
        cmd                      = rec[4].field_4;
        cmd[1].field_4           = 1;
        cmd[2].field_4           = 1;
        cmd[3].field_4           = 1;
        cmd[4].field_4           = 1;
        cmd[5].field_4           = 1;
        cmd[6].field_4           = 0;
        D_neo_ark_altar_801800AE = 0;
    }
    D_neo_ark_altar_801800AC = 0;
    for (i = 0x10; i >= 0; i--) {
        D_neo_ark_altar_801800B0[i] = 0;
    }
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017E260);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017E658);

/// Walls in one altar tile. The view matrix is re-derived from
/// `Gfx_ViewCoord` and `Gfx_ViewWorldMtx` pushed into the GTE first, then each
/// of the tile's four sides goes to `func_neo_ark_altar_8017E658` as its two
/// corners at the floor height `y0` and at `y0 - arg1`, so `arg1` is how far a
/// side drops below the tile. The sides walk the tile rectangle
/// `(x, z) -> (x + w, z) -> (x + w, z + d) -> (x, z + d)` as `arg0` selects
/// the tile in the table.
void func_neo_ark_altar_8017E92C(s16 arg0, s32 arg1)
{
    NeoArkAltarTile* tile;
    NeoArkAltarTile* base;
    SVECTOR          p0;
    SVECTOR          p1;
    SVECTOR          p2;
    SVECTOR          p3;
    s16              y0;
    s16              y1;

    base = D_neo_ark_altar_8017F014;
    y0   = -0x1086;

    Gfx_ViewCoord.flg = 0;
    Gp_UpdateCoord(&Gfx_ViewCoord);

    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);

    tile = &base[arg0];
    y1   = y0 - arg1;

    p0.vx = tile->x;
    p0.vy = y0;
    p0.vz = tile->z;
    p1.vx = tile->x + tile->w;
    p1.vy = y0;
    p1.vz = tile->z;
    p2.vx = tile->x;
    p2.vy = y1;
    p2.vz = tile->z;
    p3.vx = tile->x + tile->w;
    p3.vy = y1;
    p3.vz = tile->z;
    func_neo_ark_altar_8017E658(&p0, &p1, &p2, &p3);

    p0.vx = tile->x + tile->w;
    p0.vy = y0;
    p0.vz = tile->z;
    p1.vx = tile->x + tile->w;
    p1.vy = y0;
    p1.vz = tile->z + tile->d;
    p2.vx = tile->x + tile->w;
    p2.vy = y1;
    p2.vz = tile->z;
    p3.vx = tile->x + tile->w;
    p3.vy = y1;
    p3.vz = tile->z + tile->d;
    func_neo_ark_altar_8017E658(&p0, &p1, &p2, &p3);

    p0.vx = tile->x;
    p0.vy = y0;
    p0.vz = tile->z + tile->d;
    p1.vx = tile->x + tile->w;
    p1.vy = y0;
    p1.vz = tile->z + tile->d;
    p2.vx = tile->x;
    p2.vy = y1;
    p2.vz = tile->z + tile->d;
    p3.vx = tile->x + tile->w;
    p3.vy = y1;
    p3.vz = tile->z + tile->d;
    func_neo_ark_altar_8017E658(&p0, &p1, &p2, &p3);

    p0.vx = tile->x;
    p0.vy = y0;
    p0.vz = tile->z;
    p1.vx = tile->x;
    p1.vy = y0;
    p1.vz = tile->z + tile->d;
    p2.vx = tile->x;
    p2.vy = y1;
    p2.vz = tile->z;
    p3.vx = tile->x;
    p3.vy = y1;
    p3.vz = tile->z + tile->d;
    func_neo_ark_altar_8017E658(&p0, &p1, &p2, &p3);
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017EC34);

void func_neo_ark_altar_8017ED60(Task* task);
void func_neo_ark_altar_8017EDBC(Task* task);
void func_neo_ark_altar_8017DF0C(Task* task);
void func_neo_ark_altar_8017EDF8(Task* task);
void func_neo_ark_altar_8017EE30(Task* task);
void func_neo_ark_altar_8017EE90(Task* task);
void func_neo_ark_altar_8017EF00(Task* task);
void func_neo_ark_altar_8017EF34(Task* task);

/// State handlers of the altar task, dispatched by
/// `func_neo_ark_altar_8017ECE0` off `Task::state`. The table is declared here
/// rather than left to the splitter: only a file-scope definition in this unit
/// puts the bytes in this unit's `.rodata`, after the jump tables
/// `func_neo_ark_altar_8017DC40` contributes to the same block.
const TaskFuncTable8 D_neo_ark_altar_8017D648 = {
    func_neo_ark_altar_8017ED60,
    func_neo_ark_altar_8017EDBC,
    func_neo_ark_altar_8017DF0C,
    func_neo_ark_altar_8017EDF8,
    func_neo_ark_altar_8017EE30,
    func_neo_ark_altar_8017EE90,
    func_neo_ark_altar_8017EF00,
    func_neo_ark_altar_8017EF34,
};

void func_neo_ark_altar_8017ECE0(Task* arg0)
{
    TaskFuncTable8 sp = D_neo_ark_altar_8017D648;

    sp.funcs[arg0->state](arg0);
}

void func_neo_ark_altar_8017ED60(Task* arg0)
{
    TaskIdMap* work;

    work        = Mem_Calloc(0x10, 0);
    arg0->idMap = work;
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    func_neo_ark_altar_8017E148();
    arg0->killCountdown = 0;
    arg0->state         = (s32)(arg0->state + 1);
}

void func_neo_ark_altar_8017EDBC(Task* arg0)
{
    arg0->killCountdown = arg0->killCountdown + 1;
    if (arg0->killCountdown >= 3) {
        arg0->state = (s32)(arg0->state + 1);
    }
}

void func_neo_ark_altar_8017EDF8(Task* arg0)
{
    Gp_MsgPlayerWeapon(0);
    arg0->killCountdown = 0;
    arg0->state         = (s32)(arg0->state + 1);
}

void func_neo_ark_altar_8017EE30(Task* arg0)
{
    u8 temp_a0;

    arg0->killCountdown = arg0->killCountdown + 6;
    if (arg0->killCountdown >= 0x100) {
        arg0->killCountdown = 0xFF;
        arg0->state         = (s32)(arg0->state + 1);
    }
    temp_a0 = (u8)arg0->killCountdown;
    Fade_DrawOverlay(temp_a0, temp_a0, temp_a0, 2);
}

void func_neo_ark_altar_8017EE90(Task* arg0)
{
    Task** temp_s1;

    temp_s1 = arg0->idMap;
    Gp_MsgPlayer3F3(0);
    Game_Session->field_68 = 1;
    *temp_s1               = Task_SpawnFromTable(&RoomsShared8018397cDesc, 0, 2, 0);
    arg0->state            = (s32)(arg0->state + 1);
}

void func_neo_ark_altar_8017EF00(Task* arg0)
{
    *(s16*)((u8*)Game_Session + OFFSET_OF(GameSession, field_52)) = 1;
    D_8007216D                                                    = 2;
    Game_Session->field_5                                         = 2;
    arg0->state                                                   = (s32)(arg0->state + 1);
}

void func_neo_ark_altar_8017EF34(Task* arg0)
{
    SetDispMask(1);
    Gp_MsgPlayer3F3(1);
    Gp_MsgPlayerWeapon(1);
    Game_Session->field_68 = 0;
    arg0->state            = 2;
}

void func_neo_ark_altar_8017EF84(void)
{
}
