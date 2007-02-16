
#include "gl_main.h"
#include "gl_models.h"


CBaseModel::CBaseModel()
{
   m_modelState = MODELSTATE_IDLE;
}


CBaseModel::CBaseModel(const char *fileName)
{
   CBaseModel();
}


CBaseModel::CBaseModel(void *memPtr)
{
   CBaseModel();
}



CMD2Model::CMD2Model(const char *fileName) : CBaseModel(fileName)
{
}


CMD2Model::CMD2Model(void *memPtr) : CBaseModel(memPtr)
{
}
