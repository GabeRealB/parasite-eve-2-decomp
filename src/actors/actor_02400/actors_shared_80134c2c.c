#include "common.h"

#include "actors/actor_102400_fn0208c.h"
#include "actors/actors_shared_80134084.h"
#include "actors/actors_shared_80134c2c.h"
#include "actors/actors_shared_80134cfc.h"
#include "actors/actors_shared_80134eb8.h"
#include "actors/actors_shared_80134f60.h"
#include "actors/actors_shared_80134ff0.h"
#include "actors/actors_shared_80135048.h"

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Actor02400_Fn02EDC(ActorShared80134cfc* arg0);
void Actor02400_Fn02264(ActorShared80134084* arg0);
void Actor02400_Fn03140(ActorShared80134f60* arg0);
void Actor02400_Fn03098(ActorShared80134eb8* arg0);
void Actor02400_Fn031D0(ActorShared80134ff0* arg0);
void Actor02400_Fn03228(ActorShared80135048* arg0);
void Actor02400_Fn00C08(ActorShared80134c2c* arg0, ActorShared80134c2cObj2C* arg1, s32 arg2);

extern u8 D_801153F4;

void Actor02400_Fn02E0C(ActorShared80134c2cCtx* arg0, ActorShared80134c2c* arg1)
{
    GsCOORDINATE2*            temp_s1;
    ActorShared80134c2cObj2C* temp_a1;
    s32                       state;
    s32                       one;

    temp_a1 = arg1->field_2C;
    temp_s1 = temp_a1->field_8;
    state   = D_801153F4;
    one     = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    temp_a1->field_C = 0;
    arg0->field_14   = 0;
    goto default_body;
case2:
    temp_a1->field_C = 0x80;
    arg0->field_14   = one;
    return;
default_body:
    Actor02400_Fn00C08(arg1, temp_a1, one);
    Actor02400_Fn02EDC((ActorShared80134cfc*)arg1);
    Actor02400_Fn02264((ActorShared80134084*)arg1);
    Actor02400_Fn03140((ActorShared80134f60*)arg1);
    Actor02400_Fn03098((ActorShared80134eb8*)arg1);
    Actor02400_Fn0208C((Actor02400Scale*)arg1);
    temp_s1->flg = 0;
    Gp_UpdateCoord(temp_s1);
case1:
    Actor02400_Fn031D0((ActorShared80134ff0*)arg1);
    Actor02400_Fn03228((ActorShared80135048*)arg1);
}
