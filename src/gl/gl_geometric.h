#ifndef __GL_GEOM
#define __GL_GEOM

#include "math.h"
#include "r_defs.h"

class Vector
{
public:
	Vector()
	{
	   SetX(0.f);
	   SetY(1.f);
	   SetZ(0.f);
	   m_length = 1.f;
	}

	Vector(float x, float y, float z)
	{
	   SetX(x);
	   SetY(y);
	   SetZ(z);
	   m_length=-1.0f;
	}

	Vector(vertex_t * v)
	{
		SetX(v->x/65536.0f);
		SetY(v->y/65536.0f);
		SetZ(0);
	}

	void Normalize()
	{
	   float l = 1.f / Length();

	   SetX(X() * l);
	   SetY(Y() * l);
	   SetZ(Z() * l);
	   m_length=1.0f;
	}

	void UpdateLength()
	{
	   m_length = sqrtf((X() * X()) + (Y() * Y()) + (Z() * Z()));
	}

	void Set(float *v)
	{
	   SetX(v[0]);
	   SetY(v[1]);
	   SetZ(v[2]);
	   m_length=-1.0f;
	}

	void Set(float x, float y, float z)
	{
	   SetX(x);
	   SetY(y);
	   SetZ(z);
	   m_length=-1.0f;
	}

	float Length()
	{
		if (m_length<0.0f) UpdateLength();
		return m_length;
	}

	float Dist(Vector &v)
	{
	   Vector t(X() - v.X(), Y() - v.Y(), Z() - v.Z());

	   return t.Length();
	}

	float Dot(Vector &v)
	{
	   return (X() * v.X()) + (Y() * v.Y()) + (Z() * v.Z());
	}
	
	Vector Cross(Vector &v);
	Vector operator- (Vector &v);
	Vector operator+ (Vector &v);
	Vector operator* (float f);
	Vector operator/ (float f);
	bool operator== (Vector &v);
	bool operator!= (Vector &v) { return !((*this) == v); }
	
	void GetRightUp(Vector &up, Vector &right);
	float operator[] (int index) const { return m_vec[index]; }
	float &operator[] (int index) { return m_vec[index]; }
	float X() { return m_vec[0]; }
	float Y() { return m_vec[1]; }
	float Z() { return m_vec[2]; }
	void SetX(float x) { m_vec[0] = x; }
	void SetY(float y) { m_vec[1] = y; }
	void SetZ(float z) { m_vec[2] = z; }
	void Scale(float scale);
	
	Vector ProjectVector(Vector &a);
	Vector ProjectPlane(Vector &right, Vector &up);
protected:
	float m_vec[3];
	float m_length;
};


class Plane
{
public:
	Plane()
	{
	   m_normal.Set(0.f, 1.f, 0.f);
	   m_d = 0.f;
	}
   void Init(float *v1, float *v2, float *v3);
   void Init(float a, float b, float c, float d);
   void Init(float *verts, int numVerts);
   void Set(secplane_t &plane);
   float DistToPoint(float x, float y, float z);
   bool PointOnSide(float x, float y, float z);
   bool PointOnSide(Vector &v) { return PointOnSide(v.X(), v.Y(), v.Z()); }
   bool ValidNormal() { return m_normal.Length() == 1.f; }

   float A() { return m_normal.X(); }
   float B() { return m_normal.Y(); }
   float C() { return m_normal.Z(); }
   float D() { return m_d; }

   Vector Normal() { return m_normal; }
protected:
   Vector m_normal;
   float m_d;
};

#endif