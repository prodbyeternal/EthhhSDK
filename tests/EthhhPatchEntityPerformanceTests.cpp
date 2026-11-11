#include "ethhhpatch_entity_reader.h"
#include "util.h"
#include <cstdio>
#include <cstdlib>
#include <new>

using namespace eth::patch::entity;
static size_t allocations=0;
void* operator new(size_t bytes) { ++allocations; if(void* p=malloc(bytes?bytes:1)) return p; throw std::bad_alloc(); }
void* operator new[](size_t bytes) { return ::operator new(bytes); }
void operator delete(void* p) noexcept { free(p); }
void operator delete[](void* p) noexcept { free(p); }
void operator delete(void* p,size_t) noexcept { free(p); }
void operator delete[](void* p,size_t) noexcept { free(p); }
void* operator new(size_t bytes,std::align_val_t align) {
    ++allocations; if(void* p=_aligned_malloc(bytes?bytes:1,size_t(align))) return p; throw std::bad_alloc();
}
void* operator new[](size_t bytes,std::align_val_t align) { return ::operator new(bytes,align); }
void operator delete(void* p,std::align_val_t) noexcept { _aligned_free(p); }
void operator delete[](void* p,std::align_val_t) noexcept { _aligned_free(p); }
void operator delete(void* p,size_t,std::align_val_t) noexcept { _aligned_free(p); }
void operator delete[](void* p,size_t,std::align_val_t) noexcept { _aligned_free(p); }

static void Check(bool ok,const char* label) { if(!ok) { fprintf(stderr,"FAIL %s\n",label); exit(1); } }
static int64_t Counter() { LARGE_INTEGER q{}; Check(QueryPerformanceCounter(&q)!=FALSE,"QPC"); return q.QuadPart; }
static Matrix Identity() { return Matrix{{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}}; }
struct Scene {
    uint8_t* data=nullptr;
    uint32_t count=128,nodes=32;
    uintptr_t base=0,manager=0,head=0;
    Scene() {
        data=static_cast<uint8_t*>(VirtualAlloc(nullptr,0x200000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        Check(data!=nullptr,"isolated scene allocation"); base=uintptr_t(data); manager=base+0x1000; head=base+0x4000;
        Word(base+0x8000,nodes); Word(base+0x9024,nodes);
        Word(base+0xA000,uint32_t(base+0xB000)); Word(base+0xB044,0);
        Word(head,uint32_t(Owner(0)));
        for(uint32_t i=0;i<count;++i) {
            const uintptr_t owner=Owner(i),bone=base+0x100000+i*0x900;
            Word(owner+0x198,i+1==count?0:uint32_t(Owner(i+1)));
            Word(owner+0x1A0,0x200+i); Word(owner+0x1A8,uint32_t(base+0x7000));
            Word(owner+0x268,uint32_t(base+0x8000)); Word(owner+0x26C,uint32_t(base+0x9000));
            Word(owner+0x128,uint32_t(base+0xA000)); Word(owner+0x13C,uint32_t(bone)); Word(owner+0x140,nodes);
            Word(bone-16,1); Word(bone-12,uint32_t(bone+nodes*64)); Word(bone-8,uint32_t(base));
            Word(manager+i*8,0x200+i); Word(manager+i*8+4,uint32_t(owner));
            Put(owner+0x10,Identity());
            for(uint32_t j=0;j<nodes;++j) Put(bone+j*64,Identity());
        }
    }
    ~Scene() { VirtualFree(data,0,MEM_RELEASE); }
    uintptr_t Owner(uint32_t i) const { return base+0x10000+i*0x300; }
    template<class T> void Put(uintptr_t p,const T& v) { memcpy(reinterpret_cast<void*>(p),&v,sizeof(v)); }
    void Word(uintptr_t p,uint32_t v) { Put(p,v); }
};

template<class Read>
static void SampleScene(Scene& scene,Cache& cache,OwnerList& owners,uint32_t tick,Read read)
{
    Check(cache.BeginTick(tick),"distinct completed tick");
    Check(owners.Collect(scene.head,read,[]{return true;}),"production complete list");
    Check(owners.Size()==scene.count,"complete scene membership");
    for(size_t i=0;i<owners.Size();++i) {
        Key key; Check(ReadKey(owners[i],scene.manager,key,read),"production key");
        Check(cache.Sample(key,scene.manager,read),"production sample");
    }
    cache.EndTick();
}

template<class Read>
static void RenderScene(Scene& scene,Cache& cache,uint32_t tick,Read read)
{
    for(uint32_t i=0;i<scene.count;++i) {
        Matrix root; Check(read(scene.Owner(i)+0x10,&root,sizeof(root)),"submitted root");
        Pose* pose=cache.Render(scene.Owner(i),scene.base+0xA000,root,scene.manager,tick,.5f,read);
        Check(pose!=nullptr,"production interpolated submission");
        uint32_t meshes[2]{}; Check(read(scene.base+0xA000,meshes,sizeof(meshes)),"native mesh passes");
        Check(MeshFits(meshes[0],scene.nodes,read),"root preflight");
        Check(MeshFits(meshes[0],scene.nodes,read),"node submission recheck");
    }
}

int main()
{
    // Real page revocation between query and copy must fail safely, including
    // on a cached hit; the subsequent phase queries permissions anew.
    auto* page=static_cast<uint8_t*>(VirtualAlloc(nullptr,0x2000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Check(page!=nullptr,"page safety fixture"); ReadScope safety; uint32_t value=0;
    Check(ReadProtected(safety,uintptr_t(page),&value,4) && safety.Queries()==1,"production first query");
    DWORD previous=0;
    Check(VirtualProtect(page,0x1000,PAGE_NOACCESS,&previous)!=FALSE,"revoke fixture page");
    Check(!ReadProtected(safety,uintptr_t(page),&value,4) && safety.Queries()==1,"cached hit remains SEH-protected");
    safety.Reset(); Check(!ReadProtected(safety,uintptr_t(page),&value,4) && safety.Copies()==0,"phase reset rechecks revoked page");
    Check(VirtualProtect(page,0x1000,PAGE_READWRITE,&previous)!=FALSE,"restore fixture page");
    safety.Reset(); Check(ReadProtected(safety,uintptr_t(page),&value,4),"restored page readable");
    Check(VirtualProtect(page+0x1000,0x1000,PAGE_GUARD|PAGE_READWRITE,&previous)!=FALSE,"guard fixture page");
    safety.Reset(); Check(!ReadProtected(safety,uintptr_t(page+0x1000),&value,4) && safety.Copies()==0,"guard page refused without copy");
    uint64_t wide=0;
    Check(!ReadProtected(safety,uintptr_t(page+0xFFC),&wide,8),"cross-region production read refused");
    VirtualFree(page,0,MEM_RELEASE);

    Scene scene; Cache cache; OwnerList owners; ReadScope scope;
    size_t safeQueries=0;
    const auto safe=[&](uintptr_t p,void* out,size_t n) { ++safeQueries; return eth::SafeReadBytes(p,out,n); };
    const auto scoped=[&](uintptr_t p,void* out,size_t n) { return ReadProtected(scope,p,out,n); };
    for(uint32_t tick=1;tick<=4;++tick) { scope.Reset(); SampleScene(scene,cache,owners,tick,scoped); }
    scope.Reset(); RenderScene(scene,cache,4,scoped);
    constexpr uint32_t iterations=200;
    LARGE_INTEGER frequency{}; Check(QueryPerformanceFrequency(&frequency)!=FALSE,"QPC frequency");
    int64_t begin=Counter();
    for(uint32_t i=0;i<iterations;++i) SampleScene(scene,cache,owners,5+i,safe);
    const double safeSample=1000.0*double(Counter()-begin)/frequency.QuadPart/iterations;
    const size_t safeSampleQueries=safeQueries; safeQueries=0;
    begin=Counter();
    for(uint32_t i=0;i<iterations;++i) RenderScene(scene,cache,4+iterations,safe);
    const double safeRender=1000.0*double(Counter()-begin)/frequency.QuadPart/iterations;
    const size_t safeRenderQueries=safeQueries;
    size_t sampleQueries=0,renderQueries=0;
    const size_t allocationStart=allocations;
    begin=Counter();
    for(uint32_t i=0;i<iterations;++i) {
        scope.Reset(); SampleScene(scene,cache,owners,5+iterations+i,scoped); sampleQueries+=scope.Queries();
    }
    const double scopedSample=1000.0*double(Counter()-begin)/frequency.QuadPart/iterations;
    begin=Counter();
    for(uint32_t i=0;i<iterations;++i) {
        scope.Reset(); RenderScene(scene,cache,4+iterations*2,scoped); renderQueries+=scope.Queries();
    }
    const double scopedRender=1000.0*double(Counter()-begin)/frequency.QuadPart/iterations;
    const size_t stableAllocations=allocations-allocationStart;
    Check(stableAllocations==0,"zero warmed-up sample/render allocations");
    // VirtualQuery describes the queried page and following matching pages,
    // not necessarily the preceding pages. Descending addresses can therefore
    // need a few suffix-range queries even in one committed allocation.
    Check(sampleQueries<=iterations*3 && renderQueries<=iterations*3,"at most three production suffix-range queries per fixture phase");
    Check(safeSampleQueries>sampleQueries*100 && safeRenderQueries>renderQueries*100,"protected query reduction exceeds 100x");
    printf("PASS production entity reader safety/performance: %u entities x %u nodes, %u ticks/frames, %zu warmed-up allocations\n",scene.count,scene.nodes,iterations,stableAllocations);
    printf("sample_ms safe=%.4f scoped=%.4f; render_sdk_ms safe=%.4f scoped=%.4f\n",safeSample,scopedSample,safeRender,scopedRender);
    printf("page_queries_per_sample safe=%zu scoped=%zu; per_frame safe=%zu scoped=%zu\n",safeSampleQueries/iterations,sampleQueries/iterations,safeRenderQueries/iterations,renderQueries/iterations);
    // Timings are evidence, not flaky pass thresholds or an in-engine FPS claim.
}
