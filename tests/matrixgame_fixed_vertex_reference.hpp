// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
static void RunFixedVertexChecks(){
    unsigned cases=0,gl_cases=0,d3d_cases=0;
    for(unsigned fvf:{0x112u,0x212u,0x352u,0x3d2u,0x10112u,0x144u})
    for(bool lighting:{false,true})for(bool normalize:{false,true})for(unsigned tci:{0u,0x10000u,0x20000u,0x30000u}){
        if((fvf&0xe)==4 && tci)continue;
        mat::FixedVertexState state;state.Layout(fvf);state.Render(137,lighting);state.Render(143,normalize);state.Render(139,0x00335577);
        auto world=mat::Identity();world[0]=1.2f;world[5]=.8f;world[10]=1.4f;world[12]=.3f;world[13]=-.2f;world[14]=1;
        auto view=mat::Identity();view[0]=0;view[1]=-1;view[4]=1;view[5]=0;view[14]=3;
        const mat::Matrix projection{1.3f,0,0,0,0,1.7f,0,0,0,0,1.01f,1,0,0,-.101f,0};
        state.Transform(256,world);state.Transform(2,view);state.Transform(3,projection);state.Viewport({0,0,320,240,0,1});
        rp3g::VideoMaterial material{};material.Diffuse={.7f,.8f,.9f,.6f};material.Ambient={.25f,.35f,.45f,.9f};material.Emissive={.02f,.03f,.04f,.7f};state.Material(material);
        rp3g::VideoLight light{};light.Type=3;light.Direction={.3f,-.7f,-.6f};light.Diffuse={.4f,.6f,.2f,0};light.Ambient={.03f,.01f,.02f,0};state.Light(0,light);state.EnableLight(0,true);
        state.Stage(0,11,tci);state.Stage(0,24,2);auto texture=mat::Identity();texture[8]=.25f;texture[9]=-.125f;state.Transform(16,texture);
        std::vector<std::byte> bytes;auto append=[&](const void* p,std::size_t n){auto first=static_cast<const std::byte*>(p);bytes.insert(bytes.end(),first,first+n);};
        const float position[]{.3f,.2f,.7f,.5f},normal[]{.2f,.3f,.9f};
        append(position,((fvf&0xe)==4?4:3)*4);if(fvf&0x10)append(normal,12);
        const unsigned dc=0xb14b87d3,sc=0x63d12b79;if(fvf&0x40)append(&dc,4);if(fvf&0x80)append(&sc,4);
        const auto tex_count=(fvf>>8)&15;
        for(unsigned i=0;i<tex_count;++i){const unsigned encoded=(fvf>>(16+i*2))&3;const unsigned dimension=encoded==0?2:encoded==1?3:encoded==2?4:1;const float coords[]{.125f*(i+1),.25f,.5f,.75f};append(coords,dimension*4);}
        assert(bytes.size()==state.Stride());const auto vertex=state.Decode(bytes,state.Stride()).at(0);const auto uniforms=state.Prepare();
        const auto expected=mat::EvaluateVertex(uniforms,vertex);++cases;
        for(float x:expected.clip)assert(std::isfinite(x));
#ifdef _WIN32
        if(gl_reference){const auto got=gl_reference->Transform(uniforms,vertex);++gl_cases;
            std::array<float,24> a,b;std::memcpy(a.data(),&expected,sizeof(expected));std::memcpy(b.data(),&got,sizeof(got));
            for(unsigned i=0;i<24;++i)if(std::abs(a[i]-b[i])>1e-4f){std::printf("fixed vertex GLSL mismatch fvf=%x lighting=%u norm=%u tci=%x component=%u cpu=%f gl=%f\n",fvf,lighting,normalize,tci,i,a[i],b[i]);throw std::runtime_error("fixed vertex GLSL parity");}}
        if((fvf&0xe)!=4){
            auto* d=reference->device.p;
            for(auto pair:{std::pair{D3DTS_WORLD,world},std::pair{D3DTS_VIEW,view},std::pair{D3DTS_PROJECTION,projection},std::pair{D3DTS_TEXTURE0,texture}}){D3DMATRIX m;std::memcpy(&m,pair.second.data(),64);HR(d->SetTransform(pair.first,&m));}
            const D3DVIEWPORT9 viewport{0,0,320,240,0,1};HR(d->SetViewport(&viewport));
            D3DMATERIAL9 m{};std::memcpy(&m.Diffuse,&material.Diffuse,16);std::memcpy(&m.Ambient,&material.Ambient,16);std::memcpy(&m.Emissive,&material.Emissive,16);HR(d->SetMaterial(&m));
            D3DLIGHT9 l{};l.Type=D3DLIGHT_DIRECTIONAL;std::memcpy(&l.Direction,&light.Direction,12);std::memcpy(&l.Diffuse,&light.Diffuse,16);std::memcpy(&l.Ambient,&light.Ambient,16);HR(d->SetLight(0,&l));HR(d->LightEnable(0,TRUE));
            HR(d->SetRenderState(D3DRS_LIGHTING,lighting));HR(d->SetRenderState(D3DRS_NORMALIZENORMALS,normalize));HR(d->SetRenderState(D3DRS_AMBIENT,0x00335577));
            HR(d->SetRenderState(D3DRS_COLORVERTEX,TRUE));HR(d->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE,D3DMCS_COLOR1));HR(d->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE,D3DMCS_MATERIAL));HR(d->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE,D3DMCS_MATERIAL));HR(d->SetRenderState(D3DRS_SPECULARENABLE,FALSE));
            for(unsigned i=0;i<8;++i){HR(d->SetTexture(i,reference->textures[0]));HR(d->SetTextureStageState(i,D3DTSS_COLOROP,D3DTOP_SELECTARG1));HR(d->SetTextureStageState(i,D3DTSS_COLORARG1,D3DTA_TEXTURE));HR(d->SetTextureStageState(i,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1));HR(d->SetTextureStageState(i,D3DTSS_ALPHAARG1,D3DTA_CURRENT));HR(d->SetTextureStageState(i,D3DTSS_TEXCOORDINDEX,uniforms.coordinates[i][0]));HR(d->SetTextureStageState(i,D3DTSS_TEXTURETRANSFORMFLAGS,uniforms.coordinates[i][1]));}
            Owned<IDirect3DVertexBuffer9> source,destination;
            HR(d->CreateVertexBuffer(unsigned(bytes.size()),0,fvf,D3DPOOL_SYSTEMMEM,source.Put(),nullptr));void* mapped;HR(source.p->Lock(0,0,&mapped,0));std::memcpy(mapped,bytes.data(),bytes.size());HR(source.p->Unlock());
            HR(d->CreateVertexBuffer(84,0,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX8,D3DPOOL_SYSTEMMEM,destination.Put(),nullptr));
            HR(d->SetFVF(fvf));HR(d->SetStreamSource(0,source.p,0,state.Stride()));HR(d->ProcessVertices(0,0,1,destination.p,nullptr,0));
            HR(destination.p->Lock(0,0,&mapped,D3DLOCK_READONLY));std::array<std::byte,84> output;std::memcpy(output.data(),mapped,84);HR(destination.p->Unlock());
            std::array<float,4> screen;std::memcpy(screen.data(),output.data(),16);unsigned color;std::memcpy(&color,output.data()+16,4);const auto actual_color=Unpack(color);
            const std::array<float,4> expected_screen{(expected.clip[0]/expected.clip[3]+1)*160,(1-expected.clip[1]/expected.clip[3])*120,expected.clip[2]/expected.clip[3],1/expected.clip[3]};
            for(unsigned c=0;c<4;++c){if(std::abs(screen[c]-expected_screen[c])>1e-3f || std::abs(actual_color[c]-expected.diffuse[c])*255>1.1f){std::printf("D3D vertex mismatch fvf=%x light=%u norm=%u tci=%x component=%u screen=%f/%f color=%f/%f\n",fvf,lighting,normalize,tci,c,expected_screen[c],screen[c],expected.diffuse[c],actual_color[c]);throw std::runtime_error("D3D vertex transform/light parity");}}
            // ProcessVertices exposes transformed UVs without texture sampling.
            for(unsigned s=0;s<tex_count;++s)for(unsigned c=0;c<2;++c){float actual;std::memcpy(&actual,output.data()+20+s*8+c*4,4);if(std::abs(actual-expected.uv[s][c])>1e-4f){std::printf("D3D TCI mismatch fvf=%x light=%u norm=%u tci=%x stage=%u component=%u expected=%f actual=%f\n",fvf,lighting,normalize,tci,s,c,expected.uv[s][c],actual);throw std::runtime_error("D3D TCI parity");}}
            HR(d->SetStreamSource(0,nullptr,0,0));++d3d_cases;
        }
#endif
    }
    std::printf("fixed vertex cases=%u GLSL_feedback=%u D3D9_ProcessVertices=%u; Switch GPU NOT RUN\n",cases,gl_cases,d3d_cases);
}
