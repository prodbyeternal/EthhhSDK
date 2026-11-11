// Same production controls/fonts/pages as the rendered preview, but this mode
// creates no HWND or D3D device and remains usable during fullscreen gameplay.
#include "lua_host.h"
#include <limits>
extern "C" int __cdecl EthImGui_Checkbox(const char*,int*);

static int RunCpuUiChecks() {
    int assertions=0;
    auto check=[&](bool ok,const char* label) {++assertions;Check(ok,label);};
    eth::Config::Get().SetDefaults();eth::ui::LoadFonts(1);
    unsigned char* pixels=nullptr;int fw=0,fh=0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&fw,&fh);
    check(pixels && fw>0 && fh>0,"embedded font atlas builds without a graphics device");
    eth::ui::InitUiAudio();check(eth::ui::UiAudioReadyCount()==5,"all five supplied sounds embed/decode, without playback");
    auto begin=[&](int width=900,int height=640) {
        auto& io=ImGui::GetIO();io.DisplaySize=ImVec2(float(width),float(height));io.DeltaTime=1.0f/60;
        ImGui::NewFrame();eth::ui::BeginGlassFrame();
        ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("##cpu_controls",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDecoration);
    };
    auto finish=[&]() {
        ImGui::End();ImGui::Render();
        auto* data=ImGui::GetDrawData();
        for(int n=0;n<data->CmdListsCount;++n) for(const auto& v:data->CmdLists[n]->VtxBuffer)
            check(std::isfinite(v.pos.x) && std::isfinite(v.pos.y),"control vertices stay finite");
        check(ImGui::GetCurrentContext()->StyleVarStack.Size==0 && ImGui::GetCurrentContext()->ColorStack.Size==0,"CPU style stacks balanced");
    };
    lua_State* L=luaL_newstate();check(L!=nullptr,"isolated Lua binding state");eth::lua::RegisterImGui(L);
    begin();
    check(luaL_dostring(L,"a,b=imgui.checkbox_flags('bit31',2147483648,2147483648); c,d=imgui.checkbox_flags('allbits',4294967295,4294967295); e,f=imgui.checkbox_flags('mixed',1,3)")==LUA_OK,"production Lua animated flags binding executes");
    for(const auto& field:std::initializer_list<std::pair<const char*,lua_Integer>>{{"a",-2147483648LL},{"c",-1},{"e",1}}) {
        lua_getglobal(L,field.first);check(lua_tointeger(L,-1)==field.second,"Lua flags retain signed 32-bit return and all bits");lua_pop(L,1);
    }
    check((ImGui::GetCurrentContext()->LastItemData.ItemFlags&ImGuiItemFlags_MixedValue)!=0,"partially selected masks draw mixed state");
    int nativeValue=1;check(!EthImGui_Checkbox("C ABI checkbox",&nativeValue) && nativeValue==1,"managed C ABI still accepts integer checkbox storage");
    float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity(),fixed=2,reverse=7,wide=0;
    check(eth::ui::GlassSlider("NaN",&nan,0,1) && nan==0,"nonfinite slider value recovers safely");
    check(eth::ui::GlassSlider("Infinity",&inf,0,1) && inf==1,"infinite slider value clamps safely");
    check(eth::ui::GlassSlider("Fixed",&fixed,1,1) && fixed==1,"zero-width slider range has finite geometry");
    check(!eth::ui::GlassSlider("Reversed",&reverse,8,2) && reverse==7,"invalid slider range cannot mutate user value");
    check(!eth::ui::GlassSlider("Wide",&wide,-FLT_MAX,FLT_MAX),"finite extreme slider range does not overflow");
    ImGui::PushItemWidth(4);eth::ui::GlassSlider("Tiny",&wide,0,1);ImGui::PopItemWidth();
    finish();
    // Generate mouse events inside ImGui ONLY; no Windows input is sent.
    bool value=false;ImVec2 center;
    for(int frame=0;frame<4;++frame) {
        if(frame==2) ImGui::GetIO().AddMouseButtonEvent(0,true);
        if(frame==3) ImGui::GetIO().AddMouseButtonEvent(0,false);
        begin();const bool clicked=eth::ui::Checkbox("Click regression",&value);
        center=ImVec2(ImGui::GetItemRectMin().x+10,ImGui::GetItemRectMin().y+10);
        if(frame==3) {check(clicked && value,"animated checkbox toggles exactly once on release");check(ImGui::IsItemEdited(),"checkbox preserves edited-item metadata");}
        finish();ImGui::GetIO().AddMousePosEvent(center.x,center.y);
    }
    lua_close(L);
    // In-memory layout fixtures only: do not scan/load mods or read manifests.
    auto& manager=eth::mods::ModManager::Get();
    eth::mods::ModInfo fixture;
    fixture.id="cpu_layout";fixture.name="Layout stress fixture with a deliberately long title that must wrap instead of overlapping the toggle";
    fixture.type="csharp";fixture.version="0.1.0";fixture.author="Offline fixture";fixture.enabled=true;
    fixture.description="Long descriptions must remain inside their card in small and HiDPI layouts, with all controls reachable and no horizontal scrolling.";
    manager.List().push_back(fixture);
    fixture.id="cpu_error";fixture.type="lua";fixture.enabled=false;fixture.errored=true;
    fixture.status="Synthetic load error: this intentionally lengthy status should remain inside the card and should never overlap another card or become inaccessible behind a disabled scrollbar.";
    manager.List().push_back(fixture);
    // All actual manager pages in both appearances/modes and small/HiDPI/4K
    // layouts. This is structural validation, not optical/DWM acceptance.
    struct Size {int w,h;float scale;};
    const Size sizes[]={{640,480,1},{900,640,1.1f},{1728,1056,1.5f},{3840,2160,2}};
    for(const auto size:sizes) {
        eth::ui::LoadFonts(size.scale);ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&fw,&fh);
        for(bool light:{false,true}) for(bool launcher:{false,true}) for(int page=0;page<int(eth::ui::pages::Page::Count);++page) {
            eth::Config::Get().SetInt("theme","appearance",light ? 1 : 0);eth::ui::ApplyTheme();
            eth::ui::pages::SetCurrentPage(eth::ui::pages::Page(page));
            const bool patchPage = page == int(eth::ui::pages::Page::Patch);
            for (int tab = 0; tab < (patchPage ? 5 : 1); ++tab) {
            for (bool bottom : {false, true}) {
            ImGuiTabBar* selectedBar=nullptr;
            for(int frame=0;frame<6;++frame) {
                eth::ui::pages::SetCurrentPage(eth::ui::pages::Page(page));
                auto& io=ImGui::GetIO();io.DisplaySize=ImVec2(float(size.w),float(size.h));io.DeltaTime=1.0f/60;
                io.AddMousePosEvent(-FLT_MAX,-FLT_MAX);ImGui::NewFrame();eth::ui::BeginGlassFrame();
                ImGui::SetNextWindowPos(ImVec2(0,0));eth::ui::SetManagerOpacity(.5f);
                bool open=true;
                if(eth::ui::BeginChromeWindow("##cpu_manager","EthhhSDK","I-Ninja",&open,io.DisplaySize,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove)) {
                    eth::ui::pages::DrawManagerLayout(launcher);eth::ui::EndChromeWindow();
                }
                ImGui::Render();
                if (patchPage && frame == 1) {
                    ImGuiTabBar* bar = nullptr;
                    for (auto* w : ImGui::GetCurrentContext()->Windows) {
                        const char* leaf = strrchr(w->Name, '/');
                        if (w->Active && leaf && strncmp(leaf, "/##patch_", 9) == 0)
                            bar = ImGui::GetCurrentContext()->TabBars.GetByKey(w->GetID("##patch_tabs"));
                    }
                    check(bar && bar->Tabs.Size == 5, "all five actual EthhhPatch tabs exist");
                    ImGui::TabBarQueueFocus(bar, &bar->Tabs[tab]);
                    selectedBar=bar;
                }
                for(auto* w:ImGui::GetCurrentContext()->Windows) if(w->Active && strstr(w->Name,"##cpu_manager"))
                    ImGui::SetScrollY(w,bottom ? w->ScrollMax.y : 0);
            }
            if (patchPage) {
                const char* tabs[]={"Display","Camera","Animation","PestControl","Diagnostics"};
                ImGuiTabItem* selected=ImGui::TabBarFindTabByID(selectedBar,selectedBar->SelectedTabId);
                check(selected && !strcmp(ImGui::TabBarGetTabName(selectedBar,selected),tabs[tab]),"requested actual patch tab is selected");
            }
            for(auto* w:ImGui::GetCurrentContext()->Windows) if(w->Active && strstr(w->Name,"##cpu_manager")) {
                if(w->ScrollMax.x>=1) fprintf(stderr,"CPU overflow: page %d tab %d %s, %dx%d scale %.2f, %s %.1f\n",page,tab,bottom ? "bottom" : "top",size.w,size.h,size.scale,w->Name,w->ScrollMax.x);
                check(w->ScrollMax.x<1,"real manager page stays inside horizontal bounds");
                check(std::abs(w->Scroll.y-(bottom ? w->ScrollMax.y : 0))<1,"real manager page reaches requested scroll end");
                for(const auto& v:w->DrawList->VtxBuffer) {
                    check(std::isfinite(v.pos.x) && std::isfinite(v.pos.y),"real manager vertices finite");
                    check((v.col>>IM_COL32_A_SHIFT)<=128,"manager contents cannot fade ahead of their single pane");
                }
            }
            check(ImGui::GetCurrentContext()->CurrentWindowStack.Size==0,"all real pages finish balanced");
            }
            }
        }
    }
    eth::ui::SetManagerOpacity(1);eth::ui::ShutdownGlass();eth::ui::ShutdownUiAudio();ImGui::DestroyContext();
    printf("PASS CPU production UI/layout/bindings/audio checks: %d assertions (no graphics device or playback)\n",assertions);
    return 0;
}
