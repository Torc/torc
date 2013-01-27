/* Class UIDirect3D9Shaders
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QLibrary>

// Torc
#include "torclogging.h"
#include "uidirect3d9window.h"
#include "uidirect3d9shaders.h"

static const char DefaultVertexShader[] =
"struct Input\n"
"{\n"
"    float3 Position : POSITION0;\n"
"    float2 TexCoord : TEXCOORD0;\n"
"};\n"
"struct Output\n"
"{\n"
"    float4 Position : POSITION0;\n"
"    float2 TexCoord : TEXCOORD0;\n"
"};\n"
"\n"
"float4x4 Projection;\n"
"float4x4 Transform;\n"
"\n"
"Output main(Input In)\n"
"{\n"
"    Output Out;\n"
"    float4 world = mul(float4(In.Position, 1.0), Transform);\n"
"    Out.Position = mul(world, Projection);\n"
"    Out.TexCoord = In.TexCoord;\n"
"    return Out;\n"
"};\n";

static const char DefaultPixelShader[] =
"struct Output\n"
"{\n"
"    float4 Position : POSITION0;\n"
"    float2 TexCoord : TEXCOORD0;\n"
"};\n"
"sampler Sampler;\n"
"float4  Color;\n"
"float4 main(Output In): COLOR0\n"
"{\n"
"    float4 color = tex2D(Sampler, In.TexCoord);\n"
"    float  grey  = dot(color.rgb, float3(0.299, 0.587, 0.114));\n"
"    return (lerp(float4(grey, grey, grey, color.a), color, Color.r) * float4(Color.g, Color.g, Color.g, Color.a)) + float4(Color.b, Color.b, Color.b, 0.0);\n"
"}\n";

static D3DVERTEXELEMENT9 vertexdeclaration[] =
{
    { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};

D3D9Shader::D3D9Shader(IDirect3DVertexShader9 *Vertex, IDirect3DPixelShader9 *Pixel,
                       ID3DXConstantTable *VertexConstants, ID3DXConstantTable *PixelConstants)
  : m_vertexShader(Vertex),
    m_pixelShader(Pixel),
    m_vertexConstants(VertexConstants),
    m_pixelConstants(PixelConstants)
{
}

D3D9Shader::D3D9Shader()
  : m_vertexShader(NULL),
    m_pixelShader(NULL),
    m_vertexConstants(NULL),
    m_pixelConstants(NULL)
{
}

D3D9Shader::~D3D9Shader()
{
    if (m_vertexShader)
        m_vertexShader->Release();

    if (m_pixelShader)
        m_pixelShader->Release();

    if (m_vertexConstants)
        m_vertexConstants->Release();

    if (m_pixelConstants)
        m_pixelConstants->Release();
}

bool D3D9Shader::IsComplete(void)
{
    return m_vertexConstants && m_vertexShader &&
           m_pixelConstants  && m_pixelShader;
}

UIDirect3D9Shaders::UIDirect3D9Shaders()
  : m_shadersValid(false),
    m_defaultShader(NULL),
    m_vertexDeclaration(NULL),
    m_D3DXCompileShader(NULL)
{
}

UIDirect3D9Shaders::~UIDirect3D9Shaders()
{
    DeleteDefaultShaders();
    DeleteShaders();
}

bool UIDirect3D9Shaders::InitialiseShaders(IDirect3DDevice9 *Device)
{
    HRESULT ok = Device->CreateVertexDeclaration(vertexdeclaration, &m_vertexDeclaration);
    if (D3D_OK != ok)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to create vertex declaration");
        if (m_vertexDeclaration)
            m_vertexDeclaration->Release();
        m_vertexDeclaration = NULL;
    }

    Device->SetVertexDeclaration(m_vertexDeclaration);

    m_D3DXCompileShader  = (TORC_D3DXCOMPILESHADER)UIDirect3D9Window::GetD3DX9ProcAddress("D3DXCompileShader");

    m_shadersValid = m_D3DXCompileShader && m_vertexDeclaration;

    if (m_shadersValid)
        return CreateDefaultShaders(Device);

    return false;
}

bool UIDirect3D9Shaders::CreateDefaultShaders(IDirect3DDevice9 *Device)
{
    unsigned int vertexsize = sizeof(DefaultVertexShader) / sizeof(char);
    unsigned int pixelsize  = sizeof(DefaultPixelShader) / sizeof(char);
    m_defaultShader = CreateShader(Device, DefaultVertexShader, vertexsize,
                                   DefaultPixelShader, pixelsize);

    if (m_defaultShader)
    {
        LOG(VB_GENERAL, LOG_INFO, "Created default shader");
        return true;
    }

    return false;
}

void UIDirect3D9Shaders::DeleteDefaultShaders(void)
{
    DeleteShader(m_defaultShader);
    m_defaultShader = 0;
}

D3D9Shader* UIDirect3D9Shaders::CreateShader(IDirect3DDevice9 *Device,
                                             const char *Vertex, unsigned int VertexSize,
                                             const char *Pixel, unsigned int PixelSize)
{
    if (!m_shadersValid)
        return NULL;

    bool errored = false;
    D3D9Shader *shader = new D3D9Shader();

    // compile and create the vertex shader
    ID3DXBuffer *vertexbuffer = NULL;
    ID3DXBuffer *vertexerror  = NULL;

    HRESULT ok = m_D3DXCompileShader(Vertex, VertexSize, NULL, NULL, "main", "vs_2_0",
                                     0, &vertexbuffer, &vertexerror, &shader->m_vertexConstants);

    if (vertexerror)
    {
        errored |= true;
        LOG(VB_GENERAL, LOG_ERR, QString("%1").arg((const char*)vertexerror->GetBufferPointer()));
        vertexerror->Release();
    }

    if (D3D_OK != ok)
    {
        errored |= true;
        LOG(VB_GENERAL, LOG_ERR, "Failed to compile vertex shader");
    }

    if (!errored && vertexbuffer)
        ok = Device->CreateVertexShader((const DWORD*)vertexbuffer->GetBufferPointer(), &shader->m_vertexShader);

    if (vertexbuffer)
        vertexbuffer->Release();

    if (D3D_OK != ok)
    {
        errored |= true;
        LOG(VB_GENERAL, LOG_ERR, "Failed to create vertex shader");
    }

    // compile and create the pixel shader
    ID3DXBuffer *pixelbuffer = NULL;
    ID3DXBuffer *pixelerror  = NULL;

    ok = m_D3DXCompileShader(Pixel, PixelSize, NULL, NULL, "main", "ps_2_a",
                             0, &pixelbuffer, &pixelerror, &shader->m_pixelConstants);

    if (pixelerror)
    {
        errored |= true;
        LOG(VB_GENERAL, LOG_ERR, QString("%1").arg((const char*)pixelerror->GetBufferPointer()));
        pixelerror->Release();
    }

    if (D3D_OK != ok)
    {
        errored |= true;
        LOG(VB_GENERAL, LOG_ERR, "Failed to compile pixel shader");
    }

    if (!errored && pixelbuffer)
        ok = Device->CreatePixelShader((const DWORD*)pixelbuffer->GetBufferPointer(), &shader->m_pixelShader);

    if (pixelbuffer)
        pixelbuffer->Release();

    if (D3D_OK != ok)
    {
        errored |= true;
        LOG(VB_GENERAL, LOG_ERR, "Failed to create pixel shader");
    }

    // if everything checks out, add it to our map
    if (!errored && shader->IsComplete())
    {
        m_shaders.append(shader);
        return shader;
    }

    delete shader;
    return NULL;
}

void UIDirect3D9Shaders::DeleteShader(D3D9Shader *Shader)
{
    if (Shader && m_shaders.removeAll(Shader))
        delete Shader;
}

void UIDirect3D9Shaders::DeleteShaders(void)
{
    foreach (D3D9Shader* shader, m_shaders)
        delete shader;
    m_shaders.clear();
}
