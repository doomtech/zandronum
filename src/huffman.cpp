//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2007 Sean White, Brad Carney
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Date created: 
//
//
// Filename: huffman.cpp
//
// Description: Compresses network traffic.
//
//-----------------------------------------------------------------------------

#include "networkheaders.h"

#include "huffman.h"
#include "i_system.h"

//*****************************************************************************
//	PROTOTYPES

static void						huffman_BuildTree( float *freq );
static void						huffman_PutBit( unsigned char *puf, int pos, int bit );
static int						huffman_GetBit( unsigned char *puf, int pos );
static void						huffman_ZeroFreq( void );
static void						huffman_RecursiveFreeNode( huffnode_t *pNode );

//*****************************************************************************
//	VARIABLES

static huffnode_t	*g_HuffTree = 0;
static hufftab_t	g_HuffLookup[256];

#ifdef _DEBUG
static int			HuffIn=0;
static int			HuffOut=0;
#endif

static unsigned char g_Masks[8]=
{
	0x1,
	0x2,
	0x4,
	0x8,
	0x10,
	0x20,
	0x40,
	0x80
};

static float g_FrequencyTable[256]=
{
	(float) 0.14473691,
	(float) 0.01147017,
	(float) 0.00167522,
	(float) 0.03831121,
	(float) 0.00356579,
	(float) 0.03811315,
	(float) 0.00178254,
	(float) 0.00199644,
	(float) 0.00183511,
	(float) 0.00225716,
	(float) 0.00211240,
	(float) 0.00308829,
	(float) 0.00172852,
	(float) 0.00186608,
	(float) 0.00215921,
	(float) 0.00168891,
	(float) 0.00168603,
	(float) 0.00218586,
	(float) 0.00284414,
	(float) 0.00161833,
	(float) 0.00196043,
	(float) 0.00151029,
	(float) 0.00173932,
	(float) 0.00218370,
	(float) 0.00934121,
	(float) 0.00220530,
	(float) 0.00381211,
	(float) 0.00185456,
	(float) 0.00194675,
	(float) 0.00161977,
	(float) 0.00186680,
	(float) 0.00182071,
	(float) 0.06421956,
	(float) 0.00537786,
	(float) 0.00514019,
	(float) 0.00487155,
	(float) 0.00493925,
	(float) 0.00503143,
	(float) 0.00514019,
	(float) 0.00453520,
	(float) 0.00454241,
	(float) 0.00485642,
	(float) 0.00422407,
	(float) 0.00593387,
	(float) 0.00458130,
	(float) 0.00343687,
	(float) 0.00342823,
	(float) 0.00531592,
	(float) 0.00324890,
	(float) 0.00333388,
	(float) 0.00308613,
	(float) 0.00293776,
	(float) 0.00258918,
	(float) 0.00259278,
	(float) 0.00377105,
	(float) 0.00267488,
	(float) 0.00227516,
	(float) 0.00415997,
	(float) 0.00248763,
	(float) 0.00301555,
	(float) 0.00220962,
	(float) 0.00206990,
	(float) 0.00270369,
	(float) 0.00231694,
	(float) 0.00273826,
	(float) 0.00450928,
	(float) 0.00384380,
	(float) 0.00504728,
	(float) 0.00221251,
	(float) 0.00376961,
	(float) 0.00232990,
	(float) 0.00312574,
	(float) 0.00291688,
	(float) 0.00280236,
	(float) 0.00252436,
	(float) 0.00229461,
	(float) 0.00294353,
	(float) 0.00241201,
	(float) 0.00366590,
	(float) 0.00199860,
	(float) 0.00257838,
	(float) 0.00225860,
	(float) 0.00260646,
	(float) 0.00187256,
	(float) 0.00266552,
	(float) 0.00242641,
	(float) 0.00219450,
	(float) 0.00192082,
	(float) 0.00182071,
	(float) 0.02185930,
	(float) 0.00157439,
	(float) 0.00164353,
	(float) 0.00161401,
	(float) 0.00187544,
	(float) 0.00186248,
	(float) 0.03338637,
	(float) 0.00186968,
	(float) 0.00172132,
	(float) 0.00148509,
	(float) 0.00177749,
	(float) 0.00144620,
	(float) 0.00192442,
	(float) 0.00169683,
	(float) 0.00209439,
	(float) 0.00209439,
	(float) 0.00259062,
	(float) 0.00194531,
	(float) 0.00182359,
	(float) 0.00159096,
	(float) 0.00145196,
	(float) 0.00128199,
	(float) 0.00158376,
	(float) 0.00171412,
	(float) 0.00243433,
	(float) 0.00345704,
	(float) 0.00156359,
	(float) 0.00145700,
	(float) 0.00157007,
	(float) 0.00232342,
	(float) 0.00154198,
	(float) 0.00140730,
	(float) 0.00288807,
	(float) 0.00152830,
	(float) 0.00151246,
	(float) 0.00250203,
	(float) 0.00224420,
	(float) 0.00161761,
	(float) 0.00714383,
	(float) 0.08188576,
	(float) 0.00802537,
	(float) 0.00119484,
	(float) 0.00123805,
	(float) 0.05632671,
	(float) 0.00305156,
	(float) 0.00105584,
	(float) 0.00105368,
	(float) 0.00099246,
	(float) 0.00090459,
	(float) 0.00109473,
	(float) 0.00115379,
	(float) 0.00261223,
	(float) 0.00105656,
	(float) 0.00124381,
	(float) 0.00100326,
	(float) 0.00127550,
	(float) 0.00089739,
	(float) 0.00162481,
	(float) 0.00100830,
	(float) 0.00097229,
	(float) 0.00078864,
	(float) 0.00107240,
	(float) 0.00084409,
	(float) 0.00265760,
	(float) 0.00116891,
	(float) 0.00073102,
	(float) 0.00075695,
	(float) 0.00093916,
	(float) 0.00106880,
	(float) 0.00086786,
	(float) 0.00185600,
	(float) 0.00608367,
	(float) 0.00133600,
	(float) 0.00075695,
	(float) 0.00122077,
	(float) 0.00566955,
	(float) 0.00108249,
	(float) 0.00259638,
	(float) 0.00077063,
	(float) 0.00166586,
	(float) 0.00090387,
	(float) 0.00087074,
	(float) 0.00084914,
	(float) 0.00130935,
	(float) 0.00162409,
	(float) 0.00085922,
	(float) 0.00093340,
	(float) 0.00093844,
	(float) 0.00087722,
	(float) 0.00108249,
	(float) 0.00098598,
	(float) 0.00095933,
	(float) 0.00427593,
	(float) 0.00496661,
	(float) 0.00102775,
	(float) 0.00159312,
	(float) 0.00118404,
	(float) 0.00114947,
	(float) 0.00104936,
	(float) 0.00154342,
	(float) 0.00140082,
	(float) 0.00115883,
	(float) 0.00110769,
	(float) 0.00161112,
	(float) 0.00169107,
	(float) 0.00107816,
	(float) 0.00142747,
	(float) 0.00279804,
	(float) 0.00085922,
	(float) 0.00116315,
	(float) 0.00119484,
	(float) 0.00128559,
	(float) 0.00146204,
	(float) 0.00130215,
	(float) 0.00101551,
	(float) 0.00091756,
	(float) 0.00161184,
	(float) 0.00236375,
	(float) 0.00131872,
	(float) 0.00214120,
	(float) 0.00088875,
	(float) 0.00138570,
	(float) 0.00211960,
	(float) 0.00094060,
	(float) 0.00088083,
	(float) 0.00094564,
	(float) 0.00090243,
	(float) 0.00106160,
	(float) 0.00088659,
	(float) 0.00114514,
	(float) 0.00095861,
	(float) 0.00108753,
	(float) 0.00124165,
	(float) 0.00427016,
	(float) 0.00159384,
	(float) 0.00170547,
	(float) 0.00104431,
	(float) 0.00091395,
	(float) 0.00095789,
	(float) 0.00134681,
	(float) 0.00095213,
	(float) 0.00105944,
	(float) 0.00094132,
	(float) 0.00141883,
	(float) 0.00102127,
	(float) 0.00101911,
	(float) 0.00082105,
	(float) 0.00158448,
	(float) 0.00102631,
	(float) 0.00087938,
	(float) 0.00139290,
	(float) 0.00114658,
	(float) 0.00095501,
	(float) 0.00161329,
	(float) 0.00126542,
	(float) 0.00113218,
	(float) 0.00123661,
	(float) 0.00101695,
	(float) 0.00112930,
	(float) 0.00317976,
	(float) 0.00085346,
	(float) 0.00101190,
	(float) 0.00189849,
	(float) 0.00105728,
	(float) 0.00186824,
	(float) 0.00092908,
	(float) 0.00160896,
};

//=============================================================================
//
//	HUFFMAN_Construct
//
//	Builds the Huffman tree.
//
//=============================================================================

void HUFFMAN_Construct( void )
{
#ifdef _DEBUG
	huffman_ZeroFreq( );
#endif

	huffman_BuildTree( g_FrequencyTable );

	// Free the table when the program closes.
	atterm( HUFFMAN_Destruct );
}

//=============================================================================
//
//	HUFFMAN_Destruct
//
//	Frees memory allocated by the Huffman tree.
//
//=============================================================================

void HUFFMAN_Destruct( void )
{
	huffman_RecursiveFreeNode( g_HuffTree );

	M_Free( g_HuffTree );
	g_HuffTree = NULL;
}

//=============================================================================
//
//	HUFFMAN_Encode
//
//=============================================================================

void HUFFMAN_Encode( unsigned char *in, unsigned char *out, int inlen, int *outlen )
{
	int i,j,bitat;
	unsigned int t;
	bitat=0;
	for (i=0;i<inlen;i++)
	{
		t=g_HuffLookup[in[i]].bits;
		for (j=0;j<g_HuffLookup[in[i]].len;j++)
		{
			huffman_PutBit(out+1,bitat+g_HuffLookup[in[i]].len-j-1,t&1);
			t>>=1;
		}
		bitat+=g_HuffLookup[in[i]].len;
	}
	*outlen=1+(bitat+7)/8;
	*out=8*((*outlen)-1)-bitat;
	if(*outlen >= inlen+1)
	{
		*out=0xff;
		memcpy(out+1,in,inlen);
		*outlen=inlen+1;
	}
/*#if _DEBUG

	HuffIn+=inlen;
	HuffOut+=*outlen;

	{
		unsigned char *buf;
		int tlen;
		
		buf= (unsigned char *)Malloc(inlen);
		
		HuffDecode(out,buf,*outlen,&tlen);
		if(!tlen==inlen)
			I_FatalError("bogus compression");
		for (i=0;i<inlen;i++)
		{
			if(in[i]!=buf[i])
				I_FatalError("bogus compression");
		}
		M_Free(buf);
	}
#endif*/
}

//=============================================================================
//
//	HUFFMAN_Decode
//
//=============================================================================

void HUFFMAN_Decode( unsigned char *in, unsigned char *out, int inlen, int *outlen )
{
	int bits,tbits;
	huffnode_t *tmp;	
	if (*in==0xff)
	{
		memcpy(out,in+1,inlen-1);
		*outlen=inlen-1;
		return;
	}
	tbits=(inlen-1)*8-*in;
	bits=0;
	*outlen=0;
	while (bits<tbits)
	{
		tmp=g_HuffTree;
		do
		{
			if (huffman_GetBit(in+1,bits))
				tmp=tmp->one;
			else
				tmp=tmp->zero;
			bits++;
		} while (tmp->zero);
		*out++=tmp->val;
		(*outlen)++;
	}
}

#ifdef _DEBUG
static int freqs[256];
void huffman_ZeroFreq(void)
{
	memset(freqs, 0, 256*sizeof(int));
}


void CalcFreq(unsigned char *packet, int packetlen)
{
	int ix;

	for (ix=0; ix<packetlen; ix++)
	{
		freqs[packet[ix]]++;
	}
}

void PrintFreqs(void)
{
	int ix;
	float total=0;
	char string[100];

	for (ix=0; ix<256; ix++)
	{
		total += freqs[ix];
	}
	if (total>.01)
	{
		for (ix=0; ix<256; ix++)
		{
			sprintf(string, "\t%.8f,\n",((float)freqs[ix])/total);
			//OutputDebugString(string);
			Printf(string);
		}
	}
	huffman_ZeroFreq();
}
#endif


void FindTab(huffnode_t *tmp,int len,unsigned int bits)
{
	if(!tmp)
		I_FatalError("no huff node");
	if (tmp->zero)
	{
		if(!tmp->one)
			I_FatalError("no one in node");
		if(len>=32)
			I_FatalError("compression screwd");
		FindTab(tmp->zero,len+1,bits<<1);
		FindTab(tmp->one,len+1,(bits<<1)|1);
		return;
	}
	g_HuffLookup[tmp->val].len=len;
	g_HuffLookup[tmp->val].bits=bits;
	return;
}

//=============================================================================
//
//	huffman_PutBit
//
//=============================================================================

void huffman_PutBit(unsigned char *buf,int pos,int bit)
{
	if (bit)
		buf[pos/8]|=g_Masks[pos%8];
	else
		buf[pos/8]&=~g_Masks[pos%8];
}

//=============================================================================
//
//	huffman_GetBit
//
//=============================================================================

int huffman_GetBit(unsigned char *buf,int pos)
{
	if (buf[pos/8]&g_Masks[pos%8])
		return 1;
	else
		return 0;
}

//=============================================================================
//
//	huffman_BuildTree
//
//=============================================================================

void huffman_BuildTree( float *freq )
{
	float min1,min2;
	int i,j,minat1,minat2;
	huffnode_t *work[256];	
	huffnode_t *tmp;	
	for (i=0;i<256;i++)
	{
		work[i]=(huffnode_s *)M_Malloc(sizeof(huffnode_t));
		
		
		work[i]->val=(unsigned char)i;
		work[i]->freq=freq[i];
		work[i]->zero=0;
		work[i]->one=0;
		g_HuffLookup[i].len=0;
	}
	for (i=0;i<255;i++)
	{
		minat1=-1;
		minat2=-1;
		min1=1E30;
		min2=1E30;
		for (j=0;j<256;j++)
		{
			if (!work[j])
				continue;
			if (work[j]->freq<min1)
			{
				minat2=minat1;
				min2=min1;
				minat1=j;
				min1=work[j]->freq;
			}
			else if (work[j]->freq<min2)
			{
				minat2=j;
				min2=work[j]->freq;
			}
		}
		if (minat1<0)
			I_FatalError("minatl: %d",minat1);
		if (minat2<0)
			I_FatalError("minat2: %d",minat2);
		
		tmp= (huffnode_s *)M_Malloc(sizeof(huffnode_t));
		
		
		tmp->zero=work[minat2];
		tmp->one=work[minat1];
		tmp->freq=work[minat2]->freq+work[minat1]->freq;
		tmp->val=0xff;
		work[minat1]=tmp;
		work[minat2]=0;
	}		
	g_HuffTree=tmp;
	FindTab(g_HuffTree,0,0);

#if _DEBUG
	for (i=0;i<256;i++)
	{
		if(!g_HuffLookup[i].len&&g_HuffLookup[i].len<=32)
			I_FatalError("bad frequency table");
		//Printf(PRINT_HIGH, "%d %d %2X\n", g_HuffLookup[i].len, g_HuffLookup[i].bits, i);
	}
#endif
}

//=============================================================================
//
//	huffman_RecursiveFreeNode
//
//=============================================================================

static void huffman_RecursiveFreeNode( huffnode_t *pNode )
{
	if ( pNode->one )
	{
		huffman_RecursiveFreeNode( pNode->one );

		M_Free( pNode->one );
		pNode->one = NULL;
	}

	if ( pNode->zero )
	{
		huffman_RecursiveFreeNode( pNode->zero );

		M_Free( pNode->zero );
		pNode->zero = NULL;
	}
}
