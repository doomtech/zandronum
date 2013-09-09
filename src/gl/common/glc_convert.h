
#ifndef __GLC_CONVERT
#define __GLC_CONVERT

#define TO_GL(v) ((float)(v)/FRACUNIT)
#define TO_MAP(f) (fixed_t)quickertoint((f)*FRACUNIT)
#define ANGLE_TO_FLOAT(ang) ((float)((ang) * 180. / ANGLE_180))
#define FLOAT_TO_ANGLE(ang) (angle_t)((ang) / 180. * ANGLE_180)

#define ANGLE_TO_RAD(ang) ((float)((ang) * M_PI / ANGLE_180))
#define RAD_TO_ANGLE(ang) (angle_t)((ang) / M_PI * ANGLE_180)

#define FLOAT_TO_RAD(ang) float((ang) * M_PI / 180.)
#define RAD_TO_FLOAT(ang) float((ang) / M_PI * 180.)

#endif