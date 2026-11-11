// Off-screen preview: no game launch, no visible window, no keyboard/mouse input.
#include "precomp.h"
#include "imgui_ext.h"
#include "ui_pages.h"
#include "config.h"
#include "mods.h"
#include "launcher.h"
#include "ethhhpatch.h"
#include "imgui_impl_dx9.h"
#include "imgui_internal.h"
#include "liquid_glass.h"
#include "load_island_policy.h"
#include "ui_audio.h"
#include <gdiplus.h>
#include <stdexcept>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

static void Check(bool ok, const char* label) { if (!ok) throw std::runtime_error(label); }
static void Hr(HRESULT result, const char* label) { if(FAILED(result)) fprintf(stderr,"%s HRESULT 0x%08lX\n",label,(unsigned long)result);Check(SUCCEEDED(result), label); }
#include "glass_device_checks.inc"
#include "ui_cpu_checks.inc"

int main(int argc, char** argv)
{
    try {
        if(argc>1 && !strcmp(argv[1],"--cpu")) return RunCpuUiChecks();
        Check(argc >= 2, "usage: eth_chrome_preview output.png [width height scale stress patchPage patchTab scrollBottom page launcher light fallback startupSeconds]");
        int width = argc > 2 ? atoi(argv[2]) : 1152, height = argc > 3 ? atoi(argv[3]) : 704;
        float scale = argc > 4 ? (float)atof(argv[4]) : 1.0f;
        bool stress = argc > 5 && atoi(argv[5]);
        bool patchPage = argc > 6 && atoi(argv[6]);
        int patchTab = argc > 7 ? atoi(argv[7]) : 0;
        bool scrollBottom = argc > 8 && atoi(argv[8]);
        const char* tabs[] = {"Display", "Camera", "Animation", "PestControl", "Diagnostics"};
        Check(patchTab>=0 && patchTab<5,"preview tab index");
        const auto page = argc>9 ? (eth::ui::pages::Page)atoi(argv[9]) :
            (patchPage ? eth::ui::pages::Page::Patch : eth::ui::pages::Page::Mods);
        Check(int(page)>=0 && page<eth::ui::pages::Page::Count,"preview page");
        patchPage=page==eth::ui::pages::Page::Patch;
        const bool modsPage=page==eth::ui::pages::Page::Mods;
        const bool launcherMode=argc<=10 || atoi(argv[10])!=0;
        const bool light=argc>11 && atoi(argv[11]);
        const bool fallback=argc>12 && atoi(argv[12]);
        const float startupSeconds=argc>13 ? float(atof(argv[13])) : -1;
        const bool islandOnly=startupSeconds>=0;
        const bool transparent=argc>14 && atoi(argv[14]);
        const float managerOpacity=argc>15 ? float(atof(argv[15])) : 1;
        Check(width >= 640 && height >= 480 && width <= 4096 && height <= 2160, "preview dimensions");
        // Parse the actual trainer with vendored Lua 5.4; do not execute it or
        // register callbacks/read settings. The source path is a build define.
        lua_State* syntax=luaL_newstate(); Check(syntax!=nullptr,"Lua syntax state");
        const int parsed=luaL_loadfile(syntax,ETH_TRAINER_SOURCE);
        if(parsed!=LUA_OK) fprintf(stderr,"trainer syntax: %s\n",lua_tostring(syntax,-1));
        lua_close(syntax); Check(parsed==LUA_OK,"actual Lua trainer syntax");
        HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = instance; wc.lpszClassName = L"EthChromeOffscreen";
        RegisterClassW(&wc);
        HWND hwnd = CreateWindowW(wc.lpszClassName, L"Offscreen SDK preview", WS_OVERLAPPED,
                                  0,0,width,height,nullptr,nullptr,instance,nullptr);
        Check(hwnd != nullptr, "hidden window");
        IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION); Check(d3d != nullptr,"D3D9");
        D3DPRESENT_PARAMETERS pp{}; pp.Windowed=TRUE; pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow=hwnd; pp.BackBufferWidth=width; pp.BackBufferHeight=height;
        pp.BackBufferFormat=D3DFMT_A8R8G8B8; pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
        IDirect3DDevice9* device = nullptr;
        HRESULT createDevice=d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device);
        if(FAILED(createDevice)) {
            fprintf(stderr,"preview HAL HRESULT 0x%08lX\n",(unsigned long)createDevice);
            createDevice=d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_REF,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device);
        }
        Hr(createDevice,"preview device");
        IDirect3DSurface9* target=nullptr; IDirect3DSurface9* readable=nullptr;
        Hr(device->CreateRenderTarget(width,height,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr),"render target");
        Hr(device->CreateOffscreenPlainSurface(width,height,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&readable,nullptr),"readback surface");
        Hr(device->SetRenderTarget(0,target),"bind render target");

        eth::Config::Get().SetDefaults(); // in-memory only; no user settings loaded
        eth::Config::Get().SetInt("launcher","last_page",(int)page);
        eth::Config::Get().SetInt("theme","appearance",light ? 1 : 0);
        eth::Config::Get().SetBool("core","watermark",false); // obsolete setting must not hide it
        auto& mgr = eth::mods::ModManager::Get(); mgr.SetLauncherMode(launcherMode);
        eth::mods::ModInfo hl; hl.id="halflife_movement"; hl.name="Half-Life Movement + FPS Camera";
        hl.version="0.1.0"; hl.type="csharp"; hl.author="EthhhSDK"; hl.enabled=true; hl.status="Ready on game start";
        hl.description="Half-Life acceleration, friction and bunnyhopping with a mouse-look FPS camera. F2 toggles; F3 stops. Experimental collision adapter: pin the ninja before activating.";
        eth::mods::ModInfo trainer; trainer.id="trainer"; trainer.name="I-Ninja Trainer"; trainer.version="0.1.0";
        trainer.type="lua"; trainer.author="EthhhSDK"; trainer.enabled=true; trainer.status="Ready on game start";
        trainer.description="Game speed, camera tools, player tracking, save stats and live inspection. Written entirely with the public Lua API.";
        mgr.List().push_back(hl); mgr.List().push_back(trainer);
        if (stress) {
            eth::mods::ModInfo error=trainer; error.id="stress";
            error.name="Layout stress fixture with a deliberately long title that must wrap instead of overlapping the toggle";
            error.description=hl.description+" "+hl.description;
            error.status="Synthetic load error: this intentionally lengthy status should remain inside the card and should never overlap another card or become inaccessible behind a disabled scrollbar.";
            error.errored=true; error.enabled=false; mgr.List().push_back(error);
        }
        if (patchPage && stress) {
            eth::patch::Status fixture;
            fixture.sampled = fixture.deviceReady = fixture.projectionValid = true;
            fixture.width = fixture.viewportWidth = 1920; fixture.height = fixture.viewportHeight = 1080;
            fixture.gamePresentsPerSecond = 100; fixture.tickCallsPerSecond = fixture.activeTicksPerSecond = 60;
            fixture.simulationSecondsPerSecond = 1; fixture.engineAspect = 4.0f/3;
            fixture.projection = {4.0/3, 60, eth::patch::HorizontalFov(60,4.0/3), .01, 200};
            fixture.projectionSamples = 12345; fixture.cadence = {400,2,3}; fixture.observationAgeSeconds = .1;
            fixture.resetAttempts = 2; fixture.resetSuccesses = 1; fixture.resetFailures = 1;
            fixture.lastResetHresult = D3DERR_DEVICELOST; fixture.lastResetParametersValid = true;
            fixture.resetWidth = 1920; fixture.resetHeight = 1080; fixture.resetFullscreenHz = 100;
            fixture.resetInterval = D3DPRESENT_INTERVAL_ONE; fixture.resetWindowed = false;
            eth::patch::SetPreviewStatus(fixture);
        }
        eth::ui::LoadFonts(scale); ImGui_ImplDX9_Init(device);
        Check(eth::ui::InitGlass(device),"actual DX9 glass / Dual Kawase SM3 shaders compile");
        Check(eth::ui::UiAudioReadyCount()==5,"all five user sounds embed and decode as PCM16 (no playback in tests)");
        Glass::g->SetTransparentTarget(transparent);
        eth::ui::SetManagerOpacity(managerOpacity);
        eth::ui::pages::ResetStartupWatermark();eth::ui::pages::TestSetStartupSeconds(startupSeconds);
        if(fallback) eth::ui::InvalidateGlass();
        eth::ui::pages::SetCurrentPage(page);
        bool startRequested=false, exitRequested=false, open=true;
        bool previewWatermark=false;
        auto drawFrame = [&]() {
            ImGuiIO& io=ImGui::GetIO(); io.DisplaySize=ImVec2((float)width,(float)height); io.DeltaTime=1.0f/60;
            ImGui_ImplDX9_NewFrame(); ImGui::NewFrame();
            eth::ui::BeginGlassFrame();
            ImGui::SetNextWindowPos(ImVec2(0,0));
            startRequested=false; exitRequested=false; open=true;
            if (!islandOnly && eth::ui::BeginChromeWindow("##preview","EthhhSDK",launcherMode ? "I-Ninja / Pre-game mod manager" : "I-Ninja / In-game mod manager",&open,io.DisplaySize,
                                          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
                eth::ui::pages::DrawManagerLayout(launcherMode,&startRequested,&exitRequested);
                eth::ui::EndChromeWindow();
            }
            if (previewWatermark) eth::ui::pages::DrawWatermark();
            else if(islandOnly) eth::ui::pages::DrawStartupWatermark();
            Check(ImGui::GetCurrentContext()->CurrentWindowStack.Size==1,"chrome window/child stack pairing");
            Check(ImGui::GetCurrentContext()->StyleVarStack.Size==0 && ImGui::GetCurrentContext()->ColorStack.Size==0,
                  "chrome style pairing");
            ImGui::Render();
            Hr(device->Clear(0,nullptr,D3DCLEAR_TARGET,transparent ? 0 : D3DCOLOR_ARGB(255,18,20,25),1,0),"clear");
            Hr(device->BeginScene(),"begin scene");
            if(!transparent) eth::ui::DrawLauncherBackdrop(device,width,height);
            eth::ui::RenderGlassDrawData(device,ImGui::GetDrawData()); Hr(device->EndScene(),"end scene");
            Check(fallback || Glass::g->PrimitiveCount()==0 || Glass::g->BackdropReady(),"scene capture and six-level heavy/soft blur succeed");
            if(islandOnly && startupSeconds<eth::ui::load::kDelay && !previewWatermark)
                Check(Glass::g->PrimitiveCount()==0,"four-second quiet period has no surface or hidden measuring clone");
        };
        drawFrame(); eth::ui::pages::SetCurrentPage(page); // sidebar initialization preserves the actual default
        for(int frame=0; frame<60; ++frame) drawFrame();
        if (patchPage) {
            ImGuiWindow* patch=nullptr;
            for (ImGuiWindow* w:ImGui::GetCurrentContext()->Windows) {
                const char* leaf=strrchr(w->Name,'/');
                if (w->Active && leaf && strncmp(leaf,"/##patch_",9)==0) patch=w;
            }
            Check(patch!=nullptr,"real patch window found");
            ImGuiTabBar* bar=ImGui::GetCurrentContext()->TabBars.GetByKey(patch->GetID("##patch_tabs"));
            Check(bar && bar->Tabs.Size==5,"all five real patch tabs exist");
            // The pointer overload is valid outside a frame; the name overload
            // hashes through CurrentWindow and must not be called after Render.
            ImGui::TabBarQueueFocus(bar,&bar->Tabs[patchTab]);
            for(int frame=0; frame<8; ++frame) drawFrame();
            ImGuiTabItem* selected=ImGui::TabBarFindTabByID(bar,bar->SelectedTabId);
            Check(selected && !strcmp(ImGui::TabBarGetTabName(bar,selected),tabs[patchTab]),"requested patch tab selected");
            if (patchTab==1) {
                // The longer camera/FOV/input page must remain reachable by
                // scrolling. Exercise both ends without any Windows input.
                for(int frame=0;frame<8;++frame) {
                    ImGui::SetScrollY(patch,patch->ScrollMax.y); drawFrame();
                }
                Check(std::abs(patch->Scroll.y-patch->ScrollMax.y)<1,"camera page bottom reachable");
                if (!scrollBottom) for(int frame=0;frame<8;++frame) {
                    ImGui::SetScrollY(patch,0); drawFrame();
                }
            }
        }
        int cards=0;
        for(ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
            if (w->Active && strstr(w->Name,"/##card_")) {
                ++cards;
                Check(w->ScrollMax.y < 1.0f,"card contents clipped vertically");
                Check(w->ScrollMax.x < 1.0f,"card contents clipped horizontally");
            }
        }
        if (modsPage && !islandOnly) Check(cards >= 2,"expected visible real mod cards");
        if (patchPage) {
            bool found=false;
            for(ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
                if(w->Active && strstr(w->Name,"/##patch_")) {
                    found=true;
                    if (w->ScrollMax.x>=1) fprintf(stderr,"patch overflow: %s, size %.1f, content %.1f, inner %.1f, scroll x %.1f\n",
                        w->Name,w->Size.x,w->ContentSize.x,w->InnerRect.GetWidth(),w->ScrollMax.x);
                    Check(w->ScrollMax.x<1,"EthhhPatch page clipped horizontally");
                }
            }
            Check(found,"real EthhhPatch page rendered");
        }
        Hr(device->GetRenderTargetData(target,readable),"read back");
        D3DLOCKED_RECT pixels{}; Hr(readable->LockRect(&pixels,nullptr,D3DLOCK_READONLY),"lock pixels");
        Gdiplus::GdiplusStartupInput gdiplusInput; ULONG_PTR token=0;
        Check(Gdiplus::GdiplusStartup(&token,&gdiplusInput,nullptr)==Gdiplus::Ok,"GDI+");
        {
            if(transparent && !islandOnly) {
                const auto* row=static_cast<const unsigned char*>(pixels.pBits)+(height-60)*pixels.Pitch;
                const unsigned alpha=row[(width/2)*4+3];
                Check(alpha>0 && alpha<250,"launcher material really writes translucent alpha");
                const unsigned corner=static_cast<const unsigned char*>(pixels.pBits)[3];
                Check(corner<32,"outside rounded launcher is transparent, not a dark rectangular window");
            }
            Gdiplus::Bitmap bitmap(width,height,pixels.Pitch,transparent ? PixelFormat32bppARGB : PixelFormat32bppRGB,(BYTE*)pixels.pBits);
            CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0,0,0xf8,0x1e,0xf3,0x2e}};
            int count=MultiByteToWideChar(CP_UTF8,0,argv[1],-1,nullptr,0); std::wstring output(count,L'\0');
            MultiByteToWideChar(CP_UTF8,0,argv[1],-1,&output[0],count);
            Check(bitmap.Save(output.c_str(),&png,nullptr)==Gdiplus::Ok,"save PNG");
        }
        Gdiplus::GdiplusShutdown(token); readable->UnlockRect();

        // Gameplay watermark contract; keep the pre-game screenshots accurate
        // by testing the overlay only after the manager PNG has been saved.
        previewWatermark=true;
        for(int corner=0;corner<4;++corner) for(int detail=0;detail<3;++detail) {
            eth::Config::Get().SetInt("core","watermark_pos",corner);
            eth::Config::Get().SetInt("core","watermark_detail",detail);
            for(int frame=0;frame<3;++frame) drawFrame();
            ImGuiWindow* watermark=ImGui::FindWindowByName("##ethhh_watermark");
            Check(watermark && watermark->Active,"always-on watermark despite persisted disable");
            Check(watermark->ScrollMax.x<1 && watermark->ScrollMax.y<1,"watermark unclipped");
            Check(watermark->Pos.x>=0 && watermark->Pos.y>=0 &&
                watermark->Pos.x+watermark->Size.x<=width && watermark->Pos.y+watermark->Size.y<=height,"watermark stays on screen");
        }
        previewWatermark=false;
        CheckGlassDeviceState(device,drawFrame,width,height);
        if(!fallback) {
            eth::ui::InvalidateGlass();ImGui_ImplDX9_InvalidateDeviceObjects();
            Check(!Glass::g->Ready() && !Glass::g->BackdropReady(),"glass invalidation releases all default-pool optics");
            IDirect3DSurface9* back=nullptr;Hr(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back),"reset backbuffer");
            Hr(device->SetRenderTarget(0,back),"restore backbuffer before Reset");back->Release();
            target->Release();target=nullptr;
            Hr(device->Reset(&pp),"actual device Reset succeeds without retained default-pool glass resources");
            Hr(device->CreateRenderTarget(width,height,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr),"recreate offscreen target");
            Hr(device->SetRenderTarget(0,target),"bind recreated target");
            Check(ImGui_ImplDX9_CreateDeviceObjects() && eth::ui::RecreateGlass(),"font atlas and glass shaders recreate after Reset");
            for(int frame=0;frame<8;++frame) drawFrame();
        }
        if(islandOnly && startupSeconds>=eth::ui::load::kDelay+eth::ui::load::kDuration) {
            Check(Glass::g->CaptureWidth()*Glass::g->CaptureHeight()<width*height/10,"steady watermark blurs less than ten percent of the framebuffer");
            printf("watermark atlas: %dx%d versus framebuffer %dx%d\n",Glass::g->CaptureWidth(),Glass::g->CaptureHeight(),width,height);
        }

        // Synthetic ImGui navigation only: no keyboard/mouse events are sent to
        // Windows, no visible window, and launcher::Run is never called.
        if(!islandOnly) {
        ImGuiWindow* sidebar=nullptr; ImGuiWindow* modlist=nullptr;
        for(ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
            const char* leaf=strrchr(w->Name,'/');
            if(leaf && strncmp(leaf,"/##sidebar_",11)==0) sidebar=w;
            if(leaf && strncmp(leaf,"/##modlist_",11)==0) modlist=w;
        }
        Check(sidebar!=nullptr,"sidebar window found");
        if(launcherMode) {
        ImGui::ActivateItemByID(sidebar->GetID("Start game")); drawFrame();
        Check(startRequested && !exitRequested,"real Start button requests start, not exit");
        eth::launcher::TestReset(); eth::launcher::TestApplyRequests(startRequested,exitRequested);
        Check(eth::launcher::TestState()==1,"Start routing continues into game");
        ImGui::ActivateItemByID(sidebar->GetID("Exit")); drawFrame();
        Check(!startRequested && exitRequested,"real Exit button requests exit, not start");
        eth::launcher::TestReset(); eth::launcher::TestApplyRequests(startRequested,exitRequested);
        Check(eth::launcher::TestState()==0,"Exit routing does not start game");
        }
        ImGuiWindow* chrome=ImGui::FindWindowByName("##preview"); Check(chrome!=nullptr,"chrome window found");
        ImGuiID close=ImHashStr("##close",0,ImHashStr("##preview",0,chrome->IDStack[0]));
        ImGui::ActivateItemByID(close); drawFrame(); Check(!open,"real close button closes manager");
        eth::launcher::TestReset(); eth::launcher::TestApplyRequests(false,!open);
        Check(eth::launcher::TestState()==0,"close routing exits after balanced frame");
        eth::launcher::TestReset(); eth::launcher::TestApplyRequests(true,true);
        Check(eth::launcher::TestState()==0,"Exit wins conflicting frame requests");
        for(UINT key : { VK_RETURN, VK_ESCAPE }) {
            eth::launcher::TestReset(); ImGui::GetIO().WantTextInput=true;
            eth::launcher::TestMessage(hwnd,WM_KEYDOWN,key);
            Check(eth::launcher::TestState()==2,"typing does not trigger launcher shortcut");
            ImGui::GetIO().WantTextInput=false; eth::launcher::TestMessage(hwnd,WM_KEYDOWN,key);
            Check(eth::launcher::TestState()==(key==VK_RETURN ? 1 : 0),"launcher keyboard routing");
        }
        eth::launcher::TestReset(); eth::launcher::TestMessage(hwnd,WM_SYSKEYDOWN,VK_F4);
        Check(eth::launcher::TestState()==0,"Alt-F4 routing");
        eth::launcher::TestReset(); eth::launcher::TestMessage(hwnd,WM_CLOSE,0);
        Check(eth::launcher::TestState()==0,"WM_CLOSE routing");
        eth::launcher::TestMessage(hwnd,WM_DESTROY,0);
        MSG message{}; Check(!PeekMessageW(&message,nullptr,WM_QUIT,WM_QUIT,PM_REMOVE),"launcher teardown does not poison game message loop");
        if(stress && modsPage) {
            Check(modlist && modlist->ScrollMax.y>0,"stress list exposes scrolling");
            for(int frame=0; frame<8; ++frame) {
                ImGui::SetScrollY(modlist,modlist->ScrollMax.y); drawFrame();
            }
            // The last card is the synthetic error fixture; it must become visible.
            ImGuiWindow* last=nullptr;
            for(ImGuiWindow* w : ImGui::GetCurrentContext()->Windows)
                if(w->Active && strstr(w->Name,"/##card_") && (!last || w->Pos.y>last->Pos.y)) last=w;
            if(last && (last->Pos.y<modlist->InnerClipRect.Min.y-1 || last->Pos.y+last->Size.y>modlist->InnerClipRect.Max.y+1))
                fprintf(stderr,"tail card y %.1f height %.1f, list clip %.1f..%.1f scroll %.1f/%.1f\n",last->Pos.y,last->Size.y,modlist->InnerClipRect.Min.y,modlist->InnerClipRect.Max.y,modlist->Scroll.y,modlist->ScrollMax.y);
            Check(last && last->Pos.y>=modlist->InnerClipRect.Min.y-1 &&
                  last->Pos.y+last->Size.y<=modlist->InnerClipRect.Max.y+1,"stress error card reachable by scrolling");
        }
        }
        eth::ui::ShutdownGlass(); ImGui_ImplDX9_Shutdown(); ImGui::DestroyContext(); readable->Release(); target->Release(); device->Release(); d3d->Release();
        DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName,instance);
        printf("PASS off-screen chrome + launcher routing: %dx%d scale %.2f, %s, %d unclipped cards -> %s\n",width,height,scale,
            patchPage ? tabs[patchTab] : (modsPage ? "Mods" : "manager page"),cards,argv[1]);
        return 0;
    } catch(const std::exception& ex) { fprintf(stderr,"preview failed: %s\n",ex.what()); return 1; }
}
