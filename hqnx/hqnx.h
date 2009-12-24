#ifndef __HQNX_H__
#define __HQNX_H__

#include "Image.h"

void DLL hq2x_32( int * pIn, unsigned char * pOut, int Xres, int Yres, int BpL );
void DLL hq3x_32( int * pIn, unsigned char * pOut, int Xres, int Yres, int BpL );
void DLL hq4x_32( int * pIn, unsigned char * pOut, int Xres, int Yres, int BpL );
int DLL hq4x_32 ( CImage &ImageIn, CImage &ImageOut );

void DLL InitLUTs();


#endif //__HQNX_H__