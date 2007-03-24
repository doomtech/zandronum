
#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include "Glext.h"
#include "wglext.h"

#define USE_WINDOWS_DWORD
#include "OpenGLVideo.h"
#include "zgl_main.h"
#include "tarray.h"


void R_SetWindow (int windowSize, int fullWidth, int fullHeight, int stHeight);
void R_ExecuteSetViewSize ();
void GL_RenderViewToTexture(AActor *viewpoint, FTexture *tex, int x, int y, int width, int height, int fov)
{
   glViewport(0, 0, width, height);
   R_SetWindow (12, width, height, height);
   GL_RenderActorView(viewpoint, width * 1.f / height, fov * 1.f);

   // just update all the textures regardless of translation
   for (unsigned int i = 0; i < tex->glData.Size(); i++)
   {
      textureList.BindGLTexture(tex->glData[i]->glTex);
      glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
      //glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 0, 0, width, height, 0);
   }

   R_ExecuteSetViewSize();
   GL_ResetViewport();
}
