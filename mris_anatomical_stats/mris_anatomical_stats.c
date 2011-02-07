/**
 * @file  mris_anatomical_stats.c
 * @brief measures a variety of anatomical properties
 *
 */
/*
 * Original Author: Bruce Fischl and Doug Greve
 * CVS Revision Info:
 *    $Author: greve $
 *    $Date: 2011/02/07 15:26:25 $
 *    $Revision: 1.68.2.1 $
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <sys/utsname.h>

#include "macros.h"
#include "error.h"
#include "diag.h"
#include "proto.h"
#include "mrisurf.h"
#include "mri.h"
#include "macros.h"
#include "fio.h"
#include "version.h"
#include "colortab.h"

static char vcid[] =
  "$Id: mris_anatomical_stats.c,v 1.68.2.1 2011/02/07 15:26:25 greve Exp $";

int main(int argc, char *argv[]) ;
static int  get_option(int argc, char *argv[]) ;
static void usage_exit(void) ;
static void print_usage(void) ;
static void print_help(void) ;
static void print_version(void) ;
double MRISmeasureTotalWhiteMatterVolume(MRI *mri) ;
double MRISmeasureCorticalGrayMatterVolume(MRI_SURFACE *mris) ;
int MRIScomputeCurvatureIndicesMarked(MRI_SURFACE *mris, double *pici,
                                      double *pfi, int mark) ;
int MRIScomputeCurvatureStats(MRI_SURFACE *mris, double *pavg, double *pvar,
                              float ignore_below, float ignore_above) ;
double MRIScomputeAbsoluteCurvature(MRI_SURFACE *mris) ;
double MRIScomputeAbsoluteCurvatureMarked(MRI_SURFACE *mris, int mark) ;
int    MRISrestoreSurface(MRI_SURFACE *mris) ;
int    MRIScountVertices(MRI_SURFACE *mris);
#if 0
int    MRISreadAnnotFile(MRI_SURFACE *mris, char *fname) ;
int    MRISripVerticesWithMark(MRI_SURFACE *mris, int mark) ;
int    MRISripVerticesWithoutMark(MRI_SURFACE *mris, int mark) ;
int    MRISreplaceMarks(MRI_SURFACE *mris, int in_mark, int out_mark) ;
#endif
int    MRISripVerticesWithAnnotation(MRI_SURFACE *mris, int annotation) ;
int    MRISripVerticesWithoutAnnotation(MRI_SURFACE *mris, int annotation) ;
int    MRISreplaceAnnotations(MRI_SURFACE *mris,
                              int in_annotation,
                              int out_annotation) ;
char *Progname ;
static double sigma = 0.0f ;
static float ignore_below = 0 ;
static float ignore_above = 20 ;
static char *label_name = NULL ;
static char *annotation_name = NULL ;
static char *thickness_name = "thickness" ;
static int histo_flag = 0 ;
static char *gray_histo_name ;
static char *mri_name = "T1" ;
static int noheader = 0 ;
static char *log_file_name = NULL ;
static int tabular_output_flag = 0;
static char sdir[STRLEN] = "" ;
static int MGZ = 1; // for use with MGZ format
static char *tablefile=NULL;
static char *annotctabfile=NULL; // for outputing the color table
static FILE *fp=NULL;
static int nsmooth = 0;
static char *white_name = "white" ;
static char *pial_name = "pial" ;
static LABEL *cortex_label = NULL ; // limit surface area calc to cortex.label
static int crosscheck = 0;

#define MAX_INDICES 50000
static char *names[MAX_INDICES];

int
main(int argc, char *argv[])
{
  char          **av, *hemi, *sname, *cp, fname[STRLEN], *surf_name ;
  int           ac, nargs, vno ;
  MRI_SURFACE   *mris ;
  MRI           *mri_wm, *mri_kernel = NULL, *mri_orig ;
  double        gray_volume, wm_volume;
  double        mean_abs_mean_curvature, mean_abs_gaussian_curvature;
  double        intrinsic_curvature_index, folding_index ;
  FILE          *log_fp = NULL ;
  VERTEX        *v ;
  HISTOGRAM     *histo_gray ;
  MRI           *ThicknessMap = NULL;
  struct utsname uts;
  char          *cmdline, full_name[STRLEN] ;
  int           num_cortex_vertices = 0;
  float         total_cortex_area = 0;
  float         mean_cortex_thickness = 0;

  /* rkt: check for and handle version tag */
  nargs = handle_version_option
    (argc, argv,
     "$Id: mris_anatomical_stats.c,v 1.68.2.1 2011/02/07 15:26:25 greve Exp $",
     "$Name:  $");
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  cmdline = argv2cmdline(argc,argv);
  uname(&uts);

  mean_abs_mean_curvature = mean_abs_gaussian_curvature = gray_volume = 0.0 ;
  Progname = argv[0] ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  ac = argc ;
  av = argv ;
  for ( ; argc > 1 && ISOPTION(*argv[1]) ; argc--, argv++)
  {
    nargs = get_option(argc, argv) ;
    argc -= nargs ;
    argv += nargs ;
  }

  if (argc < 3)
    usage_exit() ;

  if (argc > 4)
  {
    fprintf(stderr,"\nFlagged-options must go first in argument list.\n\n");
    usage_exit() ;
  }

  memset(names, 0, sizeof(names)) ;
  sname = argv[1] ;
  if (strlen(sdir) == 0)
  {
    cp = getenv("SUBJECTS_DIR") ;
    if (!cp)
      ErrorExit(ERROR_BADPARM,
                "%s: SUBJECTS_DIR not defined in environment.\n", Progname) ;
    strcpy(sdir, cp) ;
  }

  hemi = argv[2] ;
  if (argc > 3)
    surf_name = argv[3] ;
  else
    surf_name = WHITE_MATTER_NAME ;

  if (sigma > 0.0)
    mri_kernel = MRIgaussian1d(sigma, 100) ;
  sprintf(fname, "%s/%s/mri/wm", sdir, sname) ;
  if (MGZ) strcat(fname, ".mgz");
  fprintf(stderr, "reading volume %s...\n", fname) ;
  mri_wm = MRIread(fname) ;
  if (!mri_wm)
    ErrorExit(ERROR_NOFILE, "%s: could not read input volume %s",
              Progname, fname) ;

  if (mri_kernel)
  {
    fprintf(stderr, "smoothing brain volume with sigma = %2.3f\n", sigma) ;
    MRIconvolveGaussian(mri_wm, mri_wm, mri_kernel) ;
#if 0
    fprintf(stderr, "smoothing wm volume with sigma = %2.3f\n", sigma) ;
    MRIconvolveGaussian(mri_wm, mri_wm, mri_kernel) ;
    if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
      MRIwrite(mri_wm, "/tmp/wm_smooth.mnc") ;
#endif
    MRIfree(&mri_kernel);
  }

  sprintf(fname, "%s/%s/surf/%s.%s", sdir, sname, hemi, surf_name) ;
  fprintf(stderr, "reading input surface %s...\n", fname) ;
  mris = MRISread(fname) ;
  if (!mris)
    ErrorExit(ERROR_NOFILE, "%s: could not read surface file %s",
              Progname, fname) ;

  if (mris->group_avg_vtxarea_loaded)
  {
    printf("\n");
    printf("ERROR: subject %s is an average subject. mris_anatomical_stats\n",
           sname);
    printf("cannot currently be used with an average subject. \n");
    printf("\n");
    exit(1);
  }

  MRISsaveVertexPositions(mris, ORIGINAL_VERTICES) ;
  // read in white and pial surfaces
  MRISsaveVertexPositions(mris, TMP_VERTICES) ;
  sprintf(fname, "%s/%s/surf/%s.%s", sdir, sname, hemi, pial_name) ;
  fprintf(stderr, "reading input pial surface %s...\n", fname) ;
  if (MRISreadVertexPositions(mris, fname) != NO_ERROR)
    ErrorExit(ERROR_NOFILE, "%s: could not read surface file %s",
              Progname, fname) ;
  MRISsaveVertexPositions(mris, PIAL_VERTICES) ;
  sprintf(fname, "%s/%s/surf/%s.%s", sdir, sname, hemi, white_name) ;
  fprintf(stderr, "reading input white surface %s...\n", fname) ;
  if (MRISreadVertexPositions(mris, fname) != NO_ERROR)
    ErrorExit(ERROR_NOFILE, "%s: could not read surface file %s",
              Progname, fname) ;
  MRISsaveVertexPositions(mris, WHITE_VERTICES) ;
  MRISrestoreVertexPositions(mris, TMP_VERTICES) ;

  MRIScomputeMetricProperties(mris) ;
  wm_volume = MRISmeasureTotalWhiteMatterVolume(mri_wm) ;
#if 0
  fprintf(stderr, "measuring gray matter thickness of surface...\n") ;
  sprintf(fname, "%s/%s/surf/%s.%s", sdir, sname, hemi, GRAY_MATTER_NAME) ;
  fprintf(stderr, "reading pial surface position from %s...\n", fname) ;
  MRISreadVertexPositions(mris, fname) ;
#else
  MRISreadCurvatureFile(mris, thickness_name) ;

  if (nsmooth > 0)
  {
    printf("Smooth thickness map with %d iterations on surface\n", nsmooth);
    ThicknessMap = MRIcopyMRIS(NULL, mris, 0, "curv");
    if (ThicknessMap == NULL)
    {
      printf("Unable to copy thickness data to a MRI volume \n");
    }
    else
    {
      MRISsmoothMRI(mris, ThicknessMap, nsmooth, NULL,ThicknessMap);
      MRIScopyMRI(mris, ThicknessMap, 0, "curv");
      MRIfree(&ThicknessMap);
    }
  }

#endif

#if 0
  fprintf(stderr, "measuring cortical thickness...") ;
  MRISmeasureCorticalThickness(mris, mri_wm) ;
#endif
  MRIScopyCurvatureToImagValues(mris) ;   /* save thickness measures */

  MRISsetNeighborhoodSize(mris, 2) ;
  MRIScomputeSecondFundamentalForm(mris) ;

  if (annotation_name)
  {
    if (MRISreadAnnotation(mris, annotation_name) != NO_ERROR)
      ErrorExit(ERROR_NOFILE, "%s:  could  not read annotation file %s",
                Progname, annotation_name) ;
    if (annotctabfile != NULL && mris->ct != NULL)
    {
      printf("Saving annotation colortable %s\n",annotctabfile);
      CTABwriteFileASCII(mris->ct,annotctabfile);
    }
  }

  if (histo_flag)
  {
    sprintf(fname, "%s/%s/mri/%s", sdir, sname, mri_name) ;
    if (MGZ) strcat(fname, ".mgz");
    fprintf(stderr, "reading volume %s...\n", fname) ;
    mri_orig = MRIread(fname) ;
    if (!mri_orig)
      ErrorExit(ERROR_NOFILE, "%s: could not read input volume %s",
                Progname, fname) ;
    histo_gray = HISTOalloc(256) ;
  }
  else
  {
    histo_gray = NULL ;
    mri_orig = NULL ;
  }

  if (log_file_name)
  {
    log_fp = fopen(log_file_name, "a") ;
    if (!log_fp)
      ErrorExit(ERROR_NOFILE, "%s: could not open log file %s", Progname,
                log_file_name) ;
  }

#define SHOW_WHITE_MATTER_VOLUME 0
#if SHOW_WHITE_MATTER_VOLUME
  fprintf(stdout, "total white matter volume               = %2.0f mm^3\n",
          wm_volume) ;
#endif

  /*
    set the v->marked field to tell whether the vertex should be
    included in one of the summary stats.
  */
  if (label_name)
  {
    LABEL  *area ;
    char   fname[STRLEN] ;

    sprintf(fname, "%s/%s/label/%s", sdir, sname, label_name) ;
    // If that does not exist, use label_name as absolute path.
    if (! fio_FileExistsReadable(fname))  sprintf(fname, "%s", label_name) ;

    area = LabelRead(NULL, fname) ;
    if (!area)
      ErrorExit(ERROR_NOFILE, "%s: could not read label file %s\n", sname) ;
    MRISsetMarks(mris, -1) ;
    LabelMark(area, mris) ;  // set the vertices in the label to marked=1
    LabelFree(&area) ;
    MRIScomputeMetricProperties(mris) ;
    names[1] = label_name ;
    names[0] = NULL ;
    if ((label_name[0] == '/') || !strncmp(label_name, "./", 2))// a full path
      strcpy(full_name, label_name) ;
    else
    {
      // build the full path string
      if (strstr(label_name, ".label") == NULL)
        sprintf(full_name, "%s/%s/label/%s.label", sdir, sname, label_name) ;
      else
        sprintf(full_name, "%s/%s/label/%s", sdir, sname, label_name) ;
    }
    if (! fio_FileExistsReadable(full_name))
      sprintf(full_name, "%s", label_name) ;
  }
  else if (annotation_name)
  {
    int vno, index ;
    VERTEX *v ;

    if ((annotation_name[0] == '/') ||
        !strncmp(annotation_name, "./", 2))// a full path
      strcpy(full_name, annotation_name) ;
    else  // build the full path string
    {
      char tmp[STRLEN] ;
      sprintf(tmp, "%s.", hemi) ;
      if (strstr(annotation_name, tmp) == NULL) // no hemi in string
      {
        if (strstr(annotation_name, ".annot") == NULL)
          sprintf(full_name, "%s/%s/label/%s.%s.annot", sdir, sname,
                  hemi, annotation_name) ;
        else
          sprintf(full_name, "%s/%s/label/%s.%s", sdir, sname,
                  hemi,annotation_name) ;
      }
      else
      {
        if (strstr(annotation_name, ".annot") == NULL)
          sprintf(full_name, "%s/%s/label/%s.annot", sdir, sname,
                  annotation_name) ;
        else
          sprintf(full_name, "%s/%s/label/%s", sdir, sname,
                  annotation_name) ;
      }

    }
    for (vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v = &mris->vertices[vno] ;
      CTABfindAnnotation(mris->ct, v->annotation, &index);
      v->marked = index ;
      if (index >= MAX_INDICES)
      {
        printf("ERROR: index = %d > MAX_INDICES = %d\n",index,MAX_INDICES);
        exit(1);
      }
      else if (index < 0) continue;
      names[index] = mris->ct->entries[index]->name ;
      //printf("idx=%d, name=%s\n",index,names[index]);
    }
  }
  else // do all of surface
  {
    MRISclearMarks(mris) ;
    names[0] = mris->fname ;
  }

  if (tabular_output_flag)
  {
    fprintf(stdout, "\n");
    fprintf(stdout, "table columns are:\n");
    fprintf(stdout, "    number of vertices\n");
    fprintf(stdout, "    total surface area (mm^2)\n");
    fprintf(stdout, "    total gray matter volume (mm^3)\n");
    fprintf(stdout, "    average cortical thickness "
            "+- standard deviation (mm)\n");
    fprintf(stdout, "    integrated rectified mean curvature\n");
    fprintf(stdout, "    integrated rectified Gaussian curvature\n");
    fprintf(stdout, "    folding index\n");
    fprintf(stdout, "    intrinsic curvature index\n");
    fprintf(stdout, "    structure name\n");
    fprintf(stdout, "\n");
  }

  if (tablefile != NULL)
  {
    fp = fopen(tablefile,"w");
    if (fp == NULL)
      ErrorExit(ERROR_NOFILE, "%s: couldn't open file %s",Progname,tablefile);
    fprintf(fp,"# Table of FreeSurfer cortical "
            "parcellation anatomical statistics \n");
    fprintf(fp,"# \n");
    fprintf(fp,"# CreationTime %s\n",VERcurTimeStamp());
    fprintf(fp,"# generating_program %s\n",Progname);
    fprintf(fp,"# cvs_version %s\n",vcid);
    fprintf(fp,"# mrisurf.c-cvs_version %s\n",MRISurfSrcVersion());
    fprintf(fp,"# cmdline %s\n",cmdline);
    fprintf(fp,"# sysname  %s\n",uts.sysname);
    fprintf(fp,"# hostname %s\n",uts.nodename);
    fprintf(fp,"# machine  %s\n",uts.machine);
    fprintf(fp,"# user     %s\n",VERuser());
    fprintf(fp,"# \n");
    fprintf(fp,"# SUBJECTS_DIR %s\n",sdir);
    fprintf(fp,"# anatomy_type surface\n");
    fprintf(fp,"# subjectname %s\n",sname);
    fprintf(fp,"# hemi %s\n",hemi);
    fprintf(fp,"# AnnotationFile %s\n",
            annotation_name ? annotation_name : label_name ? label_name :
            mris->fname);
    fprintf(fp,"# AnnotationFileTimeStamp %s\n",
            (annotation_name || label_name) ?
            VERfileTimeStamp(full_name) :
            VERfileTimeStamp(mris->fname));
#if SHOW_WHITE_MATTER_VOLUME
    fprintf(fp,"# TotalWhiteMatterVolume  %2.0f mm^3\n",wm_volume) ;
#endif

    num_cortex_vertices = mris->nvertices;
    total_cortex_area = mris->total_area;
    // if -cortex option selected, then count vertices and area only in cortex
    if (cortex_label)
    {
      /* calculate "area" and thickness of the vertices labeled as cortex */
      num_cortex_vertices = 0;
      total_cortex_area = 0;
      mean_cortex_thickness = 0;
      int vno;
      for (vno = 0 ; vno < mris->nvertices ; vno++)
      {
        VERTEX *v = &mris->vertices[vno] ;
        if (v->ripflag) continue ;

        // skip vertices that dont have an annotation so that the sum of
        // annotated ROIs equals this cortex label sum
        if (annotation_name && v->annotation <= 0) continue;

        int lno;
        for (lno = 0 ; lno < cortex_label->n_points ; lno++)
        {
          if (cortex_label->lv[lno].vno == vno)
          {
            float area = 0.0 ;
            int fno;
            for (fno = 0 ; fno < v->num ; fno++)
            {
              FACE *face = &mris->faces[v->f[fno]];
              if (face->ripflag) continue;
              area += face->area/VERTICES_PER_FACE;
            }
            total_cortex_area += area;
        
            num_cortex_vertices++;

            // thickness measures were saved to v->imag_val earlier by the
            // call to MRIScopyCurvatureToImagValues(mris)
            mean_cortex_thickness += v->imag_val;
            
            break;
          }
        }
      }
      if (mean_cortex_thickness && num_cortex_vertices )
      {
        mean_cortex_thickness /= num_cortex_vertices;
      }
    }
    fprintf(fp,
            "# Measure Cortex, NumVert, Number of Vertices, %d, unitless\n",
            num_cortex_vertices);
    fprintf(fp,
            "# Measure Cortex, WhiteSurfArea, White Surface Total Area, %g, mm^2\n",
            total_cortex_area);
    if (cortex_label)
    {
      fprintf(fp,
            "# Measure Cortex, MeanThickness, Mean Thickness, %g, mm\n",
              mean_cortex_thickness);
    }

    fprintf(fp,"# NTableCols 10\n");

    fprintf(fp,"# TableCol  1 ColHeader StructName\n");
    fprintf(fp,"# TableCol  1 FieldName Structure Name\n");
    fprintf(fp,"# TableCol  1 Units     NA\n");

    fprintf(fp,"# TableCol  2 ColHeader NumVert\n");
    fprintf(fp,"# TableCol  2 FieldName Number of Vertices\n");
    fprintf(fp,"# TableCol  2 Units     unitless\n");

    fprintf(fp,"# TableCol  3 ColHeader SurfArea\n");
    fprintf(fp,"# TableCol  3 FieldName Surface Area\n");
    fprintf(fp,"# TableCol  3 Units     mm^2\n");

    fprintf(fp,"# TableCol  4 ColHeader GrayVol\n");
    fprintf(fp,"# TableCol  4 FieldName Gray Matter Volume\n");
    fprintf(fp,"# TableCol  4 Units     mm^3\n");

    fprintf(fp,"# TableCol  5 ColHeader ThickAvg \n");
    fprintf(fp,"# TableCol  5 FieldName Average Thickness\n");
    fprintf(fp,"# TableCol  5 Units     mm\n");

    fprintf(fp,"# TableCol  6 ColHeader ThickStd\n");
    fprintf(fp,"# TableCol  6 FieldName Thickness StdDev\n");
    fprintf(fp,"# TableCol  6 Units     mm \n");

    fprintf(fp,"# TableCol  7 ColHeader MeanCurv\n");
    fprintf(fp,"# TableCol  7 FieldName Integrated Rectified"
            " Mean Curvature\n");
    fprintf(fp,"# TableCol  7 Units     mm^-1\n");

    fprintf(fp,"# TableCol  8 ColHeader GausCurv \n");
    fprintf(fp,"# TableCol  8 FieldName Integrated Rectified"
            " Gaussian Curvature\n");
    fprintf(fp,"# TableCol  8 Units     mm^-2\n");

    fprintf(fp,"# TableCol  9 ColHeader  FoldInd\n");
    fprintf(fp,"# TableCol  9 FieldName  Folding Index \n");
    fprintf(fp,"# TableCol  9 Units      unitless \n");

    fprintf(fp,"# TableCol 10 ColHeader CurvInd\n");
    fprintf(fp,"# TableCol 10 FieldName Intrinsic Curvature Index\n");
    fprintf(fp,"# TableCol 10 Units     unitless\n");

    fprintf(fp,"# ColHeaders StructName NumVert SurfArea GrayVol "
            "ThickAvg ThickStd MeanCurv GausCurv FoldInd CurvInd\n");
    fclose(fp);
  }

  {
    double  areas[MAX_INDICES],
    volumes[MAX_INDICES], thicknesses[MAX_INDICES],
    avg_thick, volume, thickness_vars[MAX_INDICES], std ;
    int     v0_index, v1_index, v2_index, fno, m, i, dofs[MAX_INDICES] ;
    VERTEX  *v0, *v1, *v2 ;
    FACE    *f ;

    memset(areas, 0, sizeof(areas)) ;
    memset(volumes, 0, sizeof(volumes)) ;
    memset(thicknesses, 0, sizeof(thicknesses)) ;
    memset(dofs, 0, sizeof(dofs)) ;
    memset(thickness_vars, 0, sizeof(thickness_vars)) ;
    v0_index = v1_index = v2_index = 0 ;

    MRIScomputeMetricProperties(mris) ;

    // first do white surface
    MRISsaveVertexPositions(mris, TMP_VERTICES) ;
    MRISrestoreVertexPositions(mris, WHITE_VERTICES) ;
    MRIScomputeMetricProperties(mris) ;
    for (fno = 0 ; fno < mris->nfaces ; fno++)
    {
      f = &mris->faces[fno] ;
      if (f->ripflag)
        continue ;
      v0 = &mris->vertices[f->v[0]] ;
      v1 = &mris->vertices[f->v[1]] ;
      v2 = &mris->vertices[f->v[2]] ;
      v0_index = v0->marked ;
      v1_index = v1->marked ;
      v2_index = v2->marked ;

      for (avg_thick = 0.0, m = 0 ; m < VERTICES_PER_FACE ; m++)
      {
        vno = f->v[m] ;
        v = &mris->vertices[vno] ;
        avg_thick += v->imag_val ;
      }
      avg_thick /= VERTICES_PER_FACE ;
      volume = (avg_thick * f->area) ;
      if (v0_index >= 0)
        volumes[v0_index] += volume/VERTICES_PER_FACE ;
      if (v1_index >= 0)
        volumes[v1_index] += volume/VERTICES_PER_FACE ;
      if (v2_index >= 0)
        volumes[v2_index] += volume/VERTICES_PER_FACE ;
    }

    // now do pial surface
    MRISrestoreVertexPositions(mris, PIAL_VERTICES) ;
    MRIScomputeMetricProperties(mris) ;
    for (fno = 0 ; fno < mris->nfaces ; fno++)
    {
      f = &mris->faces[fno] ;
      if (f->ripflag)
        continue ;
      v0 = &mris->vertices[f->v[0]] ;
      v1 = &mris->vertices[f->v[1]] ;
      v2 = &mris->vertices[f->v[2]] ;

      for (avg_thick = 0.0, m = 0 ; m < VERTICES_PER_FACE ; m++)
      {
        vno = f->v[m] ;
        v = &mris->vertices[vno] ;
        avg_thick += v->imag_val ;
      }
      avg_thick /= VERTICES_PER_FACE ;
      volume = (avg_thick * f->area) ;
      if (v0->marked >= 0)
        volumes[v0->marked] += volume/VERTICES_PER_FACE ;
      if (v1->marked >= 0)
        volumes[v1->marked] += volume/VERTICES_PER_FACE ;
      if (v2->marked >= 0)
        volumes[v2->marked] += volume/VERTICES_PER_FACE ;
    }

    // area should just be surface area as
    // specified surface (in ORIG_VERTICES)
    MRISrestoreVertexPositions(mris, ORIGINAL_VERTICES) ;
    MRIScomputeMetricProperties(mris) ;
    for (fno = 0 ; fno < mris->nfaces ; fno++)
    {
      f = &mris->faces[fno] ;
      if (f->ripflag)
        continue ;
      v0 = &mris->vertices[f->v[0]] ;
      v1 = &mris->vertices[f->v[1]] ;
      v2 = &mris->vertices[f->v[2]] ;

      if (v0->marked >= 0)
        areas[v0->marked] += f->area/VERTICES_PER_FACE ;
      if (v1->marked >= 0)
        areas[v1->marked] += f->area/VERTICES_PER_FACE ;
      if (v2->marked >= 0)
        areas[v2->marked] += f->area/VERTICES_PER_FACE ;
    }

    // compute thickness for each annotation
    for (vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v0 = &mris->vertices[vno] ;
      if (v0->marked < 0)
        continue ;
      thicknesses[v0->marked] += v0->imag_val ;
      dofs[v0->marked]++ ;
    }

    for (i = 0 ; i < MAX_INDICES ; i++)
    {
      if (dofs[i] == 0)
        continue ;
      thicknesses[i] /= dofs[i] ;
    }

    // compute thickness variance for each annotation
    for (vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v0 = &mris->vertices[vno] ;
      if (v0->marked < 0)
        continue ;
      std = (v0->imag_val-thicknesses[v0->marked]);
      thickness_vars[v0->marked] += std*std ;
    }

    int dofs_total = 0;
    float areas_total = 0.0f;

    for (i = 0 ; i < MAX_INDICES ; i++)
    {
      if (dofs[i] == 0 || names[i] == NULL)
        continue ;

      // don't bother printing Corpus Callosum stats: its not cortex
      if (0 == strcmp(names[i],"corpuscallosum")) // desikan label
        continue;
      // ditto for medial wall area: its not cortex
      if (0 == strcmp(names[i],"unknown")) // desikan label
        continue;
      if (0 == strcmp(names[i],"Medial_wall")) // christophe label
        continue;
      if (0 == strcmp(names[i],"Unknown")) // christophe label
        continue;

      dofs_total += dofs[i];
      areas_total += areas[i];

      MRISuseMeanCurvature(mris) ;
      mean_abs_mean_curvature = MRIScomputeAbsoluteCurvatureMarked(mris,i) ;

      MRISuseGaussianCurvature(mris) ;
      mean_abs_gaussian_curvature = MRIScomputeAbsoluteCurvatureMarked(mris,i);
      MRIScomputeCurvatureIndicesMarked(mris,
                                        &intrinsic_curvature_index,
                                        &folding_index,i) ;

      volumes[i] /= 2 ;
      thickness_vars[i] /= dofs[i] ;

      /* output */

      if (tablefile != NULL)
      {
        fp = fopen(tablefile,"a");
        fprintf(fp, "%-40s", fio_basename(names[i],NULL)) ;
        fprintf(fp, "%5d", dofs[i]);
        fprintf(fp, "  %5.0f", areas[i]) ;
        fprintf(fp, "  %5.0f", volumes[i]) ;
        fprintf(fp, "  %5.3f %5.3f",
                thicknesses[i], sqrt(thickness_vars[i])) ;
        fprintf(fp, "  %8.3f", mean_abs_mean_curvature) ;
        fprintf(fp, "  %8.3f", mean_abs_gaussian_curvature) ;
        fprintf(fp, "  %7.0f", folding_index); // deliberate precision of .0f
        fprintf(fp, "  %6.1f",intrinsic_curvature_index); // precision of .1f
        fprintf(fp, "\n");
        fclose(fp);
      }

      if (tabular_output_flag)
      {
        fprintf(stdout, "%5d", dofs[i]);
        fprintf(stdout, "  %5.0f", areas[i]) ;
        fprintf(stdout, "  %5.0f", volumes[i]) ;
        fprintf(stdout, "  %5.3f %5.3f",
                thicknesses[i], sqrt(thickness_vars[i])) ;
        fprintf(stdout, "  %8.3f", mean_abs_mean_curvature) ;
        fprintf(stdout, "  %8.3f", mean_abs_gaussian_curvature) ;
        fprintf(stdout, "  %7.0f", folding_index);
        fprintf(stdout, "  %6.1f",intrinsic_curvature_index);
        fprintf(stdout, "  %s", names[i]) ;
        fprintf(stdout, "\n");
      }
      else
      {
        if (annotation_name && mris->ct == NULL)
          ErrorExit
          (ERROR_BADFILE,
           "%s: no color table loaded - cannot translate annot  file",
           Progname);

        fprintf(stdout,
                "structure is \"%s\"\n", names[i]) ;
        fprintf(stdout,
                "number of vertices                      = %d\n",
                dofs[i]);
        fprintf(stdout,
                "total surface area                      = %2.0f mm^2\n",
                areas[i]) ;
        fprintf(stdout,
                "total gray matter volume                = %2.0f mm^3\n",
                volumes[i]) ;
        fprintf(stdout,
                "average cortical thickness              = %2.3f mm "
                "+- %2.3f mm\n",
                thicknesses[i], sqrt(thickness_vars[i])) ;
        fprintf(stdout,
                "average integrated rectified mean curvature     = "
                "%2.3f\n",
                mean_abs_mean_curvature) ;
        fprintf(stdout,
                "average integrated rectified Gaussian curvature = "
                "%2.3f\n",
                mean_abs_gaussian_curvature) ;
        fprintf(stdout,
                "folding index                           = %2.0f\n",
                folding_index);
        fprintf(stdout,
                "intrinsic curvature index               = %2.1f\n",
                intrinsic_curvature_index);
      }

      if (log_fp)
      {
#if SHOW_WHITE_MATTER_VOLUME
        if (!noheader)
          fprintf(log_fp, "%% %s: <wm vol> <surf area> <gray vol> "
                  "<thick mean> <thick var> <integ rect. mean curv> "
                  "<integ rect. Gauss curv> <fold index> <intr curv ind>\n",
                  sname) ;
        fprintf
        (log_fp,
         "%2.0f\t%2.0f\t%2.0f\t%2.3f\t%2.3f\t%2.3f\t%2.3f\t%2.0f\t%2.1f\n",
         wm_volume,
         areas[i],
         volumes[i],
         thicknesses[i],
         sqrt(thickness_vars[i]),
         mean_abs_mean_curvature,
         mean_abs_gaussian_curvature,
         folding_index,
         intrinsic_curvature_index) ;
#else
        if (!noheader)
          fprintf(log_fp, "%% %s: <surf area> <gray vol> "
                  "<thick mean> <thick var> <integ rect. mean curv> "
                  "<integ rect. Gauss curv> <fold index> <intr curv ind>\n",
                  sname) ;
        fprintf
        (log_fp,
         "%2.0f\t%2.0f\t%2.3f\t%2.3f\t%2.3f\t%2.3f\t%2.0f\t%2.1f\n",
         areas[i],
         volumes[i],
         thicknesses[i],
         sqrt(thickness_vars[i]),
         mean_abs_mean_curvature,
         mean_abs_gaussian_curvature,
         folding_index,
         intrinsic_curvature_index) ;
#endif
        fclose(log_fp) ;
      }
    }

    MRISrestoreVertexPositions(mris, TMP_VERTICES) ;

    // check of the sum of vertices equals what was calculated for the
    // whole cortex, ditto for surface area
    if (crosscheck)
    {
      if (dofs_total != num_cortex_vertices)
      {
        printf("ERROR: cross-check failed! dofs_total=%d != "
               "num_cortex_vertices=%d\n",dofs_total,num_cortex_vertices);
        exit(1);
      }
      int areas_total_int = (int)areas_total;
      if (areas_total_int != (int)total_cortex_area)
      {
        if ((areas_total_int+1) != (int)total_cortex_area)
        {
          if ((areas_total_int-1) != (int)total_cortex_area)
          {
            printf("ERROR: cross-check failed! areas_total=%d != "
                   "total_cortex_area=%d\n",
                   areas_total_int,(int)total_cortex_area);
            exit(1);
          }
        }
      }
      printf("INFO: cross-check passed\n");
    }
  }

  if (histo_flag)
  {
    fprintf(stderr, "plotting gray matter histogram to file %s...\n",
            gray_histo_name) ;
    HISTOplot(histo_gray, gray_histo_name) ;
    MRIfree(&mri_orig) ;
    HISTOfree(&histo_gray) ;
  }

  if (mri_wm) MRIfree(&mri_wm);
  if (mri_kernel) MRIfree(&mri_kernel);
  if (mri_orig) MRIfree(&mri_orig);
  if (ThicknessMap) MRIfree(&ThicknessMap);

  exit(0) ;
  return(0) ;  /* for ansi */
}


/*----------------------------------------------------------------------
  Parameters:

  Description:
  ----------------------------------------------------------------------*/
static int
get_option(int argc, char *argv[])
{
  int  nargs = 0 ;
  char *option ;

  option = argv[1] + 1 ;            /* past '-' */
  if (!stricmp(option, "-help"))
    print_help() ;
  else if (!stricmp(option, "-version"))
    print_version() ;
  else if (!stricmp(option, "log"))
  {
    log_file_name = argv[2] ;
    nargs = 1 ;
    fprintf(stderr, "outputting results to %s...\n", log_file_name) ;
  }
  else if (!stricmp(option, "nsmooth"))
  {
    nsmooth = atoi(argv[2]);
    nargs = 1;
    printf("Smooth thickness by %d steps before using it \n", nsmooth);
  }
  else if (!stricmp(option, "noheader"))
  {
    noheader = 1 ;
    printf("suppressing printing of headers to log file\n") ;
  }
  else if (!stricmp(option, "white"))
  {
    white_name = argv[2] ;
    nargs = 1 ;
    printf("using %s as white matter surface name\n", white_name) ;
  }
  else if (!stricmp(option, "pial"))
  {
    pial_name = argv[2] ;
    nargs = 1 ;
    printf("using %s as pial matter surface name\n", pial_name) ;
  }
  else if (!stricmp(option, "sdir"))
  {
    char str[STRLEN] ;
    strcpy(sdir, argv[2]) ;
    printf("using  %s as  SUBJECTS_DIR...\n", sdir)  ;
    nargs = 1 ;
    sprintf(str, "SUBJECTS_DIR=%s", sdir) ;
    putenv(str) ;
  }
  else if (!stricmp(option, "mgz"))
  {
    MGZ = 1;
    printf("INFO: assuming MGZ format for volumes.\n");
  }
  else if (!stricmp(option, "COR"))
  {
    MGZ = 0;
    printf("INFO: assuming COR format for volumes.\n");
  }
  else if (!stricmp(option, "cortex"))
  {
    cortex_label = LabelRead(NULL, argv[2]) ;
    if (cortex_label == NULL)
      ErrorExit(ERROR_NOFILE, "") ;
    nargs = 1 ;
    printf("INFO: using %s as mask to calc cortex "
           "NumVert, SurfArea and MeanThickness.\n",
      argv[2]);
  }
  else if (!stricmp(option, "crosscheck"))
  {
    crosscheck = 1;
    printf("INFO: will cross-check cortex NumVert and SurfArea with "
           "sum of all annotation structures.\n");
  }
  else switch (toupper(*option))
    {
    case 'T':
      thickness_name = argv[2] ;
      nargs = 1 ;
      fprintf(stderr, "using thickness file %s.\n", thickness_name) ;
      break ;
    case 'L':
      label_name = argv[2] ;
      nargs = 1 ;
      fprintf(stderr, "limiting computations to label %s.\n", label_name) ;
      break ;
    case 'M':
      mri_name = argv[2] ;
      nargs = 1 ;
      fprintf(stderr, "computing histograms on intensity values from %s...\n",
              mri_name) ;
      break ;
    case 'H':
      histo_flag = 1 ;
      gray_histo_name = argv[2] ;
      nargs = 1 ;
      fprintf
      (stderr,
       "writing histograms of intensity distributions to %s...\n",
       gray_histo_name);
      break ;
    case 'V':
      Gdiag_no = atoi(argv[2]) ;
      nargs = 1 ;
      break ;
    case 'A':
      annotation_name = argv[2] ;
      nargs = 1 ;
      fprintf(stderr, "computing statistics for each annotation in %s.\n",
              annotation_name) ;
      break ;
    case 'C':
      annotctabfile = argv[2] ;
      nargs = 1 ;
      break ;
    case 'I':
      ignore_below = atof(argv[2]) ;
      ignore_above = atof(argv[3]) ;
      fprintf(stderr,
              "only considering thicknesses in the range [%2.1f,%2.1f].\n",
              ignore_below, ignore_above) ;
      nargs = 2 ;
      break ;
    case 'B':
      tabular_output_flag = 1;
      nargs = 0;
      break;
    case 'F':
      tablefile = argv[2] ;
      nargs = 1 ;
      break ;
    case '?':
    case 'U':
      print_usage() ;
      exit(1) ;
      break ;
    default:
      fprintf(stderr, "unknown option %s\n", argv[1]) ;
      exit(1) ;
      break ;
    }

  return(nargs) ;
}

static void
usage_exit(void)
{
  print_help() ;
  exit(1) ;
}

static void
print_usage(void)
{
  fprintf(stderr,
          "usage: %s [options] <subject name> <hemi> [<surface name>]\n",
          Progname) ;
}

static void
print_help(void)
{
  print_usage() ;
  fprintf
  (stderr,
   "\nThis program measures a variety of anatomical properties.\n") ;
  fprintf
  (stderr, "\nValid options are:\n\n") ;
  fprintf
  (stderr,
   "-i <low thresh> <hi thresh>  - only consider thicknesses in\n"
   "                               the specified range\n") ;
  fprintf
  (stderr,
   "-l <.label file>             - limit calculations to specified "
   "label\n") ;
  fprintf
  (stderr,
   "-t <thickness file>          - use specified file for computing\n"
   "                               thickness statistics\n") ;
  fprintf
  (stderr,
   "-a <.annot file>             - compute properties for each label\n"
   "                               in the annotation file separately\n") ;
  fprintf
  (stderr,
   "-b                           - tabular output\n");
  fprintf
  (stderr,
   "-f <tablefile>               - table output to a file named tablefile\n"
   "                               (different format than -b) must specify\n"
   "                               an annotation file with -a or label\n"
   "                               file with -l\n");
  fprintf
  (stderr,
   "-log <log>                   - will write the stats into a file\n"
   "                               named <log>\n");
  fprintf
  (stderr,
   "-nsmooth <#>                 - will smooth the thicknessmap # of\n"
   "                               iterations before using it\n");
  fprintf
  (stderr,
   "-c <.ctab file>              - output the colortable for this annotation\n") ;

  exit(1) ;
}

static void
print_version(void)
{
  fprintf(stderr, "%s\n", vcid) ;
  exit(1) ;
}

double
MRISmeasureTotalWhiteMatterVolume(MRI *mri)
{
  double  total_volume, voxel_volume ;
  int     x, y, z, width, height, depth ;
  BUFTYPE *psrc ;

  width = mri->width ;
  height = mri->height ;
  depth = mri->depth ;
  voxel_volume = mri->xsize * mri->ysize * mri->zsize ;
  for (total_volume = 0.0, y = 0 ; y < height ; y++)
  {
    for (z = 0 ; z < depth ; z++)
    {
      psrc = &MRIvox(mri, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        if (*psrc++ > 10)
          total_volume += voxel_volume ;
      }
    }
  }
  return(total_volume) ;
}

int
MRIScomputeCurvatureStats(MRI_SURFACE *mris, double *pavg, double *pvar,
                          float ignore_below, float ignore_above)
{
  VERTEX    *v ;
  int       vno ;
  double    mean, var, n ;

  for (n = mean = 0.0, vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (v->curv < ignore_below || v->curv > ignore_above)
      continue ;
    mean += v->curv ;
    n += 1.0 ;
  }

  mean /= n ;
  for (var = 0.0, vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (v->curv < ignore_below || v->curv > ignore_above)
      continue ;
    var += (v->curv - mean) * (v->curv - mean) ;
  }

  var /= n ;
  *pavg = mean ;
  *pvar = var ;
  return(NO_ERROR) ;
}
double
MRISmeasureCorticalGrayMatterVolume(MRI_SURFACE *mris)
{
  FACE      *f ;
  VERTEX    *v ;
  int       fno, m, vno ;
  double    total, volume, avg_thick, white_area, pial_area, *white_areas ;

  white_areas = (double *)calloc(mris->nfaces,sizeof(double)) ;
  if (!white_areas)
    ErrorExit(ERROR_NOMEMORY,
              "MRISmeasureCorticalGrayMatterVolume: calloc failed") ;

  MRISsaveVertexPositions(mris, TMP_VERTICES) ;
  MRISrestoreVertexPositions(mris, WHITE_VERTICES) ;
  MRIScomputeMetricProperties(mris) ;
  for (white_area = total = 0.0, fno = 0 ; fno < mris->nfaces ; fno++)
  {
    f = &mris->faces[fno] ;
    if (f->ripflag)
      continue ;
    white_area += f->area ;
    white_areas[fno] = f->area;
  }

  MRISrestoreVertexPositions(mris, PIAL_VERTICES) ;
  MRIScomputeMetricProperties(mris) ;
  for (pial_area = 0.0, fno = 0 ; fno < mris->nfaces ; fno++)
  {
    f = &mris->faces[fno] ;
    if (f->ripflag)
      continue ;
    for (avg_thick = 0.0, m = 0 ; m < VERTICES_PER_FACE ; m++)
    {
      vno = f->v[m] ;
      v = &mris->vertices[vno] ;
      avg_thick += v->curv ;
    }
    avg_thick /= VERTICES_PER_FACE ;
    volume = avg_thick * (f->area+white_areas[fno])/2 ;
    pial_area += f->area ;
    total += volume ;
  }

  MRISrestoreVertexPositions(mris, TMP_VERTICES) ;
  MRIScomputeMetricProperties(mris);
  free(white_areas) ;
  return(total) ;
}


double
MRIScomputeAbsoluteCurvature(MRI_SURFACE *mris)
{
  VERTEX    *v ;
  int       vno ;
  double    total, n ;

  for (n = total = 0.0, vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    total += (fabs(v->curv) * v->area) ;
    n += 1.0 ;
  }

  return(total/n) ;
}

#if 0
int
MRISreadAnnotFile(MRI_SURFACE *mris, char *name)
{
  int    i,j,vno,num;
  FILE   *fp;
  char   fname[100] ;

  MRISbuildFileName(mris, name, fname) ;
  fp = fopen(fname,"r");
  if (fp==NULL)
    ErrorReturn(ERROR_NOFILE,
                (ERROR_NOFILE, "Annotion file %s not found",fname));
  for (vno=0;vno<mris->nvertices;vno++)
    mris->vertices[vno].marked = -1 ;
  num = freadInt(fp);
  /*  printf("num=%d\n",num);*/
  for (j=0;j<num;j++)
  {
    vno = freadInt(fp);
    i = freadInt(fp);
    if (vno>=mris->nvertices||vno<0)
      printf("vertex index out of range: %d i=%d\n",vno,i);
    else
      mris->vertices[vno].marked = i;
  }
  fclose(fp);
  return(NO_ERROR) ;
}


int
MRISripVerticesWithMark(MRI_SURFACE *mris, int mark)
{
  int      vno ;
  VERTEX   *v ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (v->marked == mark)
      v->ripflag = 1 ;
  }
  MRISripFaces(mris) ;
  return(NO_ERROR) ;
}


int
MRISripVerticesWithoutMark(MRI_SURFACE *mris, int mark)
{
  int      vno ;
  VERTEX   *v ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (v->marked != mark)
      v->ripflag = 1 ;
  }
  MRISripFaces(mris) ;
  return(NO_ERROR) ;
}


int
MRISreplaceMarks(MRI_SURFACE *mris, int in_mark, int out_mark)
{
  int      vno ;
  VERTEX   *v ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (v->marked == in_mark)
      v->marked = out_mark ;
  }
  return(NO_ERROR) ;
}

#endif

int
MRISripVerticesWithAnnotation(MRI_SURFACE *mris, int annotation)
{
  int      vno ;
  VERTEX   *v ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (v->annotation == annotation)
      v->ripflag = 1 ;
  }
  MRISripFaces(mris) ;
  return(NO_ERROR) ;
}


int
MRISripVerticesWithoutAnnotation(MRI_SURFACE *mris, int annotation)
{
  int      vno ;
  VERTEX   *v ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
#if 0
    if (v->ripflag)
      continue ;
#endif
    if (v->annotation != annotation)
      v->ripflag = 1 ;
    else
      v->ripflag = 0 ;
  }
  MRISripFaces(mris) ;
  return(NO_ERROR) ;
}


int
MRISreplaceAnnotations(MRI_SURFACE *mris,
                       int in_annotation,
                       int out_annotation)
{
  int      vno ;
  VERTEX   *v ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (v->annotation == in_annotation)
      v->annotation = out_annotation ;
  }
  return(NO_ERROR) ;
}


int
MRISrestoreSurface(MRI_SURFACE *mris)
{
  int      vno, fno ;
  VERTEX   *v ;
  FACE     *f ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    v->ripflag = 0 ;
  }
  for (fno = 0 ; fno < mris->nfaces ; fno++)
  {
    f = &mris->faces[fno] ;
    f->ripflag = 0 ;
  }
  return(NO_ERROR) ;
}


int
MRIScountVertices(MRI_SURFACE *mris)
{

  int vno, n;

  n = 0;
  for (vno = 0;vno < mris->nvertices;vno++)
    if (!mris->vertices[vno].ripflag)
      n++;

  return(n);

}


double
MRIScomputeAbsoluteCurvatureMarked(MRI_SURFACE *mris, int mark)
{
  VERTEX *v ;
  double curv ;
  int    vno ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->marked != mark)
      v->ripflag = 1 ;
  }
  curv = MRIScomputeAbsoluteCurvature(mris) ;
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->marked != mark)
      v->ripflag = 0 ;
  }
  return(curv) ;
}


int
MRIScomputeCurvatureIndicesMarked(MRI_SURFACE *mris, double *pici,
                                  double *pfi, int mark)
{
  VERTEX *v ;
  int    ret, vno ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->marked != mark)
      v->ripflag = 1 ;
  }
  ret = MRIScomputeCurvatureIndices(mris, pici, pfi) ;
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->marked != mark)
      v->ripflag = 0 ;
  }
  return(ret) ;
}

