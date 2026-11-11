// Production pestcontrol.cpp + production VirtualQuery/SEH reader, against
// isolated VirtualAlloc fixtures. No game DLL, process, settings or files.
// A 12-byte synthetic cdecl fixture reproduces native 128's single flag OR.
#include "pestcontrol.h"
#include "pestcontrol_policy.h"
#include "game.h"
#include "log.h"
#include <cstdio>
#include <cstdlib>

namespace {
using namespace eth::patch::pest;
unsigned char* globals = nullptr;
uint32_t tick = 0;
int checks = 0, reads = 0, hashes = 0;
bool supportedHash = false;
void Check(bool ok, const char* message) {++checks;if(!ok) {fprintf(stderr,"FAIL %s\n",message);exit(1);}}
template<class T> void Put(uintptr_t address,const T& value) {memcpy(reinterpret_cast<void*>(address),&value,sizeof(value));}
uintptr_t Address(int i) {return uintptr_t(globals)+0x8000+i*0x400;}
void MakeList(int count=3) {
    using namespace eth::game;
    Put(A(off::StratListHead),uint32_t(count ? Address(0) : 0));
    Put(A(off::StratListTail),uint32_t(count ? Address(count-1) : 0));
    Put(A(off::StratIterCur),uint32_t(0));Put(A(0x511A58),uint32_t(0));
    for(int i=0;i<count;++i) {
        const uint32_t handle=0x201+i,cls=uint32_t(uintptr_t(globals)+0xa000+i*0x80);
        Node n{0,0,uint32_t(i+1<count ? Address(i+1) : 0),uint32_t(i ? Address(i-1) : 0),handle,0,cls};
        Put(Address(i)+0x190,n);const uint32_t name=cls+0x20;Put(cls+4,name);
        strcpy(reinterpret_cast<char*>(uintptr_t(name)),i==1 ? "Ninja" : "Rat");
        const uint32_t pair[2]={handle,uint32_t(Address(i))};
        memcpy(reinterpret_cast<void*>(A(off::StratMgrObject)+(handle&0x1ffu)*8),pair,sizeof(pair));
    }
}
void NativeCode(uint8_t flags) {
    const uintptr_t entry=eth::game::A(0x416F10);
    DWORD old=0;Check(VirtualProtect(reinterpret_cast<void*>(entry),12,PAGE_READWRITE,&old)!=0,"fixture writable");
    const uint8_t bytes[]={0x8b,0x44,0x24,0x04,0x83,0x88,0x90,0x01,0x00,0x00,flags,0xc3};
    memcpy(reinterpret_cast<void*>(entry),bytes,sizeof(bytes));
    Check(VirtualProtect(reinterpret_cast<void*>(entry),12,PAGE_EXECUTE_READ,&old)!=0,"fixture executable, not RWX");
    Check(FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(entry),12)!=0,"fixture instruction cache refreshed");
}
}
namespace eth {
uintptr_t GameAddr(uintptr_t va) {return uintptr_t(globals)+va-0x400000;}
bool SafeReadBytes(uintptr_t address,void* output,size_t bytes) {
    ++reads;if(address<uintptr_t(globals) || address-uintptr_t(globals)>0x130000 || bytes>0x130000-(address-uintptr_t(globals))) return false;
    memcpy(output,reinterpret_cast<void*>(address),bytes);return true;
}
std::string Sha256FileHex(const std::string&) {++hashes;return supportedHash ? "49099fef26610e84f94f1aa2dd33aa9e83413ee24ff87fed63a8a98cc4f3daf0" : "unsupported-fixture";}
void LogMsg(LogLevel,const char*,...) {}
namespace game {uint32_t TickCount() {return tick;}}
}
int main() {
    using namespace eth::game;
    globals=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x130000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Check(globals!=nullptr,"isolated fixture allocation");
    NativeCode(5);Put(A(off::NativeTable)+128*4,A(0x416F10));
    const struct {uintptr_t va;const char* bytes;size_t count;} signatures[]={
        {0x42477b,"\xf6\xc1\x01",3},{0x424782,"\xe8\xd9\xc0\xfe\xff",5},
        {0x4247bf,"\xe8\x8c\xf4\xff\xff",5},{0x4247d4,"\xe8\xd7\xfb\xff\xff",5},
        {0x4247df,"\xe8\x2c\x6e\x00\x00",5},{0x4247e6,"\xe8\xf5\xf2\xfe\xff",5},
        {0x4247f7,"\x89\xbe\xa4\x18\x00\x00",6},{0x411594,"\x89\x90\x9c\x01\x51\x00",6},
        {0x413ae4,"\xe8\x57\xf4\xff\xff",5}};
    for(const auto& s:signatures) memcpy(reinterpret_cast<void*>(A(s.va)),s.bytes,s.count);
    MakeList();reads=hashes=0;
    SetEnabled(false);for(int i=0;i<100;++i) OnNativeTickCompleted();
    Check(!reads && !hashes && !ReadStatus().searches,"disabled production path has no engine/hash work");
    SetEnabled(true);OnNativeTickCompleted();
    Check(!ReadStatus().verified && !ReadStatus().requested,"unsupported executable refuses native work");
    supportedHash=true;SetEnabled(true);Check(ReadStatus().verified,"supported fingerprint/body/table/lifecycle fixture accepted");
    Put(A(off::StratIterCur),uint32_t(Address(0)));OnNativeTickCompleted();
    Check(!ReadStatus().searches,"busy native manager prevents search");Put(A(off::StratIterCur),uint32_t(0));
    Node before{};memcpy(&before,reinterpret_cast<void*>(Address(1)+0x190),sizeof(before));
    OnNativeTickCompleted();auto status=ReadStatus();
    Check(status.requested==2 && status.pending==2 && status.detected==2 && status.listComplete,"production runtime queues exact Rats, tracks deferred completion");
    Node after{};memcpy(&after,reinterpret_cast<void*>(Address(1)+0x190),sizeof(after));
    Check(!memcmp(&before,&after,sizeof(before)),"production native boundary leaves Ninja bytes unchanged");
    uint32_t flags=0;memcpy(&flags,reinterpret_cast<void*>(Address(0)+0x190),4);Check(flags==5,"synthetic native body executes the verified pending OR");
    for(tick=1;tick<15;++tick) OnNativeTickCompleted();Check(ReadStatus().searches==1,"production search cadence is simulation-tick based");
    OnNativeTickCompleted();Check(ReadStatus().searches==2 && ReadStatus().requested==2,"pending Rats are not resubmitted on next search");
    // Simulate the game's following tick invalidating Rat handles; Ninja stays.
    for(int i:{0,2}) {const uint32_t slot=(0x201+i)&0x1ffu;Put(A(off::StratMgrObject)+slot*8,uint64_t(0));}
    ++tick;OnNativeTickCompleted();Check(ReadStatus().removed==2 && !ReadStatus().pending,"telemetry confirms native generation removals, not just requests");
    SetEnabled(false);const int oldReads=reads;tick+=15;OnNativeTickCompleted();
    Check(reads==oldReads && ReadStatus().requested==2,"disabling stops future engine work without undoing removals");
    MakeList();SetEnabled(true);Put(Address(2)+0x198,uint32_t(Address(0)));tick+=15;OnNativeTickCompleted();
    Check(!ReadStatus().listComplete && ReadStatus().requested==2,"production corrupt-list path refuses partial deletion");
    MakeList();Put(A(off::StratMgrObject)+8,uint32_t(0x401));tick+=15;OnNativeTickCompleted();
    Check(ReadStatus().requested==3,"production generation mismatch excludes one Rat and preserves the other");
    NativeCode(0x11);tick+=15;OnNativeTickCompleted();
    Check(ReadStatus().faulted && ReadStatus().requested==3,"changed native bytes block subsequent requests");
    NativeCode(5);SetEnabled(false);SetEnabled(true);tick+=15;OnNativeTickCompleted();
    Check(ReadStatus().faulted && ReadStatus().requested==3,"native-guard fault is not automatically retried in the same run");
    SetEnabled(false);VirtualFree(globals,0,MEM_RELEASE);
    printf("PASS production PestControl tick/cdecl boundary, cadence, telemetry and refusal tests: %d assertions\n",checks);
}
