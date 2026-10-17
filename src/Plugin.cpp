#include "Core.h"
#include "Lifecycle.h"
#include "Serialization.h"
#include "Vanilla.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace logger=SKSE::log;
namespace {
using namespace NHA;
using namespace std::chrono_literals;
constexpr std::uint32_t serializationID=0x4E484131,recordID=0x414D4D4F;
template<std::size_t N> bool includes(const std::array<std::uint32_t,N>& values,std::uint32_t id) { return std::find(values.begin(),values.end(),id)!=values.end(); }
struct GiftEvent { std::uint32_t actor,form;int count; };
class Manager final : public RE::BSTEventSink<RE::TESObjectLoadedEvent>,public RE::BSTEventSink<RE::TESResetEvent>,public RE::BSTEventSink<RE::TESFormDeleteEvent>,public RE::BSTEventSink<RE::TESContainerChangedEvent>,public RE::BSTEventSink<RE::TESActorLocationChangeEvent> {
public:
    static Manager& get() {static Manager instance;return instance;}
    Save::Data saved;
    bool validSave=true;
    std::atomic_bool active{false};std::atomic_uint64_t session{0};
    void initialize() {
        auto* data=RE::TESDataHandler::GetSingleton();
        auto faction=[data](std::uint32_t id) {return data->LookupForm<RE::TESFaction>(id,"Skyrim.esm");};
        hunters={faction(Vanilla::HunterFaction),faction(Vanilla::WEServicesHunterFaction)};
        banditFaction=faction(Vanilla::BanditFaction);forswornFaction=faction(Vanilla::ForswornFaction);followerFaction=faction(Vanilla::CurrentFollowerFaction);
        cityKeyword=data->LookupForm<RE::BGSKeyword>(Vanilla::LocTypeCity,"Skyrim.esm");campKeyword=data->LookupForm<RE::BGSKeyword>(Vanilla::LocTypeBanditCamp,"Skyrim.esm");
        iron=data->LookupForm<RE::TESAmmo>(Vanilla::IronArrow,"Skyrim.esm");steel=data->LookupForm<RE::TESAmmo>(Vanilla::SteelArrow,"Skyrim.esm");
        falmer=data->LookupForm<RE::TESAmmo>(Vanilla::FalmerArrow,"Skyrim.esm");ancient=data->LookupForm<RE::TESAmmo>(Vanilla::DraugrArrow,"Skyrim.esm");forsworn=data->LookupForm<RE::TESAmmo>(Vanilla::ForswornArrow,"Skyrim.esm");
        steelBolt=data->LookupForm<RE::TESAmmo>(Vanilla::SteelBolt,"Dawnguard.esm");dwarvenBolt=data->LookupForm<RE::TESAmmo>(Vanilla::DwarvenBolt,"Dawnguard.esm");
        forceConsumption();
        auto* source=RE::ScriptEventSourceHolder::GetSingleton();
        source->AddEventSink<RE::TESObjectLoadedEvent>(this);source->AddEventSink<RE::TESResetEvent>(this);source->AddEventSink<RE::TESFormDeleteEvent>(this);source->AddEventSink<RE::TESContainerChangedEvent>(this);source->AddEventSink<RE::TESActorLocationChangeEvent>(this);
        logger::info("Fallbacks: iron={}, steel={}, Falmer={}, ancient Nord={}, Forsworn={}, steel bolt={}, dwarven bolt={}",iron!=nullptr,steel!=nullptr,falmer!=nullptr,ancient!=nullptr,forsworn!=nullptr,steelBolt!=nullptr,dwarvenBolt!=nullptr);
        worker=std::jthread([this](std::stop_token stop) {while (!stop.stop_requested()) {for (int i=0;i<20 && !stop.stop_requested();++i) std::this_thread::sleep_for(250ms);if (!stop.stop_requested() && active.load()) queueScan();}});
    }
    bool forceConsumption() {
        auto* ini=RE::INISettingCollection::GetSingleton();auto* setting=ini ? ini->GetSetting("bForceNPCsUseAmmo:Combat") : nullptr;
        if (!setting || setting->GetType()!=RE::Setting::Type::kBool) {
            if (!settingWarning) {logger::error("Cannot resolve boolean bForceNPCsUseAmmo:Combat; restocking paused");settingWarning=true;}return false;
        }
        if (!setting->GetBool()) {setting->SetBool(true);logger::info("Forced bForceNPCsUseAmmo:Combat=true (runtime only)");}
        return setting->GetBool();
    }
    void clear() {
        active=false;++session;saved={};saved.seed=static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());validSave=true;
        resets.clear();preparing.clear();weaponDirty.clear();{std::scoped_lock lock(giftMutex);pendingGifts.clear();}queuedScan=noScan;
    }
    void enterGame() {
        if (!validSave || !forceConsumption()) {logger::error("Ammo restock disabled: validSave={}",validSave);return;}
        active=true;initialDiagnostics=0;refillDiagnostics=0;giftDiagnostics=0;capDiagnostics=0;logger::info("Ammo restock enabled; preserving {} actor records; followers use twice the normal interval",saved.actors.size());queueScan();
    }
    template<class F> void queue(F task) {
        const auto generation=session.load();if (auto* tasks=SKSE::GetTaskInterface()) tasks->AddTask([this,generation,task=std::move(task)] {if (active.load() && session.load()==generation) task();});
    }
    bool eligible(RE::Actor* actor) const {
        return actor && !actor->IsPlayerRef() && actor->GetActorBase() && actor->GetRace() && !actor->IsChild() && !actor->IsSummoned() && !actor->GetCommandingActor() && !actor->IsDead() && !actor->IsDeleted() && !actor->IsDisabled();
    }
    State* stateFor(RE::Actor* actor) {
        const auto id=actor->GetFormID(),base=actor->GetActorBase()->GetFormID();auto found=saved.actors.find(id);
        if (found==saved.actors.end()) {
            if (saved.actors.size()>=Save::maxActors) {validSave=false;active=false;logger::error("Actor state limit reached; disabling restocking rather than losing provenance");return nullptr;}
            State state;state.actor=id;state.base=base;Random seed{saved.seed^(static_cast<std::uint64_t>(id)<<32)^base};state.random=seed.next();found=saved.actors.emplace(id,std::move(state)).first;
        }
        found->second.base=base;return &found->second;
    }
    Family species(RE::Actor* actor) const {
        const auto race=actor->GetRace()->GetFormID();
        if (includes(Vanilla::falmerRaces,race)) return Family::Falmer;
        if (includes(Vanilla::undeadRaces,race)) return Family::AncientNord;
        if (forswornFaction && actor->IsInFaction(forswornFaction)) return Family::Forsworn;
        return Family::General;
    }
    bool locationHas(RE::BGSLocation* location,RE::BGSKeyword* keyword) const {
        if (!keyword) return false;std::set<RE::BGSLocation*> seen;
        while (location && seen.size()<64 && seen.insert(location).second) {if (location->HasKeyword(keyword)) return true;location=location->parentLoc;}return false;
    }
    Context context(RE::Actor* actor) const {
        const auto id=actor->GetActorBase()->npcClass ? actor->GetActorBase()->npcClass->GetFormID() : 0;
        Context c;c.city=locationHas(actor->GetCurrentLocation(),cityKeyword);c.guard=actor->IsGuard() || includes(Vanilla::guardClasses,id);
        const bool bandit=(banditFaction && actor->IsInFaction(banditFaction)) || (forswornFaction && actor->IsInFaction(forswornFaction));
        c.banditCamp=bandit && locationHas(actor->GetCurrentLocation(),campKeyword);
        c.follower=actor->IsPlayerTeammate() || (followerFaction && actor->IsInFaction(followerFaction));
        const bool hunter=std::any_of(hunters.begin(),hunters.end(),[&](auto* f) {return f && actor->IsInFaction(f);});
        c.battle=c.guard || bandit || species(actor)!=Family::General || (!hunter && includes(Vanilla::battleClasses,id));return c;
    }
    bool rangedWeapon(RE::TESObjectWEAP* weapon,Kind kind) const {
        if (!weapon || weapon->IsDeleted() || weapon->IsBound() || weapon->weaponData.flags2.any(RE::TESObjectWEAP::Data::Flag2::kPlayerOnly)) return false;
        return kind==Kind::Bolt ? weapon->IsCrossbow() : weapon->IsBow() && !weapon->IsCrossbow();
    }
    bool usableAmmo(RE::TESAmmo* ammo) const {
        return ammo && !ammo->IsDeleted() && ammo->GetPlayable() && ammo->GetRuntimeData().data.projectile && std::isfinite(ammo->GetRuntimeData().data.damage) && ammo->GetRuntimeData().data.damage>0;
    }
    std::optional<Candidate> candidate(RE::TESAmmo* ammo) const {
        if (!usableAmmo(ammo)) return std::nullopt;
        Family family=Family::General;
        if (ammo==falmer) family=Family::Falmer;else if (ammo==ancient) family=Family::AncientNord;else if (ammo==forsworn) family=Family::Forsworn;
        else if (auto* origin=ammo->GetFile(0);origin && std::string_view(origin->fileName)!="Skyrim.esm" && std::string_view(origin->fileName)!="Dawnguard.esm" && std::string_view(origin->fileName)!="Dragonborn.esm") family=Family::Custom;
        return Candidate{ammo->GetFormID(),ammo->IsBolt() ? Kind::Bolt : Kind::Arrow,std::max(0,ammo->GetGoldValue()),ammo->GetRuntimeData().data.damage,family};
    }
    void collect(RE::TESForm* form,std::uint16_t level,std::vector<RE::TESForm*>& forms,std::set<std::uint32_t>& path,int depth,int& nodes) const {
        if (!form || depth>16 || ++nodes>4096 || form->IsDeleted()) return;
        auto* list=form->As<RE::TESLevItem>();if (!list) {forms.push_back(form);return;}
        if (!list->entries.data() || !path.insert(form->GetFormID()).second) return;
        if (list->GetChanceNone()>=100) {path.erase(form->GetFormID());return;}
        std::uint16_t best=0;
        for (std::uint32_t i=0;i<std::min<std::size_t>(list->numEntries,list->entries.size());++i) {const auto& e=list->entries[i];if (e.form && e.count && e.level<=level) best=std::max(best,e.level);}
        const bool all=(static_cast<unsigned>(list->llFlags)&(RE::TESLeveledList::kCalculateFromAllLevelsLTOrEqPCLevel|RE::TESLeveledList::kUseAll))!=0;
        for (std::uint32_t i=0;i<std::min<std::size_t>(list->numEntries,list->entries.size());++i) {const auto& e=list->entries[i];if (e.form && e.count && e.level<=level && (all || e.level==best)) collect(e.form,level,forms,path,depth+1,nodes);}
        path.erase(form->GetFormID());
    }
    void flushGifts() {
        std::vector<GiftEvent> events;{std::scoped_lock lock(giftMutex);events.swap(pendingGifts);}
        for (const auto& e:events) {
            auto* actor=RE::TESForm::LookupByID<RE::Actor>(e.actor);if (!actor || !actor->GetActorBase() || actor->IsDeleted()) continue;
            auto* state=stateFor(actor);if (!state) return;
            if (!state->gifts.contains(e.form) && state->gifts.size()>=Save::maxGifts) {validSave=false;active=false;logger::error("Gift tracking limit reached; restocking disabled");return;}
            protectGift(*state,e.form,e.count,std::numeric_limits<int>::max());
            if (giftDiagnostics++<20) logger::info("Protected player-given ammo {:08X}: actor {:08X}, transferred {}",e.form,e.actor,e.count);
        }
    }
    void flushResets() {
        flushGifts();for (auto id:resets.drain()) if (auto it=saved.actors.find(id);it!=saved.actors.end()) resetInventoryCycle(it->second);
    }
    void visit(RE::Actor* actor) {
        if (!validSave || !eligible(actor)) return;
        const auto id=actor->GetFormID();const auto generation=session.load();
        if (preparing.contains(id) || resets.contains(id)) return;
        preparing.insert(id);struct Guard {std::set<std::uint32_t>& set;std::uint32_t id;~Guard(){set.erase(id);}} guard{preparing,id};
        flushGifts();if (!validSave) return;
        auto* calendar=RE::Calendar::GetSingleton();const double now=calendar ? calendar->GetDaysPassed() : -1;
        if (!std::isfinite(now) || now<0) return;
        const auto environment=context(actor);
        if (auto cached=saved.actors.find(id);cached!=saved.actors.end()) {
            bool initialized=false,due=false;
            Random capRandom{cached->second.random};
            for (auto& channel:cached->second.channels) {
                if (channel.cap || channel.initialized) {
                    const auto previous=channel.cap;
                    if (synchronizeCap(channel,environment,capRandom) && capDiagnostics++<20)
                        logger::info("Cap changed {:08X} '{}': {} -> {}, follower={}",id,actor->GetName(),previous,channel.cap,environment.follower);
                }
                if (channel.initialized) {initialized=true;due=advanceTimer(channel,environment,now) || due;}
            }
            cached->second.random=capRandom.state;
            if (initialized && !due && !weaponDirty.contains(id)) return;
        }
        // Location events can update saved progress/credit without 3D, but
        // inventory additions wait until the actor is actually loaded.
        if (!actor->Get3D()) return;
        weaponDirty.erase(id);
        RE::TESObjectREFR::InventoryItemMap inventory;
        {auto init=resets.prepare(id);if (!actor->GetInventoryChanges()) return;inventory=actor->GetInventory();}
        if (!active.load() || session.load()!=generation || resets.contains(id)) return;
        std::array<bool,2> weapons{};std::vector<RE::TESForm*> listForms;std::set<std::uint32_t> path;int nodes=0;
        auto* base=actor->GetActorBase();
        base->ForEachContainerObject([&](RE::ContainerObject& entry) {if (entry.count>0) collect(entry.obj,actor->GetLevel(),listForms,path,0,nodes);return RE::BSContainer::ForEachResult::kContinue;});
        if (base->defaultOutfit) base->defaultOutfit->ForEachItem([&](RE::TESForm* form) {collect(form,actor->GetLevel(),listForms,path,0,nodes);return RE::BSContainer::ForEachResult::kContinue;});
        for (const auto& [item,data]:inventory) if (data.first>0) for (int k=0;k<2;++k) weapons[k]=weapons[k] || rangedWeapon(item ? item->As<RE::TESObjectWEAP>() : nullptr,static_cast<Kind>(k));
        const auto classID=base->npcClass ? base->npcClass->GetFormID() : 0;
        const bool hunter=std::any_of(hunters.begin(),hunters.end(),[&](auto* f) {return f && actor->IsInFaction(f);});
        // Hunters/archer classes may receive arrows even after losing their bow.
        if (!weapons[0] && !weapons[1] && (hunter || includes(Vanilla::archerClasses,classID))) {
            bool listedBow=false,listedCrossbow=false;
            for (auto* form:listForms) {listedBow=listedBow || rangedWeapon(form->As<RE::TESObjectWEAP>(),Kind::Arrow);listedCrossbow=listedCrossbow || rangedWeapon(form->As<RE::TESObjectWEAP>(),Kind::Bolt);}
            weapons[listedCrossbow && !listedBow ? 1 : 0]=true;
        }
        if (!weapons[0] && !weapons[1]) return;
        auto* state=stateFor(actor);if (!state) return;
        std::vector<std::uint32_t> protectedForms;for (const auto& [form,count]:state->gifts) protectedForms.push_back(form);
        for (auto form:protectedForms) {int held=0;for (const auto& [item,data]:inventory) if (item && item->GetFormID()==form) held=std::max(0,data.first);reconcileGift(*state,form,held);}
        const auto family=species(actor);
        for (int k=0;k<2;++k) {
            if (!weapons[k] || resets.contains(id) || !active.load() || session.load()!=generation) continue;
            auto found=saved.actors.find(id);if (found==saved.actors.end()) return;
            auto channel=found->second.channels[k];Random random{found->second.random};const bool initial=!channel.initialized;
            synchronizeCap(channel,environment,random);
            found->second.channels[k]=channel;found->second.random=random.state;
            if (!initial && !advanceTimer(channel,environment,now)) {found->second.channels[k]=channel;continue;}
            // Snapshot again: callbacks from an earlier arrow addition may have
            // changed bolt quantities, gifts, or weapons.
            flushGifts();auto live=actor->GetInventory();int current=0;
            for (const auto& [item,data]:live) if (auto* ammo=item ? item->As<RE::TESAmmo>() : nullptr;ammo && data.first>0 && (ammo->IsBolt() ? 1 : 0)==k) current=static_cast<int>(std::min<std::int64_t>(std::numeric_limits<int>::max(),static_cast<std::int64_t>(current)+data.first));
            if (resets.contains(id) || !active.load() || session.load()!=generation || !validSave || !eligible(actor)) return;
            found=saved.actors.find(id);if (found==saved.actors.end()) return;
            std::vector<std::uint32_t> liveGiftForms;for (const auto& [form,count]:found->second.gifts) liveGiftForms.push_back(form);
            for (auto form:liveGiftForms) {int held=0;for (const auto& [item,data]:live) if (item && item->GetFormID()==form) held=std::max(0,data.first);reconcileGift(found->second,form,held);}
            std::vector<Candidate> choices;
            for (const auto& [item,data]:live) {
                auto* ammo=item ? item->As<RE::TESAmmo>() : nullptr;if (!ammo || data.first<=0 || (data.second && data.second->IsQuestObject())) continue;
                const int protectedCount=found->second.gifts.contains(ammo->GetFormID()) ? found->second.gifts.at(ammo->GetFormID()) : 0;
                if (nativeCount(data.first,protectedCount)>0) if (auto c=candidate(ammo)) choices.push_back(*c);
            }
            for (auto* form:listForms) if (auto* ammo=form->As<RE::TESAmmo>()) if (auto c=candidate(ammo)) choices.push_back(*c);
            const int quantity=refillCount(channel,current,initial);
            const auto kind=static_cast<Kind>(k);auto selected=chooseAmmo(choices,kind,family);
            RE::TESAmmo* selectedAmmo=selected ? RE::TESForm::LookupByID<RE::TESAmmo>(selected->form) : nullptr;
            if (!selectedAmmo) {
                if (kind==Kind::Bolt) selectedAmmo=usableAmmo(steelBolt) ? steelBolt : dwarvenBolt;
                else if (family==Family::Falmer) selectedAmmo=falmer;else if (family==Family::AncientNord) selectedAmmo=ancient;else if (family==Family::Forsworn) selectedAmmo=forsworn;
                else selectedAmmo=(environment.battle || environment.follower) ? (usableAmmo(steel) ? steel : iron) : (usableAmmo(iron) ? iron : steel);
            }
            if (quantity>0 && (!usableAmmo(selectedAmmo) || (selectedAmmo->IsBolt() ? 1 : 0)!=k)) continue;
            schedule(channel,environment,now,random);
            // Commit before callbacks, never overwrite gift/reset state afterward.
            found=saved.actors.find(id);if (found==saved.actors.end()) return;
            found->second.channels[k]=channel;found->second.random=random.state;
            if (quantity>0) actor->AddObjectToContainer(selectedAmmo,nullptr,quantity,nullptr);
            if ((initial ? initialDiagnostics : refillDiagnostics)++<30) logger::info("{} {:08X} '{}': {}, cap={}, existing={}, added={}, ammo='{}', next game day={}, interval={} days, follower={}",initial ? "Initialized" : "Restocked",id,actor->GetName(),k ? "bolts" : "arrows",channel.cap,current,quantity,selectedAmmo ? selectedAmmo->GetName() : "none needed",channel.due,channel.interval,environment.follower);
        }
    }
    void queueScan() {
        const auto generation=session.load();if (queuedScan.exchange(generation)==generation) return;
        if (auto* tasks=SKSE::GetTaskInterface()) tasks->AddTask([this,generation] {auto expected=generation;queuedScan.compare_exchange_strong(expected,noScan);if (active.load() && session.load()==generation) scan();});
    }
    void scan() {
        if (!validSave || !forceConsumption() || RE::UI::GetSingleton()->GameIsPaused()) return;
        flushGifts();std::vector<RE::ActorHandle> actors;
        RE::ProcessLists::GetSingleton()->ForAllActors([&](RE::Actor* actor) {if (actor) actors.push_back(actor->GetHandle());return RE::BSContainer::ForEachResult::kContinue;});
        const auto generation=session.load();for (const auto& handle:actors) {if (!active.load() || session.load()!=generation) break;if (auto actor=handle.get()) visit(actor.get());}
    }
    void requestReset(RE::Actor* actor) {
        if (!active.load() || !actor || actor->IsPlayerRef() || !resets.request(actor->GetFormID())) return;
        const auto id=actor->GetFormID();queue([this,id] {if (!resets.take(id)) return;flushGifts();if (auto it=saved.actors.find(id);it!=saved.actors.end()) resetInventoryCycle(it->second);visit(RE::TESForm::LookupByID<RE::Actor>(id));});
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event,RE::BSTEventSource<RE::TESContainerChangedEvent>*) override {
        if (event && active.load() && event->newContainer && event->newContainer!=0x14 && RE::TESForm::LookupByID<RE::TESObjectWEAP>(event->baseObj)) {
            const auto id=event->newContainer;queue([this,id] {if (auto* actor=RE::TESForm::LookupByID<RE::Actor>(id)) {weaponDirty.insert(id);visit(actor);}});
        }
        if (event && active.load() && event->oldContainer==0x14 && event->newContainer && event->newContainer!=0x14 && event->itemCount>0 && RE::TESForm::LookupByID<RE::TESAmmo>(event->baseObj)) {
            {std::scoped_lock lock(giftMutex);if (pendingGifts.size()>=100000) {active=false;validSave=false;logger::error("Pending gifts overflow; restock disabled");return RE::BSEventNotifyControl::kContinue;}pendingGifts.push_back({event->newContainer,event->baseObj,event->itemCount});}
            queue([this] {flushGifts();});
        }return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* event,RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override {
        if (event && event->loaded && active.load()) {const auto id=event->formID;queue([this,id] {visit(RE::TESForm::LookupByID<RE::Actor>(id));});}return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESActorLocationChangeEvent* event,RE::BSTEventSource<RE::TESActorLocationChangeEvent>*) override {
        if (event && event->actor && active.load()) {const auto id=event->actor->GetFormID();queue([this,id] {visit(RE::TESForm::LookupByID<RE::Actor>(id));});}return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESResetEvent* event,RE::BSTEventSource<RE::TESResetEvent>*) override {if (event && event->object) requestReset(event->object->As<RE::Actor>());return RE::BSEventNotifyControl::kContinue;}
    RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* event,RE::BSTEventSource<RE::TESFormDeleteEvent>*) override {
        if (event && active.load()) {const auto id=event->formID;queue([this,id] {auto* form=RE::TESForm::LookupByID(id);if (!form || form->IsDeleted()) saved.actors.erase(id);});}return RE::BSEventNotifyControl::kContinue;
    }
private:
    std::array<RE::TESFaction*,2> hunters{};RE::TESFaction *banditFaction=nullptr,*forswornFaction=nullptr,*followerFaction=nullptr;
    RE::BGSKeyword *cityKeyword=nullptr,*campKeyword=nullptr;RE::TESAmmo *iron=nullptr,*steel=nullptr,*falmer=nullptr,*ancient=nullptr,*forsworn=nullptr,*steelBolt=nullptr,*dwarvenBolt=nullptr;
    ResetInbox resets;std::set<std::uint32_t> preparing,weaponDirty;std::mutex giftMutex;std::vector<GiftEvent> pendingGifts;
    static constexpr std::uint64_t noScan=std::numeric_limits<std::uint64_t>::max();std::atomic_uint64_t queuedScan{noScan};
    bool settingWarning=false;std::uint32_t initialDiagnostics=0,refillDiagnostics=0,giftDiagnostics=0,capDiagnostics=0;std::jthread worker;
};
struct ResetHook {
    static inline REL::Relocation<void(*)(RE::Actor*,bool)> actorOriginal,characterOriginal;
    static void actor(RE::Actor* self,bool leveledOnly) {actorOriginal(self,leveledOnly);Manager::get().requestReset(self);}
    static void character(RE::Actor* self,bool leveledOnly) {characterOriginal(self,leveledOnly);Manager::get().requestReset(self);}
    static void install() {REL::Relocation<std::uintptr_t> a{RE::VTABLE_Actor[0]},c{RE::VTABLE_Character[0]};actorOriginal=a.write_vfunc(0x8A,actor);characterOriginal=c.write_vfunc(0x8A,character);}
};
void save(SKSE::SerializationInterface* api) {
    auto& manager=Manager::get();manager.flushResets();if (!manager.validSave) {api->OpenRecord(recordID,0xFFFFFFFF);return;}
    const auto bytes=Save::encode(manager.saved);if (!api->OpenRecord(recordID,Save::schema) || !api->WriteRecordData(bytes.data(),static_cast<std::uint32_t>(bytes.size()))) logger::error("Could not serialize ammo state");
}
void load(SKSE::SerializationInterface* api) {
    auto& manager=Manager::get();manager.clear();std::uint32_t type,version,length;bool seen=false;
    while (api->GetNextRecordInfo(type,version,length)) {
        if (type!=recordID) continue;if (seen || length>64*1024*1024) {manager.validSave=false;continue;}seen=true;
        std::vector<std::uint8_t> bytes(length);if (api->ReadRecordData(bytes.data(),length)!=length) {manager.validSave=false;continue;}
        auto data=Save::decode(version,bytes);if (!data) {manager.validSave=false;logger::error("Unreadable ammo state; restocking disabled to preserve gift protection");continue;}
        manager.saved.seed=data->seed;
        for (auto& [id,state]:data->actors) {
            std::uint32_t actor,base;if (!api->ResolveFormID(id,actor)) continue;if (!api->ResolveFormID(state.base,base)) base=state.base;state.actor=actor;state.base=base;
            std::unordered_map<std::uint32_t,int> remapped;for (auto [form,count]:state.gifts) {std::uint32_t resolved;if (api->ResolveFormID(form,resolved)) remapped[resolved]=static_cast<int>(std::min<std::int64_t>(std::numeric_limits<int>::max(),static_cast<std::int64_t>(remapped[resolved])+count));}state.gifts=std::move(remapped);manager.saved.actors.emplace(actor,std::move(state));
        }
    }logger::info("Loaded ammo state for {} actors",manager.saved.actors.size());
}
void revert(SKSE::SerializationInterface*) {Manager::get().clear();}
void message(SKSE::MessagingInterface::Message* message) {
    auto& manager=Manager::get();switch (message->type) {
    case SKSE::MessagingInterface::kDataLoaded:manager.initialize();ResetHook::install();break;
    case SKSE::MessagingInterface::kPreLoadGame:manager.active=false;++manager.session;break;
    case SKSE::MessagingInterface::kPostLoadGame:if (message->data) manager.enterGame();break;
    case SKSE::MessagingInterface::kNewGame:manager.clear();manager.enterGame();break;
    default:break;
    }
}
}
SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);if (const auto path=SKSE::log::log_directory()) {auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>((*path/"NPCsHaveAmmo.log").string(),true);spdlog::set_default_logger(std::make_shared<spdlog::logger>("NPCsHaveAmmo",std::move(sink)));spdlog::set_level(spdlog::level::info);spdlog::flush_on(spdlog::level::info);}
    logger::info("NPCs Have Ammo 1.0.2; runtime {}",REL::Module::get().version().string());
    const auto v=REL::Module::get().version();const std::set<unsigned> ae{317,318,323,342,353,629,640,659,1130,1170,1179};
    if (!(v.major()==1 && ((v.minor()==5 && v.patch()==97) || (v.minor()==6 && ae.contains(v.patch())) || (v.minor()==7 && (v.patch()==99 || v.patch()==104))))) {logger::error("Unsupported runtime");return false;}
    auto* api=SKSE::GetSerializationInterface();if (!api || !SKSE::GetTaskInterface()) return false;
    api->SetUniqueID(serializationID);api->SetSaveCallback(save);api->SetLoadCallback(load);api->SetRevertCallback(revert);return SKSE::GetMessagingInterface()->RegisterListener(message);
}
