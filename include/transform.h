/**
 * @file  transform.h
 * @brief linear transform array utilities
 *
 */
/*
 * Original Author: Bruce Fischl
 * CVS Revision Info:
 *    $Author: greve $
 *    $Date: 2010/11/16 00:08:19 $
 *    $Revision: 1.66.2.1 $
 *
 * Copyright (C) 2002-2010,
 * The General Hospital Corporation (Boston, MA). 
 * All rights reserved.
 *
 * Distribution, usage and copying of this software is covered under the
 * terms found in the License Agreement file named 'COPYING' found in the
 * FreeSurfer source code root directory, and duplicated here:
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferOpenSourceLicense
 *
 * General inquiries: freesurfer@nmr.mgh.harvard.edu
 *
 */

#ifndef MGH_TRANSFORM_H
#define MGH_TRANSFORM_H

#include "matrix.h"

#if defined(__cplusplus)
extern "C" {
#endif


#include "const.h"

typedef enum { MINC, TKREG, GENERIC, UNKNOWN=-1 } TransformType;

typedef struct
{
  int           valid;   /* whether this is a
                                    valid info or not (1 valid, 0 not valid) */
  int           width ;
  int           height ;
  int           depth ;
  float         xsize ;
  float         ysize ;
  float         zsize ;
  float         x_r, x_a, x_s;
  float         y_r, y_a, y_s;
  float         z_r, z_a, z_s;
  float         c_r, c_a, c_s;
  char          fname[STRLEN];  // volume filename
}
VOL_GEOM, VG;

typedef struct
{
  float      x0 ;            /* center of transform */
  float      y0 ;
  float      z0 ;
  float      sigma ;         /* spread of transform */
  MATRIX     *m_L ;          /* transform matrix */
  MATRIX     *m_dL ;         /* gradient of fuctional wrt transform matrix */
  MATRIX     *m_last_dL ;    /* last time step for momentum */
  TransformType type;        /* record transform type       */
  VOL_GEOM   src;            /* src for the transform       */
  VOL_GEOM   dst;            /* dst for the transform       */
  int        label ;         // if this xform only applies to a specific label
}
LINEAR_TRANSFORM, LT ;

typedef struct
{
  int               num_xforms ;      /* number linear transforms */
  LINEAR_TRANSFORM  *xforms ;         /* transforms */
  LINEAR_TRANSFORM  *inv_xforms ;     /* inverse transforms */
  int               type ;
  float             fscale ;        // for tkregister2
  char              subject[STRLEN];// for tkregister2
} LINEAR_TRANSFORM_ARRAY, LTA ;

#include "mri.h"
typedef struct
{
  int        type ;
  MRI        *mri_xn ;
  MRI        *mri_yn ;
  MRI        *mri_zn ;
  void       *xform ;
}
TRANSFORM ;

void mincGetVolInfo(const char *infoline, const char *infoline2,
                    VOL_GEOM *vgSrc, VOL_GEOM *vgDst);

int      LTAfree(LTA **plta) ;
LTA      *LTAreadInVoxelCoords(const char *fname, MRI *mri) ;
LTA      *LTAread(const char *fname) ;
LTA      *LTAreadTal(const char *fname) ;
int      LTAwrite(LTA *lta, const char *fname) ;
LTA      *LTAalloc(int nxforms, MRI *mri) ;
int      LTAdivide(LTA *lta, MRI *mri) ;
MRI      *LTAtransform(MRI *mri_src, MRI *mri_dst, LTA *lta) ;
MRI      *LTAtransformInterp
(MRI *mri_src, MRI *mri_dst, LTA *lta, int interp) ;
MATRIX   *LTAtransformAtPoint(LTA *lta, float x, float y,float z,MATRIX *m_L);
int      LTAworldToWorld(LTA *lta, float x, float y, float z,
                         float *px, float *py, float *pz);
int      LTAworldToWorldEx(LTA *lta, float x, float y, float z,
                           float *px, float *py, float *pz);
int      LTAinverseWorldToWorld(LTA *lta, float x, float y, float z,
                                float *px, float *py, float *pz);
int      LTAinverseWorldToWorldEx(LTA *lta, float x, float y, float z,
                                  float *px, float *py, float *pz);
VECTOR   *LTAtransformPoint(LTA *lta, VECTOR *v_X, VECTOR *v_Y) ;
VECTOR   *LTAinverseTransformPoint(LTA *lta, VECTOR *v_X, VECTOR *v_Y) ;
double   LTAtransformPointAndGetWtotal(LTA *lta, VECTOR *v_X, VECTOR *v_Y) ;
MATRIX   *LTAinverseTransformAtPoint(LTA *lta, float x, float y, float z,
                                     MATRIX *m_L) ;
MATRIX   *LTAworldTransformAtPoint(LTA *lta, float x, float y,float z,
                                   MATRIX *m_L);
int      LTAtoVoxelCoords(LTA *lta, MRI *mri) ; // don't use this

/* the following routine reorient and
   reposition the volume by editing the direction cosines and c_ras values
   it does not perform resampling */
MRI *MRITransformedCentered(MRI *src, MRI *orig_dst, LTA *lta);

LTA *LTAinvert(LTA *lta); // fill inverse part of LTA
int LTAmodifySrcDstGeom(LTA *lta, MRI *src, MRI *dst);  /* src and dst can be
                                                           null. only those
                                                           non-null used to
                                                           modify geom */
LTA *LTAchangeType(LTA *lta, int ltatype);  /* must have both src and
                                               dst vol_geom valid */

// new routines to retrieve src and dst volume info for transform
LTA      *LTAreadEx(const char *fname);
int      LTAwriteEx(const LTA *lta, const char *fname);
int      LTAprint(FILE *fp, const LTA *lta);

#define LINEAR_VOX_TO_VOX       0
#define LINEAR_VOXEL_TO_VOXEL   LINEAR_VOX_TO_VOX
#define LINEAR_RAS_TO_RAS       1
#define LINEAR_PHYSVOX_TO_PHYSVOX 2
#define TRANSFORM_ARRAY_TYPE    10
#define MORPH_3D_TYPE           11
#define MNI_TRANSFORM_TYPE      12
#define MATLAB_ASCII_TYPE       13
#define LINEAR_CORONAL_RAS_TO_CORONAL_RAS       21
#define LINEAR_COR_TO_COR       LINEAR_CORONAL_RAS_TO_CORONAL_RAS
#define REGISTER_DAT            14
#define FSLREG_TYPE             15


int      TransformFileNameType(const char *fname) ;
int      LTAvoxelToRasXform(LTA *lta, MRI *mri_src, MRI *mri_dst) ;
int      LTArasToVoxelXform(LTA *lta, MRI *mri_src, MRI *mri_dst) ;
int      LTAvoxelTransformToCoronalRasTransform(LTA *lta) ;

int FixMNITal(float  xmni, float  ymni, float  zmni,
              float *xtal, float *ytal, float *ztal);
MATRIX *DevolveXFM(const char *subjid, MATRIX *XFM, const char *xfmname);
MATRIX *DevolveXFMWithSubjectsDir(const char *subjid,
                                  MATRIX *XFM,
                                  const char *xfmname,
                                  const char *sdir);

int       TransformRas2Vox(TRANSFORM *transform, MRI *mri_src, MRI *mri_dst) ;
int       TransformVox2Ras(TRANSFORM *transform, MRI *mri_src, MRI *mri_dst) ;
TRANSFORM *TransformRead(const char *fname) ;
int       TransformWrite(TRANSFORM *transform, const char *fname) ;
TRANSFORM *TransformIdentity(void) ;
int       TransformFree(TRANSFORM **ptrans) ;
int       TransformSample(TRANSFORM *transform,
                          float xv, float yv, float zv,
                          float *px, float *py, float *pz) ;
int       TransformSampleInverse(TRANSFORM *transform, int xv, int yv, int zv,
                                 float *px, float *py, float *pz) ;
int       TransformSampleInverseVoxel(TRANSFORM *transform,
                                      int width, int height, int depth,
                                      int xv, int yv, int zv,
                                      int *px, int *py, int *pz) ;
TRANSFORM *TransformAlloc(int type, MRI *mri) ;
TRANSFORM *TransformCopy(TRANSFORM *tsrc, TRANSFORM *tdst) ;

int       TransformInvert(TRANSFORM *transform, MRI *mri) ;
int       TransformSwapInverse(TRANSFORM *transform) ;
MRI       *TransformApply(TRANSFORM *transform, MRI *mri_src, MRI *mri_dst) ;
MRI       *TransformCreateDensityMap(TRANSFORM *transform,
                                     MRI *mri_src, MRI *mri_dst) ;
MRI *      TransformApplyInverseType(TRANSFORM *transform, MRI *mri_src, MRI *mri_dst, 
                                     int interp_type);
MRI       *TransformApplyType(TRANSFORM *transform,
                              MRI *mri_src, MRI *mri_dst, int interp_type) ;
MRI       *TransformApplyInverse(TRANSFORM *transform,
                                 MRI *mri_src, MRI *mri_dst) ;

TRANSFORM *TransformCompose(TRANSFORM *t_src, MATRIX *m_left, MATRIX *m_right, TRANSFORM *t_dst);
LTA       *LTAcompose(LTA *lta_src, MATRIX *m_left, MATRIX *m_right, LTA *lta_dst) ;

int     TransformGetSrcVolGeom(TRANSFORM *transform, VOL_GEOM *vg) ;
int     TransformGetDstVolGeom(TRANSFORM *transform, VOL_GEOM *vg) ;
int     TransformSetMRIVolGeomToSrc(TRANSFORM *transform, MRI *mri) ;
int     TransformSetMRIVolGeomToDst(TRANSFORM *transform, MRI *mri) ;


// VOL_GEOM utilities
void initVolGeom(VOL_GEOM *vg);
void copyVolGeom(const VOL_GEOM *src, VOL_GEOM *dst);
void writeVolGeom(FILE *fp, const VOL_GEOM *vg);
void readVolGeom(FILE *fp, VOL_GEOM *vg);
void getVolGeom(const MRI *src, VOL_GEOM *vg);
void useVolGeomToMRI(const VOL_GEOM *src, MRI *dst);
MATRIX *vg_i_to_r(const VOL_GEOM *vg);
MATRIX *vg_r_to_i(const VOL_GEOM *vg);
#define vg_getRasToVoxelXform vg_r_to_i
#define vg_getVoxelToRasXform vg_i_to_r
MATRIX *TkrVox2RASfromVolGeom(const VOL_GEOM *vg);
MATRIX *TkrRAS2VoxfromVolGeom(const VOL_GEOM *vg);

int TransformCopyVolGeomToMRI(TRANSFORM *transform, MRI *mri);

int vg_isEqual(const VOL_GEOM *vg1,
               const VOL_GEOM *vg2); /* return 1 if equal
                                        return 0 if not equal */
void vg_print(const VOL_GEOM *vg);

int LTAvoxelXformToRASXform(const MRI *src, const MRI *dst,
                            LT *voxTran, LT *rasTran);

// well uses LTA
MATRIX *surfaceRASFromSurfaceRAS_(MRI *dst, MRI *src, LTA *lta);
MRI   *MRITransformedCentered(MRI *src, MRI *orig_dst, LTA *lta) ;
int TransformSampleReal(TRANSFORM *transform,
                        float xv, float yv, float zv,
                        float *px, float *py, float *pz) ;

const char *LTAtransformTypeName(int ltatype);
int LTAdump(FILE *fp, LTA *lta);
int LTAdumpLinearTransform(FILE *fp, LT *lt);
int LTAsetVolGeom(LTA *lta, MRI *mri_src, MRI *mri_dst) ;
MATRIX *VGgetVoxelToRasXform(VOL_GEOM *vg, MATRIX *m, int base) ;
MATRIX *VGgetRasToVoxelXform(VOL_GEOM *vg, MATRIX *m, int base) ;
LTA *TransformRegDat2LTA(MRI *targ, MRI *mov, MATRIX *R);
MATRIX *TransformLTA2RegDat(LTA *lta);
int TransformSampleDirection(TRANSFORM *transform, float x0, float y0, float z0, float nx, float ny, float nz,
                             float *pnx, float *pny, float *pnz);

int TransformRas2Vox(TRANSFORM *transform, MRI *mri_src, MRI *mri_dst);
int TransformVox2Ras(TRANSFORM *transform, MRI *mri_src, MRI *mri_dst);

MATRIX *MRIangles2RotMat(double *angles);
double *SegRegCost(MRI *regseg, MRI *f, double *costs);
MRI *MRIaffineDisplacment(MRI *mri, MATRIX *R);

#if defined(__cplusplus)
};
#endif



#endif
