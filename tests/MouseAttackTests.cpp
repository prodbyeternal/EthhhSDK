#include "mouse_attack_policy.h"
#include <array>
#include <cstdio>
#include <cstdlib>

using namespace eth::input;
static int checks=0;
static void Check(bool ok,const char* name) { ++checks; if(!ok) { std::fprintf(stderr,"FAIL %s\n",name); std::exit(1); } }
int main() {
    AttackBinding slash,spin; slash.type=spin.type=0; slash.scan=0xcb; spin.scan=0xcd;
    MouseAttackPolicy policy;
    std::array<uint8_t,512> keys{},expected{};
    auto poll=[&](bool left,bool right,bool eligible=true) {
        keys.fill(0); return policy.Apply(keys.data(),keys.size(),slash,spin,eligible,left,right);
    };
    Check(!poll(true,false),"initial held click blocked until release");
    Check(poll(false,false),"release arms bridge");
    Check(poll(true,false) && keys[0xcb]==1 && keys[256+0xcb]==1 && !keys[0xcd],"M1 slash press");
    for(int i=0;i<300;++i) Check(poll(true,false) && keys[0xcb]==1 && keys[256+0xcb]==0,"slash hold has no repeated edges");
    Check(poll(false,false) && !keys[0xcb],"slash releases");
    Check(poll(false,true) && keys[0xcd]==1 && keys[256+0xcd]==1,"M2 spin press");
    for(int i=0;i<600;++i) Check(poll(false,true) && keys[0xcd]==1 && keys[256+0xcd]==0,"spin/hover remains held across native polls");
    Check(poll(true,true) && keys[0xcb] && keys[0xcd] && keys[256+0xcb] && !keys[256+0xcd],"independent buttons");
    Check(!poll(true,true,false),"focus/menu exclusion");
    Check(!poll(true,true),"manager held clicks cannot attack after return");
    Check(poll(false,false),"release after focus re-arms");
    keys.fill(0); keys[0xcd]=keys[256+0xcd]=1;
    Check(policy.Apply(keys.data(),512,slash,spin,true,false,false) && keys[0xcd] && keys[256+0xcd],"native keyboard remains usable");
    keys.fill(0); keys[0xcd]=1;
    Check(policy.Apply(keys.data(),512,slash,spin,true,false,true) && keys[0xcd] && !keys[256+0xcd],"mouse joining native hold has no extra press");
    keys.fill(0); keys[0xcd]=1;
    Check(policy.Apply(keys.data(),512,slash,spin,true,false,false) && keys[0xcd],"mouse release does not cancel physical key");
    keys.fill(0); keys[0xcd]=keys[256+0xcd]=1;
    Check(policy.Apply(keys.data(),512,slash,spin,true,false,false) && keys[256+0xcd],"native edge preserved even during previous merged hold");
    keys.fill(0); keys[0x39]=1; keys[0x11]=keys[256+0x11]=1; expected=keys;
    policy.Apply(keys.data(),512,slash,spin,true,true,true);
    expected[0xcb]=expected[0xcd]=1; expected[256+0xcb]=1;
    Check(keys==expected,"only two attack down/edge byte pairs changed");
    slash.scan=0x24; spin.scan=0x25;
    Check(!poll(true,false),"live remap requires button release");
    Check(poll(false,false) && poll(true,true) && keys[0x24] && keys[0x25] && !keys[0xcb],"live custom keyboard binding followed");
    for(uint32_t scan : {1u,0x11u,0x39u,0x1du,0x2au,0x43u,256u}) {
        slash.scan=scan; keys.fill(42); expected=keys;
        Check(!policy.Apply(keys.data(),512,slash,spin,true,true,true) && keys==expected,"unsafe custom key refused without writes");
    }
    slash.scan=spin.scan;
    Check(!poll(false,false),"overlapping action keys refused");
    slash.scan=0xcb; slash.gateType=0;
    Check(!poll(false,false),"unverified gated binding refused");
    Check(!policy.Apply(nullptr,512,slash,spin,true,true,true),"null buffer refused");
    Check(!policy.Apply(keys.data(),511,slash,spin,true,true,true),"short buffer refused");
    AttackBindingTable table;
    slash.gateType=6; spin.scan=0xcd; slash.scan=0xcb;
    slash.padding[0]=0xff; slash.padding[1]=0x7a;
    table[kSlashAction*2]=slash; table[kSpinAction*2]=spin;
    Check(AttackBindingsSupported(table),"native bool padding is ignored");
    for(size_t i=0;i<table.size();++i) if(i/2!=kSlashAction) {
        const auto saved=table[i]; table[i].type=0; table[i].scan=slash.scan;
        Check(!AttackBindingsSupported(table),"other action primary/alternate keyboard overlap refused");
        table[i]=saved; table[i].gateType=0; table[i].gateScan=slash.scan;
        Check(!AttackBindingsSupported(table),"other action keyboard gate overlap refused");
        table[i]=saved;
    }
    std::printf("PASS mouse attack native hold/edge policy: %d assertions\n",checks);
}
