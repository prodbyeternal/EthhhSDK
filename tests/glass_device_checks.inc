// Production renderer checks, not a mock device. State altered by both ImGui
// and the glass callbacks must survive byte-for-byte, including MRT/depth.
template<class Draw> void CheckGlassDeviceState(IDirect3DDevice9* d,Draw draw,int width,int height) {
    IDirect3DStateBlock9* baseline=nullptr;Hr(d->CreateStateBlock(D3DSBT_ALL,&baseline),"baseline state block");
    Hr(baseline->Capture(),"capture baseline");
    D3DVIEWPORT9 oldViewport{};Hr(d->GetViewport(&oldViewport),"baseline viewport");
    IDirect3DSurface9* oldDepth=nullptr;d->GetDepthStencilSurface(&oldDepth);
    IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;IDirect3DTexture9* texture=nullptr;
    IDirect3DSurface9* depth=nullptr;IDirect3DSurface9* mrt=nullptr;
    Hr(d->CreateVertexBuffer(128,0,D3DFVF_XYZRHW|D3DFVF_DIFFUSE,D3DPOOL_MANAGED,&vb,nullptr),"sentinel VB");
    Hr(d->CreateIndexBuffer(16,0,D3DFMT_INDEX16,D3DPOOL_MANAGED,&ib,nullptr),"sentinel IB");
    Hr(d->CreateTexture(4,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr),"sentinel texture");
    Hr(d->CreateDepthStencilSurface(width,height,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,FALSE,&depth,nullptr),"sentinel depth");
    D3DCAPS9 caps{};Hr(d->GetDeviceCaps(&caps),"test caps");
    if(caps.NumSimultaneousRTs>1) {
        Hr(d->CreateRenderTarget(width,height,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&mrt,nullptr),"sentinel MRT");
        Hr(d->SetRenderTarget(1,mrt),"bind sentinel MRT");
    }
    Hr(d->SetDepthStencilSurface(depth),"bind sentinel depth");
    Hr(d->SetStreamSource(0,vb,0,20),"sentinel stream");Hr(d->SetIndices(ib),"sentinel indices");
    Hr(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE),"sentinel FVF");
    Hr(d->SetTexture(0,texture),"sentinel texture slot zero");Hr(d->SetTexture(1,texture),"sentinel texture slot one");
    Hr(d->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP),"sentinel sampler");
    Hr(d->SetSamplerState(1,D3DSAMP_MAGFILTER,D3DTEXF_POINT),"sentinel second sampler");
    Hr(d->SetRenderState(D3DRS_COLORWRITEENABLE,3),"sentinel color mask");
    Hr(d->SetRenderState(D3DRS_SRGBWRITEENABLE,TRUE),"sentinel sRGB");
    Hr(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW),"sentinel culling");
    const RECT clip{7,9,width-11,height-13};Hr(d->SetScissorRect(&clip),"sentinel scissor");
    const D3DVIEWPORT9 viewport{3,4,UINT(width-10),UINT(height-12),.2f,.8f};Hr(d->SetViewport(&viewport),"sentinel viewport");
    const float constants[4]={.123f,.456f,.789f,.314f};Hr(d->SetPixelShaderConstantF(0,constants,1),"sentinel pixel constants");
    const float constants1[4]={.271f,.618f,.141f,.421f};Hr(d->SetVertexShaderConstantF(9,constants1,1),"sentinel vertex constants");
    D3DMATRIX world{};for(int i=0;i<4;++i) world.m[i][i]=float(i+2);Hr(d->SetTransform(D3DTS_WORLD,&world),"sentinel transform");
    draw();
    auto state=[&](D3DRENDERSTATETYPE which,DWORD expected) {DWORD value=0;Hr(d->GetRenderState(which,&value),"read state");Check(value==expected,"glass restores every modified render state");};
    state(D3DRS_COLORWRITEENABLE,3);state(D3DRS_SRGBWRITEENABLE,TRUE);state(D3DRS_CULLMODE,D3DCULL_CW);
    DWORD value=0;Hr(d->GetSamplerState(0,D3DSAMP_ADDRESSU,&value),"read sampler");Check(value==D3DTADDRESS_WRAP,"sampler U restored");
    Hr(d->GetSamplerState(1,D3DSAMP_MAGFILTER,&value),"read second sampler");Check(value==D3DTEXF_POINT,"sampler 1 restored");
    Hr(d->GetFVF(&value),"read FVF");Check(value==(D3DFVF_XYZRHW|D3DFVF_DIFFUSE),"FVF restored after ordered callbacks");
    IDirect3DVertexBuffer9* gotVB=nullptr;UINT offset=0,stride=0;Hr(d->GetStreamSource(0,&gotVB,&offset,&stride),"read stream");
    Check(gotVB==vb && offset==0 && stride==20,"DrawPrimitiveUP restores stream zero");if(gotVB) gotVB->Release();
    IDirect3DIndexBuffer9* gotIB=nullptr;Hr(d->GetIndices(&gotIB),"read indices");Check(gotIB==ib,"index binding restored");if(gotIB) gotIB->Release();
    for(int i=0;i<2;++i) {IDirect3DBaseTexture9* got=nullptr;Hr(d->GetTexture(i,&got),"read texture");Check(got==texture,"texture bindings restored");if(got) got->Release();}
    D3DVIEWPORT9 gotViewport{};Hr(d->GetViewport(&gotViewport),"read viewport");Check(!memcmp(&viewport,&gotViewport,sizeof(viewport)),"viewport restored including depth range");
    RECT gotClip{};Hr(d->GetScissorRect(&gotClip),"read clip");Check(EqualRect(&clip,&gotClip)!=0,"scissor restored");
    float got[4]{};Hr(d->GetPixelShaderConstantF(0,got,1),"read pixel constants");Check(!memcmp(got,constants,sizeof(got)),"pixel constants restored");
    Hr(d->GetVertexShaderConstantF(9,got,1),"read vertex constants");Check(!memcmp(got,constants1,sizeof(got)),"vertex constants restored");
    D3DMATRIX gotWorld{};Hr(d->GetTransform(D3DTS_WORLD,&gotWorld),"read world");Check(!memcmp(&world,&gotWorld,sizeof(world)),"world transform restored");
    IDirect3DSurface9* gotSurface=nullptr;Hr(d->GetDepthStencilSurface(&gotSurface),"read depth");Check(gotSurface==depth,"depth restored");if(gotSurface) gotSurface->Release();
    if(mrt) {Hr(d->GetRenderTarget(1,&gotSurface),"read MRT");Check(gotSurface==mrt,"additional render target restored");if(gotSurface) gotSurface->Release();d->SetRenderTarget(1,nullptr);}
    d->SetDepthStencilSurface(oldDepth);if(oldDepth) oldDepth->Release();
    baseline->Apply();baseline->Release();d->SetViewport(&oldViewport);
    if(mrt) mrt->Release();depth->Release();vb->Release();ib->Release();texture->Release();
    printf("PASS actual DX9 state restoration (MRT/depth/FVF/streams/textures/samplers/constants/transform/viewport)\n");
}
