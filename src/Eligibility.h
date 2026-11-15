#pragma once
#include "Vanilla.h"
#include <algorithm>
namespace NHA {
inline bool automatonExcluded(std::uint32_t race,bool dwarvenKeyword){return dwarvenKeyword||std::find(Vanilla::automatonRaces.begin(),Vanilla::automatonRaces.end(),race)!=Vanilla::automatonRaces.end();}
inline bool rangedFormEligible(std::uint32_t form,bool playable){return playable&&std::find(Vanilla::creatureRangedWeapons.begin(),Vanilla::creatureRangedWeapons.end(),form)==Vanilla::creatureRangedWeapons.end();}
// Legacy saves have no per-addition provenance. Bound recovery to surplus
// inventory deltas and the previous initialized cap, preserving tracked gifts.
inline int sphereRecoveryCount(int held,int positiveDelta,int protectedGifts,int oldCap){
 if(held<=0||positiveDelta<=0||oldCap<=0)return 0;
 const int surplus=std::min(held,positiveDelta);
 return std::min(oldCap,std::max(0,surplus-std::max(0,protectedGifts)));
}
}
