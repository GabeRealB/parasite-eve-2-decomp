#include "common.h"

#include "main/display.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/mist_r18.h"

void func_mist_r18_8017E448(MistR18Sprite* sprite);
void func_mist_r18_8017E654(s16 abr, s16 x, s16 y, s32 otIdx);
/// Spawn descriptor handed to entry 5 of `RoomsShared8017e5b8Desc`.
extern s32      D_mist_r18_80184EE4;
extern TaskDesc RoomsShared8017e5b8Desc;
/// The two prop tasks `func_mist_r18_8017E784` tears down, by index.
extern Task* D_mist_r18_80186E90;
extern Task* D_mist_r18_80186E94;

void func_mist_r18_8017E39C(void)
{
}

/// Redraw the room's sprite rectangle each frame until the spawn countdown in
/// `Task::spawnArg1` runs out, then kill the task.
void func_mist_r18_8017E3A4(Task* task)
{
    MistR18Sprite       sprite;
    MistR18SpriteSpawn* spawn;

    spawn = task->spawnArg2;

    if (task->state == 0) {
        sprite.x         = spawn->x;
        sprite.y         = spawn->y;
        sprite.w         = spawn->w;
        sprite.h         = spawn->h;
        sprite.b         = 0;
        sprite.g         = 0;
        sprite.r         = 0;
        sprite.semiTrans = 1;
        func_mist_r18_8017E448(&sprite);
        func_mist_r18_8017E654(0, 0, 0, 5);

        if (--task->spawnArg1 > 0) {
            return;
        }
    }
    Task_Kill(task);
}

/// Emit the sprite's screen rectangle as a flat-shaded `TILE` into OT slot 5.
void func_mist_r18_8017E448(MistR18Sprite* sprite)
{
    TILE* tile;

    tile           = (TILE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
    setTile(tile);
    if (sprite->semiTrans == 0) {
        SetShadeTex(tile, 1);
        SetSemiTrans(tile, 0);
    } else {
        SetShadeTex(tile, 0);
        SetSemiTrans(tile, 1);
    }
    tile->r0 = sprite->r;
    tile->g0 = sprite->g;
    tile->b0 = sprite->b;
    tile->x0 = sprite->x;
    tile->y0 = sprite->y;
    tile->w  = sprite->w - 1;
    tile->h  = sprite->h - 1;
    AddPrim(Gpu_CurrentOt + 5, tile);
}

/// Emit the sprite's screen rectangle as a textured `SPRT` into OT slot 4,
/// with the CLUT taken from the framebuffer position `clutX`/`clutY`.
void func_mist_r18_8017E534(MistR18Sprite* sprite, u32 clutX, s32 clutY)
{
    SPRT* p;
    u8    v;

    p              = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    SetSprt(p);
    if (sprite->semiTrans == 0) {
        SetShadeTex(p, 1);
        SetSemiTrans(p, 0);
    } else {
        SetShadeTex(p, 0);
        SetSemiTrans(p, 1);
    }
    p->r0   = sprite->r;
    p->g0   = sprite->g;
    p->b0   = sprite->b;
    p->x0   = sprite->x;
    p->y0   = sprite->y;
    p->u0   = sprite->u;
    v       = sprite->v;
    p->clut = getClut(clutX, clutY);
    p->v0   = v;
    p->w    = sprite->w - 1;
    p->h    = sprite->h - 1;
    AddPrim(Gpu_CurrentOt + 4, p);
}

/// Append a `DR_TPAGE` for the given tpage to OT slot `otIdx`.
void func_mist_r18_8017E654(s16 abr, s16 x, s16 y, s32 otIdx)
{
    DR_TPAGE* dr;

    dr             = Gpu_PrimCursor;
    Gpu_PrimCursor = dr + 1;
    SetDrawTPage(dr, 1, 0, GetTPage(0, abr, x, y));
    AddPrim(Gpu_CurrentOt + otIdx, dr);
}

/// Spawn prop task `idx` (0 or 1) into its slot if it is not already running,
/// and clear bit 7 of its model's flags. Other indices do nothing.
void func_mist_r18_8017E6D8(s32 idx)
{
    Task** slot;
    Task*  task;

    switch (idx) {
        case 0:
            slot = &D_mist_r18_80186E90;
            break;
        case 1:
            slot = &D_mist_r18_80186E94;
            break;
        default:
            slot = NULL;
            break;
    }

    if ((slot != NULL) && (*slot == NULL)) {
        task  = Task_SpawnFromTable(&RoomsShared8017e5b8Desc, idx, 8, (s32)Game_GetPtrSlot(3));
        *slot = task;
        if (task != NULL) {
            ((TmdObject*)task->extra)->field_C &= 0xFF7F;
        }
    }
}

/// Kill and clear prop task `idx` (0 or 1); other indices do nothing.
void func_mist_r18_8017E784(s32 idx)
{
    if (idx == 0) {
        if (D_mist_r18_80186E90 != NULL) {
            Task_Kill(D_mist_r18_80186E90);
        }
        D_mist_r18_80186E90 = NULL;
    } else if (idx == 1) {
        if (D_mist_r18_80186E94 != NULL) {
            Task_Kill(D_mist_r18_80186E94);
        }
        D_mist_r18_80186E94 = NULL;
    }
}

void func_mist_r18_8017E7F0(void)
{
    Task_SpawnFromTable(&RoomsShared8017e5b8Desc, 5, 0, (s32)&D_mist_r18_80184EE4);
}
