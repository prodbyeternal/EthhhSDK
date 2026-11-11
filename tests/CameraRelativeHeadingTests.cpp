#include "camera_relative_heading.h"
#include "camera_relative_heading_thunk.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace eth::patch::runtime::movement;
static int checks = 0;
static void Check(bool ok, const char* name) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL %s\n", name); std::exit(1); }
}
static VisibleHeading Visible(uint32_t tick = 100, double time = 1) {
    return {{0x2000,0xB000,0x601},{0x3000,0xB100,0x602},0xC000,0xD000,tick,time,1.57079632679f};
}
static Gates Ready() { Gates g; g.active=g.following=g.focused=true; g.timeStep=1; return g; }

struct Memory {
    std::array<uint8_t,0x10000> bytes{};
    size_t reads = 0, writes = 0, failRead = 0;
    bool failWrite = false;
    Context context{true,Visible(),0x5000,0x1000,0x1004,0x1008,0x4252EA};
    Call call{0x2000,0x2000,0x91D4,0x4252EA,0xA004,0x6A};
    template<class T> void Set(uintptr_t address, T value) { std::memcpy(bytes.data()+address,&value,sizeof(value)); }
    Memory() {
        bytes.fill(0x5A);
        Set<uint32_t>(0x1000,0x3000); Set<uint32_t>(0x1004,0xC000); Set<uint32_t>(0x1008,0xD000);
        for (const Owner owner : {context.visible.player,context.visible.cameraOwner}) {
            Set<uint32_t>(owner.addr+0x1A8,uint32_t(owner.pClass)); Set(owner.addr+0x1A0,owner.handle);
            const uintptr_t slot=context.manager+(owner.handle&0x1FF)*8;
            Set(slot,owner.handle); Set<uint32_t>(slot+4,uint32_t(owner.addr));
        }
        Set<uint32_t>(0x3238,0x601); Set<uint32_t>(0x21B4,0x9000);
        const uint32_t words[5]={0x4275106A,0x42761091,0x4276500E,0x0B747576,0x4374701F};
        std::memcpy(bytes.data()+0xA000,words,sizeof(words));
        Set<float>(0x3174,-1024); Set<float>(call.output,512);
    }
    bool Run() {
        auto read=[&](uintptr_t a,void* out,size_t n) {
            ++reads;
            if (reads==failRead || a>bytes.size() || n>bytes.size()-a) return false;
            std::memcpy(out,bytes.data()+a,n); return true;
        };
        auto write=[&](uintptr_t a,const void* in,size_t n) {
            ++writes;
            Check(a==call.output && n==4,"only verified VM scratch float is written");
            if (failWrite) return false;
            std::memcpy(bytes.data()+a,in,n); return true;
        };
        return Route(context,call,read,write);
    }
};

// Test the PRODUCTION naked thunk, not a reimplementation. No game/engine call.
static uintptr_t gotSelf,gotEdx,gotOutput,gotOther,gotCaller,gotPC,expectedCaller;
static uint32_t gotField,testField,beforeStack,afterStack,gotResult,gotEsi,gotEdi,gotEbx,gotEbp;
static uint32_t __fastcall FakeScoped(void* self,void* edx,void* output,void* other,
    uint32_t field,uintptr_t caller,uintptr_t pc) {
    gotSelf=uintptr_t(self); gotEdx=uintptr_t(edx); gotOutput=uintptr_t(output); gotOther=uintptr_t(other);
    gotField=field; gotCaller=caller; gotPC=pc;
    return 0xA5B00000u | field;
}
ETH_HEADING_THUNK(TestThunk, FakeScoped)
__declspec(naked) static void InvokeThunk() {
    __asm {
        push ebx
        push esi
        push edi
        push ebp
        mov esi, 0A004h
        mov edi, 12345678h
        mov ebx, 87654321h
        mov ebp, 13579BDFh
        mov beforeStack, esp
        mov eax, offset continuation
        mov expectedCaller, eax
        mov ecx, 2000h
        mov edx, 2468ACE0h
        push testField
        push 2000h
        push 91D4h
        call TestThunk
    continuation:
        mov afterStack, esp
        mov gotResult, eax
        mov gotEsi, esi
        mov gotEdi, edi
        mov gotEbx, ebx
        mov gotEbp, ebp
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
    }
}

int main() {
    constexpr double turn=6.28318530717958647692;
    for (int owner=-8192;owner<=8192;owner+=137) for (int yaw=-180;yaw<=180;yaw+=15)
        for (float heading : {-2048.f,-1536.f,-1024.f,-512.f,0.f,512.f,1024.f,1536.f,2048.f}) {
            float out=0;
            const float radians=float(yaw*turn/360);
            Check(Compensate(heading,radians,float(owner),out),"finite native heading compensated");
            const double expected=heading*turn/4096+radians;
            const double actual=(out+owner)*turn/4096;
            Check(std::abs(std::sin(expected)-std::sin(actual))<0.000003 &&
                std::abs(std::cos(expected)-std::cos(actual))<0.000003,"WASD/diagonal world heading follows displayed yaw with correct signs");
            Check(out>=-2048 && out<=2048,"angle wrap bounded across multiple native turns");
        }
    for (float bad : {std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        float output=123;
        Check(!Compensate(bad,0,0,output) && output==123,"bad native heading leaves output untouched");
        Check(!Compensate(0,bad,0,output) && output==123,"bad visible yaw leaves output untouched");
        Check(!Compensate(0,0,bad,output) && output==123,"bad camera yaw leaves output untouched");
    }

    RoutingPolicy policy;
    policy.Begin(100,1,Ready()); Check(!policy.Eligible(),"no routing before first successful primary orbit render");
    policy.Rendered(Visible()); policy.Begin(100,1.01,Ready()); Check(policy.Eligible(),"last displayed heading leased for one native tick");
    policy.End(); Check(!policy.Eligible(),"native tick end clears lease, including exception cleanup");
    for (unsigned mask=0;mask<512;++mask) {
        Gates g=Ready();
        if(mask&1) g.active=false; if(mask&2) g.following=false; if(mask&4) g.focused=false;
        if(mask&8) g.overlay=true; if(mask&16) g.externalCamera=true; if(mask&32) g.leasedKeys=true;
        if(mask&64) g.movie=true; if(mask&128) g.restoreFailed=true; if(mask&256) g.timeStep=0;
        policy.Reset(); policy.Rendered(Visible()); policy.Begin(100,1.01,g);
        Check(policy.Eligible()==(mask==0),"all inactive/scripted/movie/HL/F9/focus/restore guards compose");
        if(mask) { policy.Begin(100,1.02,Ready()); Check(!policy.Eligible(),"handoff requires a fresh displayed camera, not stale yaw"); }
    }
    for (uint32_t age : {0u,1u,4u,5u,100u}) {
        policy.Reset(); policy.Rendered(Visible(UINT32_MAX-1)); policy.Begin(uint32_t(UINT32_MAX-1+age),1.01,Ready());
        Check(policy.Eligible()==(age<=4),"catch-up bound survives tick wraparound");
    }
    for (double age : {-0.01,0.0,0.25,0.251,std::numeric_limits<double>::quiet_NaN()}) {
        policy.Reset(); policy.Rendered(Visible()); policy.Begin(100,1+age,Ready());
        Check(policy.Eligible()==(std::isfinite(age) && age>=0 && age<=0.25),"stale/future/nonfinite render timestamps refused");
    }
    policy.Reset();
    Gates control=Ready(); control.control=true;
    for (unsigned i=0;i<90;++i) {
        policy.Rendered(Visible(100+i,1+i/60.0)); policy.Begin(100+i,1+i/60.0,control);
        Check(!policy.Eligible() && policy.TraversalTicks()==30,"held Ctrl always yields without player/input edits");
    }
    for (unsigned i=1;i<=35;++i) {
        policy.Rendered(Visible(200+i,3+i/60.0)); policy.Begin(200+i,3+i/60.0,Ready());
        Check(policy.Eligible()==(i>=30),"Ctrl release grace expires rather than refreshing from native motion");
    }

    Memory good;
    const auto baseline=good.bytes;
    Check(good.Run() && good.writes==1,"exact verified Ninja/Camera/VM contract routes");
    for (size_t i=0;i<good.bytes.size();++i) if(i<good.call.output || i>=good.call.output+4)
        Check(good.bytes[i]==baseline[i],"axes/actions/strat transforms/speed/camera/adjacent script registers unchanged");
    const size_t readCount=good.reads;
    for(size_t fail=1;fail<=readCount;++fail) {
        Memory m; m.failRead=fail; const auto before=m.bytes;
        Check(!m.Run() && m.writes==0 && m.bytes==before,"every protected-read failure leaves native result intact");
    }
    for(unsigned mode=0;mode<16;++mode) {
        Memory m;
        switch(mode) {
        case 0:m.context.enabled=false;break;
        case 1:m.call.field=0x69;break;
        case 2:m.call.caller++;break;
        case 3:m.call.self++;break;
        case 4:m.call.other++;break;
        case 5:m.call.output+=4;break;
        case 6:m.call.nextInstruction++;break;
        case 7:m.call.nextInstruction=0;break;
        case 8:m.call.nextInstruction=UINT32_MAX-3;break;
        case 9:m.context.visible.player.handle=0;break;
        case 10:m.context.visible.cameraOwner.pClass=0;break;
        case 11:m.Set<uint32_t>(0x1000,0x3004);break;
        case 12:m.Set<uint32_t>(0x1004,0xC004);break;
        case 13:m.Set<uint32_t>(0x1008,0xD004);break;
        case 14:m.Set<uint32_t>(0x3238,0x603);break;
        case 15:m.Set<uint32_t>(0x21B4,UINT32_MAX-8);break;
        }
        const auto before=m.bytes;
        Check(!m.Run() && !m.writes && m.bytes==before,"malformed/replaced/foreign owners, output and caller remain native");
    }
    for (unsigned word=0;word<5;++word) for (unsigned bit=0;bit<32;++bit) {
        Memory m; m.bytes[0xA000+word*4+bit/8]^=uint8_t(1u<<(bit%8)); const auto before=m.bytes;
        Check(!m.Run() && !m.writes && m.bytes==before,"each ASL contract bit is guarded, including selector F0 vs 10");
    }
    for(uintptr_t at : {0x21A0u,0x21A8u,0x31A0u,0x31A8u,0x5008u,0x500Cu,0x5010u,0x5014u}) {
        Memory m; m.Set<uint32_t>(at,0); const auto before=m.bytes;
        Check(!m.Run() && !m.writes && m.bytes==before,"identity/class/generation replacement refused");
    }
    // Intervening changes after the first snapshot must not get overwritten.
    for (uintptr_t changed : {0x1000u,0x1004u,0x1008u,0x21B4u,0x21A0u,0x31A0u,0x3238u,0x3174u,0x91D4u,0xA004u}) {
        Memory m; size_t reads=0,writes=0; bool mutated=false;
        auto read=[&](uintptr_t a,void* out,size_t n) {
            ++reads;
            if(!mutated && reads==17) { m.Set<uint32_t>(changed,0); mutated=true; }
            if(a>m.bytes.size() || n>m.bytes.size()-a) return false;
            std::memcpy(out,m.bytes.data()+a,n); return true;
        };
        auto write=[&](uintptr_t,const void*,size_t) { ++writes; return true; };
        Check(!Route(m.context,m.call,read,write) && mutated && !writes,"intervening owner/VM/yaw changes are preserved");
    }
    { Memory m; m.failWrite=true; const auto before=m.bytes;
      Check(!m.Run() && m.writes==1 && m.bytes==before,"failed scratch write cannot affect any native state"); }

    for (unsigned i=0;i<1000;++i) {
        testField=i%184; InvokeThunk();
        Check(beforeStack==afterStack,"production thunk consumes exactly three native stack arguments");
        Check(gotSelf==0x2000 && gotEdx==0x2468ACE0 && gotOutput==0x91D4 && gotOther==0x2000 &&
            gotField==testField && gotCaller==expectedCaller && gotPC==0xA004,"production thunk preserves exact native caller/ESI/fastcall arguments");
        Check(gotResult==(0xA5B00000u|testField) && gotEsi==0xA004 && gotEdi==0x12345678 &&
            gotEbx==0x87654321 && gotEbp==0x13579BDF,"production thunk preserves native EAX and all nonvolatile registers");
    }
    std::printf("PASS scoped camera-relative native heading / Win32 ABI: %d assertions (%zu protected reads per accepted call)\n",checks,readCount);
}
