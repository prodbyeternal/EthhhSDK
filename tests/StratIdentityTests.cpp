// Offline policy tests: no engine functions, process access or user settings.
#include "strat_identity.h"
#include "ninja_control_policy.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

using namespace eth::game;
static int checks = 0;
static void Check(bool ok, const char* label) { ++checks; if (!ok) { fprintf(stderr,"FAIL %s\n",label); exit(1); } }
struct Memory {
    std::map<uintptr_t, unsigned char> bytes;
    bool Read(uintptr_t addr, void* out, size_t size) {
        for (size_t i=0;i<size;++i) if (!bytes.count(addr+i)) return false;
        for (size_t i=0;i<size;++i) ((unsigned char*)out)[i]=bytes[addr+i];
        return true;
    }
    void Put(uintptr_t addr, const void* data, size_t size) {
        for(size_t i=0;i<size;++i) bytes[addr+i]=((const unsigned char*)data)[i];
    }
    void Word(uintptr_t addr,uint32_t value) { Put(addr,&value,4); }
};
struct Strat { uintptr_t addr; char className[kClassNameCapacity]; uint32_t selfHandle; };

int main()
{
    Memory memory; auto read=[&](uintptr_t addr,void* out,size_t size){return memory.Read(addr,out,size);};
    char name[kClassNameCapacity]; memset(name,'x',sizeof(name));
    Check(!ReadClassName(0,name,read) && !name[0],"null class clears output");
    memory.Word(0x1004,0x2000); memory.Put(0x2000,"Ninja",6);
    Check(ReadClassName(0x1000,name,read) && !strcmp(name,"Ninja"),"bounded boundary fallback reads exact name");
    char padded[64]={}; memcpy(padded,"NinjaRageAttachment",19); memory.Put(0x2000,padded,sizeof(padded));
    Check(ReadClassName(0x1000,name,read) && ClassRole(name)==StratRole::Attachment,"bounded bulk copy");
    memory.bytes[0x2002]=1;
    Check(!ReadClassName(0x1000,name,read) && !name[0],"control byte refused");
    memory.bytes[0x2002]=0x80;
    Check(!ReadClassName(0x1000,name,read),"non-ASCII refused");
    memset(padded,'A',64); memory.Put(0x2000,padded,64);
    Check(!ReadClassName(0x1000,name,read) && !name[0],"unterminated name refused, not truncated");
    padded[63]=0; memory.Put(0x2000,padded,64);
    Check(ReadClassName(0x1000,name,read) && strlen(name)==63,"maximum supported terminated name");
    memory.bytes[0x2000]=0;
    Check(!ReadClassName(0x1000,name,read),"empty name refused");
    memory.Word(0x1004,0xfffffff0);
    Check(!ReadClassName(0x1000,name,read),"name address overflow refused");
    Check(!ReadClassName((std::numeric_limits<uintptr_t>::max)()-3,name,read),"class address overflow refused");
    memory.Word(0x1004,0x2000); memory.Put(0x2000,"Ninja",6);
    int pointerReads=0;
    auto racing=[&](uintptr_t addr,void* out,size_t size){
        if(addr==0x1004 && ++pointerReads==2) memory.Word(addr,0x3000);
        return memory.Read(addr,out,size);
    };
    Check(!ReadClassName(0x1000,name,racing) && !name[0],"changed name pointer refused");
    for(const char* text : {"NinjaHead","NinjaSword","NinjaRageAttachment"})
        Check(ClassRole(text)==StratRole::Attachment,"attachment roles");
    for(const char* text : {"NinjaSpeech","Camera"})
        Check(ClassRole(text)==StratRole::Related && HasKnownOwnerField(text),"corroborated related owners");
    Check(ClassRole("Ninja")==StratRole::Ninja && !HasKnownOwnerField("Ninja"),"root is not an attachment owner consumer");
    for(const char* text : {"ninja","NinjaHeadExtra","NinjaClone"," Ninja","Ninja "})
        Check(ClassRole(text)==StratRole::Other,"no prefix/case/proximity identity guessing");
    Check(!HasKnownOwnerField("NinjaHead") && !HasKnownOwnerField("NinjaSword"),"unknown attachment owner fields not inferred");
    Check(ClassRole(nullptr)==StratRole::Unknown && !HasKnownOwnerField(nullptr),"null role policy");
    const uintptr_t manager=0x4000;
    const uint32_t handle=0x200001ff;
    const uintptr_t slot=manager+(handle&0x1ff)*8;
    memory.Word(slot,handle); memory.Word(slot+4,0x9000);
    Check(ResolveOwnerHandle(manager,handle,read)==0x9000,"slot mask and full generation handle");
    Check(!ResolveOwnerHandle(manager,0x201ff,read),"matching slot with wrong generation refused");
    Check(!ResolveOwnerHandle(manager,0,read),"null handle refused");
    int slotReads=0;
    auto reused=[&](uintptr_t addr,void* out,size_t size){
        if(addr==slot && ++slotReads==2) memory.Word(slot+4,0xa000);
        return memory.Read(addr,out,size);
    };
    Check(!ResolveOwnerHandle(manager,handle,reused),"owner slot reuse during read refused");
    std::vector<Strat> list={{1,"NinjaSpeech",0x123},{2,"NinjaHead",0x124},{3,"Ninja",0x125},{4,"NinjaSword",0x126}};
    Check(UniqueNinja(list)==3,"exact Ninja beats followers");
    list.push_back({5,"",0x127}); Check(!UniqueNinja(list),"unreadable class cannot prove uniqueness"); list.pop_back();
    list.push_back({5,"Ninja",0x127}); Check(!UniqueNinja(list),"two exact Ninja roots are ambiguous");
    list.pop_back(); list[2].selfHandle=0; Check(!UniqueNinja(list),"uninitialized Ninja handle refused");
    list.erase(list.begin()+2); Check(!UniqueNinja(list),"no root never falls back to followers");
    printf("PASS strat identity: %d assertions (offline; no live identity/model claim)\n",checks);
    checks = 0;
    NinjaPinConsent consent;
    Check(!consent.AllowsReacquire(true), "fresh manual mode cannot create pin consent");
    Check(!consent.AllowsReacquire(false), "fresh auto mode cannot reacquire");
    consent.Selected(false);
    Check(!consent.AllowsReacquire(true), "attachment/ambiguous/incomplete selection cannot authorize reacquire");
    consent.Selected(true);
    Check(consent.AllowsReacquire(true), "explicit unique exact-Ninja selection authorizes reacquire");
    Check(!consent.AllowsReacquire(false), "even prior pin cannot reacquire in another mode");
    Check(consent.AllowsReacquire(true), "player loss does not itself revoke explicit intent");
    consent.Selected(false);
    Check(!consent.AllowsReacquire(true), "different or failed explicit selection revokes consent");
    consent.Selected(true); consent.Revoke();
    Check(!consent.AllowsReacquire(true), "reset/mode switch/learning revokes consent");

    std::array<unsigned char, 0x30> pose{};
    for (size_t i = 0; i < pose.size(); ++i) pose[i] = (unsigned char)(i * 7 + 3);
    const auto original = pose;
    const uintptr_t base = (uintptr_t)pose.data();
    int writes = 0;
    auto write = [&](uintptr_t addr, const void* data, size_t size) {
        ++writes;
        if (addr < base || addr - base > pose.size() || size > pose.size() - (addr - base)) return false;
        memcpy((void*)addr, data, size); return true;
    };
    std::vector<float> yaws;
    for (int i = -64; i <= 64; ++i) yaws.push_back(i * 0.25f);
    yaws.push_back((std::numeric_limits<float>::max)());
    yaws.push_back(-(std::numeric_limits<float>::max)());
    for (float yaw : yaws) {
        pose = original; writes = 0;
        Check(WriteNinjaFacingPose(base, base + 0x10, yaw, write) && writes == 2, "facing uses one yaw scalar and one quaternion write");
        float units = 0, q[4]{};
        memcpy(&units, pose.data() + 4, sizeof(units));
        memcpy(q, pose.data() + 0x10, sizeof(q));
        Check(std::isfinite(units) && std::abs(units) <= 2048.01f, "finite wrapped facing script units");
        Check(q[0] == 0 && q[2] == 0 && std::abs(q[1]*q[1]+q[3]*q[3]-1) < 0.000001f, "finite unit yaw quaternion");
        Check(std::abs(std::sin(double(units) * 6.283185307179586 / 4096) - 2*q[1]*q[3]) < 0.000001f,
            "yaw scalar and quaternion agree on forward X");
        auto unchanged = pose;
        memcpy(unchanged.data() + 4, original.data() + 4, sizeof(float));
        memcpy(unchanged.data() + 0x10, original.data() + 0x10, sizeof(q));
        Check(unchanged == original, "pitch/roll/fourth-slot padding and neighboring bytes are untouched");
    }
    for (float invalid : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()}) {
        pose = original; writes = 0;
        Check(!WriteNinjaFacingPose(base, base + 0x10, invalid, write) && !writes && pose == original,
            "nonfinite yaw performs no writes");
    }
    writes = 0;
    Check(!WriteNinjaFacingPose(0, base + 0x10, 0, write) && !writes, "null angle address refused");
    Check(!WriteNinjaFacingPose(base, 0, 0, write) && !writes, "null quaternion address refused");
    int attempts = 0;
    auto refuse = [&](uintptr_t, const void*, size_t) { ++attempts; return false; };
    Check(!WriteNinjaFacingPose(base, base + 0x10, 0, refuse) && attempts == 1, "failed yaw write stops before quaternion");
    printf("PASS native Ninja consent/facing: %d assertions (offline; no engine transform-refresh claim)\n", checks);
}
