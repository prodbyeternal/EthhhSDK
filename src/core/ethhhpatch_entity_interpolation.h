#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace eth::patch::entity {
constexpr uint32_t kMaxNodes = 512;
constexpr size_t kMaxEntities = 512, kMaxTotalNodes = 16384;
struct ReadableRegion { uintptr_t begin=0, end=0; };
class ReadScope {
public:
    void Reset() { used_=0; queries_=copies_=bytes_=0; replace_=0; }
    template<class Query, class Copy>
    bool Read(uintptr_t address, void* output, size_t bytes, Query query, Copy copy) {
        if (!address || !output || !bytes || address>UINT32_MAX || bytes>UINT32_MAX-address) return false;
        for(size_t i=0;i<used_;++i) {
            const auto& r=regions_[i];
            if(address>=r.begin && address<r.end && bytes<=r.end-address)
                return CopyScoped(address,output,bytes,copy); // every copy still has SEH; no cached native data
        }
        ReadableRegion region; ++queries_;
        if (!query(address,region) || address<region.begin || address>=region.end || bytes>region.end-address) return false;
        if(used_<regions_.size()) regions_[used_++]=region;
        else { regions_[replace_]=region; replace_=(replace_+1)%regions_.size(); }
        return CopyScoped(address,output,bytes,copy);
    }
    size_t Queries() const { return queries_; }
    size_t Copies() const { return copies_; }
    size_t Bytes() const { return bytes_; }
private:
    template<class CopyFn>
    bool CopyScoped(uintptr_t address, void* output, size_t bytes, CopyFn copy) {
        ++copies_; bytes_+=bytes;
        return copy(address,output,bytes);
    }
    std::array<ReadableRegion,64> regions_{};
    size_t used_=0, queries_=0, copies_=0, bytes_=0, replace_=0;
};

// Optional interpolation work, not a simulation clock. Render callers charge
// ONLY SDK validation/blending, excluding the original renderer and Present.
class WorkBudget {
public:
    void Reset(int64_t limit) { limit_=limit>0?limit:0; spent_=0; }
    bool Available() const { return limit_>0 && spent_<limit_; }
    void Charge(int64_t begin, int64_t end) {
        if(begin<=0 || end<begin) { spent_=limit_; return; }
        const int64_t elapsed=end-begin;
        spent_=elapsed>=limit_-spent_?limit_:spent_+elapsed;
    }
private:
    int64_t limit_=0, spent_=0;
};

// Entity sampling needs list membership, not inspector strings/angles. Reuse a
// fixed hash set and owner array: linear complete-list walk, no per-tick heap
// churn or class-name queries. Identity/allocation checks still happen per pose.
class OwnerList {
public:
    template<class Read, class Continue>
    bool Collect(uintptr_t headAddress, Read read, Continue withinBudget) {
        count_=0; seen_.fill(0);
        uint32_t head=0,current=0,check=0;
        if(!read(headAddress,&head,4)) return false;
        current=head;
        while(current) {
            if(count_==owners_.size() || current>UINT32_MAX-0x19C || !withinBudget()) return false;
            size_t slot=((current>>4)*2654435761u)&(seen_.size()-1);
            while(seen_[slot] && seen_[slot]!=current) slot=(slot+1)&(seen_.size()-1);
            if(seen_[slot]) return false;
            seen_[slot]=current; owners_[count_++]=current;
            if(!read(uintptr_t(current)+0x198,&current,4)) return false;
        }
        return read(headAddress,&check,4) && check==head;
    }
    size_t Size() const { return count_; }
    uint32_t operator[](size_t i) const { return owners_[i]; }
private:
    std::array<uint32_t,8192> owners_{};
    std::array<uint32_t,16384> seen_{};
    size_t count_=0;
};
struct alignas(16) Matrix { float m[16]{}; };
static_assert(sizeof(Matrix) == 64, "native node stride");
struct Affine {
    float q[4]{}, scale[3]{}, position[3]{};
};

// DirectX row-vector convention. Reflections live on scale X, never in the
// quaternion. Sheared/degenerate/projective matrices use the native fallback.
inline bool Decompose(const Matrix& in, Affine& out)
{
    for (float f : in.m) if (!std::isfinite(f) || std::abs(f) > 1e6f) return false;
    if (std::abs(in.m[3]) > 1e-4f || std::abs(in.m[7]) > 1e-4f ||
        std::abs(in.m[11]) > 1e-4f || std::abs(in.m[15] - 1) > 1e-4f) return false;
    float r[3][3]{};
    for (int i = 0; i < 3; ++i) {
        float length = 0;
        for (int j = 0; j < 3; ++j) length += in.m[i * 4 + j] * in.m[i * 4 + j];
        length = std::sqrt(length);
        if (length < 1e-4f || length > 1e4f) return false;
        out.scale[i] = length; out.position[i] = in.m[12 + i];
        for (int j = 0; j < 3; ++j) r[i][j] = in.m[i * 4 + j] / length;
    }
    for (int i = 0; i < 3; ++i) for (int j = i + 1; j < 3; ++j) {
        float dot = 0;
        for (int k = 0; k < 3; ++k) dot += r[i][k] * r[j][k];
        if (std::abs(dot) > 0.002f) return false;
    }
    const float det = r[0][0] * (r[1][1]*r[2][2] - r[1][2]*r[2][1]) -
        r[0][1] * (r[1][0]*r[2][2] - r[1][2]*r[2][0]) +
        r[0][2] * (r[1][0]*r[2][1] - r[1][1]*r[2][0]);
    if (std::abs(std::abs(det) - 1) > 0.004f) return false;
    if (det < 0) { out.scale[0] = -out.scale[0]; for (float& f : r[0]) f = -f; }
    // Transpose the row-vector basis for the conventional quaternion extraction.
    const float trace = r[0][0] + r[1][1] + r[2][2];
    if (trace > 0) {
        const float s = std::sqrt(trace + 1) * 2;
        out.q[3] = s * 0.25f;
        out.q[0] = (r[1][2] - r[2][1]) / s;
        out.q[1] = (r[2][0] - r[0][2]) / s;
        out.q[2] = (r[0][1] - r[1][0]) / s;
    } else {
        int i = 0;
        if (r[1][1] > r[0][0]) i = 1;
        if (r[2][2] > r[i][i]) i = 2;
        const int j = (i + 1) % 3, k = (i + 2) % 3;
        const float s = std::sqrt(1 + r[i][i] - r[j][j] - r[k][k]) * 2;
        out.q[i] = s * 0.25f;
        out.q[j] = (r[j][i] + r[i][j]) / s;
        out.q[k] = (r[k][i] + r[i][k]) / s;
        out.q[3] = (r[j][k] - r[k][j]) / s;
    }
    float norm = 0;
    for (float f : out.q) norm += f*f;
    if (!std::isfinite(norm) || norm < 0.5f) return false;
    norm = 1 / std::sqrt(norm);
    for (float& f : out.q) f *= norm;
    return true;
}

inline bool Continuous(const Affine& a, const Affine& b, float maxDistance)
{
    float distance = 0;
    for (int i = 0; i < 3; ++i) {
        const float d = b.position[i] - a.position[i]; distance += d*d;
        if ((a.scale[i] < 0) != (b.scale[i] < 0)) return false;
        const float ratio = std::abs(b.scale[i] / a.scale[i]);
        if (ratio < 0.25f || ratio > 4) return false;
    }
    return distance <= maxDistance * maxDistance;
}

inline Matrix Blend(const Affine& a, const Affine& b, float alpha)
{
    float dot = 0, q[4]{};
    for (int i = 0; i < 4; ++i) dot += a.q[i] * b.q[i];
    const float sign = dot < 0 ? -1.0f : 1.0f;
    dot = std::clamp(std::abs(dot), 0.0f, 1.0f);
    float wa = 1 - alpha, wb = alpha;
    if (dot < 0.9995f) {
        const float angle = std::acos(dot), denominator = std::sin(angle);
        wa = std::sin((1-alpha)*angle) / denominator;
        wb = std::sin(alpha*angle) / denominator;
    }
    float norm = 0;
    for (int i = 0; i < 4; ++i) { q[i] = wa*a.q[i] + wb*sign*b.q[i]; norm += q[i]*q[i]; }
    norm = 1 / std::sqrt(norm);
    for (float& f : q) f *= norm;
    const float x=q[0], y=q[1], z=q[2], w=q[3];
    Matrix out{{1-2*(y*y+z*z), 2*(x*y+z*w), 2*(x*z-y*w), 0,
        2*(x*y-z*w), 1-2*(x*x+z*z), 2*(y*z+x*w), 0,
        2*(x*z+y*w), 2*(y*z-x*w), 1-2*(x*x+y*y), 0, 0,0,0,1}};
    for (int i = 0; i < 3; ++i) {
        const float scale = a.scale[i] + (b.scale[i]-a.scale[i])*alpha;
        for (int j = 0; j < 3; ++j) out.m[i*4+j] *= scale;
        out.m[12+i] = a.position[i] + (b.position[i]-a.position[i])*alpha;
    }
    return out;
}

struct Key {
    uint32_t owner=0, pClass=0, handle=0, model=0, animation=0, nodes=0,
        instance=0, alternateInstance=0, instanceMode=0, count=0, allocationEnd=0, heap=0;
    bool operator==(const Key& b) const {
        return owner==b.owner && pClass==b.pClass && handle==b.handle && model==b.model &&
            animation==b.animation && nodes==b.nodes && instance==b.instance &&
            alternateInstance==b.alternateInstance && instanceMode==b.instanceMode && count==b.count &&
            allocationEnd==b.allocationEnd && heap==b.heap;
    }
    bool SamePoseStorage(const Key& b) const {
        Key ordered=b;
        // 0x43E7D0..0x43E7EC swaps only the two existing model instances when
        // owner+0x144 is nonzero. Do not accept any new/replaced third instance.
        if (instanceMode && instanceMode==b.instanceMode && alternateInstance &&
            alternateInstance!=instance && instance==b.alternateInstance && alternateInstance==b.instance) {
            ordered.instance=instance; ordered.alternateInstance=alternateInstance;
        }
        return *this==ordered;
    }
};

template<class Read>
bool ReadKey(uintptr_t owner, uintptr_t manager, Key& out, Read read)
{
    if (!owner || owner > UINT32_MAX - 0x270 || !manager || manager > UINT32_MAX - 0x1000) return false;
    Key k; k.owner = uint32_t(owner);
    const auto word = [&](uintptr_t address, uint32_t& value) { return read(address, &value, 4); };
    uint32_t modelCount=0, animationCount=0, slot[2]{}, identity[3]{}, model[2]{}, instance[8]{}, header[4]{};
    if (!read(owner+0x128,instance,sizeof(instance)) || !read(owner+0x1A0,identity,sizeof(identity)) ||
        !read(owner+0x268,model,sizeof(model))) return false;
    k.instance=instance[0]; k.alternateInstance=instance[1]; k.nodes=instance[5]; k.count=instance[6]; k.instanceMode=instance[7];
    k.handle=identity[0]; k.pClass=identity[2]; k.model=model[0]; k.animation=model[1];
    if (!k.pClass || !k.handle || !k.model || k.model>UINT32_MAX-4 ||
        !k.animation || k.animation>UINT32_MAX-0x28 || !k.instance ||
        k.nodes<0x10000 || (k.nodes&15) || !k.count || k.count>kMaxNodes ||
        !word(k.model,modelCount) || modelCount != k.count ||
        !word(k.animation+0x24,animationCount) || animationCount != k.count ||
        k.nodes > UINT32_MAX - k.count * sizeof(Matrix) ||
        !read(k.nodes-0x10,header,sizeof(header)) || !(header[0]&1) ||
        header[1] < k.nodes + k.count*sizeof(Matrix) || !header[2] ||
        !read(manager+(k.handle&0x1FF)*8,slot,sizeof(slot)) || slot[0]!=k.handle || slot[1]!=k.owner) return false;
    k.allocationEnd=header[1]; k.heap=header[2];
    out = k;
    return true;
}

// Mesh+0x44 is an iterative next link, NOT recursion. Native sign-extends the
// WORD node index, so 0x8000..0xFFFE is unsafe for an SDK-owned bone array.
template<class Read>
bool MeshFits(uintptr_t mesh, uint32_t count, Read read)
{
    size_t length = 0;
    while (mesh) {
        // Cycles also hit this fixed link cap. No quadratic seen-vector scan.
        if (mesh > UINT32_MAX-0x48 || length++ == 512) return false;
        uint32_t fields[3]{};
        if (!read(mesh+0x3C,fields,sizeof(fields))) return false;
        const uint16_t index=uint16_t(fields[0]);
        if (index!=0xFFFF && (index>=count || index>=0x8000)) return false;
        mesh = fields[2];
    }
    return length != 0;
}

struct Pose {
    Key key;
    uint32_t tick=0;
    bool paired=false;
    bool blendValid=false;
    float blendAlpha=1;
    Matrix nativeWorld, renderWorld;
    Affine world, previousWorld;
    std::vector<Matrix> nativeNodes, renderNodes;
    std::vector<Affine> nodes, previousNodes;
};

class Cache {
public:
    void Reset() { poses_.clear(); sampled_=false; total_=0; }
    bool BeginTick(uint32_t tick) {
        if (sampled_ && tick==tick_) return false; // Never rotate endpoints per render/duplicate notification.
        tick_=tick; sampled_=true; total_=0; return true;
    }
    template<class Read>
    bool Sample(const Key& key, uintptr_t manager, Read read) {
        if (!sampled_ || !key.count || key.count>kMaxNodes) return false;
        if (poses_.size()>=kMaxEntities && !poses_.count(key.owner)) return false;
        if (total_+key.count>kMaxTotalNodes) return false;
        Key check;
        Matrix world; Affine affine;
        if (!read(key.owner+0x10,&world,sizeof(Matrix)) ||
            !read(key.nodes,current_.data(),key.count*sizeof(Matrix)) ||
            !ReadKey(key.owner,manager,check,read) || !(check==key) ||
            !Decompose(world,affine)) { poses_.erase(key.owner); return false; }
        for(size_t i=0;i<key.count;++i) if(!Decompose(current_[i],affineScratch_[i])) {
            poses_.erase(key.owner); return false;
        }
        // Unsupported poses never allocate a new cache entry every tick.
        Pose& p=poses_[key.owner];
        p.nativeNodes.resize(key.count);
        memcpy(p.nativeNodes.data(),current_.data(),key.count*sizeof(Matrix));
        p.paired=p.key.SamePoseStorage(key) && uint32_t(tick_-p.tick)==1 && p.nodes.size()==key.count && Continuous(p.world,affine,6);
        p.previousWorld=p.world; p.world=affine; p.nativeWorld=world;
        p.previousNodes.swap(p.nodes); p.nodes.resize(key.count);
        for(size_t i=0;i<key.count;++i) {
            p.nodes[i]=affineScratch_[i];
            if(p.paired && !Continuous(p.previousNodes[i],p.nodes[i],8)) p.paired=false;
        }
        p.key=key; p.tick=tick_; p.renderNodes.resize(key.count); p.blendValid=false;
        total_+=key.count; return true;
    }
    void EndTick() {
        for (auto it=poses_.begin();it!=poses_.end();) {
            if (it->second.tick!=tick_) it=poses_.erase(it); else ++it;
        }
    }
    template<class Read>
    Pose* Render(uintptr_t owner, uintptr_t instance, const Matrix& nativeWorld,
        uintptr_t manager, uint32_t tick, float alpha, Read read) {
        auto found=poses_.find(uint32_t(owner));
        if (found==poses_.end() || !std::isfinite(alpha) || alpha<0 || alpha>=1) return nullptr;
        Pose& p=found->second; Key check;
        if (!p.paired || p.tick!=tick ||
            memcmp(&nativeWorld,&p.nativeWorld,sizeof(Matrix)) ||
            !ReadKey(owner,manager,check,read) || instance!=check.instance || !p.key.SamePoseStorage(check)) return nullptr;
        // Never substitute over a render callback/mod's intervening pose write.
        Matrix liveWorld; Key finalKey;
        // Reuse bounded scratch storage rather than zeroing a 32-KiB stack array
        // for EVERY rendered entity, even one-bone objects.
        if (!read(p.key.nodes,current_.data(),p.key.count*sizeof(Matrix)) ||
            memcmp(current_.data(),p.nativeNodes.data(),p.key.count*sizeof(Matrix)) ||
            !read(owner+0x10,&liveWorld,sizeof(Matrix)) || memcmp(&liveWorld,&p.nativeWorld,sizeof(Matrix)) ||
            !ReadKey(owner,manager,finalKey,read) || !(finalKey==check)) return nullptr;
        if(!p.blendValid || p.blendAlpha!=alpha) {
            p.renderWorld=Blend(p.previousWorld,p.world,alpha);
            for (size_t i=0;i<p.key.count;++i) p.renderNodes[i]=Blend(p.previousNodes[i],p.nodes[i],alpha);
            p.blendAlpha=alpha; p.blendValid=true;
        }
        return &p;
    }
    size_t Size() const { return poses_.size(); }
    size_t Paired() const { size_t n=0; for (const auto& p:poses_) n+=p.second.paired; return n; }
private:
    std::unordered_map<uint32_t,Pose> poses_;
    std::array<Matrix,kMaxNodes> current_{};
    std::array<Affine,kMaxNodes> affineScratch_{};
    uint32_t tick_=0;
    bool sampled_=false;
    size_t total_=0;
};
} // namespace eth::patch::entity
