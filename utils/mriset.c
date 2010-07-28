/**
 * @file  mriset.c
 * @brief utilities for MRI data structure
 *
 * Operators like union, intersection, erosion, dialation and masking on
 * MRI data structs.
 */
/*
 * Original Author: Bruce Fischl
 * CVS Revision Info:
 *    $Author: greve $
 *    $Date: 2010/07/28 20:07:29 $
 *    $Revision: 1.74.2.1 $
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

/*-----------------------------------------------------
  INCLUDE FILES
  -------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
double round(double x);
#include <string.h>
#include <memory.h>

#include "error.h"
#include "proto.h"
#include "mri.h"
#include "mri2.h"
#include "macros.h"
#include "diag.h"
#include "minc_volume_io.h"
#include "filter.h"
#include "box.h"
#include "region.h"
#include "matrix.h"

extern const char* Progname;

/*-----------------------------------------------------
  MACROS AND CONSTANTS
  -------------------------------------------------------*/

#define DEBUG_POINT(x,y,z)  (((x==72) && (y==142)) &&((z)==127))

/*-----------------------------------------------------
  STATIC DATA
  -------------------------------------------------------*/

/*-----------------------------------------------------
  STATIC PROTOTYPES
  -------------------------------------------------------*/
/*-----------------------------------------------------
  GLOBAL FUNCTIONS
  -------------------------------------------------------*/

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description: A sanity-check: make sure the volumes being
  operated upon are the same dimensions
  ------------------------------------------------------*/
static void MRIcheckVolDims(MRI *mri1, MRI *mri2)
{
  if (mri1 == NULL) return;
  if (mri2 == NULL) return;

  if (mri1->height != mri2->height)
    ErrorExit(ERROR_BADPARM,
              "ERROR: %s-MRIcheckVolDims: volume1 height=%d != "
              "volume2 height=%d.\n",
              Progname, mri1->height, mri2->height);
  if (mri1->width != mri2->width)
    ErrorExit(ERROR_BADPARM,
              "ERROR: %s-MRIcheckVolDims: volume1 width=%d != "
              "volume2 width=%d.\n",
              Progname, mri1->width, mri2->width);
  if (mri1->depth != mri2->depth)
    ErrorExit(ERROR_BADPARM,
              "ERROR: %s-MRIcheckVolDims: volume1 depth=%d != "
              "volume2 depth=%d.\n",
              Progname, mri1->depth, mri2->depth);
}


/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIunion(MRI *mri1, MRI *mri2, MRI *mri_dst)
{
  int     width, height, depth, x, y, z ;
  BUFTYPE *p1, *p2, *pdst, v1, v2 ;

  MRIcheckVolDims(mri1, mri2);

  width = mri1->width ;
  height = mri1->height ;
  depth = mri1->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri1, NULL) ;

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      pdst = &MRIvox(mri_dst, 0, y, z) ;
      p1 = &MRIvox(mri1, 0, y, z) ;
      p2 = &MRIvox(mri2, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        v1 = *p1++ ;
        v2 = *p2++ ;
        *pdst++ = MAX(v1, v2) ;
      }
    }
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIintersect(MRI *mri1, MRI *mri2, MRI *mri_dst)
{
  int     width, height, depth, x, y, z, f, v1, v2 ;

  MRIcheckVolDims(mri1, mri2);

  width = mri1->width ; height = mri1->height ; depth = mri1->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri1, NULL) ;

  for (f = 0 ; f < mri_dst->nframes ; f++)
    for (z = 0 ; z < depth ; z++)
      for (y = 0 ; y < height ; y++)
        for (x = 0 ; x < width ; x++)
        {
          v1 = MRIgetVoxVal(mri1, x, y, z, f) ;
          v2 = MRIgetVoxVal(mri2, x, y, z, f) ;
          MRIsetVoxVal(mri_dst, x, y,z, f, MIN(v1, v2)) ;
        }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIcomplement(MRI *mri_src, MRI *mri_dst)
{
  int     width, height, depth, x, y, z, val, f ;

  MRIcheckVolDims(mri_src, mri_dst);

  width = mri_src->width ; height = mri_src->height ; depth = mri_src->depth ;

  if (!mri_dst)
  {
    mri_dst = MRIalloc(width, height, depth, mri_src->type) ;
    MRIcopyHeader(mri_src, mri_dst) ;
  }

  for (f = 0 ; f < mri_src->nframes ; f++)
    for (z = 0 ; z < depth ; z++)
      for (y = 0 ; y < height ; y++)
        for (x = 0 ; x < width ; x++)
        {
          val = (int)MRIgetVoxVal(mri_src, x, y, z, f) ;
          MRIsetVoxVal(mri_dst, x, y, z, f, !val) ;
        }

  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIand(MRI *mri1, MRI *mri2, MRI *mri_dst, int thresh)
{
  int     width, height, depth, x, y, z, f ;
  double    val1, val2 ;

  MRIcheckVolDims(mri1, mri2);

  width = mri1->width ;
  height = mri1->height ;
  depth = mri1->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri1, NULL) ;

  for (f = 0 ; f < mri1->nframes ; f++)
  {
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        for (x = 0 ; x < width ; x++)
        {
          MRIsampleVolumeFrame(mri1, x, y, z, f, &val1) ;
          if (val1 < thresh)
            val1 = 0 ;
          MRIsampleVolumeFrame(mri2, x, y, z, f, &val2) ;
          if (val2 < thresh)
            val2 = 0 ;
          MRIsetVoxVal(mri_dst, x, y, z, f, val1 && val2) ;
        }
      }
    }
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIor(MRI *mri1, MRI *mri2, MRI *mri_dst, int thresh)
{
  int     width, height, depth, x, y, z, f ;
  double    val1, val2 ;

  MRIcheckVolDims(mri1, mri2);

  width = mri1->width ;
  height = mri1->height ;
  depth = mri1->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri1, NULL) ;

  for (f = 0 ; f < mri1->nframes ; f++)
  {
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        for (x = 0 ; x < width ; x++)
        {
          MRIsampleVolumeFrame(mri1, x, y, z, f, &val1) ;
          if (val1 < thresh)
            val1 = 0 ;
          MRIsampleVolumeFrame(mri2, x, y, z, f, &val2) ;
          if (val2 < thresh)
            val2 = 0 ;
          MRIsetVoxVal(mri_dst, x, y, z, f, val1 || val2) ;
        }
      }
    }
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIxor(MRI *mri1, MRI *mri2, MRI *mri_dst, int t1, int t2)
{
  int     width, height, depth, x, y, z ;
  BUFTYPE *p1, *p2, *pdst, v1, v2 ;

  MRIcheckVolDims(mri1, mri2);

  if ((mri1->type != MRI_UCHAR) || (mri2->type != MRI_UCHAR))
    ErrorReturn(NULL,
                (ERROR_UNSUPPORTED, "MRIxor: inputs must be UCHAR")) ;

  width = mri1->width ;
  height = mri1->height ;
  depth = mri1->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri1, NULL) ;
  else if (mri_dst->type != MRI_UCHAR)
    ErrorReturn(NULL,
                (ERROR_UNSUPPORTED, "MRIxor: destination must be UCHAR")) ;

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      pdst = &MRIvox(mri_dst, 0, y, z) ;
      p1 = &MRIvox(mri1, 0, y, z) ;
      p2 = &MRIvox(mri2, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        v1 = *p1++ ;
        v2 = *p2++ ;
        v1 = ((v1 >= t1) && (v1 <= t2)) ;
        v2 = ((v2 >= t1) && (v2 <= t2)) ;
        *pdst++ = v1 ^ v2 ;
      }
    }
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
#define KLEN 3
MRI *
MRImorph(MRI *mri_src, MRI *mri_dst, int which)
{
  BUFTYPE kernel[KLEN][KLEN][KLEN] ;

  switch (which)
  {
  case FILTER_OPEN:
    memset(kernel, 1, sizeof(BUFTYPE)*KLEN*KLEN*KLEN) ;
    break ;
  case FILTER_CLOSE:
    break ;
  case FILTER_DILATE:
    memset(kernel, 1, sizeof(BUFTYPE)*KLEN*KLEN*KLEN) ;
    break ;
  case FILTER_ERODE:
    memset(kernel, 1, sizeof(BUFTYPE)*KLEN*KLEN*KLEN) ;
    break ;
  default:
    ErrorReturn(NULL,
                (ERROR_UNSUPPORTED, "MRImorph: unknown type %d", which)) ;
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------*/
MRI * MRIerode(MRI *mri_src, MRI *mri_dst)
{
  int     width, height, depth, x, y, z, x0, y0, z0, xi, yi, zi, same ;
  BUFTYPE *pdst, min_val, val ;

  MRIcheckVolDims(mri_src, mri_dst);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  if (mri_dst == mri_src)
  {
    same = 1 ;
    mri_dst = MRIclone(mri_src, NULL) ;
  }
  else
    same = 0 ;

  if (mri_src->type != MRI_UCHAR || mri_dst->type != MRI_UCHAR)
  {
    double fmin_val, fval ;

    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        for (x = 0 ; x < width ; x++)
        {
          fmin_val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
          for (z0 = -1 ; z0 <= 1 ; z0++)
          {
            zi = mri_src->zi[z+z0] ;
            for (y0 = -1 ; y0 <= 1 ; y0++)
            {
              yi = mri_src->yi[y+y0] ;
              for (x0 = -1 ; x0 <= 1 ; x0++)
              {
                xi = mri_src->xi[x+x0] ;
                fval = MRIgetVoxVal(mri_src, xi,yi,zi, 0) ;
                if (fval < fmin_val)
                  fmin_val = fval ;
              }
            }
          }
          MRIsetVoxVal(mri_dst, x, y, z, 0, fmin_val) ;
        }
      }
    }
  }
  else
  {
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        pdst = &MRIvox(mri_dst, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          min_val = 255 ;
          for (z0 = -1 ; z0 <= 1 ; z0++)
          {
            zi = mri_src->zi[z+z0] ;
            for (y0 = -1 ; y0 <= 1 ; y0++)
            {
              yi = mri_src->yi[y+y0] ;
              for (x0 = -1 ; x0 <= 1 ; x0++)
              {
                xi = mri_src->xi[x+x0] ;
                val = MRIvox(mri_src, xi,yi,zi) ;
                if (val < min_val)
                  min_val = val ;
              }
            }
          }
          *pdst++ = min_val ;
        }
      }
    }
  }
  if (same)
  {
    MRIcopy(mri_dst, mri_src) ;
    MRIfree(&mri_dst) ;
    mri_dst = mri_src ;
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------*/
MRI * MRIerodeThresh(MRI *mri_src, MRI *mri_intensity, double thresh, 
                     MRI *mri_dst)
{
  int     width, height, depth, x, y, z, x0, y0, z0, xi, yi, zi, same ;
  double    min_val, val ;

  MRIcheckVolDims(mri_src, mri_dst);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIcopy(mri_src, NULL) ;

  if (mri_dst == mri_src)
  {
    same = 1 ;
    mri_dst = MRIcopy(mri_src, NULL) ;
  }
  else
    same = 0 ;

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        val = MRIgetVoxVal(mri_intensity, x,y,z, 0) ;
        if (val >= thresh)
          continue ;
        min_val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        for (z0 = -1 ; z0 <= 1 ; z0++)
        {
          zi = mri_src->zi[z+z0] ;
          for (y0 = -1 ; y0 <= 1 ; y0++)
          {
            yi = mri_src->yi[y+y0] ;
            for (x0 = -1 ; x0 <= 1 ; x0++)
            {
              xi = mri_src->xi[x+x0] ;
              val = MRIgetVoxVal(mri_src, xi,yi,zi, 0) ;
              if (val < min_val)
                min_val = val ;
              }
          }
        }
        MRIsetVoxVal(mri_dst, x, y, z, 0, min_val) ;
      }
    }
  }
  if (same)
  {
    MRIcopy(mri_dst, mri_src) ;
    MRIfree(&mri_dst) ;
    mri_dst = mri_src ;
  }
  return(mri_dst) ;
}
MRI * MRIdilateThresh(MRI *mri_src, MRI *mri_intensity, double thresh, 
                     MRI *mri_dst)
{
  int     width, height, depth, x, y, z, x0, y0, z0, xi, yi, zi, same ;
  double    max_val, val ;

  MRIcheckVolDims(mri_src, mri_dst);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIcopy(mri_src, NULL) ;

  if (mri_dst == mri_src)
  {
    same = 1 ;
    mri_dst = MRIcopy(mri_src, NULL) ;
  }
  else
    same = 0 ;

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        val = MRIgetVoxVal(mri_intensity, x,y,z, 0) ;
        if (val < thresh)
          continue ;
        max_val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        for (z0 = -1 ; z0 <= 1 ; z0++)
        {
          zi = mri_src->zi[z+z0] ;
          for (y0 = -1 ; y0 <= 1 ; y0++)
          {
            yi = mri_src->yi[y+y0] ;
            for (x0 = -1 ; x0 <= 1 ; x0++)
            {
              xi = mri_src->xi[x+x0] ;
              val = MRIgetVoxVal(mri_src, xi,yi,zi, 0) ;
              if (val > max_val)
                max_val = val ;
              }
          }
        }
        MRIsetVoxVal(mri_dst, x, y, z, 0, max_val) ;
      }
    }
  }
  if (same)
  {
    MRIcopy(mri_dst, mri_src) ;
    MRIfree(&mri_dst) ;
    mri_dst = mri_src ;
  }
  return(mri_dst) ;
}
MRI * MRIdilate6Thresh(MRI *mri_src, MRI *mri_intensity, double thresh, 
                       MRI *mri_dst)
{
  int     width, height, depth, x, y, z, x0, y0, z0, xi, yi, zi, same ;
  double    max_val, val ;

  MRIcheckVolDims(mri_src, mri_dst);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIcopy(mri_src, NULL) ;

  if (mri_dst == mri_src)
  {
    same = 1 ;
    mri_dst = MRIcopy(mri_src, NULL) ;
  }
  else
    same = 0 ;

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        val = MRIgetVoxVal(mri_intensity, x,y,z, 0) ;
        if (val < thresh)
          continue ;
        max_val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        for (z0 = -1 ; z0 <= 1 ; z0++)
        {
          zi = mri_src->zi[z+z0] ;
          for (y0 = -1 ; y0 <= 1 ; y0++)
          {
            yi = mri_src->yi[y+y0] ;
            for (x0 = -1 ; x0 <= 1 ; x0++)
            {
              if (fabs(x0) + fabs(y0) + fabs(z0) != 1)
                continue ;
              xi = mri_src->xi[x+x0] ;
              val = MRIgetVoxVal(mri_src, xi,yi,zi, 0) ;
              if (val > max_val)
                max_val = val ;
              }
          }
        }
        MRIsetVoxVal(mri_dst, x, y, z, 0, max_val) ;
      }
    }
  }
  if (same)
  {
    MRIcopy(mri_dst, mri_src) ;
    MRIfree(&mri_dst) ;
    mri_dst = mri_src ;
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------*/
MRI *MRIerodeZero(MRI *mri_src, MRI *mri_dst)
{
  int     width, height, depth, x, y, z, x0, y0, z0, xi, yi, zi, same ;
  BUFTYPE *pdst, min_val, val ;

  MRIcheckVolDims(mri_src, mri_dst);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  if (mri_dst == mri_src)
  {
    same = 1 ;
    mri_dst = MRIclone(mri_src, NULL) ;
  }
  else
    same = 0 ;

  if (mri_src->type != MRI_UCHAR || mri_dst->type != MRI_UCHAR)
  {
    double fmin_val, fval ;

    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        for (x = 0 ; x < width ; x++)
        {
          fmin_val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
          if (x == Gx && y == Gy && z == Gz)
            DiagBreak() ;
          for (z0 = -1 ; z0 <= 1 ; z0++)
          {
            zi = mri_src->zi[z+z0] ;
            for (y0 = -1 ; y0 <= 1 ; y0++)
            {
              yi = mri_src->yi[y+y0] ;
              for (x0 = -1 ; x0 <= 1 ; x0++)
              {
                xi = mri_src->xi[x+x0] ;
                fval = MRIgetVoxVal(mri_src, xi,yi,zi, 0) ;
                if (FZERO(fval))
                  fmin_val = fval ;
              }
            }
          }
          MRIsetVoxVal(mri_dst, x, y, z, 0, fmin_val) ;
        }
      }
    }
  }
  else
  {
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        pdst = &MRIvox(mri_dst, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          min_val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
          for (z0 = -1 ; z0 <= 1 ; z0++)
          {
            zi = mri_src->zi[z+z0] ;
            for (y0 = -1 ; y0 <= 1 ; y0++)
            {
              yi = mri_src->yi[y+y0] ;
              for (x0 = -1 ; x0 <= 1 ; x0++)
              {
                xi = mri_src->xi[x+x0] ;
                val = MRIvox(mri_src, xi,yi,zi) ;
                if (val == 0)
                  min_val = val ;
              }
            }
          }
          *pdst++ = min_val ;
        }
      }
    }
  }
  if (same)
  {
    MRIcopy(mri_dst, mri_src) ;
    MRIfree(&mri_dst) ;
    mri_dst = mri_src ;
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------*/
MRI * MRIerode2D(MRI *mri_src, MRI *mri_dst)
{
  int     width, height, depth, x, y, z, x0, y0, xi, yi, same ;
  double fmin_val, fval ;

  MRIcheckVolDims(mri_src, mri_dst);

  width  = mri_src->width ;
  height = mri_src->height ;
  depth  = mri_src->depth ;

  if(!mri_dst)  mri_dst = MRIclone(mri_src, NULL) ;

  if (mri_dst == mri_src){
    same = 1 ;
    mri_dst = MRIclone(mri_src, NULL) ;
  }
  else same = 0 ;

  for (z = 0 ; z < depth ; z++)    {
    for (y = 0 ; y < height ; y++)  {
      for (x = 0 ; x < width ; x++) {
	fmin_val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
	for (y0 = -1 ; y0 <= 1 ; y0++){
	  yi = mri_src->yi[y+y0] ;
	  for (x0 = -1 ; x0 <= 1 ; x0++){
	    xi = mri_src->xi[x+x0] ;
	    fval = MRIgetVoxVal(mri_src, xi,yi,z, 0) ;
	    if (fval < fmin_val) fmin_val = fval ;
	  } // xi
	} // yi
	MRIsetVoxVal(mri_dst, x, y, z, 0, fmin_val) ;
      } // x
    } // y
  } // z
  if(same){
    // dst and src point to the same MRI struct
    MRIcopy(mri_dst, mri_src) ;
    MRIfree(&mri_dst) ;
    mri_dst = mri_src ;
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIdilateThreshLabel(MRI *mri_src, MRI *mri_val, MRI *mri_dst, int label,
                     int niter, int thresh)
{
  int     width, height, depth, x, y, z, x0, y0, z0, xi, yi, zi,
  xmin, xmax, ymin, ymax, zmin, zmax, i, nadded ;
  BUFTYPE *psrc, out_val, val ;
  MRI     *mri_tmp = NULL ;

  MRIcheckVolDims(mri_src, mri_val);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  /* get everything outside of bounding box */
  mri_dst = MRIcopy(mri_src, mri_dst) ;

  for (i = 0 ; i < niter ; i++)
  {
    nadded = 0 ;
    mri_tmp = MRIcopy(mri_dst, mri_tmp) ; /* will allocate first time */
    xmax = 0 ;
    xmin = width-1 ;
    ymax = 0 ;
    ymin = height-1 ;
    zmax = 0 ;
    zmin = depth-1 ;
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        psrc = &MRIvox(mri_src, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          if (*psrc++ == label)
          {
            if (x-1 < xmin)
              xmin = x-1 ;
            if (x+1 > xmax)
              xmax = x+1 ;
            if (y-1 < ymin)
              ymin = y-1 ;
            if (y+1 > ymax)
              ymax = y+1 ;
            if (z-1 < zmin)
              zmin = z-1 ;
            if (z+1 > zmax)
              zmax = z+1 ;
          }
        }
      }
    }
    xmin = MAX(0, xmin) ;
    xmax = MIN(width-1, xmax) ;
    ymin = MAX(0, ymin) ;
    ymax = MIN(height-1, ymax) ;
    zmin = MAX(0, zmin) ;
    zmax = MIN(depth-1, zmax) ;
    for (z = zmin ; z <= zmax ; z++)
    {
      for (y = ymin ; y <= ymax ; y++)
      {
        for (x = xmin ; x <= xmax ; x++)
        {
          out_val = MRIvox(mri_tmp, x, y, z) ;
          if (MRIvox(mri_val, x, y, z) >= thresh)
          {
            for (z0 = -1 ; z0 <= 1 ; z0++)
            {
              zi = mri_tmp->zi[z+z0] ;
              for (y0 = -1 ; y0 <= 1 ; y0++)
              {
                yi = mri_tmp->yi[y+y0] ;
                for (x0 = -1 ; x0 <= 1 ; x0++)
                {
                  xi = mri_tmp->xi[x+x0] ;
                  val = MRIvox(mri_tmp, xi,yi,zi) ;
                  if (val == label)
                    out_val = label ;
                }
              }
            }
            if (out_val == label)
            {
              if (xi < xmin)
                xmin = xi ;
              if (xi > xmax)
                xmax = xi ;
              if (yi < ymin)
                ymin = yi ;
              if (yi > ymax)
                ymax = yi ;
              if (zi < zmin)
                zmin = zi ;
              if (zi > zmax)
                zmax = zi ;
              if (MRIvox(mri_dst, x,y,z) != label)
                nadded++ ;
            }
          }
          MRIvox(mri_dst, x,y,z) = out_val ;
        }
      }
    }
    if (nadded == 0)
      break ;
  }
  MRIfree(&mri_tmp) ;

  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIdilate6ThreshLabel(MRI *mri_src, MRI *mri_val, MRI *mri_dst, int label,
                      int niter, int thresh)
{
  int     width, height, depth, x, y, z, x0, y0, z0, xi, yi, zi,
  xmin, xmax, ymin, ymax, zmin, zmax, i, nadded ;
  BUFTYPE *psrc, out_val, val ;
  MRI     *mri_tmp = NULL ;

  MRIcheckVolDims(mri_src, mri_val);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  /* get everything outside of bounding box */
  mri_dst = MRIcopy(mri_src, mri_dst) ;

  for (i = 0 ; i < niter ; i++)
  {
    nadded = 0 ;
    mri_tmp = MRIcopy(mri_dst, mri_tmp) ; /* will allocate first time */
    xmax = 0 ;
    xmin = width-1 ;
    ymax = 0 ;
    ymin = height-1 ;
    zmax = 0 ;
    zmin = depth-1 ;
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        psrc = &MRIvox(mri_src, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          if (*psrc++ == label)
          {
            if (x-1 < xmin)
              xmin = x-1 ;
            if (x+1 > xmax)
              xmax = x+1 ;
            if (y-1 < ymin)
              ymin = y-1 ;
            if (y+1 > ymax)
              ymax = y+1 ;
            if (z-1 < zmin)
              zmin = z-1 ;
            if (z+1 > zmax)
              zmax = z+1 ;
          }
        }
      }
    }
    xmin = MAX(0, xmin) ;
    xmax = MIN(width-1, xmax) ;
    ymin = MAX(0, ymin) ;
    ymax = MIN(height-1, ymax) ;
    zmin = MAX(0, zmin) ;
    zmax = MIN(depth-1, zmax) ;
    for (z = zmin ; z <= zmax ; z++)
    {
      for (y = ymin ; y <= ymax ; y++)
      {
        for (x = xmin ; x <= xmax ; x++)
        {
          out_val = MRIvox(mri_tmp, x, y, z) ;
          if (MRIvox(mri_val, x, y, z) >= thresh)
          {
            for (z0 = -1 ; z0 <= 1 ; z0++)
            {
              zi = mri_tmp->zi[z+z0] ;
              for (y0 = -1 ; y0 <= 1 ; y0++)
              {
                yi = mri_tmp->yi[y+y0] ;
                for (x0 = -1 ; x0 <= 1 ; x0++)
                {
                  xi = mri_tmp->xi[x+x0] ;
                  val = MRIvox(mri_tmp, xi,yi,zi) ;
                  if (val == label)
                    out_val = label ;
                }
              }
            }
            if (out_val == label)
            {
              if (xi < xmin)
                xmin = xi ;
              if (xi > xmax)
                xmax = xi ;
              if (yi < ymin)
                ymin = yi ;
              if (yi > ymax)
                ymax = yi ;
              if (zi < zmin)
                zmin = zi ;
              if (zi > zmax)
                zmax = zi ;
              if (MRIvox(mri_dst, x,y,z) != label)
                nadded++ ;
            }
          }
          MRIvox(mri_dst, x,y,z) = out_val ;
        }
      }
    }
    if (nadded == 0)
      break ;
  }
  MRIfree(&mri_tmp) ;

  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIdilateInvThreshLabel(MRI *mri_src, MRI *mri_val, MRI *mri_dst, int label,
                        int niter, int thresh)
{
  int     width, height, depth, x, y, z, x0, y0, z0, xi, yi, zi,
  xmin, xmax, ymin, ymax, zmin, zmax, i, nadded ;
  BUFTYPE *psrc, out_val, val ;
  MRI     *mri_tmp = NULL ;

  MRIcheckVolDims(mri_src, mri_val);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  /* get everything outside of bounding box */
  mri_dst = MRIcopy(mri_src, mri_dst) ;

  for (i = 0 ; i < niter ; i++)
  {
    nadded = 0 ;
    mri_tmp = MRIcopy(mri_dst, mri_tmp) ; /* will allocate first time */
    xmax = 0 ;
    xmin = width-1 ;
    ymax = 0 ;
    ymin = height-1 ;
    zmax = 0 ;
    zmin = depth-1 ;
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        psrc = &MRIvox(mri_src, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          if (*psrc++ == label)
          {
            if (x-1 < xmin)
              xmin = x-1 ;
            if (x+1 > xmax)
              xmax = x+1 ;
            if (y-1 < ymin)
              ymin = y-1 ;
            if (y+1 > ymax)
              ymax = y+1 ;
            if (z-1 < zmin)
              zmin = z-1 ;
            if (z+1 > zmax)
              zmax = z+1 ;
          }
        }
      }
    }
    xmin = MAX(0, xmin) ;
    xmax = MIN(width-1, xmax) ;
    ymin = MAX(0, ymin) ;
    ymax = MIN(height-1, ymax) ;
    zmin = MAX(0, zmin) ;
    zmax = MIN(depth-1, zmax) ;
    for (z = zmin ; z <= zmax ; z++)
    {
      for (y = ymin ; y <= ymax ; y++)
      {
        for (x = xmin ; x <= xmax ; x++)
        {
          out_val = MRIvox(mri_tmp, x, y, z) ;
          if (MRIvox(mri_val, x, y, z) <= thresh)
          {
            for (z0 = -1 ; z0 <= 1 ; z0++)
            {
              zi = mri_tmp->zi[z+z0] ;
              for (y0 = -1 ; y0 <= 1 ; y0++)
              {
                yi = mri_tmp->yi[y+y0] ;
                for (x0 = -1 ; x0 <= 1 ; x0++)
                {
                  xi = mri_tmp->xi[x+x0] ;
                  val = MRIvox(mri_tmp, xi,yi,zi) ;
                  if (val == label)
                    out_val = label ;
                }
              }
            }
            if (out_val == label)
            {
              if (xi < xmin)
                xmin = xi ;
              if (xi > xmax)
                xmax = xi ;
              if (yi < ymin)
                ymin = yi ;
              if (yi > ymax)
                ymax = yi ;
              if (zi < zmin)
                zmin = zi ;
              if (zi > zmax)
                zmax = zi ;
              if (MRIvox(mri_dst, x,y,z) != label)
                nadded++ ;
            }
          }
          MRIvox(mri_dst, x,y,z) = out_val ;
        }
      }
    }
    if (nadded == 0)
      break ;
  }
  MRIfree(&mri_tmp) ;

  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIdilateLabelUchar(MRI *mri_src, MRI *mri_dst, int label, int niter)
{
  int     width, height, depth, x, y, z, x0, y0, z0, xi, yi, zi,
  xmin, xmax, ymin, ymax, zmin, zmax, i;
  BUFTYPE *psrc, out_val, val ;
  MRI     *mri_tmp = NULL ;

  MRIcheckVolDims(mri_src, mri_dst);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  /* get everything outside of bounding box */
  mri_dst = MRIcopy(mri_src, mri_dst) ;

  for (i = 0 ; i < niter ; i++)
  {
    mri_tmp = MRIcopy(mri_dst, mri_tmp) ; /* will allocate first time */
    xmax = 0 ;
    xmin = width-1 ;
    ymax = 0 ;
    ymin = height-1 ;
    zmax = 0 ;
    zmin = depth-1 ;
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        psrc = &MRIvox(mri_src, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          if (*psrc++ == label)
          {
            if (x-1 < xmin)
              xmin = x-1 ;
            if (x+1 > xmax)
              xmax = x+1 ;
            if (y-1 < ymin)
              ymin = y-1 ;
            if (y+1 > ymax)
              ymax = y+1 ;
            if (z-1 < zmin)
              zmin = z-1 ;
            if (z+1 > zmax)
              zmax = z+1 ;
          }
        }
      }
    }
    xmin = MAX(0, xmin) ;
    xmax = MIN(width-1, xmax) ;
    ymin = MAX(0, ymin) ;
    ymax = MIN(height-1, ymax) ;
    zmin = MAX(0, zmin) ;
    zmax = MIN(depth-1, zmax) ;
    for (z = zmin ; z <= zmax ; z++)
    {
      for (y = ymin ; y <= ymax ; y++)
      {
        for (x = xmin ; x <= xmax ; x++)
        {
          out_val = MRIvox(mri_tmp, x, y, z) ;
          for (z0 = -1 ; z0 <= 1 ; z0++)
          {
            if (out_val == label)
              break ;
            zi = mri_tmp->zi[z+z0] ;
            for (y0 = -1 ; y0 <= 1 ; y0++)
            {
              if (out_val == label)
                break ;
              yi = mri_tmp->yi[y+y0] ;
              for (x0 = -1 ; x0 <= 1 ; x0++)
              {
                if (out_val == label)
                  break ;
                xi = mri_tmp->xi[x+x0] ;
                val = MRIvox(mri_tmp, xi,yi,zi) ;
                if (val == label)
                  out_val = label ;
              }
            }
          }
          MRIvox(mri_dst, x,y,z) = out_val ;
        }
      }
    }
  }
  MRIfree(&mri_tmp) ;

  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIdilateLabel(MRI *mri_src, MRI *mri_dst, int label, int niter)
{
  int     width, height, depth, x, y, z, x0, y0, z0, xi, yi, zi,
  xmin, xmax, ymin, ymax, zmin, zmax, i, f ;
  MRI     *mri_tmp = NULL ;
  double    out_val, val ;

  MRIcheckVolDims(mri_src, mri_dst);

  if (mri_src->type == MRI_UCHAR)
    return(MRIdilateLabelUchar(mri_src, mri_dst, label, niter)) ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  /* get everything outside of bounding box */
  mri_dst = MRIcopy(mri_src, mri_dst) ;
  
  for (i = 0 ; i < niter ; i++)
  {
    mri_tmp = MRIcopy(mri_dst, mri_tmp) ; /* will allocate first time */
    xmax = 0 ;
    xmin = width-1 ;
    ymax = 0 ;
    ymin = height-1 ;
    zmax = 0 ;
    zmin = depth-1 ;
    for (f = 0 ; f < mri_src->nframes ; f++)
    {
      for (z = 0 ; z < depth ; z++)
      {
        for (y = 0 ; y < height ; y++)
        {
          for (x = 0 ; x < width ; x++)
          {
            val = MRIgetVoxVal(mri_tmp, x, y, z, f) ;
            if (val == label)
            {
              if (x-1 < xmin)
                xmin = x-1 ;
              if (x+1 > xmax)
                xmax = x+1 ;
              if (y-1 < ymin)
                ymin = y-1 ;
              if (y+1 > ymax)
                ymax = y+1 ;
              if (z-1 < zmin)
                zmin = z-1 ;
              if (z+1 > zmax)
                zmax = z+1 ;
            }
          }
        }
      }
    }
    xmin = MAX(0, xmin) ;
    xmax = MIN(width-1, xmax) ;
    ymin = MAX(0, ymin) ;
    ymax = MIN(height-1, ymax) ;
    zmin = MAX(0, zmin) ;
    zmax = MIN(depth-1, zmax) ;
    for (f = 0 ; f < mri_src->nframes ; f++)
    {
      for (z = zmin ; z <= zmax ; z++)
      {
        for (y = ymin ; y <= ymax ; y++)
        {
          for (x = xmin ; x <= xmax ; x++)
          {
            if (x == Gx && y == Gy && z == Gz)
              DiagBreak() ;
            out_val = MRIgetVoxVal(mri_tmp, x, y, z, f) ;
            for (z0 = -1 ; z0 <= 1 ; z0++)
            {
              if (out_val == label)
                break ;
              zi = mri_tmp->zi[z+z0] ;
              for (y0 = -1 ; y0 <= 1 ; y0++)
              {
                if (out_val == label)
                  break ;
                yi = mri_tmp->yi[y+y0] ;
                for (x0 = -1 ; x0 <= 1 ; x0++)
                {
                  if (out_val == label)
                    break ;
                  xi = mri_tmp->xi[x+x0] ;
                  val = MRIgetVoxVal(mri_tmp,xi,yi,zi,f) ;
                  if (val == label)
                  {
                    out_val = label ;
                    break ;
                  }
                }
              }
            }
            if (x == Gx && y == Gy && z == Gz)
              DiagBreak() ;
            MRIsetVoxVal(mri_dst, x, y, z, f, out_val) ;
          }
        }
      }
    }
  }
  MRIfree(&mri_tmp) ;

  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIdilateUchar(MRI *mri_src, MRI *mri_dst)
{
  int     width, height, depth, x, y, z, x0, y0, z0, xi, yi, zi, same,
  xmin, xmax, ymin, ymax, zmin, zmax;
  BUFTYPE *psrc, max_val, val ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  MRIcheckVolDims(mri_src, mri_dst);

  if (mri_dst == mri_src)
  {
    same = 1 ;
    mri_dst = MRIclone(mri_src, NULL) ;
  }
  else
    same = 0 ;

  xmax = 0 ;
  xmin = width-1 ;
  ymax = 0 ;
  ymin = height-1 ;
  zmax = 0 ;
  zmin = depth-1 ;
  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      psrc = &MRIvox(mri_src, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        if (*psrc++ > 0)
        {
          if (x-1 < xmin)
            xmin = x-1 ;
          if (x+1 > xmax)
            xmax = x+1 ;
          if (y-1 < ymin)
            ymin = y-1 ;
          if (y+1 > ymax)
            ymax = y+1 ;
          if (z-1 < zmin)
            zmin = z-1 ;
          if (z+1 > zmax)
            zmax = z+1 ;
        }
      }
    }
  }
  xmin = MAX(0, xmin) ;
  xmax = MIN(width-1, xmax) ;
  ymin = MAX(0, ymin) ;
  ymax = MIN(height-1, ymax) ;
  zmin = MAX(0, zmin) ;
  zmax = MIN(depth-1, zmax) ;
  for (z = zmin ; z <= zmax ; z++)
  {
    for (y = ymin ; y <= ymax ; y++)
    {
      for (x = xmin ; x <= xmax ; x++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        max_val = 0 ;
        for (z0 = -1 ; z0 <= 1 ; z0++)
        {
          zi = mri_src->zi[z+z0] ;
          for (y0 = -1 ; y0 <= 1 ; y0++)
          {
            yi = mri_src->yi[y+y0] ;
            for (x0 = -1 ; x0 <= 1 ; x0++)
            {
              xi = mri_src->xi[x+x0] ;
              val = MRIvox(mri_src, xi,yi,zi) ;
              if (val > max_val)
                max_val = val ;
            }
          }
        }
        MRIvox(mri_dst, x,y,z) = max_val ;
      }
    }
  }
  if (same)
  {
    MRIcopy(mri_dst, mri_src) ;
    MRIfree(&mri_dst) ;
    mri_dst = mri_src ;
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIdilate(MRI *mri_src, MRI *mri_dst)
{
  int     width, height, depth, x, y, z, x0, y0, z0, xi, yi, zi, same,
  xmin, xmax, ymin, ymax, zmin, zmax, f;
  double    val, max_val ;

  if (mri_src->type == MRI_UCHAR)
    return(MRIdilateUchar(mri_src, mri_dst)) ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  MRIcheckVolDims(mri_src, mri_dst);

  if (mri_dst == mri_src)
  {
    same = 1 ;
    mri_dst = MRIclone(mri_src, NULL) ;
  }
  else
    same = 0 ;

#if 1
  xmax = 0 ;
  xmin = width-1 ;
  ymax = 0 ;
  ymin = height-1 ;
  zmax = 0 ;
  zmin = depth-1 ;
  for (f = 0 ; f < mri_src->nframes ; f++)
  {
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        for (x = 0 ; x < width ; x++)
        {
          MRIsampleVolumeFrameType
          (mri_src, x, y, z, f, SAMPLE_NEAREST, &val) ;
          if (val > 0)
          {
            if (x-1 < xmin)
              xmin = x-1 ;
            if (x+1 > xmax)
              xmax = x+1 ;
            if (y-1 < ymin)
              ymin = y-1 ;
            if (y+1 > ymax)
              ymax = y+1 ;
            if (z-1 < zmin)
              zmin = z-1 ;
            if (z+1 > zmax)
              zmax = z+1 ;
          }
        }
      }
    }
  }
  xmin = MAX(0, xmin) ;
  xmax = MIN(width-1, xmax) ;
  ymin = MAX(0, ymin) ;
  ymax = MIN(height-1, ymax) ;
  zmin = MAX(0, zmin) ;
  zmax = MIN(depth-1, zmax) ;
#else
  xmin = 0 ;
  xmax = width-1 ;
  ymin = 0 ;
  ymax = height-1 ;
  zmin = 0 ;
  zmax = depth-1 ;
#endif
  for (f = 0 ; f < mri_src->nframes ; f++)
  {
    for (z = zmin ; z <= zmax ; z++)
    {
      for (y = ymin ; y <= ymax ; y++)
      {
        for (x = xmin ; x <= xmax ; x++)
        {
          max_val = 0 ;
          for (z0 = -1 ; z0 <= 1 ; z0++)
          {
            zi = mri_src->zi[z+z0] ;
            for (y0 = -1 ; y0 <= 1 ; y0++)
            {
              yi = mri_src->yi[y+y0] ;
              for (x0 = -1 ; x0 <= 1 ; x0++)
              {
                xi = mri_src->xi[x+x0] ;
                MRIsampleVolumeFrameType
                (mri_src,xi,yi,zi,f,SAMPLE_NEAREST,&val)  ;
                if (val > max_val)
                  max_val = val ;
              }
            }
          }
          MRIsetVoxVal(mri_dst, x,y,z, f, max_val) ;
        }
      }
    }
  }
  if (same)
  {
    MRIcopy(mri_dst, mri_src) ;
    MRIfree(&mri_dst) ;
    mri_dst = mri_src ;
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIopen(MRI *mri_src, MRI *mri_dst)
{
  MRI *mri_tmp ;

  mri_tmp = MRIerode(mri_src, NULL) ;
  mri_dst = MRIdilate(mri_tmp, mri_dst) ;
  MRIfree(&mri_tmp) ;
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIclose(MRI *mri_src, MRI *mri_dst)
{
  MRI *mri_tmp ;

  mri_tmp = MRIdilate(mri_src, NULL) ;
  mri_dst = MRIerode(mri_tmp, mri_dst) ;
  MRIfree(&mri_tmp) ;
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIerode6(MRI *mri_src, MRI *mri_dst)
{
  int     width, height, depth, x, y, z, x1, y1, z1, xi, yi, zi;
  float    min_val, val ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  MRIcheckVolDims(mri_src, mri_dst);

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        xi = mri_src->xi[x] ;
        yi = mri_src->yi[y] ;
        min_val = 1e10 ;
        for (z1 = -1 ; z1 <= 1 ; z1++)
        {
          zi = mri_src->zi[z+z1] ;
          val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
          if (val < min_val)
            min_val = val ;
        }
        zi = mri_src->zi[z] ;
        for (y1 = -1 ; y1 <= 1 ; y1++)
        {
          if (!y1)    /* already done */
            continue ;
          yi = mri_src->yi[y+y1] ;
          val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
          if (val < min_val)
            min_val = val ;
        }
        yi = mri_src->yi[y] ;
        for (x1 = -1 ; x1 <= 1 ; x1++)
        {
          if (!y1)    /* already done */
            continue ;
          xi = mri_src->xi[x+x1] ;
          val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
          if (val < min_val)
            min_val = val ;
        }
        MRIsetVoxVal(mri_dst, x, y, z, 0, min_val) ;
      }
    }
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIdilate6(MRI *mri_src, MRI *mri_dst)
{
  int     width, height, depth, x, y, z, x1, y1, z1, xi, yi, zi;
  float   max_val, val ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  MRIcheckVolDims(mri_src, mri_dst);

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        xi = mri_src->xi[x] ;
        yi = mri_src->yi[y] ;
        max_val = -1e10 ;
        for (z1 = -1 ; z1 <= 1 ; z1++)
        {
          zi = mri_src->zi[z+z1] ;
          val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
          if (val > max_val)
            max_val = val ;
        }
        zi = mri_src->zi[z] ;
        for (y1 = -1 ; y1 <= 1 ; y1++)
        {
          if (!y1)    /* already done */
            continue ;
          yi = mri_src->yi[y+y1] ;
          val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
          if (val > max_val)
            max_val = val ;
        }
        yi = mri_src->yi[y] ;
        for (x1 = -1 ; x1 <= 1 ; x1++)
        {
          if (!y1)    /* already done */
            continue ;
          xi = mri_src->xi[x+x1] ;
          val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
          if (val > max_val)
            max_val = val ;
        }
        MRIsetVoxVal(mri_dst, x, y, z, 0, max_val) ;
      }
    }
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIopen6(MRI *mri_src, MRI *mri_dst)
{
  MRI *mri_tmp ;

  mri_tmp = MRIerode6(mri_src, NULL) ;
  mri_dst = MRIdilate6(mri_tmp, mri_dst) ;
  MRIfree(&mri_tmp) ;
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIclose6(MRI *mri_src, MRI *mri_dst)
{
  MRI *mri_tmp ;

  mri_tmp = MRIdilate6(mri_src, NULL) ;
  mri_dst = MRIerode6(mri_tmp, mri_dst) ;
  MRIfree(&mri_tmp) ;
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIerodeRegion(MRI *mri_src, MRI *mri_dst, int wsize, MRI_REGION *region)
{
  int     width, height, depth, x, y, z, whalf, x0, y0, z0, val,
  xk, yk, zk, xi, yi, zi, min_val ;
  BUFTYPE *pdst ;

  MRIcheckVolDims(mri_src, mri_dst);

  whalf = wsize/2 ;
  width = region->x + region->dx ;
  if (width > mri_src->width)
    width = mri_src->width ;
  height = region->y + region->dy ;
  if (height > mri_src->height)
    height = mri_src->height ;
  depth = region->z + region->dz ;
  if (depth > mri_src->depth)
    depth = mri_src->depth ;
  x0 = region->x ;
  if (x0 < 0)
    x0 = 0 ;
  y0 = region->y ;
  if (y0 < 0)
    y0 = 0 ;
  z0 = region->z ;
  if (z0 < 0)
    z0 = 0 ;

  if (!mri_dst)
  {
    int  w, h, d ;

    w = width - region->x ;
    h = height - region->y ;
    d = depth - region->z ;
    mri_dst = MRIalloc(w, h, d, MRI_UCHAR) ;
    MRIcopyHeader(mri_src, mri_dst) ;
    mri_dst->xstart = mri_src->xstart + region->x * mri_src->xsize ;
    mri_dst->ystart = mri_src->ystart + region->y * mri_src->ysize ;
    mri_dst->zstart = mri_src->zstart + region->z * mri_src->zsize ;
    mri_dst->xend = mri_src->xstart + w * mri_src->xsize ;
    mri_dst->yend = mri_src->ystart + h * mri_src->ysize ;
    mri_dst->zend = mri_src->zstart + d * mri_src->zsize ;
  }

  for (z = z0 ; z < depth ; z++)
  {
    for (y = y0 ; y < height ; y++)
    {
      pdst = &MRIvox(mri_dst, 0, y-y0, z-z0) ;
      for (x = x0 ; x < width ; x++)
      {
        min_val = 255 ;
        for (zk = -whalf ; zk <= whalf ; zk++)
        {
          zi = mri_src->zi[z+zk] ;
          for (yk = -whalf ; yk <= whalf ; yk++)
          {
            yi = mri_src->yi[y+yk] ;
            for (xk = -whalf ; xk <= whalf ; xk++)
            {
              xi = mri_src->xi[x+xk] ;
              val = (int)MRIvox(mri_src, xi, yi, zi) ;
              if (val < min_val)
                min_val = val ;
            }
          }
        }
        *pdst++ = min_val ;
      }
    }
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIcomputeResidual(MRI *mri1, MRI *mri2, MRI *mri_dst, int t1, int t2)
{
  int     width, height, depth, x, y, z ;
  BUFTYPE *p1, *p2, *pdst, v1, v2, out ;

  if ((mri1->type != MRI_UCHAR) || (mri2->type != MRI_UCHAR))
    ErrorReturn(NULL,
                (ERROR_UNSUPPORTED, "MRIxor: inputs must be UCHAR")) ;

  width = mri1->width ;
  height = mri1->height ;
  depth = mri1->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri1, NULL) ;
  else if (mri_dst->type != MRI_UCHAR)
    ErrorReturn(NULL,
                (ERROR_UNSUPPORTED,
                 "MRIcomputeResidual: destination must be UCHAR")) ;

  MRIcheckVolDims(mri1, mri2);

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      pdst = &MRIvox(mri_dst, 0, y, z) ;
      p1 = &MRIvox(mri1, 0, y, z) ;
      p2 = &MRIvox(mri2, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        v1 = *p1++ ;
        v2 = *p2++ ;
        v1 = ((v1 >= t1) && (v1 <= t2)) ;
        v2 = ((v2 >= t1) && (v2 <= t2)) ;
        if (v1 && !v2)
          out = 255 ;   /* v2 off and v1 on - make it 'positive' */
        else if (!v1 && v2)
          out = 0 ;     /* v2 on and v1 off - make it 'negative' */
        else
          out = 128 ;   /* both on or both off */
        *pdst++ = out ;
      }
    }
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIminmax(MRI *mri_src, MRI *mri_dst, MRI *mri_dir, int wsize)
{
  int     width, height, depth, x, y, z, x0, y0, z0, xi, yi, zi, whalf, offset;
  float   max_val, val, min_val ;
  float   fmin, fmax ;

  MRIcheckVolDims(mri_src, mri_dst);

  MRIvalRange(mri_src, &fmin, &fmax) ;
  whalf = (wsize-1) / 2 ;
  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        offset = MRIgetVoxVal(mri_dir, x, y, z, 0);
        max_val = fmin ;
        min_val = fmax ;
        if (offset != OFFSET_ZERO) for (z0 = -whalf ; z0 <= whalf ; z0++)
          {
            zi = mri_src->zi[z+z0] ;
            for (y0 = -whalf ; y0 <= whalf ; y0++)
            {
              yi = mri_src->yi[y+y0] ;
              for (x0 = -whalf ; x0 <= whalf ; x0++)
              {
                xi = mri_src->xi[x+x0] ;
                val = MRIgetVoxVal(mri_src, xi,yi,zi,0) ;
                if (val > max_val)
                  max_val = val ;
                if (val < min_val)
                  min_val = val ;
              }
            }
          }
        switch (offset)
        {
        case OFFSET_GRADIENT_DIRECTION:
          val = max_val ;
          break ;
        case OFFSET_NEGATIVE_GRADIENT_DIRECTION:
          val = min_val ;
          break ;
        default:
        case OFFSET_ZERO:
          val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
          break ;
        }
        MRIsetVoxVal(mri_dst, x, y, z, 0, val) ;
      }
    }
  }
  return(mri_dst) ;
}
// replace a range of values with a specified one
MRI   *
MRIreplaceValueRange(MRI *mri_src, MRI *mri_dst,float low_in_val, float hi_in_val, float out_val) 
{
  int     width, height, depth, x, y, z;
  float   val ;

  MRIcheckVolDims(mri_src, mri_dst);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        if (val >= low_in_val && val <= hi_in_val)
          val = out_val ;
        MRIsetVoxVal(mri_dst, x, y, z, 0, val) ;
      }
    }
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  replace a single value with a specified one
  ------------------------------------------------------*/
MRI *
MRIreplaceValues(MRI *mri_src, MRI *mri_dst, float in_val, float out_val)
{
  int     width, height, depth, x, y, z, frame ;
  float   val ;

  MRIcheckVolDims(mri_src, mri_dst);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  if (mri_src->type == MRI_UCHAR && mri_dst->type == MRI_UCHAR)
    return(MRIreplaceValuesUchar
           (mri_src, mri_dst, (BUFTYPE)nint(in_val), (BUFTYPE)nint(out_val))) ;
  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        for (frame = 0 ; frame < mri_src->nframes ; frame++)
        {
          val = MRIgetVoxVal(mri_src, x, y, z, frame) ;
          if (FEQUAL(val, in_val)) val = out_val ;
          MRIsetVoxVal(mri_dst, x, y, z, frame, val) ;
        }
      }
    }
  }
  return(mri_dst) ;
}
MRI *
MRIreplaceValuesOnly(MRI *mri_src, MRI *mri_dst, float in_val, float out_val)
{
  int     width, height, depth, x, y, z, frame ;
  float   val ;

  MRIcheckVolDims(mri_src, mri_dst);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  if (mri_src->type == MRI_UCHAR && mri_dst->type == MRI_UCHAR)
    return(MRIreplaceValuesUchar
           (mri_src, mri_dst, (BUFTYPE)nint(in_val), (BUFTYPE)nint(out_val))) ;
  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        for (frame = 0 ; frame < mri_src->nframes ; frame++)
        {
          val = MRIgetVoxVal(mri_src, x, y, z, frame) ;
          if (FEQUAL(val, in_val))
          {
            val = out_val ;
            MRIsetVoxVal(mri_dst, x, y, z, frame, val) ;
          }
        }
      }
    }
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIreplaceValuesUchar(MRI *mri_src, MRI *mri_dst,
                      BUFTYPE in_val, BUFTYPE out_val)
{
  int     width, height, depth, x, y, z;
  BUFTYPE  val ;
  BUFTYPE *psrc, *pdst ;

  MRIcheckVolDims(mri_src, mri_dst);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  if (mri_src->type != MRI_UCHAR || mri_dst->type != MRI_UCHAR)
    return(MRIreplaceValues(mri_src, mri_dst, (float)in_val, (float)out_val)) ;

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      psrc = &MRIvox(mri_src, 0, y, z) ;
      pdst = &MRIvox(mri_dst, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        val = *psrc++ ;
        if (val == in_val)
          val = out_val ;
        *pdst++ = val ;
      }
    }
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRImeanMask(MRI *mri_src, MRI *mri_mask, MRI *mri_dst,int mask, int wsize)
{
  int     width, height, depth, x, y, z, mask_val;
  BUFTYPE *pmask ;
  float   val ;

  MRIcheckVolDims(mri_src, mri_mask);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  if (mri_src->type != mri_dst->type)
    ErrorReturn
    (NULL,
     (ERROR_UNSUPPORTED, "MRImeanMask: src and dst must be same type")) ;

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      pmask = &MRIvox(mri_mask, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        mask_val = MRIgetVoxVal(mri_mask, x, y, z, 0) ;
        val = MRIgetVoxVal(mri_src, x, y, z, 0) ;

        if (mask_val == mask)
        {
          val = MRIvoxelMean(mri_src, x, y, z, wsize, 0) ;
        }
        MRIsetVoxVal(mri_dst, x, y, z, 0, val) ;
      }
    }
  }
  return(mri_dst) ;
}
MRI   *MRImaskDifferentGeometry(MRI *mri_src, MRI *mri_mask, MRI *mri_dst,
                                int mask, float out_val)
{
  MATRIX *m_vox2vox ;
  VECTOR *v1, *v2 ;
  int    x, y, z, xd, yd, zd, f, mask_val, val ;

  v1 = VectorAlloc(4, MATRIX_REAL) ;
  v2 = VectorAlloc(4, MATRIX_REAL) ;
  VECTOR_ELT(v1, 4) = 1.0 ;
  VECTOR_ELT(v2, 4) = 1.0 ;
  m_vox2vox = MRIgetVoxelToVoxelXform(mri_src, mri_mask) ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;
  for (x = 0 ; x < mri_dst->width ; x++)
  {
    V3_X(v1) = x ;
    for (y = 0 ; y < mri_dst->height ; y++)
    {
      V3_Y(v1) = y ;
      for (z = 0 ; z < mri_dst->depth ; z++)
      {
        V3_Z(v1) = z ;
        MatrixMultiply(m_vox2vox, v1, v2) ;
        xd = nint(V3_X(v2)) ;
        yd = nint(V3_Y(v2)) ;
        zd = nint(V3_Z(v2)) ;
        if (xd < 0 || xd >= mri_mask->width ||
            yd < 0 || yd >= mri_mask->height ||
            zd < 0 || zd >= mri_mask->depth )
          mask_val = mask+1 ; // allow it through
        else
          mask_val = nint(MRIgetVoxVal(mri_mask, xd, yd, zd,0)) ;
        for (f = 0; f < mri_dst->nframes; f++)
        {
          val = nint(MRIgetVoxVal(mri_src, x, y, z, f)) ;
          if (mask_val == mask)
            val = out_val ;
          else
            val = MRIgetVoxVal(mri_src, x, y, z, f) ;
          MRIsetVoxVal(mri_dst, x, y, z, f, val) ;
        }
      }
    }
  }

  VectorFree(&v1) ;
  VectorFree(&v2) ;
  MatrixFree(&m_vox2vox) ;
  return(mri_dst) ;
}

/*--------------------------------------------------------------------------
  MRImask() - applies mask to an mri data set. If mri_mask == mask at a voxel,
  then that voxel is set to out_val. Eg, if your mask is binary (0 and 1),
  and you want to set voxels outside the mask to 0 (ie, maskmri=0), then
  MRImask(src,mask,dst,0,0). Handles multiple frames.
  --------------------------------------------------------------------------*/
MRI *
MRImask(MRI *mri_src, MRI *mri_mask, MRI *mri_dst, int mask, float out_val)
{
  int     width, height, depth, nframes, x, y, z, f, mask_val ;
  float   val ;

  if (mri_src->width != mri_mask->width ||
      mri_src->height != mri_mask->height ||
      mri_src->depth != mri_mask->depth)
    return(MRImaskDifferentGeometry(mri_src, mri_mask, mri_dst,mask,out_val));

  MRIcheckVolDims(mri_src, mri_mask);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  nframes = mri_src->nframes ;

  if (!mri_dst) mri_dst = MRIclone(mri_src, NULL) ;

  if (mri_src->type != mri_dst->type)
    ErrorReturn
    (NULL,
     (ERROR_UNSUPPORTED, "MRImask: src and dst must be same type")) ;

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (x == Gx && y == Gy && z == Gz) DiagBreak() ;
        mask_val = MRIgetVoxVal(mri_mask, x, y, z, 0) ;
        for (f = 0; f < nframes; f++)
        {
          if (mask_val == mask) val = out_val ;
          else val = MRIgetVoxVal(mri_src, x, y, z, f) ;
          MRIsetVoxVal(mri_dst, x, y, z, f, val) ;
        }
      }
    }
  }
  return(mri_dst) ;
}
/*------------------------------------------------------------------
  MRImaskInvert() - changes the 1s to 0s and vice versa.
  ------------------------------------------------------------------*/
MRI *MRImaskInvert(MRI *mask, MRI *outmask)
{
  int c,r,s,f;
  double v;

  MRIcheckVolDims(mask, outmask);

  if (outmask == NULL) outmask = MRIcopy(mask,NULL);

  for (c=0; c < mask->width; c++)
  {
    for (r=0; r < mask->height; r++)
    {
      for (s=0; s < mask->depth; s++)
      {
        for (f=0; f < mask->nframes; f++)
        {
          v = MRIgetVoxVal(mask,c,r,s,f);
          if (v > 0.5) MRIsetVoxVal(outmask,c,r,s,f,0);
          else        MRIsetVoxVal(outmask,c,r,s,f,1);
        }
      }
    }
  }
  return(outmask);
}

/*------------------------------------------------------------------
  MRInMask() - count the number of voxels in the mask. "In the mask"
  means > 0.5.
  ------------------------------------------------------------------*/
int MRInMask(MRI *mask)
{
  int c,r,s,f,nmask;
  double v;

  nmask = 0;
  for (c=0; c < mask->width; c++)
  {
    for (r=0; r < mask->height; r++)
    {
      for (s=0; s < mask->depth; s++)
      {
        for (f=0; f < mask->nframes; f++)
        {
          v = MRIgetVoxVal(mask,c,r,s,f);
          if (v > 0.5) nmask++;
        }
      }
    }
  }
  return(nmask);
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIthresholdMask(MRI *mri_src, MRI *mri_mask, MRI *mri_dst,
                 float mask_threshold,float out_val)
{
  int     width, height, depth, x, y, z;
  BUFTYPE *pdst, *psrc, *pmask, val, mask_val ;
  float   fval, fmask ;

  MRIcheckVolDims(mri_src, mri_mask);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  if (mri_src->type == MRI_UCHAR && mri_mask->type == MRI_UCHAR)
  {
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        pdst = &MRIvox(mri_dst, 0, y, z) ;
        psrc = &MRIvox(mri_src, 0, y, z) ;
        pmask = &MRIvox(mri_mask, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          val = *psrc++ ;
          mask_val = *pmask++ ;
          if (mask_val >= mask_threshold)
            *pdst++ = val ;
          else
            *pdst++ = out_val ;
        }
      }
    }
  }
  else
  {
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        for (x = 0 ; x < width ; x++)
        {
          fval = MRIgetVoxVal(mri_src, x, y, z, 0) ;
          fmask = MRIgetVoxVal(mri_mask, x, y, z, 0) ;
          if (fmask < mask_threshold)
            fval = out_val ;
          MRIsetVoxVal(mri_dst, x, y, z, 0,fval) ;
        }
      }
    }
  }
  return(mri_dst) ;
}
/*----------------------------------------------------------------------
  Parameters:

  Description:
  ----------------------------------------------------------------------*/
MRI *
MRImaskThreshold(MRI *mri_src, MRI *mri_mask, MRI *mri_dst, float threshold,
                 int out_label)
{
  BUFTYPE   *pmask, *pdst, *psrc, out_val, mask, in_val ;
  int       width, height, depth, x, y, z, nchanged, noff, non ;
  float     nvox ;

  MRIcheckVolDims(mri_src, mri_mask);

  if (mri_mask->type != MRI_UCHAR)
    ErrorReturn(NULL, (ERROR_UNSUPPORTED,
                       "MRI3Dthreshold: mask must be MRI_FLOAT")) ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  /* now apply the inverse morph to build an average wm representation
     of the input volume
  */


  noff = non = nchanged = 0 ;
  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      pmask = &MRIvox(mri_mask, 0, y, z) ;
      psrc = &MRIvox(mri_src, 0, y, z) ;
      pdst = &MRIvox(mri_dst, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        if (DEBUG_POINT(x,y,z))
          DiagBreak() ;  /* marked as 255, should be 127 */
        out_val = 0 ;
        mask = *pmask++ ;   /* value from inverse morphed volume */
        in_val = *psrc++ ;
        if (mask < 100-threshold)        /* probably off */
          out_val = 0 ;
        else  if (mask > threshold)      /* probably on */
          out_val = out_label ;
        else                    /* not sure, use original fill val */
          out_val = in_val ;
        if (out_val != in_val)
        {
          if (out_val)
            non++ ;
          else
            noff++ ;
          nchanged++ ;
        }
        *pdst++ = out_val ;
      }
    }
  }

  nvox = (float)(width * height * depth) ;
  fprintf(stderr, "%8d of %8d voxels changed - %2.1f%%.\n",
          nchanged, (int)nvox, 100.0f*(float)nchanged/nvox) ;
  fprintf(stderr, "%8d of %8d voxels off     - %2.1f%%.\n",
          noff, (int)nvox,   100.0f*(float)noff/nvox) ;
  fprintf(stderr, "%8d of %8d voxels on      - %2.1f%%.\n",
          nchanged, (int)nvox, 100.0f*(float)non/nvox) ;
  return(mri_dst) ;
}
/*----------------------------------------------------------------------
  Parameters:

  Description:
  ----------------------------------------------------------------------*/
int
MRIgrowLabel(MRI *mri, MRI *mri_filled, int in_label, int out_label)
{
  int      x, y, z, width, height, depth, xi, yi, zi, xk, yk, zk, nfilled,
  total_filled, xmin, xmax, ymin, ymax, zmin, zmax ;

  MRIcheckVolDims(mri, mri_filled);

  width = mri->width ;
  height = mri->height ;
  depth = mri->depth ;
  total_filled = 0 ;

  xmin = width ;
  ymin = height ;
  zmin = depth ;
  xmax = ymax = zmax = 0 ;
  if (in_label) for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        for (x = 0 ; x < width ; x++)
        {
          if (MRIvox(mri, x, y, z) == in_label)
          {
            if (x > xmax)
              xmax = x ;
            if (y > ymax)
              ymax = y ;
            if (z > zmax)
              zmax = z ;
            if (x < xmin)
              xmin = x ;
            if (y < ymin)
              ymin = y ;
            if (z < zmin)
              zmin = z ;
          }
        }
      }
    }
  else   /* filling bg - do outside first (hack, but much faster)*/
  {
    /* find bounding box for real data */
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        for (x = 0 ; x < width ; x++)
        {
          if (MRIvox(mri, x, y, z))
          {
            if (x > xmax)
              xmax = x ;
            if (y > ymax)
              ymax = y ;
            if (z > zmax)
              zmax = z ;
            if (x < xmin)
              xmin = x ;
            if (y < ymin)
              ymin = y ;
            if (z < zmin)
              zmin = z ;
          }
        }
      }
    }

    /* fill everything outside it */
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        for (x = 0 ; x < width ; x++)
        {
          if (z <= zmin || z >= zmax ||
              y <= ymin || y >= ymax ||
              x <= xmin || x >= xmax)
          {
            total_filled++ ;
            MRIvox(mri_filled, x, y, z) = out_label ;
          }
        }
      }
    }
  }

#if 0
  xmin = ymin = zmin = 0 ;
  xmax = width-1 ;
  ymax = height-1 ;
  zmax = depth-1 ;
#endif

  do
  {
    nfilled = 0 ;
    for (z = zmin ; z <= zmax ; z++)
    {
      for (y = ymin ; y <= ymax ; y++)
      {
        for (x = xmin ; x <= xmax ; x++)
        {
          if (MRIvox(mri_filled, x, y, z) == out_label)
          {
            for (zk = -1 ; zk <= 1 ; zk++)
            {
              zi = z+zk ;
              if (zi < 0 || zi >= depth)
                continue ;
              for (yk = -1 ; yk <= 1 ; yk++)
              {
                yi = y+yk ;
                if (yi < 0 || yi >= height)
                  continue ;
                for (xk = -1 ; xk <= 1 ; xk++)
                {
                  xi = x+xk ;
                  if (xi < 0 || xi >= width)
                    continue ;
                  if ((MRIvox(mri, xi, yi, zi) ==
                       in_label) &&
                      (MRIvox(mri_filled, xi, yi, zi) !=
                       out_label))
                  {
                    nfilled++ ;
                    MRIvox(mri_filled, xi, yi, zi) =
                      out_label ;
                  }
                }
              }
            }
          }
        }
      }
    }
    total_filled += nfilled ;
    fprintf(stderr,
            "%d voxels filled, total = %d.\n", nfilled, total_filled);
  }
  while (nfilled > 0) ;
  return(NO_ERROR) ;
}
/*----------------------------------------------------------------------
  Parameters:

  Description:
  ----------------------------------------------------------------------*/
int
MRIturnOnFG(MRI *mri, MRI *mri_fg, MRI *mri_bg)
{
  int    x, y, z, width, height, depth ;

  MRIcheckVolDims(mri, mri_fg);

  width = mri->width ;
  height = mri->height ;
  depth = mri->depth ;

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (x == 157 && y == 140 && z == 56)
          DiagBreak() ;
        if (MRIvox(mri_fg, x, y, z) > 0)
          MRIvox(mri, x, y, z) = MRIvox(mri_fg, x, y, z) ;
      }
    }
  }

  /* now remove islands */
  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (x == 157 && y == 140 && z == 56)
          DiagBreak() ;
        if ((MRIvox(mri_fg, x, y, z) == 0) && (MRIvox(mri,x,y,z) > 0))
        {
          MRIvox(mri, x, y, z) = 0 ;
          MRIvox(mri_bg, x, y, z) = 1 ;
        }
      }
    }
  }
  return(NO_ERROR) ;
}
static BUFTYPE findLabel(MRI *mri, int x0, int y0, int z0) ;
static BUFTYPE
findLabel(MRI *mri, int x, int y, int z)
{
  int   xi, yi, zi, xk, yk, zk, label, width, height, depth, counts[256],
  max_label, max_count, whalf = 1 ;

  if (x == 148 && y == 104 && z == 136)
    DiagBreak() ;

  do
  {
    memset(counts, 0, sizeof(counts)) ;

    width = mri->width ;
    height = mri->height ;
    depth = mri->depth ;
    for (zk = -whalf ; zk <= whalf ; zk++)
    {
      zi = z + zk ;
      if (zi < 0 || zi >= depth)
        continue ;
      for (yk = -whalf ; yk <= whalf ; yk++)
      {
        yi = y + yk ;
        if (yi < 0 || yi >= height)
          continue ;
        for (xk = -whalf ; xk <= whalf ; xk++)
        {
          xi = x + xk ;
          if (xi < 0 || xi >= width)
            continue ;
          label = MRIvox(mri, xi, yi, zi) ;
          if (label <= WM_MIN_VAL)
            continue ;
          counts[label]++ ;
        }
      }
    }

    max_count = max_label = 0 ;
    for (label = 0 ; label <= 255 ; label++)
      if (counts[label] > max_count)
      {
        max_count = counts[label] ;
        max_label = label ;
      }
    whalf++ ;
  }
  while (max_count == 0 && whalf < 5) ;

  return(max_label) ;
}
/*----------------------------------------------------------------------
  Parameters:

  Description:
  turn off all voxels which are set in the bg image
  ----------------------------------------------------------------------*/
int
MRIturnOffBG(MRI *mri, MRI *mri_bg)
{
  int    x, y, z, width, height, depth ;

  MRIcheckVolDims(mri, mri_bg);

  width = mri->width ;
  height = mri->height ;
  depth = mri->depth ;

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (x == 157 && y == 140 && z == 56)
          DiagBreak() ;
        if (MRIvox(mri_bg, x, y, z) > 0)
          MRIvox(mri, x, y, z) = 0 ;
      }
    }
  }

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (x == 157 && y == 140 && z == 56)
          DiagBreak() ;
        if ((MRIvox(mri_bg, x, y, z) == 0) &&
            (MRIvox(mri, x, y, z) == 0))
          MRIvox(mri, x, y, z) = findLabel(mri, x, y, z) ;
      }
    }
  }
  return(NO_ERROR) ;
}
#if 0
static BUFTYPE
findLabel(MRI *mri, int x, int y, int z)
{
  int   xi, yi, zi, xk, yk, zk, left_count, right_count, label,
  width, height, depth ;

  width = mri->width ;
  height = mri->height ;
  depth = mri->depth ;
  left_count = right_count = 0 ;
  for (zk = -1 ; zk <= 1 ; zk++)
  {
    zi = z + zk ;
    if (zi < 0 || zi >= depth)
      continue ;
    for (yk = -1 ; yk <= 1 ; yk++)
    {
      yi = y + yk ;
      if (yi < 0 || yi >= height)
        continue ;
      for (xk = -1 ; xk <= 1 ; xk++)
      {
        xi = x + xk ;
        if (xi < 0 || xi >= width)
          continue ;
        label = MRIvox(mri, xi, yi, zi) ;
        if (label == MRI_LEFT_HEMISPHERE)
          left_count++ ;
        else if (label == MRI_RIGHT_HEMISPHERE)
          right_count++ ;
      }
    }
  }
  return ((left_count > right_count) ?
          MRI_LEFT_HEMISPHERE :
          MRI_RIGHT_HEMISPHERE) ;
}
#endif
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  Set an MRI intensity values to 0
  ------------------------------------------------------*/
int
MRIsetValues(MRI *mri, float val)
{
  int   width, depth, height, x, y, z, frame, nframes ;

  width = mri->width ;
  height = mri->height ;
  depth = mri->depth ;
  nframes = mri->nframes ;

  for (frame = 0 ; frame < nframes ; frame++)
  {
    for (x = 0 ; x < width  ; x++)
    {
      for (y = 0 ; y < height ; y++)
      {
        for (z = 0 ; z < depth ; z++)
        {
          MRIsetVoxVal(mri, x, y, z, frame, val) ;
        }
      }
    }
  }

  return(NO_ERROR) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIsoapBubbleLabel(MRI *mri_src, MRI *mri_label, MRI *mri_dst, int label,
                   int niter)
{
  int     width, height, depth, x, y, z, x0, y0, z0, xi, yi, zi,
  xmin, xmax, ymin, ymax, zmin, zmax, i, n;
  BUFTYPE *plabel ;
  MRI     *mri_tmp = NULL ;
  float   mean ;

  MRIcheckVolDims(mri_src, mri_label);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  mri_dst = MRIcopy(mri_src, mri_dst) ;

  xmax = 0 ;
  xmin = width-1 ;
  ymax = 0 ;
  ymin = height-1 ;
  zmax = 0 ;
  zmin = depth-1 ;
  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      plabel = &MRIvox(mri_label, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        if (*plabel++ == label)
        {
          if (x-1 < xmin)
            xmin = x-1 ;
          if (x+1 > xmax)
            xmax = x+1 ;
          if (y-1 < ymin)
            ymin = y-1 ;
          if (y+1 > ymax)
            ymax = y+1 ;
          if (z-1 < zmin)
            zmin = z-1 ;
          if (z+1 > zmax)
            zmax = z+1 ;
        }
      }
    }
  }
  xmin = MAX(0, xmin) ;
  xmax = MIN(width-1, xmax) ;
  ymin = MAX(0, ymin) ;
  ymax = MIN(height-1, ymax) ;
  zmin = MAX(0, zmin) ;
  zmax = MIN(depth-1, zmax) ;

  for (i = 0 ; i < niter ; i++)
  {
    mri_tmp = MRIcopy(mri_dst, mri_tmp) ; /* will allocate first time */
    for (z = zmin ; z <= zmax ; z++)
    {
      for (y = ymin ; y <= ymax ; y++)
      {
        for (x = xmin ; x <= xmax ; x++)
        {
          if (x == 97 && y == 131 && z == 181)
            DiagBreak() ;
          if (x == 97 && y == 131 && z == 181 && i > 171)
            DiagBreak() ;
          if (nint(MRIgetVoxVal(mri_label, x, y, z, 0)) == label)
          {
            mean = MRIgetVoxVal(mri_tmp, x, y, z, 0) ;
            n = 1 ;
            for (z0 = -1 ; z0 <= 1 ; z0++)
            {
              zi = mri_tmp->zi[z+z0] ;
              for (y0 = -1 ; y0 <= 1 ; y0++)
              {
                yi = mri_tmp->yi[y+y0] ;
                for (x0 = -1 ; x0 <= 1 ; x0++)
                {
                  xi = mri_tmp->xi[x+x0] ;
                  mean += MRIgetVoxVal(mri_tmp, xi,yi,zi,0) ;
                  n++ ;
                }
              }
            }
            MRIsetVoxVal(mri_dst, x,y,z, 0, mean / n) ;
          }
        }
      }
    }
  }
  MRIfree(&mri_tmp) ;

  return(mri_dst) ;
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  Copy the label from one volume to another
  ------------------------------------------------------*/
int
MRIcopyLabel(MRI *mri_src, MRI *mri_dst, int label)
{
  int     width, height, depth, x, y, z, nvox ;
  BUFTYPE *psrc, *pdst ;

  MRIcheckVolDims(mri_src, mri_dst);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (mri_src->type == MRI_UCHAR && mri_dst->type == MRI_UCHAR)
  {
    for (nvox = z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        psrc = &MRIvox(mri_src, 0, y, z) ;
        pdst = &MRIvox(mri_dst, 0, y, z) ;
        for (x = 0 ; x < width ; x++, pdst++)
        {
          if (*psrc++ == label)
          {
            *pdst = label ;
            nvox++ ;
          }
        }
      }
    }
  }
  else
  {
    for (nvox = z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        for (x = 0 ; x < width ; x++, pdst++)
        {
          if (MRIgetVoxVal(mri_src, x, y, z, 0) == label)
          {
            MRIsetVoxVal(mri_dst, x, y, z, 0, label) ;
            nvox++ ;
          }
        }
      }
    }
  }
  return(nvox) ;
}
/*int
MRIlabelOverlap(MRI *mri1, MRI *mri2, int label)
{
  int     width, height, depth, x, y, z, nvox ;
  BUFTYPE *pbuf1, *pbuf2 ;

  MRIcheckVolDims(mri1, mri2);

  width = mri1->width ;
  height = mri1->height ;
  depth = mri1->depth ;

  for (nvox = z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      pbuf1 = &MRIvox(mri1, 0, y, z) ;
      pbuf2 = &MRIvox(mri2, 0, y, z) ;
      for (x = 0 ; x < width ; x++, pbuf1++, pbuf2++)
      {
        if (nint(*pbuf1) == label && nint(*pbuf2) == label)
          {
	    nvox++ ;
	  }
      }
    }
  }
  return(nvox) ;
}*/
int
MRIlabelOverlap(MRI *mri1, MRI *mri2, int label)
{
  int     width, height, depth, x, y, z, nvox ;
  int val1, val2;

  MRIcheckVolDims(mri1, mri2);

  width = mri1->width ;
  height = mri1->height ;
  depth = mri1->depth ;

  for (nvox = z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
	{ 
	  val1 = nint(MRIgetVoxVal(mri1, x, y, z, 0)) ;
	  val2 = nint(MRIgetVoxVal(mri2, x, y, z, 0)) ;
	  if ((val1 == label) && (val2 == label))
	    nvox++ ;
	}
    }
  }
  return(nvox) ;
}
int
MRIlabelUnion(MRI *mri1, MRI *mri2, int label)
{
  int     width, height, depth, x, y, z, nvox ;
  int val1, val2;

  MRIcheckVolDims(mri1, mri2);

  width = mri1->width ;
  height = mri1->height ;
  depth = mri1->depth ;

  for (nvox = z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
	{ 
	  val1 = nint(MRIgetVoxVal(mri1, x, y, z, 0)) ;
	  val2 = nint(MRIgetVoxVal(mri2, x, y, z, 0)) ;
	  if ((val1 == label) || (val2 == label))
	    nvox++ ;
	}
    }
  }
  return(nvox) ;
}
int
MRItotalVoxelsOn(MRI *mri, int thresh)
{
  int     width, height, depth, x, y, z, nvox, f ;
  float   val ;

  width = mri->width ;
  height = mri->height ;
  depth = mri->depth ;

  for (nvox = f = 0 ; f < mri->nframes ; f++)
  {
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        for (x = 0 ; x < width ; x++)
        {
          val = MRIgetVoxVal(mri, x, y, z, f) ;
          if (val >= thresh)
            nvox++ ;
        }
      }
    }
  }
  return(nvox) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  Count and return the # of voxels with a given label.
  ------------------------------------------------------*/
int
MRIvoxelsInLabel(MRI *mri, int label)
{
  int     width, height, depth, x, y, z, nvox, val ;

  width = mri->width ;
  height = mri->height ;
  depth = mri->depth ;

  for (nvox = z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        val = nint(MRIgetVoxVal(mri, x, y, z, 0)) ;
        if (val == label)
          nvox++ ;
      }
    }
  }
  return(nvox) ;
}

MRI *
MRInot(MRI *mri_src, MRI *mri_dst)
{
  int   x, y, z, out ;
  float val ;

  MRIcheckVolDims(mri_src, mri_dst);

  if (mri_dst == NULL)
    mri_dst = MRIclone(mri_src, NULL) ;

  for (x = 0 ; x < mri_src->width ; x++)
  {
    for (y = 0 ;  y < mri_src->height ; y++)
    {
      for (z = 0 ; z < mri_src->depth ; z++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        out = !nint(val) ;
        MRIsetVoxVal(mri_dst, x, y, z, 0, out) ;
      }
    }
  }
  return(mri_dst) ;
}
int
MRIcopyLabeledVoxels(MRI *mri_src, MRI *mri_labeled, MRI *mri_dst, int label)
{
  int     width, height, depth, x, y, z, nvox ;
  float   val ;

  MRIcheckVolDims(mri_src, mri_labeled);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  for (nvox = z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        if (MRIgetVoxVal(mri_labeled, x, y, z, 0) == label)
        {
          val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
          MRIsetVoxVal(mri_dst, x, y, z, 0, val) ;
          nvox++ ;
        }
      }
    }
  }
  return(nvox) ;
}
MRI *
MRIsetLabelValues(MRI *mri_src, MRI *mri_label, MRI *mri_dst,
                  int label, float val)
{
  int     x, y, z, l ;

  MRIcheckVolDims(mri_src, mri_label);

  mri_dst = MRIcopy(mri_src, mri_dst) ;

  for (x = 0 ; x < mri_dst->width; x++)
  {
    for (y = 0 ; y < mri_dst->height; y++)
    {
      for (z = 0 ; z < mri_dst->depth; z++)
      {
        l = MRIgetVoxVal(mri_label, x, y, z, 0) ;
        if (l == label)
          MRIsetVoxVal(mri_dst, x, y, z, 0, val) ;
      }
    }
  }
  return(mri_dst) ;
}

/*---------------------------------------------------------------
  MRIsphereMask() - creates a binary spherical mask centered at
  voxel (c0,r0,s0) with radius voxradius (measured in voxels).
  Voxels in the sphere will have a value of val, those outside
  will be 0. All frames will have the same mask.
  ---------------------------------------------------------------*/
MRI *MRIsphereMask(int ncols, int nrows, int nslices, int nframes,
                   int c0, int r0, int s0, double voxradius, double val,
                   MRI *mri)
{
  int r,c,s,f;
  double d2, r2, v;

  r2 = voxradius*voxradius;

  if (mri==NULL) mri = MRIallocSequence(ncols,nrows,nslices,MRI_FLOAT,nframes);

  for (c=0; c < mri->width; c++)
  {
    for (r=0; r < mri->height; r++)
    {
      for (s=0; s < mri->depth; s++)
      {
        d2 = (c-c0)*(c-c0) + (r-r0)*(r-r0) + (s-s0)*(s-s0);
        if (d2 > r2) v = 0;
        else        v = val;
        for (f=0; f < mri->nframes; f++) MRIsetVoxVal(mri,c,r,s,f,v);
      }
    }
  }
  return(mri);
}
int
MRIlabelInVolume(MRI *mri_src, int label)
{
	int x, y, z ;

	for (x = 0 ; x < mri_src->width ; x++)
		for (y = 0 ; y < mri_src->height ; y++)
			for (z = 0 ; z < mri_src->depth ; z++)
				if ((int)MRIgetVoxVal(mri_src, x, y, z, 0) == label)
					return(1) ;
	return(0) ;
}

/*!
  \fn MRI *MRIsetBoundingBox(MRI *template, MRI_REGION *region, 
                             double InVal, double OutVal)
  \brief Sets the values inside of the region to be InVal and
         the values outside to be OutVal. template is a 
	 geometry template. 
*/
MRI *MRIsetBoundingBox(MRI *template, MRI_REGION *region, 
		       double InVal, double OutVal)
{
  MRI *mri;
  int x2, y2, z2;
  int c, r, s;

  if(region->x < 0) region->x = 0;
  if(region->y < 0) region->y = 0;
  if(region->z < 0) region->z = 0;

  if(region->dx < 0) region->dx = template->width;
  if(region->dy < 0) region->dy = template->height;
  if(region->dz < 0) region->dz = template->depth;

  if(region->x >= template->width){
    printf("ERROR: MRIsegBoundingBox: region x >= width\n");
    return(NULL);
  }
  if(region->y >= template->height){
    printf("ERROR: MRIsegBoundingBox: region y >= height\n");
    return(NULL);
  }
  if(region->z >= template->depth){
    printf("ERROR: MRIsegBoundingBox: region z >= depth\n");
    return(NULL);
  }
  x2 = region->x + region->dx;
  y2 = region->y + region->dy;
  z2 = region->z + region->dz;

  if(x2 >= template->width){
    printf("ERROR: MRIsegBoundingBox: region x2 >= width\n");
    return(NULL);
  }
  if(y2 >= template->height){
    printf("ERROR: MRIsegBoundingBox: region y2 >= height\n");
    return(NULL);
  }
  if(z2 >= template->depth){
    printf("ERROR: MRIsegBoundingBox: region z2 >= depth\n");
    return(NULL);
  }

  mri = MRIcloneBySpace(template, MRI_FLOAT, 1);
  if(mri == NULL) return(NULL);
    

  for(c=0; c < mri->width; c++){
    for(r=0; r < mri->height; r++){
      for(s=0; s < mri->depth; s++){
	if(c >= region->x && c <= x2 &&
	   r >= region->y && r <= y2 &&
	   s >= region->z && s <= z2)
	  MRIsetVoxVal(mri,c,r,s,0,InVal);
	else
	  MRIsetVoxVal(mri,c,r,s,0,OutVal);
      }
    }
  }
  return(mri);
}

extern char *cma_label_to_name(int label) ;
double
MRIcomputeLabelAccuracy(MRI *mri_src, MRI *mri_ref, int which, FILE *fp)
{
  double  total_accuracy = 0.0, accuracy ;
  int     label, nlabels, *present, x, y, z, nvox ;
  float   min_val, max_val, vsize ;

  vsize = mri_src->xsize ;
  MRInonzeroValRange(mri_src, &min_val, &max_val) ;
  present = (int *)calloc(max_val+1, sizeof(int)) ;
  if (present == NULL)
    ErrorExit(ERROR_NOMEMORY, "MRIcomputeLabelAccuracy: "
              "could not allocate %d element array",max_val+1) ;

  present[0] = 1 ; // don't do unknown/background
  max_val = 0 ;
  for (x = 0 ;  x < mri_src->width ; x++)
    for (y = 0 ;  y < mri_src->height ; y++)
      for (z = 0 ;  z < mri_src->depth ; z++)
      {
        label = (int)MRIgetVoxVal(mri_src, x, y, z, 0) ;
        if (present[label])
          continue ;
        nvox = MRIvoxelsInLabel(mri_ref, label) ;
        if (nvox == 0)
        {
          present[label] = -1 ;
          continue ;
        }
        present[label] = 1 ;
        if (label >= max_val)
          max_val = label ;
      }
  for (nlabels = label = 1 ;  label <= max_val ; label++)
  {
    if (present[label] <= 0)
      continue ;
    accuracy = MRIcomputeMeanMinLabelDistance(mri_src, mri_ref, label) ;
    nlabels++ ;
    total_accuracy += accuracy ;
    if (fp)
    {
      fprintf(fp, "%d %s %f\n",
              label, cma_label_to_name(label), vsize*accuracy) ;
      fflush(fp) ;
    }
  }
  total_accuracy /= nlabels ;
  free(present) ;
  return(total_accuracy) ;
}

double
MRIcomputeMeanMinLabelDistance(MRI *mri_src, MRI *mri_ref, int label)
{
  MRI   *mri_src_dist, *mri_ref_dist ;
  int    nvox1, nvox2, x, y, z ;
  float  xd, yd, zd ;
  double mean_min_dist, min_dist1, min_dist2, val1, val2 ;
  VECTOR *v1, *v2 ;
  MATRIX *m_vox2vox ;

  mri_src_dist = 
    MRIdistanceTransform(mri_src, NULL, label, -1, DTRANS_MODE_UNSIGNED, NULL);
  mri_ref_dist =
    MRIdistanceTransform(mri_ref, NULL, label, -1, DTRANS_MODE_UNSIGNED, NULL);
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
  {
    MRIwrite(mri_src, "s.mgz") ;
    MRIwrite(mri_ref, "r.mgz") ;
    MRIwrite(mri_src_dist, "sd.mgz") ;
    MRIwrite(mri_ref_dist, "rd.mgz") ;
  }

  m_vox2vox = MRIgetVoxelToVoxelXform(mri_src, mri_ref) ;
  v1 = VectorAlloc(4, MATRIX_REAL) ; v2 = VectorAlloc(4, MATRIX_REAL) ;
  VECTOR_ELT(v1,4) = VECTOR_ELT(v2, 4) = 1.0 ;
  for (nvox1 = 0, min_dist1 = 0.0, x = 0 ; x < mri_src->width ; x++)
    for (y = 0 ; y < mri_src->height ; y++)
      for (z = 0 ; z < mri_src->depth ; z++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        val1 = MRIgetVoxVal(mri_src_dist, x, y, z, 0) ;
        if (val1 > 0.5)   // not on boundary of src image
          continue ;
        V3_X(v1) = x ; V3_Y(v1) = y ; V3_Z(v1) = z ;
        MatrixMultiply(m_vox2vox, v1, v2) ;
        xd = V3_X(v2) ; yd = V3_Y(v2) ; zd = V3_Z(v2) ;
        MRIsampleVolume(mri_ref_dist, xd, yd, zd, &val2) ;
        nvox1++ ; min_dist1 += fabs(val2-val1) ;
        if (((label == Gdiag_no) || (Gdiag_no < 0)) && Gdiag_fp != NULL)
          fprintf(Gdiag_fp, "%d %f %f %f %f %f\n",
                  label, xd, yd, zd, val1, val2) ;
      }


  MatrixFree(&m_vox2vox) ;
  m_vox2vox = MRIgetVoxelToVoxelXform(mri_ref, mri_src) ;
  for (nvox2 = 0, min_dist2 = 0.0, x = 0 ; x < mri_ref->width ; x++)
    for (y = 0 ; y < mri_ref->height ; y++)
      for (z = 0 ; z < mri_ref->depth ; z++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        val1 = MRIgetVoxVal(mri_ref_dist, x, y, z, 0) ;
        if (val1 > 0.5)   // not on boundary of ref image
          continue ;
        V3_X(v1) = x ; V3_Y(v1) = y ; V3_Z(v1) = z ;
        MatrixMultiply(m_vox2vox, v1, v2) ;
        xd = V3_X(v2) ; yd = V3_Y(v2) ; zd = V3_Z(v2) ;
        MRIsampleVolume(mri_src_dist, xd, yd, zd, &val2) ;
        nvox2++ ; min_dist2 += fabs(val2-val1) ;
        if (((label == Gdiag_no) || (Gdiag_no < 0)) && Gdiag_fp != NULL)
          fprintf(Gdiag_fp, "%d %d %d %d %f %f\n",
                  label, x, y, z, val2, val1) ;
      }

  if (nvox1 == 0 || nvox2 == 0)
    return(-1) ;
  mean_min_dist = (min_dist1/nvox1 + min_dist2/nvox2) ;
  MRIfree(&mri_ref_dist) ;MRIfree(&mri_src_dist) ; 
  MatrixFree(&m_vox2vox) ; MatrixFree(&v1) ; MatrixFree(&v2) ;
  return(mean_min_dist) ;
}

int
MRIcomputeLabelCentroid(MRI *mri_aseg, int label, 
												double *pxc, double *pyc, double *pzc)
{
	int    l, x, y, z, num ;
	double xc, yc, zc ;

	for (xc = yc = zc = 0.0, num = x = 0 ; x < mri_aseg->width ; x++)
	{
		for (y = 0 ; y < mri_aseg->height ; y++)
		{
			for (z = 0 ; z < mri_aseg->depth ; z++)
			{
				l = nint(MRIgetVoxVal(mri_aseg, x, y, z, 0)) ;
				if (l != label)
					continue ;
				num++ ;
				xc += x; yc += y; zc += z;
			}
		}
	}

	if (num>0)
	{
		xc /= num;  yc /= num;  zc /= num; 
	}
	*pxc = xc ; *pyc = yc ; *pzc = zc ;

	return(NO_ERROR) ;
}
MRI *
MRIdivideAseg(MRI *mri_src, MRI *mri_dst, int label, int nunits)
{
  float  evalues[3], dx, dy, dz, e1x, e1y, e1z, mx ;
  MATRIX *m_obs, *m_obs_T, *m_cov, *m_eig ;
  int    x, y, z, l, num ;
  double cx, cy, cz, dot, min_dot, max_dot ;

  mri_dst = MRIcopy(mri_src, mri_dst) ;
  if (nunits <= 1)
    return(mri_dst) ;

  // compute centroid of label
  MRIcomputeLabelCentroid(mri_src, label, &cx, &cy, &cz);
  num = MRIvoxelsInLabel(mri_dst, label) ;

  // now compute eigensystem around this vertex
  m_obs = MatrixAlloc(3, num, MATRIX_REAL) ;
  for (num = x = 0 ; x < mri_dst->width ; x++)
    for (y = 0 ; y < mri_dst->height ; y++)
      for (z = 0 ; z < mri_dst->depth ; z++)
      {
        l = nint(MRIgetVoxVal(mri_dst, x, y, z, 0)) ;
        if (l != label)
          continue ;
        dx = x - cx ; dy = y - cy ; dz = z - cz ;
        *MATRIX_RELT(m_obs, 1, num+1) = dx ;
        *MATRIX_RELT(m_obs, 2, num+1) = dy ;
        *MATRIX_RELT(m_obs, 3, num+1) = dz ;
        num++ ;
      }
  
  m_obs_T = MatrixTranspose(m_obs, NULL) ;
  m_cov = MatrixMultiply(m_obs,m_obs_T, NULL) ;
  m_eig = MatrixEigenSystem(m_cov, evalues, NULL) ;
  e1x = *MATRIX_RELT(m_eig, 1,1) ;
  e1y = *MATRIX_RELT(m_eig, 2,1) ;
  e1z = *MATRIX_RELT(m_eig, 3,1) ;
  if (fabs(e1x) > fabs(e1y) &&  fabs(e1x) > fabs(e1z))
    mx = e1x ;
  else if (fabs(e1y) > fabs(e1z))
    mx = e1y ;
  else
    mx = e1z ;
  //  if (mx < 0)
  if (e1y < 0)  // orient them from posterior to anterior
  {
    e1x *= -1 ;
    e1y *= -1 ;
    e1z *= -1 ;
  }
  min_dot = max_dot = 0 ;
  for (x = 0 ; x < mri_dst->width ; x++)
    for (y = 0 ; y < mri_dst->height ; y++)
      for (z = 0 ; z < mri_dst->depth ; z++)
      {
        l = nint(MRIgetVoxVal(mri_dst, x, y, z, 0)) ;
        if (l != label)
          continue ;
        dx = x - cx ; dy = y - cy ; dz = z - cz ;
        dot = dx*e1x + dy*e1y + dz*e1z ;
        if (dot < min_dot)
           min_dot = dot ;
        if (dot > max_dot)
          max_dot = dot ;
      }


  for (x = 0 ; x < mri_dst->width ; x++)
    for (y = 0 ; y < mri_dst->height ; y++)
      for (z = 0 ; z < mri_dst->depth ; z++)
      {
        l = nint(MRIgetVoxVal(mri_dst, x, y, z, 0)) ;
        if (l != label)
          continue ;
        dx = x - cx ; dy = y - cy ; dz = z - cz ;
        dot = dx*e1x + dy*e1y + dz*e1z ;
        l += (int)(nunits * ((dot-min_dot) / (max_dot-min_dot+1))) ;
        if (l < label || l >= label+nunits)
          DiagBreak() ;
        MRIsetVoxVal(mri_dst, x, y, z, 0, l) ;
      }

  MatrixFree(&m_obs) ; MatrixFree(&m_obs_T) ; 
  MatrixFree(&m_eig) ; MatrixFree(&m_cov) ; 
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    MRIwrite(mri_dst, "d.mgz") ;
  return(mri_dst) ;
}

/*-----------------------------------------------------------------*/
/*!
  \fn MRI *MRIbinMaskToCol(MRI *binmask, MRI *bincol)
  \brief Sets the value of a voxel in the mask to its column number.
*/
MRI *MRIbinMaskToCol(MRI *binmask, MRI *bincol)
{
  int c,r,s,v;
  float m;

  if (bincol == NULL) {
    bincol = MRIcloneBySpace(binmask,MRI_INT,-1);
    if(bincol == NULL) return(NULL);
  }
  else{
    if (MRIdimMismatch(binmask, bincol, 1)){
      printf("ERROR: MRIbinMaskToCol: input/output dim mismatch\n");
      return(NULL);
    }
  }

  for (c=0; c < binmask->width; c++) {
    for (r=0; r < binmask->height; r++) {
      for (s=0; s < binmask->depth; s++) {
	m = MRIgetVoxVal(binmask,c,r,s,0);
	if(m < 0.5) v = 0;
	else        v = c;
	MRIsetVoxVal(bincol,c,r,s,0,v);
      }
    }
  }
  return(bincol);
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  Iteratively set all voxels in mri_dst that neighbor
  a voxel that has already been filled (starting with the seed),
  and for which the corresponding voxel in mri_src is
  below threshold.
------------------------------------------------------*/
MRI *MRIfloodFillRegion(MRI *mri_src, MRI *mri_dst, 
                        int threshold, int fill_val, int max_count)
{
  int     width, height, depth, x, y, z, nfilled, xmin, xmax, ymin, ymax,
  zmin, zmax, on, x0, x1, y0, y1, z0, z1 ;
  BUFTYPE *psrc, *pdst, val ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;


  xmin = width ;
  ymin = height ;
  zmin = depth ;
  xmax = ymax = zmax = 0 ;
  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      pdst = &MRIvox(mri_dst, 0, y, z) ;
      for (x = 0 ; x < width ; x++, pdst++)
      {
        val = *pdst ;
        if (val == fill_val)
        {
          if (x < xmin)
            xmin = x ;
          if (x > xmax)
            xmax = x ;
          if (y < ymin)
            ymin = y ;
          if (y > ymax)
            ymax = y ;
          if (z < zmin)
            zmin = z ;
          if (z > zmax)
            zmax = z ;
        }
      }
    }
  }

  do
  {
    z0 = zmin ;
    z1 = zmax ;
    y0 = ymin ;
    y1 = ymax ;
    x0 = xmin ;
    x1 = xmax ;
    nfilled = 0 ;
    for (z = zmin ; z <= zmax ; z++)
    {
      for (y = ymin ; y <= ymax ; y++)
      {
        psrc = &MRIvox(mri_src, xmin, y, z) ;
        pdst = &MRIvox(mri_dst, xmin, y, z) ;
        for (x = xmin ; x <= xmax ; x++, psrc++, pdst++)
        {
          if (x == 89 && y == 110 && z == 127)
            DiagBreak() ;
          val = *psrc ;
          if ((val > threshold) || (*pdst == fill_val))
            continue ;

          on = 0 ;
          if (x > 0)
            on = (MRIvox(mri_dst, x-1, y, z) > 0) ;
          if (!on && (x < width-1))
            on = (MRIvox(mri_dst, x+1, y, z) > 0) ;
          if (!on && (y > 0))
            on = (MRIvox(mri_dst, x, y-1, z) > 0) ;
          if (!on && (y < height-1))
            on = (MRIvox(mri_dst, x, y+1, z) > 0) ;
          if (!on && (z > 0))
            on = (MRIvox(mri_dst, x, y, z-1) > 0) ;
          if (!on && (z < depth-1))
            on = (MRIvox(mri_dst, x, y, z+1) > 0) ;
          if (on)
          {
            if (x <= x0)
              x0 = MAX(x-1,0) ;
            if (x >= x1)
              x1 = MIN(x+1,width-1) ;
            if (y <= y0)
              y0 = MAX(y-1,0) ;
            if (y >= y1)
              y1 = MIN(y+1,height-1) ;
            if (z <= z0)
              z0 = MAX(z-1,0) ;
            if (z >= z1)
              z1 = MIN(z+1,depth-1) ;
            nfilled++ ;
            *pdst = fill_val ;
          }
        }
      }
    }
    zmin = z0 ;
    zmax = z1 ;
    ymin = y0 ;
    ymax = y1 ;
    xmin = x0 ;
    xmax = x1 ;
    /*    fprintf(stderr, "# filled = %d\n", nfilled) ;*/
    if ((max_count > 0) && (nfilled > max_count))
      break ;
  }
  while (nfilled > 0) ;

  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  set voxels with a different value in mri1 and mri2 to 
  a dst_val in target volume
  ------------------------------------------------------*/
int
MRIsetDifferentVoxelsWithValue(MRI *mri1, MRI *mri2, MRI *mri_dst, int dst_val)
{
  int     width, height, depth, x, y, z, nvox ;

  if (mri_dst == NULL)
    mri_dst = MRIclone(mri1, NULL) ;
  MRIcheckVolDims(mri1, mri_dst);
  MRIcheckVolDims(mri2, mri_dst);

  width = mri1->width ;
  height = mri1->height ;
  depth = mri1->depth ;
  
  for (nvox = z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (MRIgetVoxVal(mri1, x, y, z, 0) != MRIgetVoxVal(mri2, x, y, z, 0))
        {
          MRIsetVoxVal(mri_dst, x, y, z, 0, dst_val) ;
          nvox++ ;
        }
      }
    }
  }
  return(nvox) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  set voxels with value in src volume to a different value in target volume
  ------------------------------------------------------*/
int
MRIsetVoxelsWithValue(MRI *mri_src, MRI *mri_dst, int src_val, int dst_val)
{
  int     width, height, depth, x, y, z, nvox ;
  
  if (mri_dst == NULL)
    mri_dst = MRIclone(mri_src, NULL) ;
  MRIcheckVolDims(mri_src, mri_dst);

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  
  for (nvox = z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (MRIgetVoxVal(mri_src, x, y, z, 0) == src_val)
        {
          MRIsetVoxVal(mri_dst, x, y, z, 0, dst_val) ;
          nvox++ ;
        }
      }
    }
  }
  return(nvox) ;
}
/*-------------------------------------------------------*/
/*!
  \fn double MRIpercentThresh(MRI *mri, MRI *mask, int frame, double pct)
  \brief Computes a threshold above which there will be pct percent of the
    voxels in the mask.
 */
double MRIpercentThresh(MRI *mri, MRI *mask, int frame, double pct)
{
  double *vlist, thresh;
  int nlist, ntot,npct;
  int c, r, s;
  
  ntot = mri->width*mri->depth*mri->height;
  vlist = (double *) calloc(ntot,sizeof(double));

  nlist = 0;
  for(c=0; c < mri->width; c++){
    for(r=0; r < mri->height; r++){
      for(s=0; s < mri->depth; s++){
	if(mask && MRIgetVoxVal(mask,c,r,s,0) < 0.5) continue;
	vlist[nlist] = MRIgetVoxVal(mri,c,r,s,frame);
	nlist++;
      }
    }
  }

  // sort from lowest to highest
  qsort(vlist,nlist,sizeof(double),CompareDoubles);

  // ascending order so 100-pct
  npct = round(nlist*(100-pct)/100);
  thresh = vlist[npct];

  //printf("nlist %d, npct = %d, pct=%g, thresh %g\n",nlist,npct,pct,thresh);
  free(vlist);
  return(thresh);
}

