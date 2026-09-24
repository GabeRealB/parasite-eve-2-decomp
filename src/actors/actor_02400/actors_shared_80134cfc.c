#include "common.h"

#include "actors/actors_shared_801341d4.h"
#include "actors/actors_shared_80134cfc.h"
#include "actors/actors_shared_80134db4.h"

void Actor02400_Fn01420(ActorShared80134cfc* arg0);
void Actor02400_Fn01590(ActorShared80134cfc* arg0);
void Actor02400_Fn01A10(ActorShared80134cfc* arg0);
void Actor02400_Fn01B90(ActorShared80134cfc* arg0);
void Actor02400_Fn01F74(ActorShared80134cfc* arg0);
void Actor02400_Fn023B4(ActorShared801341d4* arg0);
void Actor02400_Fn02F94(ActorShared80134db4* arg0);

void Actor02400_Fn02EDC(ActorShared80134cfc* arg0)
{
    switch (arg0->field_1C->field_13C) {
        case 0:
            Actor02400_Fn01420(arg0);
            break;
        case 1:
            Actor02400_Fn01590(arg0);
            Actor02400_Fn023B4((ActorShared801341d4*)arg0);
            break;
        case 2:
            Actor02400_Fn01A10(arg0);
            Actor02400_Fn023B4((ActorShared801341d4*)arg0);
            break;
        case 3:
            Actor02400_Fn01B90(arg0);
            Actor02400_Fn023B4((ActorShared801341d4*)arg0);
            break;
        case 4:
            Actor02400_Fn02F94((ActorShared80134db4*)arg0);
            break;
        case 5:
            Actor02400_Fn01F74(arg0);
            break;
    }
}
