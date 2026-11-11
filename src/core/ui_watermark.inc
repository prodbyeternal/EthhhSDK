namespace {
using GreetingClock=std::chrono::steady_clock;
GreetingClock::time_point greetingStart{};
bool greetingStarted=false,greetingSound=false;
#if defined(ETH_TESTING)
float greetingTestSeconds=-1;
#endif
struct WatermarkContent {std::vector<std::string> lines;ImVec2 size;};
WatermarkContent MakeWatermark() {
    WatermarkContent model;const int detail=std::clamp(Config::Get().GetInt("core","watermark_detail",1),0,2);
    const float s=ui::Scale();model.lines.push_back("EthhhSDK");
    if(detail==0) model.lines[0]+="  /  I-Ninja";
    if(detail>=1) {
        const auto rates=patch::ReadStatus();
        model.lines.push_back(rates.sampled ? Format("I-Ninja  /  %.0f FPS  /  %.0f Hz",rates.gamePresentsPerSecond,rates.activeTicksPerSecond) : "I-Ninja  /  F9 to explore");
    }
    if(detail>=2) {
        game::CameraView cam;
        if(game::ReadCamera(cam)) {
            model.lines.push_back(Format("cam %.1f %.1f %.1f",cam.x,cam.y,cam.z));
            model.lines.push_back(Format("fov %.1f deg",cam.fov*180.0f/3.14159265358979f));
        } else model.lines.push_back("cam: waiting for level");
        const auto player=game::PlayerTracker::Get().Addr();
        model.lines.push_back(Format("player %s   mods %zu/%zu",player ? Format("%08X",unsigned(player)).c_str() : "auto",
            mods::ModManager::Get().CountLoaded(),mods::ModManager::Get().CountEnabled()));
        model.lines.push_back(Format("hooks tick:%d d3d:%d wnd:%d  %s",hooks::TickHooked(),hooks::D3DHooked(),hooks::WndProcHooked(),overlay::IsVisible() ? "menu open" : "F9 menu"));
    }
    float width=0,height=0;
    for(size_t i=0;i<model.lines.size();++i) {
        ImFont* f=i==0 ? FontBold() : FontSmall();
        const ImVec2 size=f->CalcTextSizeA(f->FontSize,FLT_MAX,0,model.lines[i].c_str());
        width=std::max(width,size.x);height+=size.y+(i+1<model.lines.size() ? 4*s : 0);
    }
    model.size=ImVec2(width+64*s,std::max(44*s,height+20*s));return model;
}
void RenderIsland(float seconds,bool startup) {
    const auto& io=ImGui::GetIO();if(io.DisplaySize.x<16 || io.DisplaySize.y<16) return;
    const float s=ui::Scale(),margin=12*s;const auto model=MakeWatermark();
    const int corner=std::clamp(Config::Get().GetInt("core","watermark_pos",0),0,3);
    const bool reduced=Config::Get().GetBool("theme","reduce_motion",false);
    load::Rect target{corner==0 || corner==2 ? margin : io.DisplaySize.x-margin-model.size.x,
        corner<2 ? margin : io.DisplaySize.y-margin-model.size.y,model.size.x,model.size.y};
    const auto frame=load::Sample(startup ? seconds : 100,io.DisplaySize.x,io.DisplaySize.y,s,target,reduced);
    if(frame.fade<=0 || frame.rect.w<=0 || frame.rect.h<=0) return;
    if(startup && !greetingSound && frame.notice>.01f) {greetingSound=true;PlayUiSound(Sound::Welcome);}
    ImGui::SetNextWindowPos(ImVec2(frame.rect.x,frame.rect.y));
    ImGui::SetNextWindowSize(ImVec2(frame.rect.w,frame.rect.h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize,ImVec2(1,1));
    // The SAME window/surface becomes the watermark. No hidden measuring
    // window, snapshot clone, blob tail, or second island exists at any stage.
    ImGui::Begin("##ethhh_watermark",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|
        ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav);
    auto* dl=ImGui::GetWindowDrawList();const ImVec2 p(frame.rect.x,frame.rect.y);
    GlassPanel(p,ImVec2(frame.rect.w,frame.rect.h),std::min(24*s,frame.rect.h*.5f),Glass::Material::Regular,frame.fade);
    dl->PushClipRect(p,ImVec2(p.x+frame.rect.w,p.y+frame.rect.h),true);
    if(frame.notice>0) {
        const float a=frame.notice*frame.fade;
        const ImVec2 c(p.x+38*s,p.y+frame.rect.h*.5f);
        dl->AddCircleFilled(c,18*s,ui::WithAlpha(ui::Accent(),a),40);
        const float tick=std::clamp((seconds-load::kDelay-.35f)/.5f,0.0f,1.0f);
        const ImVec2 v0(c.x-7*s,c.y),v1(c.x-2*s,c.y+5*s),v2(c.x+8*s,c.y-6*s);
        const float first=std::clamp(tick*2,0.0f,1.0f),second=std::clamp(tick*2-1,0.0f,1.0f);
        if(first>0) dl->AddLine(v0,ImVec2(v0.x+(v1.x-v0.x)*first,v0.y+(v1.y-v0.y)*first),ui::WithAlpha(IM_COL32_WHITE,a),2.2f*s);
        if(second>0) dl->AddLine(v1,ImVec2(v1.x+(v2.x-v1.x)*second,v1.y+(v2.y-v1.y)*second),ui::WithAlpha(IM_COL32_WHITE,a),2.2f*s);
        const float textX=p.x+68*s;
        dl->AddText(FontBold(),FontBold()->FontSize,ImVec2(textX,c.y-17*s),ui::WithAlpha(ThemeInk(),a),"EthhhSDK loaded successfully!");
        dl->AddText(FontSmall(),FontSmall()->FontSize,ImVec2(textX,c.y+6*s),ui::WithAlpha(ThemeSoftInk(),a),"You're all set. Let's play.");
        const float celebration=reduced ? 0 : std::sin(std::clamp((seconds-load::kDelay-.45f)/1.0f,0.0f,1.0f)*3.14159265f);
        for(int i=0;i<3;++i) {
            const float angle=(-1.0f+i*.70f),r=(22+celebration*5)*s;
            const ImVec2 dot(c.x+std::sin(angle)*r,c.y-std::cos(angle)*r);
            dl->AddCircleFilled(dot,(1.0f+celebration*.5f)*s,ui::WithAlpha(ui::Accent(),a*celebration),12);
        }
    }
    if(frame.watermark>0) {
        const float a=frame.watermark*frame.fade;
        const ImVec2 c(p.x+27*s,p.y+frame.rect.h*.5f);
        dl->AddCircleFilled(c,13*s,ui::WithAlpha(ui::Accent(),a*.16f),32);
        Glass::DrawIcon(dl,Glass::Icon::Spark,c,14,ui::WithAlpha(ui::Accent(),a));
        float y=p.y+10*s;
        for(size_t i=0;i<model.lines.size();++i) {
            ImFont* f=i==0 ? FontBold() : FontSmall();
            dl->AddText(f,f->FontSize,ImVec2(p.x+48*s,y),ui::WithAlpha(i==0 ? ThemeInk() : ThemeSoftInk(),a),model.lines[i].c_str());
            y+=f->FontSize+4*s;
        }
    }
    dl->PopClipRect();ImGui::End();ImGui::PopStyleVar(2);
}
}
void ResetStartupWatermark() {greetingStarted=false;greetingSound=false;}
void DrawWatermark() {RenderIsland(100,false);}
void DrawStartupWatermark() {
    if(!greetingStarted) {greetingStart=GreetingClock::now();greetingStarted=true;}
    float seconds=std::chrono::duration<float>(GreetingClock::now()-greetingStart).count();
#if defined(ETH_TESTING)
    if(greetingTestSeconds>=0) seconds=greetingTestSeconds;
#endif
    // Wall-clock time: 60/100/144/240 Hz and temporary Reset stalls cannot
    // accelerate the four-second quiet period or replay the welcome sound.
    RenderIsland(seconds,true);
}
#if defined(ETH_TESTING)
void TestSetStartupSeconds(float seconds) {greetingTestSeconds=seconds;}
#endif
