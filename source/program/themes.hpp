
#include "lib.hpp"
#include "themes.hpp"
#include <nn/hid.h>

// SMM2 v3.0.3 (01009B90006DC000) Function Offsets
namespace Offsets {
    constexpr uintptr_t PlayerCalcMove    = 0x008A4210; // Player movement & physics step
    constexpr uintptr_t EnemyCalcAI       = 0x007B2940; // Base enemy AI update loop
    constexpr uintptr_t ThemeWheelGetName = 0x00C14890; // Course Maker theme wheel text
    constexpr uintptr_t EnvColorShader    = 0x00A18450; // Stage ambient RGB color multiplier
    constexpr uintptr_t ProjectileSpawn   = 0x00941120; // Fireball / Superball spawn velocity
}

static CrossoverTheme g_CurrentTheme = CrossoverTheme::Valorant;
static ValorantAgent  g_ActiveAgent  = ValorantAgent::Jett;
static bool g_Noclip = false;
static int  g_AbilityCooldown = 0;

// 1. VALORANT AGENTS + CROSSOVER CHARACTER PHYSICS + NOCLIP
HOOK_DEFINE_TRAMPOLINE(PlayerMoveHook) {
    static void Callback(uintptr_t playerActor) {
        nn::hid::NpadFullKeyState state{};
        nn::hid::GetNpadState(&state, 0);

        if (g_AbilityCooldown > 0) g_AbilityCooldown--;

        // Toggle Noclip with L + R + D-Pad Up
        static bool lastNoclipCombo = false;
        bool noclipCombo = (state.mButtons & nn::hid::NpadButton::L) &&
                           (state.mButtons & nn::hid::NpadButton::R) &&
                           (state.mButtons & nn::hid::NpadButton::Up);
        if (noclipCombo && !lastNoclipCombo) g_Noclip = !g_Noclip;
        lastNoclipCombo = noclipCombo;

        // Switch Valorant Agent with L + D-Pad Right
        static bool lastSwitchCombo = false;
        bool switchCombo = (state.mButtons & nn::hid::NpadButton::L) &&
                           (state.mButtons & nn::hid::NpadButton::Right);
        if (switchCombo && !lastSwitchCombo) {
            int next = (static_cast<int>(g_ActiveAgent) + 1) % 4;
            g_ActiveAgent = static_cast<ValorantAgent>(next);
        }
        lastSwitchCombo = switchCombo;

        float* posX = reinterpret_cast<float*>(playerActor + 0x110);
        float* posY = reinterpret_cast<float*>(playerActor + 0x114);
        float* velX = reinterpret_cast<float*>(playerActor + 0x120);
        float* velY = reinterpret_cast<float*>(playerActor + 0x124);

        if (g_Noclip) {
            *velX = 0.0f;
            *velY = 0.0f;
            *posX += (static_cast<float>(state.mAnalogStickL.mX) / 32767.0f) * 6.5f;
            *posY += (static_cast<float>(state.mAnalogStickL.mY) / 32767.0f) * 6.5f;
            return;
        }

        Orig(playerActor);

        const ThemeRules& rules = kThemeTable[static_cast<int>(g_CurrentTheme)];

        if (rules.autoRunner) {
            *velX = 5.2f * rules.speedMultiplier;
        } else {
            *velX *= rules.speedMultiplier;
        }

        // VALORANT AGENT ABILITIES
        if (g_CurrentTheme == CrossoverTheme::Valorant) {
            bool abilityTrigger = (state.mButtons & nn::hid::NpadButton::ZL) &&
                                  (state.mButtons & nn::hid::NpadButton::Y);

            switch (g_ActiveAgent) {
                case ValorantAgent::Jett:
                    // Jett Passive: Hold B/A while falling to glide slowly
                    if ((state.mButtons & nn::hid::NpadButton::B) && *velY < -1.2f) {
                        *velY = -1.2f;
                    }
                    // Jett E (Tailwind Dash): ZL + Y
                    if (abilityTrigger && g_AbilityCooldown == 0) {
                        *velX *= 3.2f;
                        g_AbilityCooldown = 45;
                    }
                    // Jett Q (Updraft): ZL + X
                    if ((state.mButtons & nn::hid::NpadButton::ZL) &&
                        (state.mButtons & nn::hid::NpadButton::X) && g_AbilityCooldown == 0) {
                        *velY = 9.5f;
                        g_AbilityCooldown = 45;
                    }
                    break;

                case ValorantAgent::Raze:
                    // Raze Q (Blast Pack Satchel Launch): ZL + Y
                    if (abilityTrigger && g_AbilityCooldown == 0) {
                        *velX *= 2.5f;
                        *velY = 8.5f;
                        g_AbilityCooldown = 50;
                    }
                    break;

                case ValorantAgent::Phoenix:
                    // Phoenix Passive: Faster combat sprint speed
                    *velX *= 1.15f;
                    break;

                case ValorantAgent::Omen:
                    // Omen C (Shrouded Step Teleport): ZL + Y teleports 64 units (4 blocks)
                    if (abilityTrigger && g_AbilityCooldown == 0) {
                        float dir = (*velX >= 0.0f) ? 64.0f : -64.0f;
                        *posX += dir;
                        g_AbilityCooldown = 60;
                    }
                    break;
            }
        }
    }
};

// 2. VALORANT WEAPON / ABILITY PROJECTILES (Vandal Rifle Tracers & Raze Paint Shells)
HOOK_DEFINE_TRAMPOLINE(ProjectileHook) {
    static void Callback(uintptr_t projActor, float speedX, float speedY) {
        if (g_CurrentTheme == CrossoverTheme::Valorant) {
            if (g_ActiveAgent == ValorantAgent::Raze) {
                // Raze Paint Shells: Fast arcing explosive trajectory
                Orig(projActor, speedX * 2.4f, speedY * 1.4f);
            } else {
                // Vandal / Sheriff Bullet Tracer: Ultra-fast straight line
                Orig(projActor, speedX * 4.0f, 0.0f);
            }
        } else {
            Orig(projActor, speedX, speedY);
        }
    }
};

// 3. ENEMY AI HOOK (Valorant Training Bots, FNAF, Geometry Dash, League)
HOOK_DEFINE_TRAMPOLINE(EnemyAIHook) {
    static void Callback(uintptr_t enemyActor) {
        Orig(enemyActor);

        float* enemyVelX = reinterpret_cast<float*>(enemyActor + 0x120);
        float* enemyVelY = reinterpret_cast<float*>(enemyActor + 0x124);

        if (g_CurrentTheme == CrossoverTheme::Valorant) {
            // Valorant Spike Bots / Turrets patrol crisply and react faster
            *enemyVelX *= 1.30f;
        } else if (g_CurrentTheme == CrossoverTheme::FNAF) {
            *enemyVelX *= 1.75f;
        } else if (g_CurrentTheme == CrossoverTheme::GeometryDash) {
            *enemyVelX = 0.0f;
            *enemyVelY = 0.0f;
        }
    }
};

// 4. AGENT & THEME ATMOSPHERE SHADER TINT
HOOK_DEFINE_TRAMPOLINE(EnvColorHook) {
    static void Callback(uintptr_t envObj, float* rgbOut) {
        Orig(envObj, rgbOut);

        if (g_CurrentTheme == CrossoverTheme::Valorant) {
            switch (g_ActiveAgent) {
                case ValorantAgent::Jett:
                    rgbOut[0] *= 0.80f; rgbOut[1] *= 1.15f; rgbOut[2] *= 1.30f; // Wind Cyan
                    break;
                case ValorantAgent::Raze:
                    rgbOut[0] *= 1.30f; rgbOut[1] *= 0.95f; rgbOut[2] *= 0.65f; // Boom Orange
                    break;
                case ValorantAgent::Phoenix:
                    rgbOut[0] *= 1.35f; rgbOut[1] *= 0.75f; rgbOut[2] *= 0.60f; // Blaze Fire Red
                    break;
                case ValorantAgent::Omen:
                    rgbOut[0] *= 0.65f; rgbOut[1] *= 0.55f; rgbOut[2] *= 1.25f; // Shadow Violet
                    break;
            }
        } else if (g_CurrentTheme == CrossoverTheme::FNAF) {
            rgbOut[0] *= 0.18f; rgbOut[1] *= 0.16f; rgbOut[2] *= 0.22f;
        } else if (g_CurrentTheme == CrossoverTheme::GeometryDash) {
            rgbOut[0] *= 1.25f; rgbOut[1] *= 0.65f; rgbOut[2] *= 1.35f;
        }
    }
};

// 5. COURSE MAKER UI THEME WHEEL LABELS
HOOK_DEFINE_TRAMPOLINE(ThemeWheelNameHook) {
    static const char16_t* Callback(int themeId) {
        if (themeId >= 0 && themeId < 8) {
            g_CurrentTheme = static_cast<CrossoverTheme>(themeId);
            return kThemeTable[themeId].displayName;
        }
        return Orig(themeId);
    }
};

extern "C" void exl_main(void* x0, void* x1) {
    exl::hook::Initialize();

    PlayerMoveHook::InstallAtOffset(Offsets::PlayerCalcMove);
    ProjectileHook::InstallAtOffset(Offsets::ProjectileSpawn);
    EnemyAIHook::InstallAtOffset(Offsets::EnemyCalcAI);
    EnvColorHook::InstallAtOffset(Offsets::EnvColorShader);
    ThemeWheelNameHook::InstallAtOffset(Offsets::ThemeWheelGetName);
}
