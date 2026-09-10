#include "common.h"

#include "main/task.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "actors/actor_503500.h"
#include "actors/actors_shared_801366fc.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/tmd.h"
#include "main/gameflag.h"

/// `Gp_DispatchMsg` handler table installed at `Task::field_24` by
/// `func_actor_503500_80132430`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_503500_80146888[];
/// State handlers `func_actor_503500_8013270C` dispatches through by
/// `Task::state`, copied onto the stack first.
extern TaskFuncTable3 D_actor_503500_80131E24;
void                  func_actor_503500_801324EC(Task* arg0);
/// Spawn positions `func_actor_503500_80132778` indexes by `Task::spawnArg1`.
extern SVECTOR  D_actor_503500_8014B97C[];
extern Task*    D_actor_503500_80176558;
extern TaskDesc D_actor_503500_8014B964;
extern s8       D_actor_503500_80176D5A;
extern s16      D_actor_503500_80176D2E;
extern u16      D_actor_503500_80176D24;
/// Opaque script/table blobs in the overlay's `.data`, handed to
/// `func_800E8634` (which forwards them to `Task_Spawn`) as raw addresses.
extern u8 D_actor_503500_8014CD98[];
extern u8 D_actor_503500_8014D098[];
/// whatever room overlay is resident owns the body.
extern void              func_8017E27C(s32 arg0);
extern Actor503500MsgPos D_actor_503500_8017655C;
/// Player-facing flag byte in the main executable; no module header owns it yet.
extern u8 D_80073BA9;
/// Main-executable globals with no module header yet: `D_80071075` gates the
/// "everything is dead" message, `D_80073BA0` is the remaining-enemy count and
/// `D_80114C12` the cutscene/among-us mode flag.
extern u8  D_80071075;
extern s16 D_80073BA0;
extern s8  D_80114C12;
s32        func_actor_503500_80133684(Actor503500* arg0);
/// Reports whether slot `arg1` of the boss work block's `enemies` array is
/// empty. `arg0` is loaded by every caller but the body ignores it.
s32  func_actor_503500_80135E04(Task* arg0, s32 arg1);
void func_actor_503500_801338E8(Actor503500* arg0);
void func_actor_503500_80134408(Actor503500* arg0);
void func_actor_503500_801345F4(Actor503500* arg0);
void func_actor_503500_80134A24(Actor503500* arg0);
void func_actor_503500_80134C68(Actor503500* arg0);
void func_actor_503500_80135FB4(Actor503500* arg0, s32 arg1, s32 arg2);
s32  func_actor_503500_80136014(Actor503500* arg0, s32 arg1);
void func_actor_503500_8013611C(s32 arg0);
void func_actor_503500_80135828(Actor503500* arg0, s8* arg1);
void func_actor_503500_801372AC(s32 arg0);
void func_actor_503500_80136450(Actor503500* arg0);
void func_actor_503500_801369E4(Actor503500* arg0);
void func_actor_503500_80136A80(Actor503500* arg0);
void func_actor_503500_80136EFC(Actor503500* arg0, s32 arg1);
void func_actor_503500_801374BC(Actor503500* arg0);
void func_actor_503500_80137678(Actor503500* arg0);
void func_actor_503500_80138454(Actor503500* arg0);
void func_actor_503500_8013B460(Actor503500* arg0);
void func_actor_503500_8013B8D0(Actor503500* arg0);
void func_actor_503500_8013BE0C(Actor503500* arg0);
void func_actor_503500_8013E384(Actor503500* arg0);
void func_actor_503500_8013E740(Actor503500* arg0);
void func_actor_503500_8013EBE4(Actor503500* arg0);
/// Global "everything is frozen" mode byte in the main executable: 1 pauses the
/// actor, 2 hides it, anything else runs the normal per-frame chain.
extern u8 D_801153F4;
void      func_actor_503500_801398D0(Actor503500* arg0);
void      func_actor_503500_80139EFC(Actor503500* arg0);
void      func_actor_503500_8013A0D0(Actor503500* arg0);
void      func_actor_503500_8013A96C(Actor503500* arg0);
void      func_actor_503500_8013AA44(Actor503500* arg0);
void      func_actor_503500_8013AAC0(Actor503500* arg0);
void      func_actor_503500_8013AB38(Actor503500* arg0);
void      func_actor_503500_8013DBA8(Actor503500* arg0, s32 arg1);
void      func_actor_503500_8013F328(Actor503500* arg0);
void      func_actor_503500_8013F4A4(Actor503500* arg0);
void      func_actor_503500_8013F948(Actor503500* arg0);
void      func_actor_503500_8013F984(Actor503500* arg0);
void      func_actor_503500_80140BE8(Actor503500* arg0);
void      func_actor_503500_80141248(Actor503500* arg0);
void      func_actor_503500_80141448(Actor503500* arg0);
void      func_actor_503500_80141B94(Actor503500* arg0);
void      func_actor_503500_80141D7C(Actor503500* arg0);
void      func_actor_503500_801420C4(Actor503500* arg0);
void      func_actor_503500_801421A8(Actor503500* arg0);
void      func_actor_503500_80142310(Actor503500* arg0, s32 arg1);
void      func_actor_503500_8014271C(Actor503500* arg0);
void      func_actor_503500_80142980(Actor503500* arg0);
void      func_actor_503500_8014418C(Actor503500* arg0);
void      func_actor_503500_801441E8(Actor503500* arg0);
void      func_actor_503500_80144238(Actor503500* arg0, s32 arg1);

void func_actor_503500_801324EC(Task* arg0)
{
    TmdObject*           ext;
    Actor503500ColorMtx* work;

    ext           = arg0->extra;
    work          = (Actor503500ColorMtx*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80132508);

s32 func_actor_503500_80132584(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra;
    ret = 0;
    switch (mode) {
        case 0:
            obj->field_C |= 0x80;
            obj->field_C &= ~4;
            break;
        case 1:
            obj->field_C &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->field_C &= ~4;
            break;
        case 2:
            obj->field_C                                 |= 0x80;
            ((Actor503500ColorMtx*)task->idMap)->field_44 = mode;
            obj->field_C                                 |= 4;
            break;
        case 3:
            obj->field_C &= ~0x80;
            obj->field_C |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_503500_80132664(Task* task, s32 arg1, Actor503500ModeMsg* msg)
{
    Actor503500ColorMtx* work;

    work = (Actor503500ColorMtx*)task->idMap;
    switch (msg->mode) {
        case 0:
            work->field_45 = 0;
            work->field_40 = 0;
            Display_ClampField126(0);
            break;
        case 1:
            work->field_45                     = 1;
            work->field_40                     = 0;
            ((TmdObject*)task->extra)->field_E = 0x15;
            break;
        case 2:
            work->field_45                     = 2;
            work->field_40                     = 0;
            ((TmdObject*)task->extra)->field_E = 0x14;
            break;
        case 3:
            work->field_45 = 0;
            work->field_40 = 10000;
            break;
    }
    return 0;
}

void func_actor_503500_8013270C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80131E24;
    if (D_801153F4 == 0) {
        sp.funcs[task->state](task);
    }
}

void func_actor_503500_80132778(Task* task)
{
    GsCOORDINATE2*      coord;
    GpMtxWords*         rot;
    Actor503500EffWork* work;
    SVECTOR*            pos;
    u8                  done;

    coord = ((TmdObject*)task->extra)->field_8;
    if (task->state == 0) {
        pos               = &D_actor_503500_8014B97C[task->spawnArg1];
        coord->coord.t[0] = pos->vx;
        coord->coord.t[1] = pos->vy;
        coord->coord.t[2] = pos->vz;
        rot               = (GpMtxWords*)&coord->coord;
        rot->w0           = 0x1000;
        rot->w1           = 0;
        rot->w2           = 0x1000;
        rot->w3           = 0;
        rot->h4           = 0x1000;
        coord->flg        = 0;
        work              = Mem_Calloc(0xC, false);
        if (work == NULL) {
            Task_Kill(task);
            return;
        }
        task->idMap     = (TaskIdMap*)work;
        work->field_0   = 0xC00;
        work->field_4   = 0x4000;
        work->field_8.w = 0x60000;
        task->state++;
    }
    work = (Actor503500EffWork*)task->idMap;
    if (D_801153F4 == 0) {
        if (work->field_8.h.hi < ++task->killCountdown) {
            task->killCountdown = 0;
            Gp_SpawnEff(0x6018C, coord,
                        (work->field_4 & 0xF000) | 0x03800000 | (work->field_0 & 0xFFF), NULL);
        }
    }
    switch (GameFlag_GetNibble(0x12A)) {
        case 0:
        case 1:
            if (Game_Session->field_1 == 0) {
                Task_Kill(task);
                return;
            }
            done = Game_Session->field_5F;
            break;
        case 2:
        case 3:
            if (Game_Session->field_1 != 0) {
                return;
            }
            work->field_4 -= 0x20;
            if (work->field_4 < 0x1000) {
                work->field_4 = 0x1000;
            }
            work->field_0 -= 0x10;
            if (work->field_0 < 0x100) {
                work->field_0 = 0x100;
            }
        case 4:
            work->field_8.w += 0x1000;
            done             = work->field_8.w > 0x100000;
            break;
        default:
            Task_Kill(task);
            return;
    }
    if (done) {
        Task_Kill(task);
    }
}

void func_actor_503500_80132990(Task* task)
{
    TILE*     tile;
    DR_TPAGE* dr;
    u8        r, g, b;

    r = g = b = task->killCountdown;
    if (D_801153F4 == 0) {
        switch (task->state) {
            case 0:
                task->killCountdown = 0xFF;
                task->state++;
                break;
            case 1:
                if (--task->spawnArg1 < 0 || Game_Session->field_5F != 0) {
                    task->state++;
                }
                break;
            case 2:
                task->killCountdown -= 8;
                if (task->killCountdown < 0) {
                    Task_Kill(task);
                }
                break;
            default:
                Task_Kill(task);
                break;
        }
    }
    tile           = (TILE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
    setTile(tile);
    SetSemiTrans(tile, 1);
    tile->x0 = -160;
    tile->y0 = -120;
    tile->w  = 320;
    tile->h  = 240;
    setRGB0(tile, r, g, b);
    addPrim(Gpu_CurrentOt + 3, tile);
    dr             = Gpu_PrimCursor;
    Gpu_PrimCursor = dr + 1;
    setDrawTPage(dr, 1, 0, getTPage(0, 2, 320, 0));
    addPrim(Gpu_CurrentOt + 3, dr);
}

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_2", D_actor_503500_80131E44);
