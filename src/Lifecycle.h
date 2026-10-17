#pragma once
#include "Core.h"
#include <mutex>
#include <set>

namespace NHA {
// Event callbacks can queue the same reset more than once. Both the queued
// consumer and save callback use this inbox, so saving cannot lose pending work.
class ResetInbox {
public:
    bool request(std::uint32_t actor) { std::scoped_lock lock(mutex); return !preparing.contains(actor) && pending.insert(actor).second; }
    bool contains(std::uint32_t actor) { std::scoped_lock lock(mutex); return pending.contains(actor); }
    bool take(std::uint32_t actor) { std::scoped_lock lock(mutex); return pending.erase(actor)!=0; }
    std::set<std::uint32_t> drain() { std::scoped_lock lock(mutex); std::set<std::uint32_t> out; out.swap(pending); return out; }
    void clear() { std::scoped_lock lock(mutex); pending.clear(); preparing.clear(); }
    class Preparation {
    public:
        Preparation(ResetInbox& owner,std::uint32_t actor) : owner(owner),actor(actor) { std::scoped_lock lock(owner.mutex); owner.preparing.insert(actor); }
        ~Preparation() { std::scoped_lock lock(owner.mutex); owner.preparing.erase(actor); }
        Preparation(const Preparation&)=delete;
        Preparation& operator=(const Preparation&)=delete;
    private:
        ResetInbox& owner;
        std::uint32_t actor;
    };
    Preparation prepare(std::uint32_t actor) { return Preparation(*this,actor); }
private:
    std::mutex mutex;
    std::set<std::uint32_t> pending;
    std::set<std::uint32_t> preparing;
};
}
