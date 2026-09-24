#include "common.h"

typedef struct {
    u8  _pad[0x30];
    s32 field_30;
} ShelterB1PodServiceGantryState;

extern s32 D_80115738;
extern s8  D_shelter_b1_pod_service_gantry_8018256C[];
extern u32 Gp_LcgState;

void func_shelter_b1_pod_service_gantry_8017FA7C(ShelterB1PodServiceGantryState* arg0)
{
    s32 i;

    if (arg0->field_30 == 0) {
        for (i = 0; i < 8; i++) {
            D_shelter_b1_pod_service_gantry_8018256C[i] = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
        }
        D_80115738     = 0x601BE;
        arg0->field_30 = 1;
    }
}
