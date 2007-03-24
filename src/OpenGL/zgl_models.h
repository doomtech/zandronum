
#ifndef __GL_MODELS_H__
#define __GL_MODELS_H__


enum
{
   MODELSTATE_IDLE,
   MODELSTATE_WALK,
   MODELSTATE_FIRE,
   MODELSTATE_DIE,
   MODELSTATE_GIB,
   MODELSTATE_DEAD,
   NUM_MODELSTATES
};


class CBaseModel
{
public:
   // initialize vars
   CBaseModel();
   // load from file
   CBaseModel(const char *fileName);
   // load from lump
   CBaseModel(void *memPtr);
protected:
   // this is setup per model loaded to remap the animation to an entity state
   int m_stateRemap[NUM_MODELSTATES];
   int m_modelState;
};


class CMD2Model : public CBaseModel
{
public:
   CMD2Model(const char *fileName);
   CMD2Model(void *memPtr);
};



#endif //__GL_MODELS_H__