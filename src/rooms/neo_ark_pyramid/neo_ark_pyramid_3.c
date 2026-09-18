#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/task.h"

extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

extern s32  D_neo_ark_pyramid_801818A4;
extern void func_neo_ark_pyramid_8017D7F4(s32 arg0);

void func_neo_ark_pyramid_8017DB5C(void)
{
    if (gGameSession->at4.loc.view == 8) {
        func_neo_ark_pyramid_8017D7F4(D_neo_ark_pyramid_801818A4);
    }
}
