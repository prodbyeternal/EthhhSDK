// The production tracker/game.cpp on isolated local memory. No DLL injection,
// engine calls, game process, settings, files, or graphics device are involved.
#include "game.h"
#include "log.h"
#include <initializer_list>
#include <limits>

namespace {
int checks=0,writes=0,fileWrites=0;
size_t reads=0,tailReads=0;
uintptr_t watchTail=0;
std::array<unsigned char,0x130000> globals{};
struct Object {std::array<unsigned char,0x400> bytes{};};
struct Class {uint32_t pad=0;uint32_t namePtr=0;char name[64]{};};
Object ninja,head,other,second;
Class ninjaClass,headClass,otherClass,secondClass;
struct Range {uintptr_t base;size_t size;};
std::vector<Range> memory;
uintptr_t mutateAt=0;uint32_t mutateValue=0;int mutateCountdown=0;
void Check(bool ok,const char* label) {++checks;if(!ok) {fprintf(stderr,"FAIL %s\n",label);exit(1);}}
uintptr_t Address(const Object& o) {return uintptr_t(o.bytes.data());}
template<class T> void Put(uintptr_t addr,const T& value) {memcpy(reinterpret_cast<void*>(addr),&value,sizeof(value));}
void Make(Object& o,Class& c,const char* name,uint32_t handle) {
    o.bytes.fill(0);c={};strcpy_s(c.name,name);c.namePtr=uint32_t(uintptr_t(c.name));
    Put(Address(o)+eth::game::off::StratClass,uintptr_t(&c));
    Put(Address(o)+eth::game::off::StratSelfHandle,handle);
    const uint32_t pair[2]={handle,uint32_t(Address(o))};
    memcpy(reinterpret_cast<void*>(eth::game::A(eth::game::off::StratMgrObject)+(handle&0x1ffu)*8),pair,sizeof(pair));
}
void List(std::initializer_list<Object*> objects) {
    uintptr_t first=objects.size() ? Address(**objects.begin()) : 0,previous=0;
    Put(eth::game::A(eth::game::off::StratListHead),first);
    for(auto it=objects.begin();it!=objects.end();++it) {
        const uintptr_t addr=Address(**it),next=it+1!=objects.end() ? Address(**(it+1)) : 0;
        Put(addr+eth::game::off::StratNext,next);Put(addr+eth::game::off::StratPrev,previous);previous=addr;
    }
}
void Reset() {
    globals.fill(0);memory.clear();
    memory.push_back({uintptr_t(globals.data()),globals.size()});
    for(Object* o:{&ninja,&head,&other,&second}) memory.push_back({Address(*o),o->bytes.size()});
    for(Class* c:{&ninjaClass,&headClass,&otherClass,&secondClass}) memory.push_back({uintptr_t(c),sizeof(*c)});
    Make(ninja,ninjaClass,"Ninja",0x201);Make(head,headClass,"NinjaHead",0x202);
    Make(other,otherClass,"Camera",0x203);Make(second,secondClass,"Ninja",0x204);
    List({&ninja,&head,&other});eth::game::PlayerTracker::Get().Reset();
    writes=fileWrites=0;reads=tailReads=0;watchTail=Address(other);mutateAt=0;mutateCountdown=0;
}
void Advance(int ticks=20) {for(int i=0;i<ticks;++i) eth::game::PlayerTracker::Get().Update();}
}

// Link-time test boundary: only memory/timing-independent host services are
// replaced. All selection, uniqueness, generation and tracker code is real.
namespace eth {
uintptr_t GameAddr(uintptr_t va) {return uintptr_t(globals.data())+va-game::off::ImageBase;}
bool ValidateGameImage() {return true;}
bool IsReadable(const void* ptr,size_t size) {
    const uintptr_t addr=uintptr_t(ptr);
    for(const auto& r:memory) if(addr>=r.base && addr-r.base<=r.size && size<=r.size-(addr-r.base)) return true;
    return false;
}
bool SafeReadBytes(uintptr_t addr,void* out,size_t size) {
    ++reads;if(addr>=watchTail && addr-watchTail<0x400) ++tailReads;
    if(!out || !IsReadable(reinterpret_cast<void*>(addr),size)) return false;
    memcpy(out,reinterpret_cast<void*>(addr),size);
    if(addr==mutateAt && mutateCountdown>0 && --mutateCountdown==0) Put(mutateAt,mutateValue);
    return true;
}
bool SafeWriteBytes(uintptr_t addr,const void* value,size_t size) {
    ++writes;if(!IsReadable(reinterpret_cast<void*>(addr),size)) return false;
    memcpy(reinterpret_cast<void*>(addr),value,size);return true;
}
void LogMsg(LogLevel,const char*,...) {}
std::string Format(const char* fmt,...) {char b[2048];va_list args;va_start(args,fmt);vsnprintf(b,sizeof(b),fmt,args);va_end(args);return b;}
std::string GetLogsDir() {return "test-only";}
std::string JoinPath(const std::string& a,const std::string& b) {return a+"/"+b;}
bool WriteStringToFile(const std::string&,const std::string&) {++fileWrites;return false;}
namespace overlay {bool IsVisible() {return false;} bool DeferTimeScale(float) {return false;}}
namespace input {HWND Window() {return nullptr;} bool KeyDown(int) {return false;}}
}

int main() {
    using namespace eth::game;auto& t=PlayerTracker::Get();
    Check(int(PlayerMode::Auto)==0 && int(PlayerMode::Manual)==1 && int(PlayerMode::Learning)==2 && int(PlayerMode::AutoPlus)==3,"legacy mode values preserved; Auto+ appended");
    Reset();Check(t.Mode()==PlayerMode::AutoPlus && !t.Pinned() && !t.Addr(),"Auto+ defaults to unpinned before discovery");
    Check(!t.ReacquireExactNinja(),"Auto+ cannot reacquire before its first verified pin");
    t.Update();Check(t.Pinned() && t.Valid() && t.Addr()==Address(ninja) && t.Mode()==PlayerMode::AutoPlus,"unique exact Ninja becomes an Auto+ pin");
    Check(t.DetectionInfo().find("HL remains OFF")!=std::string::npos,"pinning does not activate HL");
    const auto savedNinja=ninja.bytes,savedHead=head.bytes;
    reads=tailReads=0;for(int i=0;i<600;++i) {t.Update();Check(t.Valid() && t.Pinned(),"cached pinned validity stable");}
    Check(tailReads==0 && reads<2000,"stable Auto+ pin never refreshes full candidates; validation throttled");
    Check(writes==0 && fileWrites==0 && ninja.bytes==savedNinja && head.bytes==savedHead,"pinning/validation never writes body/head poses or runtime files");
    List({&head,&other});Advance();Check(!t.Pinned() && !t.Addr(),"removed allocation loses pin");
    Make(second,secondClass,"Ninja",0x404);List({&head,&second,&other});
    Check(t.ReacquireExactNinja() && t.Addr()==Address(second) && t.Mode()==PlayerMode::AutoPlus,"explicit unique reacquire retains Auto+ mode");
    t.SetMode(PlayerMode::Auto);Check(!t.Pinned() && !t.ReacquireExactNinja(),"plain Auto revokes pin consent");
    t.Update();Check(t.Addr()==Address(second) && t.Valid() && !t.Pinned(),"plain Auto remains detection-only");
    t.SetMode(PlayerMode::Manual);Check(!t.Pinned() && !t.ReacquireExactNinja(),"mode change revokes pin identity");
    t.Select(Address(second));Check(t.Pinned() && t.Mode()==PlayerMode::Manual,"manual selection remains supported");
    List({&head});Advance();Check(!t.Pinned() && !t.Addr(),"manual pin loses dead allocation");
    Make(ninja,ninjaClass,"Ninja",0x601);List({&head,&ninja});
    Advance();Check(!t.Pinned(),"Manual never discovers or pins automatically after loss");
    Check(t.ReacquireExactNinja() && t.Mode()==PlayerMode::Manual && t.Addr()==Address(ninja),"prior unique manual pin allows explicit bounded reacquire");
    t.Select(Address(head));Check(t.Pinned() && !t.ReacquireExactNinja(),"manual attachment selection never grants exact-Ninja consent");
    Reset();List({&ninja,&second});t.Update();Check(!t.Pinned() && !t.Addr(),"two exact Ninja classes are ambiguous");
    List({&ninja,&head});Advance();Check(t.Pinned(),"Auto+ recovers automatically when ambiguity clears");
    // Address reuse is not identity reuse. Generation changes invalidate the
    // old pin; a new pin is allowed only after complete rediscovery.
    Make(ninja,ninjaClass,"Camera",0x801);Advance();Check(!t.Pinned() && !t.Addr(),"same address reused as Camera cannot retain a pin");
    Make(ninja,ninjaClass,"Ninja",0xa01);Advance();Check(t.Pinned() && t.Mode()==PlayerMode::AutoPlus,"new exact-Ninja generation requires fresh Auto+ discovery");
    Reset();Put(Address(head)+off::StratNext,Address(ninja));t.Update();Check(!t.Pinned(),"cyclic/incomplete list cannot prove uniqueness");
    Reset();Put(Address(head)+off::StratNext,uintptr_t(0x123456));t.Update();Check(!t.Pinned(),"unreadable list tail cannot prove uniqueness");
    Reset();otherClass.namePtr=0;t.Update();Check(!t.Pinned(),"unknown class anywhere in list prevents unique identity proof");
    Reset();strcpy_s(ninjaClass.name,"NinjaHead");t.Update();Check(!t.Pinned(),"Ninja prefix never pins a head attachment");
    Reset();Put(Address(ninja)+off::StratSelfHandle,uint32_t(0));t.Update();Check(!t.Pinned(),"zero generation handle never pins");
    Reset();Put(A(off::StratMgrObject)+(0x201&0x1ffu)*8,uint32_t(0x401));t.Update();Check(!t.Pinned(),"owner slot generation mismatch never pins");
    Reset();mutateAt=Address(ninja)+off::StratSelfHandle;mutateValue=0x401;mutateCountdown=2;t.Update();
    Check(!t.Pinned() && !t.Addr(),"identity changing after snapshot is rejected before granting pin");
    Reset();t.SetMode(static_cast<PlayerMode>(99));Check(t.Mode()==PlayerMode::AutoPlus,"invalid public mode leaves tracker unchanged");
    Check(writes==0 && fileWrites==0,"all discovery checks remain read-only");
    printf("PASS production PlayerTracker Auto+/Manual/Auto identity, loss, reacquisition and hot-path tests: %d assertions\n",checks);
}
