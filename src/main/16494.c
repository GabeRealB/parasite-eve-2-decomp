#include "common.h"

#include <psyq/libspu.h>

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/16494", func_80025C94);

INCLUDE_ASM("main/nonmatchings/16494", func_80025DD8);

void F16494_ResetSpuAttr(void)
{
    // Set all attributes.
    D5B498_SpuAttr.mask = 0;

    // Set the master volume to maximum.
    D5B498_SpuAttr.mvol.left      = 0x3FFF;
    D5B498_SpuAttr.mvol.right     = 0x3FFF;
    D5B498_SpuAttr.mvolmode.left  = SPU_VOICE_DIRECT;
    D5B498_SpuAttr.mvolmode.right = SPU_VOICE_DIRECT;

    // Set the CD Input volume to the minimum.
    D5B498_SpuAttr.cd.volume.left  = 0;
    D5B498_SpuAttr.cd.volume.right = 0;
    D5B498_SpuAttr.cd.reverb       = SPU_OFF;
    D5B498_SpuAttr.cd.mix          = SPU_ON;

    // Disable External Digital Input.
    D5B498_SpuAttr.ext.volume.left  = 0;
    D5B498_SpuAttr.ext.volume.right = 0;
    D5B498_SpuAttr.ext.reverb       = SPU_OFF;
    D5B498_SpuAttr.ext.mix          = SPU_OFF;

    // Apply the settings.
    SpuSetCommonAttr(&D5B498_SpuAttr);
    D5B498_8006EBF0 = 0;
}

INCLUDE_ASM("main/nonmatchings/16494", func_800260B0);

INCLUDE_ASM("main/nonmatchings/16494", func_80026138);

INCLUDE_ASM("main/nonmatchings/16494", func_80026148);

INCLUDE_ASM("main/nonmatchings/16494", func_80026178);

INCLUDE_ASM("main/nonmatchings/16494", func_800261C8);

INCLUDE_ASM("main/nonmatchings/16494", func_800261D4);

INCLUDE_ASM("main/nonmatchings/16494", func_800261F4);

INCLUDE_ASM("main/nonmatchings/16494", func_80026218);

INCLUDE_ASM("main/nonmatchings/16494", func_80026268);

INCLUDE_ASM("main/nonmatchings/16494", func_800262A8);
