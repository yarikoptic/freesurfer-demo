/**
 * @file  mrinorm.c
 * @brief utilities for normalizing MRI intensity values
 *
 * "Cortical Surface-Based Analysis I: Segmentation and Surface
 * Reconstruction", Dale, A.M., Fischl, B., Sereno, M.I.
 * (1999) NeuroImage 9(2):179-194
 */
/*
 * Original Author: Bruce Fischl, 4/9/97
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2010/12/16 22:32:37 $
 *    $Revision: 1.98.2.1 $
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
#include <string.h>
#include <memory.h>

#include "error.h"
#include "proto.h"
#include "mri.h"
#include "macros.h"
#include "diag.h"
#include "minc_volume_io.h"
#include "filter.h"
#include "box.h"
#include "region.h"
#include "mrinorm.h"
#include "talairachex.h"
#include "ctrpoints.h"
#include "numerics.h"
#include "mrisegment.h"

/*-----------------------------------------------------
  MACROS AND CONSTANTS
  -------------------------------------------------------*/


/*-----------------------------------------------------
  STATIC PROTOTYPES
  -------------------------------------------------------*/

static float find_tissue_intensities(MRI *mri_src, MRI *mri_ctrl,
                                     float *pwm, float *pgm, float *pcsf) ;
static int remove_gray_matter_control_points(MRI *mri_ctrl, MRI *mri_src,
    float wm_target,
    float intensity_above,
    float intensity_below,
    int scan_type) ;
static float csf_in_window(MRI *mri, int x0,int y0,int z0,
                           float max_dist,float csf);
#if 0
static double mriNormComputeWMStandardDeviation(MRI *mri_orig, MRI *mri_ctrl) ;
static int remove_extreme_control_points(MRI *mri_orig, MRI *mri_ctrl,
    float low_thresh,
    float high_thresh,
    float target,
    double wm_std) ;
#endif
static MRI *mriSplineNormalizeShort(MRI *mri_src,MRI *mri_dst,
                                    MRI **pmri_field, float *inputs,
                                    float *outputs, int npoints) ;
static MRI *mriMarkUnmarkedNeighbors(MRI *mri_src, MRI *mri_marked,
                                     MRI *mri_dst, int mark, int nbr_mark) ;
static int mriRemoveOutliers(MRI *mri, int min_nbrs) ;
#if 0
static MRI *mriDownsampleCtrl2(MRI *mri_src, MRI *mri_dst) ;
#endif
static MRI *mriSoapBubbleFloat(MRI *mri_src, MRI *mri_ctrl, MRI *mri_dst,
                               int niter) ;
static MRI *mriSoapBubbleShort(MRI *mri_src, MRI *mri_ctrl, MRI *mri_dst,
                               int niter) ;
static MRI *mriSoapBubbleExpandFloat(MRI *mri_src, MRI *mri_ctrl, MRI *mri_dst,
                                     int niter) ;
static MRI *mriBuildVoronoiDiagramFloat(MRI *mri_src, MRI *mri_ctrl,
                                        MRI *mri_dst);
static MRI *mriBuildVoronoiDiagramUchar(MRI *mri_src, MRI *mri_ctrl,
                                        MRI *mri_dst);
static MRI *mriBuildVoronoiDiagramShort(MRI *mri_src, MRI *mri_ctrl,
                                        MRI *mri_dst);

static int num_control_points = 0 ;
static int *xctrl=0 ;
static int *yctrl=0 ;
static int *zctrl=0 ;

static char *control_volume_fname = NULL ;
static char *bias_volume_fname = NULL ;

/*-----------------------------------------------------
  GLOBAL FUNCTIONS
  -------------------------------------------------------*/
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  Given a set of control points, calculate the
  1 dimensional splines which fit them and apply it
  to an image.
  ------------------------------------------------------*/
#define BIAS_IMAGE_WIDTH  25

MRI *
MRIsplineNormalize(MRI *mri_src,MRI *mri_dst, MRI **pmri_field,
                   float *inputs,float *outputs, int npoints)
{
  int       width, height, depth, x, y, z, i, dval ;
  BUFTYPE   *psrc, *pdst, sval, *pfield = NULL ;
  float     outputs_2[MAX_SPLINE_POINTS], frac ;
  MRI       *mri_field = NULL ;
  double    d ;
  char      *cp ;

  if (mri_src->type == MRI_SHORT)
    return(mriSplineNormalizeShort(mri_src, mri_dst, pmri_field, inputs,
                                   outputs, npoints)) ;
  cp = getenv("RAN") ;
  if (cp)
    d = atof(cp) ;
  else
    d = 0.0 ;

  if (pmri_field)
  {
    mri_field = *pmri_field ;
    if (!mri_field)
      *pmri_field = 
        mri_field =
        MRIalloc(BIAS_IMAGE_WIDTH, mri_src->height, 1, MRI_UCHAR) ;
  }

  if (npoints > MAX_SPLINE_POINTS)
    npoints = MAX_SPLINE_POINTS ;

  printf("Starting OpenSpline(): npoints = %d\n",npoints);
  OpenSpline(inputs, outputs, npoints, 0.0f, 0.0f, outputs_2) ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  for (y = 0 ; y < height ; y++)
  {
    if (pmri_field)
      pfield = &MRIvox(mri_field, 0, y, 0) ;
    OpenSplint(inputs, outputs, outputs_2, npoints, (float)y, &frac) ;
    if (pmri_field)
      for (i = 0 ; i < BIAS_IMAGE_WIDTH ; i++)
        *pfield++ = nint(110.0f/frac) ;

    for (z = 0 ; z < depth ; z++)
    {
      psrc = &MRIvox(mri_src, 0, y, z) ;
      pdst = &MRIvox(mri_dst, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        sval = *psrc++ ;
        dval = nint((float)sval * frac + randomNumber(0.0,d)) ;
        if (dval > 255)
          dval = 255 ;
        else if (dval < 0)
          dval = 0 ;
        *pdst++ = (BUFTYPE)dval ;
      }
    }
  }
  return(mri_dst) ;
}
static MRI *
mriSplineNormalizeShort(MRI *mri_src,MRI *mri_dst, MRI **pmri_field,
                        float *inputs,float *outputs, int npoints)
{
  int       width, height, depth, x, y, z, i, dval ;
  short     *psrc, *pdst, sval ;
  char      *pfield = NULL ;
  float     outputs_2[MAX_SPLINE_POINTS], frac ;
  MRI       *mri_field = NULL ;
  double    d ;
  char      *cp ;

  cp = getenv("RAN") ;
  if (cp)
    d = atof(cp) ;
  else
    d = 0.0 ;

  if (pmri_field)
  {
    mri_field = *pmri_field ;
    if (!mri_field)
      *pmri_field = 
        mri_field =
        MRIalloc(BIAS_IMAGE_WIDTH, mri_src->height, 1, MRI_UCHAR) ;
  }

  if (npoints > MAX_SPLINE_POINTS)
    npoints = MAX_SPLINE_POINTS ;
  OpenSpline(inputs, outputs, npoints, 0.0f, 0.0f, outputs_2) ;
  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  for (y = 0 ; y < height ; y++)
  {
    if (pmri_field)
      pfield = (char*)&MRIvox(mri_field, 0, y, 0) ;
    OpenSplint(inputs, outputs, outputs_2, npoints, (float)y, &frac) ;
    if (pmri_field)
      for (i = 0 ; i < BIAS_IMAGE_WIDTH ; i++)
        *pfield++ = nint(110.0f/frac) ;

    for (z = 0 ; z < depth ; z++)
    {
      psrc = &MRISvox(mri_src, 0, y, z) ;
      pdst = &MRISvox(mri_dst, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        sval = *psrc++ ;
        dval = nint((float)sval * frac + randomNumber(0.0,d)) ;
        if (dval > 255)
          dval = 255 ;
        else if (dval < 0)
          dval = 0 ;
        *pdst++ = (short)dval ;
      }
    }
  }
  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  perform an adaptive histogram normalization. For each
  wsize x wsize region in the image, for the histogram of
  the hsize x hsize region around it (hsize >> wsize) and
  around the corresponding region in mri_template, and adjust
  ------------------------------------------------------*/
MRI *
MRIadaptiveHistoNormalize(MRI *mri_src, MRI *mri_norm, MRI *mri_template,
                          int wsize, int hsize, int low)
{
  int        width, height, depth, woff ;
  MRI_REGION wreg, h_src_reg, h_tmp_reg, h_clip_reg ;

  /* offset the left edge of histo region w.r.t the windowed region */
  woff = (wsize - hsize) / 2 ;

  /* align the two regions so that they have a common center */
  wreg.dx = wreg.dy = wreg.dz = wsize ;
  h_src_reg.dx = h_src_reg.dy = h_src_reg.dz = hsize ;
  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  h_src_reg.z = woff ;
  for (wreg.z = 0 ; wreg.z < depth ; wreg.z += wsize, h_src_reg.z += wsize)
  {
    h_src_reg.y = woff ;
    for (wreg.y = 0 ;
         wreg.y < height ;
         wreg.y += wsize, h_src_reg.y += wsize)
    {
      h_src_reg.x = woff ;
      for (wreg.x = 0 ;
           wreg.x < width ;
           wreg.x += wsize, h_src_reg.x += wsize)
      {
        MRIclipRegion(mri_src, &h_src_reg, &h_clip_reg) ;
        MRItransformRegion(mri_src, mri_template,
                           &h_clip_reg, &h_tmp_reg) ;
        if (Gdiag & DIAG_SHOW)
#if 1
          fprintf(stderr,
                  "\rnormalizing (%d, %d, %d) --> (%d, %d, %d)       ",
                  wreg.x, wreg.y, wreg.z,
                  wreg.x+wreg.dx-1, wreg.y+wreg.dy-1,
                  wreg.z+wreg.dz-1) ;
#else
          fprintf(stderr,
                  "\rnormalizing (%d, %d, %d) --> (%d, %d, %d)       ",
                  h_tmp_reg.x, h_tmp_reg.y, h_tmp_reg.z,
                  h_tmp_reg.x+h_tmp_reg.dx-1, h_tmp_reg.y+h_tmp_reg.dy-1,
                  h_tmp_reg.z+h_tmp_reg.dz-1) ;
#endif
#if 0
        mri_norm = MRIhistoNormalizeRegion(mri_src,
                                           mri_norm, mri_template,
                                           low, &wreg,
                                           &h_src_reg, &h_tmp_reg);
#else
        mri_norm = MRIhistoNormalizeRegion(mri_src,
                                           mri_norm,
                                           mri_template,
                                           low,
                                           &wreg,
                                           &h_clip_reg,
                                           &h_clip_reg);
#endif
      }
    }
  }

  if (Gdiag & DIAG_SHOW)
    fprintf(stderr, " done.\n") ;

  return(mri_norm) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIhistoNormalizeRegion(MRI *mri_src, MRI *mri_norm, MRI *mri_template,
                        int low, MRI_REGION *wreg, MRI_REGION *h_src_reg,
                        MRI_REGION *h_tmp_reg)
{
  HISTOGRAM  h_fwd_eq, h_template_eq, h_norm ;

  MRIgetEqualizeHistoRegion(mri_src, &h_fwd_eq, low, h_src_reg, 0) ;
  MRIgetEqualizeHistoRegion(mri_template, &h_template_eq, low, h_tmp_reg, 0);
  HISTOcomposeInvert(&h_fwd_eq, &h_template_eq, &h_norm) ;
  mri_norm = MRIapplyHistogramToRegion(mri_src, mri_norm, &h_norm, wreg) ;
  if (Gdiag & DIAG_WRITE)
  {
    FILE      *fp ;
    HISTOGRAM h ;

    fp = fopen("histo.dat", "w") ;
    MRIhistogramRegion(mri_src, 0, &h, h_src_reg) ;
    fprintf(fp, "src histo\n") ;
    HISTOdump(&h, fp) ;
    fprintf(fp, "src eq\n") ;
    HISTOdump(&h_fwd_eq, fp) ;
    fprintf(fp, "template eq\n") ;
    HISTOdump(&h_template_eq, fp) ;
    fprintf(fp, "composite mapping\n") ;
    HISTOdump(&h_norm, fp) ;
  }

  return(mri_norm) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIhistoNormalize(MRI *mri_src, MRI *mri_norm, MRI *mri_template, int low,
                  int high)
{
  HISTOGRAM  h_fwd_eq, h_template_eq, h_norm ;

  HISTOclear(&h_fwd_eq, &h_fwd_eq) ;
  HISTOclear(&h_template_eq, &h_template_eq) ;
  HISTOclear(&h_norm, &h_norm) ;
  MRIgetEqualizeHisto(mri_src, &h_fwd_eq, low, high, 0) ;
  MRIgetEqualizeHisto(mri_template, &h_template_eq, low, high, 0) ;
  HISTOcomposeInvert(&h_fwd_eq, &h_template_eq, &h_norm) ;
  mri_norm = MRIapplyHistogram(mri_src, mri_norm, &h_norm) ;

  if (Gdiag & DIAG_WRITE)
  {
    FILE       *fp ;

    fp = fopen("histo.dat", "w") ;
    fprintf(fp, "src eq\n") ;
    HISTOdump(&h_fwd_eq, fp) ;
    fprintf(fp, "template eq\n") ;
    HISTOdump(&h_template_eq, fp) ;
    fprintf(fp, "composite mapping\n") ;
    HISTOdump(&h_norm, fp) ;
    fclose(fp) ;
  }

  return(mri_norm) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
#define WINDOW_WIDTH      120  /* in millimeters */

int
MRInormInit(MRI *mri, MNI *mni, int windows_above_t0,int windows_below_t0,
            int wsize, int desired_wm_value, float smooth_sigma)
{
  MRI_REGION  *reg ;
  int         i, x, y, z, dx, dy, dz, nup, z_offset, nwindows;
  int         x0_tal, y0_tal, z0_tal ;
  float       size_mod ;
  double        x0, y0, z0 ;

  LTA         *lta = 0; // need to be freeed
  LT          *lt;   // just a reference pointer (no need to free)
  MATRIX      *m_L;  // just a reference pointer (no need to free)
  VOL_GEOM *dst = 0; // just a reference pointer (no need to free)
  VOL_GEOM *src =0;  // just a reference pointer (no need to free)
  int row;

  if (wsize <= 0)
    wsize = DEFAULT_WINDOW_SIZE ;

  if (!desired_wm_value)
    desired_wm_value = mni->desired_wm_value =
                         DEFAULT_DESIRED_WHITE_MATTER_VALUE ;
  else
    mni->desired_wm_value = desired_wm_value ;
  if (FZERO(smooth_sigma))
    smooth_sigma = mni->smooth_sigma = DEFAULT_SMOOTH_SIGMA ;
  else
    mni->smooth_sigma = smooth_sigma ;
  // look for talairach.xfm
  if (mri->inverse_linear_transform)
  {
    // create lta
    lta = LTAalloc(1, NULL) ;
    // this will allocate lta->xforms[0].m_L = MatrixIdentity(4, NULL)
    lt = &lta->xforms[0] ;
    lt->sigma = 1.0f ;
    lt->x0 = lt->y0 = lt->z0 = 0 ;
    // m_L points to lt->m_L
    m_L = lt->m_L;
    lta->type = LINEAR_RAS_TO_RAS;
    // try getting from mri
    // transform is MNI transform (only COR volume reads transform)
    if (mri->linear_transform)
    {
      // linear_transform is zero based column-major array
      // sets lt->m_L
      for (row = 1 ; row <= 3 ; row++)
      {
        *MATRIX_RELT(m_L,row,1) = mri->linear_transform->m[0][row-1];
        *MATRIX_RELT(m_L,row,2) = mri->linear_transform->m[1][row-1];
        *MATRIX_RELT(m_L,row,3) = mri->linear_transform->m[2][row-1];
        *MATRIX_RELT(m_L,row,4) = mri->linear_transform->m[3][row-1];
      }
      fprintf(stderr, "talairach transform\n");
      MatrixPrint(stderr, m_L);
      // set lt->dst and lt->src
      dst = &lt->dst;
      src = &lt->src;
      // copy mri values
      getVolGeom(mri, src);
      getVolGeom(mri, dst);
      // dst is unknown, since transform is ras-to-ras
      if (getenv("NO_AVERAGE305")) // if this is set
        fprintf(stderr, "INFO: tal dst c_(r,a,s) not modified\n");
      else
      {
        fprintf(stderr,
                "INFO: Modifying talairach volume c_(r,a,s) "
                "based on average_305\n");
        dst->valid = 1;
        // use average_305 value
        dst->c_r = -0.095;
        dst->c_a = -16.51;
        dst->c_s =   9.75;
      }
      // now we finished setting up lta
    }
    if (MRItalairachToVoxelEx(mri, 0.0, 0.0, 0.0,
                              &x0, &y0, &z0, lta) != NO_ERROR)
      ErrorReturn(Gerror,
                  (Gerror,
                   "MRInormComputeWindows: "
                   "could not find Talairach origin"));
    x0_tal = nint(x0) ;
    y0_tal = nint(y0) ;
    z0_tal = nint(z0) ;
    LTAfree(&lta);
  }
  else  /* no Talairach information available */
  {
    MRI_REGION  bbox ;

#if 0
    MRIboundingBoxNbhd(mri, 50, 5, &bbox) ;
#else
    MRIfindApproximateSkullBoundingBox(mri, 50, &bbox) ;
#endif
    /* place x and z at center of bounding box */
    x0_tal = bbox.x + bbox.dx/2.0 ;
    z0_tal = bbox.z + bbox.dz/2.0 ;

    /* due to neck variability, place y 7.5 cm down from top of skull */
    y0_tal = bbox.y + 75.0f/mri->ysize ;

  }

  if (windows_above_t0 > 0)
    mni->windows_above_t0 = windows_above_t0 ;
  else
    windows_above_t0 = mni->windows_above_t0 = DEFAULT_WINDOWS_ABOVE_T0 ;

  if (windows_below_t0 > 0)
    mni->windows_below_t0 = windows_below_t0 ;
  else
    windows_below_t0 = mni->windows_below_t0 = DEFAULT_WINDOWS_BELOW_T0 ;
  nwindows = mni->windows_above_t0 + mni->windows_below_t0 ;

  if (Gdiag & DIAG_SHOW)
  {
    fprintf(stderr, "MRInormInit:\n") ;
    fprintf(stderr, "Talairach origin at (%d, %d, %d)\n",
            x0_tal, y0_tal, z0_tal) ;
    fprintf(stderr, "wsize %d, windows %d above, %d below\n",
            wsize, mni->windows_above_t0, mni->windows_below_t0) ;
  }

  x = 0 ;
  dx = mri->width ;
  z = 0 ;
  dz = mri->depth ;
  y = y0_tal - nint((float)wsize*OVERLAP) * windows_above_t0 ;
  dy = wsize ;
  for (i = 0 ; i < nwindows ; i++)
  {
    reg = &mni->regions[i] ;
    reg->x = x ;
    reg->y = y ;
    reg->z = z ;
    if (y < y0_tal)  /* head gets smaller as we get further up */
    {
      nup = windows_above_t0 - i ;
      size_mod = pow(SIZE_MOD, (double)nup) ;
    }
    else
      size_mod = 1.0f ;

    dx = nint(size_mod * WINDOW_WIDTH) ;
    z_offset = nint((float)dx * Z_OFFSET_SCALE) ;
    dz = nint(size_mod * WINDOW_WIDTH) ;
    reg->dx = dx ;
    reg->x = x0_tal - dx/2 ;
    reg->z = z0_tal - (dz/2+z_offset) ;
    reg->dz = dz + z_offset ;
    reg->dy = dy ;
    reg->y = y ;
    y += nint((float)wsize*OVERLAP) ;
    if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
      fprintf(stderr, "window %d: (%d, %d, %d) -> (%d, %d, %d)\n",
              i, reg->x, reg->y, reg->z, reg->dx, reg->dy, reg->dz) ;
  }

  return(NO_ERROR) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
#define TOO_BRIGHT   225
int
MRInormFillHistograms(MRI *mri, MNI *mni)
{
  int         i, nwindows ;

  nwindows = mni->windows_above_t0 + mni->windows_below_t0 ;
  for (i = 0 ; i < nwindows ; i++)
  {
    MRIhistogramRegion(mri, HISTO_BINS, mni->histograms+i, mni->regions+i) ;
    HISTOclearBins(mni->histograms+i,mni->histograms+i,
                   0,BACKGROUND_INTENSITY);
    HISTOclearBins(mni->histograms+i,mni->histograms+i,TOO_BRIGHT, 255);
  }
  return(NO_ERROR) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int
MRInormFindPeaks(MNI *mni, float *inputs, float *outputs)
{
  int        i, peak, deleted, nwindows, npeaks, whalf = (HISTO_WINDOW_SIZE-1)/2 ;
  HISTOGRAM  *hsmooth = NULL ;
  MRI_REGION *reg ;

  nwindows = mni->windows_above_t0 + mni->windows_below_t0 ;
  for (deleted = i = 0 ; i < nwindows ; i++)
  {
    reg = &mni->regions[i] ;
    hsmooth = HISTOsmooth(&mni->histograms[i], hsmooth, mni->smooth_sigma) ;
    peak = HISTOfindLastPeakInRegion(hsmooth, HISTO_WINDOW_SIZE, MIN_HISTO_PCT, whalf, hsmooth->nbins-whalf) ;
    if (peak < 0)
      deleted++ ;
    else
    {
      inputs[i-deleted] = (float)reg->y + (float)reg->dy/ 2.0f ;
#if 0
      outputs[i-deleted] = mni->desired_wm_value / (float)peak ;
#else
      outputs[i-deleted] = (float)mni->histograms[i].bins[peak] ;
#endif
    }
  }

  npeaks = nwindows - deleted ;
  npeaks = MRInormCheckPeaks(mni, inputs, outputs, npeaks) ;
  for (i = 0 ; i < npeaks ; i++)
    outputs[i] = (float)mni->desired_wm_value / outputs[i] ;

  if (hsmooth)
    HISTOfree(&hsmooth);

  return(npeaks) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  the variation in the magnetic field should be relatively
  slow. Use this to do a consistency check on the peaks, removing
  any outliers.

  Note that the algorithm only deletes the furthest outlier on
  each iteration. This is because you can get a patch of bad
  peaks, the middle of which looks fine by the local slope
  test. However, the outside of the patch will be eroded at
  each iteration, and the avg and sigma estimates will improve.
  Of course, this is slower, but since the number of peaks is
  small (20 or so), time isn't a concern.

  Also, I use both forwards and backwards derivatives to try
  and arbitrate which of two neighboring peaks is bad (the bad
  one is more likely to have both fwd and bkwd derivatives far
  from the mean).
  ------------------------------------------------------*/
int
MRInormCheckPeaks(MNI *mni, float *inputs, float *outputs, int npeaks)
{
  int        starting_slice, slice, deleted[MAX_SPLINE_POINTS], old_slice,n ;
  float      Iup, I, Idown, dI, dy, grad, max_gradient ;
  int i;

  for (i=0; i < MAX_SPLINE_POINTS; ++i)
    deleted[i] = 0;

  /* rule of thumb - at least a third of the coefficients must be valid */
  if (npeaks < (mni->windows_above_t0+mni->windows_below_t0)/3)
    return(0) ;

  if (FZERO(mni->max_gradient))
    max_gradient = MAX_GRADIENT ;
  else
    max_gradient = mni->max_gradient ;

  if (Gdiag & DIAG_SHOW)
    fprintf(stderr, "max gradient %2.3f\n", mni->max_gradient) ;
  if (Gdiag & DIAG_SHOW)
    for (slice = 0 ; slice < npeaks ; slice++)
      fprintf(stderr, "%d: %2.0f --> %2.0f\n",
              slice,inputs[slice],outputs[slice]) ;

  /*
    first find a good starting slice to anchor the checking by taking the
    median peak value of three slices around the center of the brain.
  */
  starting_slice = mni->windows_above_t0-STARTING_SLICE ;
  Iup = outputs[starting_slice-SLICE_OFFSET] ;
  I = outputs[starting_slice] ;
  Idown = outputs[starting_slice+SLICE_OFFSET] ;

  if (Gdiag & DIAG_SHOW)
    fprintf(stderr,
            "testing slices %d-%d=%d (%2.0f), %d (%2.0f), and %d (%2.0f)\n",
            mni->windows_above_t0,STARTING_SLICE, starting_slice, I,
            starting_slice+SLICE_OFFSET, Idown, starting_slice-SLICE_OFFSET,
            Iup) ;

  if ((I >= MIN(Iup, Idown)) && (I <= MAX(Iup, Idown)))
    starting_slice = starting_slice ;
  else if ((Iup >= MIN(I, Idown)) && (Iup <= MAX(I, Idown)))
    starting_slice = starting_slice - SLICE_OFFSET ;
  else
    starting_slice = starting_slice + SLICE_OFFSET ;

  if (Gdiag & DIAG_SHOW)
    fprintf(stderr, "starting slice %d --> %2.0f\n",
            starting_slice, outputs[starting_slice]) ;

  /* search forward and fill in arrays */
  old_slice = starting_slice ;
  for (slice = starting_slice+1 ; slice < npeaks ; slice++)
  {
    dI = outputs[slice] - outputs[old_slice] ;
#if 0
    dy = inputs[slice] - inputs[old_slice] ;
#else
    dy = inputs[slice] - inputs[slice-1] ;
#endif
    grad = fabs(dI / dy) ;
    deleted[slice] =
      ((grad > max_gradient) ||
       ((slice - old_slice) > MAX_SKIPPED));
    if (!deleted[slice])
      old_slice = slice ;
    else if (Gdiag & DIAG_SHOW)
      fprintf(stderr,
              "deleting peak[%d]=%2.0f, grad = %2.0f / %2.0f = %2.1f\n",
              slice, outputs[slice],dI, dy, grad) ;
  }

  /* now search backwards and fill stuff in */
  old_slice = starting_slice ;
  for (slice = starting_slice-1 ; slice >= 0 ; slice--)
  {
    dI = outputs[old_slice] - outputs[slice] ;
#if 0
    dy = inputs[slice] - inputs[old_slice] ;
#else
    dy = inputs[slice+1] - inputs[slice] ;
#endif
    grad = fabs(dI / dy) ;
    deleted[slice] =
      ((grad > max_gradient) ||
       ((old_slice - slice) > MAX_SKIPPED));
    if (!deleted[slice])
      old_slice = slice ;
    else if (Gdiag & DIAG_SHOW)
      fprintf(stderr,
              "deleting peak[%d]=%2.0f, grad = %2.0f / %2.0f = %2.1f\n",
              slice, outputs[slice], dI, dy, grad) ;
  }

  for (n = slice = 0 ; slice < npeaks ; slice++)
  {
    if (!deleted[slice])   /* passed consistency check */
    {
      outputs[n] = outputs[slice] ;
      inputs[n] = inputs[slice] ;
      n++ ;
    }
  }

  npeaks = n ;

  if (Gdiag & DIAG_SHOW)
    fflush(stderr) ;

  return(npeaks) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRInormalize(MRI *mri_src, MRI *mri_dst, MNI *mni)
{
  float    inputs[MAX_SPLINE_POINTS], outputs[MAX_SPLINE_POINTS] ;
  int      npeaks, dealloc = 0 ;

  if (!mni)   /* do local initialization */
  {
    dealloc = 1 ;
    mni = (MNI *)calloc(1, sizeof(MNI)) ;
    MRInormInit(mri_src, mni, 0, 0, 0, 0, 0.0f) ;
  }

  MRInormFillHistograms(mri_src, mni) ;
  npeaks = MRInormFindPeaks(mni, inputs, outputs) ;
  if (npeaks == 0)
    ErrorReturn(NULL,
                (ERROR_BADPARM,
                 "MRInormalize: could not find any valid peaks.\n"
                 "\nMake sure the Talairach alignment is correct!\n"));
  printf("MRIsplineNormalize(): npeaks = %d\n",npeaks);
  if(npeaks <= 1){
    printf("ERROR: number of peaks must be > 1 for spline\n");
    return(NULL);
  }
  mri_dst = MRIsplineNormalize(mri_src, mri_dst,NULL,inputs,outputs,npeaks);

  if (Gdiag & DIAG_SHOW)
  {
    int i ;

    fprintf(stderr, "normalization found %d peaks:\n", npeaks) ;
    for (i = 0 ; i < npeaks ; i++)
      fprintf(stderr, "%d: %2.1f --> %2.3f\n", i, inputs[i], outputs[i]) ;
  }
  if (dealloc)
    free(mni) ;

  return(mri_dst) ;
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
#define SCALE  2

MRI *
MRInormFindControlPoints(MRI *mri_src, int wm_target, float intensity_above,
                         float intensity_below, MRI *mri_ctrl, int which,
                         int scan_type)
{
  int     width, height, depth, x, y, z, xk, yk, zk, xi, yi, zi;
  int     *pxi, *pyi, *pzi, ctrl, nctrl, nfilled, too_low, total_filled;
  int     val0, val, n, whalf, pass=0 ;
  BUFTYPE low_thresh, hi_thresh ;
  float   wm_val, gm_val, csf_val, mean_val, min_val, max_val;
  float   int_below_adaptive ;
#if 0
  int     nremoved ;
  BUFTYPE csf_thresh ;
#endif

  if (!wm_target)
    wm_target = DEFAULT_DESIRED_WHITE_MATTER_VALUE ;
  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  if (!mri_ctrl)
  {
    mri_ctrl = MRIalloc(width, height, depth, MRI_UCHAR) ;
    MRIcopyHeader(mri_src, mri_ctrl) ;
  }

  pxi = mri_src->xi ;
  pyi = mri_src->yi ;
  pzi = mri_src->zi ;

  /*
    find points which are close to wm_target, and in 7x7x7 relatively
    homogenous regions.
  */
#if 1
  nctrl += MRInormAddFileControlPoints(mri_ctrl, 255) ;

  pass=0 ;
  do
  {
    MRInormFindControlPointsInWindow
    (mri_src, wm_target,
     1.5*intensity_above-pass*5, 1.5*intensity_below+pass*5,
     mri_ctrl, 3.0, "", &nctrl, scan_type) ;
    pass++ ;
  }
  while (nctrl < 10) ;
  MRInormFindControlPointsInWindow(mri_src, wm_target, intensity_above,
                                   intensity_below, mri_ctrl, 2.0, "", &n,
                                   scan_type) ;
  nctrl += n ;

  /* use these control points as an anchor to estimate wm peak and the
     other tissue classes from it. Then recompute control points using
     adaptive thresholds
  */
  find_tissue_intensities(mri_src, mri_ctrl, &wm_val, &gm_val, &csf_val) ;
  MRIclear(mri_ctrl) ;
  pass = nctrl = 0 ;
  do
  {
    MRInormFindControlPointsInWindow
    (mri_src, wm_target,
     1.5*intensity_above+5*pass, 1.5*intensity_below+5*pass,
     mri_ctrl, 3.0, "", &nctrl, scan_type) ;
    pass++ ;
  }
  while (nctrl < 10) ;
  if (Gx >= 0)
    printf("after 7x7x7 - (%d, %d, %d) is %sa control point\n",
           Gx, Gy, Gz, MRIvox(mri_ctrl, Gx, Gy,Gz) ? "" : "NOT ") ;
  int_below_adaptive = (wm_val-gm_val)/3;
  low_thresh = wm_target-int_below_adaptive;
  low_thresh = MAX(wm_target-1.5*intensity_below, low_thresh) ;
  MRInormFindControlPointsInWindow(mri_src, wm_target, intensity_above,
                                   wm_target-low_thresh, mri_ctrl,
                                   2.0, "", &n, scan_type) ;
  if (Gx >= 0)
    printf("after 5x5x5 - (%d, %d, %d) is %sa control point\n",
           Gx, Gy, Gz, MRIvox(mri_ctrl, Gx, Gy,Gz) ? "" : "NOT ") ;
  nctrl += n ;

  low_thresh = wm_target-intensity_below;
#if 1
  int_below_adaptive = (wm_val-gm_val)/4;
  low_thresh = (BUFTYPE)nint(wm_target-int_below_adaptive) ;
#endif
  hi_thresh =  wm_target+intensity_above;
#else
  low_thresh = wm_target-1.5*intensity_below;
  hi_thresh =  wm_target+1.5*intensity_above;
  for (nctrl = z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        val0 = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        if (val0 >= low_thresh && val0 <= hi_thresh)
        {
#ifdef WSIZE
#undef WSIZE
#endif
#ifdef WHALF
#undef WHALF
#endif
#define WSIZE   7
#define WHALF  ((WSIZE-1)/2)
          whalf = ceil(WHALF / mri_src->xsize) ;
          ctrl = 128 ;
          for (zk = -whalf ; ctrl && zk <= whalf ; zk++)
          {
            zi = pzi[z+zk] ;
            for (yk = -whalf ; ctrl && yk <= whalf ; yk++)
            {
              yi = pyi[y+yk] ;
              for (xk = -whalf ; ctrl && xk <= whalf ; xk++)
              {
                xi = pxi[x+xk] ;
                val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
                if (val > hi_thresh || val < low_thresh)
                  ctrl = 0 ;   /* not homogeneous enough */
              }
            }
          }
        }
        else
          ctrl = 0 ;
        if (ctrl)
          nctrl++ ;
        MRIvox(mri_ctrl, x, y, z) = ctrl ;
      }
    }
  }
  if (Gdiag & DIAG_SHOW)
    fprintf(stderr, "%d 7x7x7 control points found\n", nctrl) ;

  /*
  find points which are close to wm_target, and in 5x5x5 relatively
  homogenous regions.
  */
  low_thresh = wm_target-intensity_below;
  hi_thresh =  wm_target+intensity_above;
  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (MRIvox(mri_ctrl, x, y, z))  /* already a control point */
          continue ;
        val0 = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        if (val0 >= low_thresh && val0 <= hi_thresh)
        {
#ifdef WSIZE
#undef WSIZE
#endif
#ifdef WHALF
#undef WHALF
#endif
#define WSIZE   5
#define WHALF  ((WSIZE-1)/2)
          whalf = ceil(WHALF / mri_src->xsize) ;
          ctrl = 128 ;
          for (zk = -whalf ; ctrl && zk <= whalf ; zk++)
          {
            zi = pzi[z+zk] ;
            for (yk = -whalf ; ctrl && yk <= whalf ; yk++)
            {
              yi = pyi[y+yk] ;
              for (xk = -whalf ; ctrl && xk <= whalf ; xk++)
              {
                xi = pxi[x+xk] ;
                val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
                if (val > hi_thresh || val < low_thresh)
                  ctrl = 0 ;   /* not homogeneous enough */
              }
            }
          }
        }
        else
          ctrl = 0 ;
        if (ctrl)
          nctrl++ ;
        MRIvox(mri_ctrl, x, y, z) = ctrl ;
      }
    }
  }
  if (Gdiag & DIAG_SHOW)
    fprintf(stderr, "%d 5x5x5 control points found\n", nctrl) ;
#endif

  /*  add all voxels that neighbor a control point and are in a 3x3x3
      neighborhood that has unambiguous intensities
      (should push things out close to the border).
  */
  total_filled = 0 ;
#ifdef WSIZE
#undef WSIZE
#endif
#ifdef WHALF
#undef WHALF
#endif
#define WSIZE   3
#define WHALF  ((WSIZE-1)/2)
  whalf = ceil(WHALF / mri_src->xsize) ;
#if 0
  if (mri_src->xsize < 0.9) // be more conservative for hires volumes
  {
    low_thresh = wm_target-intensity_below/2;
    hi_thresh =  wm_target+intensity_above/2;
  }
#endif

  do
  {
    if (which < 1)
      break ;
    nfilled = 0 ;
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        for (x = 0 ; x < width ; x++)
        {
          val0 = MRIgetVoxVal(mri_src, x, y, z, 0) ;
          ctrl = MRIvox(mri_ctrl, x, y, z) ;
          too_low = 0 ;
          if (val0 >= low_thresh && val0 <= hi_thresh && !ctrl)
          {
            n = 0 ;
            mean_val = min_val = max_val = 0.0 ;
            if (x == Gx && y == Gy && z == Gz)
              DiagBreak() ;
            for (zk = -whalf ; zk <= whalf && !too_low ; zk++)
            {
              zi = pzi[z+zk] ;
              for (yk = -whalf ; yk <= whalf && !too_low ; yk++)
              {
                yi = pyi[y+yk] ;
                for (xk = -whalf ;
                     xk <= whalf && !too_low ;
                     xk++)
                {
                  /*
                    check for any 27-connected neighbor
                    that is a control
                    point and has a small intensity
                    difference with the
                    current point.
                  */
                  xi = pxi[x+xk] ;
                  val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
                  if (val >= hi_thresh || val <= low_thresh)
                  {
                    too_low = 1 ;
                    ctrl = 0 ;
                    break ;
                  }
                  if ((abs(xk) + abs(yk) + abs(zk)) > 1)
                    continue ;  /* only allow 4 (6 in 3-d)
                                   connectivity */

                  /* now make sure that a
                     6-connected neighbor exists that
                     is currently a control point.
                  */
                  if (MRIvox(mri_ctrl, xi, yi, zi))
                  {
                    n++ ;
                    val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
                    mean_val += val ;
                    if (val > max_val)
                      max_val = val ;
                    if (val < min_val)
                      min_val = val ;
                    ctrl = 128 ;
                  }
                }
              }
            }
            if (Gx == x && Gy == y && Gz == z)
            {
              if (ctrl > 0)
                DiagBreak() ;
              DiagBreak() ;
            }
            mean_val /= (float)n ;
            if ((val0 >= wm_target) ||
                (mean_val-val0 < int_below_adaptive/2))
            {
              MRIvox(mri_ctrl, x, y, z) = ctrl ;
              if (ctrl)
                nfilled++ ;
            }
          }
        }
      }
    }
    total_filled += nfilled ;
  }
  while (nfilled > 0) ;
  nctrl += total_filled ;
  if (Gdiag & DIAG_SHOW)
    fprintf(stderr,
            "%d contiguous %dx%dx%d control points added above %d\n",
            total_filled, 2*whalf+1, 2*whalf+1, 2*whalf+1, low_thresh);
  if (Gx >= 0)
    printf("after 3x3x3 - (%d, %d, %d) is %sa control point\n",
           Gx, Gy, Gz, MRIvox(mri_ctrl, Gx, Gy,Gz) ? "" : "NOT ") ;


  /*  add all voxels that neighbor at least 3 control points and
      have and that lie in a 6-connected neighborhood of high
      intensities. (should push things out even close to the border).
  */
#if 0
  low_thresh = wm_target-intensity_below/2;
  hi_thresh =  wm_target+intensity_above/2;
#endif
  total_filled = 0 ;
  low_thresh = MAX(low_thresh, wm_target-intensity_below) ;

  /* doesn't make sense to look for 6-connected
     nbrs if the voxel size is less than 1mm */
  if (mri_src->xsize > 0.9)     do
    {
      if (which  < 2)
        break ;
      nfilled = 0 ;
      for (z = 0 ; z < depth ; z++)
      {
        for (y = 0 ; y < height ; y++)
        {
          for (x = 0 ; x < width ; x++)
          {
            val0 = MRIgetVoxVal(mri_src, x, y, z, 0) ;
            ctrl = MRIvox(mri_ctrl, x, y, z) ;
            too_low = 0 ;
            if (val0 >= low_thresh && val0 <= hi_thresh && !ctrl)
            {
              if (x == Gx && y == Gy && z == Gz)
                DiagBreak() ;
              n = 0 ;
              mean_val = min_val = max_val = 0.0 ;
              for (zk = -whalf ; zk <= whalf && !too_low ; zk++)
              {
                zi = pzi[z+zk] ;
                for (yk = -whalf ; yk <= whalf && !too_low ; yk++)
                {
                  yi = pyi[y+yk] ;
                  for (xk = -whalf ;
                       xk <= whalf && !too_low ;
                       xk++)
                  {
                    /*
                      check for any 27-connected neighbor
                      that is a control
                      point and has a small intensity
                      difference with the
                      current point.
                    */
                    xi = pxi[x+xk] ;
                    val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
                    if (MRIvox(mri_ctrl, xi, yi, zi)
                        && (abs(xk)<=1)
                        && (abs(yk)<=1)
                        && (abs(zk)<=1))
                    {
                      n++ ;   /* count # of 27-connected control points */
                      mean_val += val ;
                      if (val > max_val)
                        max_val = val ;
                      if (val < min_val)
                        min_val = val ;
                    }
                    if ((abs(xk) + abs(yk) + abs(zk)) > 1)
                      continue ;  /* only allow 4 (6 in 3-d)
                                     connectivity */
                    if (val >= hi_thresh || val <= low_thresh)
                      too_low = 1 ;
                  }
                }
              }
#define MIN_CONTROL_POINTS   4
              if (x == Gx && y == Gy && z == Gz)
                DiagBreak() ;
              if (n >= MIN_CONTROL_POINTS && !too_low)
              {
                if (x == Gx && y == Gy && z == Gz)
                  DiagBreak() ;
                mean_val /= n ;
                if ((val0 >= wm_target)
                    || (mean_val-val0 < int_below_adaptive/2))
                {
                  /* if (val0 >= wm_target
                     || (max_val-min_val<int_below_adaptive))*/
                  {
                    ctrl = 1 ;
                    MRIvox(mri_ctrl, x, y, z) = 128 ;
                    nfilled++ ;
                  }
                }
              }
            }
          }
        }
      }
      total_filled += nfilled ;
    }
    while (nfilled > 0) ;
  nctrl += total_filled ;
  if (Gdiag & DIAG_SHOW)
    fprintf(stderr,"%d contiguous 6-connected control points "
            "added above threshold %d\n",
            total_filled, low_thresh);
  if (Gx >= 0)
    printf("after 6-connected - (%d, %d, %d) is %sa control point\n",
           Gx, Gy, Gz, MRIvox(mri_ctrl, Gx, Gy,Gz) ? "" : "NOT ") ;

#if 0
  low_thresh = wm_target-(2*intensity_below);
  hi_thresh =  wm_target+intensity_above;
  total_filled = 0 ;
  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        val0 = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        if (val0 >= low_thresh && val0 <= hi_thresh)
        {
#undef WHALF
#undef WSIZE
#define WSIZE   9
#define WHALF  ((WSIZE-1)/2)
          whalf = ceil(WHALF / mri_src->xsize) ;
          ctrl = 128 ;
          for (zk = -whalf ; ctrl && zk <= whalf ; zk++)
          {
            zi = pzi[z+zk] ;
            for (yk = -whalf ; ctrl && yk <= whalf ; yk++)
            {
              yi = pyi[y+yk] ;
              for (xk = -whalf ; ctrl && xk <= whalf ; xk++)
              {
                xi = pxi[x+xk] ;
                val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
                if (val > hi_thresh || val < low_thresh)
                  ctrl = 0 ;   /* not homogeneous enough */
              }
            }
          }
        }
        else
          ctrl = 0 ;
        if (ctrl)
          total_filled++ ;
        MRIvox(mri_ctrl, x, y, z) = ctrl ;
      }
    }
  }
  if (Gdiag & DIAG_SHOW)
    fprintf(stderr, "%d %d mm homogenous control points found\n",
            total_filled,WSIZE);
  nctrl += total_filled ;
#endif

#if 0
  total_filled = 0 ;
  do
  {
    int   low_gradients ;
    float dist ;

    nfilled = 0 ;
    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        for (x = 0 ; x < width ; x++)
        {
          val0 = MRIgetVoxVal(mri_src, x, y, z, 0) ;
          ctrl = MRIvox(mri_ctrl, x, y, z) ;
          low_gradients = 0 ;
          if (val0 >= low_thresh && val0 <= hi_thresh && !ctrl)
          {
            if (x == Gx && y == Gy && z == Gz)
              DiagBreak() ;
            for (zk = -1 ; zk <= 1 ; zk++)
            {
              zi = pzi[z+zk] ;
              for (yk = -1 ; yk <= 1 ; yk++)
              {
                yi = pyi[y+yk] ;
                for (xk = -1 ; xk <= 1 ; xk++)
                {
                  if (!xk && !yk && !zk)
                    continue ;

                  /*
                    check for any 27-connected
                    neighbor that is not
                    in the right intensity range.
                  */
                  xi = pxi[x+xk] ;
                  if (MRIvox(mri_ctrl, xi, yi, zi))
                    /* neighboring ctrl pt */
                  {
                    val =
                      MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
                    dist = sqrt(xk*xk+yk*yk+zk*zk) ;
                    if (FZERO(dist))
                      dist = 1.0 ;
#define MIN_GRAD 1.0
                    if (fabs(val-val0)/dist < MIN_GRAD)
                      low_gradients++ ;
                  }
                  if ((abs(xk) + abs(yk) + abs(zk)) > 1)
                    continue ;  /* only allow 4 (6 in 3-d) connectivity */
                }
              }
            }
            if (low_gradients >= 9)
            {
              MRIvox(mri_ctrl, x, y, z) = 128 ;
              nfilled++ ;
            }
          }
        }
      }
    }
    total_filled += nfilled ;
  }
  while (nfilled > 0) ;
  nctrl += total_filled ;
  if (Gdiag & DIAG_SHOW)
    fprintf(stderr,"%d contiguous low gradient control points added\n",
            total_filled);
#endif

#undef WSIZE
#undef WHALF
  mriRemoveOutliers(mri_ctrl, 2) ;

#if 0
  /* now remove control points that are within 2mm of csf */
  csf_thresh = (BUFTYPE)nint(find_csf(mri_src, mri_ctrl, 0)) ;
  printf("setting max csf intensity to %d\n", csf_thresh) ;
  for (nremoved = z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;

        ctrl = MRIvox(mri_ctrl, x, y, z) ;
        if (ctrl == 0)
          continue ;
        val0 = MRIgetVoxVal(mri_src, x, y, z, 0) ;
#ifdef WSIZE
#undef WSIZE
#endif
#ifdef WHALF
#undef WHALF
#endif
#define WSIZE   5
#define WHALF  ((WSIZE-1)/2)
        whalf = ceil(WHALF / mri_src->xsize) ;
        ctrl = 128 ;
        for (zk = -whalf ; ctrl && zk <= whalf ; zk++)
        {
          zi = pzi[z+zk] ;
          for (yk = -whalf ; ctrl && yk <= whalf ; yk++)
          {
            yi = pyi[y+yk] ;
            for (xk = -whalf ; ctrl && xk <= whalf ; xk++)
            {
              xi = pxi[x+xk] ;
              val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
              if (val <= csf_thresh)
                ctrl = 0 ;   /* a csf voxel too close */
            }
          }
        }
        if (ctrl == 0)
          nremoved++ ;
        MRIvox(mri_ctrl, x, y, z) = ctrl ;
      }
    }
  }

  printf("%d control points removed due to csf proximity...\n", nremoved) ;
  nctrl -= nremoved ;
#endif

#if 1
  nctrl += MRInormAddFileControlPoints(mri_ctrl, 255) ;
#else
  /* read in control points from a file (if specified) */
  for (i = 0 ; i < num_control_points ; i++)
  {
    /*    if (!MRIvox(mri_ctrl, xctrl[i], yctrl[i], zctrl[i]))*/
    {
      MRIvox(mri_ctrl, xctrl[i], yctrl[i], zctrl[i]) = 255 ;
      nctrl++ ;
    }
  }
#endif

  if (Gdiag & DIAG_SHOW)
    fprintf(stderr, "%d control points found.\n", nctrl) ;
  if (Gx >= 0 && Gy >= 0 && Gz >= 0)
  {
    float val = MRIgetVoxVal(mri_src, Gx, Gy, Gz, 0);
    printf("(%d, %d, %d) is%s a control point (T1=%2.0f)\n",
           Gx,Gy,Gz, MRIvox(mri_ctrl, Gx, Gy, Gz) > 0 ? "" : " NOT",val);
  }
  return(mri_ctrl) ;
}

MRI *
MRInormGentlyFindControlPoints(MRI *mri_src, int wm_target,
                               float intensity_above,
                               float intensity_below, MRI *mri_ctrl)
{
  int     width, height, depth, x, y, z, xk, yk, zk, xi, yi, zi;
  int     *pxi, *pyi, *pzi, ctrl, nctrl, val0, val, whalf = 0 ;
  BUFTYPE low_thresh, hi_thresh ;

  if (!wm_target)
    wm_target = DEFAULT_DESIRED_WHITE_MATTER_VALUE ;
  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  if (!mri_ctrl)
  {
    mri_ctrl = MRIalloc(width, height, depth, MRI_UCHAR) ;
    MRIcopyHeader(mri_src, mri_ctrl) ;
  }

  pxi = mri_src->xi ;
  pyi = mri_src->yi ;
  pzi = mri_src->zi ;
  /*
    find points which are close to wm_target, and in 7x7x7 relatively
    homogenous regions.
  */
  low_thresh = wm_target-1.5*intensity_below;
  hi_thresh =  wm_target+1.5*intensity_above;
  for (nctrl = z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;

        if (MRIvox(mri_ctrl, x, y, z) == CONTROL_MARKED)
        {
          nctrl++ ;
          continue ;  // caller specified this as a control point
        }
        val0 = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        if (val0 >= low_thresh && val0 <= hi_thresh)
        {
#ifdef WSIZE
#undef WSIZE
#endif
#ifdef WHALF
#undef WHALF
#endif
#define WSIZE   7
#define WHALF  ((WSIZE-1)/2)
          whalf = ceil(WHALF / mri_src->xsize) ;
          ctrl = 128 ;
          for (zk = -whalf ; ctrl && zk <= whalf ; zk++)
          {
            zi = pzi[z+zk] ;
            for (yk = -whalf ; ctrl && yk <= whalf ; yk++)
            {
              yi = pyi[y+yk] ;
              for (xk = -whalf ; ctrl && xk <= whalf ; xk++)
              {
                xi = pxi[x+xk] ;
                val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
                if (val > hi_thresh || val < low_thresh)
                  ctrl = 0 ;   /* not homogeneous enough */
              }
            }
          }
        }
        else
          ctrl = 0 ;
        if (ctrl)
          nctrl++ ;
        MRIvox(mri_ctrl, x, y, z) = ctrl ;
      }
    }
  }
  if (Gdiag & DIAG_SHOW)
    fprintf(stderr, "%d %dx%dx%d control points found\n",
            nctrl, 2*whalf+1, 2*whalf+1, 2*whalf+1) ;

  /*
    find points which are close to wm_target, and in x5x5x5 relatively
    homogenous regions.
  */
  low_thresh = wm_target-intensity_below;
  hi_thresh =  wm_target+intensity_above;
  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if ((int)MRIgetVoxVal(mri_ctrl, x, y, z, 0))/*already a controlpoint*/
          continue ;
        val0 = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        if (val0 >= low_thresh && val0 <= hi_thresh)
        {
#ifdef WSIZE
#undef WSIZE
#endif
#ifdef WHALF
#undef WHALF
#endif
#undef WSIZE
#define WSIZE   5
#define WHALF  ((WSIZE-1)/2)
          whalf = ceil(WHALF / mri_src->xsize) ;
          ctrl = 128 ;
          for (zk = -whalf ; ctrl && zk <= whalf ; zk++)
          {
            zi = pzi[z+zk] ;
            for (yk = -whalf ; ctrl && yk <= whalf ; yk++)
            {
              yi = pyi[y+yk] ;
              for (xk = -whalf ; ctrl && xk <= whalf ; xk++)
              {
                xi = pxi[x+xk] ;
                val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
                if (val > hi_thresh || val < low_thresh)
                  ctrl = 0 ;   /* not homogeneous enough */
              }
            }
          }
        }
        else
          ctrl = 0 ;
        if (ctrl)
          nctrl++ ;
        MRIvox(mri_ctrl, x, y, z) = ctrl ;
      }
    }
  }
  if (Gdiag & DIAG_SHOW)
    fprintf(stderr, "%d %dx%dx%d control points found above %d\n",
            nctrl, 2*whalf+1, 2*whalf+1, 2*whalf+1, low_thresh) ;

#undef WSIZE
#undef WHALF
  mriRemoveOutliers(mri_ctrl, 2) ;

  nctrl += MRInormAddFileControlPoints(mri_ctrl, 255) ;

  if (Gdiag & DIAG_SHOW)
    fprintf(stderr, "%d control points found.\n", nctrl) ;
  if (Gx >= 0 && Gy >= 0 && Gz >= 0)
  {
    float val = MRIgetVoxVal(mri_src, Gx, Gy, Gz, 0);
    printf("(%d, %d, %d) is%s a control point (T1=%2.0f)\n",
           Gx,Gy,Gz, MRIvox(mri_ctrl, Gx, Gy, Gz) > 0 ? "" : " NOT",val);
  }
  MRIbinarize(mri_ctrl, mri_ctrl, 1, 0, 1) ;
  return(mri_ctrl) ;
}

MRI *
MRInormFindControlPointsInWindow(MRI *mri_src,
                                 int wm_target,
                                 float intensity_above,
                                 float intensity_below,
                                 MRI *mri_ctrl,
                                 float whalf_mm,
                                 const char *debug_str,
                                 int *pnctrl,
                                 int  scan_type)
{
  int  width, height, depth, x, y, z, xk, yk, zk, xi, yi, zi, *pxi, *pyi;
  int  *pzi, ctrl, nctrl, val0, val, whalf;
  BUFTYPE low_thresh, hi_thresh ;

  whalf = nint(whalf_mm / mri_src->xsize) ;
  if (!wm_target)
    wm_target = DEFAULT_DESIRED_WHITE_MATTER_VALUE ;
  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  if (!mri_ctrl)
  {
    mri_ctrl = MRIalloc(width, height, depth, MRI_UCHAR) ;
    MRIcopyHeader(mri_src, mri_ctrl) ;
  }

  pxi = mri_src->xi ;
  pyi = mri_src->yi ;
  pzi = mri_src->zi ;
  /*
    find points which are close to wm_target, and in 7x7x7 relatively
    homogenous regions.
  */
  low_thresh = wm_target-intensity_below;
  hi_thresh =  wm_target+intensity_above;
  for (nctrl = z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (MRIvox(mri_ctrl, x, y, z) > 0) /* already a control point */
          continue ;
        val0 = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        if (val0 >= low_thresh && val0 <= hi_thresh)
        {
          ctrl = 128 ;
          for (zk = -whalf ; ctrl && zk <= whalf ; zk++)
          {
            zi = pzi[z+zk] ;
            for (yk = -whalf ; ctrl && yk <= whalf ; yk++)
            {
              yi = pyi[y+yk] ;
              for (xk = -whalf ; ctrl && xk <= whalf ; xk++)
              {
                xi = pxi[x+xk] ;
                val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
                if (val > hi_thresh || val < low_thresh)
                  ctrl = 0 ;   /* not homogeneous enough */
              }
            }
          }
        }
        else
          ctrl = 0 ;
        if (ctrl)
          nctrl++ ;
        MRIvox(mri_ctrl, x, y, z) = ctrl ;
      }
    }
  }
  if (Gdiag & DIAG_SHOW && debug_str != NULL)
    fprintf(stderr, "%s %d %dx%dx%d control points found\n",
            debug_str, nctrl, whalf*2+1,whalf*2+1,whalf*2+1) ;

  if (pnctrl)
    *pnctrl = nctrl ;
  return(mri_ctrl) ;
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
#if 0
MRI *
MRIbuildBiasImage(MRI *mri_src, MRI *mri_ctrl, MRI *mri_bias)
{
  int     width, height, depth ;
  MRI     *mri_s_ctrl, *mri_s_bias, *mri_s_src, *mri_tmp ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  mri_s_ctrl = mriDownsampleCtrl2(mri_ctrl, NULL) ;
  mri_s_src = MRIreduceByte(mri_src, NULL) ;
  mri_s_bias = MRIclone(mri_s_src, NULL) ;
  MRIbuildVoronoiDiagram(mri_s_src, mri_s_ctrl, mri_s_bias) ;
  MRIsoapBubble(mri_s_bias, mri_s_ctrl, mri_s_bias, 25) ;
  MRIfree(&mri_s_ctrl) ;
  MRIfree(&mri_s_src) ;
  mri_bias = MRIupsample2(mri_s_bias, mri_bias) ;

  MRIfree(&mri_s_bias) ;
  mri_tmp = MRImeanByte(mri_bias, NULL, 3) ;
  MRImeanByte(mri_tmp, mri_bias, 3) ;
  MRIfree(&mri_tmp) ;
  return(mri_bias) ;
}
#else
MRI *
MRIbuildBiasImage(MRI *mri_src, MRI *mri_ctrl, MRI *mri_bias, float sigma)
{
  int     width, height, depth, x,y,z ;
  MRI     *mri_kernel ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  mri_kernel = MRIgaussian1d(sigma, -1) ;
  mri_bias = MRIclone(mri_src, NULL) ;
  fprintf(stderr, "building Voronoi diagram...\n") ;
  MRIbuildVoronoiDiagram(mri_src, mri_ctrl, mri_bias) ;
  fprintf(stderr, "performing soap bubble smoothing...\n") ;
#if 1
  MRIconvolveGaussian(mri_bias, mri_bias, mri_kernel) ;
  if (mri_src->xsize > 0.9) /* replace smoothed bias
    with exact one for control points*/
  {
    for (x = 0 ; x < mri_src->width ; x++)
    {
      for (y = 0 ; y < mri_src->height ; y++)
      {
        for (z = 0 ; z < mri_src->depth ; z++)
        {
          if (MRIvox(mri_ctrl,x,y,z) > 0)
            MRIsetVoxVal(mri_bias,x,y,z, 0,
                         MRIgetVoxVal(mri_src,x,y,z,0)) ;
        }
      }
    }
  }
#else
MRIsoapBubble(mri_bias, mri_ctrl, mri_bias, 10) ;
#endif
  return(mri_bias) ;
}
#endif
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
#define NORMALIZE_INCLUDING_FIVE_WINDOW    0
#define NORMALIZE_INCLUDING_THREE_WINDOW   1
#define NORMALIZE_INCLUDING_SIX_CONNECTED  2

MRI *
MRI3dNormalize(MRI *mri_orig, MRI *mri_src, int wm_target, MRI *mri_norm,
               float intensity_above,
               float intensity_below,
               int only_file, int prune, float bias_sigma, int scan_type)
{
  int     width, height, depth, x, y, z, src, bias, n ;
  float   norm ;
  MRI     *mri_bias = NULL, *mri_ctrl ;
  /*    double  wm_std = 10 ;*/

  if (!wm_target)
    wm_target = DEFAULT_DESIRED_WHITE_MATTER_VALUE ;
  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (!mri_norm)
    mri_norm = MRIcopy(mri_src, NULL) ;
  mri_src = MRIcopy(mri_src, NULL) ;  /* make a copy of it */

  for (n = 2 ; n < 3 ; n++)
  {
    if (!only_file)
      mri_ctrl = MRInormFindControlPoints(mri_src, wm_target,
                                          intensity_above,
                                          intensity_below, NULL, n,
                                          scan_type);
    else
    {
      int nctrl ;

      mri_ctrl =
        MRIalloc(mri_src->width,
                 mri_src->height,
                 mri_src->depth,
                 MRI_UCHAR) ;
      MRIcopyHeader(mri_src, mri_ctrl) ;
      nctrl = MRInormAddFileControlPoints(mri_ctrl, 255) ;
      fprintf(stderr, "only using %d unique control points from file...\n",
              nctrl) ;
      if (getenv("WRITE_CONTROL_POINTS") != NULL)
      {
        printf("writing control point volume to cp.mgz\n") ;
        MRIwrite(mri_ctrl, "cp.mgz") ;
      }
    }

    if (control_volume_fname)
    {
      fprintf(stderr, "writing control point volume to %s...\n",
              control_volume_fname) ;
      MRIcopyHeader(mri_src, mri_ctrl) ;
      MRIwrite(mri_ctrl, control_volume_fname) ;
    }
    MRIbinarize(mri_ctrl, mri_ctrl, 1, CONTROL_NONE, CONTROL_MARKED) ;
    if (prune > 0)
      remove_gray_matter_control_points(mri_ctrl, mri_orig,
                                        wm_target,
                                        intensity_above, intensity_below,
                                        scan_type) ;

#if 0
    if (n == 0)
      wm_std = mriNormComputeWMStandardDeviation(mri_orig, mri_ctrl) ;
    if (n >= 1)
      remove_extreme_control_points(mri_orig, mri_ctrl,
                                    wm_target-intensity_below,
                                    wm_target+intensity_above,
                                    wm_target, wm_std) ;
#endif

    mri_bias = MRIbuildBiasImage(mri_src, mri_ctrl, mri_bias, bias_sigma) ;
    if (bias_volume_fname)
    {
      fprintf(stderr, "writing bias field volume to %s...\n",
              bias_volume_fname) ;
      MRIwrite(mri_bias, bias_volume_fname) ;
    }
    if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    {
      static int pass = 0 ;
      char fname[STRLEN] ;

      fprintf(stderr, "writing out control and bias volumes...\n") ;
      sprintf(fname, "src%d.mgh", pass) ;
      MRIwrite(mri_src, fname) ;
      sprintf(fname, "ctrl%d.mgh", pass) ;
      MRIwrite(mri_ctrl, fname) ;
      sprintf(fname, "bias%d.mgh", pass) ;
      MRIwrite(mri_bias, fname) ;
      pass++ ;
    }

    for (z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        for (x = 0 ; x < width ; x++)
        {
          src = MRIgetVoxVal(mri_src, x, y, z, 0) ;
          bias = MRIgetVoxVal(mri_bias, x, y, z, 0) ;
          if (!bias)   /* should never happen */
            norm = (float)src ;
          else
            norm = (float)src * (float)wm_target / (float)bias ;
          if (norm > 255.0f && mri_norm->type == MRI_UCHAR)
            norm = 255.0f ;
          MRIsetVoxVal(mri_norm, x, y, z, 0, norm) ;
          if (x == Gx && y == Gy && z == Gz)
          {
            if (Gx >= 0)
              printf("bias at (%d, %d, %d) = %d, T1 %d --> %d\n",
                     Gx,Gy,Gz,bias,src,nint(norm)) ;
            DiagBreak() ;
          }
        }
      }
    }
    MRIfree(&mri_ctrl) ;
    if (only_file)
      break ;
    MRIcopy(mri_norm, mri_src) ;   /* for next iteration */
  }
  MRIfree(&mri_bias) ;
  MRIfree(&mri_src) ;  /* NOT the one passed in - a copy for internal use */
  return(mri_norm) ;
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRI3dGentleNormalize(MRI *mri_src,
                     MRI *mri_bias,
                     int wm_target,
                     MRI *mri_norm,
                     float intensity_above,
                     float intensity_below,
                     int only_file,
                     float bias_sigma)
{
  int     width, height, depth, x, y, z, src, bias, free_bias ;
  float   norm ;

  if (!wm_target)
    wm_target = DEFAULT_DESIRED_WHITE_MATTER_VALUE ;
  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  if (!mri_norm)
    mri_norm = MRIclone(mri_src, NULL) ;

  if (!mri_bias)
  {
    MRI  *mri_ctrl ;

    free_bias = 1 ;
    if (!only_file)
      mri_ctrl = MRInormGentlyFindControlPoints(mri_src, wm_target,
                 intensity_above,
                 intensity_below, NULL);
    else
    {
      int nctrl ;
      double mean ;

      mri_ctrl = MRIalloc(mri_src->width,
                          mri_src->height,
                          mri_src->depth,
                          MRI_UCHAR) ;
      MRIcopyHeader(mri_src, mri_ctrl) ;
      nctrl = MRInormAddFileControlPoints(mri_ctrl, 255) ;
      mean = MRImeanInLabel(mri_src, mri_ctrl, 255) ;
      if (nctrl == 0 || FZERO(mean))
      {
        MRIcopy(mri_src, mri_norm) ;
        ErrorReturn(mri_norm, (ERROR_BADPARM,
                               "MRI3dGentleNormalize: mean = %2.1f, nctrl = %d, norm failed",
                               mean, nctrl)) ;
      }
      fprintf(stderr,
              "only using %d control points from file, mean %2.1f, scaling by %2.2f...\n", 
              nctrl, mean, wm_target/mean) ;
      MRIscalarMul(mri_src, mri_src, wm_target/mean) ;
      if (getenv("WRITE_CONTROL_POINTS") != NULL)
      {
        printf("writing control point volume to c.mgz\n") ;
        MRIwrite(mri_ctrl, "c.mgz") ;
      }
      if (Gdiag & DIAG_WRITE)
      {
        printf("writing scaled intensity volume to scaled.mgz\n") ;
        MRIwrite(mri_src, "scaled.mgz") ;
      }
    }

    if (mri_ctrl == NULL)
    {
      fprintf(stderr, "MRI3dGentleNormalize failure!  mri_ctrl=NULL \n");
      exit (1);
    }

    if (control_volume_fname)
    {
      fprintf(stderr, "writing control point volume to %s...\n",
              control_volume_fname) ;
      MRIwrite(mri_ctrl, control_volume_fname) ;
    }
    MRIbinarize(mri_ctrl, mri_ctrl, 1, CONTROL_NONE, CONTROL_MARKED) ;
    mri_bias = MRIbuildBiasImage(mri_src, mri_ctrl, NULL, bias_sigma) ;
    if (bias_volume_fname)
    {
      fprintf(stderr, "writing bias field volume to %s...\n",
              bias_volume_fname) ;
      MRIwrite(mri_bias, bias_volume_fname) ;
    }
    if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    {
      static int pass = 0 ;
      char fname[500] ;

      fprintf(stderr, "writing out control and bias volumes...\n") ;
      sprintf(fname, "src%d.mgz", pass) ;
      MRIwrite(mri_src, fname) ;
      sprintf(fname, "ctrl%d.mgz", pass) ;
      MRIwrite(mri_ctrl, fname) ;
      sprintf(fname, "bias%d.mgz", pass) ;
      MRIwrite(mri_bias, fname) ;
      pass++ ;
    }
    MRIfree(&mri_ctrl) ;
  }
  else
    free_bias = 0 ;

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        src = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        bias = MRIgetVoxVal(mri_bias, x, y, z, 0) ;
        if (!bias)   /* should never happen */
          norm = (float)src ;
        else
          norm = (float)src * (float)wm_target / (float)bias ;
        if (norm > 255.0f && mri_norm->type == MRI_UCHAR)
          norm = 255.0f ;
        MRIsetVoxVal(mri_norm, x, y, z, 0, norm) ;
      }
    }
  }
  if (free_bias)
    MRIfree(&mri_bias) ;

  return(mri_norm) ;
}


/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
static MRI *
mriBuildVoronoiDiagramShort(MRI *mri_src, MRI *mri_ctrl, MRI *mri_dst)
{
  int     width, height, depth, x, y, z, xk, yk, zk, xi, yi, zi;
  int     *pxi, *pyi, *pzi, nchanged, n, total, visited ;
  BUFTYPE *pmarked, *pctrl, ctrl, mark ;
  short   *psrc, *pdst ;
  float   src, val, mean ;
  MRI     *mri_marked ;
  float   scale ;

  if (mri_src->type != MRI_SHORT || 
      mri_ctrl->type != MRI_UCHAR || 
      mri_dst->type != MRI_SHORT)
    ErrorExit(ERROR_UNSUPPORTED,
              "mriBuildVoronoiDiagramShort: incorrect input type(s)") ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  scale = mri_src->width / mri_dst->width ;
  pxi = mri_src->xi ;
  pyi = mri_src->yi ;
  pzi = mri_src->zi ;

  /* initialize dst image */
  for (total = z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      psrc = &MRISvox(mri_src, 0, y, z) ;
      pctrl = &MRIvox(mri_ctrl, 0, y, z) ;
      pdst = &MRISvox(mri_dst, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        ctrl = *pctrl++ ;
        src = (float)*psrc++ ;
        if (!ctrl)
          val = 0 ;
        else   /* find mean in region,
                  and use it as bias field estimate */
        {
          val = src ;
          total++ ;
          val = MRISvox(mri_src, x, y, z) ;/* it's already reduced,don't avg*/
        }
        *pdst++ = val ;
      }
    }
  }

  total = width*height*depth - total ;  /* total # of voxels to be processed */
  mri_marked = MRIcopy(mri_ctrl, NULL) ;
  MRIreplaceValues(mri_marked, mri_marked, CONTROL_MARKED, CONTROL_NBR) ;

  /* now propagate values outwards */
  do
  {
    nchanged = 0 ;
    /*
      use mri_marked to keep track of the last set of voxels changed which
      are marked CONTROL_MARKED. The neighbors of these will be marked
      with CONTROL_TMP, and they are the only ones which need to be
      examined.
    */
#if 0
    MRIreplaceValues(mri_ctrl, mri_ctrl, CONTROL_HAS_VAL, CONTROL_TMP) ;
    mriMarkUnmarkedNeighbors(mri_marked, mri_ctrl, mri_marked,
                             CONTROL_MARKED,
                             CONTROL_TMP);
    MRIreplaceValues(mri_marked, mri_marked, CONTROL_MARKED, CONTROL_NONE) ;
    MRIreplaceValues(mri_marked, mri_marked, CONTROL_TMP, CONTROL_MARKED) ;
#else
    /* voxels that were CONTROL_TMP were filled on
    previous iteration, now they
    should be labeled as marked
    */
    /* only process nbrs of marked values (that aren't already marked) */
    mriMarkUnmarkedNeighbors(mri_marked, mri_marked, mri_marked,
                             CONTROL_NBR, CONTROL_TMP);
    MRIreplaceValues(mri_marked, mri_marked, CONTROL_NBR, CONTROL_MARKED) ;
    MRIreplaceValues(mri_marked, mri_marked, CONTROL_TMP, CONTROL_NBR) ;
#endif

    /*
      everything in mri_marked=CONTROL_TMP is now a nbr of a point
      with a value.
    */
    for (visited = z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        pmarked = &MRIvox(mri_marked, 0, y, z) ;
        pdst = &MRISvox(mri_dst, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          if (x == Gx && y == Gy && z == Gz)
            DiagBreak() ;
          mark = *pmarked++ ;
          if (mark != CONTROL_NBR)  /* not a neighbor of a marked point */
          {
            pdst++ ;
            ;
            continue ;
          }

          /* now see if any neighbors are on and set this voxel to the
             average of the marked neighbors (if any) */
          visited++ ;
          mean = 0.0f ;
          n = 0 ;
          for (zk = -1 ; zk <= 1 ; zk++)
          {
            zi = pzi[z+zk] ;
            for (yk = -1 ; yk <= 1 ; yk++)
            {
              yi = pyi[y+yk] ;
              for (xk = -1 ; xk <= 1 ; xk++)
              {
                xi = pxi[x+xk] ;
                if (MRIvox(mri_marked, xi, yi, zi)
                    == CONTROL_MARKED)
                {
                  n++ ;
                  mean += (float)MRISvox(mri_dst, xi, yi, zi) ;
                }
              }
            }
          }
          if (n > 0)  /* some neighbors were on */
          {
            *pdst++ = nint(mean / (float)n) ;
            nchanged++ ;
          }
          else   /* should never happen anymore */
            pdst++ ;
        }
      }
    }
    total -= nchanged ;
    if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
      fprintf(stderr,
              "Voronoi: %d voxels assigned, "
              "%d remaining, %d visited.      \n",
              nchanged, total, visited) ;
  }
  while (nchanged > 0 && total > 0) ;

  if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
    fprintf(stderr, "\n") ;
  MRIfree(&mri_marked) ;
  MRIreplaceValues(mri_ctrl, mri_ctrl, CONTROL_TMP, CONTROL_NONE) ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    MRIwrite(mri_ctrl, "ctrl.mgh") ;
  return(mri_dst) ;
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
static MRI *
mriBuildVoronoiDiagramFloat(MRI *mri_src, MRI *mri_ctrl, MRI *mri_dst)
{
  int     width, height, depth, x, y, z, xk, yk, zk, xi, yi, zi;
  int     *pxi, *pyi, *pzi, nchanged, n, total, visited ;
  BUFTYPE ctrl, mark ;
  float   src, val, mean, *pdst, *psrc ;
  MRI     *mri_marked ;
  float   scale ;

  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;
  if (mri_src->type != MRI_FLOAT || 
      mri_ctrl->type != MRI_UCHAR || 
      mri_dst->type != MRI_FLOAT)
    ErrorExit(ERROR_UNSUPPORTED,
              "mriBuildVoronoiDiagramFloat: incorrect input type(s)") ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  scale = mri_src->width / mri_dst->width ;
  pxi = mri_src->xi ;
  pyi = mri_src->yi ;
  pzi = mri_src->zi ;

  /* initialize dst image */
  for (total = z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      psrc = &MRIFvox(mri_src, 0, y, z) ;
      pdst = &MRIFvox(mri_dst, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        ctrl = MRIgetVoxVal(mri_ctrl, x, y, z, 0) ;
        src = (float)*psrc++ ;
        if (!ctrl)
          val = 0 ;
        else   /* find mean in region, and use
                  it as bias field estimate */
        {
          val = src ;
          total++ ;
          val = MRIFvox(mri_src, x, y, z) ;/* it's already reduced, don't avg*/
        }
        *pdst++ = val ;
      }
    }
  }

  total = width*height*depth - total ;  /* total # of voxels to be processed */
  mri_marked = MRIcopy(mri_ctrl, NULL) ;
  MRIreplaceValues(mri_marked, mri_marked, CONTROL_MARKED, CONTROL_NBR) ;

  /* now propagate values outwards */
  do
  {
    nchanged = 0 ;
    /*
      use mri_marked to keep track of the last set of voxels changed which
      are marked CONTROL_MARKED. The neighbors of these will be marked
      with CONTROL_TMP, and they are the only ones which need to be
      examined.
    */
#if 0
    MRIreplaceValues(mri_ctrl, mri_ctrl, CONTROL_HAS_VAL, CONTROL_TMP) ;
    mriMarkUnmarkedNeighbors(mri_marked, mri_ctrl, mri_marked,
                             CONTROL_MARKED,
                             CONTROL_TMP);
    MRIreplaceValues(mri_marked, mri_marked, CONTROL_MARKED, CONTROL_NONE) ;
    MRIreplaceValues(mri_marked, mri_marked, CONTROL_TMP, CONTROL_MARKED) ;
#else
    /* voxels that were CONTROL_TMP were
    filled on previous iteration, now they
    should be labeled as marked
    */
    /* only process nbrs of marked values (that aren't already marked) */
    mriMarkUnmarkedNeighbors(mri_marked, mri_marked, mri_marked,
                             CONTROL_NBR, CONTROL_TMP);
    MRIreplaceValues(mri_marked, mri_marked, CONTROL_NBR, CONTROL_MARKED) ;
    MRIreplaceValues(mri_marked, mri_marked, CONTROL_TMP, CONTROL_NBR) ;
#endif

    /*
      everything in mri_marked=CONTROL_TMP is now a nbr of a point
      with a value.
    */
    for (visited = z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        pdst = &MRIFvox(mri_dst, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          if (x == Gx && y == Gy && z == Gz)
            DiagBreak() ;
          mark = MRIgetVoxVal(mri_marked, x, y, z, 0) ;
          if (mark != CONTROL_NBR)  /* not a neighbor
                                       of a marked point */
          {
            pdst++ ;
            ;
            continue ;
          }

          /* now see if any neighbors are on and set this voxel to the
             average of the marked neighbors (if any) */
          visited++ ;
          mean = 0.0f ;
          n = 0 ;
          for (zk = -1 ; zk <= 1 ; zk++)
          {
            zi = pzi[z+zk] ;
            for (yk = -1 ; yk <= 1 ; yk++)
            {
              yi = pyi[y+yk] ;
              for (xk = -1 ; xk <= 1 ; xk++)
              {
                xi = pxi[x+xk] ;
                if (MRIgetVoxVal(mri_marked, xi, yi, zi, 0)
                    == CONTROL_MARKED)
                {
                  n++ ;
                  mean +=
                    MRIgetVoxVal(mri_dst, xi, yi, zi, 0) ;
                }
              }
            }
          }
          if (n > 0)  /* some neighbors were on */
          {
            *pdst++ = (mean / (float)n) ;
            nchanged++ ;
          }
          else   /* should never happen anymore */
            pdst++ ;
        }
      }
    }
    total -= nchanged ;
    if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
      fprintf(stderr,
              "Voronoi: %d voxels assigned, %d remaining, %d visited.\n",
              nchanged, total, visited) ;
  }
  while (nchanged > 0 && total > 0) ;

  if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
    fprintf(stderr, "\n") ;
  MRIfree(&mri_marked) ;
  MRIreplaceValues(mri_ctrl, mri_ctrl, CONTROL_TMP, CONTROL_NONE) ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    MRIwrite(mri_ctrl, "ctrl.mgh") ;
  return(mri_dst) ;
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
static MRI *
mriBuildVoronoiDiagramUchar(MRI *mri_src, MRI *mri_ctrl, MRI *mri_dst)
{
  int     width, height, depth, x, y, z, xk, yk, zk, xi, yi, zi;
  int     *pxi, *pyi, *pzi, nchanged, n, total, visited ;
  int     ctrl, mark ;
  BUFTYPE *psrc, *pdst ;
  float   src, val, mean ;
  MRI     *mri_marked ;
  float   scale ;

  if (mri_src->type != MRI_UCHAR || 
      mri_dst->type != MRI_UCHAR)
    ErrorExit(ERROR_UNSUPPORTED,
              "mriBuildVoronoiDiagramUchar: incorrect input type(s)") ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  scale = mri_src->width / mri_dst->width ;
  pxi = mri_src->xi ;
  pyi = mri_src->yi ;
  pzi = mri_src->zi ;

  /* initialize dst image */
  for (total = z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      psrc = &MRIvox(mri_src, 0, y, z) ;
      pdst = &MRIvox(mri_dst, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        ctrl = (int)MRIgetVoxVal(mri_ctrl, x,y,z, 0);

        if (ctrl > 255){
          printf("ctrl=%d\n",ctrl);


        }




        src = (float)*psrc++ ;
        if (!ctrl)
          val = 0 ;
        else   /* find mean in region, and use it as bias field estimate */
        {
          val = src ;
          total++ ;
          val = MRIvox(mri_src, x, y, z) ;/* it's already reduced, don't avg*/
        }
        *pdst++ = val ;
      }
    }
  }

  total = width*height*depth - total ;  /* total # of voxels to be processed */
  mri_marked = MRIcopy(mri_ctrl, NULL) ;
  MRIreplaceValues(mri_marked, mri_marked, CONTROL_MARKED, CONTROL_NBR) ;

  /* now propagate values outwards */
  int counter = 0;
  do
  {
    nchanged = 0 ;
    /*
      use mri_marked to keep track of the last set of voxels changed which
      are marked CONTROL_MARKED. The neighbors of these will be marked
      with CONTROL_TMP, and they are the only ones which need to be
      examined.
    */
#if 0
    MRIreplaceValues(mri_ctrl, mri_ctrl, CONTROL_HAS_VAL, CONTROL_TMP) ;
    mriMarkUnmarkedNeighbors(mri_marked, mri_ctrl, mri_marked,
                             CONTROL_MARKED,
                             CONTROL_TMP);
    MRIreplaceValues(mri_marked, mri_marked, CONTROL_MARKED, CONTROL_NONE) ;
    MRIreplaceValues(mri_marked, mri_marked, CONTROL_TMP, CONTROL_MARKED) ;
#else
    /* voxels that were CONTROL_TMP were filled
    on previous iteration, now they
    should be labeled as marked
    */
    /* only process nbrs of marked values (that aren't already marked) */
    mriMarkUnmarkedNeighbors(mri_marked, mri_marked, mri_marked,
                             CONTROL_NBR, CONTROL_TMP);
    MRIreplaceValues(mri_marked, mri_marked, CONTROL_NBR, CONTROL_MARKED) ;
    MRIreplaceValues(mri_marked, mri_marked, CONTROL_TMP, CONTROL_NBR) ;
#endif

    /*
      everything in mri_marked=CONTROL_TMP is now a nbr of a point
      with a value.
    */
    for (visited = z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        pdst = &MRIvox(mri_dst, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          if (x == Gx && y == Gy && z == Gz)
            DiagBreak() ;
          mark = (int)MRIgetVoxVal(mri_marked, x,y,z, 0);
          if (mark != CONTROL_NBR)  /* not a neighbor of a marked point */
          {
            pdst++ ;
            ;
            continue ;
          }

          /* now see if any neighbors are on and set this voxel to the
             average of the marked neighbors (if any) */
          visited++ ;
          mean = 0.0f ;
          n = 0 ;
          for (zk = -1 ; zk <= 1 ; zk++)
          {
            zi = pzi[z+zk] ;
            for (yk = -1 ; yk <= 1 ; yk++)
            {
              yi = pyi[y+yk] ;
              for (xk = -1 ; xk <= 1 ; xk++)
              {
                xi = pxi[x+xk] ;
                if ((int)MRIgetVoxVal(mri_marked, xi,yi,zi, 0) ==
                    CONTROL_MARKED)
                {
                  n++ ;
                  mean += (float)MRIvox(mri_dst, xi, yi, zi) ;
                }
              }
            }
          }
          if (n > 0)  /* some neighbors were on */
          {
            *pdst++ = mean / (float)n ;
            nchanged++ ;
          }
          else   /* should never happen anymore */
            pdst++ ;
        }
      }
    }
    total -= nchanged ;
    if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
      fprintf(stderr,
              "Voronoi: %d voxels assigned, %d remaining, %d visited.\n",
              nchanged, total, visited) ;
    //sprintf(tmpstr, "/tmp/tmp_dst_%d.mgz", counter); MRIwrite(mri_dst, tmpstr);
    counter ++;
  }
  while (nchanged > 0 && total > 0) ;

  if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
    fprintf(stderr, "\n") ;
  MRIfree(&mri_marked) ;
  MRIreplaceValues(mri_ctrl, mri_ctrl, CONTROL_TMP, CONTROL_NONE) ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    MRIwrite(mri_ctrl, "ctrl.mgh") ;
  return(mri_dst) ;
}



/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIbuildVoronoiDiagram(MRI *mri_src, MRI *mri_ctrl, MRI *mri_dst)
{
  switch (mri_src->type)
  {
  case MRI_FLOAT:
    return(mriBuildVoronoiDiagramFloat(mri_src, mri_ctrl, mri_dst));
  case MRI_SHORT:
    return(mriBuildVoronoiDiagramShort(mri_src, mri_ctrl, mri_dst));
  case MRI_UCHAR:
    return(mriBuildVoronoiDiagramUchar(mri_src, mri_ctrl, mri_dst));
  default:
    break ;
  }
  ErrorReturn(NULL,
              (ERROR_UNSUPPORTED,
               "MRIbuildVoronoiDiagram: src type %d unsupported",
               mri_src->type)) ;
}



/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIsoapBubble(MRI *mri_src, MRI *mri_ctrl, MRI *mri_dst,int niter)
{
  int     width, height, depth, frames, x, y, z, f, xk, yk, zk, xi, yi, zi, i,
  *pxi, *pyi, *pzi, mean ;
  BUFTYPE *pctrl, ctrl, *ptmp ;
  MRI     *mri_tmp ;


  if (mri_src->type == MRI_FLOAT)
    return(mriSoapBubbleFloat(mri_src, mri_ctrl, mri_dst, niter)) ;
  else if (mri_src->type == MRI_SHORT)
    return(mriSoapBubbleShort(mri_src, mri_ctrl, mri_dst, niter)) ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  frames = mri_src->nframes ;

  if (!mri_dst)
    mri_dst = MRIcopy(mri_src, NULL) ;

  pxi = mri_src->xi ;
  pyi = mri_src->yi ;
  pzi = mri_src->zi ;

  mri_tmp = MRIcopy(mri_dst, NULL) ;

  /* now propagate values outwards */
  for (f = 0; f < frames; f++) // NOTE: not very efficient with the multi-frame, but testing for now.
    {
      for (i = 0 ; i < niter ; i++)
	{
	  if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
	    fprintf(stderr, "soap bubble iteration %d of %d\n", i+1, niter) ;
	  for ( z = 0 ; z < depth ; z++)
	    {
	      for (y = 0 ; y < height ; y++)
		{
		  pctrl = &MRIvox(mri_ctrl, 0, y, z) ;
		  ptmp = &MRIseq_vox(mri_tmp, 0, y, z, f) ;
		  for (x = 0 ; x < width ; x++)
		    {
		      ctrl = *pctrl++ ;
		      if (ctrl == CONTROL_MARKED)   /* marked point - don't change it */
			{
			  ptmp++ ;
			  continue ;
			}
		      /* now set this voxel to the average
			 of the marked neighbors */
		      mean = 0 ;
		      for (zk = -1 ; zk <= 1 ; zk++)
			{
			  zi = pzi[z+zk] ;
			  for (yk = -1 ; yk <= 1 ; yk++)
			    {
			      yi = pyi[y+yk] ;
			      for (xk = -1 ; xk <= 1 ; xk++)
				{
				  xi = pxi[x+xk] ;
				  mean += MRIseq_vox(mri_dst, xi, yi, zi, f) ;
				}
			    }
			}
		      *ptmp++ = nint((float)mean / (3.0f*3.0f*3.0f)) ;
		    }
		}
	    } //z
	  MRIcopy(mri_tmp, mri_dst) ;
	} // iter
    } //frames
  
  MRIfree(&mri_tmp) ;

  /*MRIwrite(mri_dst, "soap.mnc") ;*/
  return(mri_dst) ;
}


/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIaverageFixedPoints(MRI *mri_src, MRI *mri_ctrl, MRI *mri_dst,int niter)
{
  int     width, height, depth, x, y, z, xk, yk, zk, xi, yi, zi, i,
  *pxi, *pyi, *pzi, mean, num ;
  BUFTYPE *pctrl, ctrl, *ptmp ;
  MRI     *mri_tmp ;


  if (mri_src->type != MRI_UCHAR)
    ErrorExit(ERROR_UNSUPPORTED,
              "MRIaverageFixedPoints: only works on UCHAR") ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  if (!mri_dst)
    mri_dst = MRIcopy(mri_src, NULL) ;

  pxi = mri_src->xi ;
  pyi = mri_src->yi ;
  pzi = mri_src->zi ;

  mri_tmp = MRIcopy(mri_dst, NULL) ;

  /* now propagate values outwards */
  for (i = 0 ; i < niter ; i++)
  {
    if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
      fprintf(stderr, "soap bubble iteration %d of %d\n", i+1, niter) ;
    for ( z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        pctrl = &MRIvox(mri_ctrl, 0, y, z) ;
        ptmp = &MRIvox(mri_tmp, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          ctrl = *pctrl++ ;
          if (ctrl != CONTROL_MARKED) /* marked point - don't change it */
          {
            ptmp++ ;
            continue ;
          }
          /* now set this voxel to the average
             of the marked neighbors */
          mean = 0 ;
          num = 0 ;
          for (zk = -1 ; zk <= 1 ; zk++)
          {
            zi = pzi[z+zk] ;
            for (yk = -1 ; yk <= 1 ; yk++)
            {
              yi = pyi[y+yk] ;
              for (xk = -1 ; xk <= 1 ; xk++)
              {
                xi = pxi[x+xk] ;
                if (MRIvox(mri_ctrl, xi, yi, zi))
                {
                  mean += MRIvox(mri_dst, xi, yi, zi) ;
                  num++ ;
                }
              }
            }
          }
          *ptmp++ = nint((float)mean / (float)num) ;
        }
      }
    }
    MRIcopy(mri_tmp, mri_dst) ;
  }

  MRIfree(&mri_tmp) ;

  /*MRIwrite(mri_dst, "soap.mnc") ;*/
  return(mri_dst) ;
}


/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
MRI *
MRIsoapBubbleExpand(MRI *mri_src, MRI *mri_ctrl, MRI *mri_dst,int niter)
{
  int     width, height, depth, x, y, z, xk, yk, zk, xi, yi, zi, i,
  *pxi, *pyi, *pzi ;
  BUFTYPE *pctrl, ctrl, *ptmp ;
  MRI     *mri_tmp ;
  float   mean, nvox ;

  if (mri_src->type == MRI_FLOAT)
    return(mriSoapBubbleExpandFloat(mri_src, mri_ctrl, mri_dst, niter)) ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  if (!mri_dst)
    mri_dst = MRIcopy(mri_src, NULL) ;

  pxi = mri_src->xi ;
  pyi = mri_src->yi ;
  pzi = mri_src->zi ;

  mri_tmp = MRIcopy(mri_dst, NULL) ;

  /* now propagate values outwards */
  for (i = 0 ; i < niter ; i++)
  {
    if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
      fprintf(stderr,
              "soap bubble expansion iteration %d of %d\n", i+1, niter) ;
    for ( z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        pctrl = &MRIvox(mri_ctrl, 0, y, z) ;
        ptmp = &MRIvox(mri_tmp, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          ctrl = *pctrl++ ;
          if (ctrl == CONTROL_MARKED)   /* marked point - don't change it */
          {
            ptmp++ ;
            continue ;
          }
          /* now set this voxel to the average
             of the marked neighbors */
          mean = nvox = 0.0f ;
          for (zk = -1 ; zk <= 1 ; zk++)
          {
            zi = pzi[z+zk] ;
            for (yk = -1 ; yk <= 1 ; yk++)
            {
              yi = pyi[y+yk] ;
              for (xk = -1 ; xk <= 1 ; xk++)
              {
                xi = pxi[x+xk] ;
                if (MRIvox(mri_ctrl, xi, yi, zi) ==
                    CONTROL_MARKED)
                {
                  mean += (float)MRIvox(mri_dst, xi, yi, zi) ;
                  nvox++ ;
                }
              }
            }
          }
          if (nvox)
          {
            *ptmp++ = (BUFTYPE)nint(mean / nvox) ;
            MRIvox(mri_ctrl, x, y, z) = CONTROL_TMP ;
          }
          else
            ptmp++ ;
        }
      }
    }
    MRIcopy(mri_tmp, mri_dst) ;
    for ( z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        pctrl = &MRIvox(mri_ctrl, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          ctrl = *pctrl ;
          if (ctrl == CONTROL_TMP)
            ctrl = CONTROL_MARKED ;
          *pctrl++ = ctrl ;
        }
      }
    }
  }

  MRIfree(&mri_tmp) ;

  /*MRIwrite(mri_dst, "soap.mnc") ;*/
  return(mri_dst) ;
}


/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
#if 0
static MRI *
mriSoapBubbleFloat(MRI *mri_src, MRI *mri_ctrl, MRI *mri_dst,int niter)
{
  int     width, height, depth, x, y, z, xk, yk, zk, xi, yi, zi, i;
  int     *pxi, *pyi, *pzi ;
  BUFTYPE ctrl ;
  float   *ptmp, mean ;
  MRI     *mri_tmp ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  if (!mri_dst)
    mri_dst = MRIcopy(mri_src, NULL) ;

  pxi = mri_src->xi ;
  pyi = mri_src->yi ;
  pzi = mri_src->zi ;

  mri_tmp = MRIcopy(mri_dst, NULL) ;

  /* now propagate values outwards */
  for (i = 0 ; i < niter ; i++)
  {
    if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
      fprintf(stderr, "soap bubble iteration %d of %d\n", i+1, niter) ;
    for ( z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        ptmp = &MRIFvox(mri_tmp, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          if (x == Gx && y == Gy && z == Gz)
            DiagBreak() ;
          ctrl = MRIgetVoxVal(mri_ctrl, x, y, z, 0) ;
          if (ctrl == CONTROL_MARKED)   /* marked point - don't change it */
          {
            ptmp++ ;
            continue ;
          }
          /* now set this voxel to the average of
             the marked neighbors */
          /* where is the check for marked or non-marked? -xh */
          mean = 0.0f ;
          for (zk = -1 ; zk <= 1 ; zk++)
          {
            zi = pzi[z+zk] ;
            for (yk = -1 ; yk <= 1 ; yk++)
            {
              yi = pyi[y+yk] ;
              for (xk = -1 ; xk <= 1 ; xk++)
              {
                xi = pxi[x+xk] ;
                mean += MRIFvox(mri_dst, xi, yi, zi) ;
              }
            }
          }
          *ptmp++ = (float)mean / (3.0f*3.0f*3.0f) ;
        }
      }
    }
    MRIcopy(mri_tmp, mri_dst) ;
  }

  MRIfree(&mri_tmp) ;

  /*MRIwrite(mri_dst, "soap.mnc") ;*/
  return(mri_dst) ;
}
#else
static MRI *
mriSoapBubbleFloat(MRI *mri_src, MRI *mri_ctrl, MRI *mri_dst,int niter)
{
  int     width, height, depth, frames, x, y, z, f, xk, yk, zk, xi, yi, zi, i,
          *pxi, *pyi, *pzi, num, x1, y1, z1, x2, y2, z2 ;
  BUFTYPE *pctrl, ctrl ;
  float   *ptmp ;
  float     mean ;
  MRI     *mri_tmp ;
  MRI_REGION box ;

  if (mri_ctrl->type != MRI_UCHAR)
    ErrorExit(ERROR_UNSUPPORTED, "mriSoapBubbleFloat: ctrl must be UCHAR");
  
  width = mri_src->width ; 
  height = mri_src->height ; 
  depth = mri_src->depth ;
  frames = mri_src->nframes;

  if (!mri_dst)
    mri_dst = MRIcopy(mri_src, NULL) ;

  pxi = mri_src->xi ; 
  pyi = mri_src->yi ; 
  pzi = mri_src->zi ;

  mri_tmp = MRIcopy(mri_dst, NULL) ;

#ifdef WHALF 
#undef WHALF
#endif
#define WHALF 2

  MRIboundingBox(mri_ctrl, CONTROL_MARKED-1, &box) ;
  x1 = box.x ; x2 = box.x+box.dx-1 ;
  y1 = box.y ; y2 = box.y+box.dy-1 ;
  z1 = box.z ; z2 = box.z+box.dz-1 ;

  for (f = 0; f < frames; f++) // NOTE: not very efficient with the multi-frame, but testing for now.
    {
      for ( z = MAX(0,z1-WHALF) ; z <= MIN(z2+WHALF,depth-1) ; z++)
	{
	  for (y = MAX(0,y1-WHALF) ; y <= MIN(y2+WHALF,height-1) ; y++)
	    {
	      pctrl = &MRIvox(mri_ctrl, MAX(0,x1-WHALF), y, z) ;
	      ptmp = &MRIFseq_vox(mri_tmp, MAX(x1-WHALF, 0), y, z, f) ;
	      for (x = MAX(0,x1-WHALF) ; x <= MIN(x2+WHALF,width-1) ; x++)
		{
		  ctrl = *pctrl++ ;
		  if (ctrl == CONTROL_MARKED)
		    continue ;
		  num = mean = 0 ;
		  for (zk = -WHALF ; zk <= WHALF ; zk++)
		    {
		      zi = pzi[z+zk] ;
		      for (yk = -WHALF ; yk <= WHALF ; yk++)
			{
			  yi = pyi[y+yk] ;
			  for (xk = -WHALF ; xk <= WHALF ; xk++)
			    {
			      xi = pxi[x+xk] ;
			      if (MRIvox(mri_ctrl, xi, yi, zi) != CONTROL_MARKED)
				continue ;
			      mean += MRIFseq_vox(mri_dst, xi, yi, zi, f) ;
			      num++ ;
			    }
			}
		    }
		  if (num > 0)
		    MRIFseq_vox(mri_dst, x, y, z, f) = (float)mean / (float)num;
		  // MRIFvox(mri_dst, x, y, z) =
		  //   (float)nint((float)mean / (float)num);
		}
	    }
	}
      
      /* now propagate values outwards */
      for (i = 0 ; i < niter ; i++)
	{
	  if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
	    fprintf(stderr, "soap bubble iteration %d of %d\n", i+1, niter) ;
	  for ( z = z1 ; z <= z2 ; z++)
	    {
	      for (y = y1 ; y <= y2 ; y++)
		{
		  pctrl = &MRIvox(mri_ctrl, x1, y, z) ;
		  ptmp = &MRIFseq_vox(mri_tmp, x1, y, z, f) ;
		  for (x = x1 ; x <= x2 ; x++)
		    {
		      ctrl = *pctrl++ ;
		      if (ctrl == CONTROL_MARKED)  // marked point - don't change it
			{
			  ptmp++ ;
			  continue ;
			}
		      /* now set this voxel to the average of
			 the marked neighbors */
		      mean = 0.0;
		      for (zk = -1 ; zk <= 1 ; zk++)
			{
			  zi = pzi[z+zk] ;
			  for (yk = -1 ; yk <= 1 ; yk++)
			    {
			      yi = pyi[y+yk] ;
			      for (xk = -1 ; xk <= 1 ; xk++)
				{
				  xi = pxi[x+xk] ;
				  mean += MRIFseq_vox(mri_dst, xi, yi, zi,f) ;
				}
			    }
			}
		      *ptmp++ = (float)mean / (3.0f*3.0f*3.0f);
		      // *ptmp++ = (float)nint((float)mean / (3.0f*3.0f*3.0f));
		    }
		}
	    }
	  MRIcopy(mri_tmp, mri_dst) ;
	  x1 = MAX(x1-1, 0) ; y1 = MAX(y1-1, 0) ; z1 = MAX(z1-1, 0) ;
	  x2 = MIN(x2+1,width-1); y2 = MIN(y2+1,height-1); z2 = MIN(z2+1, depth-1);
	} // iter
    } //frames
  
  MRIfree(&mri_tmp) ;

  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    MRIwrite(mri_dst, "soap.mgh") ;
  return(mri_dst) ;
}
#endif


/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
static MRI *
mriSoapBubbleShort(MRI *mri_src, MRI *mri_ctrl, MRI *mri_dst,int niter)
{
  int     width, height, depth, frames, x, y, z, f, xk, yk, zk, xi, yi, zi, i,
    *pxi, *pyi, *pzi, num ;
  BUFTYPE *pctrl, ctrl ;
  short   *ptmp ;
  int     mean ;
  MRI     *mri_tmp ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  frames = mri_src->nframes ;
  
  if (!mri_dst)
    mri_dst = MRIcopy(mri_src, NULL) ;

  pxi = mri_src->xi ;
  pyi = mri_src->yi ;
  pzi = mri_src->zi ;

  mri_tmp = MRIcopy(mri_dst, NULL) ;

  for ( f = 0 ; f < frames ; f++)
    {
      for ( z = 0 ; z < depth ; z++)
	{
	  for (y = 0 ; y < height ; y++)
	    {
	      pctrl = &MRIvox(mri_ctrl, 0, y, z) ;
	      ptmp = &MRISseq_vox(mri_tmp, 0, y, z, f) ;
	      for (x = 0 ; x < width ; x++)
		{
		  ctrl = *pctrl++ ;
		  if (ctrl == CONTROL_MARKED)
		    continue ;
		  num = mean = 0 ;
		  for (zk = -1 ; zk <= 1 ; zk++)
		    {
		      zi = pzi[z+zk] ;
		      for (yk = -1 ; yk <= 1 ; yk++)
			{
			  yi = pyi[y+yk] ;
			  for (xk = -1 ; xk <= 1 ; xk++)
			    {
			      xi = pxi[x+xk] ;
			      if (MRIvox(mri_ctrl, xi, yi, zi) != CONTROL_MARKED)
				continue ;
			      mean += (int)MRISseq_vox(mri_dst, xi, yi, zi, f) ;
			      num++ ;
			    }
			}
		    }
		  if (num > 0)
		    MRISseq_vox(mri_dst, x, y, z, f) =
		      (short)nint((float)mean / (float)num) ;
		}
	    }
	}
      
      /* now propagate values outwards */
      for (i = 0 ; i < niter ; i++)
	{
	  if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
	    fprintf(stderr, "soap bubble iteration %d of %d\n", i+1, niter) ;
	  for ( z = 0 ; z < depth ; z++)
	    {
	      for (y = 0 ; y < height ; y++)
		{
		  pctrl = &MRIvox(mri_ctrl, 0, y, z) ;
		  ptmp = &MRISseq_vox(mri_tmp, 0, y, z, f) ;
		  for (x = 0 ; x < width ; x++)
		    {
		      ctrl = *pctrl++ ;
		      if (ctrl == CONTROL_MARKED)   /* marked point - don't change it */
			{
			  ptmp++ ;
			  continue ;
			}
		      /* now set this voxel to the average of
			 the marked neighbors */
		      mean = 0 ;
		      for (zk = -1 ; zk <= 1 ; zk++)
			{
			  zi = pzi[z+zk] ;
			  for (yk = -1 ; yk <= 1 ; yk++)
			    {
			      yi = pyi[y+yk] ;
			      for (xk = -1 ; xk <= 1 ; xk++)
				{
				  xi = pxi[x+xk] ;
				  mean += (int)MRISseq_vox(mri_dst, xi, yi, zi, f) ;
				}
			    }
			}
		      *ptmp++ = (short)nint((float)mean / (3.0f*3.0f*3.0f)) ;
		    }
		}
	    } //z
	  MRIcopy(mri_tmp, mri_dst) ;
	} //iter
    } //frames

  MRIfree(&mri_tmp) ;

  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    MRIwrite(mri_dst, "soap.mgh") ;
  return(mri_dst) ;
}


/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
static MRI *
mriSoapBubbleExpandFloat(MRI *mri_src, MRI *mri_ctrl, MRI *mri_dst,int niter)
{
  int     width, height, depth, x, y, z, xk, yk, zk, xi, yi, zi, i,
  *pxi, *pyi, *pzi ;
  BUFTYPE *pctrl, ctrl ;
  float   *ptmp, nvox, mean ;
  MRI     *mri_tmp ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  if (!mri_dst)
    mri_dst = MRIcopy(mri_src, NULL) ;

  pxi = mri_src->xi ;
  pyi = mri_src->yi ;
  pzi = mri_src->zi ;

  mri_tmp = MRIcopy(mri_dst, NULL) ;

  /* now propagate values outwards */
  for (i = 0 ; i < niter ; i++)
  {
    if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
      fprintf(stderr,
              "soap bubble expansion iteration %d of %d\n", i+1, niter) ;
    for ( z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        pctrl = &MRIvox(mri_ctrl, 0, y, z) ;
        ptmp = &MRIFvox(mri_tmp, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          ctrl = *pctrl++ ;
          if (ctrl == CONTROL_MARKED)   /* marked point - don't change it */
          {
            ptmp++ ;
            continue ;
          }
          /* now set this voxel to the average of
             the marked neighbors */
          nvox = mean = 0 ;
          for (zk = -1 ; zk <= 1 ; zk++)
          {
            zi = pzi[z+zk] ;
            for (yk = -1 ; yk <= 1 ; yk++)
            {
              yi = pyi[y+yk] ;
              for (xk = -1 ; xk <= 1 ; xk++)
              {
                xi = pxi[x+xk] ;
                if (MRIvox(mri_ctrl, xi, yi, zi) ==
                    CONTROL_MARKED)
                {
                  mean += MRIFvox(mri_dst, xi, yi, zi) ;
                  nvox++ ;
                }
              }
            }
          }
          if (nvox)
          {
            *ptmp++ = (float)mean / nvox ;
            MRIvox(mri_ctrl, x, y, z) = CONTROL_TMP ;
          }
          else
            ptmp++ ;
        }
      }
    }
    MRIcopy(mri_tmp, mri_dst) ;
    for ( z = 0 ; z < depth ; z++)
    {
      for (y = 0 ; y < height ; y++)
      {
        pctrl = &MRIvox(mri_ctrl, 0, y, z) ;
        for (x = 0 ; x < width ; x++)
        {
          ctrl = *pctrl ;
          if (ctrl == CONTROL_TMP)
            ctrl = CONTROL_MARKED ;
          *pctrl++ = ctrl ;
        }
      }
    }
  }

  MRIfree(&mri_tmp) ;

  /*MRIwrite(mri_dst, "soap.mnc") ;*/
  return(mri_dst) ;
}


static MRI *
mriMarkUnmarkedNeighbors(MRI *mri_src, MRI *mri_marked,
                         MRI *mri_dst, int mark, int nbr_mark)
{
  int width, height, depth, 
    x, y, z, 
    xk, yk, zk, 
    xi, yi, zi, 
    *pxi, *pyi, *pzi,
    *psrc_int=NULL, val=0 ;
  unsigned char *psrc_uchar=NULL ;

  if ((mri_src->type != MRI_UCHAR || 
       mri_marked->type != MRI_UCHAR || 
       mri_dst->type != MRI_UCHAR) &&
      (mri_src->type != MRI_INT || 
       mri_marked->type != MRI_INT || 
       mri_dst->type != MRI_INT))
    ErrorExit(
      ERROR_UNSUPPORTED,
      "mriMarkUnmarkedNeighbors: all inputs must be MRI_UCHAR or MRI_INT") ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  pxi = mri_src->xi ;
  pyi = mri_src->yi ;
  pzi = mri_src->zi ;

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      if (mri_src->type == MRI_UCHAR)
      {
        psrc_uchar = &MRIvox(mri_src, 0, y, z) ;
      }
      else if (mri_src->type == MRI_INT)
      {
        psrc_int = &MRIIvox(mri_src, 0, y, z) ;
      }
      else
      {
        ErrorExit(
          ERROR_UNSUPPORTED,
          "mriMarkUnmarkedNeighbors: all inputs must be "
          "MRI_UCHAR or MRI_INT") ;
      }
      for (x = 0 ; x < width ; x++)
      {
        if (mri_src->type == MRI_UCHAR)
        {
          val = *psrc_uchar++ ;
        }
        else if (mri_src->type == MRI_INT)
        {
          val = *psrc_int++;
        }
        if (val == mark)   /* mark all neighbors */
        {
          for (zk = -1 ; zk <= 1 ; zk++)
          {
            zi = pzi[z+zk] ;
            for (yk = -1 ; yk <= 1 ; yk++)
            {
              yi = pyi[y+yk] ;
              for (xk = -1 ; xk <= 1 ; xk++)
              {
                xi = pxi[x+xk] ;
                if (mri_src->type == MRI_UCHAR)
                {
                  if (MRIvox(mri_marked, xi, yi, zi) ==
                      CONTROL_NONE)
                    MRIvox(mri_dst, xi, yi, zi) = nbr_mark ;
                }
                else if (mri_src->type == MRI_INT)
                {
                  if (MRIIvox(mri_marked, xi, yi, zi) ==
                      CONTROL_NONE)
                    MRIIvox(mri_dst, xi, yi, zi) = nbr_mark ;
                }
                else
                {
                  ErrorExit(
                    ERROR_UNSUPPORTED,
                    "mriMarkUnmarkedNeighbors: all inputs must be "
                    "MRI_UCHAR or MRI_INT") ;
                }
              }
            }
          }
        }
      }
    }
  }
  return(mri_dst) ;
}


static int
mriRemoveOutliers(MRI *mri, int min_nbrs)
{
  int     width, height, depth, x, y, z, xk, yk, zk, xi, yi, zi;
  int     *pxi, *pyi, *pzi, marked, nbrs, deleted ;
  BUFTYPE *pmarked ;

  width = mri->width ;
  height = mri->height ;
  depth = mri->depth ;

  pxi = mri->xi ;
  pyi = mri->yi ;
  pzi = mri->zi ;

  for (deleted = z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      pmarked = &MRIvox(mri, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        marked = *pmarked ;
        if (!marked)
        {
          pmarked++ ;
          continue ;
        }

        /* check to see that at least min_nbrs nbrs are on */
        nbrs = -1 ;  /* so it doesn't count the central voxel */
        for (zk = -1 ; marked && zk <= 1 ; zk++)
        {
          zi = pzi[z+zk] ;
          for (yk = -1 ; marked && yk <= 1 ; yk++)
          {
            yi = pyi[y+yk] ;
            for (xk = -1 ; marked && xk <= 1 ; xk++)
            {
              xi = pxi[x+xk] ;
              if (MRIvox(mri, xi, yi, zi))
                nbrs++ ;
            }
          }
        }

        if (nbrs < min_nbrs)
        {
          deleted++ ;
          *pmarked++ = 0 ;
        }
        else
          pmarked++ ;
      }
    }
  }

  if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
    fprintf(stderr, "%d control points deleted.\n", deleted) ;
  return(NO_ERROR) ;
}


#if 0
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  If any voxels are on in a 2x2x2 neighborhood, set the
  output value to it.
  ------------------------------------------------------*/
static MRI *
mriDownsampleCtrl2(MRI *mri_src, MRI *mri_dst)
{
  int     width, depth, height, x, y, z, x1, y1, z1 ;
  BUFTYPE *psrc, val ;

  if (mri_src->type != MRI_UCHAR)
    ErrorReturn(NULL,
                (ERROR_UNSUPPORTED, "MRIdownsample2: source must be UCHAR"));

  width = mri_src->width/2 ;
  height = mri_src->height/2 ;
  depth = mri_src->depth/2 ;

  if (!mri_dst)
  {
    mri_dst = MRIalloc(width, height, depth, mri_src->type) ;
    MRIcopyHeader(mri_src, mri_dst) ;
  }

  MRIclear(mri_dst) ;
  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        for (val = 0, z1 = 2*z ; !val && z1 <= 2*z+1 ; z1++)
        {
          for (y1 = 2*y ; !val && y1 <= 2*y+1 ; y1++)
          {
            psrc = &MRIvox(mri_src, 2*x, y1, z1) ;

            for (x1 = 2*x ; !val && x1 <= 2*x+1 ; x1++)
              val = *psrc++ ;
          }
        }
        MRIvox(mri_dst, x, y, z) = val ;
      }
    }
  }

  mri_dst->imnr0 = mri_src->imnr0 ;
  mri_dst->imnr1 = mri_src->imnr0 + mri_dst->depth - 1 ;
  mri_dst->xsize = mri_src->xsize*2 ;
  mri_dst->ysize = mri_src->ysize*2 ;
  mri_dst->zsize = mri_src->zsize*2 ;
  return(mri_dst) ;
}
#endif


int
MRI3dUseFileControlPoints(MRI *mri,const char *fname)
{
  int  i = 0 ;
  double xr, yr, zr;
  MPoint *pArray = 0;
  int count = 0;
  int useRealRAS = 0;

  pArray = MRIreadControlPoints(fname, &count, &useRealRAS);
  num_control_points = count;

  // initialize xctrl, yctrl, zctrl
  if (xctrl)
  {
    free(xctrl);
    xctrl = 0;
  }
  if (yctrl)
  {
    free(yctrl);
    yctrl = 0;
  }
  if (zctrl)
  {
    free(zctrl);
    zctrl = 0;
  }
  xctrl = (int *)calloc(count, sizeof(int)) ;
  yctrl = (int *)calloc(count, sizeof(int)) ;
  zctrl = (int *)calloc(count, sizeof(int)) ;
  if (!xctrl || !yctrl || !zctrl)
    ErrorExit(ERROR_NOMEMORY,
              "MRI3dUseFileControlPoints: could not allocate %d-sized table",
              num_control_points) ;
  for (i = 0 ; i < count ; i++)
  {
    switch (useRealRAS)
    {
    case 0:
      MRIsurfaceRASToVoxel(mri,
                           pArray[i].x, pArray[i].y, pArray[i].z,
                           &xr, &yr, &zr);
      break;
    case 1:
      MRIworldToVoxel(mri,
                      pArray[i].x, pArray[i].y, pArray[i].z,
                      &xr, &yr, &zr) ;
      break;
    default:
      ErrorExit(ERROR_BADPARM,
                "MRI3dUseFileControlPoints has bad useRealRAS flag %d\n",
                useRealRAS) ;

    }
    xctrl[i] = (int) nint(xr) ;
    yctrl[i] = (int) nint(yr) ;
    zctrl[i] = (int) nint(zr) ;
    if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
      fprintf(stderr, "( %5d, %5d, %5d )\n", xctrl[i], yctrl[i], zctrl[i]);
  }
  free(pArray);
  return(NO_ERROR) ;
}


int
MRI3dWriteControlPoints(char *t_control_volume_fname)
{
  control_volume_fname = t_control_volume_fname ;
  return(NO_ERROR) ;
}


int
MRI3dWriteBias(char *t_bias_volume_fname)
{
  bias_volume_fname = t_bias_volume_fname ;
  return(NO_ERROR) ;
}


int
MRInormAddFileControlPoints(MRI *mri_ctrl, int value)
{
  int  i, nctrl, x, y, z, bad = 0 ;

  /* read in control points from a file (if specified) */
  for (nctrl = i = 0 ; i < num_control_points ; i++)
  {
    x = xctrl[i] ; y = yctrl[i] ; z = zctrl[i] ;
    if (MRIindexNotInVolume(mri_ctrl, x, y, z) == 0)
    {
      if (MRIvox(mri_ctrl, x, y, z) == 0)
        nctrl++ ;
      MRIvox(mri_ctrl, x, y, z) = value ;
    }
    else
      bad++ ;
  }
  if (bad > 0)
    ErrorPrintf(ERROR_BADFILE, "!!!!! %d control points rejected for being out of bounds !!!!!!\n") ;
  return(nctrl) ;
}


#if 0
static int
remove_extreme_control_points(MRI *mri_orig,
                              MRI *mri_ctrl,
                              float low_thresh,
                              float hi_thresh,
                              float target_val,
                              double wm_std)
{
  int       x, y, z, peak, nremoved, bin, xi, yi, zi, xk, yk, zk;
  int       remove, ncontrol, whalf, wsize ;
  float     val, min_val, max_val, dist, max_nbr_val, intensity_below;
  float     val0, intensity_above, max_delta_above, max_delta_below;
  float     val_above, val_below, max_ctrl_val, min_ctrl_val ;
  HISTOGRAM *h, *hsmooth ;
  MRI_REGION reg ;

#define WSIZE 7
#define WHALF ((WSIZE-1)/2)
  wsize = ceil(WSIZE / mri_orig->xsize) ;
  whalf = (wsize-1)/2 ;

  intensity_below = target_val - low_thresh ;
  intensity_above = hi_thresh - target_val ;
  low_thresh = MIN(low_thresh, target_val - 4*wm_std) ;
  printf("checking for control points more than %2.1f below nbrs, low=%2.1f\n",
         MAX(2*wm_std, intensity_below/2), low_thresh) ;
  reg.dx = reg.dy = reg.dz = wsize ;
  nremoved = 0 ;
  max_delta_above = MAX(2*wm_std, intensity_above/2) ;
  max_delta_below = MAX(2*wm_std, intensity_below/2) ;
  for (x = 0 ; x < mri_orig->width ; x++)
  {
    for (y = 0 ; y < mri_orig->height ; y++)
    {
      for (z = 0 ; z < mri_orig->depth ; z++)
      {
        if (MRIvox(mri_ctrl, x, y, z) > 0)
        {
          if (x == Gx && y == Gy && z == Gz)
          {
            MRIwrite(mri_orig, "on.mgz") ;
            DiagBreak() ;
          }
          val = MRIgetVoxVal(mri_orig, x, y, z, 0) ;

          reg.x = x-whalf ;
          reg.y = y-whalf ;
          reg.z = z-whalf ;
          h = MRIhistogramLabelRegion(mri_orig, mri_ctrl,
                                      &reg, CONTROL_MARKED, 0) ;
          hsmooth = HISTOsmooth(h, NULL, 2) ;
          peak = HISTOfindHighestPeakInRegion
                 (hsmooth, 0, hsmooth->nbins) ;
          bin = HISTOfindBin(hsmooth, val) ;

          remove = 0 ;
          if ((hsmooth->counts[peak] > 3*hsmooth->counts[bin]) && 0)
          {
            nremoved++ ;
            MRIvox(mri_ctrl, x, y, z) = CONTROL_NONE ;
          }
          else   /* check 5x5x5 */
          {
            val0 = max_ctrl_val =
                     min_ctrl_val =
                       min_val = max_val =
                                   max_nbr_val = val ;  /* value at center */
            val_above = val0+max_delta_above ;/* assume asymmetric distbtion */
            val_below = val0-max_delta_below ;
            ncontrol = 0 ; /* # of control points in nbhd */
            for (xk = -2 ; xk <= 2 ; xk++)
            {
              xi = mri_ctrl->xi[x+xk] ;
              for (yk = -2 ; yk <= 2 ; yk++)
              {
                yi = mri_ctrl->yi[y+yk] ;
                for (zk = -2 ; zk <= 2 ; zk++)
                {
                  zi = mri_ctrl->zi[z+zk] ;
                  if (xi == Gvx && yi == Gvy && zi == Gvz)
                    DiagBreak() ;
                  val = MRIgetVoxVal(mri_orig, xi, yi, zi, 0) ;
                  dist = sqrt(SQR(x-xi)+SQR(y-yi)+SQR(z-zi));
                  if (MRIvox(mri_ctrl, xi, yi, zi))
                  {
                    if (val > max_ctrl_val)
                      max_ctrl_val = val ;
                    if (val < min_ctrl_val)
                      min_ctrl_val = val ;
                    ncontrol++ ;
                  }
                  if (dist < 2)   /* 26-connected neighbor */
                  {
#if 1
                    if (val < min_val)
                      min_val = val ;
#endif
                    if (val > max_nbr_val)
                      max_nbr_val = val ;
                    if (val < val_below || val > val_above)
                    {
                      remove = 1 ;
                      break ;
                    }
                  }
                  if (val > max_val && val < hi_thresh)
                    max_val = val ;
                  if (remove)
                    break ;
                }
                if (remove)
                  break ;
              }
              if (remove)
                break ;
            }

            /*
              check to make sure that the bias
              field at this control point isn't too different
              from ones around it */

#define WSIZE 7
#define WHALF ((WSIZE-1)/2)
            whalf = ceil(WHALF / mri_orig->xsize) ;
            if (remove == 0)
            {
              for (xk = -whalf ; xk <= whalf ; xk++)
              {
                xi = mri_ctrl->xi[x+xk] ;
                for (yk = -whalf ; yk <= whalf ; yk++)
                {
                  yi = mri_ctrl->yi[y+yk] ;
                  for (zk = -whalf ; zk <= whalf ; zk++)
                  {
                    zi = mri_ctrl->zi[z+zk] ;
                    if (xi == Gx && yi == Gy && zi == Gz)
                      DiagBreak() ;
                    if (MRIvox(mri_ctrl, xi, yi, zi) > 0)
                      /* nbr is a control point */
                    {
                      val =
                        MRIgetVoxVal(mri_orig,
                                     xi, yi, zi, 0) ;
                      if (val > max_ctrl_val)
                        max_ctrl_val = val ;
                      if (val < min_ctrl_val)
                        min_ctrl_val = val ;
                      if (val < val_below
                          || val > val_above)
                      {
                        if (xi == Gx
                            && yi == Gy
                            && zi == Gz)
                          DiagBreak() ;
                        if (x == Gx
                            && y == Gy
                            && z == Gz)
                          DiagBreak() ;
                        remove = 1 ;
                        break ;
                      }
                    }
                  }
                  if (remove)
                    break ;
                }
                if (remove)
                  break ;
              }
            }
            /*
              if image gradient in 3x3x3 nbhd in orig
              volume is too big (bigger than max - min)
              then don't let it be a control point. Or, if an
              immediate nbr is below low_thresh
              in the orig volume and their is a control point
              in a 5x5x5 window then don't let it
              be a control point (otherwise it can creep
              outwards along gently decreasing gray matter
              intensities.
              or, remove it if the intensity derivative is too big
            */
            if ((max_val < hi_thresh && max_val >= target_val &&
                 min_val < low_thresh) ||
                (fabs(max_nbr_val - min_val) >=
                 (target_val-low_thresh)) ||
                (((val0-min_ctrl_val) >= max_delta_above) ||
                 (((max_ctrl_val-val0) >= max_delta_below))) ||
                remove)
              {
                if (x == Gx && y == Gy && z == Gz)
                  DiagBreak() ;
                nremoved++ ;
                MRIvox(mri_ctrl, x, y, z) = CONTROL_NONE ;
              }
            else if (x == Gx && y == Gy && z == Gz)
            {
              DiagBreak() ;
            }
          }

          HISTOfree(&h) ;
          HISTOfree(&hsmooth) ;
          if (x == Gx && y == Gy && z == Gz)
            printf("(%d, %d, %d) initially a control point, %s\n",
                   Gx, Gy, Gz, MRIvox(mri_ctrl, x, y, z) ? "retained" :
                   "removed");
        }
        else if (x == Gx && y == Gy && z == Gz)
          printf("(%d, %d, %d) not a control point\n", Gx, Gy, Gz);


      }
    }
  }

  printf("%d outlying control points removed due to "
         "unlikely bias estimates...\n", nremoved) ;
  return(NO_ERROR) ;
}


static double
mriNormComputeWMStandardDeviation(MRI *mri_orig, MRI *mri_ctrl)
{
  int       x, y, z, xi, yi, zi, xk, yk, zk, num, whalf ;
  double    wm_std, val0, val ;

#define WSIZE 7
#define WHALF ((WSIZE-1)/2)
  whalf = ceil(WHALF / mri_orig->xsize) ;
  for (num = x = 0, wm_std = 0.0 ; x < mri_orig->width ; x++)
  {
    for (y = 0 ; y < mri_orig->height ; y++)
    {
      for (z = 0 ; z < mri_orig->depth ; z++)
      {
        if (MRIvox(mri_ctrl, x, y, z) > 0)
        {
          if (x == Gx && y == Gy && z == Gz)
          {
            MRIwrite(mri_orig, "on.mgz") ;
            DiagBreak() ;
          }
          val0 = MRIgetVoxVal(mri_orig, x, y, z, 0) ;
          for (xk = -whalf ; xk <= whalf ; xk++)
          {
            xi = mri_ctrl->xi[x+xk] ;
            for (yk = -whalf ; yk <= whalf ; yk++)
            {
              yi = mri_ctrl->yi[y+yk] ;
              for (zk = -whalf ; zk <= whalf ; zk++)
              {
                zi = mri_ctrl->zi[z+zk] ;
                if (xi == Gx && yi == Gy && zi == Gz)
                  DiagBreak() ;
                if (MRIvox(mri_ctrl, xi, yi, zi) > 0)
                {
                  val = MRIgetVoxVal(mri_orig, xi, yi, zi, 0) ;
                  wm_std += SQR(val-val0);
                  num++ ;
                }
              }
            }
          }
        }
      }
    }
  }
  if (num > 0)
    wm_std = sqrt(wm_std / num) ;

  printf("wm standard deviation = %2.2f with %d DOFs\n", wm_std, num) ;
  return(wm_std) ;
}
#endif

#define MAX_DISTANCE_TO_CSF  7
#define MIN_DISTANCE_TO_CSF  4.5


static int
remove_gray_matter_control_points(MRI *mri_ctrl,
                                  MRI *mri_src,
                                  float wm_target,
                                  float intensity_above,
                                  float intensity_below,
                                  int   scan_type)
{
  MRI    *mri_ctrl_outside, *mri_ctrl_inside, *mri_tmp = NULL ;
  int    n, x, y, z, xi, yi, zi, xk, yk, zk;
  int    nremoved, removed, num, nretained, nclose_to_csf = 0;
  int    whalf ;
  double delta_mean, delta_std ;
  float  val0, val, hi_thresh, csf_val, wm_val, gm_val, low_thresh ;
  static int ncalls = 0 ;
  char fname[STRLEN] ;

  ncalls++ ;

  csf_val =
    find_tissue_intensities(mri_src, mri_ctrl, &wm_val, &gm_val, &csf_val) ;
  printf("using csf threshold %2.0f for control point removal\n",csf_val) ;
  mri_ctrl_inside = MRIcopy(mri_ctrl, NULL) ;
  for (n = 0 ; n < 2 ; n++)
  {
    num = MRIvoxelsInLabel(mri_ctrl_inside, 1) ;
    printf("after %d erosions, %d voxels remaining\n", n, num) ;
    mri_tmp = MRIerode(mri_ctrl_inside, mri_tmp) ;
    if (MRIvoxelsInLabel(mri_tmp,1) <= 100) /* don't copy empty (or
                                               nearly empty) image */
      break ;
    MRIcopy(mri_tmp, mri_ctrl_inside) ;
  }
  mri_ctrl_outside = MRIsubtract(mri_ctrl, mri_ctrl_inside, NULL) ;
  MRIfree(&mri_ctrl_inside) ;

  /* now find a set of control points that are
     definitely in the wm (not thalamus etc). There
     don't have to be very many of them, just enough
     to get a stable estimate of the wm std
  */
  mri_ctrl_inside = MRInormFindControlPointsInWindow(mri_src,
                    wm_target,
                    intensity_above,
                    intensity_below,
                    NULL,3, NULL, NULL,
                    scan_type) ;
  MRIbinarize(mri_ctrl_inside, mri_ctrl_inside, 1,
              CONTROL_NONE, CONTROL_MARKED) ;

  /* erode to remove any potential non-wm control points */
  MRIerode(mri_ctrl_inside, mri_ctrl_inside) ;
  /* if there aren't enough voxels left to get a stable estimate, undo
     the erosion */
  if (MRIvoxelsInLabel(mri_ctrl_inside, CONTROL_MARKED) < 100)
    MRInormGentlyFindControlPoints(mri_src,
                                   wm_target,
                                   intensity_above,
                                   intensity_below,
                                   mri_ctrl_inside) ;

#ifdef WSIZE
#undef WSIZE
#endif
#ifdef WHALF
#undef WHALF
#endif
#define WSIZE 9
#define WHALF ((WSIZE-1)/2)
  whalf = ceil(WHALF / mri_src->xsize) ;

  if ((Gdiag & DIAG_WRITE) && DIAG_VERBOSE_ON)
  {
    printf("writing control point and src files for call %d...\n", ncalls) ;
    sprintf(fname, "co%d.mgz", ncalls) ;
    MRIwrite(mri_ctrl_outside, fname) ;
    sprintf(fname, "ci%d.mgz", ncalls) ;
    MRIwrite(mri_ctrl_inside, fname) ;
    sprintf(fname, "cb%d.mgz", ncalls) ;
    MRIwrite(mri_ctrl, fname) ;
    sprintf(fname, "orig%d.mgz", ncalls) ;
    MRIwrite(mri_src, fname) ;
  }

  if (Gx >= 0)
  {
    if (MRIvox(mri_ctrl, Gx, Gy, Gz) == 0)
      printf("(%d, %d, %d) is %sa control point\n",
             Gx, Gy, Gz,
             MRIvox(mri_ctrl,Gx,Gy,Gz)         ? "":"NOT ");
    else
      printf("(%d, %d, %d) is %sa control point, is "
             "%sinside, is %soutside\n",
             Gx, Gy, Gz,
             MRIvox(mri_ctrl,Gx,Gy,Gz)         ? "":"NOT ",
             MRIvox(mri_ctrl_inside,Gx,Gy,Gz)  ? "":"NOT ",
             MRIvox(mri_ctrl_outside,Gx,Gy,Gz) ? "":"NOT ") ;
  }

  for (delta_std = delta_mean = 0.0, x = n = 0 ; x < mri_src->width ; x++)
  {
    for (y = 0 ; y < mri_src->height ; y++)
    {
      for (z = 0 ; z < mri_src->depth ; z++)
      {
        if (MRIvox(mri_ctrl_inside,x,y,z)) /* check to make sure not in gm */
        {
          val0 = MRIgetVoxVal(mri_src, x, y, z, 0) ;
          for (xk = -whalf ; xk <= whalf ; xk++)
          {
            xi = mri_src->xi[x+xk] ;
            for (yk = -whalf ; yk <= whalf ; yk++)
            {
              yi = mri_src->yi[y+yk] ;
              for (zk = -whalf ; zk <= whalf ; zk++)
              {
                zi = mri_src->zi[z+zk] ;
                if (MRIvox(mri_ctrl_inside, xi, yi, zi))
                {
                  val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
                  if (val < wm_target && val > val0)
                  {
                    delta_std += (val-val0)*(val-val0) ;
                    n++ ;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  delta_std = sqrt(delta_std/n) ;
  delta_std = (wm_val - gm_val)/10 ;
  low_thresh = (wm_val+gm_val)/2 + 10 ;
  printf("wm standard deviation = %2.1f (%d DOFs), low wm thresh=%2.3f\n",
         delta_std, n, low_thresh) ;
  for (nretained = nremoved = x = 0 ; x < mri_src->width ; x++)
  {
    for (y = 0 ; y < mri_src->height ; y++)
    {
      for (z = 0 ; z < mri_src->depth ; z++)
      {
        if (MRIvox(mri_ctrl_outside,x,y,z)) /* check to make sure not in gm */
        {
          val0 = MRIgetVoxVal(mri_src, x, y, z, 0) ;
          hi_thresh = val0 + 2*delta_std ;
          if (x == Gx && y == Gy && z == Gz)
            printf("(%d, %d, %d)=%2.0f, is "
                   "ctrl - checking for nbhd vals > %2.0f\n",
                   Gx, Gy, Gz, val0, hi_thresh) ;
          if (csf_in_window(mri_src, x, y, z,
                            MIN_DISTANCE_TO_CSF, csf_val))
          {
            removed = 1 ;
            if (x == Gx && y == Gy && z == Gz)
              printf("removing voxel (%d, %d, %d) "
                     "due to csf proximity (< %2.0f mm)\n",
                     Gx, Gy, Gz, (float)MIN_DISTANCE_TO_CSF) ;
            MRIvox(mri_ctrl, x, y, z) = CONTROL_NONE ;
            nclose_to_csf++ ;
          }
          else
            n = removed = 0 ;
          if (val0 > low_thresh)  /* pretty certain it's wm regardless */
            continue ;
          if (!removed) for (xk = -whalf ; xk <= whalf ; xk++)
            {
              xi = mri_src->xi[x+xk] ;
              for (yk = -whalf ; yk <= whalf ; yk++)
              {
                yi = mri_src->yi[y+yk] ;
                for (zk = -whalf ; zk <= whalf ; zk++)
                {
                  zi = mri_src->zi[z+zk] ;
                  if (MRIvox(mri_ctrl, xi, yi, zi))
                  {
                    n++ ;
                    val = MRIgetVoxVal(mri_src, xi, yi, zi, 0) ;
                    if (xi == Gvx && yi == Gvy && Gz == Gvz)
                      DiagBreak() ;
                    if (val > hi_thresh)/* too big a change to be valid bias*/
                    {
                      /* mark it as remove even though
                         it might not be - no
                         point in continuing if no
                         csf in window, won't change */
                      removed=1 ;
                      if (csf_in_window(mri_src, x, y, z,
                                        MAX_DISTANCE_TO_CSF,
                                        csf_val))
                      {
                        nremoved++ ;
                        MRIvox(mri_ctrl, x, y, z) =
                          CONTROL_NONE ;
                        if (x == Gx && y == Gy && z == Gz)
                          printf("found nbhd val %2.0f "
                                 "at (%d, %d, %d) -> "
                                 "removing\n",
                                 val, xi, yi, zi) ;
                      }
                      else
                      {
                        if (x == Gx && y == Gy && z == Gz)
                          printf("found nbhd val %2.0f "
                                 "at (%d, %d, %d) -> "
                                 "retaining due to lack "
                                 "of CSF proximity\n",
                                 val, xi, yi, zi) ;
                        nretained++ ;  /* in body of white matter, keep it */
                      }
                      break ;
                    }
                  }
                  if (removed)
                    break ;
                }
                if (removed)
                  break ;
              }
              if (removed)
                break ;
            }
          if (x == Gx && y == Gy && z == Gz)
            printf("%d voxels examined, (%d, %d, %d) %sremoved\n",
                   n, Gx, Gy, Gz, removed ? "": "NOT ") ;
        }
      }
    }
  }

  for (n = 0 ; n < num_control_points ; n++)
  {
    if (!MRIvox(mri_ctrl, xctrl[n], yctrl[n], zctrl[n]))
    {
      nremoved-- ;
      MRIvox(mri_ctrl, xctrl[n], yctrl[n], zctrl[n]) = CONTROL_MARKED ;
    }
  }

  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
  {
    sprintf(fname, "ca%d.mgz", ncalls) ;
    MRIwrite(mri_ctrl, fname) ;
  }
  printf("%d potential gm control points removed (%d >%d mm from "
         "csf retained, %d < %2.1f mm removed)\n",
         nclose_to_csf+nremoved, nretained, MAX_DISTANCE_TO_CSF,
         nclose_to_csf, (float)MIN_DISTANCE_TO_CSF) ;
  MRIfree(&mri_tmp) ;
  MRIfree(&mri_ctrl_inside) ;
  MRIfree(&mri_ctrl_outside) ;
  return(NO_ERROR) ;
}


static float
csf_in_window(MRI *mri, int x0, int y0, int z0, float max_dist, float csf)
{
  int   xi, yi, zi, xk, yk, zk, whalf ;
  float val, dx, dy, dz, dist ;

  whalf = nint(max_dist/mri->xsize) ;
  if (x0 == Gx && y0 == Gy && z0 == Gz)
    DiagBreak() ;
  if (mri->type == MRI_UCHAR)
  {
    for (xk = -whalf ; xk <= whalf ; xk++)
    {
      xi = mri->xi[x0+xk] ;
      dx = SQR(xi-x0) ;
      for (yk = -whalf ; yk <= whalf ; yk++)
      {
        yi = mri->yi[y0+yk] ;
        dy = SQR(yi-y0) ;
        for (zk = -whalf ; zk <= whalf ; zk++)
        {
          zi = mri->zi[z0+zk] ;
          if (xi == Gx && yi == Gy && zi == Gz)
            DiagBreak() ;
          dz = SQR(zi-z0) ;
          dist = sqrt(dx+dy+dz);
          if (dist > max_dist)
            continue ;
          val = MRIvox(mri, xi, yi, zi);
          if (val <= csf)
            return(1) ;
        }
      }
    }
  }
  else
  {
    for (xk = -whalf ; xk <= whalf ; xk++)
    {
      xi = mri->xi[x0+xk] ;
      dx = SQR(xi-x0) ;
      for (yk = -whalf ; yk <= whalf ; yk++)
      {
        yi = mri->yi[y0+yk] ;
        dy = SQR(yi-y0) ;
        for (zk = -whalf ; zk <= whalf ; zk++)
        {
          zi = mri->zi[z0+zk] ;
          if (xi == Gx && yi == Gy && zi == Gz)
            DiagBreak() ;
          dz = SQR(zi-z0) ;
          dist = sqrt(dx+dy+dz);
          if (dist > max_dist)
            continue ;
          val = MRIgetVoxVal(mri, xi, yi, zi, 0) ;
          if (val <= csf)
            return(1) ;
        }
      }
    }
  }

  return(0) ;
}


static float
find_tissue_intensities(MRI *mri_src,
                        MRI *mri_ctrl,
                        float *pwm,
                        float *pgm,
                        float *pcsf)
{
  int        csf_peak, thresh_bin, bg_end, wm_peak, wm_valley;
  int        gm_peak, gm_valley ;
  HISTOGRAM *h, *hsmooth, *hwm ;
  float      csf_thresh, wm_val ;
#if 0
  MRI_REGION    bbox ;
#endif

  mri_ctrl = MRIbinarize(mri_ctrl, NULL, 1, CONTROL_NONE, CONTROL_MARKED) ;

  hwm = MRIhistogramLabel(mri_src, mri_ctrl, CONTROL_MARKED, 0) ;
  wm_peak = HISTOfindHighestPeakInRegion(hwm, 0, hwm->nbins) ;
  wm_val = hwm->bins[wm_peak] ;
  printf("white matter peak found at %2.0f\n", wm_val) ;
  HISTOfree(&hwm) ;

#if 0
  /* don't use outside of brain as it contains a bunch of dark stuff
     that isn't really csf and will mess up the histogram */
  MRIfindApproximateSkullBoundingBox(mri_src, wm_val*.5, &bbox) ;
  printf("bounding box (%d, %d, %d) --> (%d, %d, %d)\n",
         bbox.x, bbox.y, bbox.z,
         bbox.x+bbox.dx-1, bbox.y+bbox.dy-1, bbox.z+bbox.dz-1) ;
  bbox.x += bbox.dx/4 ;
  bbox.dx /= 2 ;
  bbox.y += bbox.dy/4 ;
  bbox.dy /= 2 ;
  bbox.z += bbox.dz/4 ;
  bbox.dz /= 2 ;

  h = MRIhistogramRegion(mri_src, 0, NULL, &bbox) ;
#else
  h = MRIhistogram(mri_src, 0) ;
#endif

  HISTOclearZeroBin(h) ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    HISTOplot(h, "h0.plt") ;
  if (mri_src->type == MRI_UCHAR)
  {
    HISTOclearBins(h, h, 0, 5) ;
    hsmooth = HISTOsmooth(h, NULL, 2); ;
  }
  else
  {
    hsmooth = HISTOsmooth(h, NULL, 2) ;
    HISTOclearBG(hsmooth, hsmooth, &bg_end) ;
    HISTOclearBins(h, h, 0, bg_end) ;
    HISTOsmooth(h, hsmooth, 2) ;
  }

  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
  {
    HISTOplot(h, "h.plt") ;
    HISTOplot(hsmooth, "hs.plt");
  }
  wm_peak = HISTOfindHighestPeakInRegion(hsmooth, wm_val-20, wm_val+20) ;
  wm_val = hsmooth->bins[wm_peak] ;
  printf("white matter peak found at %2.0f\n", wm_val) ;
#define MIN_PEAK_HALF_WIDTH  10
  wm_valley = HISTOfindPreviousValley
              (hsmooth, nint(wm_peak-5*hsmooth->bin_size)) ;
  gm_peak = HISTOfindPreviousPeak
            (hsmooth, wm_peak-10*hsmooth->bin_size, MIN_PEAK_HALF_WIDTH) ;
#define MIN_GM 40
  if (gm_peak < 0)
    gm_peak = HISTOfindBin(hsmooth, wm_val*.75) ;
  gm_valley = HISTOfindPreviousValley(hsmooth, gm_peak-10*hsmooth->bin_size) ;
  csf_peak = HISTOfindPreviousPeak
             (hsmooth, gm_valley-10*hsmooth->bin_size,  MIN_PEAK_HALF_WIDTH) ;
  if (csf_peak < 0)
    csf_peak = HISTOfindBin(hsmooth, gm_peak*.5) ;
  if (gm_peak > wm_peak*.8)
    gm_peak = wm_peak*.8 ;
  if (csf_peak > gm_peak*.75)
    csf_peak = gm_peak*.75 ;
  if (hsmooth->bins[gm_peak] < MIN_GM)
  {
    gm_peak = .7*wm_peak ;
  }
  if (csf_peak <= 0 || gm_valley <= 0)
  {
    csf_peak = gm_peak*.5 ;
  }

  printf("gm peak at %2.0f (%d), valley at %2.0f (%d)\n",
         hsmooth->bins[gm_peak], gm_peak,
         hsmooth->bins[gm_valley], gm_valley) ;

  // check pathological conditions
  thresh_bin = (2*gm_peak+csf_peak)/3 ;
  csf_thresh = (hsmooth->bins[thresh_bin]) ;
  printf("csf peak at %2.0f, setting threshold to %2.0f\n",
         hsmooth->bins[csf_peak], csf_thresh) ;

  *pwm = wm_val ;
  *pgm = hsmooth->bins[gm_peak] ;
  *pcsf = hsmooth->bins[csf_peak] ;

#if 0
  if (csf_thresh > 60)
    csf_thresh = 60 ;  /* HACK!!! Don't have time to fix it
                                  now for PD-suppressed synthetic images */
#endif
  HISTOfree(&hsmooth) ;
  HISTOfree(&h) ;
  MRIfree(&mri_ctrl) ;  /* copy of mri_ctrl, NOT the passed in one */
  return(csf_thresh) ;
}

#ifdef WSIZE
#undef WSIZE
#endif
#define WSIZE(mri)  ((((int)((7.0/mri->thick)/2))*2)+1)   /* make sure
it's odd */
#define MIN_WM_SNR   20


MRI *
MRInormFindHighSignalLowStdControlPoints(MRI *mri_src, MRI *mri_ctrl)
{
  MRI              *mri_ratio, *mri_mean, *mri_std/*, *mri_tmp = NULL */;
  MRI_SEGMENTATION *mriseg ;
  int              s1, s2, b ;
  HISTOGRAM        *h ;
  float            total, num ;
  
  if (mri_ctrl == NULL)
  {
    mri_ctrl = MRIalloc(mri_src->width,
                        mri_src->height,
                        mri_src->depth,
                        MRI_UCHAR) ;
    MRIcopyHeader(mri_src, mri_ctrl) ;
  }
  
  mri_mean = MRImean(mri_src, NULL, WSIZE(mri_src)) ;
  mri_std = MRIstd(mri_src, NULL, mri_mean, WSIZE(mri_src)) ;
  mri_ratio = MRIdivide(mri_mean, mri_std, NULL) ;
  MRImask(mri_ratio, mri_std, mri_ratio, 0, 0) ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
  {
    MRIwrite(mri_mean, "m.mgz") ;
    MRIwrite(mri_std, "s.mgz") ;
  }
  
  MRIfree(&mri_mean) ; MRIfree(&mri_std) ;
  
  h = MRIhistogram(mri_ratio, 0) ;
  
  /* don't count background voxels */
  for (total = 0.0f, b = 1 ; b < h->nbins ; b++)
    total += h->counts[b] ;
  for (num = 0.0f, b = h->nbins-1 ; b >= 1 ; b--)
  {
    num += h->counts[b] ;
    if (num > 20000)   /* use voxels that are in the top .5% of SNR */
      break ;
  }
  printf("using SNR threshold %2.3f at bin %d\n", h->bins[b], b) ;
  
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
  {
    HISTOplot(h, "h.plt") ;
    MRIwrite(mri_ratio, "rb.mgz") ;
  }
  mriseg = MRIsegment(mri_ratio, h->bins[b], 2*h->bins[h->nbins-1]);
  HISTOfree(&h) ;
  s1 = MRIsegmentMax(mriseg) ;
  MRIsegmentToImage(mri_ratio, mri_ctrl, mriseg, s1) ;
  num = mriseg->segments[s1].nvoxels ;
  mriseg->segments[s1].nvoxels = 0 ;
  s2 = MRIsegmentMax(mriseg) ;
  MRIsegmentToImage(mri_ratio, mri_ctrl, mriseg, s2) ;
  mriseg->segments[s1].nvoxels = num ;
  
  printf("using segments %d and %d with %d and %d voxels, "
         "centroids (%2.0f, %2.0f, %2.0f) and (%2.0f, %2.0f, %2.0f)\n",
         s1, s2, mriseg->segments[s1].nvoxels,
         mriseg->segments[s2].nvoxels,
         mriseg->segments[s1].cx,
         mriseg->segments[s1].cy,
         mriseg->segments[s1].cz,
         mriseg->segments[s2].cx,
         mriseg->segments[s2].cy,
         mriseg->segments[s2].cz) ;
  MRIbinarize(mri_ctrl, mri_ctrl, 1, CONTROL_NONE, CONTROL_MARKED) ;
#if 0
  while (MRIvoxelsInLabel(mri_ctrl, CONTROL_MARKED) > 10000)
  {
    mri_tmp = MRIerode(mri_ctrl, mri_tmp) ;
    if (MRIvoxelsInLabel(mri_tmp, CONTROL_MARKED) < 100)
      break ;
    MRIcopy(mri_tmp, mri_ctrl) ;
  }
#endif
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
  {
    MRIwrite(mri_ctrl, "csnr.mgz") ;
  }
  if (Gx >= 0)
    printf("(%d, %d, %d) is %san control point\n",
           Gx, Gy, Gz, MRIvox(mri_ctrl, Gx, Gy,Gz) ? "" : "NOT ") ;
  
  MRIsegmentFree(&mriseg) ;
  MRIfree(&mri_ratio) ; /*MRIfree(&mri_tmp) ;*/
  return(mri_ctrl) ;
}


MRI *
MRInormalizeHighSignalLowStd(MRI *mri_src,
                             MRI *mri_norm,
                             float bias_sigma,
                             float wm_target)
{
  int      nctrl, x, y, z, width, depth, height ;
  MRI      *mri_ctrl, *mri_bias ;
  float    norm, src, bias ;

  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;

  if (mri_norm == NULL)
  {
    mri_norm = MRIclone(mri_src, NULL) ;
  }

  mri_ctrl = MRInormFindHighSignalLowStdControlPoints(mri_src, NULL) ;
  nctrl = MRInormAddFileControlPoints(mri_ctrl, 255) ;
  MRIbinarize(mri_ctrl, mri_ctrl, 1, CONTROL_NONE, CONTROL_MARKED) ;
  mri_bias = MRIbuildBiasImage(mri_src, mri_ctrl, NULL, bias_sigma) ;
  MRIfree(&mri_ctrl) ;
  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (x = 0 ; x < width ; x++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        src = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        bias = MRIgetVoxVal(mri_bias, x, y, z, 0) ;
        if (!bias)   /* should never happen */
          norm = src ;
        else
          norm = src * wm_target / bias ;
        if (norm > 255.0f && mri_norm->type == MRI_UCHAR)
          norm = 255.0f ;
        MRIsetVoxVal(mri_norm, x, y, z, 0, norm) ;
      }
    }
  }
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE)
  {
    MRIwrite(mri_bias, "bsnr.mgz") ;
    MRIwrite(mri_norm, "nsnr.mgz") ;
  }

  MRIfree(&mri_bias) ;

  return(mri_norm) ;
}


MRI *
MRIapplyBiasCorrection(MRI *mri_in, MRI *mri_bias, MRI *mri_out)
{
  int    x, y, z ;
  double   bias, val, xd, yd, zd ;
  MATRIX *m_in_vox2ras, *m_bias_ras2vox, *m_vox2vox ;
  VECTOR *v1, *v2 ;

  v1 = VectorAlloc(4, MATRIX_REAL) ;
  v2 = VectorAlloc(4, MATRIX_REAL) ;
  *MATRIX_RELT(v1, 4,1) = 1 ;

  if (mri_out == NULL)
    mri_out = MRIclone(mri_in, NULL) ;

  m_in_vox2ras = MRIgetVoxelToRasXform(mri_in) ;
  m_bias_ras2vox = MRIgetRasToVoxelXform(mri_bias) ;
  m_vox2vox = MatrixMultiply(m_bias_ras2vox, m_in_vox2ras, NULL) ;
  MatrixFree(&m_in_vox2ras) ;
  MatrixFree(&m_bias_ras2vox) ;
  printf("using vox2vox xform:\n") ;
  MatrixPrint(stdout, m_vox2vox) ;
  for (x = 0 ; x < mri_in->width ; x++)
  {
    V3_X(v1) = x ;
    for (y = 0 ; y < mri_in->height ; y++)
    {
      V3_Y(v1) = y ;
      for (z = 0 ; z < mri_in->depth ; z++)
      {
        val = MRIgetVoxVal(mri_in, x, y, z, 0) ;
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        V3_Z(v1) = z ;
        MatrixMultiply(m_vox2vox, v1, v2) ;
        xd = V3_X(v2) ;
        yd = V3_Y(v2) ;
        zd = V3_Z(v2) ;
        if (MRIindexNotInVolume(mri_bias, xd, yd, zd) == 0)
        {
          MRIsampleVolume(mri_bias, xd, yd, zd, &bias) ;
          val *= bias ;
          if (bias < 0)
            DiagBreak() ;
        }
        MRIsetVoxVal(mri_out, x, y, z, 0, val) ;
      }
    }
  }

  MatrixFree(&m_vox2vox) ;
  return(mri_out) ;

}


MRI *
MRIapplyBiasCorrectionSameGeometry(MRI *mri_in,
                                   MRI *mri_bias,
                                   MRI *mri_out,
                                   float target_val)
{
  int    x, y, z ;
  double bias, val ;

  if (mri_out == NULL)
    mri_out = MRIclone(mri_in, NULL) ;

  for (x = 0 ; x < mri_in->width ; x++)
  {
    for (y = 0 ; y < mri_in->height ; y++)
    {
      for (z = 0 ; z < mri_in->depth ; z++)
      {
        val = MRIgetVoxVal(mri_in, x, y, z, 0) ;
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        MRIsampleVolume(mri_bias, x, y, z, &bias) ;
        val *= (target_val/bias) ;
        if (bias < 0)
          DiagBreak() ;
        MRIsetVoxVal(mri_out, x, y, z, 0, val) ;
      }
    }
  }

  return(mri_out) ;
}

