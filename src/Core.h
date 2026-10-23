#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <unordered_map>
#include <tuple>

namespace NHA {
enum class Kind : std::uint8_t { Arrow, Bolt };
enum class Family : std::uint8_t { General, Falmer, AncientNord, Forsworn, Custom };
struct Context { bool city=false,guard=false,banditCamp=false,follower=false,battle=false; };
struct Random {
    std::uint64_t state;
    std::uint64_t next() {
        auto z=(state+=0x9e3779b97f4a7c15ULL);
        z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;
        z=(z^(z>>27))*0x94d049bb133111ebULL;
        return z^(z>>31);
    }
    int between(int lo,int hi) { return lo+static_cast<int>(next()%static_cast<std::uint64_t>(hi-lo+1)); }
};
struct Channel {
    int cap=0;
    int defaultCap=0,combatCap=0;
    bool initialized=false,previousCity=false,cityCredit=false;
    double due=-1,interval=0,jitter=0;
};
struct State {
    std::uint32_t actor=0,base=0;
    std::uint64_t random=0;
    std::array<Channel,2> channels;
    std::unordered_map<std::uint32_t,int> gifts;
};
inline double intervalDays(const Context& c,double jitter) {
    double days=c.guard ? (c.city ? 0.5 : 1.0) : c.banditCamp ? 1.0 : c.city ? 1.0+0.5*jitter : 2.0+jitter;
    return c.follower ? days*2.0 : days;
}
inline bool synchronizeCap(Channel& channel,const Context& context,Random& rng) {
    const int old=channel.cap;
    // Default follows the native occupation, even if first seen following.
    if (!channel.defaultCap) channel.defaultCap=channel.cap ? channel.cap : rng.between(context.battle ? 40 : 18,context.battle ? 60 : 24);
    if (!channel.combatCap && channel.defaultCap>=40) channel.combatCap=channel.defaultCap;
    if (context.follower && !channel.combatCap) channel.combatCap=rng.between(40,60);
    channel.cap=context.follower ? channel.combatCap : channel.defaultCap;
    return channel.cap!=old;
}
inline void schedule(Channel& channel,const Context& context,double now,Random& rng) {
    synchronizeCap(channel,context,rng);
    channel.jitter=rng.between(0,1000000)/1000000.0;
    channel.interval=intervalDays(context,channel.jitter);
    channel.due=now+channel.interval;
    channel.previousCity=context.city;
    channel.cityCredit=false;
    channel.initialized=true;
}
inline bool advanceTimer(Channel& channel,const Context& context,double now) {
    if (!channel.initialized || !std::isfinite(now) || now<0) return false;
    const double duration=intervalDays(context,channel.jitter);
    // Keep completed progress when location or follower speed changes. An
    // already-due timer is never pushed into the future by leaving a city.
    if (channel.due>now && channel.interval>0 && duration!=channel.interval)
        channel.due=now+(channel.due-now)*duration/channel.interval;
    channel.interval=duration;
    if (context.city && !channel.previousCity && !channel.cityCredit) {
        channel.due-=1.0;
        channel.cityCredit=true;
    }
    channel.previousCity=context.city;
    return channel.due<=now;
}
inline int refillCount(const Channel& channel,int current,bool initial) {
    const auto deficit=std::max<std::int64_t>(0,static_cast<std::int64_t>(channel.cap)-std::max(0,current));
    return static_cast<int>(std::min<std::int64_t>(deficit,initial ? channel.cap : channel.cap/2));
}
inline void resetInventoryCycle(State& state) {
    for (auto& channel:state.channels) { const int cap=channel.cap,normal=channel.defaultCap,combat=channel.combatCap;channel={};channel.cap=cap;channel.defaultCap=normal;channel.combatCap=combat; }
    // Keep conservative gift protection; the next live inventory snapshot
    // clamps quantities, including after partial resets that preserve gifts.
}
inline int nativeCount(int total,int protectedCount) { return std::max(0,total-std::max(0,protectedCount)); }
inline void protectGift(State& state,std::uint32_t form,int count,int held) {
    if (!form || count<=0 || held<=0) return;
    const auto old=state.gifts.contains(form) ? state.gifts.at(form) : 0;
    state.gifts[form]=static_cast<int>(std::min<std::int64_t>(held,static_cast<std::int64_t>(old)+count));
}
inline void reconcileGift(State& state,std::uint32_t form,int held) {
    auto found=state.gifts.find(form);
    if (found==state.gifts.end()) return;
    found->second=std::min(found->second,std::max(0,held));
    if (!found->second) state.gifts.erase(found);
}
struct Candidate {
    std::uint32_t form;
    Kind kind;
    int value;
    float damage;
    Family family;
    unsigned priority=0; // Native list < carried stock < currently equipped carried stock.
    int held=0;
};
inline bool familyAllowed(Family actor,Family ammo,Kind kind) {
    if (ammo==Family::Custom) return true; // Supplied by this actor's own inventory/list.
    if (kind==Kind::Bolt) return ammo==Family::General;
    return actor==ammo;
}
inline std::optional<Candidate> chooseAmmo(std::span<const Candidate> pool,Kind kind,Family family) {
    std::optional<Candidate> chosen;
    for (const auto& candidate:pool) {
        if (!candidate.form || candidate.kind!=kind || !std::isfinite(candidate.damage) || candidate.damage<=0 || (!candidate.priority && !familyAllowed(family,candidate.family,kind))) continue;
        const auto rank=[](const Candidate& c) {return std::tuple{c.priority,c.held,-static_cast<std::int64_t>(c.value),c.damage,std::numeric_limits<std::uint32_t>::max()-c.form};};
        if (!chosen || rank(candidate)>rank(*chosen)) chosen=candidate;
    }
    return chosen;
}
}
