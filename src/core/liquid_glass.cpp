#include "liquid_glass.h"
#include "imgui_ext.h"
#include "imgui_impl_dx9.h"
#include "log.h"

namespace eth::ui {
namespace { Glass::Renderer renderer;IDirect3DStateBlock9* renderState=nullptr; }
bool InitGlass(IDirect3DDevice9* device) {
    if(renderState) {renderState->Release();renderState=nullptr;}
    Glass::g=&renderer; // widgets remain usable with the readable SM3 fallback
    const bool ready=renderer.Init(device);
    ApplyTheme();
    if (!ready) ETH_WARN("Liquid Glass DX9: optical shaders unavailable; readable fallback active");
    return ready;
}
void ShutdownGlass() { if(renderState) {renderState->Release();renderState=nullptr;}renderer.Shutdown(); Glass::g=nullptr; }
void InvalidateGlass() { if(renderState) {renderState->Release();renderState=nullptr;}renderer.InvalidateDeviceObjects(); }
bool RecreateGlass() { return renderer.CreateDeviceObjects(); }
void BeginGlassFrame() {
    if (!Glass::g) Glass::g=&renderer;
    const ImGuiIO& io=ImGui::GetIO();
    renderer.BeginFrame(int(io.DisplaySize.x),int(io.DisplaySize.y),0,0,
        int(io.DisplaySize.x),int(io.DisplaySize.y),io.MousePos);
    Glass::SetWidgetScale(Scale());
    Glass::SfxResetConsumed();
}
void RenderGlassDrawData(IDirect3DDevice9* device,ImDrawData* data) {
    if (!device || !data) return;
    // Restore all sampler/shader constants, stream/index bindings, transforms,
    // texture stages and raster state touched by either backend, not just ImGui.
    if(!renderState && FAILED(device->CreateStateBlock(D3DSBT_ALL,&renderState))) return;
    IDirect3DStateBlock9* saved=renderState;
    if (FAILED(saved->Capture())) return;
    __try { renderer.PrepareBackdrop();ImGui_ImplDX9_RenderDrawData(data); }
    __finally { saved->Apply(); }
}
void DrawLauncherBackdrop(IDirect3DDevice9* device,int width,int height) {
    if(!device || width<1 || height<1) return;
    // A cached, original cyan/indigo ribbon field gives the pre-game glass a
    // real optical background without desktop duplication or external assets.
    struct Vertex {float x,y,z,rhw;DWORD color;};
    static std::vector<Vertex> vertices;
    static int cachedWidth=0,cachedHeight=0,cachedAppearance=-1;
    const int appearance=Glass::Appearance();
    if(cachedWidth!=width || cachedHeight!=height || cachedAppearance!=appearance) {
        vertices.clear(); vertices.reserve(80*48*6);
        auto vertex=[&](int i,int j) {
            const float x=float(i)/80,y=float(j)/48;
            const float wave=y-.48f-.18f*std::sin(x*6.1f+.4f);
            const float ribbon=std::exp(-wave*wave*42);
            const float cyan=std::exp(-((x-.83f)*(x-.83f)*3+(y-.20f)*(y-.20f)*4));
            const float violet=std::exp(-((x-.14f)*(x-.14f)*5+(y-.85f)*(y-.85f)*3));
            const float strand=std::pow(.5f+.5f*std::sin(wave*35+x*3),12.0f)*ribbon;
            float r=.025f+.09f*cyan+.30f*violet+.08f*strand;
            float g=.07f+.46f*cyan+.12f*ribbon+.22f*strand;
            float b=.16f+.40f*cyan+.37f*violet+.20f*ribbon+.17f*strand;
            if(appearance==1) {r=.40f+r*.65f;g=.45f+g*.65f;b=.55f+b*.50f;}
            return Vertex{x*width-.5f,y*height-.5f,0,1,D3DCOLOR_COLORVALUE(r,g,std::min(b,1.0f),1)};
        };
        for(int j=0;j<48;++j) for(int i=0;i<80;++i) {
            const Vertex a=vertex(i,j),b=vertex(i+1,j),c=vertex(i,j+1),d=vertex(i+1,j+1);
            vertices.insert(vertices.end(),{a,b,c,c,b,d});
        }
        cachedWidth=width;cachedHeight=height;cachedAppearance=appearance;
    }
    IDirect3DStateBlock9* saved=nullptr;
    if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&saved))) return;
    if(FAILED(saved->Capture())) {saved->Release();return;}
    device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);
    device->SetTexture(0,nullptr);device->SetTexture(1,nullptr);
    device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
    device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE,FALSE);device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);device->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
    device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_LIGHTING,FALSE);
    device->SetRenderState(D3DRS_FOGENABLE,FALSE);device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
    device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
    device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(vertices.size()/3),vertices.data(),sizeof(Vertex));
    saved->Apply();saved->Release();
}
void GlassPanel(ImVec2 pos,ImVec2 size,float radius,Glass::Material material,float fade) {
    if (!Glass::g) return;
    Glass::Primitive p; p.cx=pos.x+size.x*.5f; p.cy=pos.y+size.y*.5f;
    p.hw=size.x*.5f; p.hh=size.y*.5f; p.corner_radius=radius; p.material=material; p.fade=fade;
    Glass::g->Submit(p);
}
ImU32 ThemeInk() { return Glass::InkColor(); }
ImU32 ThemeSoftInk() { return Glass::InkSoftColor(); }
ImU32 ThemeSurface() { return Glass::Appearance()==1 ? IM_COL32(230,235,247,220) : IM_COL32(24,28,40,220); }
}
