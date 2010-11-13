/**
 * @file  mri_normalize.c
 * @brief Normalize the white-matter, based on control points.
 *
 * The variation in intensity due to the B1 bias field is corrected.
 *
 * Reference:
 * "Cortical Surface-Based Analysis I: Segmentation and Surface
 * Reconstruction", Dale, A.M., Fischl, B., Sereno, M.I.
 * (1999) NeuroImage 9(2):179-194
 */
/*
 * Original Author: Bruce Fischl
 * CVS Revision Info:
 *    $Author: mreuter $
 *    $Date: 2010/11/13 20:05:25 $
 *    $Revision: 1.65.2.1 $
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


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include "mri.h"
#include "macros.h"
#include "error.h"
#include "diag.h"
#include "timer.h"
#include "proto.h"
#include "mrinorm.h"
#include "mri_conform.h"
#include "tags.h"
#include "version.h"
#include "cma.h"

static int remove_surface_outliers(MRI *mri_ctrl_src, MRI *mri_dist, MRI *mri_src, 
                                   MRI *mri_ctrl_dst) ;
static MRI *MRIremoveWMOutliersAndRetainMedialSurface(MRI *mri_src,
                                                      MRI *mri_src_ctrl,
                                                      MRI *mri_dst_ctrl, int intensity_below) ;
static MRI *MRIremoveWMOutliers(MRI *mri_src,
                                MRI *mri_src_ctrl,
                                MRI *mri_dst_ctrl, int intensity_below) ;
int main(int argc, char *argv[]) ;
static int get_option(int argc, char *argv[]) ;
static void  usage_exit(int code) ;
static MRI *compute_bias(MRI *mri_src, MRI *mri_dst, MRI *mri_bias) ;
static MRI *add_interior_points(MRI *mri_src, MRI *mri_vals, 
                                float intensity_above, float intensity_below,
                                MRI_SURFACE *mris_interior1, 
                                MRI_SURFACE *mris_interior2, 
                                MRI *mri_aseg, MRI *mri_dst) ;
static int conform = 1 ;
static int gentle_flag = 0 ;
static int nosnr = 1 ;
static double min_dist = 2.5 ; // mm away from border in -surface

static float bias_sigma = 8.0 ;

static char *mask_fname ;
static char *interior_fname1 = NULL ;
static char *interior_fname2 = NULL ;

char *Progname ;

static int scan_type = MRI_UNKNOWN ;

static int prune = 0 ;  /* off by default */
static MRI_NORM_INFO  mni = {} ;
static int verbose = 1 ;
static int num_3d_iter = 2 ;

static float intensity_above = 25 ;
static float intensity_below = 10 ;

static char *control_point_fname ;
static char *long_control_volume_fname = NULL ;
static char *long_bias_volume_fname = NULL ;

static char *aseg_fname = NULL ;
//static int aseg_wm_labels[] =
// { Left_Cerebral_White_Matter, Right_Cerebral_White_Matter, Brain_Stem} ;
static int aseg_wm_labels[] = 
{
  Left_Cerebral_White_Matter, Right_Cerebral_White_Matter
} ;
#define NWM_LABELS (sizeof(aseg_wm_labels) / sizeof(aseg_wm_labels[0]))

static char *control_volume_fname = NULL ;
static char *bias_volume_fname = NULL ;
static int read_flag = 0 ;
static int long_flag = 0 ;

static int no1d = 0 ;
static int file_only = 0 ;

static char *surface_fname = NULL ;
static TRANSFORM *surface_xform ;
static char *surface_xform_fname = NULL ;

int
main(int argc, char *argv[]) {
  char   **av ;
  int    ac, nargs, n ;
  MRI    *mri_src, *mri_dst = NULL, *mri_bias, *mri_orig, *mri_aseg = NULL ;
  char   *in_fname, *out_fname ;
  int          msec, minutes, seconds ;
  struct timeb start ;

  char cmdline[CMD_LINE_LEN] ;

  make_cmd_version_string
  (argc, argv,
   "$Id: mri_normalize.c,v 1.65.2.1 2010/11/13 20:05:25 mreuter Exp $",
   "$Name:  $",
   cmdline);

  /* rkt: check for and handle version tag */
  nargs = handle_version_option
          (argc, argv,
           "$Id: mri_normalize.c,v 1.65.2.1 2010/11/13 20:05:25 mreuter Exp $",
           "$Name:  $");
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  Progname = argv[0] ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  mni.max_gradient = MAX_GRADIENT ;
  ac = argc ;
  av = argv ;
  for ( ; argc > 1 && ISOPTION(*argv[1]) ; argc--, argv++) {
    nargs = get_option(argc, argv) ;
    argc -= nargs ;
    argv += nargs ;
  }

  if (argc < 3)
    usage_exit(0) ;

  if (argc < 1)
    ErrorExit(ERROR_BADPARM, "%s: no input name specified", Progname) ;
  in_fname = argv[1] ;

  if (argc < 2)
    ErrorExit(ERROR_BADPARM, "%s: no output name specified", Progname) ;
  out_fname = argv[2] ;

  if (verbose)
    fprintf(stderr, "reading from %s...\n", in_fname) ;
  mri_src = MRIread(in_fname) ;
  if (!mri_src)
    ErrorExit(ERROR_NO_FILE, "%s: could not open source file %s",
              Progname, in_fname) ;
  MRIaddCommandLine(mri_src, cmdline) ;
  if (surface_fname)
  {
    MRI_SURFACE *mris ;
    MRI         *mri_dist, *mri_dist_sup, *mri_ctrl ;
    LTA          *lta= NULL ;

    if (control_point_fname)  // do one pass with only file control points first 
    {
      MRI3dUseFileControlPoints(mri_src, control_point_fname) ;
      mri_dst =
        MRI3dGentleNormalize(mri_src,
                             NULL,
                             DEFAULT_DESIRED_WHITE_MATTER_VALUE,
                             NULL,
                             intensity_above,
                             intensity_below/2,1,
                             bias_sigma);
    }
    mris = MRISread(surface_fname) ;
    if (mris == NULL)
      ErrorExit(ERROR_NOFILE,"%s: could not surface %s",Progname,surface_fname);
    TransformInvert(surface_xform, NULL) ;
    if (surface_xform->type == MNI_TRANSFORM_TYPE ||
        surface_xform->type == TRANSFORM_ARRAY_TYPE ||
        surface_xform->type  == REGISTER_DAT) {
      lta = (LTA *)(surface_xform->xform) ;
      
#if 0
      if (invert) {
        VOL_GEOM vgtmp;
        LT *lt;
        MATRIX *m_tmp = lta->xforms[0].m_L ;
        lta->xforms[0].m_L = MatrixInverse(lta->xforms[0].m_L, NULL) ;
        MatrixFree(&m_tmp) ;
        lt = &lta->xforms[0];
        if (lt->dst.valid == 0 || lt->src.valid == 0) {
          fprintf(stderr, "WARNING:***************************************************************\n");
          fprintf(stderr, "WARNING:dst volume infor is invalid.  Most likely produce wrong inverse.\n");
          fprintf(stderr, "WARNING:***************************************************************\n");
        }
        copyVolGeom(&lt->dst, &vgtmp);
        copyVolGeom(&lt->src, &lt->dst);
        copyVolGeom(&vgtmp, &lt->src);
      }
#endif
    }

    if (stricmp(surface_xform_fname, "identity.nofile") != 0)
      MRIStransform(mris, NULL, surface_xform, NULL) ;
    mri_dist = MRIcloneDifferentType(mri_dst, MRI_FLOAT) ;
    printf("computing distance transform\n") ;
    MRIScomputeDistanceToSurface(mris, mri_dist, mri_dist->xsize) ;
    MRIscalarMul(mri_dist, mri_dist, -1) ;
    printf("computing nonmaximum suppression\n") ;
    mri_dist_sup = MRInonMaxSuppress(mri_dist, NULL, 0, 1) ;
    mri_ctrl = MRIcloneDifferentType(mri_dist_sup, MRI_UCHAR) ;
    MRIbinarize(mri_dist_sup, mri_ctrl, min_dist, CONTROL_NONE, CONTROL_MARKED) ;
    if (control_point_fname)
      MRInormAddFileControlPoints(mri_ctrl, CONTROL_MARKED) ;

    if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    {
      MRIwrite(mri_dist, "d.mgz");
      MRIwrite(mri_dist_sup, "dm.mgz");
      MRIwrite(mri_ctrl, "c.mgz");
    }
    MRIeraseBorderPlanes(mri_ctrl, 4) ;
    remove_surface_outliers(mri_ctrl, mri_dist, mri_src, mri_ctrl) ;
    mri_bias = MRIbuildBiasImage(mri_src, mri_ctrl, NULL, 0.0) ;
    if (bias_sigma> 0)
    {
      MRI *mri_kernel = MRIgaussian1d(bias_sigma, -1) ;
      if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
        MRIwrite(mri_bias, "b.mgz") ;
      printf("smoothing bias field\n") ;
      MRIconvolveGaussian(mri_bias, mri_bias, mri_kernel) ;
      if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
        MRIwrite(mri_bias, "bs.mgz") ;
      MRIfree(&mri_kernel);
    }
    MRIfree(&mri_ctrl) ;
    mri_dst = MRIapplyBiasCorrectionSameGeometry
              (mri_src, mri_bias, mri_dst, 
               DEFAULT_DESIRED_WHITE_MATTER_VALUE) ;
    printf("writing normalized volume to %s\n", out_fname) ;
    MRIwrite(mri_dst, out_fname) ;
    exit(0) ;
  }
  if (!mriConformed(mri_src) && conform > 0) {
    printf("unconformed source detected - conforming...\n") ;
    mri_src = MRIconform(mri_src) ;
  }

  if (mask_fname) {
    MRI *mri_mask ;

    mri_mask = MRIread(mask_fname) ;
    if (!mri_mask)
      ErrorExit(ERROR_NOFILE, "%s: could not open mask volume %s.\n",
                Progname, mask_fname) ;
    MRImask(mri_src, mri_mask, mri_src, 0, 0) ;
    MRIfree(&mri_mask) ;
  }
  if (read_flag) {
    MRI *mri_ctrl ;
    double scale ;

    mri_bias = MRIread(bias_volume_fname) ;
    if (!mri_bias)
      ErrorExit
      (ERROR_BADPARM,
       "%s: could not read bias volume %s", Progname, bias_volume_fname) ;
    mri_ctrl = MRIread(control_volume_fname) ;
    if (!mri_ctrl)
      ErrorExit
      (ERROR_BADPARM,
       "%s: could not read control volume %s",
       Progname, control_volume_fname) ;
    MRIbinarize(mri_ctrl, mri_ctrl, 1, 0, 128) ;
    mri_dst = MRImultiply(mri_bias, mri_src, NULL) ;
    scale = MRImeanInLabel(mri_dst, mri_ctrl, 128) ;
    printf("mean in wm is %2.0f, scaling by %2.2f\n", scale, 110/scale) ;
    scale = 110/scale ;
    MRIscalarMul(mri_dst, mri_dst, scale) ;
    MRIwrite(mri_dst, out_fname) ;
    exit(0) ;
  }

  if (long_flag) {
    MRI *mri_ctrl ;
    double scale ;

    mri_bias = MRIread(long_bias_volume_fname) ;
    if (!mri_bias)
      ErrorExit
      (ERROR_BADPARM,
       "%s: could not read bias volume %s", Progname, long_bias_volume_fname) ;
    mri_ctrl = MRIread(long_control_volume_fname) ;
    if (!mri_ctrl)
      ErrorExit
      (ERROR_BADPARM,
       "%s: could not read control volume %s",
       Progname, long_control_volume_fname) ;
    MRIbinarize(mri_ctrl, mri_ctrl, 1, 0, CONTROL_MARKED) ;
    if (mri_ctrl->type != MRI_UCHAR)
    {
      MRI *mri_tmp ;
      mri_tmp = MRIchangeType(mri_ctrl, MRI_UCHAR, 0, 1,1);
      MRIfree(&mri_ctrl) ; mri_ctrl = mri_tmp ;
    }
    scale = MRImeanInLabel(mri_src, mri_ctrl, CONTROL_MARKED) ;
    printf("mean in wm is %2.0f, scaling by %2.2f\n", scale, 110/scale) ;
    scale = DEFAULT_DESIRED_WHITE_MATTER_VALUE/scale ;
    mri_dst = MRIscalarMul(mri_src, NULL, scale) ;
    MRIremoveWMOutliers(mri_dst, mri_ctrl, mri_ctrl, intensity_below/2) ;
    mri_bias = MRIbuildBiasImage(mri_dst, mri_ctrl, NULL, 0.0) ;
    MRIsoapBubble(mri_bias, mri_ctrl, mri_bias, 25) ;
    MRIapplyBiasCorrectionSameGeometry(mri_dst, mri_bias, mri_dst,
                                       DEFAULT_DESIRED_WHITE_MATTER_VALUE);
    //    MRIwrite(mri_dst, out_fname) ;
    //    exit(0) ;
  }

#if 0
#if 0
  if ((mri_src->type != MRI_UCHAR) ||
      (!(mri_src->xsize == 1 && mri_src->ysize == 1 && mri_src->zsize == 1)))
#else
  if (conform || (mri_src->type != MRI_UCHAR && conform > 0))
#endif
  {
    MRI  *mri_tmp ;

    fprintf
    (stderr,
     "downsampling to 8 bits and scaling to isotropic voxels...\n") ;
    mri_tmp = MRIconform(mri_src) ;
    mri_src = mri_tmp ;
  }
#endif

  if (aseg_fname) {
    mri_aseg = MRIread(aseg_fname) ;
    if (mri_aseg == NULL)
      ErrorExit
      (ERROR_NOFILE,
       "%s: could not read aseg from file %s", Progname, aseg_fname) ;
    if (!mriConformed(mri_aseg))
      ErrorExit(ERROR_UNSUPPORTED, "%s: aseg volume %s must be conformed", Progname, aseg_fname) ;
  } else
    mri_aseg = NULL ;

  if (verbose)
    fprintf(stderr, "normalizing image...\n") ;
  TimerStart(&start) ;

  if (control_point_fname)
    MRI3dUseFileControlPoints(mri_src, control_point_fname) ;
  if (control_volume_fname)
    // this just setup writing control-point volume saving
    MRI3dWriteControlPoints(control_volume_fname) ;

  /* first do a gentle normalization to get
     things in the right intensity range */
  if (long_flag == 0)  // if long, then this will already have been done with base control points
  {
    if (control_point_fname != NULL)  /* do one pass with only
                                         file control points first */
      mri_dst =
        MRI3dGentleNormalize(mri_src,
                             NULL,
                             DEFAULT_DESIRED_WHITE_MATTER_VALUE,
                             NULL,
                             intensity_above,
                             intensity_below/2,1,
                             bias_sigma);
    else
      mri_dst = MRIcopy(mri_src, NULL) ;
  }
  if (mri_aseg) {
    MRI *mri_ctrl, *mri_bias ;
    int  i ;

    mri_ctrl = MRIclone(mri_aseg, NULL) ;
    for (i = 0 ; i < NWM_LABELS ; i++)
      MRIcopyLabel(mri_aseg, mri_ctrl, aseg_wm_labels[i]) ;
    fprintf(stderr,"removing outliers in the aseg WM...\n") ;
    MRIremoveWMOutliersAndRetainMedialSurface(mri_dst, mri_ctrl, mri_ctrl, intensity_below) ;
    MRIbinarize(mri_ctrl, mri_ctrl, 1, CONTROL_NONE, CONTROL_MARKED) ;
    MRInormAddFileControlPoints(mri_ctrl, CONTROL_MARKED) ;

    if (interior_fname1)
    {
      MRIS *mris_interior1, *mris_interior2 ;
      mris_interior1 = MRISread(interior_fname1) ;
      if (mris_interior1 == NULL)
        ErrorExit(ERROR_NOFILE, 
                  "%s: could not read white matter surface from %s\n", 
                  Progname, interior_fname1) ;
      mris_interior2 = MRISread(interior_fname2) ;
      if (mris_interior2 == NULL)
        ErrorExit(ERROR_NOFILE, 
                  "%s: could not read white matter surface from %s\n", 
                  Progname, interior_fname2) ;
      add_interior_points(mri_ctrl, mri_dst, intensity_above, 1.25*intensity_below, mris_interior1, mris_interior2, mri_aseg, mri_ctrl) ;
      MRISfree(&mris_interior1) ;
      MRISfree(&mris_interior2) ;
    }
    if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
      MRIwrite(mri_ctrl, "norm_ctrl.mgz") ;
    mri_bias = MRIbuildBiasImage(mri_dst, mri_ctrl, NULL, 0.0) ;
    if (bias_sigma> 0)
    {
      MRI *mri_kernel = MRIgaussian1d(bias_sigma, -1) ;
      MRIconvolveGaussian(mri_bias, mri_bias, mri_kernel) ;
      MRIfree(&mri_kernel);
    }
    MRIfree(&mri_ctrl) ;
    MRIfree(&mri_aseg) ;
    mri_dst = MRIapplyBiasCorrectionSameGeometry
              (mri_dst, mri_bias, mri_dst, 
               DEFAULT_DESIRED_WHITE_MATTER_VALUE) ;
    if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
      MRIwrite(mri_dst, "norm_1.mgz") ;
  } else {
    if (!no1d) {
      MRInormInit(mri_src, &mni, 0, 0, 0, 0, 0.0f) ;
      mri_dst = MRInormalize(mri_src, NULL, &mni) ;
      if (!mri_dst)
      {
        no1d = 1 ;
        printf("1d normalization failed - trying no1d...\n") ;
        //        ErrorExit(ERROR_BADPARM, "%s: normalization failed", Progname) ;
      }
    } 
    if (no1d) {
      if ((file_only && nosnr) ||
          ((gentle_flag != 0) && (control_point_fname != NULL))) {
        if (mri_dst == NULL)
          mri_dst = MRIcopy(mri_src, NULL) ;
      } else {
        if (nosnr)
        {
          if (interior_fname1)
          {
            MRIS *mris_interior1, *mris_interior2 ;
            MRI  *mri_ctrl ;

            printf("computing initial normalization using surface interiors\n");
            mri_ctrl = MRIcloneDifferentType(mri_src, MRI_UCHAR) ;
            mris_interior1 = MRISread(interior_fname1) ;
            if (mris_interior1 == NULL)
              ErrorExit(ERROR_NOFILE, 
                        "%s: could not read white matter surface from %s\n", 
                        Progname, interior_fname1) ;
            mris_interior2 = MRISread(interior_fname2) ;
            if (mris_interior2 == NULL)
              ErrorExit(ERROR_NOFILE, 
                        "%s: could not read white matter surface from %s\n", 
                        Progname, interior_fname2) ;
            add_interior_points(mri_ctrl, mri_dst, intensity_above, 1.25*intensity_below, 
                                mris_interior1, mris_interior2, mri_aseg, mri_ctrl) ;
            MRISfree(&mris_interior1) ;
            MRISfree(&mris_interior2) ;
            mri_bias = MRIbuildBiasImage(mri_dst, mri_ctrl, NULL, 0.0) ;
            if (bias_sigma> 0)
            {
              MRI *mri_kernel = MRIgaussian1d(bias_sigma, -1) ;
              MRIconvolveGaussian(mri_bias, mri_bias, mri_kernel) ;
              MRIfree(&mri_kernel);
            }
            mri_dst = MRIapplyBiasCorrectionSameGeometry(mri_src, mri_bias, mri_dst, 
                                                         DEFAULT_DESIRED_WHITE_MATTER_VALUE) ;
            MRIfree(&mri_ctrl) ;
          }
          else if (long_flag == 0)  // no initial normalization specified
            mri_dst = MRIclone(mri_src, NULL) ;
        }
        else
        {
          printf("computing initial normalization using SNR...\n") ;
          mri_dst = MRInormalizeHighSignalLowStd
            (mri_src, mri_dst, bias_sigma,
             DEFAULT_DESIRED_WHITE_MATTER_VALUE) ; 
        }
      }
      if (!mri_dst)
        ErrorExit
        (ERROR_BADPARM, "%s: could not allocate volume", Progname) ;
    }
  }

  if (file_only == 0)
    MRI3dGentleNormalize(mri_dst, NULL, DEFAULT_DESIRED_WHITE_MATTER_VALUE,
                         mri_dst,
                         intensity_above, intensity_below/2,
                         file_only, bias_sigma);
  mri_orig = MRIcopy(mri_dst, NULL) ;
  for (n = 0 ; n < num_3d_iter ; n++) {
    if (file_only)
      break ;
    fprintf(stderr, "3d normalization pass %d of %d\n", n+1, num_3d_iter) ;
    if (gentle_flag)
      MRI3dGentleNormalize(mri_dst, NULL, DEFAULT_DESIRED_WHITE_MATTER_VALUE,
                           mri_dst,
                           intensity_above/2, intensity_below/2,
                           file_only, bias_sigma);
    else
      MRI3dNormalize(mri_orig, mri_dst, DEFAULT_DESIRED_WHITE_MATTER_VALUE,
                     mri_dst,
                     intensity_above, intensity_below,
                     file_only, prune, bias_sigma, scan_type);
  }

  if (control_volume_fname)
    // this just setup writing control-point volume saving
    MRI3dWriteControlPoints(control_volume_fname) ;
  if (bias_volume_fname) {
    mri_bias = compute_bias(mri_src, mri_dst, NULL) ;
    printf("writing bias field to %s....\n", bias_volume_fname) ;
    MRIwrite(mri_bias, bias_volume_fname) ;
    MRIfree(&mri_bias) ;
  }

  if (verbose)
    fprintf(stderr, "writing output to %s\n", out_fname) ;
  MRIwrite(mri_dst, out_fname) ;
  msec = TimerStop(&start) ;

  MRIfree(&mri_src);
  MRIfree(&mri_dst);

  seconds = nint((float)msec/1000.0f) ;
  minutes = seconds / 60 ;
  seconds = seconds % 60 ;
  fprintf(stderr, "3D bias adjustment took %d minutes and %d seconds.\n",
          minutes, seconds) ;
  exit(0) ;
  return(0) ;
}

/*----------------------------------------------------------------------
  Parameters:

  Description:
  ----------------------------------------------------------------------*/
static int
get_option(int argc, char *argv[]) {
  int  nargs = 0 ;
  char *option ;

  option = argv[1] + 1 ;            /* past '-' */
  if (!stricmp(option, "-help")||!stricmp(option, "-usage")) {
    usage_exit(0);
  } else if (!stricmp(option, "no1d")) {
    no1d = 1 ;
    fprintf(stderr, "disabling 1d normalization...\n") ;
  } else if (!stricmp(option, "MASK")) {
    mask_fname = argv[2] ;
    nargs = 1 ;
    printf("using MR volume %s to mask input volume...\n", mask_fname) ;
  } else if (!stricmp(option, "SURFACE")) {
    surface_fname = argv[2] ;
    surface_xform = TransformRead(argv[3]) ;
    surface_xform_fname = argv[3] ;
    if (surface_xform == NULL)
      ErrorExit(ERROR_NOFILE, "%s:could not load xform from %s",Progname,
                argv[3]) ;
    nargs = 2 ;
  } else if (!stricmp(option, "MIN_DIST")) {
    min_dist = atof(argv[2]) ;
    nargs = 1 ;
    printf("retaining nonmaximum suppressed points that are at least %2.3fmm from the boundary\n",
           min_dist) ;
  } else if (!stricmp(option, "INTERIOR")) {
    interior_fname1 = argv[2] ;
    interior_fname2 = argv[3] ;
    nargs = 2 ;
    printf("using surfaces %s and %s to compute wm interior\n", 
           interior_fname1, interior_fname2) ;
  } else if (!stricmp(option, "MGH_MPRAGE") || !stricmp(option, "MPRAGE")) {
    scan_type = MRI_MGH_MPRAGE;
    printf("assuming input volume is MGH (Van der Kouwe) MP-RAGE\n") ;
    intensity_below = 15 ;
  } else if (!stricmp(option, "WASHU_MPRAGE")) {
    scan_type = MRI_WASHU_MPRAGE;
    printf("assuming input volume is WashU MP-RAGE (dark GM)\n") ;
    intensity_below = 22 ;

  } else if (!stricmp(option, "monkey")) {
    no1d = 1 ;
    num_3d_iter = 1 ;
    printf("disabling 1D normalization and "
           "setting niter=1, make sure to use "
           "-f to specify control points\n") ;
  } else if (!stricmp(option, "nosnr")) {
    nosnr = 1 ;
    printf("disabling SNR normalization\n") ;
  } else if (!stricmp(option, "snr")) {
    nosnr = 0 ;
    printf("enabling SNR normalization\n") ;
  } else if (!stricmp(option, "sigma")) {
    bias_sigma = atof(argv[2]) ;
    nargs = 1 ;
    printf("using Gaussian smoothing of bias field, sigma=%2.3f\n",
           bias_sigma) ;
  } else if (!stricmp(option, "conform")) {
    conform = atoi(argv[1]) ;
    nargs = 1 ;
    fprintf(stderr, 
            "%sinterpolating and embedding volume to be 256^3...\n", 
            conform ? "": "not ") ;
  } else if (!stricmp(option, "noconform")) {
    conform = 0 ;
    fprintf(stderr, 
            "%sinterpolating and embedding volume to be 256^3...\n", 
            conform ? "": "not ") ;
  } else if (!stricmp(option, "aseg") || !stricmp(option, "segmentation")) {
    aseg_fname = argv[2] ;
    nargs = 1  ;
    fprintf(stderr,
            "using segmentation for initial intensity normalization\n") ;
  } else if (!stricmp(option, "gentle")) {
    gentle_flag = 1 ;
    fprintf(stderr, "performing kinder gentler normalization...\n") ;
  } else if (!stricmp(option, "file_only") ||
             !stricmp(option, "fonly") ||
             !stricmp(option, "fileonly")) {
    file_only = 1 ;
    control_point_fname = argv[2] ;
    no1d = 1 ;
    nargs = 1 ;
    fprintf(stderr, "using control points from file %s...\n",
            control_point_fname) ;
    fprintf(stderr, "only using file control points...\n") ;
  } else switch (toupper(*option)) {
    case 'D':
      Gx = atoi(argv[2]) ;
      Gy = atoi(argv[3]) ;
      Gz = atoi(argv[4]) ;
      nargs = 3 ;
      printf("debugging voxel (%d, %d, %d)\n", Gx, Gy, Gz) ;
      break ;
    case 'V':
      Gvx = atoi(argv[2]) ;
      Gvy = atoi(argv[3]) ;
      Gvz = atoi(argv[4]) ;
      nargs = 3 ;
      printf("debugging alternative voxel (%d, %d, %d)\n", Gvx, Gvy, Gvz) ;
      break ;
    case 'P':
      prune = atoi(argv[2]) ;
      nargs = 1 ;
      printf("turning control point pruning %s\n", prune > 0 ? "on" : "off") ;
      if (prune == 0)
        prune = -1 ;
      break ;
    case 'R':
      read_flag = 1 ;
      nargs = 2 ;
      control_volume_fname = argv[2] ;
      bias_volume_fname = argv[3] ;
      printf("reading bias field from %s and ctrl points from %s\n",
             bias_volume_fname, control_volume_fname) ;
      break ;
    case 'L':
      long_flag = 1 ;
      no1d = 1 ;
      nargs = 2 ;
      long_control_volume_fname = argv[2] ;
      long_bias_volume_fname = argv[3] ;
      printf("reading bias field from %s and ctrl points from %s\n",
             long_bias_volume_fname, long_control_volume_fname) ;
      break ;
    case 'W':
      control_volume_fname = argv[2] ;
      bias_volume_fname = argv[3] ;
      nargs = 2 ;
      printf("writing ctrl pts to   %s\n", control_volume_fname) ;
      printf("writing bias field to %s\n", bias_volume_fname) ;
      break ;
    case 'F':
      control_point_fname = argv[2] ;
      nargs = 1 ;
      fprintf(stderr, "using control points from file %s...\n",
              control_point_fname) ;
      break ;
    case 'A':
      intensity_above = atof(argv[2]) ;
      fprintf(stderr,
              "using control point with intensity %2.1f above target.\n",
              intensity_above) ;
      nargs = 1 ;
      break ;
    case 'B':
      intensity_below = atof(argv[2]) ;
      fprintf(stderr,
              "using control point with intensity %2.1f below target.\n",
              intensity_below) ;
      nargs = 1 ;
      break ;
    case 'G':
      mni.max_gradient = atof(argv[2]) ;
      fprintf(stderr, "using max gradient = %2.3f\n", mni.max_gradient) ;
      nargs = 1 ;
      break ;
#if 0
    case 'V':
      verbose = !verbose ;
      break ;
#endif
    case 'N':
      num_3d_iter = atoi(argv[2]) ;
      nargs = 1 ;
      fprintf(stderr, "performing 3d normalization %d times\n", num_3d_iter) ;
      break ;
    case '?':
    case 'U':
    case 'H':
      usage_exit(0) ;
      break ;
    default:
      fprintf(stderr, "unknown option %s\n", argv[1]) ;
      exit(1) ;
      break ;
    }

  return(nargs) ;
}

static void
usage_exit(int code) {
  printf("%s input output\n\n", Progname) ;
  printf("  -n <int n>         use n 3d normalization "
         "iterations (default=%d)\n", num_3d_iter);
  printf("  -no1d              disable 1d normalization\n");
  printf("  -conform           interpolate and embed volume to be 256^3\n");
  printf("  -noconform         do not conform the volume\n");
  printf("  -gentle            perform kinder gentler normalization\n");
  printf("  -f <path to file>  use control points "
         "file (usually control.dat)\n");
  printf("  -fonly <fname>     use only control points file\n");
  printf("  -w <mri_vol c> <mri_vol b> : write ctrl point(c) "
         "and bias field(b) volumes\n");
  printf("  -a <float a>       use control point with "
         "intensity a above target (default=%2.1f)\n", intensity_above);
  printf("  -b <float b>       use control point with "
         "intensity b below target (default=%2.1f)\n", intensity_below);
  printf("  -g <float g>       use max intensity/mm "
         "gradient g (default=%2.3f)\n", mni.max_gradient);
  printf("  -prune <boolean>   turn pruning of control points "
         "on/off (default=off).\n"
         "                     pruning useful if white is "
         "expanding into gm\n") ;
  printf("  -MASK maskfile \n");
  printf("  -monkey            turns off 1d, sets num_3d_iter=1\n");
  printf("  -nosnr             disable snr normalization\n");
  printf("  -sigma sigma       smooth bias field\n");
  printf("  -aseg aseg\n");
  printf("  -v Gvx Gvy Gvz     for debugging\n");
  printf("  -d Gx Gy Gz        for debugging\n");
  printf("  -r controlpoints biasfield : for reading\n");
  printf("  -surface <surface> <xform> : normalize based on the skelton of the\ninterior"
         "                               of the transformed surface\n") ;
         
  printf("  -u or -h           print usage\n");
  printf("  \n");

  exit(code);
}

static MRI *
compute_bias(MRI *mri_src, MRI *mri_dst, MRI *mri_bias) {
  int x, y, z ;
  float bias, src, dst ;

  if (!mri_bias)
    mri_bias = MRIalloc
               (mri_src->width, mri_src->height, mri_src->depth, MRI_FLOAT) ;

  MRIcopyHeader(mri_src, mri_bias) ;
  for (x = 0 ; x < mri_src->width ; x++) {
    for (y = 0; y < mri_src->height ; y++) {
      for (z = 0 ; z < mri_src->depth ; z++) {
        src = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        dst = MRIgetVoxVal(mri_dst, x, y, z, 0) ;
        if (FZERO(src))
          bias = 1 ;
        else
          bias = dst/src ;
        MRIsetVoxVal(mri_bias, x, y, z, 0, bias) ;
      }
    }
  }

  return(mri_bias) ;
}

#if 1
static MRI *
MRIremoveWMOutliersAndRetainMedialSurface(MRI *mri_src, MRI *mri_src_ctrl, MRI *mri_dst_ctrl,
                    int intensity_below) 
{
  MRI       *mri_bin, *mri_dist, *mri_dist_sup, *mri_outliers = NULL ;
  float     max, thresh, val;
  HISTOGRAM *histo, *hsmooth ;
  int       wm_peak, x, y, z, nremoved = 0, whalf = 5 ;

  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    MRIwrite(mri_src_ctrl, "sc.mgz") ;

  mri_bin = MRIbinarize(mri_dst_ctrl, NULL, 1, 0, 1) ;
  mri_dist = MRIdistanceTransform(mri_bin, NULL, 1, -1, DTRANS_MODE_SIGNED, NULL);
  MRIscalarMul(mri_dist, mri_dist, -1) ;
  mri_dist_sup = MRInonMaxSuppress(mri_dist, NULL, 0, 1) ;
  mri_dst_ctrl = MRIbinarize(mri_dist_sup, mri_dst_ctrl, 1, 0, 1) ;
  histo = MRIhistogramLabel(mri_src, mri_src_ctrl, 1, 256) ;
  hsmooth = HISTOcopy(histo, NULL) ;
  HISTOsmooth(histo, hsmooth, 2) ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON) {
    HISTOplot(histo, "h.plt") ;
    HISTOplot(hsmooth, "hs.plt") ;
  }
  wm_peak = HISTOfindHighestPeakInRegion(hsmooth, 1, hsmooth->nbins-1) ;
  wm_peak = hsmooth->bins[wm_peak] ;
  thresh = wm_peak-intensity_below ;

  HISTOfree(&histo) ;
  HISTOfree(&hsmooth) ;
  if (Gdiag & DIAG_WRITE)
    mri_outliers = MRIclone(mri_dst_ctrl, NULL) ;
  for (x = 0 ; x < mri_src->width ; x++) {
    for (y = 0 ; y < mri_src->height ; y++) {
      for (z = 0 ; z < mri_src->depth ; z++) {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        if (nint(MRIgetVoxVal(mri_dst_ctrl, x, y, z, 0)) == 0)
          continue ;
        max = MRImaxInLabelInRegion(mri_src, mri_dst_ctrl, 1, x, y, z, whalf);
        val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        if (val+intensity_below < max && val < thresh) {
          MRIsetVoxVal(mri_dst_ctrl, x, y, z, 0, 0) ;
          if (mri_outliers)
            MRIsetVoxVal(mri_outliers, x, y, z, 0, 128) ;
          nremoved++ ;
        }
      }
    }
  }

  fprintf(stderr, "%d control points removed\n", nremoved) ;
  if (mri_outliers)
  {
    fprintf(stderr, "writing out.mgz outlier volume\n") ;
    MRIwrite(mri_outliers, "out.mgz") ; 
    MRIfree(&mri_outliers) ;
  }
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    MRIwrite(mri_dst_ctrl, "dc.mgz") ;
  MRIfree(&mri_bin) ;
  MRIfree(&mri_dist);
  MRIfree(&mri_dist_sup);
  return(mri_dst_ctrl);
}
static MRI *
MRIremoveWMOutliers(MRI *mri_src, MRI *mri_src_ctrl, MRI *mri_dst_ctrl,
                    int intensity_below) 
{
  MRI       *mri_bin, *mri_outliers = NULL ;
  float     max, thresh, val;
  HISTOGRAM *histo, *hsmooth ;
  int       wm_peak, x, y, z, nremoved = 0, whalf = 5, total  ;

  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    MRIwrite(mri_src_ctrl, "sc.mgz") ;

  if (mri_dst_ctrl == NULL)
    mri_dst_ctrl = MRIcopy(mri_src_ctrl, NULL) ;
  mri_bin = MRIbinarize(mri_src_ctrl, NULL, 1, 0, CONTROL_MARKED) ;
  histo = MRIhistogramLabel(mri_src, mri_bin, 1, 256) ;
  hsmooth = HISTOcopy(histo, NULL) ;
  HISTOsmooth(histo, hsmooth, 2) ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON) {
    HISTOplot(histo, "h.plt") ;
    HISTOplot(hsmooth, "hs.plt") ;
  }
  wm_peak = HISTOfindHighestPeakInRegion(hsmooth, 1, hsmooth->nbins-1) ;
  wm_peak = hsmooth->bins[wm_peak] ;
  thresh = wm_peak-intensity_below ;

  HISTOfree(&histo) ;
  HISTOfree(&hsmooth) ;
  if (Gdiag & DIAG_WRITE)
    mri_outliers = MRIclone(mri_dst_ctrl, NULL) ;
  for (total = x = 0 ; x < mri_src->width ; x++) {
    for (y = 0 ; y < mri_src->height ; y++) {
      for (z = 0 ; z < mri_src->depth ; z++) {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        if (nint(MRIgetVoxVal(mri_dst_ctrl, x, y, z, 0)) == 0)
          continue ;
        max = MRImaxInLabelInRegion(mri_src, mri_bin, 1, x, y, z, whalf);
        val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        total++ ;
        if (val+intensity_below < max && val < thresh) {
          MRIsetVoxVal(mri_dst_ctrl, x, y, z, 0, 0) ;
          if (mri_outliers)
            MRIsetVoxVal(mri_outliers, x, y, z, 0, 128) ;
          nremoved++ ;
        }
      }
    }
  }

  fprintf(stderr, "%d control points removed (%2.1f%%)\n", nremoved, 100.0*(double)nremoved/(double)total) ;
  if (mri_outliers)
  {
    fprintf(stderr, "writing out.mgz outlier volume\n") ;
    MRIwrite(mri_outliers, "out.mgz") ; 
    MRIfree(&mri_outliers) ;
  }
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    MRIwrite(mri_dst_ctrl, "dc.mgz") ;
  MRIfree(&mri_bin) ;
  return(mri_dst_ctrl);
}
#else
static MRI *
MRIremoveWMOutliersAndRetainMedialSurface(MRI *mri_src, MRI *mri_src_ctrl, MRI *mri_dst_ctrl,
                    int intensity_below) {
  MRI  *mri_inside, *mri_bin ;
  HISTOGRAM *histo, *hsmooth ;
  int   wm_peak, x, y, z, nremoved ;
  float thresh, hi_thresh ;
  double val, lmean, max ;

  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    MRIwrite(mri_src_ctrl, "sc.mgz") ;
  if (mri_dst_ctrl != mri_src_ctrl)
    mri_dst_ctrl = MRIcopy(mri_src_ctrl, mri_dst_ctrl) ;
  mri_inside = MRIerode(mri_dst_ctrl, NULL) ;
  MRIbinarize(mri_inside, mri_inside, 1, 0, 1) ;

  histo = MRIhistogramLabel(mri_src, mri_inside, 1, 256) ;
  hsmooth = HISTOcopy(histo, NULL) ;
  HISTOsmooth(histo, hsmooth, 2) ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON) {
    HISTOplot(histo, "h.plt") ;
    HISTOplot(hsmooth, "hs.plt") ;
  }
  printf("using wm (%d) threshold %2.1f for removing exterior voxels\n",
         wm_peak, thresh) ;
  wm_peak = HISTOfindHighestPeakInRegion(hsmooth, 1, hsmooth->nbins-1) ;
  wm_peak = hsmooth->bins[wm_peak] ;
  thresh = wm_peak-intensity_below ;
  hi_thresh = wm_peak-.5*intensity_below ;
  printf("using wm (%d) threshold %2.1f for removing exterior voxels\n",
         wm_peak, thresh) ;

  // now remove stuff that's on the border and is pretty dark
  for (nremoved = x = 0 ; x < mri_src->width ; x++) {
    for (y = 0 ; y < mri_src->height ; y++) {
      for (z = 0 ; z < mri_src->depth ; z++) {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        /* if it's a control point,
            it's not in the interior of the wm,
            and it's T1 val is too low */
        if (MRIgetVoxVal(mri_dst_ctrl, x, y, z, 0) == 0)
          continue ;  // not a  control point

        /* if it's way far from the wm mode
            then remove it even if it's in the interior */
        val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        if (val < thresh-5) {
          MRIsetVoxVal(mri_dst_ctrl, x, y, z, 0, 0) ;
          nremoved++ ;
        }

        if (nint(MRIgetVoxVal(mri_inside, x, y, z, 0)) > 0)
          // don't process interior voxels further
          continue ;   // in the interior
        if (val < thresh) {
          MRIsetVoxVal(mri_dst_ctrl, x, y, z, 0, 0) ;
          nremoved++ ;
        } else {
          lmean =
            MRImeanInLabelInRegion(mri_src, mri_inside, 1, x, y, z, 7);
          if (val < lmean-10) {
            MRIsetVoxVal(mri_dst_ctrl, x, y, z, 0, 0) ;
            nremoved++ ;
          }
        }
      }
    }
  }

#if 0
  for (x = 0 ; x < mri_src->width ; x++) {
    for (y = 0 ; y < mri_src->height ; y++) {
      for (z = 0 ; z < mri_src->depth ; z++) {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        /* if it's a control point,
            it's not in the interior of the wm,
            and it's T1 val is too low */
        if (MRIgetVoxVal(mri_dst_ctrl, x, y, z, 0) == 0)
          continue ;  // not a  control point
        if (MRIcountNonzeroInNbhd(mri_dst_ctrl,3, x, y, z)<=2) {
          MRIsetVoxVal(mri_dst_ctrl, x, y, z, 0, 0) ;
          nremoved++ ;
        }
      }
    }
  }
#endif

  /* now take out voxels that have too big an intensity diff
     with surrounding ones */
  mri_bin = MRIbinarize(mri_dst_ctrl, NULL, 1, 0, 1) ;
  for (x = 0 ; x < mri_src->width ; x++) {
    for (y = 0 ; y < mri_src->height ; y++) {
      for (z = 0 ; z < mri_src->depth ; z++) {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        /* if it's a control point,
            it's not in the interior of the wm,
            and it's T1 val is too low */
        if (MRIgetVoxVal(mri_dst_ctrl, x, y, z, 0) == 0)
          continue ;  // not a  control point
        val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        max = MRImaxInLabelInRegion(mri_src, mri_bin, 1, x, y, z, 3);
        if (val+7 < max && val < hi_thresh) {
          MRIsetVoxVal(mri_dst_ctrl, x, y, z, 0, 0) ;
          nremoved++ ;
        }
      }
    }
  }
  MRIfree(&mri_bin) ;

  fprintf(stderr, "%d control points removed\n", nremoved) ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    MRIwrite(mri_dst_ctrl, "dc.mgz") ;
  HISTOfree(&histo) ;
  HISTOfree(&hsmooth) ;
  MRIfree(&mri_inside) ;
  return(mri_dst_ctrl) ;
}
#endif

static MRI *
add_interior_points(MRI *mri_src, MRI *mri_vals, float intensity_above,
                    float intensity_below, MRI_SURFACE *mris_white1, 
                    MRI_SURFACE *mris_white2,
                    MRI *mri_aseg, MRI *mri_dst)
{
  int   x, y, z, ctrl, label, i ;
  float val ;
  MRI   *mri_core ;
  MRI   *mri_interior, *mri_tmp ;

  mri_interior = MRIclone(mri_src, NULL) ;
  MRISfillInterior(mris_white1, mri_src->xsize, mri_interior) ;
  mri_tmp = MRIclone(mri_src, NULL) ;
  MRISfillInterior(mris_white2, mri_src->xsize, mri_tmp) ;
  MRIcopyLabel(mri_tmp, mri_interior, 1) ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    MRIwrite(mri_interior, "i.mgz") ;
  mri_core = MRIerode(mri_interior, NULL) ;
  for (i = 1 ; i < nint(1.0/mri_src->xsize) ; i++)
  {
    MRIcopy(mri_core, mri_tmp) ;
    MRIerode(mri_tmp, mri_core) ;
  }
  MRIfree(&mri_tmp) ;

  if (mri_aseg == NULL)
  {
    for (x = 0 ; x < mri_src->width ; x++)
      for (y = 0 ; y < mri_src->height ; y++)
        for (z = 0 ; z < mri_src->depth ; z++)
        {
          if (Gx == x && Gy == y && Gz == z)
            DiagBreak() ;
          ctrl = MRIgetVoxVal(mri_core, x, y, z, 0) ;
          if (ctrl > 0) 
            MRIsetVoxVal(mri_dst, x, y, z, 0, ctrl) ;
        }
  }
  else
  {
    for (x = 0 ; x < mri_src->width ; x++)
      for (y = 0 ; y < mri_src->height ; y++)
        for (z = 0 ; z < mri_src->depth ; z++)
        {
          if (Gx == x && Gy == y && Gz == z)
            DiagBreak() ;
          ctrl = MRIgetVoxVal(mri_src, x, y, z, 0) ;
          if (ctrl == 0)  // add in some missed ones that are inside the surface
          {
            label = MRIgetVoxVal(mri_aseg, x, y, z, 0) ;
            
            if (IS_GM(label) || IS_WM(label))
            {
              val = MRIgetVoxVal(mri_vals, x, y, z, 0) ;
              if ((val >= 110-intensity_below && val <= 110 + intensity_above)&&
                  MRIgetVoxVal(mri_core, x, y, z, 0) > 0)
                ctrl = 1 ;
            }
          }
          MRIsetVoxVal(mri_dst, x, y, z, 0, ctrl) ;
        }
  }

  MRIfree(&mri_core) ;
  return(mri_dst) ;
}

#define WSIZE_MM  10
static int
remove_surface_outliers(MRI *mri_ctrl_src, MRI *mri_dist, MRI *mri_src, 
                                   MRI *mri_ctrl_dst)
{
  int       x, y, z, wsize ;
  HISTOGRAM *h, *hs ;
  double    mean, sigma, val ;
  MRI       *mri_outlier = MRIclone(mri_ctrl_src, NULL) ;

  mri_ctrl_dst = MRIcopy(mri_ctrl_src, mri_ctrl_dst) ;
  wsize = nint(WSIZE_MM/mri_src->xsize) ;
  for (x = 0 ; x < mri_src->width ; x++)
    for (y = 0 ; y < mri_src->height ; y++)
      for (z = 0 ; z < mri_src->depth ; z++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        if ((int)MRIgetVoxVal(mri_ctrl_src, x, y,z, 0) == 0)
          continue ; // not a control point
        val = MRIgetVoxVal(mri_src, x, y, z, 0) ;
        if (val < 80 || val > 130)
        {
          MRIsetVoxVal(mri_ctrl_dst, x, y, z, 0, 0) ;  // remove it as a control point
          MRIsetVoxVal(mri_outlier, x, y, z, 0, 1) ;   // diagnostics
          continue ;
        }
        if (val > 100 || val < 120)
          continue ;   // not an outlier
        h = MRIhistogramVoxel(mri_src, 0, NULL, x, y, z, wsize, mri_dist, mri_src->xsize) ;
        HISTOsoapBubbleZeros(h, h, 100) ;
        hs = HISTOsmooth(h, NULL, .5);
        HISTOrobustGaussianFit(hs, .5, &mean, &sigma) ;
#define MAX_SIGMA 10   // for intensity normalized images
        if (sigma > MAX_SIGMA)
          sigma = MAX_SIGMA ;
        if (fabs((mean-val)/sigma) > 2)
        {
          MRIsetVoxVal(mri_ctrl_dst, x, y, z, 0, 0) ;  // remove it as a control point
          MRIsetVoxVal(mri_outlier, x, y, z, 0, 1) ;   // diagnostics
        }

        if (Gdiag & DIAG_WRITE)
        {
          HISTOplot(h, "h.plt") ;
          HISTOplot(h, "hs.plt") ;
        }
        HISTOfree(&h) ; HISTOfree(&hs) ;
      }
  if (Gdiag & DIAG_WRITE)
    MRIwrite(mri_outlier, "o.mgz") ;
  MRIfree(&mri_outlier) ;
  return(NO_ERROR) ;
}
