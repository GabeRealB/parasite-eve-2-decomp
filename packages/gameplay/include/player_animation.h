#ifndef GAMEPLAY_PLAYER_ANIMATION_H
#define GAMEPLAY_PLAYER_ANIMATION_H

/*
 * The slots of the player's animation banks, by index: what the player's code plays, and what a weapon's bank fills
 * (the editor's player schema names them, engine/animation/player_schema, and a bank's table is written with these
 * names, so the two cannot disagree and still build). Slots 0, 45 and 46 are empty; the 32 after them are the
 * extension rooms copy scripted clips into (`ANIMATION_BANK_EXTENSION_CAPACITY`). Slots no code names are `SLOT_<n>`.
 */
enum {
    PLAYER_ANIMATION_IDLE                     = 1,
    PLAYER_ANIMATION_WALK                     = 2,
    PLAYER_ANIMATION_BACKWARD_WALK            = 3,
    PLAYER_ANIMATION_RUN                      = 4,
    PLAYER_ANIMATION_TURN_LEFT                = 5,
    PLAYER_ANIMATION_TURN_RIGHT               = 6,
    PLAYER_ANIMATION_AIM_ENTER                = 7,
    PLAYER_ANIMATION_AIM_EXIT                 = 8,
    PLAYER_ANIMATION_AIM_IDLE                 = 9,
    PLAYER_ANIMATION_FIRE                     = 10,
    PLAYER_ANIMATION_SECONDARY_FIRE           = 11,
    PLAYER_ANIMATION_AIM_FORWARD              = 12,
    PLAYER_ANIMATION_AIM_BACKWARD             = 13,
    PLAYER_ANIMATION_RELOAD                   = 14,
    PLAYER_ANIMATION_SECONDARY_RELOAD         = 15,
    PLAYER_ANIMATION_HIT_HEAD                 = 16,
    PLAYER_ANIMATION_HIT_BODY                 = 17,
    PLAYER_ANIMATION_DEATH                    = 18,
    PLAYER_ANIMATION_UNARMED_WALK             = 19,
    PLAYER_ANIMATION_RELOAD_BATTLE_END        = 20,
    PLAYER_ANIMATION_SLOT_21                  = 21,
    PLAYER_ANIMATION_SLOT_22                  = 22,
    PLAYER_ANIMATION_HIGH_HEALTH_IDLE         = 23,
    PLAYER_ANIMATION_MID_HEALTH_IDLE          = 24,
    PLAYER_ANIMATION_LOW_HEALTH_IDLE          = 25,
    PLAYER_ANIMATION_PARASITE_ENERGY_BEGIN_0  = 26,
    PLAYER_ANIMATION_PARASITE_ENERGY_CHARGE_0 = 27,
    PLAYER_ANIMATION_PARASITE_ENERGY_CAST_0   = 28,
    PLAYER_ANIMATION_PARASITE_ENERGY_BEGIN_1  = 29,
    PLAYER_ANIMATION_PARASITE_ENERGY_CHARGE_1 = 30,
    PLAYER_ANIMATION_PARASITE_ENERGY_CAST_1   = 31,
    PLAYER_ANIMATION_EXAMINE_ENTER            = 32,
    PLAYER_ANIMATION_EXAMINE_EXIT             = 33,
    PLAYER_ANIMATION_SLOT_34                  = 34,
    PLAYER_ANIMATION_SLOT_35                  = 35,
    PLAYER_ANIMATION_STAIR_ASCENT             = 36,
    PLAYER_ANIMATION_STAIR_DESCENT            = 37,
    PLAYER_ANIMATION_STAIR_ASCENT_FINISH_EVEN = 38,
    PLAYER_ANIMATION_STAIR_ASCENT_FINISH_ODD  = 39,
    PLAYER_ANIMATION_ITEM_USE                 = 40,
    PLAYER_ANIMATION_SLOT_41                  = 41,
    PLAYER_ANIMATION_PARASITE_ENERGY_BEGIN_2  = 42,
    PLAYER_ANIMATION_PARASITE_ENERGY_CHARGE_2 = 43,
    PLAYER_ANIMATION_PARASITE_ENERGY_CAST_2   = 44,
};

#endif // GAMEPLAY_PLAYER_ANIMATION_H
