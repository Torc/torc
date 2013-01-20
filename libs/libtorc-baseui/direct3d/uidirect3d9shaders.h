#ifndef UIDIRECT3D9SHADERS_H
#define UIDIRECT3D9SHADERS_H

// Qt
#include <QSize>
#include <QHash>

// Torc
#include "torcbaseuiexport.h"
#include "d3dx9.h"
#include "uidirect3d9defs.h"

class TORC_BASEUI_PUBLIC D3D9Shader
{
  public:
    D3D9Shader(IDirect3DVertexShader9 *Vertex, IDirect3DPixelShader9 *Pixel,
               ID3DXConstantTable *VertexConstants, ID3DXConstantTable *PixelConstants);
    D3D9Shader();
    ~D3D9Shader();

    bool IsComplete(void);

    IDirect3DVertexShader9 *m_vertexShader;
    IDirect3DPixelShader9  *m_pixelShader;
    ID3DXConstantTable     *m_vertexConstants;
    ID3DXConstantTable     *m_pixelConstants;
};

class TORC_BASEUI_PUBLIC UIDirect3D9Shaders
{
  public:
    UIDirect3D9Shaders();
    virtual ~UIDirect3D9Shaders();

    bool          InitialiseShaders    (IDirect3DDevice9 *Device);
    bool          CreateDefaultShaders (IDirect3DDevice9 *Device);
    void          DeleteDefaultShaders (void);
    unsigned int  CreateShader         (IDirect3DDevice9 *Device,
                                        const char* Vertex, unsigned int VertexSize,
                                        const char* Pixel, unsigned int PixelSize);
    void          DeleteShader         (unsigned int Shader);
    void          DeleteShaders        (void);

  protected:
    bool                                  m_shadersValid;
    unsigned int                          m_defaultShader;
    IDirect3DVertexDeclaration9          *m_vertexDeclaration;
    QHash<unsigned int,D3D9Shader*>       m_shaders;

    TORC_D3DXCOMPILESHADER                m_D3DXCompileShader;
};

#endif // UIDIRECT3D9SHADERS_H
