// EthhhSDK connected controls. Glass optics live on the shared material pane;
// controls use one shape/one knob, never a head+lagging-tail duplicate.
namespace {
float g_managerOpacity=1;
size_t g_managerFirstPrimitive=0;
ImU32 Mix(ImU32 a,ImU32 b,float t) {return LerpColor(a,b,std::clamp(t,0.0f,1.0f));}
float Animate(ImGuiID id,unsigned slot,float target,float initial) {
    if(!Glass::g) return target;
    auto& spring=Glass::g->springs().Get(id,slot,Glass::SpringStyle::CriticalFast,initial);
    spring.target=target;spring.Tick(ImGui::GetIO().DeltaTime);return spring.x;
}
ImU32 ControlFill() {return Glass::Appearance()==1 ? IM_COL32(255,255,255,112) : IM_COL32(255,255,255,22);}
void CheckStroke(ImDrawList* dl,ImVec2 center,float size,float progress,ImU32 color) {
    const ImVec2 a(center.x-size*.28f,center.y),b(center.x-size*.07f,center.y+size*.21f),c(center.x+size*.30f,center.y-size*.23f);
    const float first=std::clamp(progress*2.6f,0.0f,1.0f),second=std::clamp((progress-.38f)/.62f,0.0f,1.0f);
    if(first>0) dl->AddLine(a,ImVec2(a.x+(b.x-a.x)*first,a.y+(b.y-a.y)*first),color,2.1f*Scale());
    if(second>0) dl->AddLine(b,ImVec2(b.x+(c.x-b.x)*second,b.y+(c.y-b.y)*second),color,2.1f*Scale());
}
bool ConnectedButton(const char* label,bool primary,ImVec2 size) {
    const float s=Scale();const ImVec2 text=ImGui::CalcTextSize(label,nullptr,true);
    if(size.x<0) size.x=ImGui::GetContentRegionAvail().x;
    if(size.x==0) size.x=text.x+30*s;
    if(size.y==0) size.y=36*s;
    size.x=std::max(1.0f,std::min(size.x,ImGui::GetContentRegionAvail().x));
    const ImVec2 p=ImGui::GetCursorScreenPos();const ImGuiID id=ImGui::GetID(label);
    const bool clicked=ImGui::InvisibleButton(label,size,ImGuiButtonFlags_EnableNav);
    const bool hovered=ImGui::IsItemHovered(),active=ImGui::IsItemActive();
    const float press=Animate(id,101,active ? .975f : 1,1),hover=Animate(id,102,hovered ? 1 : 0,0);
    const ImVec2 center(p.x+size.x*.5f,p.y+size.y*.5f),half(size.x*.5f*press,size.y*.5f*press);
    ImU32 fill=primary ? Accent() : Mix(ControlFill(),WithAlpha(ThemeInk(),.16f),hover);
    fill=WithAlpha(fill,ImGui::GetStyle().Alpha*(active ? .8f : 1));
    auto* dl=ImGui::GetWindowDrawList();dl->AddRectFilled(ImVec2(center.x-half.x,center.y-half.y),ImVec2(center.x+half.x,center.y+half.y),fill,std::min(half.y,12*s));
    const ImU32 ink=primary ? IM_COL32_WHITE : ThemeInk();
    const char* end=ImGui::FindRenderedTextEnd(label);
    dl->AddText(ImVec2(center.x-text.x*.5f,center.y-text.y*.5f),WithAlpha(ink,ImGui::GetStyle().Alpha),label,end);
    if(clicked && strcmp(label,"Start game")) PlayUiSound(Sound::Click);
    return clicked;
}
}
void SetManagerOpacity(float alpha) {g_managerOpacity=std::clamp(alpha,0.0f,1.0f);}
bool Checkbox(const char* label,bool* value) {
    const float s=Scale(),box=20*s,gap=9*s;
    const ImVec2 p=ImGui::GetCursorScreenPos();const ImGuiID id=ImGui::GetID(label);
    const char* end=ImGui::FindRenderedTextEnd(label);
    const float avail=std::max(box,ImGui::GetContentRegionAvail().x);
    const bool hasLabel=end>label;
    const ImVec2 text=ImGui::CalcTextSize(label,end,false,std::max(1.0f,avail-box-gap));
    const ImVec2 size(hasLabel ? std::min(avail,box+gap+text.x) : box,std::max(box,hasLabel ? text.y : 0.0f));
    const bool clicked=ImGui::InvisibleButton(label,size,ImGuiButtonFlags_EnableNav);
    const bool mixed=(ImGui::GetCurrentContext()->LastItemData.ItemFlags&ImGuiItemFlags_MixedValue)!=0;
    if(clicked) {*value=!*value;ImGui::MarkItemEdited(id);PlayUiSound(*value ? Sound::On : Sound::Off);}
    const float t=std::clamp(Animate(id,110,*value ? 1 : 0,*value ? 1 : 0),0.0f,1.0f);
    const float dash=std::clamp(Animate(id,112,mixed ? 1 : 0,mixed ? 1 : 0),0.0f,1.0f);
    const float fill=std::max(t,dash);
    const float press=Animate(id,111,ImGui::IsItemActive() ? .94f : 1,1);
    const ImVec2 c(p.x+box*.5f,p.y+box*.5f);const float r=box*.5f*press;
    auto* dl=ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(c.x-r,c.y-r),ImVec2(c.x+r,c.y+r),WithAlpha(Mix(ControlFill(),Accent(),fill),ImGui::GetStyle().Alpha),6*s);
    if(fill<.99f) dl->AddRect(ImVec2(c.x-r,c.y-r),ImVec2(c.x+r,c.y+r),WithAlpha(ThemeSoftInk(),.28f*(1-fill)*ImGui::GetStyle().Alpha),6*s);
    CheckStroke(dl,c,box,t*(1-dash),WithAlpha(IM_COL32_WHITE,ImGui::GetStyle().Alpha));
    if(dash>0) dl->AddLine(ImVec2(c.x-box*.25f*dash,c.y),ImVec2(c.x+box*.25f*dash,c.y),WithAlpha(IM_COL32_WHITE,dash*ImGui::GetStyle().Alpha),2.1f*s);
    if(hasLabel) dl->AddText(nullptr,0,ImVec2(p.x+box+gap,p.y+(box-std::min(box,text.y))*.5f),
        WithAlpha(ThemeInk(),ImGui::GetStyle().Alpha),label,end,std::max(1.0f,avail-box-gap));
    return clicked;
}
bool CheckboxFlags(const char* label,unsigned int* value,unsigned int flags) {
    bool on=(*value&flags)==flags;
    const bool mixed=!on && (*value&flags)!=0;
    if(mixed) ImGui::PushItemFlag(ImGuiItemFlags_MixedValue,true);
    const bool changed=Checkbox(label,&on);
    if(mixed) ImGui::PopItemFlag();
    if(!changed) return false;
    if(on) *value|=flags; else *value&=~flags;return true;
}
bool GlassSlider(const char* label,float* value,float low,float high,const char* format) {
    if(!value || !std::isfinite(low) || !std::isfinite(high) || low>high) {ImGui::TextDisabled("%s: invalid range",label);return false;}
    const float old=*value;
    if(std::isnan(*value)) *value=low;
    *value=std::clamp(*value,low,high);
    ImGui::PushID(label);const float s=Scale();const float width=std::max(1.0f,ImGui::GetContentRegionAvail().x);
    ImGui::TextWrapped("%s",label);const float h=28*s,r=std::min(9*s,width*.5f);
    const ImVec2 p=ImGui::GetCursorScreenPos();const ImGuiID id=ImGui::GetID("##slider");
    ImGui::InvisibleButton("##slider",ImVec2(width,h),ImGuiButtonFlags_EnableNav);
    const double span=double(high)-low;
    if(ImGui::IsItemActive() && ImGui::IsMouseDown(0))
        *value=float(low+span*std::clamp((ImGui::GetIO().MousePos.x-p.x-r)/std::max(1.0f,width-2*r),0.0f,1.0f));
    if(ImGui::IsItemFocused()) {
        if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) *value=float(std::max(double(low),double(*value)-span/100));
        if(ImGui::IsKeyPressed(ImGuiKey_RightArrow)) *value=float(std::min(double(high),double(*value)+span/100));
    }
    *value=std::clamp(*value,low,high);
    if(old!=*value) ImGui::MarkItemEdited(id);
    const bool released=ImGui::IsItemDeactivatedAfterEdit();
    const float t=span>0 ? float((double(*value)-low)/span) : 0,x=p.x+r+(width-2*r)*t,y=p.y+h*.5f;
    auto* dl=ImGui::GetWindowDrawList();const float alpha=ImGui::GetStyle().Alpha;
    dl->AddRectFilled(ImVec2(p.x,y-2*s),ImVec2(p.x+width,y+2*s),WithAlpha(ControlFill(),alpha),2*s);
    if(x>p.x) dl->AddRectFilled(ImVec2(p.x,y-2*s),ImVec2(x,y+2*s),WithAlpha(Accent(),alpha),2*s);
    const float grow=Animate(id,120,ImGui::IsItemActive() ? 1.12f : 1,1);
    dl->AddCircleFilled(ImVec2(x,y+1*s),r*grow,WithAlpha(IM_COL32(0,0,0,60),alpha),24);
    dl->AddCircleFilled(ImVec2(x,y),r*grow,WithAlpha(IM_COL32_WHITE,alpha),24);
    char text[64];snprintf(text,sizeof(text),format ? format : "%.2f",*value);ImGui::PushFont(FontSmall());ImGui::TextDisabled("%s",text);ImGui::PopFont();
    if(released) PlayUiSound(Sound::Click);
    ImGui::PopID();return old!=*value;
}
bool SearchField(const char* id,char* value,size_t capacity,const char* hint) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,10*Scale());
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed=ImGui::InputTextWithHint(id,hint,value,capacity);
    ImGui::PopStyleVar();return changed;
}
