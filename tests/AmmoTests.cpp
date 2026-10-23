#include "Core.h"
#include "Lifecycle.h"
#include "Serialization.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>

using namespace NHA;
namespace {
int checks=0;
void check(bool value,const char* message) {++checks;if (!value) throw std::runtime_error(message);}
bool near(double a,double b) {return std::abs(a-b)<0.000001;}
void tests() {
    Context city;city.city=true;Context guardCity=city;guardCity.guard=true;guardCity.battle=true;
    Context guard;guard.guard=true;guard.battle=true;Context wilderness;
    Context camp;camp.banditCamp=true;camp.battle=true;
    for (double jitter:{0.0,0.5,1.0}) {
        check(near(intervalDays(city,jitter),1+jitter*0.5),"City interval is 1-1.5 days");
        check(intervalDays(guardCity,jitter)==0.5,"City guards use half a day");
        check(intervalDays(guard,jitter)==1,"Outside guards use one day");
        check(near(intervalDays(wilderness,jitter),2+jitter),"Outside archers/hunters use 2-3 days");
        check(intervalDays(camp,jitter)==1,"Bandits in camps use one day");
        for (auto context:{city,guardCity,guard,wilderness,camp}) {const auto normal=intervalDays(context,jitter);context.follower=true;check(near(intervalDays(context,jitter),normal*2),"Followers take exactly twice as long for every environment");}
    }
    Random random{123};Channel timer;schedule(timer,wilderness,10,random);
    timer.jitter=0.5;timer.interval=2.5;timer.due=12.5;
    check(advanceTimer(timer,city,10.5) && near(timer.due,10.5) && timer.cityCredit,"Entering city can make timer immediately due");
    Channel once=timer;advanceTimer(once,city,10.5);check(near(once.due,timer.due),"Repeated scans inside city cannot subtract another day");
    auto left=once;advanceTimer(left,wilderness,10.5);advanceTimer(left,city,10.5);check(near(left.due,timer.due),"Leaving and reentering cannot repeat city credit on same timer");
    schedule(timer,city,20,random);check(!timer.cityCredit && timer.previousCity,"A timer starting in city does not get an automatic entry discount");
    const auto due=timer.due;advanceTimer(timer,city,20);check(timer.due==due,"City-resident daily interval is preserved");
    Channel follower;schedule(follower,wilderness,0,random);follower.jitter=1;follower.interval=3;follower.due=3;
    auto following=wilderness;following.follower=true;
    check(!advanceTimer(follower,following,1) && near(follower.due,5),"Recruitment doubles remaining time instead of restarting timer");
    check(!advanceTimer(follower,wilderness,1.5) && near(follower.due,3.25),"Dismissal preserves completed progress at normal speed");
    check(advanceTimer(follower,wilderness,100) && follower.due==3.25,"Overdue timer never moves forward during context change");
    check(!advanceTimer(follower,city,std::numeric_limits<double>::quiet_NaN()),"Invalid clock cannot expire timer");
    for (std::uint64_t seed=0;seed<2000;++seed) {
        for (bool battle:{false,true}) {
            Random rng{seed};auto context=wilderness;context.battle=battle;Channel channel;schedule(channel,context,25,rng);
            check(channel.cap>=(battle?40:18) && channel.cap<=(battle?60:24),"Occupation cap range");
            const auto cap=channel.cap;
            for (int current:{0,1,cap/2,cap-1,cap,cap+100}) {
                const auto initial=refillCount(channel,current,true),later=refillCount(channel,current,false);
                check(initial==std::max(0,cap-current),"Initial fill adds only deficit and counts every carried arrow");
                check(later==std::min(cap/2,std::max(0,cap-current)),"Refill adds floor(half-cap) bounded by deficit");
            }
            check(advanceTimer(channel,context,1000),"Long absence permits one refill");
            const auto half=refillCount(channel,0,false);schedule(channel,context,1000,rng);
            check(channel.cap==cap && half==cap/2 && !advanceTimer(channel,context,1000),"No accumulated missed refills and cap remains stable");
        }
    }
    Channel oldFollower;oldFollower.initialized=true;oldFollower.cap=40;oldFollower.previousCity=true;oldFollower.interval=0.25;oldFollower.due=10.25;
    auto correctedGuard=guardCity;correctedGuard.follower=true;
    check(!advanceTimer(oldFollower,correctedGuard,10.125) && near(oldFollower.due,10.625),"Previously saved fast follower timer preserves its elapsed fraction at corrected slower speed");
    {
        Random rng{456};Channel civilian;schedule(civilian,wilderness,10,rng);
        const auto native=civilian.cap;const auto originalDue=civilian.due;const auto credit=civilian.cityCredit;
        check(native>=18 && native<=24 && civilian.defaultCap==native,"Civilian native cap saved independently");
        auto recruited=wilderness;recruited.follower=true;
        check(synchronizeCap(civilian,recruited,rng) && civilian.cap>=40 && civilian.cap<=60,"Recruitment selects combat cap even for a civilian");
        const auto combat=civilian.cap;const auto randomAfter=rng.state;
        check(civilian.due==originalDue && civilian.cityCredit==credit && civilian.initialized,"Cap change does not reopen generation or reset timer/credit");
        for (int i=0;i<100;++i) {
            synchronizeCap(civilian,wilderness,rng);
            check(civilian.cap==native,"Dismissal restores exact original native cap");
            check(refillCount(civilian,combat,false)==0,"Above-native stock on dismissal is retained without extra additions");
            synchronizeCap(civilian,recruited,rng);
            check(civilian.cap==combat && rng.state==randomAfter,"Repeated recruitment cannot reroll combat cap");
        }
        Channel firstFollower;schedule(firstFollower,recruited,10,rng);
        check(firstFollower.cap>=40 && firstFollower.defaultCap>=18 && firstFollower.defaultCap<=24,"First seen following still saves original civilian occupation cap");
        synchronizeCap(firstFollower,wilderness,rng);
        check(firstFollower.cap==firstFollower.defaultCap,"First-seen follower restores native occupation cap on dismissal");
        Channel soldier;schedule(soldier,guard,10,rng);const auto soldierCap=soldier.cap;
        auto soldierFollowing=guard;soldierFollowing.follower=true;synchronizeCap(soldier,soldierFollowing,rng);
        check(soldier.cap==soldierCap && soldier.combatCap==soldier.defaultCap,"Already-combat NPC retains original combat cap while following");
        State savedFollower;savedFollower.actor=12;savedFollower.base=34;savedFollower.random=rng.state;savedFollower.channels[0]=civilian;
        Save::Data data;data.actors.emplace(12,savedFollower);auto restored=Save::decode(Save::schema,Save::encode(data));
        check(restored && restored->actors.at(12).channels[0].defaultCap==native && restored->actors.at(12).channels[0].combatCap==combat,"Both caps persist through save/load");
        auto reset=savedFollower;resetInventoryCycle(reset);
        check(reset.channels[0].defaultCap==native && reset.channels[0].combatCap==combat,"Real inventory reset preserves both cap rolls");
    }
    State state;state.actor=0x123;state.base=0x456;state.random=789;
    protectGift(state,0x111,20,20);check(state.gifts.at(0x111)==20 && nativeCount(20,20)==0,"A player-given stack alone cannot seed restocking");
    check(nativeCount(30,state.gifts.at(0x111))==10,"Independent native copies remain eligible in a mixed stack");
    protectGift(state,0x111,5,25);check(state.gifts.at(0x111)==25,"Repeated player transfers accumulate protection");
    reconcileGift(state,0x111,12);check(state.gifts.at(0x111)==12,"Firing or transfers conservatively clamp protection to current quantity");
    reconcileGift(state,0x111,0);check(state.gifts.empty(),"Absent gifted stack removes stale provenance");
    protectGift(state,0x111,std::numeric_limits<int>::max(),std::numeric_limits<int>::max());protectGift(state,0x111,1,std::numeric_limits<int>::max());
    check(state.gifts.at(0x111)==std::numeric_limits<int>::max(),"Gift quantities cannot overflow");
    std::vector<Candidate> candidates{{1,Kind::Arrow,2,8,Family::General},{2,Kind::Arrow,1,6,Family::General},{3,Kind::Arrow,1,9,Family::General},{4,Kind::Arrow,1,9,Family::General},{5,Kind::Bolt,0,50,Family::General},{6,Kind::Arrow,0,50,Family::Falmer}};
    check(chooseAmmo(candidates,Kind::Arrow,Family::General)->form==3,"Cheapest first, highest damage at equal cost, deterministic tie");
    {
        std::vector<Candidate> stock{{101,Kind::Arrow,100,20,Family::General,1,5},{102,Kind::Arrow,1,8,Family::General,0,0}};
        check(chooseAmmo(stock,Kind::Arrow,Family::General)->form==101,"Carried arrows beat cheaper native-list ammo");
        stock.push_back({103,Kind::Arrow,200,25,Family::General,1,12});
        check(chooseAmmo(stock,Kind::Arrow,Family::General)->form==103,"Larger carried stack is preferred");
        stock.push_back({104,Kind::Arrow,300,30,Family::General,2,1});
        check(chooseAmmo(stock,Kind::Arrow,Family::General)->form==104,"Equipped carried arrows take first priority");
        check(chooseAmmo(stock,Kind::Arrow,Family::Falmer)->form==104,"Actually carried valid stock is not replaced by species fallback");
        check(!chooseAmmo(stock,Kind::Bolt,Family::General),"Current arrows cannot refill crossbow bolts");
        stock.push_back({105,Kind::Arrow,0,0,Family::General,2,50});
        check(chooseAmmo(stock,Kind::Arrow,Family::General)->form==104,"Unusable carried ammo is rejected");
    }
    check(chooseAmmo(candidates,Kind::Arrow,Family::Falmer)->form==6,"Falmer selects Falmer ammunition rather than generic arrows");
    check(chooseAmmo(candidates,Kind::Bolt,Family::General)->form==5,"Bolts cannot be selected for bows");
    check(!chooseAmmo(candidates,Kind::Arrow,Family::AncientNord),"Ancient Nord species cannot clone another species' arrows");
    candidates.push_back({7,Kind::Arrow,1,0,Family::General});candidates.push_back({8,Kind::Arrow,0,std::numeric_limits<float>::quiet_NaN(),Family::General});
    check(chooseAmmo(candidates,Kind::Arrow,Family::General)->form==3,"Zero damage or nonfinite ammo cannot win selection");
    candidates.push_back({9,Kind::Arrow,0,60,Family::Custom});check(chooseAmmo(candidates,Kind::Arrow,Family::Falmer)->form==9,"Native custom inventory/list ammunition is supported");
    schedule(state.channels[0],guardCity,10,random);schedule(state.channels[1],city,10,random);const auto cap=state.channels[0].cap;
    auto slowSave=state;auto slowContext=wilderness;slowContext.follower=true;
    schedule(slowSave.channels[0],slowContext,10,random);slowSave.channels[0].interval=6;slowSave.channels[0].due=16;
    Save::Data slowData;slowData.actors.emplace(slowSave.actor,slowSave);
    check(Save::decode(Save::schema,Save::encode(slowData)).has_value(),"Six-day follower timer survives existing save schema");
    auto reset=state;resetInventoryCycle(reset);
    check(!reset.channels[0].initialized && reset.channels[0].cap==cap && reset.channels[0].due==-1 && reset.gifts==state.gifts,"Real reset reopens initial fill without rerolling cap or discarding surviving gifts");
    Save::Data data;data.seed=123;data.actors.emplace(state.actor,state);const auto bytes=Save::encode(data);auto loaded=Save::decode(Save::schema,bytes);
    check(loaded && Save::encode(*loaded)==bytes,"Deterministic save/load roundtrip");
    const auto& restored=loaded->actors.at(0x123);
    check(restored.gifts==state.gifts && restored.random==state.random && restored.channels[0].due==state.channels[0].due && restored.channels[1].cap==state.channels[1].cap,"Save/load retains both channels, timers, random state, and gift quantities");
    auto branch=data;branch.actors.at(state.actor).channels[0].due=500;
    check(Save::decode(Save::schema,Save::encode(branch))->actors.at(state.actor).channels[0].due!=restored.channels[0].due,"Older save branches have independent timers");
    check(!Save::decode(Save::schema+1,bytes),"Future schemas fail closed instead of losing provenance");
    for (std::size_t n=0;n<bytes.size();++n) check(!Save::decode(Save::schema,std::span(bytes).first(n)),"Truncated save rejected");
    auto trailing=bytes;trailing.push_back(0);check(!Save::decode(Save::schema,trailing),"Trailing garbage rejected");
    auto invalid=data;invalid.actors.at(state.actor).channels[0].cap=35;check(!Save::decode(Save::schema,Save::encode(invalid)),"Invalid cap rejected");
    invalid=data;invalid.actors.at(state.actor).channels[0].due=std::numeric_limits<double>::infinity();check(!Save::decode(Save::schema,Save::encode(invalid)),"Invalid deadline rejected");
    const std::string legacyHex="7b0000000000000001000000230100005604000015030000000000001501000000000000000029400000000000000440000000000000e03f00000000000000000000f0bf0000000000000000000000000000000000000000";
    std::vector<std::uint8_t> legacyBytes;
    for (std::size_t i=0;i<legacyHex.size();i+=2) legacyBytes.push_back(static_cast<std::uint8_t>(std::stoi(legacyHex.substr(i,2),nullptr,16)));
    auto migrated=Save::decode(1,legacyBytes);
    check(migrated && migrated->actors.at(0x123).channels[0].initialized && migrated->actors.at(0x123).channels[0].defaultCap==21 && migrated->actors.at(0x123).channels[0].due==12.5,"Schema-1 migration retains native cap and timer without regeneration");
    auto& migratedActor=migrated->actors.at(0x123);auto asFollower=wilderness;asFollower.follower=true;Random migratedRng{migratedActor.random};
    synchronizeCap(migratedActor.channels[0],asFollower,migratedRng);
    check(migratedActor.channels[0].cap>=40 && migratedActor.channels[0].defaultCap==21 && migratedActor.channels[0].due==12.5,"Migrated follower can gain combat cap without resetting saved countdown");
    auto malformed=data;malformed.actors.at(state.actor).channels[0].combatCap=25;
    check(!Save::decode(Save::schema,Save::encode(malformed)),"Invalid combat cap fails closed");
    ResetInbox inbox;check(inbox.request(state.actor) && !inbox.request(state.actor),"Duplicate engine reset notifications coalesce");
    for (auto actor:inbox.drain()) resetInventoryCycle(data.actors.at(actor));
    check(!inbox.take(state.actor) && !Save::decode(Save::schema,Save::encode(data))->actors.at(state.actor).channels[0].initialized,"Saving before reset task preserves pending fill and consumes reset once");
    {auto preparation=inbox.prepare(state.actor);check(!inbox.request(state.actor),"Inventory initialization does not recursively open reset cycles");}
    check(inbox.request(state.actor),"A later genuine reset still works");
}
}
int main() {try {tests();std::cout<<"Passed "<<checks<<" ammo behavior checks\n";return 0;}catch (const std::exception& e) {std::cerr<<"Failed after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
