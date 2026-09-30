#include "attachments.h"

#include <psyq/rand.h>

#include "types.h"

#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/player_state.h"
#include "gameplay/weapon_data.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

/// Draws one of the prompt's button labels on line `line`, `dx` pixels right of
/// the prompt's left edge.
#define DRAW_PROMPT_LABEL(req, dx, line, color, str)                      \
    {                                                                     \
        req.x          = obj.panel.field_20.unsignedValue + (dx) + xBase; \
        req.y          = (obj.panel.field_22.unsignedValue + 9) + (line); \
        req.otIndex    = obj.panel.field_14.signedValue + 1;              \
        req.field_8    = (color);                                         \
        req.glyphTable = 5;                                               \
        req.centerMode = 0;                                               \
        req.field_E    = 1;                                               \
        Text_DrawString(&req, (str));                                     \
    }

/// Draws a quantity right-aligned on line `line`; an empty count sets `flag`.
#define DRAW_PROMPT_COUNT(req, line, count)                               \
    {                                                                     \
        req.field_8    = 0x606060;                                        \
        req.glyphTable = 5;                                               \
        req.centerMode = 2;                                               \
        req.field_E    = 0;                                               \
        req.x          = obj.panel.field_20.unsignedValue + 0x94;         \
        req.y          = (obj.panel.field_22.unsignedValue + 9) + (line); \
        req.otIndex    = obj.panel.field_14.signedValue + 1;              \
        Text_DrawString(&req, Text_ItoaSigned(buf, (count)));             \
        if ((count) == 0) {                                               \
            flag = 1;                                                     \
        }                                                                 \
    }

#include "main/wipsys.h"
#include <psyq/rand.h>

s32 D_80114F28;

/// Inline copy of `Gp_IsStateF0Active`.
static __inline__ s32 isStateF0Active_(void);

#undef DRAW_PROMPT_LABEL
#undef DRAW_PROMPT_COUNT

/// Inline copy of `Gp_IsStateF0Active`.
static __inline__ s32 isStateF0Active_(void)
{
    GpStateF0* p;

    p = &Gp_StateF0;
    if ((p->prefix.bytes.field_0 == 1 && p->field_6 != 0) || p->prefix.bytes.field_1 != 0) {
        return 1;
    }
    return 0;
}

void Gp_UpdateAttachCombo(s32 arg0)
{
    PlayerStatus* cfg;

    if (arg0 == 0) {
        D_80114F28 = 1;
        return;
    }

    cfg = &Player_Status;
    switch (Gp_StateC08.field_0) {
        case 411:
        case 412:
        case 413: {
            GpItemRec8* rec;
            s32         lvl;
            s32         count;
            s32         time;

            lvl                  = Gp_StateC08.field_0 % 10;
            rec                  = &Gp_AttachParams[27 + lvl].combo;
            count                = Gp_StateC08.field_C & 0xF;
            time                 = rec->field_6;
            Gp_StateC08.field_C  = count;
            Gp_StateC08.field_10 = time;
            if (Gp_StateC08.field_C < 2) {
                Gp_StateC08.field_C++;
            }
            Gp_StateC08.field_C |= lvl << 4;
            break;
        }
        case 421:
        case 422:
        case 423: {
            GpItemRec8* rec;
            s32         lvl;
            s32         count;
            s32         time;

            lvl                  = Gp_StateC08.field_0 % 10;
            rec                  = &Gp_AttachParams[30 + lvl].combo;
            count                = Gp_StateC08.field_D & 0xF;
            time                 = rec->field_6;
            Gp_StateC08.field_D  = count;
            Gp_StateC08.field_12 = time;
            if (Gp_StateC08.field_D < 2) {
                Gp_StateC08.field_D++;
            }
            Gp_StateC08.field_D |= lvl << 4;
            break;
        }
        case 311:
        case 312:
        case 313: {
            GpItemRec8* rec;
            s32         lvl;
            s32         count;
            s32         time;

            lvl                  = Gp_StateC08.field_0 % 10;
            rec                  = &Gp_AttachParams[18 + lvl].combo;
            count                = Gp_StateC08.field_F & 0xF;
            time                 = rec->field_6;
            Gp_StateC08.field_F  = count;
            Gp_StateC08.field_14 = time;
            if (Gp_StateC08.field_F == 0) {
                Gp_StateC08.field_F++;
            }
            Gp_StateC08.field_F |= lvl << 4;
            Gp_TriggerPeState(1, 0xFF);
            break;
        }
        case 321:
        case 322:
        case 323: {
            GpRec16* params;
            s32      row;
            s32      min;
            s32      max;
            s32      heal;

            /* The parameter rows attach 7 uses at levels 1 to 3. */
            params = Gp_IdParamHi.rows;
            row    = 7 * 3 + 1 + Gp_StateC08.field_0 % 3;
            max    = params[row].field[5];
            min    = params[row].field[4];
            if (min >= max || !isStateF0Active_()) {
                heal = min;
            } else {
                /* A random blend between the row's two amounts. */
                heal = (rand() & 0xFF) + 1;
                heal = (max * heal + min * (0x100 - heal)) >> 8;
                if (heal <= 0) {
                    heal = 1;
                }
            }
            cfg->hp += heal;
            if (cfg->hpMax < cfg->hp) {
                cfg->hp = cfg->hpMax;
            }
            break;
        }
    }
}
