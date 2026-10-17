#pragma once
#include <array>
#include <cstdint>
namespace NHA::Vanilla {
inline constexpr std::uint32_t HunterFaction=0x0004E509;
inline constexpr std::uint32_t WEServicesHunterFaction=0x000AC9C2;
inline constexpr std::uint32_t BanditFaction=0x0001BCC0;
inline constexpr std::uint32_t ForswornFaction=0x00043599;
inline constexpr std::uint32_t CurrentFollowerFaction=0x0005C84E;
inline constexpr std::uint32_t LocTypeCity=0x00013168;
inline constexpr std::uint32_t LocTypeBanditCamp=0x000130DF;
inline constexpr std::uint32_t IronArrow=0x0001397D;
inline constexpr std::uint32_t SteelArrow=0x0001397F;
inline constexpr std::uint32_t FalmerArrow=0x00038341;
inline constexpr std::uint32_t DraugrArrow=0x00034182;
inline constexpr std::uint32_t ForswornArrow=0x000CEE9E;
inline constexpr std::array<std::uint32_t,7> archerClasses{{
    0x00072891, // EncClassThalmorMissile
    0x0006766C, // EncClassAlikrMissile
    0x00043BCD, // EncClassForswornMissile
    0x00015BE7, // EncClassBanditMissile
    0x00023C0D, // EncClassDraugrMissile
    0x00013181, // CombatRanger
    0x0001317D, // CombatScout
}};
inline constexpr std::array<std::uint32_t,69> battleClasses{{
    0x00017008, // EncClassDremoraMelee
    0x0010F71E, // EncClassHorse
    0x0010E714, // GuardOrc2H
    0x0010B1D8, // CWSoldierClass
    0x00106AED, // EncClassBear
    0x000FB0DA, // CombatNightingale
    0x000F2594, // EncClassMammoth
    0x000EDD36, // EncClassHorker
    0x000C969F, // CombatMageNecro
    0x000A1995, // EncClassWerewolfBoss
    0x000A1994, // EncClassWerewolfMage
    0x000A1993, // EncClassWerewolf
    0x000BB4B0, // EncClassMudCrab
    0x000AD237, // EncClassAtronachStorm
    0x000AD236, // EncClassAtronachFrost
    0x000AD235, // EncClassAtronachFlame
    0x000ABB44, // EncClassGiant
    0x000A93B3, // EncClassHagraven
    0x00090359, // EncClassDwarvenSpider
    0x00090358, // EncClassDwarvenSphere
    0x00090356, // EncClassDwarvenCenturion
    0x0007D98D, // EncClassPenitusOculatus
    0x0007289D, // EncClassThalmorMelee
    0x00072891, // EncClassThalmorMissile
    0x00072887, // EncClassThalmorWizard
    0x00073F1F, // EncClassIceWraith
    0x0006766D, // EncClassAlikrWizard
    0x0006766C, // EncClassAlikrMissile
    0x0006766B, // EncClassAlikrMelee
    0x0002E00F, // EncClassVampire
    0x00044CCB, // EncClassFrostbiteSpider
    0x00044CCA, // EncClassChaurus
    0x00043BCD, // EncClassForswornMissile
    0x00043BCC, // EncClassForswornShaman
    0x00043BCB, // EncClassForsworn
    0x00015BE7, // EncClassBanditMissile
    0x00039D30, // EncClassBanditWizard
    0x0002F201, // EncClassDragon
    0x00023C0E, // EncClassDraugrMagic
    0x00023C0D, // EncClassDraugrMissile
    0x00023C0C, // EncClassDraugrMelee
    0x0002A477, // GuardOrc1H
    0x000253F3, // GuardSonsSkyrim
    0x000253F2, // GuardImperial
    0x0001E7D0, // CombatMageDestruction
    0x0001CE1F, // EncClassFalmerShaman
    0x0001CE1E, // EncClassFalmer
    0x0001CE1D, // EncClassAnimalPrey
    0x0001CE1C, // EncClassDragonPriest
    0x0001CE17, // EncClassBanditMelee
    0x0001CE16, // CombatBarbarian
    0x0001CE15, // CombatWarrior2H
    0x0001CE14, // CombatMageConjurer
    0x00013280, // SoldierSonsSkyrimNotGuard
    0x0001327F, // SoldierImperialNotGuard
    0x000131E6, // EncClassAnimalPredator
    0x00013182, // CombatMonk
    0x00013181, // CombatRanger
    0x00013180, // CombatRogue
    0x0001317F, // CombatAssassin
    0x0001317E, // CombatThief
    0x0001317D, // CombatScout
    0x0001317C, // CombatNightblade
    0x0001317B, // CombatMystic
    0x0001317A, // CombatMageElemental
    0x00013179, // CombatSorcerer
    0x00013178, // CombatWitchblade
    0x00013177, // CombatSpellsword
    0x00013176, // CombatWarrior1H
}};
inline constexpr std::array<std::uint32_t,4> guardClasses{{
    0x0010E714, // GuardOrc2H
    0x0002A477, // GuardOrc1H
    0x000253F3, // GuardSonsSkyrim
    0x000253F2, // GuardImperial
}};
inline constexpr std::array<std::uint32_t,1> falmerRaces{{
    0x000131F4, // FalmerRace
}};
inline constexpr std::array<std::uint32_t,6> undeadRaces{{
    0x000F71DC, // DraugrMagicRace
    0x000EBE18, // SkeletonNecroPriestRace
    0x000EB872, // SkeletonNecroRace
    0x000B9FD7, // RigidSkeletonRace
    0x000B7998, // SkeletonRace
    0x00000D53, // DraugrRace
}};
inline constexpr std::uint32_t SteelBolt=0x000BB3; // Dawnguard.esm local ID
inline constexpr std::uint32_t DwarvenBolt=0x00D099; // Dawnguard.esm local ID
}
