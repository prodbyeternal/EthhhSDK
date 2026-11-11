#include "precomp.h"
#include "ui_audio.h"
#include "config.h"
#include <mmsystem.h>

namespace eth::ui {
namespace {
std::array<std::vector<unsigned char>,5> bank;
bool initialized=false;
ULONGLONG protectUntil=0,lastClick=0;
uint32_t U32(const unsigned char* b) {uint32_t v;memcpy(&v,b,4);return v;}
uint16_t U16(const unsigned char* b) {uint16_t v;memcpy(&v,b,2);return v;}
void Put(std::vector<unsigned char>& b,size_t at,uint32_t value,int size) {memcpy(b.data()+at,&value,size);}
// Keep the supplied originals intact. Convert their PCM24 payloads once in
// memory to ubiquitous PCM16 for PlaySound; no runtime file reads/decoding.
bool DecodeWave(const unsigned char* b,size_t size,std::vector<unsigned char>& out) {
    if(size<12 || memcmp(b,"RIFF",4) || memcmp(b+8,"WAVE",4)) return false;
    const unsigned char* fmt=nullptr,*data=nullptr;size_t bytes=0;
    for(size_t p=12;p+8<=size;) {
        const size_t n=U32(b+p+4); if(n>size-p-8) return false;
        if(!memcmp(b+p,"fmt ",4) && n>=16) fmt=b+p+8;
        if(!memcmp(b+p,"data",4)) {data=b+p+8;bytes=n;}
        p+=8+n+(n&1);
    }
    if(!fmt || !data || U16(fmt)!=1 || U16(fmt+14)!=24) return false;
    const uint16_t channels=U16(fmt+2);const uint32_t rate=U32(fmt+4);
    if(channels<1 || channels>2 || rate<8000 || rate>192000 || bytes%(channels*3)) return false;
    const size_t samples=bytes/3,payload=samples*2;out.assign(44+payload,0);
    memcpy(out.data(),"RIFF",4);Put(out,4,uint32_t(36+payload),4);memcpy(out.data()+8,"WAVEfmt ",8);
    Put(out,16,16,4);Put(out,20,1,2);Put(out,22,channels,2);Put(out,24,rate,4);
    Put(out,28,rate*channels*2,4);Put(out,32,channels*2,2);Put(out,34,16,2);
    memcpy(out.data()+36,"data",4);Put(out,40,uint32_t(payload),4);
    for(size_t i=0;i<samples;++i) {out[44+i*2]=data[i*3+1];out[45+i*2]=data[i*3+2];}
    return true;
}
void TemplateSound(Glass::Sfx s) {
    using S=Glass::Sfx;
    if(s==S::ToggleOn || s==S::CheckOn) PlayUiSound(Sound::On);
    else if(s==S::ToggleOff || s==S::CheckOff) PlayUiSound(Sound::Off);
    else if(s==S::Success) PlayUiSound(Sound::Welcome);
    else if(s!=S::SliderTick && s!=S::ScrubGrab && s!=S::SliderGrab) PlayUiSound(Sound::Click);
}
}
void InitUiAudio() {
    Glass::g_sfxHandler=TemplateSound;
    if(initialized) return;
    initialized=true;HMODULE module=nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&InitUiAudio),&module);
    for(int i=0;i<5;++i) {
        HRSRC r=FindResourceA(module,MAKEINTRESOURCEA(301+i),RT_RCDATA);
        if(!r) continue;
        const auto* p=static_cast<const unsigned char*>(LockResource(LoadResource(module,r)));
        if(p) DecodeWave(p,SizeofResource(module,r),bank[i]);
    }
}
void PlayUiSound(Sound sound) {
    if(!Config::Get().GetBool("theme","sounds",true)) return;
    const auto now=GetTickCount64();
    if(sound==Sound::Click && now-lastClick<75) return;
    if(sound!=Sound::Welcome && sound!=Sound::Start && now<protectUntil) return;
    InitUiAudio();const auto& wav=bank[size_t(sound)];if(wav.empty()) return;
    if(sound==Sound::Click) lastClick=now;
    if(sound==Sound::Welcome || sound==Sound::Start)
        protectUntil=now+ULONGLONG(1000.0*(wav.size()-44)/U32(wav.data()+28));
#if !defined(ETH_TESTING)
    PlaySoundA(reinterpret_cast<LPCSTR>(wav.data()),nullptr,SND_MEMORY|SND_ASYNC|SND_NODEFAULT);
#endif
}
void ShutdownUiAudio() {
#if !defined(ETH_TESTING)
    if(initialized) PlaySoundA(nullptr,nullptr,0);
#endif
    // Resource-derived memory remains alive across launcher -> game handoff.
    Glass::g_sfxHandler=nullptr;protectUntil=0;lastClick=0;
}
int UiAudioReadyCount() {int count=0;for(const auto& sound:bank) if(!sound.empty()) ++count;return count;}
}
