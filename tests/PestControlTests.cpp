// Isolated policy fixtures. No process access, engine calls, files, or settings.
#include "pestcontrol_policy.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {
using namespace eth::patch::pest;
constexpr uintptr_t base = 0x100000;
std::vector<unsigned char> memory(0x700000);
size_t reads = 0, requests = 0, checks = 0;
uintptr_t unreadable = 0;
Layout layout{base,base+0x189c,base+0x18a0,base+0x18b8,base+0x18bc};
void Check(bool ok, const char* message) { ++checks; if (!ok) {fprintf(stderr,"FAIL %s\n",message);exit(1);} }
template<class T> void Put(uintptr_t address, const T& value) { memcpy(memory.data()+address-base,&value,sizeof(value)); }
bool Read(uintptr_t address, void* output, size_t size) {
    ++reads;
    if (!output || address<base || address-base>memory.size() || size>memory.size()-(address-base) ||
        (unreadable && address<=unreadable && unreadable-address<size)) return false;
    memcpy(output,memory.data()+address-base,size);return true;
}
uintptr_t Address(size_t i) {return base+0x10000+i*0x200;}
void ClassName(size_t i, const char* name) {
    const uintptr_t cls=base+0x500000+i*0x80;
    const uint32_t ptr=uint32_t(cls+0x20);
    Put(cls+4,ptr);memset(memory.data()+ptr-base,0,64);strcpy(reinterpret_cast<char*>(memory.data()+ptr-base),name);
}
void Reset(size_t count=3) {
    std::fill(memory.begin(),memory.end(),0);reads=requests=0;unreadable=0;
    Put(layout.head,uint32_t(count ? Address(0) : 0));Put(layout.tail,uint32_t(count ? Address(count-1) : 0));
    for(size_t i=0;i<count;++i) {
        const uint32_t handle=uint32_t(i+1),cls=uint32_t(base+0x500000+i*0x80);
        Node node{0,0,uint32_t(i+1<count ? Address(i+1) : 0),uint32_t(i ? Address(i-1) : 0),handle,0,cls};
        Put(Address(i)+0x190,node);ClassName(i,i==1 ? "Ninja" : "Rat");
        const uint32_t pair[2]={handle,uint32_t(Address(i))};
        memcpy(memory.data()+(handle&0x1ffu)*8,pair,sizeof(pair));
    }
}
Plan SearchNow(bool enabled=true) {return Search(enabled,layout,Read,[]{return true;});}
bool NativeFake(const Target& t) {
    ++requests;Node n;Check(ReadNode(t.address,n,Read),"fake native receives readable strat");
    n.flags|=kDeleteFlags;Put(t.address+0x190,n);return true;
}
size_t ApplyNow(const Plan& p, bool enabled=true) {return Apply(enabled,layout,p,Read,NativeFake);}
}
int main() {
    Reset();const auto original=memory;
    auto plan=SearchNow(false);Check(reads==0 && !plan.complete && !plan.count,"disabled search has zero memory work");
    Check(ApplyNow(plan,false)==0 && reads==0 && requests==0,"disabled apply has zero memory/native work");
    plan=SearchNow();Check(plan.complete && plan.visited==3 && plan.detected==2 && plan.count==2,"exact Rats selected from a complete mixed list");
    Check(memory==original,"search never writes any strat, body, head, handle, or pose");
    Check(ApplyNow(plan)==2 && requests==2,"one native deletion request per exact live Rat");
    Node ninja;ReadNode(Address(1),ninja,Read);Check(ninja.flags==0,"Ninja is never touched");
    Check(ApplyNow(plan)==0 && requests==2,"pending deletion is never submitted twice");
    plan=SearchNow();Check(plan.complete && plan.detected==2 && !plan.count,"pending Rat remains visible in telemetry, not resubmitted");
    for(const char* name:{"rat","RAT","RatSpawner","RatHead","Pirate","NinjaHead","NinjaSword","Camera","Crab","Mosquito"}) {
        Reset(1);ClassName(0,name);plan=SearchNow();Check(plan.complete && !plan.count && ApplyNow(plan)==0,"prefix/suffix/case/other species are not Rats");
    }
    Reset(1);plan=SearchNow();Put(Address(0)+0x1a0,uint32_t(0x201));Check(!ApplyNow(plan),"allocation generation changes invalidate a planned request");
    Reset(1);plan=SearchNow();Put(base+8,uint32_t(0x201));Check(!ApplyNow(plan),"handle-table generation mismatch refuses native request");
    Reset(1);plan=SearchNow();Put(base+12,uint32_t(Address(0)+0x200));Check(!ApplyNow(plan),"handle-table address mismatch refuses native request");
    Reset(1);plan=SearchNow();ClassName(0,"Ninja");Check(!ApplyNow(plan),"class name rechecked immediately before request");
    Reset(1);plan=SearchNow();Put(Address(0)+0x1a8,uint32_t(0));Check(!ApplyNow(plan),"unreadable/reused class refuses native request");
    Reset(1);Put(Address(0)+0x1a0,uint32_t(0));plan=SearchNow();Check(plan.complete && !plan.count,"zero self handle is not deletable identity");
    Reset();plan=SearchNow();Put(layout.head,uint32_t(Address(1)));Check(!Validate(layout,plan.targets[0],Read),"removed list head fails membership validation");
    Reset();plan=SearchNow();Put(Address(1)+0x19c,uint32_t(0));Check(!Validate(layout,plan.targets[0],Read),"broken next/previous linkage fails membership validation");
    Reset();Put(layout.tail,uint32_t(Address(1)));plan=SearchNow();Check(!plan.complete && !ApplyNow(plan),"tail disagreement refuses entire search");
    Reset();Put(Address(2)+0x198,uint32_t(Address(0)));plan=SearchNow();Check(!plan.complete && !ApplyNow(plan),"cyclic list refuses entire search without unbounded iteration");
    Reset();unreadable=Address(2)+0x198;plan=SearchNow();Check(!plan.complete && !ApplyNow(plan),"unreadable tail refuses partial deletion plan");
    Reset();unreadable=base+0x500080+4;plan=SearchNow();Check(!plan.complete && !ApplyNow(plan),"unknown class causes fail-closed incomplete scan");
    Reset();int budget=1;plan=Search(true,layout,Read,[&]{return budget-->0;});Check(plan.budgetStopped && !plan.complete && !ApplyNow(plan),"soft work budget aborts partial plan without native work");
    for(uintptr_t iterator:{layout.current,layout.next}) {
        Reset();Put(iterator,uint32_t(Address(0)));reads=0;plan=SearchNow();Check(!plan.complete && !plan.visited && !ApplyNow(plan),"native iteration in progress refuses all requests");
        Reset();plan=SearchNow();Put(iterator,uint32_t(Address(0)));Check(!ApplyNow(plan),"iteration rechecked between search and apply");
    }
    Reset(65);ClassName(1,"Rat");plan=SearchNow();Check(plan.complete && plan.detected==65 && plan.count==64,"per-pass request cap preserves complete-list validation");
    Check(ApplyNow(plan)==64,"bounded pass queues at most 64 native requests");
    plan=SearchNow();Check(plan.complete && plan.count==1 && ApplyNow(plan)==1,"overflow Rats handled in a subsequent search");
    Reset(kMaxStrats+1);plan=SearchNow();Check(plan.visited==kMaxStrats && !plan.complete && !ApplyNow(plan),"8192-node hard cap refuses oversized incomplete list");
    Reset(0);plan=SearchNow();Check(plan.complete && !plan.count,"empty valid level is harmless");
    // Fake the native completion: remove the Rat from membership and invalidate
    // its generation. SDK policy cannot reuse an old token to delete the respawn.
    Reset(1);plan=SearchNow();Check(ApplyNow(plan)==1,"deferred lifecycle request succeeds");
    Put(layout.head,uint32_t(0));Put(layout.tail,uint32_t(0));Put(base+8,uint32_t(0));Put(base+12,uint32_t(0));
    Check(!ApplyNow(plan) && SearchNow().count==0,"completed native removal cannot be requested again");
    printf("PASS PestControl exact identity, lifetime, membership, disabled and bounded-work policy: %zu assertions\n",checks);
}
