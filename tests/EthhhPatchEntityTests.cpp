#include "ethhhpatch_entity_interpolation.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace eth::patch::entity;
static int checks=0;
static void Check(bool ok,const char* label) {
    ++checks; if (!ok) { fprintf(stderr,"FAIL %s\n",label); exit(1); }
}
static void Near(float a,float b,const char* label,float epsilon=0.0001f) {
    Check(std::isfinite(a) && std::abs(a-b)<=epsilon,label);
}
static Matrix Rotation(float angle,float x=0,float scale=1) {
    const float c=std::cos(angle),s=std::sin(angle);
    return Matrix{{c*scale,s*scale,0,0,-s, c,0,0,0,0,1,0,x,0,0,1}};
}
static Matrix AxisRotation(float angle,int axis,const float scale[3]) {
    Matrix out=Rotation(0);
    const int j=(axis+1)%3,k=(axis+2)%3;
    out.m[j*4+j]=out.m[k*4+k]=std::cos(angle);
    out.m[j*4+k]=std::sin(angle); out.m[k*4+j]=-std::sin(angle);
    for(int i=0;i<3;++i) {
        for(int col=0;col<3;++col) out.m[i*4+col]*=scale[i];
        out.m[12+i]=float(i+2);
    }
    return out;
}
static Matrix Multiply(const Matrix& a,const Matrix& b) {
    Matrix out;
    for(int row=0;row<4;++row) for(int col=0;col<4;++col)
        for(int i=0;i<4;++i) out.m[row*4+col]+=a.m[row*4+i]*b.m[i*4+col];
    return out;
}
struct Memory {
    std::vector<uint8_t> data=std::vector<uint8_t>(0x100000);
    uintptr_t fail=0, mutate=0;
    bool Read(uintptr_t addr,void* out,size_t count) {
        if (!addr || addr==fail || addr+count>data.size()) return false;
        memcpy(out,data.data()+addr,count);
        if (mutate && addr==0x20000) { ++data[0x30000+0x1A0]; mutate=0; }
        return true;
    }
    template<class T> void Put(uintptr_t addr,const T& value) { memcpy(data.data()+addr,&value,sizeof(value)); }
    void Word(uintptr_t addr,uint32_t value) { Put(addr,value); }
    void Init() {
        Word(0x301A8,0x40000); Word(0x301A0,0x205);
        Word(0x30268,0x41000); Word(0x3026C,0x42000); Word(0x30128,0x43000);
        Word(0x3013C,0x20000); Word(0x30140,2); Word(0x41000,2); Word(0x42024,2);
        Word(0x1FFF0,1); Word(0x1FFF4,0x20080); Word(0x1FFF8,0x45000);
        Word(0x10000+5*8,0x205); Word(0x10000+5*8+4,0x30000);
        Put(0x30010,Rotation(0)); Put(0x20000,Rotation(0)); Put(0x20040,Rotation(0));
        Word(0x43000,0x46000); Word(0x43004,0);
        Put<uint16_t>(0x4603C,0); Word(0x46044,0x47000);
        Put<uint16_t>(0x4703C,1); Word(0x47044,0);
    }
};

int main() {
    constexpr float pi=3.14159265358979323846f;
    // Round-trip Z rotations, signed scale, and translation without
    // shrinking the bone basis as an element-wise matrix lerp would.
    for (int i=-360;i<=360;++i) for (float scale:{-2.0f,1.0f,3.0f}) {
        Matrix m=Rotation(i*pi/180,7,scale); Affine a;
        Check(Decompose(m,a),"rotation/scale decomposition");
        Matrix round=Blend(a,a,0.37f);
        for (int j=0;j<16;++j) Near(round.m[j],m.m[j],"row-vector round trip");
    }
    for(int axis=0;axis<3;++axis) for(int angle=-180;angle<=180;angle+=3)
        for(int signs=0;signs<8;++signs) {
            const float scale[3]={(signs&1)?-2.f:2.f,(signs&2)?-3.f:3.f,(signs&4)?-.5f:.5f};
            Matrix m=AxisRotation(angle*pi/180,axis,scale); Affine pose;
            Check(Decompose(m,pose),"all axes and reflection combinations decompose");
            Matrix round=Blend(pose,pose,.63f);
            for(int j=0;j<16;++j) Near(round.m[j],m.m[j],"all-axis signed scale round trip");
        }
    for(int x=-180;x<=180;x+=30) for(int y=-180;y<=180;y+=30) for(int z=-180;z<=180;z+=30) {
        const float scale[3]={1,1,1};
        Matrix m=Multiply(Multiply(AxisRotation(x*pi/180,0,scale),AxisRotation(y*pi/180,1,scale)),AxisRotation(z*pi/180,2,scale));
        Affine pose; Check(Decompose(m,pose),"composed XYZ rotation decompose");
        Matrix round=Blend(pose,pose,.4f);
        for(int j=0;j<16;++j) Near(round.m[j],m.m[j],"composed XYZ round trip");
    }
    Affine a,b;
    Check(Decompose(Rotation(0),a) && Decompose(Rotation(pi/2,2),b),"quarter-turn endpoints");
    Matrix halfway=Blend(a,b,0.5f);
    Near(halfway.m[0],std::sqrt(0.5f),"slerp cosine");
    Near(halfway.m[1],std::sqrt(0.5f),"slerp sine"); Near(halfway.m[12],1,"translation lerp");
    for (float alpha:{0.0f,0.1f,0.5f,0.9f,1.0f}) {
        Matrix blended=Blend(a,b,alpha);
        Near(blended.m[0]*blended.m[0]+blended.m[1]*blended.m[1],1,"no rotation shrink");
    }
    Check(Decompose(Rotation(179*pi/180),a) && Decompose(Rotation(-179*pi/180),b),"wrap endpoints");
    Near(Blend(a,b,0.5f).m[0],-1,"shortest arc across quaternion sign");
    Matrix bad=Rotation(0); bad.m[4]=0.2f; Check(!Decompose(bad,a),"shear refused");
    bad=Rotation(0); bad.m[0]=0; Check(!Decompose(bad,a),"degenerate basis refused");
    bad=Rotation(0); bad.m[15]=0; Check(!Decompose(bad,a),"projective basis refused");
    bad=Rotation(0); bad.m[3]=0.1f; Check(!Decompose(bad,a),"non-affine refused");
    bad=Rotation(0); bad.m[8]=std::numeric_limits<float>::quiet_NaN(); Check(!Decompose(bad,a),"NaN refused");
    Check(Decompose(Rotation(0),a) && Decompose(Rotation(0,0,-1),b),"reflection endpoints");
    Check(!Continuous(a,b,8),"reflection flip discontinuity");
    Decompose(Rotation(0,9),b); Check(!Continuous(a,b,8),"teleport discontinuity");

    Memory mem; mem.Init(); const auto read=[&](uintptr_t p,void* d,size_t n){return mem.Read(p,d,n);};
    Key key;
    Check(ReadKey(0x30000,0x10000,key,read),"verified count/allocation/handle key");
    Check(key.count==2 && key.nodes==0x20000,"bounded bone contract");
    for (const auto& test:std::vector<std::pair<uintptr_t,uint32_t>>{
        {0x30140,0},{0x30140,513},{0x41000,3},{0x42024,3},{0x1FFF0,0},{0x1FFF4,0x2007F},
        {0x1FFF8,0},{0x10000+5*8,0x405},{0x10000+5*8+4,0x30100},{0x3013C,0x20001}}) {
        uint32_t old=0; mem.Read(test.first,&old,4); mem.Word(test.first,test.second);
        Key rejected; Check(!ReadKey(0x30000,0x10000,rejected,read),"malformed key refused");
        mem.Word(test.first,old);
    }
    Check(MeshFits(0x46000,2,read),"two-mesh iterative chain");
    for (uint16_t index:{uint16_t(2),uint16_t(0x8000),uint16_t(0xFFFE)}) {
        mem.Put(0x4703C,index); Check(!MeshFits(0x46000,2,read),"signed/out-of-bounds node refused");
    }
    mem.Put<uint16_t>(0x4703C,0xFFFF); Check(MeshFits(0x46000,2,read),"world-only mesh sentinel");
    mem.Word(0x47044,0x46000); Check(!MeshFits(0x46000,2,read),"cyclic mesh chain refused");
    mem.Word(0x47044,0); mem.Put<uint16_t>(0x4703C,1);

    Cache cache;
    Check(cache.BeginTick(10) && cache.Sample(key,0x10000,read),"first completed pose"); cache.EndTick();
    Check(cache.Size()==1 && cache.Paired()==0,"no interpolation before two ticks");
    Check(!cache.Render(key.owner,key.instance,Rotation(0),0x10000,10,0.5f,read),"first pose native fallback");
    mem.Put(0x30010,Rotation(0,1)); mem.Put(0x20040,Rotation(pi/2));
    Check(cache.BeginTick(11) && cache.Sample(key,0x10000,read),"second completed pose"); cache.EndTick();
    Check(cache.Paired()==1,"adjacent ticks paired");
    const auto before=mem.data;
    Pose* rendered=cache.Render(key.owner,key.instance,Rotation(0,1),0x10000,11,0.5f,read);
    Check(rendered && rendered->renderNodes.size()==2,"SDK-owned render inputs");
    Near(rendered->renderNodes[1].m[0],std::sqrt(0.5f),"bone interpolation");
    Near(rendered->renderWorld.m[12],0.5f,"entity-root interpolation");
    Check(mem.data==before,"zero native-memory writes");
    Check(!cache.BeginTick(11),"duplicate tick cannot rotate endpoints");
    for (int i=0;i<1000;++i) {
        float alpha=float(i%100)/100;
        rendered=cache.Render(key.owner,key.instance,Rotation(0,1),0x10000,11,alpha,read);
        Check(rendered!=nullptr,"many renders from same fixed-tick pair");
        Near(rendered->renderWorld.m[12],alpha,"render alpha independent of tick count");
    }
    Check(mem.data==before,"1000 renders do not advance scripts/animation");
    Check(!cache.Render(key.owner,key.instance+4,Rotation(0,1),0x10000,11,0.5f,read),"instance reuse refused");
    Check(!cache.Render(key.owner,key.instance,Rotation(0,2),0x10000,11,0.5f,read),"foreign root write preserved");
    Check(!cache.Render(key.owner,key.instance,Rotation(0,1),0x10000,12,0.5f,read),"stale tick refused");
    for (float alpha:{-1.0f,1.0f,std::numeric_limits<float>::quiet_NaN()})
        Check(!cache.Render(key.owner,key.instance,Rotation(0,1),0x10000,11,alpha,read),"invalid alpha native fallback");
    mem.Put(0x20040,Rotation(0));
    Check(!cache.Render(key.owner,key.instance,Rotation(0,1),0x10000,11,0.5f,read),"foreign bone write preserved");
    mem.Put(0x20040,Rotation(pi/2)); mem.Word(0x301A0,0x405);
    Check(!cache.Render(key.owner,key.instance,Rotation(0,1),0x10000,11,0.5f,read),"owner generation reuse refused");
    mem.Word(0x301A0,0x205);
    Check(cache.BeginTick(13) && cache.Sample(key,0x10000,read),"missed-tick resample"); cache.EndTick();
    Check(cache.Paired()==0,"missed tick breaks interpolation");
    mem.Put(0x30010,Rotation(0,100));
    Check(cache.BeginTick(14) && cache.Sample(key,0x10000,read),"teleport sample"); cache.EndTick();
    Check(cache.Paired()==0,"teleport snaps to native");
    cache.BeginTick(15); cache.EndTick(); Check(cache.Size()==0,"despawn prunes lifetime");
    mem.Init(); mem.mutate=1;
    cache.BeginTick(16); Check(!cache.Sample(key,0x10000,read),"identity changes during sample refused"); cache.EndTick();
    mem.Init(); mem.fail=0x20000;
    cache.BeginTick(17); Check(!cache.Sample(key,0x10000,read),"unreadable bone array refused"); cache.EndTick();
    mem.fail=0; cache.Reset();
    cache.BeginTick(UINT32_MAX); Check(cache.Sample(key,0x10000,read),"tick wrap first sample"); cache.EndTick();
    cache.BeginTick(0); Check(cache.Sample(key,0x10000,read),"tick wrap second sample"); cache.EndTick();
    Check(cache.Paired()==1,"unsigned adjacent tick wrap");
    cache.Reset(); Check(cache.Size()==0,"deactivation clears poses");

    mem.Init(); mem.Word(0x3012C,0x44000); mem.Word(0x30144,1);
    Check(ReadKey(0x30000,0x10000,key,read),"double-buffered key");
    cache.BeginTick(20); Check(cache.Sample(key,0x10000,read),"double-buffered first sample"); cache.EndTick();
    cache.BeginTick(21); Check(cache.Sample(key,0x10000,read),"double-buffered second sample"); cache.EndTick();
    for(int frame=0;frame<100;++frame) {
        const uint32_t active=(frame&1)?0x43000:0x44000, other=(frame&1)?0x44000:0x43000;
        mem.Word(0x30128,active); mem.Word(0x3012C,other);
        Check(cache.Render(key.owner,active,Rotation(0),0x10000,21,.5f,read)!=nullptr,"verified per-render instance swap stays smooth");
        Check(!cache.Render(key.owner,other,Rotation(0),0x10000,21,.5f,read),"inactive swapped instance refused");
    }
    mem.Word(0x30128,0x44000); mem.Word(0x3012C,0x43000);
    Key swapped; Check(ReadKey(0x30000,0x10000,swapped,read),"post-swap tick key");
    cache.BeginTick(22); Check(cache.Sample(swapped,0x10000,read),"swapped tick sample"); cache.EndTick();
    Check(cache.Paired()==1,"verified instance swaps do not break adjacent tick pair");
    mem.Word(0x3012C,0x45000);
    Check(!cache.Render(key.owner,0x44000,Rotation(0),0x10000,22,.5f,read),"replacement alternate instance refused");
    mem.Word(0x3012C,0x43000); mem.Word(0x30144,0);
    Check(!cache.Render(key.owner,0x44000,Rotation(0),0x10000,22,.5f,read),"changed double-buffer mode refused");
    mem.Word(0x30144,1); mem.Put(0x30010,Rotation(0,3));
    Check(!cache.Render(key.owner,0x44000,Rotation(0),0x10000,22,.5f,read),"stale submitted root cannot bypass live-root guard");

    // Hard resource ceilings, including maximal native bone arrays and mesh
    // chains. Bounds refusal must never turn into an unbounded render walk.
    for(uint32_t count:{kMaxNodes,1u}) {
        mem.Init(); cache.Reset();
        mem.Word(0x30140,count); mem.Word(0x41000,count); mem.Word(0x42024,count);
        mem.Word(0x1FFF4,0x20000+count*64);
        for(uint32_t i=0;i<count;++i) mem.Put(0x20000+i*64,Rotation(0));
        cache.BeginTick(30);
        const uint32_t accepted=count==kMaxNodes ? uint32_t(kMaxTotalNodes/kMaxNodes) : uint32_t(kMaxEntities);
        for(uint32_t i=0;i<=accepted;++i) {
            uint32_t owner=0x60000+i*0x300,handle=0x200+i;
            memcpy(mem.data.data()+owner,mem.data.data()+0x30000,0x270);
            mem.Word(owner+0x1A0,handle);
            mem.Word(0x10000+(handle&0x1FF)*8,handle); mem.Word(0x10000+(handle&0x1FF)*8+4,owner);
            Key bounded; Check(ReadKey(owner,0x10000,bounded,read),"resource-cap fixture identity");
            Check(cache.Sample(bounded,0x10000,read)==(i<accepted),"bounded entity/total-bone sampling");
        }
        cache.EndTick(); Check(cache.Size()==accepted,"exact resource ceiling");
    }
    for(uint32_t i=0;i<513;++i) {
        uint32_t mesh=0x50000+i*0x50;
        mem.Put<uint16_t>(mesh+0x3C,0); mem.Word(mesh+0x44,i==511 ? 0 : mesh+0x50);
    }
    Check(MeshFits(0x50000,1,read),"512-mesh bound accepted");
    mem.Word(0x50000+511*0x50+0x44,0x50000+512*0x50);
    Check(!MeshFits(0x50000,1,read),"513-mesh chain refused");
    Check(!MeshFits(UINT32_MAX-0x20,1,read),"mesh address overflow refused");

    ReadScope scope;
    size_t queryCalls=0,copyCalls=0;
    bool queryOk=true,copyOk=true;
    const auto query=[&](uintptr_t p,ReadableRegion& region) {
        ++queryCalls; region={0x10000,0x90000}; return queryOk && p>=region.begin && p<region.end;
    };
    const auto copy=[&](uintptr_t p,void* out,size_t n) { ++copyCalls; return copyOk && mem.Read(p,out,n); };
    uint32_t value=0;
    for(int i=0;i<1000;++i) Check(scope.Read(0x30140,&value,4,query,copy),"scoped readable copy");
    Check(queryCalls==1 && scope.Queries()==1 && copyCalls==1000 && scope.Copies()==1000,"one page query, every copy protected");
    Check(scope.Bytes()==4000,"bounded copy-byte telemetry");
    mem.Word(0x30140,17);
    Check(scope.Read(0x30140,&value,4,query,copy) && value==17,"scope caches permissions, never native bytes");
    copyOk=false;
    Check(!scope.Read(0x30140,&value,4,query,copy) && queryCalls==1,"cached permission does not suppress protected-copy failure");
    copyOk=true; scope.Reset(); queryOk=false;
    Check(!scope.Read(0x30140,&value,4,query,copy) && scope.Copies()==0,"new phase revalidates revoked region");
    queryOk=true; scope.Reset();
    Check(!scope.Read(0x8FFFF,&value,4,query,copy),"cross-region copy refused");
    const size_t q=scope.Queries();
    Check(!scope.Read(0,&value,4,query,copy) && !scope.Read(0x30140,nullptr,4,query,copy) &&
        !scope.Read(0x30140,&value,0,query,copy) && !scope.Read(UINT32_MAX-1,&value,4,query,copy),"invalid/overflow copies refused");
    Check(scope.Queries()==q && scope.Copies()==0,"invalid copy never queries or copies");
    const auto pages=[&](uintptr_t p,ReadableRegion& region) { region={p&~uintptr_t(0xFFF),(p&~uintptr_t(0xFFF))+0x1000}; return true; };
    scope.Reset();
    for(uint32_t i=0;i<100;++i) Check(scope.Read(0x10000+i*0x1000,&value,4,pages,copy),"region cache overflow stays bounded and safe");
    Check(scope.Queries()==100,"one query per distinct region");

    OwnerList owners;
    mem.Word(0x1000,0x30000); mem.Word(0x30198,0x31000); mem.Word(0x31198,0);
    Check(owners.Collect(0x1000,read,[]{return true;}) && owners.Size()==2 && owners[0]==0x30000 && owners[1]==0x31000,"complete lightweight owner list");
    mem.Word(0x31198,0x30000);
    Check(!owners.Collect(0x1000,read,[]{return true;}),"owner-list cycle refused");
    mem.Word(0x31198,0x31000);
    Check(!owners.Collect(0x1000,read,[]{return true;}),"owner-list self-link refused");
    mem.Word(0x31198,0); mem.fail=0x31198;
    Check(!owners.Collect(0x1000,read,[]{return true;}),"unreadable owner link is incomplete"); mem.fail=0;
    Check(!owners.Collect(0x1000,read,[]{return false;}),"sampling budget cannot accept an incomplete list");
    bool changedHead=false;
    const auto changingList=[&](uintptr_t p,void* out,size_t n) {
        const bool ok=mem.Read(p,out,n);
        if(p==0x31198 && !changedHead) { mem.Word(0x1000,0x31000); changedHead=true; }
        return ok;
    };
    Check(!owners.Collect(0x1000,changingList,[]{return true;}),"changed list head refused");
    mem.Word(0x1000,0);
    Check(owners.Collect(0x1000,read,[]{return true;}) && !owners.Size(),"empty terminated list complete");
    mem.data.resize(0x500000);
    for(uint32_t i=0;i<8193;++i) mem.Word(0x10000+i*0x200+0x198,i==8191?0:0x10000+(i+1)*0x200);
    mem.Word(0x1000,0x10000);
    Check(owners.Collect(0x1000,read,[]{return true;}) && owners.Size()==8192,"8192-owner complete-list cap accepted");
    mem.Word(0x10000+8191*0x200+0x198,0x10000+8192*0x200);
    Check(!owners.Collect(0x1000,read,[]{return true;}),"8193-owner list refused, never treated as complete");

    WorkBudget budget; budget.Reset(2000);
    Check(budget.Available(),"optional work budget starts available");
    budget.Charge(1000,1500); budget.Charge(100000,101499);
    Check(budget.Available(),"native renderer gap is not charged");
    budget.Charge(200000,200001);
    Check(!budget.Available(),"exact budget boundary stops new interpolation");
    budget.Charge(200000,INT64_MAX);
    Check(!budget.Available(),"budget charge saturates without overflow");
    budget.Reset(0); Check(!budget.Available(),"unavailable clock uses native fallback");
    budget.Reset(2000); budget.Charge(0,1); Check(!budget.Available(),"failed clock closes optional budget");
    budget.Reset(2000); budget.Charge(100,99); Check(!budget.Available(),"reversed clock closes optional budget");

    // Stable scene: both affine endpoint buffers rotate without losing their
    // capacity, and native/render vectors retain allocations after warm-up.
    mem.Init(); cache.Reset(); ReadKey(0x30000,0x10000,key,read);
    for(uint32_t tick=100;tick<103;++tick) { cache.BeginTick(tick); Check(cache.Sample(key,0x10000,read),"capacity warmup sample"); cache.EndTick(); }
    rendered=cache.Render(key.owner,key.instance,Rotation(0),0x10000,102,.5f,read);
    Check(rendered!=nullptr,"capacity fixture render");
    auto nativeStorage=rendered->nativeNodes.data(),renderStorage=rendered->renderNodes.data();
    auto endpointA=rendered->nodes.data(),endpointB=rendered->previousNodes.data();
    for(uint32_t tick=103;tick<1103;++tick) {
        cache.BeginTick(tick); Check(cache.Sample(key,0x10000,read),"reused pose sample"); cache.EndTick();
        rendered=cache.Render(key.owner,key.instance,Rotation(0),0x10000,tick,.5f,read);
        Check(rendered && rendered->nativeNodes.data()==nativeStorage && rendered->renderNodes.data()==renderStorage,"matrix buffers retain allocation");
        Check((rendered->nodes.data()==endpointA && rendered->previousNodes.data()==endpointB) ||
            (rendered->nodes.data()==endpointB && rendered->previousNodes.data()==endpointA),"two affine buffers reused");
    }
    mem.Word(0x301A0,0x405);
    Check(!cache.Render(key.owner,key.instance,Rotation(0),0x10000,1102,.5f,read),"cached blend never bypasses fresh identity guards");
    printf("PASS entity render-only interpolation: %d assertions\n",checks);
}
