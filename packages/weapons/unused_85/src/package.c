#include "unused_85.h"

#include <core/pe2.h>

#include <gameplay/player_animation.h>

#include <weapons/weapon/weapon.h>

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(model, model)
PE2_BIND_ARG(model, _symbol, gUnused85HeldWeaponModel)
PE2_VALUE(model)

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
PE2_BIND_ARG(bank, _symbol, gUnused85PlayerAnimationBank)
PE2_VALUE(bank)
