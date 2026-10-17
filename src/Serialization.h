#pragma once
#include "Core.h"
#include <bit>
#include <vector>

namespace NHA::Save {
inline constexpr std::uint32_t schema=2,maxActors=100000,maxGifts=4096;
struct Data { std::uint64_t seed=0;std::unordered_map<std::uint32_t,State> actors; };
struct Writer {
    std::vector<std::uint8_t> bytes;
    template<class T> void number(T value) { for (std::size_t i=0;i<sizeof(T);++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i))); }
    void real(double value) { number(std::bit_cast<std::uint64_t>(value)); }
};
struct Reader {
    std::span<const std::uint8_t> bytes;std::size_t offset=0;bool valid=true;
    template<class T> T number() {
        if (offset+sizeof(T)>bytes.size()) {valid=false;return 0;}
        T value=0;for (std::size_t i=0;i<sizeof(T);++i) value|=static_cast<T>(bytes[offset++])<<(8*i);return value;
    }
    double real() {return std::bit_cast<double>(number<std::uint64_t>());}
};
inline std::vector<std::uint8_t> encode(const Data& data) {
    Writer out;out.number(data.seed);out.number(static_cast<std::uint32_t>(data.actors.size()));
    std::vector<std::uint32_t> keys;for (auto& [id,state]:data.actors) keys.push_back(id);std::sort(keys.begin(),keys.end());
    for (auto id:keys) {
        const auto& state=data.actors.at(id);out.number(state.actor);out.number(state.base);out.number(state.random);
        for (const auto& c:state.channels) {
            out.number(static_cast<std::uint8_t>(c.cap));out.number(static_cast<std::uint8_t>(c.initialized));
            out.number(static_cast<std::uint8_t>(c.previousCity));out.number(static_cast<std::uint8_t>(c.cityCredit));
            out.real(c.due);out.real(c.interval);out.real(c.jitter);
            out.number(static_cast<std::uint8_t>(c.defaultCap));out.number(static_cast<std::uint8_t>(c.combatCap));
        }
        std::vector<std::uint32_t> forms;for (auto& [form,count]:state.gifts) if (count>0) forms.push_back(form);
        std::sort(forms.begin(),forms.end());out.number(static_cast<std::uint32_t>(forms.size()));
        for (auto form:forms) {out.number(form);out.number(static_cast<std::uint32_t>(state.gifts.at(form)));}
    }
    return out.bytes;
}
inline std::optional<Data> decode(std::uint32_t version,std::span<const std::uint8_t> bytes) {
    if (version!=1 && version!=schema) return std::nullopt;
    Reader in{bytes};Data data;data.seed=in.number<std::uint64_t>();const auto count=in.number<std::uint32_t>();
    if (!in.valid || count>maxActors) return std::nullopt;
    for (std::uint32_t i=0;i<count;++i) {
        State state;state.actor=in.number<std::uint32_t>();state.base=in.number<std::uint32_t>();state.random=in.number<std::uint64_t>();
        if (!state.actor || !state.base) return std::nullopt;
        for (auto& c:state.channels) {
            c.cap=in.number<std::uint8_t>();const auto initialized=in.number<std::uint8_t>(),city=in.number<std::uint8_t>(),credit=in.number<std::uint8_t>();
            c.initialized=initialized!=0;c.previousCity=city!=0;c.cityCredit=credit!=0;c.due=in.real();c.interval=in.real();c.jitter=in.real();
            if (version>=2) {c.defaultCap=in.number<std::uint8_t>();c.combatCap=in.number<std::uint8_t>();}
            else {c.defaultCap=c.cap;c.combatCap=c.cap>=40 ? c.cap : 0;}
            if (!in.valid || initialized>1 || city>1 || credit>1 || !std::isfinite(c.due) || !std::isfinite(c.interval) || !std::isfinite(c.jitter) || c.jitter<0 || c.jitter>1) return std::nullopt;
            if (c.cap!=0 && !(c.cap>=18 && c.cap<=24) && !(c.cap>=40 && c.cap<=60)) return std::nullopt;
            if (c.defaultCap!=0 && !(c.defaultCap>=18 && c.defaultCap<=24) && !(c.defaultCap>=40 && c.defaultCap<=60)) return std::nullopt;
            if (c.combatCap!=0 && !(c.combatCap>=40 && c.combatCap<=60)) return std::nullopt;
            if ((!c.cap && (c.defaultCap || c.combatCap)) || (c.cap && (!c.defaultCap || (c.cap!=c.defaultCap && c.cap!=c.combatCap)))) return std::nullopt;
            if (c.defaultCap>=40 && c.combatCap!=c.defaultCap) return std::nullopt;
            if (c.initialized && (!c.cap || c.due< -1 || c.interval<0.25 || c.interval>6)) return std::nullopt;
            if (!c.initialized && (c.due!=-1 || c.interval!=0 || c.jitter!=0 || c.cityCredit || c.previousCity)) return std::nullopt;
        }
        const auto gifts=in.number<std::uint32_t>();if (!in.valid || gifts>maxGifts) return std::nullopt;
        for (std::uint32_t j=0;j<gifts;++j) {
            const auto form=in.number<std::uint32_t>(),quantity=in.number<std::uint32_t>();
            if (!in.valid || !form || !quantity || quantity>static_cast<std::uint32_t>(std::numeric_limits<int>::max()) || !state.gifts.emplace(form,static_cast<int>(quantity)).second) return std::nullopt;
        }
        if (!data.actors.emplace(state.actor,std::move(state)).second) return std::nullopt;
    }
    if (!in.valid || in.offset!=bytes.size()) return std::nullopt;
    return data;
}
}
