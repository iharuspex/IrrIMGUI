/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Andre Netzeband and Omar Cornut
 *
 * The original OpenGL implementation where this driver is based on was implemented
 * by Omar Cornut as an example how to use the IMGUI with OpenGL.
 * You can find the IMGUI here: https://github.com/ocornut/imgui
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/**
 * @file       COpenGLIMGUIDriver.cpp
 * @author     Andre Netzeband
 * @brief      Contains a driver that uses native OpenGL functions to render the GUI.
 * @attention  This driver is just a test- and fallback implementation. It is not officially supported by the Irrlicht IMGUI binding.
 * @addtogroup IrrIMGUIPrivateDriver
 */

// library includes
#include <windows.h>
#include <GL/GL.h>

// module includes
#include "COpenGLIMGUIDriver.h"
#include "IrrIMGUIDebug_priv.h"

namespace IrrIMGUI
{
namespace Private
{
namespace Driver
{

  /// @brief Helper functions for OpenGL
  namespace OpenGLHelper
  {
    /// @return Returns the value of an OpenGL Enum Value
    /// @param Which is the enum where we want to know the value.
    GLenum getGlEnum(GLenum const Which);

    /// @brief Restores an OpenGL Bit
    /// @param WhichBit is the bit to restore.
    /// @param Value must be true or false, whether it was set or cleared.
    void restoreGLBit(GLenum const WhichBit, bool const Value);

    /// @brief Helper Class to store and restore the OpenGL state.
    class COpenGLState
    {
      public:
        /// @brief The Constructor stored the OpenGL state.
        COpenGLState(void);

        /// @brief The Destructor restores the state.
        ~COpenGLState(void);

      private:
        GLint  mOldTexture;
    };
  }

  COpenGLIMGUIDriver::COpenGLIMGUIDriver(irr::IrrlichtDevice * const pDevice):
      IIMGUIDriver(pDevice)
  {
    setupFunctionPointer();

    return;
  }

  COpenGLIMGUIDriver::~COpenGLIMGUIDriver(void)
  {
    deleteFontTexture(ImGui::GetIO().Fonts->TexID);
    return;
  }

  void COpenGLIMGUIDriver::setupFunctionPointer(void)
  {
    ImGuiIO &rGUIIO  = ImGui::GetIO();

    rGUIIO.RenderDrawListsFn = COpenGLIMGUIDriver::drawGUIList;
    //rGUIIO.SetClipboardTextFn = TODO;
    //rGUIIO.GetClipboardTextFn = TODO;

#ifdef _WIN32
    irr::video::IVideoDriver * const pDriver = getIrrDevice()->getVideoDriver();
    irr::video::SExposedVideoData const &rExposedVideoData = pDriver->getExposedVideoData();

    rGUIIO.ImeWindowHandle = reinterpret_cast<HWND>(rExposedVideoData.OpenGLWin32.HWnd);

#else
#warning "Maybe for Linux you have to pass a X11 Window handle (rExposedVideoData.OpenGLLinux.X11Window)?"
#endif

    return;
  }

  void COpenGLIMGUIDriver::drawCommandList(ImDrawList * const pCommandList)
  {
    ImGuiIO& rGUIIO = ImGui::GetIO();
    float const FrameBufferHeight = rGUIIO.DisplaySize.y * rGUIIO.DisplayFramebufferScale.y;

    #define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))

    ImDrawVert * const pVertexBuffer = &(pCommandList->VtxBuffer.front());
    ImDrawIdx  * const pIndexBuffer  = &(pCommandList->IdxBuffer.front());
    int FirstIndexElement = 0;

    glVertexPointer(  2, GL_FLOAT,         sizeof(ImDrawVert), (void*)(((irr::u8*)pVertexBuffer) + OFFSETOF(ImDrawVert, pos)));
    glTexCoordPointer(2, GL_FLOAT,         sizeof(ImDrawVert), (void*)(((irr::u8*)pVertexBuffer) + OFFSETOF(ImDrawVert, uv)));
    glColorPointer(   4, GL_UNSIGNED_BYTE, sizeof(ImDrawVert), (void*)(((irr::u8*)pVertexBuffer) + OFFSETOF(ImDrawVert, col)));

    for (int CommandIndex = 0; CommandIndex < pCommandList->CmdBuffer.size(); CommandIndex++)
    {

      ImDrawCmd * const pCommand = &pCommandList->CmdBuffer[CommandIndex];

      if (pCommand->UserCallback)
      {
        pCommand->UserCallback(pCommandList, pCommand);
      }
      else
      {
        glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pCommand->TextureId);
        glScissor((int)pCommand->ClipRect.x, (int)(FrameBufferHeight - pCommand->ClipRect.w), (int)(pCommand->ClipRect.z - pCommand->ClipRect.x), (int)(pCommand->ClipRect.w - pCommand->ClipRect.y));
        glDrawElements(GL_TRIANGLES, (GLsizei)pCommand->ElemCount, GL_UNSIGNED_SHORT, &(pIndexBuffer[FirstIndexElement]));
      }

      FirstIndexElement += pCommand->ElemCount;
    }

    return;
  }

  void COpenGLIMGUIDriver::drawGUIList(ImDrawData * const pDrawData)
  {
    OpenGLHelper::COpenGLState OpenGLState;

    // setup OpenGL states
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnable(GL_TEXTURE_2D);

    // calculate framebuffe scales
    ImGuiIO& rGUIIO = ImGui::GetIO();
    pDrawData->ScaleClipRects(rGUIIO.DisplayFramebufferScale);

    // setup orthographic projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, rGUIIO.DisplaySize.x, rGUIIO.DisplaySize.y, 0.0f, -1.0f, +1.0f);

    // setup model view matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    for (int CommandListIndex = 0; CommandListIndex < pDrawData->CmdListsCount; CommandListIndex++)
    {
      drawCommandList(pDrawData->CmdLists[CommandListIndex]);
    }

    // restore modified state
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    return;
  }

  void COpenGLIMGUIDriver::deleteFontTexture(void * pTextureID)
  {
    LOG_NOTE("{IMGUI-GL} Delete old Font Texture with handle 0x" << std::hex << pTextureID << "\n");

    GLuint Texture = static_cast<GLuint>(reinterpret_cast<intptr_t>(pTextureID));
    glDeleteTextures(1, &Texture);

    return;
  }

  void * COpenGLIMGUIDriver::createFontTextureWithHandle(void)
  {
    ImGuiIO &rGUIIO  = ImGui::GetIO();

    // Get Font Texture from IMGUI system.
    unsigned char* PixelData;
    int Width, Height;
    rGUIIO.Fonts->GetTexDataAsAlpha8(&PixelData, &Width, &Height);

    // Store current Texture handle
    GLint OldTextureID;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &OldTextureID);

    // Create new texture for fonts
    GLuint NewTextureID;
    glGenTextures(1, &NewTextureID);
    glBindTexture(GL_TEXTURE_2D, NewTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, Width, Height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, PixelData);

    // Store Texture information in IMGUI system
    void * pFontTexture = reinterpret_cast<void *>(static_cast<intptr_t>(NewTextureID));
    rGUIIO.Fonts->TexID = pFontTexture;
    rGUIIO.Fonts->ClearTexData();

    // Reset Texture handle
    glBindTexture(GL_TEXTURE_2D, OldTextureID);

    LOG_NOTE("{IMGUI-GL} Created a new Font Texture with handle 0x" << std::hex << pFontTexture << "\n");

    return pFontTexture;
  }

  ImTextureID COpenGLIMGUIDriver::createTextureFromImageInternal(irr::video::IImage * const pImage)
  {
    // Convert pImage to RGBA
    int const Width  = pImage->getDimension().Width;
    int const Height = pImage->getDimension().Height;
    irr::u32 * const pImageData = new irr::u32[Width * Height];

    for (int X = 0; X < Width; X++)
    {
      for (int Y = 0; Y < Height; Y++)
      {
        irr::video::SColor const PixelColor = pImage->getPixel(X, Y);
        irr::u8 * const pPixelPointer = (irr::u8 *)(&pImageData[X + Y * Width]);
        PixelColor.toOpenGLColor(pPixelPointer);
      }
    }

    // Store current Texture handle
    GLint OldTextureID;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &OldTextureID);

    // Create new texture for image
    GLuint NewTextureID;
    glGenTextures(1, &NewTextureID);
    glBindTexture(GL_TEXTURE_2D, NewTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pImageData);

    void * pTexture = reinterpret_cast<void *>(static_cast<intptr_t>(NewTextureID));
    delete[] pImageData;

    // Reset Texture handle
    glBindTexture(GL_TEXTURE_2D, OldTextureID);

    LOG_NOTE("{IMGUI-GL} Created a new Image Texture with handle 0x" << std::hex << pTexture << "\n");

    return pTexture;
  }

  void COpenGLIMGUIDriver::deleteTextureInternal(ImTextureID const Texture)
  {
    LOG_NOTE("{IMGUI-GL} Delete Image Texture with handle 0x" << std::hex << Texture << "\n");

    GLuint TextureID = static_cast<GLuint>(reinterpret_cast<intptr_t>(Texture));
    glDeleteTextures(1, &TextureID);

    return;
  }

namespace OpenGLHelper
{
  GLenum getGlEnum(GLenum const Which)
  {
    GLint Vector[30];
    glGetIntegerv(Which, Vector);
    return Vector[0];
  }

  void restoreGLBit(GLenum const WhichBit, bool const Value)
  {
    if (Value)
    {
      glEnable(WhichBit);
    }
    else
    {
      glDisable(WhichBit);
    }
  }

  COpenGLState::COpenGLState(void)
  {
    // store current texture
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &mOldTexture);

    // store other settings
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT);

    // store projection matrix
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();

    // store view matrix
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    return;
  }

  COpenGLState::~COpenGLState(void)
  {
    // restore view matrix
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // restore projection matrix
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    // restore other settings
    glPopAttrib();

    // restore texture
    glBindTexture(GL_TEXTURE_2D, mOldTexture);

    return;
  }

}

}
}
}


/* Pure Irrlicht implementation:

  irr::u32 * pImageData = new irr::u32[Width * Height];

  for (int X = 0; X < (Width * Height); X++)
  {
    // set only Alpha
    irr::video::SColor Color(PixelData[X], 255, 255, 255);
    Color.getData(&pImageData[X], irr::video::ECF_A8R8G8B8);
  }
  pIrrDriver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, false);

  irr::core::dimension2d<irr::u32> const Size(Width, Height);
  irr::video::IImage * const pImage = pIrrDriver->createImageFromData(irr::video::ECF_A8R8G8B8, Size, pImageData);
  pTexture = pIrrDriver->addTexture("DefaultIMGUITexture", pImage);

  if (rGUIIO.RenderDrawListsFn == COpenGLIMGUIDriver::drawGUIList)
  {
    // Store Texture information in IMGUI system
    rGUIIO.Fonts->TexID = pTexture;
    rGUIIO.Fonts->ClearInputData();
    rGUIIO.Fonts->ClearTexData();
  }

  delete[] pImageData;
  */

#if 0 // Pure irrlicht implementation
void COpenGLIMGUIDriver::drawCommandList(ImDrawList * const pCommandList)
{

  /*
  const ImDrawList* cmd_list = draw_data->CmdLists[n];
  const unsigned char* vtx_buffer = (const unsigned char*)&cmd_list->VtxBuffer.front();
  const ImDrawIdx* idx_buffer = &cmd_list->IdxBuffer.front();
  glVertexPointer(2, GL_FLOAT, sizeof(ImDrawVert), (void*)(vtx_buffer + OFFSETOF(ImDrawVert, pos)));
  glTexCoordPointer(2, GL_FLOAT, sizeof(ImDrawVert), (void*)(vtx_buffer + OFFSETOF(ImDrawVert, uv)));
  glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ImDrawVert), (void*)(vtx_buffer + OFFSETOF(ImDrawVert, col)));

  for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++)
  {
      const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
      if (pcmd->UserCallback)
      {
          pcmd->UserCallback(cmd_list, pcmd);
      }
      else
      {
          glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
          glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
          glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, GL_UNSIGNED_SHORT, idx_buffer);
      }
      idx_buffer += pcmd->ElemCount;
  }
  */

  //ImDrawVert Vertex;

  irr::u16 * const pIndices = &(pCommandList->IdxBuffer.front());

  irr::u32 LastIndex = 0;
  irr::u32 NumberOfVertex = pCommandList->VtxBuffer.size();
  irr::video::S3DVertex * const pVertexArray = new irr::video::S3DVertex[NumberOfVertex];

  /*
  pIrrDriver->enableMaterial2D(true);
  pIrrDriver->getMaterial2D().TextureLayer[0].BilinearFilter=false;
  pIrrDriver->getMaterial2D().TextureLayer[0].AnisotropicFilter=false;
  pIrrDriver->getMaterial2D().TextureLayer[0].TrilinearFilter=false;
  pIrrDriver->getMaterial2D().TextureLayer[0].LODBias=-4;
  pIrrDriver->getMaterial2D().AntiAliasing=irr::video::EAAM_OFF;
  */

  for (int i = 0; i < NumberOfVertex; i++)
  {
    ImDrawVert &rImGUIVertex = pCommandList->VtxBuffer[i];

    pVertexArray[i].Pos     = irr::core::vector3df(static_cast<irr::f32>(rImGUIVertex.pos.x), static_cast<irr::f32>(rImGUIVertex.pos.y), 0.0) ;
    pVertexArray[i].Normal  = irr::core::vector3df(0.0, 0.0, 1.0);
    irr::u8 const Red   = (rImGUIVertex.col >>  0) & 0xFF;
    irr::u8 const Green = (rImGUIVertex.col >>  8) & 0xFF;
    irr::u8 const Blue  = (rImGUIVertex.col >> 16) & 0xFF;
    irr::u8 const Alpha = (rImGUIVertex.col >> 24) & 0xFF;
    pVertexArray[i].Color   = irr::video::SColor(Alpha, Red, Green, Blue);
    pVertexArray[i].TCoords = irr::core::vector2df(static_cast<irr::f32>(rImGUIVertex.uv.x), static_cast<irr::f32>(rImGUIVertex.uv.y));
  }

  pIrrDriver->enableMaterial2D(false);

  for (int CommandIndex = 0; CommandIndex < pCommandList->CmdBuffer.size(); CommandIndex++)
  {
    ImDrawCmd * const pDrawCommand = &pCommandList->CmdBuffer[CommandIndex];

    // TODO: implement user callback function!

    auto pTexture = static_cast<irr::video::ITexture*>(pDrawCommand->TextureId);
    irr::video::SMaterial Material;

    Material.MaterialType = irr::video::EMT_ONETEXTURE_BLEND;
    Material.MaterialTypeParam = irr::video::pack_textureBlendFunc(irr::video::EBF_SRC_ALPHA, irr::video::EBF_ONE_MINUS_SRC_ALPHA, irr::video::EMFN_MODULATE_1X, irr::video::EAS_VERTEX_COLOR | irr::video::EAS_TEXTURE);
    Material.setTexture(0, pTexture);
    Material.UseMipMaps = false;
    Material.Thickness = 0.0;

    /*
    pIrrDriver->getMaterial2D().MaterialType = irr::video::EMT_ONETEXTURE_BLEND;
    pIrrDriver->getMaterial2D().MaterialTypeParam = irr::video::pack_textureBlendFunc(irr::video::EBF_SRC_ALPHA, irr::video::EBF_ONE_MINUS_SRC_ALPHA, irr::video::EMFN_MODULATE_1X, irr::video::EAS_VERTEX_COLOR | irr::video::EAS_TEXTURE);
    pIrrDriver->getMaterial2D().setTexture(0, pTexture);
    pIrrDriver->getMaterial2D().Thickness = 0.0;
    */

    Material.setFlag(irr::video::EMF_ANTI_ALIASING, false);
    Material.setFlag(irr::video::EMF_BILINEAR_FILTER, false);
    Material.setFlag(irr::video::EMF_ZBUFFER, false);
    Material.setFlag(irr::video::EMF_BLEND_OPERATION, false);
    Material.setFlag(irr::video::EMF_BACK_FACE_CULLING, false);
    Material.setFlag(irr::video::EMF_FRONT_FACE_CULLING, false);
    Material.setFlag(irr::video::EMF_ANISOTROPIC_FILTER, true);
    Material.setFlag(irr::video::EMF_TRILINEAR_FILTER, false);
    Material.TextureLayer[0].LODBias = -4;
    Material.AntiAliasing = irr::video::EAAM_OFF;

    pIrrDriver->setMaterial(Material);

    pIrrDriver->draw2DVertexPrimitiveList(
        pVertexArray,
        NumberOfVertex,
        &pIndices[LastIndex],
        pDrawCommand->ElemCount / 3,
        irr::video::EVT_STANDARD,
        irr::scene::EPT_TRIANGLES,
        irr::video::EIT_16BIT
        );

    LastIndex += pDrawCommand->ElemCount;

  }

  delete[] pVertexArray;

  return;
}
#endif