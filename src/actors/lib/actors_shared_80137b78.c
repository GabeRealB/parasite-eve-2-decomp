#include "common.h"

#include "actors/actors_shared_80137b78.h"

void ActorsShared80137b78_Fn32688(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn329A4(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn3314C(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn33AEC(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn34194(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn347F4(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn3539C(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn354B0(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn35630(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn3592C(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn35A24(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn37D7C(ActorShared80137b78* arg0);
void ActorsShared80137b78_Fn380DC(ActorShared80137b78* arg0);

void ActorsShared80137b78(ActorShared80137b78* arg0)
{
    s16                      temp_v1;
    ActorShared80137b78Work* temp_s1;

    temp_s1 = arg0->field_1C;
    temp_v1 = temp_s1->field_6CC;
    switch (temp_v1) {
        case 0:
            ActorsShared80137b78_Fn329A4(arg0);
            break;
        case 1:
            ActorsShared80137b78_Fn3314C(arg0);
            break;
        case 2:
            ActorsShared80137b78_Fn33AEC(arg0);
            break;
        case 3:
            ActorsShared80137b78_Fn34194(arg0);
            break;
        case 4:
            ActorsShared80137b78_Fn347F4(arg0);
            break;
        case 5:
            ActorsShared80137b78_Fn3539C(arg0);
            break;
        case 6:
            ActorsShared80137b78_Fn354B0(arg0);
            break;
        case 7:
            ActorsShared80137b78_Fn35630(arg0);
            break;
        case 8:
            ActorsShared80137b78_Fn3592C(arg0);
            break;
        case 9:
            ActorsShared80137b78_Fn35A24(arg0);
            break;
        case 10:
            ActorsShared80137b78_Fn37D7C(arg0);
            break;
        case 11:
            ActorsShared80137b78_Fn32688(arg0);
            break;
    }
    if (temp_s1->field_6CC != 1) {
        ActorsShared80137b78_Fn380DC(arg0);
    }
}
