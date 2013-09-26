#ifndef __VERTEXBUFFER_H
#define __VERTEXBUFFER_H

#include "tarray.h"

struct vertex_t;
struct secplane_t;
struct subsector_t;
struct sector_t;

struct FVBOVertex	// exactly 32 bytes large
{
	float x,z,y,w;	// w only for padding to make one vertex 32 bytes - maybe it will find some use later
	float u,v;		// texture coordinates
	float dc, df;	// distance to floor and ceiling on walls - used for glowing

	void SetFlatVertex(vertex_t *vt, const secplane_t &plane);
};

#define VTO ((FVBOVertex*)NULL)


class FVertexBuffer
{
	unsigned int vbo_id;
	FVBOVertex *map;

	void MapVBO();
	void CheckPlanes(sector_t *sector);

public:
	int vbo_arg;
	TArray<FVBOVertex> vbo_shadowdata;	// this is kept around for non-VBO rendering

public:
	FVertexBuffer();
	~FVertexBuffer();

	int CreateSubsectorVertices(subsector_t *sub, const secplane_t &plane, int floor);
	int CreateSectorVertices(sector_t *sec, const secplane_t &plane, int floor);
	int CreateVertices(int h, sector_t *sec, const secplane_t &plane, int floor);
	void CreateFlatVBO();
	void CreateVBO();
	void UpdatePlaneVertices(sector_t *sec, int plane);
	void BindVBO();
	void CheckUpdate(sector_t *sector);
	void UnmapVBO();

};

#endif