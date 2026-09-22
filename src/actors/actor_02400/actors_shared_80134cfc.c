#include "common.h"

#include "actors/actors_shared_801341d4.h"
#include "actors/actors_shared_80134cfc.h"
#include "actors/actors_shared_80134db4.h"

void ActorsShared80133240(ActorShared80134cfc* arg0);
void ActorsShared801333b0(ActorShared80134cfc* arg0);
void ActorsShared80133830(ActorShared80134cfc* arg0);
void ActorsShared80134cfc_Fn339B0(ActorShared80134cfc* arg0);
void ActorsShared80133d94(ActorShared80134cfc* arg0);

void ActorsShared80134cfc(ActorShared80134cfc* arg0)
{
    switch (arg0->field_1C->field_13C) {
        case 0:
            ActorsShared80133240(arg0);
            break;
        case 1:
            ActorsShared801333b0(arg0);
            ActorsShared801341d4((ActorShared801341d4*)arg0);
            break;
        case 2:
            ActorsShared80133830(arg0);
            ActorsShared801341d4((ActorShared801341d4*)arg0);
            break;
        case 3:
            ActorsShared80134cfc_Fn339B0(arg0);
            ActorsShared801341d4((ActorShared801341d4*)arg0);
            break;
        case 4:
            ActorsShared80134db4((ActorShared80134db4*)arg0);
            break;
        case 5:
            ActorsShared80133d94(arg0);
            break;
    }
}
