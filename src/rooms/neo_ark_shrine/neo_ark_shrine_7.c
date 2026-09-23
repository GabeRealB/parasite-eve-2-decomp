#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32     D_8011572C;
extern s32     D_80115750;
extern s32     D_80115758;
extern SVECTOR D_neo_ark_shrine_8018268C[];
extern SVECTOR D_neo_ark_shrine_80182694[];
extern SVECTOR D_neo_ark_shrine_8018269C[];
extern SVECTOR D_neo_ark_shrine_801826AC[];
extern SVECTOR D_neo_ark_shrine_801826C4[];
extern SVECTOR D_neo_ark_shrine_801826D4[];
extern SVECTOR D_neo_ark_shrine_80182704[];

void func_neo_ark_shrine_8017FC14(SVECTOR* pos, s32 arg1, s32 arg2);

/// Tail every `NeoArkShrineFall` handler runs: clears the prop's root coordinate
/// flag, rebuilds its world matrix, and republishes the translation in
/// `func_800D7A9C`'s format, lowered by 0x320 so the prop draws on the floor.
void func_neo_ark_shrine_8017F86C(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj        = task->extra;
    coord      = obj->coords;
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
}

/// On the task's first tick stores three ids (0x601DF, 0x601FB, 0x60217) into
/// the `D_80115758` / `D_8011572C` / `D_80115750` slots; then, every tick, runs
/// `func_neo_ark_shrine_8017FC14` over the positions the current camera view
/// shows, drawn from one of the room's `SVECTOR` arrays.
void func_neo_ark_shrine_8017F8DC(Task* task)
{
    if (task->state == 0) {
        D_80115758  = 0x601DF;
        D_8011572C  = 0x601FB;
        D_80115750  = 0x60217;
        task->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p = D_neo_ark_shrine_801826D4;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            break;
        }
        case 3: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[6], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[8], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[9], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[10], 1, 0x300);
            break;
        }
        case 4: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            break;
        }
        case 5:
        case 18: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            break;
        }
        case 6: {
            SVECTOR* p = D_neo_ark_shrine_8018269C;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[6], 1, 0x300);
            break;
        }
        case 7: {
            SVECTOR* p = D_neo_ark_shrine_8018268C;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            break;
        }
        case 12: {
            SVECTOR* p = D_neo_ark_shrine_80182694;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[6], 1, 0x300);
            break;
        }
        case 14: {
            SVECTOR* p = D_neo_ark_shrine_801826C4;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            break;
        }
        case 16: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            break;
        }
        case 10:
        case 17: {
            SVECTOR* p = D_neo_ark_shrine_80182704;
            func_neo_ark_shrine_8017FC14(&p[0], 0, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 0, 0x300);
            break;
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_7", func_neo_ark_shrine_8017FC14);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_7", D_neo_ark_shrine_8017D6A4);
