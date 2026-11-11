// EthhhSDK DX9 port of liquidDX11 (MIT). All optical expressions remain in
// shaders.h; this adapter changes resource syntax, constant packing and quads.
namespace {
template<class T> void ReleaseGlass(T*& p) { if (p) { p->Release(); p=nullptr; } }

// D3DSBT_ALL does not capture render targets/depth surfaces. Keep those and the
// viewport explicitly, including additional MRTs, for every blur transaction.
struct GlassState {
    IDirect3DDevice9* device;
    IDirect3DStateBlock9* block=nullptr;
    IDirect3DSurface9* targets[4]{};
    IDirect3DSurface9* depth=nullptr;
    D3DVIEWPORT9 viewport{};
    DWORD count=1;
    bool valid=false;
    explicit GlassState(IDirect3DDevice9* d):device(d) {
        D3DCAPS9 caps{};
        if (FAILED(d->GetDeviceCaps(&caps))) return;
        count=std::min<DWORD>(4,caps.NumSimultaneousRTs);
        if (FAILED(d->CreateStateBlock(D3DSBT_ALL,&block)) || FAILED(block->Capture()) ||
            FAILED(d->GetRenderTarget(0,&targets[0])) || FAILED(d->GetViewport(&viewport))) return;
        for (DWORD i=1;i<count;++i) d->GetRenderTarget(i,&targets[i]);
        d->GetDepthStencilSurface(&depth);
        valid=true;
    }
    ~GlassState() {
        if (valid) {
            device->SetDepthStencilSurface(nullptr);
            for (DWORD i=1;i<count;++i) device->SetRenderTarget(i,nullptr);
            device->SetRenderTarget(0,targets[0]);
            for (DWORD i=1;i<count;++i) device->SetRenderTarget(i,targets[i]);
            device->SetDepthStencilSurface(depth);
            block->Apply();
            device->SetViewport(&viewport);
        }
        ReleaseGlass(block); ReleaseGlass(depth);
        for (auto& p:targets) ReleaseGlass(p);
    }
};

void ReplaceGlass(std::string& s,const char* from,const char* to) {
    size_t at=0;
    while ((at=s.find(from,at))!=std::string::npos) { s.replace(at,strlen(from),to); at+=strlen(to); }
}

const char* kGlassRegisters=R"HLSL(
float4 glass_constants[26] : register(c0);
#define window_size glass_constants[0].xy
#define desktop_size glass_constants[0].zw
#define window_origin glass_constants[1].xy
#define light_dir glass_constants[1].zw
#define widget_center glass_constants[2].xy
#define widget_half_size glass_constants[2].zw
#define corner_radius glass_constants[3].x
#define fade glass_constants[3].y
#define blur_mix glass_constants[3].z
#define saturation glass_constants[3].w
#define tint_color glass_constants[4]
#define tint_opacity glass_constants[5].x
#define brightness glass_constants[5].y
#define contrast glass_constants[5].z
#define grain glass_constants[5].w
#define refr_strength glass_constants[6].x
#define refr_band glass_constants[6].y
#define highlight glass_constants[6].z
#define inner_shadow glass_constants[6].w
#define border_intensity glass_constants[7].x
#define border_width glass_constants[7].y
#define shadow_strength glass_constants[7].z
#define shadow_radius glass_constants[7].w
#define shadow_offset glass_constants[8].xy
#define time glass_constants[8].z
#define quad_pad glass_constants[8].w
#define tilt glass_constants[9].xy
#define sheen glass_constants[9].z
#define chroma glass_constants[9].w
#define cursor glass_constants[10].xy
#define _pad_cur glass_constants[10].zw
#define blob1_center glass_constants[11].xy
#define blob1_half glass_constants[11].zw
#define blob2_center glass_constants[12].xy
#define blob2_half glass_constants[12].zw
#define blob_k glass_constants[13].x
#define target_transparency glass_constants[13].y
#define edge_p0 glass_constants[14]
#define edge_p1 glass_constants[15]
#define edge_p2 glass_constants[16]
#define edge_p3 glass_constants[17]
#define edge_p4 glass_constants[18]
#define edge_p5 glass_constants[19]
#define edge_p6 glass_constants[20]
#define edge_p7 glass_constants[21]
#define edge_p8 glass_constants[22]
#define edge_p9 glass_constants[23]
#define edge_p10 glass_constants[24]
#define edge_p11 glass_constants[25]
sampler2D BlurHeavy : register(s0);
sampler2D BlurSoft : register(s1);
)HLSL";

std::string GlassShaderDX9() {
    std::string source=kGlassHLSL;
    const size_t body=source.find("struct VSOut");
    source=source.substr(body);
    const size_t vertex=source.find("VSOut VS_Glass(");
    const size_t sdf=source.find("float sdRoundedRect");
    source.erase(vertex,sdf-vertex);
    ReplaceGlass(source,"SV_Position","POSITION"); ReplaceGlass(source,"SV_Target","COLOR0");
    ReplaceGlass(source,"BlurHeavy.SampleLevel(LinearClamp, uv, 0)","tex2Dlod(BlurHeavy, float4(uv,0,0))");
    ReplaceGlass(source,"BlurSoft .SampleLevel(LinearClamp, uv, 0)","tex2Dlod(BlurSoft, float4(uv,0,0))");
    ReplaceGlass(source,"pow(edge_p9.y * 2.71828183,","pow(max(edge_p9.y * 2.71828183, 0.000001),");
    // DWM-composited launcher: genuine alpha, not a wallpaper look-alike.
    // In-game stays the template's exact captured-scene optical expression.
    ReplaceGlass(source,"float a   = shape * fade;","float a   = shape * fade * lerp(1.0, max(0.16, opEff), target_transparency);");
    return std::string(kGlassRegisters)+source;
}

std::string BlurShaderDX9() {
    std::string source=kBlurHLSL;
    source=source.substr(source.find("float4 PSDown"));
    ReplaceGlass(source,"SV_Target","COLOR0");
    ReplaceGlass(source,"Src.Sample(LinearClamp, ","tex2D(Src, ");
    return std::string("float4 blur_params : register(c0);\n#define texel blur_params.xy\n#define spread blur_params.z\nsampler2D Src : register(s0);\nstruct VSO { float2 uv : TEXCOORD0; };\n")+source;
}

bool CompileGlassPixel(IDirect3DDevice9* device,const std::string& source,const char* entry,IDirect3DPixelShader9** shader) {
    ID3DBlob *code=nullptr,*errors=nullptr;
    const HRESULT hr=D3DCompile(source.data(),source.size(),"EthhhSDK liquid glass DX9",nullptr,nullptr,
        entry,"ps_3_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
    if (errors) { OutputDebugStringA((const char*)errors->GetBufferPointer());
        fprintf(stderr,"Liquid Glass %s: %s\n",entry,(const char*)errors->GetBufferPointer()); errors->Release(); }
    if (FAILED(hr)) { ReleaseGlass(code); return false; }
    const HRESULT created=device->CreatePixelShader((const DWORD*)code->GetBufferPointer(),shader);
    code->Release(); return SUCCEEDED(created);
}

void GlassDrawCallback(const ImDrawList*,const ImDrawCmd* cmd) {
    if (g) g->Draw(size_t(uintptr_t(cmd->UserCallbackData)-1),cmd);
}

void GlassPipeline(IDirect3DDevice9* d,bool blend) {
    d->SetVertexShader(nullptr);
    d->SetRenderState(D3DRS_ZENABLE,FALSE); d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
    d->SetRenderState(D3DRS_STENCILENABLE,FALSE); d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
    d->SetRenderState(D3DRS_FOGENABLE,FALSE); d->SetRenderState(D3DRS_LIGHTING,FALSE);
    d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE); d->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
    d->SetRenderState(D3DRS_CLIPPING,TRUE); d->SetRenderState(D3DRS_CLIPPLANEENABLE,0);
    d->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);
    d->SetRenderState(D3DRS_COLORWRITEENABLE,15);
    d->SetRenderState(D3DRS_ALPHABLENDENABLE,blend);
    d->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
    d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE);
    d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    d->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);
    for (DWORD i=0;i<2;++i) {
        d->SetSamplerState(i,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
        d->SetSamplerState(i,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
        d->SetSamplerState(i,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
        d->SetSamplerState(i,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
        d->SetSamplerState(i,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
        d->SetSamplerState(i,D3DSAMP_SRGBTEXTURE,FALSE);
    }
}
}

bool Renderer::Init(IDirect3DDevice9* device) { Shutdown(); transparent_target_=false; device_=device; return CreateDeviceObjects(); }
bool Renderer::CompileShaders() {
    D3DCAPS9 caps{};
    if (!device_ || FAILED(device_->GetDeviceCaps(&caps)) || caps.PixelShaderVersion<D3DPS_VERSION(3,0)) return false;
    return CompileGlassPixel(device_,GlassShaderDX9(),"PS_Glass",&ps_) &&
        CompileGlassPixel(device_,BlurShaderDX9(),"PSDown",&down_) &&
        CompileGlassPixel(device_,BlurShaderDX9(),"PSUp",&up_);
}
bool Renderer::CreateDeviceObjects() {
    if (ps_ && down_ && up_) return true;
    InvalidateDeviceObjects();
    if (!CompileShaders()) { InvalidateDeviceObjects(); return false; }
    return true;
}
void Renderer::InvalidateDeviceObjects() {
    g_uiBlurTex=0; backdrop_ready_=false; backdrop_age_=1;
    ReleaseGlass(ps_); ReleaseGlass(down_); ReleaseGlass(up_); ReleaseGlass(scene_);
    for (auto& p:blur_) ReleaseGlass(p);
    ReleaseGlass(heavy_);ReleaseGlass(soft_); texture_w_=texture_h_=0;
}
void Renderer::Shutdown() { InvalidateDeviceObjects(); device_=nullptr; queue_.clear(); overlay_.clear(); springs_.map.clear(); entrance_=time_=0; }

void Renderer::BeginFrame(int w,int h,int x,int y,int dw,int dh,ImVec2 cursorPosition) {
    win_w_=w; win_h_=h; win_x_=x; win_y_=y; desk_w_=dw; desk_h_=dh;
    cursor_x_=cursorPosition.x; cursor_y_=cursorPosition.y;
    const float dt=std::clamp(ImGui::GetIO().DeltaTime,0.0f,0.05f);
    time_+=dt; entrance_=std::min(1.0f,entrance_+dt*2.2f);
    const float a=light_angle_deg_*kPI/180, b=light2_angle_deg_*kPI/180;
    light_x_=std::sin(a); light_y_=-std::cos(a); light2_x_=std::sin(b); light2_y_=-std::cos(b);
    queue_.clear(); overlay_.clear(); submit_fade_=1; backdrop_age_+=dt; ClearClipRect();
    g_uiBlurTex=ImTextureID(uintptr_t(heavy_));
    if(!heavy_) {g_uiBlurOrigin=ImVec2(0,0);g_uiBlurSize=ImVec2(float(w),float(h));}
}
size_t Renderer::Submit(const Primitive& primitive) {
    const size_t index=queue_.size(); queue_.push_back(primitive);
    Primitive& p=queue_.back(); p.fade*=submit_fade_*ImGui::GetStyle().Alpha;
    ImDrawList* dl=ImGui::GetCurrentWindowRead() ? ImGui::GetWindowDrawList() : ImGui::GetForegroundDrawList();
    const ImVec2 lo=dl->GetClipRectMin(),hi=dl->GetClipRectMax();
    p.clip[0]=std::max(clip_[0],lo.x);p.clip[1]=std::max(clip_[1],lo.y);
    p.clip[2]=std::min(clip_[2],hi.x);p.clip[3]=std::min(clip_[3],hi.y);
    dl->AddCallback(GlassDrawCallback,(void*)(uintptr_t(index)+1));
    dl->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
    return index;
}
Primitive& Renderer::At(size_t i) { return queue_.at(i); }
void Renderer::SubmitOverlay(const Primitive& p) {
    const size_t index=queue_.size(); queue_.push_back(p);
    ImDrawList* dl=ImGui::GetForegroundDrawList();
    dl->AddCallback(GlassDrawCallback,(void*)(uintptr_t(index)+1));
    dl->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
}

bool Renderer::PrepareBackdrop() {
    if (!device_ || !Ready() || win_w_<=0 || win_h_<=0 || queue_.empty()) return false;
    IDirect3DSurface9* primary=nullptr;
    if(FAILED(device_->GetRenderTarget(0,&primary))) return false;
    D3DSURFACE_DESC desc{};const HRESULT described=primary->GetDesc(&desc);primary->Release();
    if(FAILED(described)) return false;
    target_w_=int(desc.Width);target_h_=int(desc.Height);
    const auto* draw=ImGui::GetDrawData();if(!draw) return false;
    const ImVec2 origin=draw->DisplayPos,scale=draw->FramebufferScale;
    if(scale.x<=0 || scale.y<=0) return false;
    float left=float(target_w_),top=float(target_h_),right=0,bottom=0;
    for(const auto& p:queue_) {
        if(p.fade<=0 || p.hw<=0 || p.hh<=0) continue;
        const auto& m=Params(p.material);const float pad=m.shadow_radius*p.elevate+m.border_width+6;
        const float x0=(std::max(p.cx-p.hw-pad,p.clip[0])-origin.x)*scale.x;
        const float y0=(std::max(p.cy-p.hh-pad,p.clip[1])-origin.y)*scale.y;
        const float x1=(std::min(p.cx+p.hw+pad,p.clip[2])-origin.x)*scale.x;
        const float y1=(std::min(p.cy+p.hh+pad,p.clip[3])-origin.y)*scale.y;
        if(x1<=x0 || y1<=y0) continue;
        left=std::min(left,x0);top=std::min(top,y0);right=std::max(right,x1);bottom=std::max(bottom,y1);
    }
    // Include a generous optical sampling apron, but never blur the whole 4K
    // scene for a 230x55 watermark. Six levels run on this bounded atlas.
    constexpr int apron=48;
    const int x=std::max(0,int(std::floor(left))-apron),y=std::max(0,int(std::floor(top))-apron);
    const int w=std::min(target_w_,int(std::ceil(right))+apron)-x;
    const int h=std::min(target_h_,int(std::ceil(bottom))+apron)-y;
    if(w<=0 || h<=0) {backdrop_ready_=false;return false;}
    // Match the template's independent 10 Hz backdrop budget. Widgets, flow,
    // highlights and text still render at every host frame, not at blur Hz.
    if(backdrop_ready_ && x==capture_x_ && y==capture_y_ && w==texture_w_ && h==texture_h_ && backdrop_age_<1.0f/backdrop_hz_) return true;
    backdrop_ready_=false;
    GlassState saved(device_);if(!saved.valid) return false;
    if (w!=texture_w_ || h!=texture_h_) {
        ReleaseGlass(scene_); for(auto& t:blur_) ReleaseGlass(t); ReleaseGlass(heavy_);ReleaseGlass(soft_);
        texture_w_=texture_h_=0;
        auto create=[&](int tw,int th,IDirect3DTexture9** t) {
            return SUCCEEDED(device_->CreateTexture(tw,th,1,D3DUSAGE_RENDERTARGET,desc.Format,D3DPOOL_DEFAULT,t,nullptr));
        };
        if (!create(w,h,&scene_)) return false;
        for(int i=0;i<6;++i) if (!create(std::max(1,w>>(i+1)),std::max(1,h>>(i+1)),&blur_[i])) return false;
        if(!create(w,h,&heavy_) || !create(w,h,&soft_)) return false;
        texture_w_=w; texture_h_=h;
    }
    IDirect3DSurface9* capture=nullptr;
    if (FAILED(scene_->GetSurfaceLevel(0,&capture))) return false;
    const RECT sourceRect{x,y,x+w,y+h};
    const HRESULT captured=device_->StretchRect(saved.targets[0],&sourceRect,capture,nullptr,D3DTEXF_NONE);
    capture->Release(); if (FAILED(captured)) return false; // safe opaque fallback on unsupported MSAA capture
    device_->SetDepthStencilSurface(nullptr);
    for(DWORD i=1;i<saved.count;++i) device_->SetRenderTarget(i,nullptr);
    GlassPipeline(device_,false); device_->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
    device_->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);
    struct Vertex { float x,y,z,rhw,u,v; };
    auto pass=[&](IDirect3DTexture9* src,IDirect3DTexture9* dst,IDirect3DPixelShader9* shader) {
        D3DSURFACE_DESC s{},t{}; src->GetLevelDesc(0,&s); dst->GetLevelDesc(0,&t);
        IDirect3DSurface9* target=nullptr; if(FAILED(dst->GetSurfaceLevel(0,&target))) return false;
        device_->SetTexture(0,nullptr); device_->SetTexture(1,nullptr);
        const HRESULT bound=device_->SetRenderTarget(0,target); target->Release(); if(FAILED(bound)) return false;
        D3DVIEWPORT9 vp{0,0,t.Width,t.Height,0,1}; device_->SetViewport(&vp);
        device_->SetTexture(0,src); device_->SetPixelShader(shader);
        const float constants[4]={1.0f/s.Width,1.0f/s.Height,1.0f,0}; device_->SetPixelShaderConstantF(0,constants,1);
        const Vertex vertices[4]={{-.5f,-.5f,0,1,0,0},{float(t.Width)-.5f,-.5f,0,1,1,0},
            {-.5f,float(t.Height)-.5f,0,1,0,1},{float(t.Width)-.5f,float(t.Height)-.5f,0,1,1,1}};
        return SUCCEEDED(device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex)));
    };
    IDirect3DTexture9* source=scene_;
    for(int i=0;i<6;++i) { if(!pass(source,blur_[i],down_)) return false; source=blur_[i]; }
    for(int i=4;i>=0;--i) if(!pass(blur_[i+1],blur_[i],up_)) return false;
    if(!pass(blur_[0],heavy_,up_) || !pass(scene_,soft_,up_)) return false;
    capture_x_=x;capture_y_=y;
    g_uiBlurTex=ImTextureID(uintptr_t(heavy_));
    g_uiBlurOrigin=ImVec2(origin.x+x/scale.x,origin.y+y/scale.y);
    g_uiBlurSize=ImVec2(w/scale.x,h/scale.y);
    backdrop_ready_=true; backdrop_age_=0;++capture_count_;return true;
}

void Renderer::Draw(size_t index,const ImDrawCmd* command) {
    if(!device_ || index>=queue_.size()) return;
    const Primitive& p=queue_[index]; if(p.fade<=0 || p.hw<=0 || p.hh<=0) return;
    // DrawPrimitiveUP clears stream 0. ImGui's reset-state callback does NOT
    // rebind its vertex buffer, so restoring it here is mandatory.
    struct StreamRestore {
        IDirect3DDevice9* device; IDirect3DVertexBuffer9* buffer=nullptr; UINT offset=0,stride=0; DWORD fvf=0;
        explicit StreamRestore(IDirect3DDevice9* d):device(d) {d->GetStreamSource(0,&buffer,&offset,&stride);d->GetFVF(&fvf);}
        ~StreamRestore(){device->SetFVF(fvf);device->SetStreamSource(0,buffer,offset,stride);ReleaseGlass(buffer);}
    } stream(device_);
    if(!backdrop_ready_) {
        // Readable rounded fallback, also used on non-SM3 and unsupported MSAA.
        const auto& material=Params(p.material);
        const DWORD color=D3DCOLOR_COLORVALUE(material.tint_rgb[0],material.tint_rgb[1],material.tint_rgb[2],
            std::clamp(p.fade*(transparent_target_ ? .72f : .92f),0.0f,1.0f));
        GlassPipeline(device_,true); device_->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
        device_->SetRenderState(D3DRS_SCISSORTESTENABLE,TRUE);
        const ImVec2 origin=ImGui::GetDrawData()->DisplayPos,scale=ImGui::GetDrawData()->FramebufferScale;
        RECT clip{LONG(std::max(0.0f,(std::max(p.clip[0],command->ClipRect.x)-origin.x)*scale.x)),
            LONG(std::max(0.0f,(std::max(p.clip[1],command->ClipRect.y)-origin.y)*scale.y)),
            LONG(std::min(float(win_w_)*scale.x,(std::min(p.clip[2],command->ClipRect.z)-origin.x)*scale.x)),
            LONG(std::min(float(win_h_)*scale.y,(std::min(p.clip[3],command->ClipRect.w)-origin.y)*scale.y))};
        if(clip.right<=clip.left || clip.bottom<=clip.top) return;
        device_->SetScissorRect(&clip); device_->SetPixelShader(nullptr);
        device_->SetTexture(0,nullptr); device_->SetTexture(1,nullptr);
        device_->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
        device_->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
        device_->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
        device_->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
        device_->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        device_->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
        struct Vertex {float x,y,z,rhw;DWORD color;}; Vertex v[38];
        auto vertex=[&](float x,float y) {return Vertex{(x-origin.x)*scale.x-.5f,(y-origin.y)*scale.y-.5f,0,1,color};};
        v[0]=vertex(p.cx,p.cy); int n=1;
        const float r=std::clamp(p.corner_radius,0.0f,std::min(p.hw,p.hh));
        for(int corner=0;corner<4;++corner) for(int step=0;step<=8;++step) {
            const float angle=(corner*90+step*90.0f/8)*kPI/180;
            const float x=p.cx+(corner==0 || corner==3 ? p.hw-r : -p.hw+r);
            const float y=p.cy+(corner<2 ? p.hh-r : -p.hh+r);
            v[n++]=vertex(x+std::cos(angle)*r,y+std::sin(angle)*r);
        }
        v[n++]=v[1]; device_->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,n-2,v,sizeof(Vertex));
        return;
    }
    const MaterialParams& m=Params(p.material);
    float c[26][4]{};
    auto row=[&](int n,float a,float b,float d,float e){c[n][0]=a;c[n][1]=b;c[n][2]=d;c[n][3]=e;};
    const ImVec2 origin=ImGui::GetDrawData()->DisplayPos,scale=ImGui::GetDrawData()->FramebufferScale;
    row(0,float(win_w_),float(win_h_),texture_w_/scale.x,texture_h_/scale.y);
    row(1,-origin.x-capture_x_/scale.x,-origin.y-capture_y_/scale.y,light_x_,light_y_); row(2,p.cx,p.cy,p.hw,p.hh);
    row(3,std::min(p.corner_radius,std::min(p.hw,p.hh)),p.fade,m.blur_mix,m.saturation);
    row(4,m.tint_rgb[0],m.tint_rgb[1],m.tint_rgb[2],1); row(5,m.tint_opacity,m.brightness,m.contrast,m.grain);
    row(6,m.refr_strength,m.refr_band,m.highlight,m.inner_shadow);
    row(7,m.border_intensity,m.border_width,std::min(.9f,m.shadow_strength*p.elevate),m.shadow_radius*p.elevate);
    const float pad=m.shadow_radius*p.elevate+std::max(std::abs(m.shadow_off_x),std::abs(m.shadow_off_y))*p.elevate+m.border_width+3;
    row(8,m.shadow_off_x*p.elevate,m.shadow_off_y*p.elevate,time_,pad); row(9,tilt_x_,tilt_y_,m.sheen,m.chroma);
    row(10,cursor_x_,cursor_y_,g_flow,0); row(11,p.b1x,p.b1y,p.b1w,p.b1h); row(12,p.b2x,p.b2y,p.b2w,p.b2h); row(13,p.blob_k,transparent_target_ ? 1.0f : 0.0f,0,0);
    const auto& e=edge_cfg_;
    row(14,e.fresnel_exp,e.bevel_scale,e.lip,e.specular_exp); row(15,e.specular_amt,e.front_amt,e.back_amt,e.sat_pop);
    row(16,e.bevel_tilt,e.light_height,e.edge_shadow,e.cursor_size); row(17,e.cursor_glow,e.sheen_amt,e.grain_amt,e.ambient_rim);
    row(18,e.front_spread,e.back_spread,e.panel_br_spread,e.panel_br_smooth); row(19,light2_x_,light2_y_,light1_amt_,light2_amt_);
    const float rim=e.panel_rim_angle*kPI/180;
    row(20,std::sin(rim),-std::cos(rim),e.tbl_fill,0); row(21,e.adapt_strength,e.adapt_pivot,e.adapt_soft,e.smooth_refraction);
    row(22,e.squircle_power,e.lens_amount,e.lens_power,e.lens_a); row(23,e.lens_b,e.lens_c,e.lens_d,e.glow_weight);
    row(24,e.glow_phase,e.glow_edge0,e.glow_edge1,e.glow_bias); row(25,e.glow_anim,0,0,0);
    GlassPipeline(device_,true); device_->SetRenderState(D3DRS_SCISSORTESTENABLE,TRUE);
    RECT clip{LONG(std::max(0.0f,(std::max(p.clip[0],command->ClipRect.x)-origin.x)*scale.x)),
        LONG(std::max(0.0f,(std::max(p.clip[1],command->ClipRect.y)-origin.y)*scale.y)),
        LONG(std::min(float(target_w_),(std::min(p.clip[2],command->ClipRect.z)-origin.x)*scale.x)),
        LONG(std::min(float(target_h_),(std::min(p.clip[3],command->ClipRect.w)-origin.y)*scale.y))};
    if(clip.right<=clip.left || clip.bottom<=clip.top) return;
    device_->SetScissorRect(&clip); device_->SetPixelShader(ps_); device_->SetPixelShaderConstantF(0,&c[0][0],26);
    device_->SetTexture(0,heavy_); device_->SetTexture(1,soft_); device_->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX2);
    struct Vertex {float x,y,z,rhw,lx,ly,cx,cy;};
    const float hw=p.hw+pad,hh=p.hh+pad;
    auto vertex=[&](float x,float y) { return Vertex{(p.cx+x-origin.x)*scale.x-.5f,(p.cy+y-origin.y)*scale.y-.5f,0,1,x,y,p.cx+x,p.cy+y}; };
    const Vertex vertices[4]={vertex(-hw,-hh),vertex(hw,-hh),vertex(-hw,hh),vertex(hw,hh)};
    device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex));
}
