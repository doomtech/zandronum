//-----------------------------------------------------------------------------
//
// Skulltag Online 2
// Copyright (C) 2007 Rivecoder, other authors unknown
// Date created: 8/29/07
//
// Huffman encoding module. Ported from Skulltag.
//-----------------------------------------------------------------------------

void HuffDecode(unsigned char *in,unsigned char *out,int inlen,int *outlen);
void HuffEncode(unsigned char *in,unsigned char *out,int inlen,int *outlen);
void HuffInit(void);
