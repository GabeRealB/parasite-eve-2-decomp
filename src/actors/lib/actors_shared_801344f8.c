#include "common.h"

#include "actors/actors_shared_801344f8.h"
#include "actors/actors_shared_801346ac.h"

void ActorsShared801344f8_Fn32AC4(void);
void ActorsShared801344f8_Fn32DE4(void);
void ActorsShared801344f8_Fn32FD0(void);
void ActorsShared801344f8_Fn333FC(void);
void ActorsShared801344f8_Fn33528(void);
void ActorsShared801344f8_Fn33EF8(void);
void ActorsShared801344f8_Fn345D0(void);
void ActorsShared801344f8_Fn346D0(void);

void ActorsShared801344f8(ActorShared801344f8* arg0)
{
    switch (arg0->field_1C->field_35A) {
        case 0:
            ActorsShared801344f8_Fn32AC4();
            break;
        case 1:
            ActorsShared801344f8_Fn345D0();
            break;
        case 2:
            ActorsShared801344f8_Fn32DE4();
            break;
        case 3:
            ActorsShared801344f8_Fn32FD0();
            break;
        case 4:
            ActorsShared801346ac((ActorShared801346ac*)arg0);
            break;
        case 5:
            ActorsShared801344f8_Fn333FC();
            break;
        case 6:
            ActorsShared801344f8_Fn346D0();
            break;
        case 7:
            ActorsShared801344f8_Fn33528();
            break;
        case 9:
            ActorsShared801344f8_Fn33EF8();
            break;
    }
}
