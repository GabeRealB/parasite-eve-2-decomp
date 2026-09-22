#include "common.h"

#include "main/wipsys.h"

#include "actors/actors_shared_80132d78.h"

s32 ActorsShared80132d78(ActorShared80132d78* arg0)
{
    ActorShared80132d78Work* work;
    s16                      count;
    s32                      i;

    work  = arg0->field_1C;
    count = work->field_6FA;
    for (i = 0; i < count; i++) {
        if (work->field_6B4[i].field_0 == 1) {
            if ((work->field_6B4[i].field_8 < Player_Status.coordMtx->t[0]) &&
                (Player_Status.coordMtx->t[0] < work->field_6B4[i].field_C)) {
                if ((Player_Status.coordMtx->t[2] < work->field_6B4[i].field_A) &&
                    (work->field_6B4[i].field_E < Player_Status.coordMtx->t[2])) {
                    work->field_708 = i;
                    return 1;
                }
            }
        }
    }
    return 0;
}
