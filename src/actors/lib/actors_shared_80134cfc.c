#include "common.h"

#include "actors/actors_shared_80134cfc.h"

void ActorsShared80134cfc_Fn33240(ActorShared80134cfc* arg0);
void ActorsShared80134cfc_Fn333B0(ActorShared80134cfc* arg0);
void ActorsShared80134cfc_Fn33830(ActorShared80134cfc* arg0);
void ActorsShared80134cfc_Fn339B0(ActorShared80134cfc* arg0);
void ActorsShared80134cfc_Fn33D94(ActorShared80134cfc* arg0);
void ActorsShared80134cfc_Fn341D4(ActorShared80134cfc* arg0);
void ActorsShared80134cfc_Fn34DB4(ActorShared80134cfc* arg0);

void ActorsShared80134cfc(ActorShared80134cfc* arg0)
{
    switch (arg0->field_1C->field_13C) {
        case 0:
            ActorsShared80134cfc_Fn33240(arg0);
            break;
        case 1:
            ActorsShared80134cfc_Fn333B0(arg0);
            ActorsShared80134cfc_Fn341D4(arg0);
            break;
        case 2:
            ActorsShared80134cfc_Fn33830(arg0);
            ActorsShared80134cfc_Fn341D4(arg0);
            break;
        case 3:
            ActorsShared80134cfc_Fn339B0(arg0);
            ActorsShared80134cfc_Fn341D4(arg0);
            break;
        case 4:
            ActorsShared80134cfc_Fn34DB4(arg0);
            break;
        case 5:
            ActorsShared80134cfc_Fn33D94(arg0);
            break;
    }
}
