/**
 * @file  resample.h
 * @brief REPLACE_WITH_ONE_LINE_SHORT_DESCRIPTION
 *
 * REPLACE_WITH_LONG_DESCRIPTION_OR_REFERENCE
 */
/*
 * Original Author: REPLACE_WITH_FULL_NAME_OF_CREATING_AUTHOR 
 * CVS Revision Info:
 *    $Author: greve $
 *    $Date: 2011/01/04 19:21:44 $
 *    $Revision: 1.15.2.1 $
 *
 * Copyright (C) 2002-2007,
 * The General Hospital Corporation (Boston, MA). 
 * All rights reserved.
 *
 * Distribution, usage and copying of this software is covered under the
 * terms found in the License Agreement file named 'COPYING' found in the
 * FreeSurfer source code root directory, and duplicated here:
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferOpenSourceLicense
 *
 * General inquiries: freesurfer@nmr.mgh.harvard.edu
 * Bug reports: analysis-bugs@nmr.mgh.harvard.edu
 *
 */


#ifndef RESAMPLE_H_INC
#define RESAMPLE_H_INC

#include "MRIio_old.h"
#include "mrisurf.h"
#include "label.h"

/* float-to-integer conversions */
#define FLT2INT_ROUND 0  /* c = (int)rint(x) */
#define FLT2INT_FLOOR 1  /* c = (int)floor(x) */
#define FLT2INT_TKREG 2  /* ceil(r), floor(c), floor(s) */

/* interpolation methods */
#define INTERP_NEAREST 0
#define INTERP_TLI     1
#define INTERP_SINC    2

#define IND2CRS
#define CRS2IND

#ifdef RESAMPLE_SOURCE_CODE_FILE
char *ResampleVtxMapFile;
#else
extern char *ResampleVtxMapFile;
#endif

int interpolation_code(char *interpolation_string);
int float2int_code(char *float2int_string);

int ProjNormFracThick(float *x, float *y, float *z,
                      MRI_SURFACE *surf, int vtx, float frac);
int ProjNormDist(float *x, float *y, float *z,
                 MRI_SURFACE *surf, int vtx, float dist);
int ProjNormDistNbr(float *x, float *y, float *z, MRI_SURFACE *surf, 
		    int vtxno, float dist, int nthNbr);
int ProjNormFracThickNbr(float *x, float *y, float *z, MRI_SURFACE *surf, 
			 int vtxno, float frac, int nthNbr);



MRI * MRILoadBVolume(char *stem);
MATRIX * RandMatrix(int nrows, int ncols);
MATRIX * ConstMatrix(int nrows, int ncols, float val);
int MatrixFill(MATRIX *M, float val);

MATRIX * FOVQuantMatrix(int ncols, int nrows, int nslcs,
                        float colres, float rowres, float slcres  );
MATRIX * FOVDeQuantMatrix(int ncols, int nrows, int nslcs,
                          float colres, float rowres, float slcres  );

int XYZAnat2CRSFunc_TkReg(int *col, int *row, int *slc,
                          int npixels, float pixsize,
                          int nslcs,   float slcthick,
                          float xanat, float yanat, float zanat,
                          MATRIX *Reg);

int float2int_TkReg(int *col, int *row, int *slc,
                    float fltcol, float fltrow, float fltslc);

MATRIX * FOVQuantMtx_TkReg(int npixels, float pixsize,
                           int nslcs,   float slcthick);

MATRIX *ComputeQFWD(MATRIX *Q, MATRIX *F, MATRIX *W, MATRIX *D, MATRIX *QFWD);

MATRIX * CorXYZ_to_VolCRS_Matrix(MATRIX * R, MATRIX * Qv);
int ind2crs(int *c, int *r, int *s, int ind, int ncols, int nrows, int nslcs);
int crs2ind(int *ind, int c, int r, int s, int ncols, int nrows, int nslcs );

MRI *vol2vol_linear(MRI *SrcVol,
                    MATRIX *Qsrc, MATRIX *Fsrc, MATRIX *Wsrc, MATRIX *Dsrc,
                    MATRIX *Qtrg, MATRIX *Ftrg, MATRIX *Wtrg, MATRIX *Dtrg,
                    int   nrows_trg, int   ncols_trg, int   nslcs_trg,
                    MATRIX *Msrc2trg, int InterpMethod, int float2int);

MRI *vol2roi_linear(MRI *SrcVol,
                    MATRIX *Qsrc, MATRIX *Fsrc, MATRIX *Wsrc, MATRIX *Dsrc,
                    MRI *SrcMskVol, MATRIX *Msrc2lbl, LABEL *Label,
                    int float2int, int *nhits, MRI *FinalMskVol);

MRI *label2mask_linear(MRI *SrcVol,
                       MATRIX *Qsrc, MATRIX *Fsrc, MATRIX *Wsrc, MATRIX *Dsrc,
                       MRI *SrcMskVol, MATRIX *Msrc2lbl, LABEL *Label,
                       float rszthresh, int float2int, int *nlabelhits, int *nfinalhits);

MRI * vol2maskavg(MRI *SrcVol, MRI *SrcMskVol, int *nhits);

MRI *vol2surf_linear(MRI *SrcVol,
                     MATRIX *Qsrc, MATRIX *Fsrc, MATRIX *Wsrc, MATRIX *Dsrc,
                     MRI_SURFACE *TrgSurf, float ProjFrac,
                     int InterpMethod, int float2int, MRI *SrcHitVol,
                     int ProjDistFlag, int nskip);

MRI *surf2surf_nnfr(MRI *SrcSurfVals, MRI_SURFACE *SrcSurfReg,
                    MRI_SURFACE *TrgSurfReg, MRI **SrcHits,
                    MRI **SrcDist, MRI **TrgHits, MRI **TrgDist,
                    int ReverseMapFlag, int UseHash);
MRI *surf2surf_nnfr_jac(MRI *SrcSurfVals, MRI_SURFACE *SrcSurfReg,
                        MRI_SURFACE *TrgSurfReg, MRI **SrcHits,
                        MRI **SrcDist, MRI **TrgHits, MRI **TrgDist,
                        int ReverseMapFlag, int UseHash);

MRI *surf2surf_nnf(MRI *SrcSurfVals, MRI_SURFACE *SrcSurfReg,
                   MRI_SURFACE *TrgSurfReg, int UseHash);

MRI *MRImapSurf2VolClosest(MRIS *surf, MRI *vol,
                           MATRIX *Ma2v, float projfrac);
int MRIsurf2Vol(MRI *surfvals, MRI *vol, MRI *map);

MRI *MRIaseg2vol(MRI *aseg, MATRIX *tkR, MRI *voltemp,
                 double fthresh, MRI **pvolhit);

#endif /* #ifndef RESAMPLE_H_INC */
