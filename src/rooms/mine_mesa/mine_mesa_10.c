#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/areaplace.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/stdio.h>

/// A place an enemy can be spawned at: its position and the yaw it faces.
typedef struct {
    s16 x;
    s16 y;
    s16 z;
    s16 yaw;
} _MineMesaSpawnPoint;

/// One of the room's four wall segments: the two corners along its
/// base. The trailing bytes are not read by the code that builds the walls.
typedef struct {
    SVECTOR start;
    SVECTOR end;
    u8      unk10[8];
} _MineMesaWall;

extern s16                 D_mine_mesa_80189B6C;
extern s16                 D_mine_mesa_80189B6E;
extern GpEnemy*            D_mine_mesa_80189B74[2];
extern s32                 D_mine_mesa_80189B1C;
extern _MineMesaSpawnPoint D_mine_mesa_80189AFC[];
extern _MineMesaWall       D_mine_mesa_80189A9C[4];
extern char                D_mine_mesa_8017D624[];
extern TaskDesc            D_8014D8A4;
extern u32                 Gp_LcgState;
extern s8                  D_8007217B;
extern s8                  D_8007218B;
extern s16                 D_801153F6;

#define MINE_MESA_RAND() ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16)

void func_mine_mesa_801811C4(s32 arg0);

/// Rebuilds collision faces 3-6 of the room's grid as vertical walls, one per
/// entry of `D_mine_mesa_80189A9C`. Each face is a quad whose lower corners are
/// the wall's two base points and whose upper corners sit `height` above them
/// (the grid's y axis points down), with a horizontal normal perpendicular to
/// the base.
void func_mine_mesa_801811C4(s32 height)
{
    SVECTOR*    normals;
    SVECTOR*    verts;
    GpGridFace* faces;
    s16         i;
    s16         face;

    normals = Gp_GridParams->field_4;
    verts   = Gp_GridParams->field_8;
    faces   = Gp_GridParams->field_C;
    for (i = 0; i < 4; i++) {
        face               = i + 3;
        verts[face * 4].vx = verts[face * 4 + 2].vx = D_mine_mesa_80189A9C[i].start.vx;
        verts[face * 4].vy = verts[face * 4 + 2].vy = D_mine_mesa_80189A9C[i].start.vy;
        verts[face * 4].vz = verts[face * 4 + 2].vz = D_mine_mesa_80189A9C[i].start.vz;
        verts[face * 4 + 1].vx = verts[face * 4 + 3].vx = D_mine_mesa_80189A9C[i].end.vx;
        verts[face * 4 + 1].vy = verts[face * 4 + 3].vy = D_mine_mesa_80189A9C[i].end.vy;
        verts[face * 4 + 1].vz = verts[face * 4 + 3].vz = D_mine_mesa_80189A9C[i].end.vz;
        verts[face * 4 + 2].vy                         -= height;
        verts[face * 4 + 3].vy                         -= height;
        faces[face].verts[1]                            = face * 4 + 1;
        faces[face].verts[0]                            = face * 4;
        faces[face].verts[2]                            = face * 4 + 2;
        faces[face].verts[3]                            = face * 4 + 3;
        faces[face].field_A                             = 3;
        faces[face].field_8                             = face;
        normals[face].vx                                = D_mine_mesa_80189A9C[i].start.vz - D_mine_mesa_80189A9C[i].end.vz;
        normals[face].vy                                = 0;
        normals[face].vz                                = D_mine_mesa_80189A9C[i].end.vx - D_mine_mesa_80189A9C[i].start.vx;
        VectorNormalSS(&normals[face], &normals[face]);
    }
}

/// Keeps the room's two enemy slots in `D_mine_mesa_80189B74` filled while the
/// kill counter `D_mine_mesa_80189B6C` is non-zero. An empty slot gets a new
/// enemy placed at one of the spawn points in `D_mine_mesa_80189AFC`, drawn at
/// random from the subset the current view allows, textured from the area's
/// place record; `D_mine_mesa_80189B6E` then delays the next spawn. When one
/// kill remains, nothing spawns until both slots are empty. Once the counter
/// is zero the task hands `Gp_ReleaseStateF0` an empty result record and
/// advances its state.
void func_mine_mesa_80181358(Task* arg0)
{
    GpAreaKey            key;
    GpObj5C              result;
    s32                  i;
    s32                  pick;
    u32                  rnd;
    _MineMesaSpawnPoint* table;
    _MineMesaSpawnPoint* pt;
    TmdObject*           tmd;
    GpAreaKey*           loc;
    GpAreaPlace*         place;
    GsCOORDINATE2*       coords;
    GpEnemy*             enemy;

    for (i = 0; i < 2; i++) {
        if (D_mine_mesa_80189B6E > 0) {
            D_mine_mesa_80189B6E--;
            break;
        }
        if (D_mine_mesa_80189B6C == 0) {
            goto end;
        }
        if (D_mine_mesa_80189B6C == 1 &&
            (D_mine_mesa_80189B74[0] != NULL || D_mine_mesa_80189B74[1] != NULL)) {
            break;
        }
        if (D_mine_mesa_80189B74[i] != NULL) {
            continue;
        }
        enemy                   = Gp_SpawnEnemyFromTable(&D_8014D8A4, 0, 0x30002, NULL);
        D_mine_mesa_80189B74[i] = enemy;
        if (enemy == NULL) {
            break;
        }
        enemy->workType                    = 0x900;
        D_mine_mesa_80189B74[i]->placeKey |= i << 12;
        switch (Gp_GetViewIndex() & 0xFF) {
            case 2:
                pick = MINE_MESA_RAND() % 3 + 1;
                break;
            case 3:
                pick = (MINE_MESA_RAND() & 1) | 2;
                break;
            case 4:
                pick = ((MINE_MESA_RAND() & 1) == 0) * 2;
                rnd  = MINE_MESA_RAND() & 1;
                if (rnd == 1) {
                    pick = rnd;
                }
                break;
            case 5:
                pick = MINE_MESA_RAND() & 1;
                break;
            case 8:
                pick = (MINE_MESA_RAND() & 1) | 2;
                break;
            default:
                pick = MINE_MESA_RAND() & 3;
                break;
        }
        table                                                                  = D_mine_mesa_80189AFC;
        pt                                                                     = &table[(s16)pick];
        ((TmdObject*)D_mine_mesa_80189B74[i]->task->extra)->coords->coord.t[0] = pt->x;
        ((TmdObject*)D_mine_mesa_80189B74[i]->task->extra)->coords->coord.t[1] = pt->y;
        ((TmdObject*)D_mine_mesa_80189B74[i]->task->extra)->coords->coord.t[2] = pt->z;
        tmd                                                                    = (TmdObject*)D_mine_mesa_80189B74[i]->task->extra;
        loc                                                                    = &gGameSession->at4.loc;
        key.stage                                                              = loc->stage;
        key.area                                                               = loc->area;
        key.room                                                               = loc->room;
        key.view                                                               = gGameSession->at4.loc.view;
        Gp_SyncAreaKeyIndex(&key);
        place      = (GpAreaPlace*)Gp_GetNestedAreaRec(&key)->field_0;
        tmd->tpage = place->tpage;
        tmd->clut  = place->clut;
        if (D_8007218B == 10) {
            printf(D_mine_mesa_8017D624, (s8)place->tpage, (s8)place->clut, 0);
        }
        if (tmd->buffer != NULL) {
            tmdProcessStream(tmd);
            tmdProcessStream(tmd);
        }
        Gfx_RotMatrixY(&((TmdObject*)D_mine_mesa_80189B74[i]->task->extra)->coords->coord,
                       pt->yaw, 1);
        coords               = ((TmdObject*)D_mine_mesa_80189B74[i]->task->extra)->coords;
        D_mine_mesa_80189B6E = 0x50;
        coords->flg          = 0;
    }
    if (D_mine_mesa_80189B6C > 0) {
        return;
    }
end:
    D_8007217B      = 0;
    arg0->spawnArg2 = &result;
    result.field_50 = NULL;
    D_801153F6      = 1;
    Gp_ReleaseStateF0((GpObj20E*)arg0, 0);
    D_80062735 = 1;
    arg0->state++;
}

/// Picks the height the room's terrain updater subtracts from its mesh
/// vertices: 0x7D0 while `gGameSession->at4.loc.place` says the session is in area 1
/// or 7, 0x190 otherwise, then hands that to `func_mine_mesa_801811C4`.
void func_mine_mesa_801817BC(void)
{
    s32 offset;

    if (gGameSession->at4.loc.place == 1 || gGameSession->at4.loc.place == 7) {
        offset = 0x7D0;
    } else {
        offset = 0x190;
    }
    func_mine_mesa_801811C4(offset);
}

/// Message 0x13F4 handler, the only entry of the room's `GpMsgEntry` table
/// `D_mine_mesa_80189B1C`. Once the enemy parked in slot `slot` of
/// `D_mine_mesa_80189B74` is dead (`field_40` is its HP) the slot is emptied and
/// the room's remaining-enemy countdown ticks down. Always consumes the message.
s32 func_mine_mesa_80181800(Task* task, s32 msgId, s32 slot, s32 arg3)
{
    if (D_mine_mesa_80189B74[slot] != NULL && D_mine_mesa_80189B74[slot]->hp <= 0) {
        D_mine_mesa_80189B74[slot] = NULL;
        D_mine_mesa_80189B6C       = (u16)D_mine_mesa_80189B6C - 1;
    }
    return 1;
}

/// Starts the room's slot countdown: seeds `D_mine_mesa_80189B6C` to 10, clears
/// the two slots at `D_mine_mesa_80189B74`, points the task at the room's state
/// descriptor and advances a state. `func_mine_mesa_80181800` later empties a
/// slot and decrements the counter once the thing in it is gone.
void func_mine_mesa_80181848(Task* arg0)
{
    D_mine_mesa_80189B6C    = 10;
    D_mine_mesa_80189B74[1] = 0;
    D_mine_mesa_80189B74[0] = 0;
    arg0->msgTable          = &D_mine_mesa_80189B1C;
    arg0->state++;
}

INCLUDE_RODATA("rooms/nonmatchings/mine_mesa/mine_mesa_10", D_mine_mesa_8017D660);
