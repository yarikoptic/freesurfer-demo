/**
 * @file  mri_convert.c
 * @brief performs all kinds of conversion and reformatting of MRI volume files
 *
 */
/*
 * Original Author: Bruce Fischl (Apr 16, 1997)
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2010/11/24 01:22:38 $
 *    $Revision: 1.166.2.5 $
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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "diag.h"
#include "error.h"
#include "mri.h"
#include "mri2.h"
#include "fmriutils.h"
#include "mri_identify.h"
#include "gcamorph.h"
#include "DICOMRead.h"
#include "unwarpGradientNonlinearity.h"
#include "version.h"
#include "utils.h"
#include "macros.h"
#include "fmriutils.h"

/* ----- determines tolerance of non-orthogonal basis vectors ----- */
#define CLOSE_ENOUGH  (5e-3)

void get_ints(int argc, char *argv[], int *pos, int *vals, int nvals);
void get_floats(int argc, char *argv[], int *pos, float *vals, int nvals);
void get_string(int argc, char *argv[], int *pos, char *val);
void usage_message(FILE *stream);
void usage(FILE *stream);
float findMinSize(MRI *mri, int *conform_width);
int   findRightSize(MRI *mri, float conform_size);

int debug=0;

extern int errno;

char *Progname;

int ncutends = 0, cutends_flag = 0;

int slice_crop_flag = FALSE;
int slice_crop_start, slice_crop_stop;
int SplitFrames=0;

/*-------------------------------------------------------------*/
int main(int argc, char *argv[]) {
  int nargs = 0;
  MRI *mri_unwarped;
  MRI *mri, *mri2, *template, *mri_in_like;
  int i,err=0;
  int reorder_vals[3];
  float invert_val;
  int in_info_flag, out_info_flag;
  int template_info_flag;
  int voxel_size_flag;
  int nochange_flag ;
  int conform_flag;
  int conform_min;  // conform to the smallest dimension
  int conform_width;
  int conform_width_256_flag;
  int parse_only_flag;
  int reorder_flag;
  int reorder4_vals[4];
  int reorder4_flag;
  int in_stats_flag, out_stats_flag;
  int read_only_flag, no_write_flag;
  char in_name[STRLEN], out_name[STRLEN];
  int in_volume_type, out_volume_type;
  char resample_type[STRLEN];
  int resample_type_val;
  int in_i_size_flag, in_j_size_flag, in_k_size_flag, crop_flag = FALSE;
  int out_i_size_flag, out_j_size_flag, out_k_size_flag;
  float in_i_size, in_j_size, in_k_size;
  float out_i_size, out_j_size, out_k_size;
  int crop_center[3], sizes_good_flag, crop_size[3] ;
  float in_i_directions[3], in_j_directions[3], in_k_directions[3];
  float out_i_directions[3], out_j_directions[3], out_k_directions[3];
  float voxel_size[3];
  int in_i_direction_flag, in_j_direction_flag, in_k_direction_flag;
  int out_i_direction_flag, out_j_direction_flag, out_k_direction_flag;
  int  in_orientation_flag = FALSE;
  char in_orientation_string[STRLEN];
  int  out_orientation_flag = FALSE;
  char out_orientation_string[STRLEN];
  char tmpstr[STRLEN], *stem, *ext;
  char ostr[4] = {'\0','\0','\0','\0'};
  char *errmsg = NULL;
  int in_tr_flag = 0;
  float in_tr = 0;
  int in_ti_flag = 0;
  float in_ti = 0;
  int in_te_flag = 0;
  float in_te = 0;
  int in_flip_angle_flag = 0;
  float in_flip_angle = 0;
  float magnitude;
  float i_dot_j, i_dot_k, j_dot_k;
  float in_center[3], out_center[3];
  int in_center_flag, out_center_flag;
  int out_data_type;
  char out_data_type_string[STRLEN];
  int out_n_i, out_n_j, out_n_k;
  int out_n_i_flag, out_n_j_flag, out_n_k_flag;
  float fov_x, fov_y, fov_z;
  int force_in_type_flag, force_out_type_flag;
  int forced_in_type, forced_out_type;
  char in_type_string[STRLEN], out_type_string[STRLEN];
  char subject_name[STRLEN];
  int force_template_type_flag;
  int forced_template_type;
  char template_type_string[STRLEN];
  char reslice_like_name[STRLEN];
  int reslice_like_flag;
  int frame_flag, mid_frame_flag;
  int frame;
  int subsample_flag, SubSampStart, SubSampDelta, SubSampEnd;
  char in_name_only[STRLEN];
  char transform_fname[STRLEN];
  int transform_flag, invert_transform_flag;
  LTA *lta_transform = NULL;
  MRI *mri_transformed = NULL;
  MRI *mritmp=NULL;
  int transform_type;
  MATRIX *inverse_transform_matrix;
  int smooth_parcellation_flag, smooth_parcellation_count;
  int in_like_flag;
  char in_like_name[STRLEN];
  char out_like_name[STRLEN];
  int out_like_flag=FALSE;
  int in_n_i, in_n_j, in_n_k;
  int in_n_i_flag, in_n_j_flag, in_n_k_flag;
  int fill_parcellation_flag;
  int read_parcellation_volume_flag;
  int zero_outlines_flag;
  int read_otl_flags;
  int color_file_flag;
  char color_file_name[STRLEN];
  int no_scale_flag;
  int temp_type;
  int roi_flag;
  FILE *fptmp;
  int j,translate_labels_flag;
  int force_ras_good = FALSE;
  char gdf_image_stem[STRLEN];
  int in_matrix_flag, out_matrix_flag;
  float conform_size;
  int zero_ge_z_offset_flag = FALSE; //E/
  int nskip = 0; // number of frames to skip from start
  int ndrop = 0; // number of frames to skip from end
  VOL_GEOM vgtmp;
  LT *lt = 0;
  int DevXFM = 0;
  char devxfm_subject[STRLEN];
  MATRIX *T;
  float scale_factor, out_scale_factor ;
  int nthframe=-1;
  float fwhm, gstd;
  char cmdline[STRLEN] ;
  int sphinx_flag = FALSE;
  int LeftRightReverse = FALSE;
  int FlipCols = FALSE;
  int SliceReverse = FALSE;
  int SliceBias  = FALSE;
  float SliceBiasAlpha = 1.0,v;
  char AutoAlignFile[STRLEN];
  MATRIX *AutoAlign = NULL;
  MATRIX *cras = NULL, *vmid = NULL;
  int ascii_flag = FALSE, c=0,r=0,s=0,f=0,c2=0;

  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  make_cmd_version_string
    (argc, argv,
     "$Id: mri_convert.c,v 1.166.2.5 2010/11/24 01:22:38 nicks Exp $", 
     "$Name:  $",
     cmdline);

  for(i=0;i<argc;i++) printf("%s ",argv[i]);
  printf("\n");
  fflush(stdout);

  crop_size[0] = crop_size[1] = crop_size[2] = 256 ;
  crop_center[0] = crop_center[1] = crop_center[2] = 128 ;
  for(i=0;i<argc;i++) {
    if(strcmp(argv[i],"--debug")==0) {
      fptmp = fopen("debug.gdb","w");
      fprintf(fptmp,"# source this file in gdb to debug\n");
      fprintf(fptmp,"file %s \n",argv[0]);
      fprintf(fptmp,"run ");
      for(j=1;j<argc;j++) {
        if(strcmp(argv[j],"--debug")!=0)
          fprintf(fptmp,"%s ",argv[j]);
      }
      fprintf(fptmp,"\n");
      fclose(fptmp);
      break;
    }
  }

  /* ----- keep the compiler quiet ----- */
  mri2 = NULL;
  forced_in_type = forced_out_type =
    forced_template_type = MRI_VOLUME_TYPE_UNKNOWN;
  invert_transform_flag = FALSE;

  /* ----- get the program name ----- */
  Progname = strrchr(argv[0], '/');
  Progname = (Progname == NULL ? argv[0] : Progname + 1);

  /* ----- pass the command line to mriio ----- */
  mriio_command_line(argc, argv);

  /* ----- catch no arguments here ----- */
  if(argc == 1) {
    usage(stdout);
    exit(1);
  }

  /* ----- initialize values ----- */
  scale_factor = 1 ;
  out_scale_factor = 1 ;
  in_name[0] = '\0';
  out_name[0] = '\0';
  invert_val = -1.0;
  in_info_flag = FALSE;
  out_info_flag = FALSE;
  voxel_size_flag = FALSE;
  conform_flag = FALSE;
  conform_width_256_flag = FALSE;
  nochange_flag = FALSE ;
  parse_only_flag = FALSE;
  reorder_flag = FALSE;
  reorder4_flag = FALSE;
  in_stats_flag = FALSE;
  out_stats_flag = FALSE;
  read_only_flag = FALSE;
  no_write_flag = FALSE;
  resample_type_val = SAMPLE_TRILINEAR;
  in_i_size_flag = in_j_size_flag = in_k_size_flag = FALSE;
  out_i_size_flag = out_j_size_flag = out_k_size_flag = FALSE;
  in_i_direction_flag = in_j_direction_flag = in_k_direction_flag = FALSE;
  out_i_direction_flag = out_j_direction_flag = out_k_direction_flag = FALSE;
  in_center_flag = FALSE;
  out_center_flag = FALSE;
  out_data_type = -1;
  out_n_i_flag = out_n_j_flag = out_n_k_flag = FALSE;
  template_info_flag = FALSE;
  out_volume_type = MRI_VOLUME_TYPE_UNKNOWN;
  force_in_type_flag = force_out_type_flag = force_template_type_flag = FALSE;
  subject_name[0] = '\0';
  reslice_like_flag = FALSE;
  frame_flag = FALSE;
  mid_frame_flag = FALSE;
  subsample_flag = FALSE;
  transform_flag = FALSE;
  smooth_parcellation_flag = FALSE;
  in_like_flag = FALSE;
  in_n_i_flag = in_n_j_flag = in_n_k_flag = FALSE;
  fill_parcellation_flag = FALSE;
  zero_outlines_flag = FALSE;
  color_file_flag = FALSE;
  no_scale_flag = FALSE;
  roi_flag = FALSE;
  translate_labels_flag = TRUE;
  gdf_image_stem[0] = '\0';
  in_matrix_flag = FALSE;
  out_matrix_flag = FALSE;
  conform_min = FALSE;
  conform_size = 1.0;
  nskip = 0;
  ndrop = 0;
  fwhm = -1;
  gstd = -1;
  in_type_string[0] = 0;

  /* rkt: check for and handle version tag */
  nargs =
    handle_version_option
    (
      argc, argv,
      "$Id: mri_convert.c,v 1.166.2.5 2010/11/24 01:22:38 nicks Exp $", 
      "$Name:  $"
      );
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  for(i = 1;i < argc;i++) {
    if(strcmp(argv[i], "-version2") == 0) exit(97);
    else if(strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--reorder") == 0) {
      get_ints(argc, argv, &i, reorder_vals, 3);
      reorder_flag = TRUE;
    }
    else if(strcmp(argv[i], "-r4") == 0 || strcmp(argv[i], "--reorder4") == 0) {
      get_ints(argc, argv, &i, reorder4_vals, 4);
      reorder4_flag = TRUE;
    }
    else if(strcmp(argv[i], "--debug") == 0) debug = 1;
    else if(strcmp(argv[i], "--left-right-reverse") == 0) LeftRightReverse = 1;
    else if(strcmp(argv[i], "--flip-cols") == 0) FlipCols = 1;
    else if(strcmp(argv[i], "--slice-reverse") == 0) SliceReverse = 1;
    else if(strcmp(argv[i], "--ascii") == 0) {
      ascii_flag = 1;
      force_out_type_flag = TRUE;
    }
    else if(strcmp(argv[i], "--ascii+crsf") == 0) {
      ascii_flag = 2;
      force_out_type_flag = TRUE;
    }
    else if(strcmp(argv[i], "--ascii-fcol") == 0) {
      ascii_flag = 3;
      force_out_type_flag = TRUE;
    }
    else if(strcmp(argv[i], "--invert_contrast") == 0)
      get_floats(argc, argv, &i, &invert_val, 1);
    else if(strcmp(argv[i], "-i") == 0 ||
            strcmp(argv[i], "--input_volume") == 0)
      get_string(argc, argv, &i, in_name);
    else if(strcmp(argv[i], "-o") == 0 ||
            strcmp(argv[i], "--output_volume") == 0)
      get_string(argc, argv, &i, out_name);
    else if (strcmp(argv[i], "-c") == 0 ||
             strcmp(argv[i], "--conform") == 0)
      conform_flag = TRUE;
    else if (strcmp(argv[i], "--cw256") == 0)
    {
      conform_flag = TRUE;
      conform_width_256_flag = TRUE;
    }
    else if (strcmp(argv[i], "--sphinx") == 0 )
      sphinx_flag = TRUE;
    else if(strcmp(argv[i], "--autoalign") == 0){
      get_string(argc, argv, &i, AutoAlignFile);
      AutoAlign = MatrixReadTxt(AutoAlignFile,NULL);
      printf("Auto Align Matrix\n");
      MatrixPrint(stdout,AutoAlign);
    }
    else if (strcmp(argv[i], "-nc") == 0 ||
             strcmp(argv[i], "--nochange") == 0)
      nochange_flag = TRUE;
    else if (strcmp(argv[i], "-cm") == 0 ||
             strcmp(argv[i], "--conform_min") == 0) {
      conform_min = TRUE;
      conform_flag = TRUE;
    }
    else if (strcmp(argv[i], "-cs") == 0
             || strcmp(argv[i], "--conform_size") == 0) {
      get_floats(argc, argv, &i, &conform_size, 1);
      conform_flag = TRUE;
    }
    else if(strcmp(argv[i], "-po") == 0 ||
            strcmp(argv[i], "--parse_only") == 0)
      parse_only_flag = TRUE;
    else if(strcmp(argv[i], "-ii") == 0 ||
            strcmp(argv[i], "--in_info") == 0)
      in_info_flag = TRUE;
    else if(strcmp(argv[i], "-oi") == 0 ||
            strcmp(argv[i], "--out_info") == 0)
      out_info_flag = TRUE;
    else if(strcmp(argv[i], "-ti") == 0
            || strcmp(argv[i], "--template_info") == 0)
      template_info_flag = TRUE;
    else if(strcmp(argv[i], "-is") == 0 ||
            strcmp(argv[i], "--in_stats") == 0)
      in_stats_flag = TRUE;
    else if(strcmp(argv[i], "-os") == 0 ||
            strcmp(argv[i], "--out_stats") == 0)
      out_stats_flag = TRUE;
    else if(strcmp(argv[i], "-ro") == 0 ||
            strcmp(argv[i], "--read_only") == 0)
      read_only_flag = TRUE;
    else if(strcmp(argv[i], "-nw") == 0 ||
            strcmp(argv[i], "--no_write") == 0)
      no_write_flag = TRUE;
    else if(strcmp(argv[i], "-im") == 0 ||
            strcmp(argv[i], "--in_matrix") == 0)
      in_matrix_flag = TRUE;
    else if(strcmp(argv[i], "-om") == 0 ||
            strcmp(argv[i], "--out_matrix") == 0)
      out_matrix_flag = TRUE;
    else if(strcmp(argv[i], "--force_ras_good") == 0)
      force_ras_good = TRUE;
    else if(strcmp(argv[i], "--split") == 0)
      SplitFrames = TRUE;
    // transform related things here /////////////////////
    else if(strcmp(argv[i], "-at") == 0 ||
            strcmp(argv[i], "--apply_transform") == 0 ||
            strcmp(argv[i], "-T") == 0) {
      get_string(argc, argv, &i, transform_fname);
      transform_flag = TRUE;
      invert_transform_flag = FALSE;
    }
    else if (strcmp(argv[i], "--like")==0) {
      get_string(argc, argv, &i, out_like_name);
      out_like_flag = TRUE;
      // creates confusion when this is printed:
      //printf("WARNING: --like does not work on multi-frame data\n");
      // but we'll leave it here for the interested coder
    }
    else if (strcmp(argv[i], "--crop")==0) {
      crop_flag = TRUE ;
      get_ints(argc, argv, &i, crop_center, 3);
    }
    else if (strcmp(argv[i], "--slice-crop")==0) {
      slice_crop_flag = TRUE ;
      get_ints(argc, argv, &i, &slice_crop_start , 1);
      get_ints(argc, argv, &i, &slice_crop_stop , 1);
      if (slice_crop_start > slice_crop_stop) {
        fprintf(stderr,"ERROR: s_start > s_end\n");
        exit(1);
      }
    }
    else if (strcmp(argv[i], "--cropsize")==0) {
      crop_flag = TRUE ;
      get_ints(argc, argv, &i, crop_size, 3);
    }
    else if(strcmp(argv[i], "--devolvexfm") == 0) {
      /* devolve xfm to account for cras != 0 */
      get_string(argc, argv, &i, devxfm_subject);
      DevXFM = 1;
    }
    else if(strcmp(argv[i], "-ait") == 0 ||
            strcmp(argv[i], "--apply_inverse_transform") == 0) {
      get_string(argc, argv, &i, transform_fname);
      if(!FileExists(transform_fname)) {
        fprintf(stderr,"ERROR: cannot find transform file %s\n",
                transform_fname);
        exit(1);
      }
      transform_flag = TRUE;
      invert_transform_flag = TRUE;
    }
    ///////////////////////////////////////////////////////////
    else if(strcmp(argv[i], "-iis") == 0 ||
            strcmp(argv[i], "--in_i_size") == 0) {
      get_floats(argc, argv, &i, &in_i_size, 1);
      in_i_size_flag = TRUE;
    }
    else if(strcmp(argv[i], "-ijs") == 0 ||
            strcmp(argv[i], "--in_j_size") == 0) {
      get_floats(argc, argv, &i, &in_j_size, 1);
      in_j_size_flag = TRUE;
    }
    else if(strcmp(argv[i], "-iks") == 0 ||
            strcmp(argv[i], "--in_k_size") == 0) {
      get_floats(argc, argv, &i, &in_k_size, 1);
      in_k_size_flag = TRUE;
    }
    else if(strcmp(argv[i], "-ois") == 0 ||
            strcmp(argv[i], "--out_i_size") == 0) {
      get_floats(argc, argv, &i, &out_i_size, 1);
      out_i_size_flag = TRUE;
    }
    else if(strcmp(argv[i], "-ojs") == 0 ||
            strcmp(argv[i], "--out_j_size") == 0) {
      get_floats(argc, argv, &i, &out_j_size, 1);
      out_j_size_flag = TRUE;
    }
    else if(strcmp(argv[i], "-oks") == 0 ||
            strcmp(argv[i], "--out_k_size") == 0) {
      get_floats(argc, argv, &i, &out_k_size, 1);
      out_k_size_flag = TRUE;
    }
    else if(strcmp(argv[i], "-iid") == 0
            || strcmp(argv[i], "--in_i_direction") == 0) {
      get_floats(argc, argv, &i, in_i_directions, 3);
      magnitude = sqrt(in_i_directions[0]*in_i_directions[0] +
                       in_i_directions[1]*in_i_directions[1] +
                       in_i_directions[2]*in_i_directions[2]);
      if(magnitude == 0.0) {
        fprintf(stderr, "\n%s: directions must have non-zero magnitude; "
                "in_i_direction = (%g, %g, %g)\n",
                Progname,
                in_i_directions[0],
                in_i_directions[1],
                in_i_directions[2]);
        usage_message(stdout);
        exit(1);
      }
      if(magnitude != 1.0) {
        printf("normalizing in_i_direction: (%g, %g, %g) -> ",
               in_i_directions[0],
               in_i_directions[1],
               in_i_directions[2]);
        in_i_directions[0] = in_i_directions[0] / magnitude;
        in_i_directions[1] = in_i_directions[1] / magnitude;
        in_i_directions[2] = in_i_directions[2] / magnitude;
        printf("(%g, %g, %g)\n",
               in_i_directions[0],
               in_i_directions[1],
               in_i_directions[2]);
      }
      in_i_direction_flag = TRUE;
    }
    else if(strcmp(argv[i], "-ijd") == 0
            || strcmp(argv[i], "--in_j_direction") == 0) {
      get_floats(argc, argv, &i, in_j_directions, 3);
      magnitude = sqrt(in_j_directions[0]*in_j_directions[0] +
                       in_j_directions[1]*in_j_directions[1] +
                       in_j_directions[2]*in_j_directions[2]);
      if(magnitude == 0.0) {
        fprintf(stderr, "\n%s: directions must have non-zero magnitude; "
                "in_j_direction = (%g, %g, %g)\n",
                Progname,
                in_j_directions[0],
                in_j_directions[1],
                in_j_directions[2]);
        usage_message(stdout);
        exit(1);
      }
      if(magnitude != 1.0) {
        printf("normalizing in_j_direction: (%g, %g, %g) -> ",
               in_j_directions[0],
               in_j_directions[1],
               in_j_directions[2]);
        in_j_directions[0] = in_j_directions[0] / magnitude;
        in_j_directions[1] = in_j_directions[1] / magnitude;
        in_j_directions[2] = in_j_directions[2] / magnitude;
        printf("(%g, %g, %g)\n",
               in_j_directions[0],
               in_j_directions[1],
               in_j_directions[2]);
      }
      in_j_direction_flag = TRUE;
    }
    else if(strcmp(argv[i], "-ikd") == 0 ||
            strcmp(argv[i], "--in_k_direction") == 0) {
      get_floats(argc, argv, &i, in_k_directions, 3);
      magnitude = sqrt(in_k_directions[0]*in_k_directions[0] +
                       in_k_directions[1]*in_k_directions[1] +
                       in_k_directions[2]*in_k_directions[2]);
      if(magnitude == 0.0) {
        fprintf(stderr, "\n%s: directions must have non-zero magnitude; "
                "in_k_direction = (%g, %g, %g)\n",
                Progname,
                in_k_directions[0],
                in_k_directions[1],
                in_k_directions[2]);
        usage_message(stdout);
        exit(1);
      }
      if(magnitude != 1.0) {
        printf("normalizing in_k_direction: (%g, %g, %g) -> ",
               in_k_directions[0],
               in_k_directions[1],
               in_k_directions[2]);
        in_k_directions[0] = in_k_directions[0] / magnitude;
        in_k_directions[1] = in_k_directions[1] / magnitude;
        in_k_directions[2] = in_k_directions[2] / magnitude;
        printf("(%g, %g, %g)\n",
               in_k_directions[0],
               in_k_directions[1],
               in_k_directions[2]);
      }
      in_k_direction_flag = TRUE;
    }

    else if(strcmp(argv[i], "--in_orientation") == 0) {
      get_string(argc, argv, &i, in_orientation_string);
      errmsg = MRIcheckOrientationString(in_orientation_string);
      if(errmsg) {
        printf("ERROR: with in orientation string %s\n",
               in_orientation_string);
        printf("%s\n",errmsg);
        exit(1);
      }
      in_orientation_flag = TRUE;
    }

    else if(strcmp(argv[i], "--out_orientation") == 0) {
      get_string(argc, argv, &i, out_orientation_string);
      errmsg = MRIcheckOrientationString(out_orientation_string);
      if(errmsg) {
        printf("ERROR: with out_orientation string %s\n",
               out_orientation_string);
        printf("%s\n",errmsg);
        exit(1);
      }
      out_orientation_flag = TRUE;
    }

    else if(strcmp(argv[i], "-oid") == 0 ||
            strcmp(argv[i], "--out_i_direction") == 0) {
      get_floats(argc, argv, &i, out_i_directions, 3);
      magnitude = sqrt(out_i_directions[0]*out_i_directions[0] +
                       out_i_directions[1]*out_i_directions[1] +
                       out_i_directions[2]*out_i_directions[2]);
      if(magnitude == 0.0) {
        fprintf(stderr, "\n%s: directions must have non-zero magnitude; "
                "out_i_direction = (%g, %g, %g)\n", Progname,
                out_i_directions[0],
                out_i_directions[1],
                out_i_directions[2]);
        usage_message(stdout);
        exit(1);
      }
      if(magnitude != 1.0) {
        printf("normalizing out_i_direction: (%g, %g, %g) -> ",
               out_i_directions[0],
               out_i_directions[1],
               out_i_directions[2]);
        out_i_directions[0] = out_i_directions[0] / magnitude;
        out_i_directions[1] = out_i_directions[1] / magnitude;
        out_i_directions[2] = out_i_directions[2] / magnitude;
        printf("(%g, %g, %g)\n",
               out_i_directions[0],
               out_i_directions[1],
               out_i_directions[2]);
      }
      out_i_direction_flag = TRUE;
    }
    else if(strcmp(argv[i], "-ojd") == 0 ||
            strcmp(argv[i], "--out_j_direction") == 0) {
      get_floats(argc, argv, &i, out_j_directions, 3);
      magnitude = sqrt(out_j_directions[0]*out_j_directions[0] +
                       out_j_directions[1]*out_j_directions[1] +
                       out_j_directions[2]*out_j_directions[2]);
      if(magnitude == 0.0) {
        fprintf(stderr, "\n%s: directions must have non-zero magnitude; "
                "out_j_direction = (%g, %g, %g)\n",
                Progname,
                out_j_directions[0],
                out_j_directions[1],
                out_j_directions[2]);
        usage_message(stdout);
        exit(1);
      }
      if(magnitude != 1.0) {
        printf("normalizing out_j_direction: (%g, %g, %g) -> ",
               out_j_directions[0],
               out_j_directions[1],
               out_j_directions[2]);
        out_j_directions[0] = out_j_directions[0] / magnitude;
        out_j_directions[1] = out_j_directions[1] / magnitude;
        out_j_directions[2] = out_j_directions[2] / magnitude;
        printf("(%g, %g, %g)\n",
               out_j_directions[0],
               out_j_directions[1],
               out_j_directions[2]);
      }
      out_j_direction_flag = TRUE;
    }
    else if(strcmp(argv[i], "-okd") == 0 ||
            strcmp(argv[i], "--out_k_direction") == 0) {
      get_floats(argc, argv, &i, out_k_directions, 3);
      magnitude = sqrt(out_k_directions[0]*out_k_directions[0] +
                       out_k_directions[1]*out_k_directions[1] +
                       out_k_directions[2]*out_k_directions[2]);
      if(magnitude == 0.0) {
        fprintf(stderr, "\n%s: directions must have non-zero magnitude; "
                "out_k_direction = (%g, %g, %g)\n",
                Progname,
                out_k_directions[0],
                out_k_directions[1],
                out_k_directions[2]);
        usage_message(stdout);
        exit(1);
      }
      if(magnitude != 1.0) {
        printf("normalizing out_k_direction: (%g, %g, %g) -> ",
               out_k_directions[0],
               out_k_directions[1],
               out_k_directions[2]);
        out_k_directions[0] = out_k_directions[0] / magnitude;
        out_k_directions[1] = out_k_directions[1] / magnitude;
        out_k_directions[2] = out_k_directions[2] / magnitude;
        printf("(%g, %g, %g)\n",
               out_k_directions[0],
               out_k_directions[1],
               out_k_directions[2]);
      }
      out_k_direction_flag = TRUE;
    }
    else if(strcmp(argv[i], "-ic") == 0 ||
            strcmp(argv[i], "--in_center") == 0) {
      get_floats(argc, argv, &i, in_center, 3);
      in_center_flag = TRUE;
    }
    else if(strcmp(argv[i], "-oc") == 0 ||
            strcmp(argv[i], "--out_center") == 0) {
      get_floats(argc, argv, &i, out_center, 3);
      out_center_flag = TRUE;
    }
    else if(strcmp(argv[i], "-oni") == 0 ||
            strcmp(argv[i], "-oic") == 0 ||
            strcmp(argv[i], "--out_i_count") == 0) {
      get_ints(argc, argv, &i, &out_n_i, 1);
      out_n_i_flag = TRUE;
    }
    else if(strcmp(argv[i], "-onj") == 0 ||
            strcmp(argv[i], "-ojc") == 0 ||
            strcmp(argv[i], "--out_j_count") == 0) {
      get_ints(argc, argv, &i, &out_n_j, 1);
      out_n_j_flag = TRUE;
    }
    else if(strcmp(argv[i], "-onk") == 0 ||
            strcmp(argv[i], "-okc") == 0 ||
            strcmp(argv[i], "--out_k_count") == 0) {
      get_ints(argc, argv, &i, &out_n_k, 1);
      out_n_k_flag = TRUE;
    }
    else if(strcmp(argv[i], "-vs") == 0 ||
            strcmp(argv[i], "-voxsize") == 0 ) {
      get_floats(argc, argv, &i, voxel_size, 3);
      voxel_size_flag = TRUE;
    }
    else if(strcmp(argv[i], "-ini") == 0 ||
            strcmp(argv[i], "-iic") == 0 ||
            strcmp(argv[i], "--in_i_count") == 0) {
      get_ints(argc, argv, &i, &in_n_i, 1);
      in_n_i_flag = TRUE;
    }
    else if(strcmp(argv[i], "-inj") == 0 ||
            strcmp(argv[i], "-ijc") == 0 ||
            strcmp(argv[i], "--in_j_count") == 0) {
      get_ints(argc, argv, &i, &in_n_j, 1);
      in_n_j_flag = TRUE;
    }
    else if(strcmp(argv[i], "-ink") == 0 ||
            strcmp(argv[i], "-ikc") == 0 ||
            strcmp(argv[i], "--in_k_count") == 0) {
      get_ints(argc, argv, &i, &in_n_k, 1);
      in_n_k_flag = TRUE;
    }
    else if(strcmp(argv[i], "--fwhm") == 0) {
      get_floats(argc, argv, &i, &fwhm, 1);
      gstd = fwhm/sqrt(log(256.0));
      printf("fwhm = %g, gstd = %g\n",fwhm,gstd);
    }
    else if( strcmp(argv[i], "-tr") == 0 ) {
      get_floats(argc, argv, &i, &in_tr, 1);
      in_tr_flag = TRUE;
    }
    else if( strcmp(argv[i], "-TI") == 0 ) {
      get_floats(argc, argv, &i, &in_ti, 1);
      in_ti_flag = TRUE;
    }
    else if( strcmp(argv[i], "-te") == 0 ) {
      get_floats(argc, argv, &i, &in_te, 1);
      in_te_flag = TRUE;
    }
    else if( strcmp(argv[i], "-flip_angle") == 0 ) {
      get_floats(argc, argv, &i, &in_flip_angle, 1);
      in_flip_angle_flag = TRUE;
    }

    else if(strcmp(argv[i], "-odt") == 0 ||
            strcmp(argv[i], "--out_data_type") == 0) {
      get_string(argc, argv, &i, out_data_type_string);
      if(strcmp(StrLower(out_data_type_string), "uchar") == 0)
        out_data_type = MRI_UCHAR;
      else if(strcmp(StrLower(out_data_type_string), "short") == 0)
        out_data_type = MRI_SHORT;
      else if(strcmp(StrLower(out_data_type_string), "int") == 0)
        out_data_type = MRI_INT;
      else if(strcmp(StrLower(out_data_type_string), "float") == 0)
        out_data_type = MRI_FLOAT;
      else
      {
        fprintf(stderr, "\n%s: unknown data type \"%s\"\n",
                Progname, argv[i]);
        usage_message(stdout);
        exit(1);
      }
    }
    else if(strcmp(argv[i], "-sc") == 0 || 
            strcmp(argv[i], "--scale") == 0) {
      scale_factor = atof(argv[i+1]) ;
      i++ ;
    }
    else if(strcmp(argv[i], "-osc") == 0 || 
            strcmp(argv[i], "--out-scale") == 0) {
      out_scale_factor = atof(argv[i+1]) ;
      i++ ;
    }
    else if(strcmp(argv[i], "-rt") == 0 ||
            strcmp(argv[i], "--resample_type") == 0) {
      get_string(argc, argv, &i, resample_type);
      if(strcmp(StrLower(resample_type), "interpolate") == 0)
        resample_type_val = SAMPLE_TRILINEAR;
      else if(strcmp(StrLower(resample_type), "nearest") == 0)
        resample_type_val = SAMPLE_NEAREST;
      else if(strcmp(StrLower(resample_type), "weighted") == 0)
        resample_type_val = SAMPLE_WEIGHTED;
      else if(strcmp(StrLower(resample_type), "sinc") == 0)
        resample_type_val = SAMPLE_SINC;
      else if(strcmp(StrLower(resample_type), "cubic") == 0)
        resample_type_val = SAMPLE_CUBIC;
      else
      {
        fprintf(stderr, "\n%s: unknown resample type \"%s\"\n",
                Progname, argv[i]);
        usage_message(stdout);
        exit(1);
      }
    }
    else if(strcmp(argv[i], "-it") == 0 || 
            strcmp(argv[i], "--in_type") == 0) {
      get_string(argc, argv, &i, in_type_string);
      forced_in_type = string_to_type(in_type_string);
      force_in_type_flag = TRUE;
    }
    else if(strcmp(argv[i], "-dicomread2") == 0) UseDICOMRead2 = 1;
    else if(strcmp(argv[i], "-dicomread0") == 0) UseDICOMRead2 = 0;
    else if(strcmp(argv[i], "-ot") == 0 ||
            strcmp(argv[i], "--out_type") == 0) {
      get_string(argc, argv, &i, out_type_string);
      forced_out_type = string_to_type(out_type_string);
      /* see mri_identify.c */
      force_out_type_flag = TRUE;
    }
    else if(strcmp(argv[i], "-tt") == 0 ||
            strcmp(argv[i], "--template_type") == 0) {
      get_string(argc, argv, &i, template_type_string);
      forced_template_type = string_to_type(template_type_string);
      force_template_type_flag = TRUE;
    }
    else if(strcmp(argv[i], "-sn") == 0 ||
            strcmp(argv[i], "--subject_name") == 0) {
      get_string(argc, argv, &i, subject_name);
    }
    else if(strcmp(argv[i], "-gis") == 0 ||
            strcmp(argv[i], "--gdf_image_stem") == 0) {
      get_string(argc, argv, &i, gdf_image_stem);
      if(strlen(gdf_image_stem) == 0) {
        fprintf(stderr, "\n%s: zero length GDF image stem given\n",
                Progname);
        usage_message(stdout);
        exit(1);
      }
    }
    else if(strcmp(argv[i], "-rl") == 0 ||
            strcmp(argv[i], "--reslice_like") == 0) {
      get_string(argc, argv, &i, reslice_like_name);
      reslice_like_flag = TRUE;
    }
    else if(strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--frame") == 0) {
      get_ints(argc, argv, &i, &frame, 1);
      frame_flag = TRUE;
    }
    else if(strcmp(argv[i], "--cutends") == 0) {
      get_ints(argc, argv, &i, &ncutends, 1);
      cutends_flag = TRUE;
    }
    else if(strcmp(argv[i], "--slice-bias") == 0) {
      get_floats(argc, argv, &i, &SliceBiasAlpha, 1);
      SliceBias = TRUE;
    }
    else if(strcmp(argv[i], "--mid-frame") == 0)        {
      frame_flag = TRUE;
      mid_frame_flag = TRUE;
    }
    else if(strcmp(argv[i], "--fsubsample") == 0) {
      get_ints(argc, argv, &i, &SubSampStart, 1);
      get_ints(argc, argv, &i, &SubSampDelta, 1);
      get_ints(argc, argv, &i, &SubSampEnd, 1);
      if(SubSampDelta == 0){
        printf("ERROR: don't use subsample delta = 0\n");
        exit(1);
      }
      subsample_flag = TRUE;
    }

    else if(strcmp(argv[i], "-il") == 0 || strcmp(argv[i], "--in_like") == 0) {
      get_string(argc, argv, &i, in_like_name);
      in_like_flag = TRUE;
    }
    else if(strcmp(argv[i], "-roi") == 0 || strcmp(argv[i], "--roi") == 0) {
      roi_flag = TRUE;
    }
    else if(strcmp(argv[i], "-fp") == 0 ||
            strcmp(argv[i], "--fill_parcellation") == 0) {
      fill_parcellation_flag = TRUE;
    }
    else if(strcmp(argv[i], "-zo") == 0 ||
            strcmp(argv[i], "--zero_outlines") == 0) {
      zero_outlines_flag = TRUE;
    }
    else if(strcmp(argv[i], "-sp") == 0 ||
            strcmp(argv[i], "--smooth_parcellation") == 0) {
      get_ints(argc, argv, &i, &smooth_parcellation_count, 1);
      if(smooth_parcellation_count < 14 || smooth_parcellation_count > 26) {
        fprintf(stderr, "\n%s: clean parcellation count must "
                "be between 14 and 26, inclusive\n", Progname);
        usage_message(stdout);
        exit(1);
      }
      smooth_parcellation_flag = TRUE;
    }
    else if (strcmp(argv[i], "-cf") == 0 ||
             strcmp(argv[i], "--color_file") == 0) {
      get_string(argc, argv, &i, color_file_name);
      color_file_flag = TRUE;
    } else if (strcmp(argv[i], "-nt") == 0 ||
               strcmp(argv[i], "--no_translate") == 0) {
      translate_labels_flag = FALSE;
    } else if (strcmp(argv[i], "-ns") == 0 ||
               strcmp(argv[i], "--no_scale") == 0) {
      get_ints(argc, argv, &i, &no_scale_flag, 1);
      no_scale_flag = (no_scale_flag == 0 ? FALSE : TRUE);
    } else if (strcmp(argv[i], "-nth") == 0 ||
               strcmp(argv[i], "--nth_frame") == 0) {
      get_ints(argc, argv, &i, &nthframe, 1);
    } else if (strcmp(argv[i], "-cg") == 0 ||
               strcmp(argv[i], "--crop_gdf") == 0) {
      mriio_set_gdf_crop_flag(TRUE);
    } else if (strcmp(argv[i], "--unwarp_gradient_nonlinearity") == 0) {
      /* !@# start */
      unwarp_flag = 1;

      /* Determine gradient type: sonata or allegra */
      get_string(argc, argv, &i, unwarp_gradientType);
      if ( (strcmp(unwarp_gradientType, "sonata")  != 0) &&
           (strcmp(unwarp_gradientType, "allegra") != 0) &&
           (strcmp(unwarp_gradientType, "GE") != 0) ) {
        fprintf(stderr, "\n%s: must specify gradient type "
                "('sonata' or 'allegra' or 'GE')\n", Progname);
        usage_message(stdout);
        exit(1);
      }

      /* Determine whether or not to do a partial unwarp */
      get_string(argc, argv, &i, unwarp_partialUnwarp);
      if ( (strcmp(unwarp_partialUnwarp, "fullUnwarp") != 0) &&
           (strcmp(unwarp_partialUnwarp, "through-plane")  != 0) ) {
        fprintf(stderr, "\n%s: must specify unwarping type "
                "('fullUnwarp' or 'through-plane')\n", Progname);
        usage_message(stdout);
        exit(1);
      }

      /* Determine whether or not to do jacobian correction */
      get_string(argc, argv, &i, unwarp_jacobianCorrection);
      if ( (strcmp(unwarp_jacobianCorrection, "JacobianCorrection")  != 0) &&
           (strcmp(unwarp_jacobianCorrection, "noJacobianCorrection") != 0) ) {
        fprintf(stderr, "\n%s: must specify intensity correction type "
                "('JacobianCorrection' or 'noJacobianCorrection')\n",
                Progname);
        usage_message(stdout);
        exit(1);
      }

      /* Determine interpolation type: linear or sinc */
      get_string(argc, argv, &i, unwarp_interpType);
      if ( (strcmp(unwarp_interpType, "linear") != 0) &&
           (strcmp(unwarp_interpType, "sinc")   != 0) ) {
        fprintf(stderr, "\n%s: must specify interpolation type "
                "('linear' or 'sinc')\n", Progname);
        usage_message(stdout);
        exit(1);
      }

      /* Get the HW for sinc interpolation (if linear interpolation,
         this integer value is not used) */
      get_ints(argc, argv, &i, &unwarp_sincInterpHW, 1);

      /* OPTIONS THAT THERE ARE NO PLANS TO SUPPORT */
      /* Jacobian correction with through-plane only correction */
      if ( (strcmp(unwarp_jacobianCorrection, "JacobianCorrection") == 0) &&
           (strcmp(unwarp_partialUnwarp, "through-plane") == 0) ) {
        fprintf(stderr, "\n%s: Jacobian correction not valid for "
                "'through-plane' only unwarping)\n", Progname);
        exit(1);
      }

      /* OPTIONS NOT CURRENTLY SUPPORTED (BUT W/ PLANS TO SUPPORT) */
      /* 1) GE unwarping not supported until we have offset data */
      if ( strcmp(unwarp_gradientType, "GE") == 0 ) {
        fprintf(stderr, "\n%s: unwarping data from GE scanners not "
                "supported at present.\n", Progname);
        exit(1);
      }

      /* 2) for GE: through-plane correction requires rewarping the
         in-plane unwarped image, which requires map inversion */
      if ( strcmp(unwarp_partialUnwarp, "through-plane") == 0 ) {
        fprintf(stderr, "\n%s: through-plane only unwarping not "
                "supported at present.\n", Progname);
        exit(1);
      }
      /* !@# end */

    } else if ((strcmp(argv[i], "-u") == 0)
               || (strcmp(argv[i], "--usage") == 0)
               || (strcmp(argv[i], "--help") == 0)
               || (strcmp(argv[i], "-h") == 0)) {
      usage(stdout);
      exit(0);
    }
    /*-------------------------------------------------------------*/
    else if (strcmp(argv[i], "--status") == 0 ||
             strcmp(argv[i], "--statusfile") == 0) {
      /* File name to write percent complete for Siemens DICOM */
      if ( (argc-1) - i < 1 ) {
        fprintf(stderr,"ERROR: option --statusfile "
                "requires one argument\n");
        exit(1);
      }
      i++;
      SDCMStatusFile = (char *) calloc(strlen(argv[i])+1,sizeof(char));
      memmove(SDCMStatusFile,argv[i],strlen(argv[i]));
      fptmp = fopen(SDCMStatusFile,"w");
      if (fptmp == NULL) {
        fprintf(stderr,"ERROR: could not open %s for writing\n",
                SDCMStatusFile);
        exit(1);
      }
      fprintf(fptmp,"0\n");
      fclose(fptmp);
    }
    /*-------------------------------------------------------------*/
    else if (strcmp(argv[i], "--sdcmlist") == 0) {
      /* File name that contains a list of Siemens DICOM files
         that are in the same run as the one listed on the
         command-line. If not present, the directory will be scanned,
         but this can take a while.
      */
      if ( (argc-1) - i < 1 ) {
        fprintf(stderr,
                "ERROR: option --sdcmlist requires one argument\n");
        exit(1);
      }
      i++;
      SDCMListFile = (char *) calloc(strlen(argv[i])+1,sizeof(char));
      memmove(SDCMListFile,argv[i],strlen(argv[i]));
      fptmp = fopen(SDCMListFile,"r");
      if (fptmp == NULL) {
        fprintf(stderr,"ERROR: could not open %s for reading\n",
                SDCMListFile);
        exit(1);
      }
      fclose(fptmp);
    }
    /*-------------------------------------------------------------*/
    else if ( (strcmp(argv[i], "--nspmzeropad") == 0) ||
              (strcmp(argv[i], "--out_nspmzeropad") == 0)) {
      /* Choose the amount of zero padding for spm output files */
      if ( (argc-1) - i < 1 ) {
        fprintf(stderr,"ERROR: option --out_nspmzeropad "
                "requires one argument\n");
        exit(1);
      }
      i++;
      sscanf(argv[i],"%d",&N_Zero_Pad_Output);
    }
    /*-------------------------------------------------------------*/
    else if ( (strcmp(argv[i], "--in_nspmzeropad") == 0)) {
      /* Choose the amount of zero padding for spm input files */
      if ( (argc-1) - i < 1 ) {
        fprintf(stderr,"ERROR: option --in_nspmzeropad "
                "requires one argument\n");
        exit(1);
      }
      i++;
      sscanf(argv[i],"%d",&N_Zero_Pad_Input);
    }
    /*-----------------------------------------------------*/ //E/
    else if (strcmp(argv[i], "-zgez") == 0 ||
             strcmp(argv[i], "--zero_ge_z_offset") == 0) {
      zero_ge_z_offset_flag = TRUE;
    }
    /*-----------------------------------------------------*/ //E/
    else if (strcmp(argv[i], "-nozgez") == 0 ||
             strcmp(argv[i], "--no_zero_ge_z_offset") == 0) {
      zero_ge_z_offset_flag = FALSE;
    } else if (strcmp(argv[i], "--nskip") == 0 ) {
      get_ints(argc, argv, &i, &nskip, 1);
      printf("nskip = %d\n",nskip);
      if (nskip < 0) {
        printf("ERROR: nskip cannot be negative\n");
        exit(1);
      }
    } else if (strcmp(argv[i], "--ndrop") == 0 ) {
      get_ints(argc, argv, &i, &ndrop, 1);
      printf("ndrop = %d\n",ndrop);
      if (ndrop < 0) {
        printf("ERROR: ndrop cannot be negative\n");
        exit(1);
      }
    }
    /*-------------------------------------------------------------*/
    else {
      if (argv[i][0] == '-') {
        fprintf(stderr,
                "\n%s: unknown flag \"%s\"\n", Progname, argv[i]);
        usage_message(stdout);
        exit(1);
      } else {
        if (in_name[0] == '\0')
          strcpy(in_name, argv[i]);
        else if (out_name[0] == '\0')
          strcpy(out_name, argv[i]);
        else {
          if (i + 1 == argc)
            fprintf(stderr, "\n%s: extra argument (\"%s\")\n",
                    Progname, argv[i]);
          else
            fprintf(stderr, "\n%s: extra arguments (\"%s\" and "
                    "following)\n", Progname, argv[i]);
          usage_message(stdout);
          exit(1);
        }
      }

    }
  }
  /**** Finished parsing command line ****/
  /* option inconsistency checks */
  if (force_ras_good && (in_i_direction_flag || in_j_direction_flag ||
                         in_k_direction_flag)) {
    fprintf(stderr, "ERROR: cannot use --force_ras_good and "
            "--in_?_direction_flag\n");
    exit(1);
  }
  if (conform_flag == FALSE && conform_min == TRUE) {
    fprintf(stderr, "In order to use -cm (--conform_min), "
            "you must set -c (--conform)  at the same time.\n");
    exit(1);
  }

  /* ----- catch zero or negative voxel dimensions ----- */

  sizes_good_flag = TRUE;

  if ((in_i_size_flag && in_i_size <= 0.0) ||
      (in_j_size_flag && in_j_size <= 0.0) ||
      (in_k_size_flag && in_k_size <= 0.0) ||
      (out_i_size_flag && out_i_size <= 0.0) ||
      (out_j_size_flag && out_j_size <= 0.0) ||
      (out_k_size_flag && out_k_size <= 0.0)) {
    fprintf(stderr, "\n%s: voxel sizes must be "
            "greater than zero\n", Progname);
    sizes_good_flag = FALSE;
  }

  if (in_i_size_flag && in_i_size <= 0.0)
    fprintf(stderr, "in i size = %g\n", in_i_size);
  if (in_j_size_flag && in_j_size <= 0.0)
    fprintf(stderr, "in j size = %g\n", in_j_size);
  if (in_k_size_flag && in_k_size <= 0.0)
    fprintf(stderr, "in k size = %g\n", in_k_size);
  if (out_i_size_flag && out_i_size <= 0.0)
    fprintf(stderr, "out i size = %g\n", out_i_size);
  if (out_j_size_flag && out_j_size <= 0.0)
    fprintf(stderr, "out j size = %g\n", out_j_size);
  if (out_k_size_flag && out_k_size <= 0.0)
    fprintf(stderr, "out k size = %g\n", out_k_size);

  if (!sizes_good_flag) {
    usage_message(stdout);
    exit(1);
  }

  /* ----- catch missing input or output volume name ----- */
  if (in_name[0] == '\0') {
    fprintf(stderr, "\n%s: missing input volume name\n", Progname);
    usage_message(stdout);
    exit(1);
  }

  if (out_name[0] == '\0' && !(read_only_flag || no_write_flag)) {
    fprintf(stderr, "\n%s: missing output volume name\n", Progname);
    usage_message(stdout);
    exit(1);
  }

  /* ----- copy file name (only -- strip '@' and '#') ----- */
  MRIgetVolumeName(in_name, in_name_only);

  /* If input type is spm and N_Zero_Pad_Input < 0, set to 3*/
  if (!strcmp(in_type_string,"spm") && N_Zero_Pad_Input < 0)
    N_Zero_Pad_Input = 3;

  /* ----- catch unknown volume types ----- */
  if (force_in_type_flag && forced_in_type == MRI_VOLUME_TYPE_UNKNOWN) {
    fprintf(stderr, "\n%s: unknown input "
            "volume type %s\n", Progname, in_type_string);
    usage_message(stdout);
    exit(1);
  }

  if (force_template_type_flag &&
      forced_template_type == MRI_VOLUME_TYPE_UNKNOWN) {
    fprintf(stderr, "\n%s: unknown template volume type %s\n",
            Progname, template_type_string);
    usage_message(stdout);
    exit(1);
  }

  if(force_out_type_flag && forced_out_type == MRI_VOLUME_TYPE_UNKNOWN &&
     ! ascii_flag) {
    fprintf(stderr, "\n%s: unknown output volume type %s\n",
            Progname, out_type_string);
    usage_message(stdout);
    exit(1);
  }

  /* ----- warn if read only is desired and an
     output volume is specified or the output info flag is set ----- */
  if (read_only_flag && out_name[0] != '\0')
    fprintf(stderr,
            "%s: warning: read only flag is set; "
            "nothing will be written to %s\n", Progname, out_name);
  if (read_only_flag && (out_info_flag || out_matrix_flag))
    fprintf(stderr, "%s: warning: read only flag is set; "
            "no output information will be printed\n", Progname);


  /* ----- get the type of the output ----- */
  if(!read_only_flag) {
    if(!force_out_type_flag) {
      // if(!read_only_flag && !no_write_flag)
      // because conform_flag value changes depending on type below
      {
        out_volume_type = mri_identify(out_name);
        if (out_volume_type == MRI_VOLUME_TYPE_UNKNOWN) {
          fprintf(stderr,
                  "%s: can't determine type of output volume\n",
                  Progname);
          exit(1);
        }
      }
    } else
      out_volume_type = forced_out_type;

    // if output type is COR, then it is always conformed
    if (out_volume_type == MRI_CORONAL_SLICE_DIRECTORY)
      conform_flag = TRUE;
  }

  /* ----- catch the parse-only flag ----- */
  if (parse_only_flag) {
    printf("input volume name: %s\n", in_name);
    printf("input name only: %s\n", in_name_only);
    printf("output volume name: %s\n", out_name);
    printf("parse_only_flag = %d\n", parse_only_flag);
    printf("conform_flag = %d\n", conform_flag);
    printf("conform_size = %f\n", conform_size);
    printf("in_info_flag = %d\n", in_info_flag);
    printf("out_info_flag = %d\n", out_info_flag);
    printf("in_matrix_flag = %d\n", in_matrix_flag);
    printf("out_matrix_flag = %d\n", out_matrix_flag);

    if (force_in_type_flag)
      printf("input type is %d\n", forced_in_type);
    if (force_out_type_flag)
      printf("output type is %d\n", forced_out_type);

    if (subject_name[0] != '\0')
      printf("subject name is %s\n", subject_name);

    if (invert_val >= 0)
      printf("inversion, value is %g\n", invert_val);

    if (reorder_flag)
      printf("reordering, values are %d %d %d\n",
             reorder_vals[0], reorder_vals[1], reorder_vals[2]);

    if (reorder4_flag)
      printf("reordering, values are %d %d %d %d\n",
             reorder4_vals[0], reorder4_vals[1], reorder4_vals[2], reorder4_vals[3]);

    printf("translation of otl labels is %s\n",
           translate_labels_flag ? "on" : "off");

    exit(0);
  }


  /* ----- check for a gdf image stem if the output type is gdf ----- */
  if (out_volume_type == GDF_FILE && strlen(gdf_image_stem) == 0) {
    fprintf(stderr, "%s: GDF output type, "
            "but no GDF image file stem\n", Progname);
    exit(1);
  }

  /* ----- read the in_like volume ----- */
  if (in_like_flag) {
    printf("reading info from %s...\n", in_like_name);
    mri_in_like = MRIreadInfo(in_like_name);
    if (mri_in_like == NULL)
      exit(1);
  }

  /* ----- read the volume ----- */
  if (force_in_type_flag)
    in_volume_type = forced_in_type;
  else
    in_volume_type = mri_identify(in_name_only);

  if (in_volume_type == MRI_VOLUME_TYPE_UNKNOWN)    {
    errno = 0;
    ErrorPrintf(ERROR_BADFILE, 
                "file not found or unknown file type for file %s",
                in_name_only);
    if (in_like_flag) MRIfree(&mri_in_like);
    exit(1);
  }


  if (roi_flag && in_volume_type != GENESIS_FILE) {
    errno = 0;
    ErrorPrintf(ERROR_BADPARM, "rois must be in GE format");
    if (in_like_flag) MRIfree(&mri_in_like);
    exit(1);
  }

  if (zero_ge_z_offset_flag && in_volume_type != DICOM_FILE) //E/
  {
    zero_ge_z_offset_flag = FALSE ;
    fprintf(stderr, "Not a GE dicom volume: -zgez "
            "= --zero_ge_z_offset option ignored.\n");
  }

  printf("$Id: mri_convert.c,v 1.166.2.5 2010/11/24 01:22:38 nicks Exp $\n");
  printf("reading from %s...\n", in_name_only);

  if (in_volume_type == OTL_FILE) {

    if (!in_like_flag && !in_n_k_flag) {
      errno = 0;
      ErrorPrintf(ERROR_BADPARM, "parcellation read: must specify"
                  "a volume depth with either in_like or in_k_count");
      exit(1);
    }

    if (!color_file_flag) {
      errno = 0;
      ErrorPrintf(ERROR_BADPARM, "parcellation read: must specify a"
                  "color file name");
      if (in_like_flag)
        MRIfree(&mri_in_like);
      exit(1);
    }

    read_parcellation_volume_flag = TRUE;
    if (read_only_flag &&
        (in_info_flag || in_matrix_flag) &&
        !in_stats_flag)
      read_parcellation_volume_flag = FALSE;

    read_otl_flags = 0x00;

    if (read_parcellation_volume_flag)
      read_otl_flags |= READ_OTL_READ_VOLUME_FLAG;

    if (fill_parcellation_flag)
      read_otl_flags |= READ_OTL_FILL_FLAG;
    else {
      printf("notice: unfilled parcellations "
             "currently unimplemented\n");
      printf("notice: filling outlines\n");
      read_otl_flags |= READ_OTL_FILL_FLAG;
    }

    if (translate_labels_flag)
      read_otl_flags |= READ_OTL_TRANSLATE_LABELS_FLAG;

    if (zero_outlines_flag) {
      read_otl_flags |= READ_OTL_ZERO_OUTLINES_FLAG;
    }

    if (in_like_flag)
      mri = MRIreadOtl(in_name, mri_in_like->width, mri_in_like->height,
                       mri_in_like->depth,
                       color_file_name, read_otl_flags);
    else
      mri = MRIreadOtl(in_name, 0, 0, in_n_k,
                       color_file_name, read_otl_flags);

    if (mri == NULL) {
      if (in_like_flag)
        MRIfree(&mri_in_like);
      exit(1);
    }

    /* ----- smooth the parcellation if requested ----- */
    if (smooth_parcellation_flag) {
      printf("smoothing parcellation...\n");
      mri2 = MRIsmoothParcellation(mri, smooth_parcellation_count);
      if (mri2 == NULL) {
        if (in_like_flag)
          MRIfree(&mri_in_like);
        exit(1);
      }
      MRIfree(&mri);
      mri = mri2;
    }

    resample_type_val = SAMPLE_NEAREST;
    no_scale_flag = TRUE;

  } else if (roi_flag) {
    if (!in_like_flag && !in_n_k_flag) {
      errno = 0;
      ErrorPrintf(ERROR_BADPARM, "roi read: must specify a volume"
                  "depth with either in_like or in_k_count");
      if (in_like_flag)
        MRIfree(&mri_in_like);
      exit(1);
    }

    if (in_like_flag)
      mri = MRIreadGeRoi(in_name, mri_in_like->depth);
    else
      mri = MRIreadGeRoi(in_name, in_n_k);

    if (mri == NULL) {
      if (in_like_flag)
        MRIfree(&mri_in_like);
      exit(1);
    }

    resample_type_val = SAMPLE_NEAREST;
    no_scale_flag = TRUE;
  } else {
    if (read_only_flag &&
        (in_info_flag || in_matrix_flag) &&
        !in_stats_flag) {
      if (force_in_type_flag)
        mri = MRIreadHeader(in_name, in_volume_type);
      else
        mri = MRIreadInfo(in_name);
    } else {
      if (force_in_type_flag) {
        //printf("MRIreadType()\n");
        mri = MRIreadType(in_name, in_volume_type);
      } else {
        if (nthframe < 0)
          mri = MRIread(in_name);
        else
          mri = MRIreadEx(in_name, nthframe);
      }
    }
  }


  if (mri == NULL) {
    if (in_like_flag) MRIfree(&mri_in_like);
    exit(1);
  }

  if(slice_crop_flag){
    printf("Cropping slices from %d to %d\n",slice_crop_start,slice_crop_stop);
    mri2  = MRIcrop(mri, 0, 0, slice_crop_start,
                    mri->width-1, mri->height-1,slice_crop_stop);
    if(mri2 == NULL) exit(1);
    MRIfree(&mri);
    mri = mri2;
  }

  if(LeftRightReverse){
    // Performs a left-right reversal of the geometry by finding the
    // dimension that is most left-right oriented and negating its
    // column in the vox2ras matrix. The pixel data itself is not
    // changed. It would also have worked to have negated the first
    // row in the vox2ras, but negating a column is the header
    // equivalent to reversing the pixel data in a dimension. This
    // also adjusts the CRAS so that if the pixel data are
    // subsequently reversed then a header registration will bring
    // the new and the original volume into perfect registration.
    printf("WARNING: applying left-right reversal to the input geometry\n"
           "without reversing the pixel data. This will likely make \n"
           "the volume geometry WRONG, so make sure you know what you  \n"
           "are doing.\n");

    printf("  CRAS Before: %g %g %g\n",mri->c_r,mri->c_a,mri->c_s);
    T = MRIxfmCRS2XYZ(mri, 0);
    vmid = MatrixAlloc(4,1,MATRIX_REAL);
    vmid->rptr[4][1] = 1;

    MRIdircosToOrientationString(mri,ostr);
    if(ostr[0] == 'L' || ostr[0] == 'R'){
      printf("  Reversing geometry for the columns\n");
      vmid->rptr[1][1] = mri->width/2.0 - 1;
      vmid->rptr[2][1] = mri->height/2.0;
      vmid->rptr[3][1] = mri->depth/2.0;
      cras = MatrixMultiply(T,vmid,NULL);
      mri->x_r *= -1.0;
      mri->x_a *= -1.0;
      mri->x_s *= -1.0;
    }
    if(ostr[1] == 'L' || ostr[1] == 'R'){
      printf("  Reversing geometry for the rows\n");
      vmid->rptr[1][1] = mri->width/2.0;
      vmid->rptr[2][1] = mri->height/2.0 - 1;
      vmid->rptr[3][1] = mri->depth/2.0;
      cras = MatrixMultiply(T,vmid,NULL);
      mri->y_r *= -1.0;
      mri->y_a *= -1.0;
      mri->y_s *= -1.0;
    }
    if(ostr[2] == 'L' || ostr[2] == 'R'){
      printf("  Reversing geometry for the slices\n");
      vmid->rptr[1][1] = mri->width/2.0;
      vmid->rptr[2][1] = mri->height/2.0;
      vmid->rptr[3][1] = mri->depth/2.0 - 1;
      cras = MatrixMultiply(T,vmid,NULL);
      mri->z_r *= -1.0;
      mri->z_a *= -1.0;
      mri->z_s *= -1.0;
    }
    mri->c_r = cras->rptr[1][1];
    mri->c_a = cras->rptr[2][1];
    mri->c_s = cras->rptr[3][1];
    printf("  CRAS After %g %g %g\n",mri->c_r,mri->c_a,mri->c_s);
    MatrixFree(&T);
    MatrixFree(&vmid);
    MatrixFree(&cras);
  }

  if(FlipCols){
    // Reverses the columns WITHOUT changing the geometry in the
    // header.  Know what you are going here.
    printf("WARNING: flipping cols without changing geometry\n");
    mri2 = MRIcopy(mri,NULL);
    printf("type %d %d\n",mri->type,mri2->type);
    for(c=0; c < mri->width; c++){
      c2 = mri->width - c - 1;
      for(r=0; r < mri->height; r++){
        for(s=0; s < mri->depth; s++){
          for(f=0; f < mri->nframes; f++){
            v = MRIgetVoxVal(mri,c,r,s,f);
            MRIsetVoxVal(mri2,c2,r,s,f,v);
          }
        }
      }
    }
    printf("type %d %d\n",mri->type,mri2->type);
    MRIfree(&mri);
    mri = mri2; 
  }

  if(SliceReverse){
    printf("Reversing slices, updating vox2ras\n");
    mri2 = MRIreverseSlices(mri, NULL);
    if(mri2 == NULL) exit(1);
    MRIfree(&mri);
    mri = mri2;
  }

  if(SliceBias){
    printf("Applying Half-Cosine Slice Bias, Alpha = %g\n",SliceBiasAlpha);
    MRIhalfCosBias(mri, SliceBiasAlpha, mri);
  }

  if(fwhm > 0) {
    printf("Smoothing input at fwhm = %g, gstd = %g\n",fwhm,gstd);
    MRIgaussianSmooth(mri, gstd, 1, mri);
  }

  MRIaddCommandLine(mri, cmdline) ;
  if (!FEQUAL(scale_factor,1.0)) {
    printf("scaling input volume by %2.3f\n", scale_factor) ;
    MRIscalarMul(mri, mri, scale_factor) ;
  }

  if (zero_ge_z_offset_flag) //E/
    mri->c_s = 0.0;

  if (unwarp_flag) {
    /* if unwarp_flag is true, unwarp the distortions due
       to gradient coil nonlinearities */
    printf("INFO: unwarping ... ");
    mri_unwarped = unwarpGradientNonlinearity(mri,
                                              unwarp_gradientType,
                                              unwarp_partialUnwarp,
                                              unwarp_jacobianCorrection,
                                              unwarp_interpType,
                                              unwarp_sincInterpHW);
    MRIfree(&mri);
    mri = mri_unwarped;
    printf("done \n ");
  }

  printf("TR=%2.2f, TE=%2.2f, TI=%2.2f, flip angle=%2.2f\n",
         mri->tr, mri->te, mri->ti, DEGREES(mri->flip_angle)) ;
  if (in_volume_type != OTL_FILE) {
    if (fill_parcellation_flag)
      printf("fill_parcellation flag ignored on a "
             "non-parcellation read\n");
    if (smooth_parcellation_flag)
      printf("smooth_parcellation flag ignored on a "
             "non-parcellation read\n");
  }

  /* ----- apply the in_like volume if it's been read ----- */
  if (in_like_flag) {
    if (mri->width   != mri_in_like->width ||
        mri->height  != mri_in_like->height ||
        mri->depth   != mri_in_like->depth) {
      errno = 0;
      ErrorPrintf(ERROR_BADPARM, "volume sizes do not match\n");
      ErrorPrintf(ERROR_BADPARM, "%s: (width, height, depth, frames) "
                  "= (%d, %d, %d, %d)\n",
                  in_name,
                  mri->width, mri->height, mri->depth, mri->nframes);
      ErrorPrintf(ERROR_BADPARM, "%s: (width, height, depth, frames) "
                  "= (%d, %d, %d, %d)\n",
                  in_like_name, mri_in_like->width,
                  mri_in_like->height, mri_in_like->depth,
                  mri_in_like->nframes);
      MRIfree(&mri);
      MRIfree(&mri_in_like);
      exit(1);
    }
    if (mri->nframes != mri_in_like->nframes)  printf("INFO: frames are not the same\n");
    temp_type = mri->type;

    mritmp = MRIcopyHeader(mri_in_like, mri);
    if (mritmp == NULL) {
      errno = 0;
      ErrorPrintf(ERROR_BADPARM, "error copying information from "
                  "%s structure to %s structure\n",
                  in_like_name, in_name);
      MRIfree(&mri);
      MRIfree(&mri_in_like);
      exit(1);
    }

    mri->type = temp_type;
    MRIfree(&mri_in_like);
  }

  if (mri->ras_good_flag == 0) {
    printf("WARNING: it does not appear that there "
           "was sufficient information\n"
           "in the input to assign orientation to the volume... \n");
    if (force_ras_good) {
      printf("However, you have specified that the "
             "default orientation should\n"
             "be used with by adding --force_ras_good "
             "on the command-line.\n");
      mri->ras_good_flag = 1;
    }
    if (in_i_direction_flag || in_j_direction_flag || in_k_direction_flag) {
      printf("However, you have specified one or more "
             "orientations on the \n"
             "command-line using -i?d or --in-?-direction (?=i,j,k).\n");
      mri->ras_good_flag = 1;
    }
  }

  /* ----- apply command-line parameters ----- */
  if (in_i_size_flag)    mri->xsize = in_i_size;
  if (in_j_size_flag)    mri->ysize = in_j_size;
  if (in_k_size_flag)    mri->zsize = in_k_size;
  if (in_i_direction_flag) {
    mri->x_r = in_i_directions[0];
    mri->x_a = in_i_directions[1];
    mri->x_s = in_i_directions[2];
    mri->ras_good_flag = 1;
  }
  if (in_j_direction_flag) {
    mri->y_r = in_j_directions[0];
    mri->y_a = in_j_directions[1];
    mri->y_s = in_j_directions[2];
    mri->ras_good_flag = 1;
  }
  if (in_k_direction_flag) {
    mri->z_r = in_k_directions[0];
    mri->z_a = in_k_directions[1];
    mri->z_s = in_k_directions[2];
    mri->ras_good_flag = 1;
  }
  if (in_orientation_flag) {
    printf("Setting input orientation to %s\n",in_orientation_string);
    MRIorientationStringToDircos(mri, in_orientation_string);
    mri->ras_good_flag = 1;
  }
  if (in_center_flag) {
    mri->c_r = in_center[0];
    mri->c_a = in_center[1];
    mri->c_s = in_center[2];
  }
  if (subject_name[0] != '\0')
    strcpy(mri->subject_name, subject_name);

  if (in_tr_flag) mri->tr = in_tr;
  if (in_ti_flag) mri->ti = in_ti;
  if (in_te_flag) mri->te = in_te;
  if (in_flip_angle_flag) mri->flip_angle = in_flip_angle;

  /* ----- correct starts, ends, and fov if necessary ----- */
  if (in_i_size_flag || in_j_size_flag || in_k_size_flag) {

    fov_x = mri->xsize * mri->width;
    fov_y = mri->ysize * mri->height;
    fov_z = mri->zsize * mri->depth;

    mri->xend = fov_x / 2.0;
    mri->xstart = -mri->xend;
    mri->yend = fov_y / 2.0;
    mri->ystart = -mri->yend;
    mri->zend = fov_z / 2.0;
    mri->zstart = -mri->zend;

    mri->fov = (fov_x > fov_y ?
                (fov_x > fov_z ? fov_x : fov_z) :
                (fov_y > fov_z ? fov_y : fov_z) );

  }

  /* ----- give a warning for non-orthogonal directions ----- */
  i_dot_j = mri->x_r *
    mri->y_r + mri->x_a *
    mri->y_a + mri->x_s *
    mri->y_s;
  i_dot_k = mri->x_r *
    mri->z_r + mri->x_a *
    mri->z_a + mri->x_s *
    mri->z_s;
  j_dot_k = mri->y_r *
    mri->z_r + mri->y_a *
    mri->z_a + mri->y_s *
    mri->z_s;
  if (fabs(i_dot_j) > CLOSE_ENOUGH ||
      fabs(i_dot_k) > CLOSE_ENOUGH ||
      fabs(i_dot_k) > CLOSE_ENOUGH) {
    printf("warning: input volume axes are not orthogonal:"
           "i_dot_j = %.6f, i_dot_k = %.6f, j_dot_k = %.6f\n",
           i_dot_j, i_dot_k, j_dot_k);
  }
  printf("i_ras = (%g, %g, %g)\n", mri->x_r, mri->x_a, mri->x_s);
  printf("j_ras = (%g, %g, %g)\n", mri->y_r, mri->y_a, mri->y_s);
  printf("k_ras = (%g, %g, %g)\n", mri->z_r, mri->z_a, mri->z_s);

  /* ----- catch the in info flag ----- */
  if (in_info_flag) {
    printf("input structure:\n");
    MRIdump(mri, stdout);
  }

  if (in_matrix_flag) {
    MATRIX *i_to_r;
    i_to_r = extract_i_to_r(mri);
    if (i_to_r != NULL) {
      printf("input ijk -> ras:\n");
      MatrixPrint(stdout, i_to_r);
      MatrixFree(&i_to_r);
    } else
      printf("error getting input matrix\n");
  }

  /* ----- catch the in stats flag ----- */
  if (in_stats_flag)
    MRIprintStats(mri, stdout);

  // Load the transform
  if (transform_flag) {
    printf("INFO: Applying transformation from file %s...\n",
           transform_fname);
    transform_type = TransformFileNameType(transform_fname);
    if (transform_type == MNI_TRANSFORM_TYPE ||
        transform_type == TRANSFORM_ARRAY_TYPE ||
        transform_type == REGISTER_DAT ||
        transform_type == FSLREG_TYPE) {
      printf("Reading transform with LTAreadEx()\n");
      // lta_transform = LTAread(transform_fname);
      lta_transform = LTAreadEx(transform_fname);
      if (lta_transform  == NULL) {
        fprintf(stderr, "ERROR: Reading transform from file %s\n",
                transform_fname);
        exit(1);
      }
      if (transform_type == FSLREG_TYPE) {
        MRI *tmp = 0;
        if (out_like_flag == 0) {
          printf("ERROR: fslmat does not have the information "
                 "on the dst volume\n");
          printf("ERROR: you must give option '--like volume' to specify the"
                 " dst volume info\n");
          MRIfree(&mri);
          exit(1);
        }
        // now setup dst volume info
        tmp = MRIreadHeader(out_like_name, MRI_VOLUME_TYPE_UNKNOWN);
        // flsmat does not contain src and dst info
        LTAmodifySrcDstGeom(lta_transform, mri, tmp);
        // add src and dst information
        LTAchangeType(lta_transform, LINEAR_VOX_TO_VOX);
        MRIfree(&tmp);
      } // end FSLREG_TYPE

      if (DevXFM) {
        printf("INFO: devolving XFM (%s)\n",devxfm_subject);
        printf("-------- before ---------\n");
        MatrixPrint(stdout,lta_transform->xforms[0].m_L);
        T = DevolveXFM(devxfm_subject,
                       lta_transform->xforms[0].m_L, NULL);
        if (T==NULL) exit(1);
        printf("-------- after ---------\n");
        MatrixPrint(stdout,lta_transform->xforms[0].m_L);
        printf("-----------------\n");
      } // end DevXFM

      if (invert_transform_flag) {
        inverse_transform_matrix =
          MatrixInverse(lta_transform->xforms[0].m_L,NULL);
        if (inverse_transform_matrix == NULL) {
          fprintf(stderr, "ERROR: inverting transform\n");
          MatrixPrint(stdout,lta_transform->xforms[0].m_L);
          exit(1);
        }

        MatrixFree(&(lta_transform->xforms[0].m_L));
        lta_transform->xforms[0].m_L = inverse_transform_matrix;
        // reverse src and dst target info.
        // since it affects the c_ras values of the result
        // in LTAtransform()
        // question is what to do when transform src info is invalid.
        lt = &lta_transform->xforms[0];
        if (lt->src.valid==0) {
          char buf[512];
          char *p;
          MRI *mriOrig;

          fprintf(stderr, "INFO: Trying to get the source "
                  "volume information from the transform name\n");
          // copy transform filename
          strcpy(buf, transform_fname);
          // reverse look for the first '/'
          p = strrchr(buf, '/');
          if (p != 0) {
            p++;
            *p = '\0';
            // set the terminator. i.e.
            // ".... mri/transforms" from "
            //..../mri/transforms/talairach.xfm"
          } else {// no / present means only a filename is given
            strcpy(buf, "./");
          }
          strcat(buf, "../orig"); // go to mri/orig from
          // mri/transforms/
          // check whether we can read header info or not
          mriOrig = MRIreadHeader(buf, MRI_VOLUME_TYPE_UNKNOWN);
          if (mriOrig) {
            getVolGeom(mriOrig, &lt->src);
            fprintf(stderr, "INFO: Succeeded in retrieving "
                    "the source volume info.\n");
          } else  printf("INFO: Failed to find %s as a source volume.  \n"
                         "      The inverse c_(ras) may not be valid.\n",
                         buf);
        } // end not valid
        copyVolGeom(&lt->dst, &vgtmp);
        copyVolGeom(&lt->src, &lt->dst);
        copyVolGeom(&vgtmp, &lt->src);
      } // end invert_transform_flag

    } // end transform type
		// if type is non-linear, load transform below when applying it...
	} // end transform_flag


  
  if (reslice_like_flag) {

    if (force_template_type_flag) {
      printf("reading template info from (type %s) volume %s...\n",
             template_type_string, reslice_like_name);
      template =
        MRIreadHeader(reslice_like_name, forced_template_type);
      if(template == NULL) {
        fprintf(stderr, "error reading from volume %s\n",
                reslice_like_name);
        exit(1);
      }
    }
    else {
      printf("reading template info from volume %s...\n",
             reslice_like_name);
      template = MRIreadInfo(reslice_like_name);
      if(template == NULL) {
        fprintf(stderr, "error reading from volume %s\n",
                reslice_like_name);
        exit(1);
      }
    }
  }
  else {
    template =
      MRIallocHeader(mri->width, mri->height, mri->depth, mri->type);
    MRIcopyHeader(mri, template);
		
		// if we loaded a transform above, set the target geometry:
    if(lta_transform && lta_transform->xforms[0].dst.valid == 1)
		  useVolGeomToMRI(&lta_transform->xforms[0].dst,template);
			
    if(conform_flag) {
      conform_width = 256;
      if(conform_min == TRUE)  conform_size = findMinSize(mri, &conform_width);
      else
      {
        if(conform_width_256_flag) conform_width = 256; // force it
        else conform_width = findRightSize(mri, conform_size);
      }
      template->width =
        template->height = template->depth = conform_width;
      template->imnr0 = 1;
      template->imnr1 = conform_width;
      template->type = MRI_UCHAR;
      template->thick = conform_size;
      template->ps = conform_size;
      template->xsize =
        template->ysize = template->zsize = conform_size;
      printf("Original Data has (%g, %g, %g) mm size "
             "and (%d, %d, %d) voxels.\n",
             mri->xsize, mri->ysize, mri->zsize,
             mri->width, mri->height, mri->depth);
      printf("Data is conformed to %g mm size and "
             "%d voxels for all directions\n",
             conform_size, conform_width);
      template->xstart = template->ystart =
        template->zstart = - conform_width/2;
      template->xend =
        template->yend = template->zend = conform_width/2;
      template->x_r = -1.0;
      template->x_a =  0.0;
      template->x_s =  0.0;
      template->y_r =  0.0;
      template->y_a =  0.0;
      template->y_s = -1.0;
      template->z_r =  0.0;
      template->z_a =  1.0;
      template->z_s =  0.0;
    }
    else if ( voxel_size_flag ) {
      template = MRIallocHeader(mri->width,
                                mri->height,
                                mri->depth,
                                mri->type );

      MRIcopyHeader( mri, template );

      template->nframes = mri->nframes ;

      template->width = (int)ceil( mri->xsize * mri->width / voxel_size[0] );
      template->height = (int)ceil( mri->ysize * mri->height / voxel_size[1] );
      template->depth = (int)ceil( mri->zsize * mri->depth / voxel_size[2] );

      template->xsize = voxel_size[0];
      template->ysize = voxel_size[1];
      template->zsize = voxel_size[2];

      template->xstart = - template->width / 2;
      template->xend   = template->width / 2;
      template->ystart = - template->height / 2;
      template->yend   = template->height / 2;
      template->zstart = - template->depth / 2;
      template->zend   = template->depth / 2;

    }
  }

  /* ----- apply command-line parameters ----- */
  if (out_i_size_flag)
    template->xsize = out_i_size;
  if (out_j_size_flag)
    template->ysize = out_j_size;
  if (out_k_size_flag)
    template->zsize = out_k_size;
  if (out_n_i_flag)
    template->width = out_n_i;
  if (out_n_j_flag)
    template->height = out_n_j;
  if (out_n_k_flag) {
    template->depth = out_n_k;
    template->imnr1 = template->imnr0 + out_n_k - 1;
  }
  if (out_i_direction_flag) {
    template->x_r = out_i_directions[0];
    template->x_a = out_i_directions[1];
    template->x_s = out_i_directions[2];
  }
  if (out_j_direction_flag) {
    template->y_r = out_j_directions[0];
    template->y_a = out_j_directions[1];
    template->y_s = out_j_directions[2];
  }
  if (out_k_direction_flag) {
    template->z_r = out_k_directions[0];
    template->z_a = out_k_directions[1];
    template->z_s = out_k_directions[2];
  }
  if (out_orientation_flag) {
    printf("Setting output orientation to %s\n",out_orientation_string);
    MRIorientationStringToDircos(template, out_orientation_string);
  }
  if(out_center_flag) {
    template->c_r = out_center[0];
    template->c_a = out_center[1];
    template->c_s = out_center[2];
  }

  /* ----- correct starts, ends, and fov if necessary ----- */
  if (out_i_size_flag || out_j_size_flag || out_k_size_flag ||
      out_n_i_flag    || out_n_j_flag    || out_n_k_flag) {

    fov_x = template->xsize * template->width;
    fov_y = template->ysize * template->height;
    fov_z = template->zsize * template->depth;
    template->xend = fov_x / 2.0;
    template->xstart = -template->xend;
    template->yend = fov_y / 2.0;
    template->ystart = -template->yend;
    template->zend = fov_z / 2.0;
    template->zstart = -template->zend;

    template->fov = (fov_x > fov_y ?
                     (fov_x > fov_z ? fov_x : fov_z) :
                     (fov_y > fov_z ? fov_y : fov_z) );

  }

  /* ----- give a warning for non-orthogonal directions ----- */
  i_dot_j = template->x_r *
    template->y_r + template->x_a *
    template->y_a + template->x_s *
    template->y_s;
  i_dot_k = template->x_r *
    template->z_r + template->x_a *
    template->z_a + template->x_s *
    template->z_s;
  j_dot_k = template->y_r *
    template->z_r + template->y_a *
    template->z_a + template->y_s *
    template->z_s;
  if (fabs(i_dot_j) > CLOSE_ENOUGH ||
      fabs(i_dot_k) > CLOSE_ENOUGH ||
      fabs(i_dot_k) > CLOSE_ENOUGH) {
    printf("warning: output volume axes are not orthogonal:"
           "i_dot_j = %.6f, i_dot_k = %.6f, j_dot_k = %.6f\n",
           i_dot_j, i_dot_k, j_dot_k);
    printf("i_ras = (%g, %g, %g)\n",
           template->x_r, template->x_a, template->x_s);
    printf("j_ras = (%g, %g, %g)\n",
           template->y_r, template->y_a, template->y_s);
    printf("k_ras = (%g, %g, %g)\n",
           template->z_r, template->z_a, template->z_s);
  }
  if (out_data_type >= 0)
    template->type = out_data_type;

  /* ----- catch the template info flag ----- */
  if (template_info_flag) {
    printf("template structure:\n");
    MRIdump(template, stdout);
  }

  /* ----- exit here if read only is desired ----- */
  if(read_only_flag)  exit(0);

  /* ----- apply a transformation if requested ----- */
  if (transform_flag) {
    printf("INFO: Applying transformation from file %s...\n",
           transform_fname);
    transform_type = TransformFileNameType(transform_fname);
    if (transform_type == MNI_TRANSFORM_TYPE ||
        transform_type == TRANSFORM_ARRAY_TYPE ||
        transform_type == REGISTER_DAT ||
        transform_type == FSLREG_TYPE) {

      // in these cases the lta_transform was loaded above
      if (!lta_transform)
      {
        fprintf(stderr, "ERROR: lta should have been loaded \n");
        exit(1);
      }
			

      /* Think about calling MRIlinearTransform() here; need vox2vox
         transform. Can create NN version. In theory, LTAtransform()
         can handle multiple transforms, but the inverse assumes only
         one. NN is good for ROI*/

      printf("---------------------------------\n");
      printf("INFO: Transform Matrix (%s)\n",
             LTAtransformTypeName(lta_transform->type));
      MatrixPrint(stdout,lta_transform->xforms[0].m_L);
      printf("---------------------------------\n");

      /* LTAtransform() runs either MRIapplyRASlinearTransform()
         for RAS2RAS or MRIlinearTransform() for Vox2Vox. */
      /* MRIlinearTransform() calls MRIlinearTransformInterp() */
      if (out_like_flag == 1) {
        MRI *tmp = 0;
        printf("INFO: transform dst into the like-volume\n");
        tmp = MRIreadHeader(out_like_name, MRI_VOLUME_TYPE_UNKNOWN);
        mri_transformed =
          MRIalloc(tmp->width, tmp->height, tmp->depth, mri->type);
        if (!mri_transformed)
          ErrorExit(ERROR_NOMEMORY, "could not allocate memory");
        MRIcopyHeader(tmp, mri_transformed);
        MRIfree(&tmp);
        tmp = 0;
        mri_transformed = LTAtransformInterp(mri, 
                                             mri_transformed, 
                                             lta_transform, 
                                             resample_type_val);
      } else {

        if (out_center_flag)
        {
          MATRIX *m, *mtmp ;
          m = MRIgetResampleMatrix(template, mri);
          mtmp = MRIrasXformToVoxelXform(mri, mri, 
                                         lta_transform->xforms[0].m_L, NULL) ;
          MatrixFree(&lta_transform->xforms[0].m_L) ;
          lta_transform->xforms[0].m_L = MatrixMultiply(m, mtmp, NULL) ;
          MatrixFree(&m) ; MatrixFree(&mtmp) ;
          lta_transform->type = LINEAR_VOX_TO_VOX ;
        }

        printf("Applying LTAtransformInterp (resample_type %d)\n",
               resample_type_val);
        if (Gdiag_no > 0) {
          printf
            ("Dumping LTA ---------++++++++++++-----------------------\n");
          LTAdump(stdout,lta_transform);
          printf
            ("========-------------++++++++++++-------------------=====\n");
        }
        mri_transformed =
          LTAtransformInterp(mri, NULL, lta_transform,resample_type_val);
      } // end out_like_flag treatment
			
      if (mri_transformed == NULL) {
        fprintf(stderr, "ERROR: applying transform to volume\n");
        exit(1);
      }
      if(out_center_flag) {
        mri_transformed->c_r = template->c_r ;
        mri_transformed->c_a = template->c_a ;
        mri_transformed->c_s = template->c_s ;
      }
      LTAfree(&lta_transform);
      MRIfree(&mri);
      mri = mri_transformed;
    } else if (transform_type == MORPH_3D_TYPE) {
      printf("Applying morph_3d ...\n");
      // this is a non-linear vox-to-vox transform
      //note that in this case trilinear
      // interpolation is always used, and -rt
      // option has no effect! -xh
      TRANSFORM *tran = TransformRead(transform_fname);
      // check whether the volume to be morphed and the morph have the same dimensions
      if (invert_transform_flag == 0)
        mri_transformed =
          GCAMmorphToAtlas(mri, (GCA_MORPH *)tran->xform, NULL, 0,
                           resample_type_val) ;
      else // invert
      {
        mri_transformed = MRIclone(mri, NULL);
	      // check whether the volume to be morphed and the morph have the same dimensions
        mri_transformed =
          GCAMmorphFromAtlas(mri,
                             (GCA_MORPH *)tran->xform,
                             mri_transformed,
                             SAMPLE_TRILINEAR);
      }
      TransformFree(&tran);
      MRIfree(&mri);
      mri = mri_transformed;
    } else {
      fprintf(stderr, "unknown transform type in file %s\n",
              transform_fname);
      exit(1);
    }
  } else if (out_like_flag) { // flag set but no transform
    // modify direction cosines to use the
    // out-like volume direction cosines
    //
    //   src --> RAS
    //    |       |
    //    |       |
    //    V       V
    //   dst --> RAS   where dst i_to_r is taken from out volume
    MRI *tmp = 0;
    MATRIX *src2dst = 0;

    printf("INFO: transform src into the like-volume: %s\n",
           out_like_name);
    tmp = MRIreadHeader(out_like_name, MRI_VOLUME_TYPE_UNKNOWN);
    mri_transformed =
      MRIalloc(tmp->width, tmp->height, tmp->depth, mri->type);
    if (!mri_transformed) {
      ErrorExit(ERROR_NOMEMORY, "could not allocate memory");
    }
    MRIcopyHeader(tmp, mri_transformed);
    MRIfree(&tmp);
    tmp = 0;
    // just to make sure
    if (mri->i_to_r__)
      mri->i_to_r__ = extract_i_to_r(mri);
    if (mri_transformed->r_to_i__)
      mri_transformed->r_to_i__ = extract_r_to_i(mri_transformed);
    // got the transform
    src2dst = MatrixMultiply(mri_transformed->r_to_i__,
                             mri->i_to_r__, NULL);
    // now get the values (tri-linear)
    MRIlinearTransform(mri, mri_transformed, src2dst);
    MatrixFree(&src2dst);
    MRIfree(&mri);
    mri=mri_transformed;
  }

  /* ----- change type if necessary ----- */
  if(mri->type != template->type && nochange_flag == FALSE) {
    printf("changing data type from %s to %s (noscale = %d)...\n",
           MRItype2str(mri->type),MRItype2str(template->type),no_scale_flag);
    mri2 =
      MRISeqchangeType(mri, template->type, 0.0, 0.999, no_scale_flag);
    if (mri2 == NULL) {
      printf("ERROR: MRISeqchangeType\n");
      exit(1);
    }
    if (conform_flag)
      MRImask(mri2, mri, mri2, 0, 0) ;  // make sure 0 maps to 0
    MRIfree(&mri);
    mri = mri2;
  }

  /* ----- reslice if necessary ----- */
  if (mri->xsize != template->xsize ||
      mri->ysize != template->ysize ||
      mri->zsize != template->zsize ||
      mri->width != template->width ||
      mri->height != template->height ||
      mri->depth != template->depth ||
      mri->x_r != template->x_r ||
      mri->x_a != template->x_a ||
      mri->x_s != template->x_s ||
      mri->y_r != template->y_r ||
      mri->y_a != template->y_a ||
      mri->y_s != template->y_s ||
      mri->z_r != template->z_r ||
      mri->z_a != template->z_a ||
      mri->z_s != template->z_s ||
      mri->c_r != template->c_r ||
      mri->c_a != template->c_a ||
      mri->c_s != template->c_s) {
    printf("Reslicing using ");
    switch (resample_type_val) {
    case SAMPLE_TRILINEAR:
      printf("trilinear interpolation \n");
      break;
    case SAMPLE_NEAREST:
      printf("nearest \n");
      break;
    case SAMPLE_SINC:
      printf("sinc \n");
      break;
    case SAMPLE_CUBIC:
      printf("cubic \n");
      break;
    case SAMPLE_WEIGHTED:
      printf("weighted \n");
      break;
    }
    mri2 = MRIresample(mri, template, resample_type_val);
    if(mri2 == NULL) exit(1);
    MRIfree(&mri);
    mri = mri2;
  }

  /* ----- invert contrast if necessary ----- */
  if(invert_val >= 0) {
    printf("inverting contrast...\n");
    mri2 = MRIinvertContrast(mri, NULL, invert_val);
    if (mri2 == NULL)
      exit(1);
    MRIfree(&mri);
    mri = mri2;
  }

  /* ----- reorder if necessary ----- */
  if (reorder_flag) {
    printf("reordering axes...\n");
    mri2 = MRIreorder(mri, NULL, reorder_vals[0], reorder_vals[1],
                      reorder_vals[2]);
    if (mri2 == NULL) {
      fprintf(stderr, "error reordering axes\n");
      exit(1);
    }
    MRIfree(&mri);
    mri = mri2;
  }

  /* ----- reorder if necessary ----- */
  if (reorder4_flag) {
    printf("reordering all axes...\n");
    printf("reordering, values are %d %d %d %d\n",
	   reorder4_vals[0], reorder4_vals[1], reorder4_vals[2], reorder4_vals[3]);
    mri2 = MRIreorder4(mri, reorder4_vals);
    if(mri2 == NULL) {
      fprintf(stderr, "error reordering all axes\n");
      exit(1);
    }
    MRIfree(&mri);
    mri = mri2;
  }

  /* ----- store the gdf file stem ----- */
  strcpy(mri->gdf_image_stem, gdf_image_stem);

  /* ----- catch the out info flag ----- */
  if (out_info_flag) {
    printf("output structure:\n");
    MRIdump(mri, stdout);
  }

  /* ----- catch the out matrix flag ----- */
  if (out_matrix_flag) {
    MATRIX *i_to_r;
    i_to_r = extract_i_to_r(mri);
    if (i_to_r != NULL) {
      printf("output ijk -> ras:\n");
      MatrixPrint(stdout, i_to_r);
      MatrixFree(&i_to_r);
    } else
      printf("error getting output matrix\n");
  }

  /* ----- catch the out stats flag ----- */
  if (out_stats_flag) MRIprintStats(mri, stdout);

  if(frame_flag == TRUE) {
    if(mid_frame_flag == TRUE) frame = nint(mri->nframes/2);
    printf("keeping frame %d\n",frame);
    mri2 = fMRIframe(mri, frame, NULL);
    if (mri2 == NULL) exit(1);
    MRIfree(&mri);
    mri = mri2;
  }

  /* ----- drop the last ndrop frames (drop before skip) ------ */
  if (ndrop > 0) {
    printf("Dropping last %d frames\n",ndrop);
    if (mri->nframes <= ndrop) {
      printf("   ERROR: can't drop, volume only has %d frames\n",
             mri->nframes);
      exit(1);
    }
    mri2 = fMRIndrop(mri, ndrop, NULL);
    if (mri2 == NULL) exit(1);
    MRIfree(&mri);
    mri = mri2;
  }

  /* ----- skip the first nskip frames ------ */
  if (nskip > 0) {
    printf("Skipping %d frames\n",nskip);
    if (mri->nframes <= nskip) {
      printf("   ERROR: can't skip, volume only has %d frames\n",
             mri->nframes);
      exit(1);
    }
    mri2 = fMRInskip(mri, nskip, NULL);
    if (mri2 == NULL) exit(1);
    MRIfree(&mri);
    mri = mri2;
  }
  if(subsample_flag){
    printf("SubSample: Start = %d  Delta = %d, End = %d\n",
           SubSampStart, SubSampDelta, SubSampEnd);
    mri2 = fMRIsubSample(mri, SubSampStart, SubSampDelta, SubSampEnd, NULL);
    if(mri2 == NULL) exit(1);
    MRIfree(&mri);
    mri = mri2;
  }

  if(cutends_flag == TRUE){
    printf("Cutting ends: n = %d\n",ncutends);
    mri2 = MRIcutEndSlices(mri, ncutends);
    if(mri2 == NULL) exit(1);
    MRIfree(&mri);
    mri = mri2;
  }


  /* ----- crops ---------*/
  if (crop_flag == TRUE) {
    MRI *mri_tmp ;
    int   x0, y0, z0 ;

    x0 = crop_center[0] ;
    y0 = crop_center[1] ;
    z0 = crop_center[2] ;

    x0 -= crop_size[0]/2 ;
    y0 -= crop_size[1]/2 ;
    z0 -= crop_size[2]/2 ;
    mri_tmp = MRIextract(mri, NULL, x0, y0, z0, crop_size[0],
                         crop_size[1], crop_size[2]) ;
    MRIfree(&mri) ;
    mri = mri_tmp ;
  }

  if (!FEQUAL(out_scale_factor,1.0)) {
    printf("scaling output intensities by %2.3f\n", out_scale_factor) ;
    MRIscalarMul(mri, mri, out_scale_factor) ;
  }

  if (sphinx_flag) {
    printf("Changing orientation information to Sphinx\n");
    MRIhfs2Sphinx(mri);
  }

  if(AutoAlign) mri->AutoAlign = AutoAlign ;

  /*------ Finally, write the output -----*/
  if(ascii_flag){
    printf("Writing as ASCII to %s\n",out_name);
    fptmp = fopen(out_name,"w");
    if(ascii_flag == 1 || ascii_flag == 2){
      for(f=0; f < mri->nframes; f++){
	for(s=0; s < mri->depth; s++){
	  for(r=0; r < mri->height; r++){
	    for(c=0; c < mri->width; c++){
	      if(ascii_flag == 1)
		fprintf(fptmp,"%lf \n",MRIgetVoxVal(mri,c,r,s,f));
	      if(ascii_flag == 2)
		fprintf(fptmp,"%3d %3d %3d %3d %lf \n",c,r,s,f,MRIgetVoxVal(mri,c,r,s,f));
	    }
	  }
	}
      }
    }
    if(ascii_flag == 3){
      for(s=0; s < mri->depth; s++){
	for(r=0; r < mri->height; r++){
	  for(c=0; c < mri->width; c++){
	    for(f=0; f < mri->nframes; f++)
	      fprintf(fptmp,"%lf ",MRIgetVoxVal(mri,c,r,s,f));
	    fprintf(fptmp,"\n");
	  }
	}
      }
    }
    fclose(fptmp);
    printf("done\n");
    exit(0);
  }
  if (!no_write_flag) {
    if(! SplitFrames) {
      printf("writing to %s...\n", out_name);
      if (force_out_type_flag) {
        err = MRIwriteType(mri, out_name, out_volume_type);
        if (err != NO_ERROR) {
          printf("ERROR: failure writing %s as volume type %d\n",
                 out_name,out_volume_type);
          exit(1);
        }
      } else {
        err = MRIwrite(mri, out_name);
        if (err != NO_ERROR) {
          printf("ERROR: failure writing %s\n",out_name);
          exit(1);
        }
      }
    }
    else {
      stem = IDstemFromName(out_name);
      ext = IDextensionFromName(out_name);

      printf("Splitting frames, stem = %s, ext = %s\n",stem,ext);
      mri2 = NULL;
      for(i=0; i < mri->nframes; i++){
        mri2 = MRIcopyFrame(mri, mri2, i, 0);
        sprintf(tmpstr,"%s%04d.%s",stem,i,ext);
        printf("%2d %s\n",i,tmpstr);
        err = MRIwrite(mri2, tmpstr);
        if (err != NO_ERROR) {
          printf("ERROR: failure writing %s\n",tmpstr);
          exit(1);
        }
      }
    }
  }
  // free memory
  MRIfree(&mri);
  MRIfree(&template);

  exit(0);

} /* end main() */
/*----------------------------------------------------------------------*/

void get_ints(int argc, char *argv[], int *pos, int *vals, int nvals) {

  char *ep;
  int i;

  if(*pos + nvals >= argc) {
    fprintf(stderr, "\n%s: argument %s expects %d integers; "
            "only %d arguments after flag\n",
            Progname, argv[*pos], nvals, argc - *pos - 1);
    usage_message(stdout);
    exit(1);
  }

  for (i = 0;
       i < nvals;
       i++) {
    if (argv[*pos+i+1][0] == '\0') {
      fprintf(stderr, "\n%s: argument to %s flag is null\n",
              Progname, argv[*pos]);
      usage_message(stdout);
      exit(1);
    }

    vals[i] = (int)strtol(argv[*pos+i+1], &ep, 10);

    if (*ep != '\0') {
      fprintf(stderr, "\n%s: error converting \"%s\" to an "
              "integer for %s flag\n",
              Progname, argv[*pos+i+1], argv[*pos]);
      usage_message(stdout);
      exit(1);
    }

  }

  *pos += nvals;

} /* end get_ints() */

void get_floats(int argc, char *argv[], int *pos, float *vals, int nvals) {

  char *ep;
  int i;

  if (*pos + nvals >= argc) {
    fprintf(stderr, "\n%s: argument %s expects %d floats; "
            "only %d arguments after flag\n",
            Progname, argv[*pos], nvals, argc - *pos - 1);
    usage_message(stdout);
    exit(1);
  }

  for (i = 0;i < nvals;i++) {
    if (argv[*pos+i+1][0] == '\0') {
      fprintf(stderr, "\n%s: argument to %s flag is null\n",
              Progname, argv[*pos]);
      usage_message(stdout);
      exit(1);
    }

    vals[i] = (float)strtod(argv[*pos+i+1], &ep);

    if (*ep != '\0') {
      fprintf(stderr, "\n%s: error converting \"%s\" to a "
              "float for %s flag\n",
              Progname, argv[*pos+i+1], argv[*pos]);
      usage_message(stdout);
      exit(1);
    }

  }

  *pos += nvals;

} /* end get_floats() */

void get_string(int argc, char *argv[], int *pos, char *val) {

  if (*pos + 1 >= argc) {
    fprintf(stderr, "\n%s: argument %s expects an extra argument; "
            "none found\n", Progname, argv[*pos]);
    usage_message(stdout);
    exit(1);
  }

  strcpy(val, argv[*pos+1]);

  (*pos)++;

} /* end get_string() */

void usage_message(FILE *stream) {

  fprintf(stream, "\n");
  fprintf(stream, "type %s -u for usage\n", Progname);
  fprintf(stream, "\n");

} /* end usage_message() */


void usage(FILE *stream) {

  fprintf(stream, "\n");
  fprintf(stream, "usage: %s [options] <in volume> <out volume>\n",
          Progname);
  fprintf(stream, "\n");
  fprintf(stream, "options are:\n");
  fprintf(stream, "\n");
  fprintf(stream, "  -ro, --read_only\n");
  fprintf(stream, "  -nw, --no_write\n");
  fprintf(stream, "\n");
  fprintf(stream, "  -ii, --in_info\n");
  fprintf(stream, "  -oi, --out_info\n");
  fprintf(stream, "  -is, --in_stats\n");
  fprintf(stream, "  -os, --out_stats\n");
  fprintf(stream, "  -im, --in_matrix\n");
  fprintf(stream, "  -om, --out_matrix\n");
  fprintf(stream, "\n");
  fprintf(stream, "  -iis, --in_i_size <size>\n");
  fprintf(stream, "  -ijs, --in_j_size <size>\n");
  fprintf(stream, "  -iks, --in_k_size <size>\n");
  fprintf(stream, "  --force_ras_good : "
          "use default when orientation info absent\n");
  fprintf(stream, "\n");
  fprintf(stream, "  -iid, --in_i_direction "
          "<R direction> <A direction> <S direction>\n");
  fprintf(stream, "  -ijd, --in_j_direction "
          "<R direction> <A direction> <S direction>\n");
  fprintf(stream, "  -ikd, --in_k_direction "
          "<R direction> <A direction> <S direction>\n");
  fprintf(stream, "  --in_orientation orientation-string "
          ": see SPECIFYING THE ORIENTATION\n");
  fprintf(stream, "\n");
  fprintf(stream, "  -ic, --in_center "
          "<R coordinate> <A coordinate> <S coordinate>\n");
  fprintf(stream, "\n");
  fprintf(stream, "  --sphinx : change orientation info to sphinx");
  fprintf(stream, "  -oni, -oic, --out_i_count <count>\n");
  fprintf(stream, "  -onj, -ojc, --out_j_count <count>\n");
  fprintf(stream, "  -onk, -okc, --out_k_count <count>\n");
  fprintf(stream, "  -vs, --voxsize <size_x> <size_y> <size_z> : specify the size (mm) - useful for upsampling or downsampling\n");
  fprintf(stream, "\n");
  fprintf(stream, "  -ois, --out_i_size <size>\n");
  fprintf(stream, "  -ojs, --out_j_size <size>\n");
  fprintf(stream, "  -oks, --out_k_size <size>\n");
  fprintf(stream, "\n");
  fprintf(stream, "  -oid, --out_i_direction "
          "<R direction> <A direction> <S direction>\n");
  fprintf(stream, "  -ojd, --out_j_direction "
          "<R direction> <A direction> <S direction>\n");
  fprintf(stream, "  -okd, --out_k_direction "
          "<R direction> <A direction> <S direction>\n");
  fprintf(stream, "  --out_orientation orientation-string "
          ": see SETTING ORIENTATION\n");
  fprintf(stream, "\n");
  fprintf(stream, "  -oc, --out_center "
          "<R coordinate> <A coordinate> <S coordinate>\n");
  fprintf(stream, "\n");
  fprintf(stream, "  -odt, --out_data_type <uchar|short|int|float>\n");
  fprintf(stream, "\n");
  fprintf(stream, "  -rt, --resample_type "
          "<interpolate|weighted|nearest|sinc|cubic> "
          "(default is interpolate)\n");
  fprintf(stream, "\n");
  fprintf(stream, "  --no_scale flag <-ns>: 1 = dont rescale "
          "values for COR\n");
  fprintf(stream, "\n");
  fprintf(stream, "  -nc --nochange - don't change type of "
          "input to that of template\n");
  fprintf(stream, "\n");
  fprintf(stream, "\n");
  fprintf(stream, "  -tr TR : TR in msec\n");
  fprintf(stream, "  -te TE : TE in msec\n");
  fprintf(stream, "  -TI TI : TI in msec (note upper case flag)\n");
  fprintf(stream, "  -flip_angle flip angle : radians \n");
  fprintf(stream, "  --autoalign mtxfile : text file with autoalign matrix \n");

  fprintf(stream, "\n");
  fprintf(stream, "  --unwarp_gradient_nonlinearity \n"
          "      <sonata | allegra | GE> \n"
          "      <fullUnwarp | through-plane> \n"
          "      <JacobianCorrection | noJacobianCorrection> \n"
          "      <linear | sinc> \n"
          "      <sincInterpHW>  \n");
  fprintf(stream, "\n");
  fprintf(stream, "APPLYING TRANSFORMS (INCLUDING TALAIRACH)\n");
  fprintf(stream, "\n");
  fprintf(stream, "  --apply_transform xfmfile (-T or -at)\n");
  fprintf(stream, "  --apply_inverse_transform xfmfile (-ait)\n");
  fprintf(stream, "  --devolvexfm subjectid : \n");
  fprintf(stream, "  --crop <x> <y> <z> crop to 256 around "
          "center (x,y,z) \n");
  fprintf(stream, "  --cropsize <dx> <dy> <dz> crop to size "
          "<dx, dy, dz> \n");
  fprintf(stream, "  --cutends ncut : remove ncut slices from the ends\n");
  fprintf(stream, "  --slice-crop s_start s_end : keep slices s_start to s_end\n");
  fprintf(stream, "  --slice-reverse : reverse order of slices, update vox2ras\n");
  fprintf(stream, "  --slice-bias alpha : apply half-cosine bias field\n");

  fprintf(stream, "  --fwhm fwhm : smooth input volume by fwhm mm\n ");
  fprintf(stream, "\n");

  fprintf(stream,
          " The volume can be resampled into another space "
          "by supplying a \n"
          " transform using the --apply_transform flag. This reads the \n"
          " transform file and applies the transform "
          "(when --apply_inverse_transform \n"
          " is used, the transform is inverted and then applied). "
          "An example of a \n"
          " transform file is talairach.xfm as found in "
          "subjectid/mri/transforms. To \n"
          " convert a subject's orig volume to "
          "talairach space, execute the \n"
          " following lines:\n"
          "\n"
          "  cd subjectid/mri\n"
          "  mkdir talairach\n"
          "  mri_convert orig.mgz --apply_transform "
          "transforms/talariach.xfm \n"
          "     -oc 0 0 0   orig.talairach.mgz\n"
          "\n"
          " This properly accounts for the case where the input "
          "volume does not"
          " have it's coordinate center at 0.\n"
          "\n"
          " To evaluate the result, run:\n"
          "\n"
          "    tkmedit -f $SUBJECTS_DIR/talairach/mri/orig.mgz "
          "       -aux orig.talairach.mgz\n"
          "\n"
          "The main and aux volumes should overlap very closely. "
          "If they do not, \n"
          " use tkregister2 to fix it (run tkregister --help for docs).\n"
          "\n");

  fprintf(stream,

          "\n"
          "SPECIFYING THE INPUT AND OUTPUT FILE TYPES\n"
          "\n"
          "The file type can be specified in two ways. First, "
          "mri_convert will try \n"
          "to figure it out on its own from the format "
          "of the file name (eg, files that\n"
          "end in .img are assumed to be in spm analyze format). "
          "Second, the user can \n"
          "explicity set the type of file using --in_type "
          "and/or --out_type.\n"
          "\n"
          "Legal values for --in_type (-it) and --out_type (-ot) are:\n"
          "\n"
          "  cor           - MGH-NMR COR format (deprecated)\n"
          "  mgh           - MGH-NMR format\n"
          "  mgz           - MGH-NMR gzipped (compressed) mgh format\n"
          "  minc          - MNI's Medical Imaging NetCDF format "
          "(output may not work)\n"
          "  analyze       - 3D analyze (same as spm)\n"
          "  analyze4d     - 4D analyze \n"
          "  spm           - SPM Analyze format "
          "(same as analyze and analyze3d)\n"
          "  ge            - GE Genesis format (input only)\n"
          "  gelx          - GE LX (input only)\n"
          "  lx            - same as gelx\n"
          "  ximg          - GE XIMG variant (input only)\n"
          "  siemens       - Siemens IMA (input only)\n"
          "  dicom         - generic DICOM Format (input only)\n"
          "  siemens_dicom - Siemens DICOM Format (input only)\n"
          "  afni          - AFNI format\n"
          "  brik          - same as afni\n"
          "  bshort        - MGH-NMR bshort format\n"
          "  bfloat        - MGH-NMR bfloat format\n"
          "  sdt           - Varian (?)\n"
          "  outline       - MGH-NMR Outline format\n"
          "  otl           - same as outline\n"
          "  gdf           - GDF volume (requires image stem "
          "for output; use -gis)\n"
          "  nifti1        - NIfTI-1 volume (separate image "
          "and header files)\n"
          "  nii           - NIfTI-1 volume (single file)\n"
          "    if the input/output has extension .nii.gz, "
          "then compressed is used\n"
          "\n"
          "CONVERTING TO SPM-ANALYZE FORMAT \n"
          "\n"
          "Converting to SPM-Analyze format can be done in two ways, "
          "depending upon\n"
          "whether a single frame or multiple frames are desired. "
          "For a single frame,\n"
          "simply specify the output file name with a .img extension, "
          "and mri_convert \n"
          "will save the first frame into the file.  "
          "For multiple frames, specify the \n"
          "base as the output file name and add --out_type spm. "
          "This will save each \n"
          "frame as baseXXX.img where XXX is the three-digit, "
          "zero-padded frame number.\n"
          "Frame numbers begin at one. By default, the width "
          "the of zero padding is 3.\n"
          "This can be controlled with --in_nspmzeropad N where "
          "N is the new width.\n"
          "\n"  );

  printf("--ascii : save output as ascii. This will be a data file with a single \n");
  printf("    column of data. The fastest dimension will be col, then row, \n");
  printf("    then slice, then frame.\n");
  printf("--ascii+crsf : same as --ascii but includes col row slice and frame\n");
  printf("--ascii-fcol : same as --ascii but each frame is a separate column in output\n");
  printf("\n");
  printf("Other options\n");
  printf("\n");
  printf("  -r, --reorder  olddim1 olddim2 olddim3\n");
  printf("  -r4,--reorder4 olddim1 olddim2 olddim3 olddim4\n");
  printf("\n");
  printf("  Reorders axes such that olddim1 is the new column dimension,\n");
  printf("  olddim2 is the new row dimension, olddim3 is the new slice, and \n");
  printf("  olddim4 is the new frame dimension.\n");
  printf("     Example: 2 1 3 will swap rows and cols.\n");
  printf("  If using -r4, the output geometry will likely be wrong. It is best\n");
  printf("  to re-run mri_convert and specify a correctly oriented volume through \n");
  printf("  the --in_like option.\n");
  printf("\n");
  printf("  --invert_contrast threshold\n");
  printf("\n");
  printf("  All voxels in volume greater than threshold are replaced\n");
  printf("  with 255-value. Only makes sense for 8 bit images.\n");
  printf("  Only operates on the first frame.\n");
  printf("\n");
  printf("  -i, --input_volume\n");
  printf("  -o, --output_volume\n");
  printf("  -c, --conform\n");
  printf("            conform to 1mm voxel size in coronal "
         "slice direction with 256^3 or more\n");
  printf("  -cm, --conform_min\n");
  printf("            conform to the src min direction size\n");
  printf("  -cs, --conform_size size_in_mm\n");
  printf("            conform to the size given in mm\n");
  printf("  -po, --parse_only\n");
  printf("  -is, --in_stats\n");
  printf("  -os, --out_stats\n");
  printf("  -ro, --read_only\n");
  printf("  -nw, --no_write\n");
  printf("  -sn, --subject_name\n");
  printf("  -rl, --reslice_like\n");
  printf("  -tt, --template_type <type> (see above)\n");
  printf("  --split : split output frames into separate output files.\n");
  printf("    Example: mri_convert a.nii b.nii --split will\n");
  printf("    create b0000.nii b0001.nii b0002.nii ...\n");
  printf("  -f,  --frame frameno : keep only 0-based frame number\n");
  printf("  --mid-frame : keep only the middle frame\n");
  printf("  --nskip n : skip the first n frames\n");
  printf("  --ndrop n : drop the last n frames\n");
  printf("  --fsubsample start delta end : frame subsampling (end = -1 for end)\n");
  printf("  -sc, --scale factor : input intensity scale factor\n") ;
  printf("  -osc, --out-scale factor : output intensity scale factor\n") ;
  printf("  -il, --in_like\n");
  printf("  -roi\n");
  printf("  -fp, --fill_parcellation\n");
  printf("  -sp, --smooth_parcellation\n");
  printf("  -zo, --zero_outlines\n");
  printf("  -cf, --color_file\n");
  printf("  -nt, --no_translate\n");
  printf("  --status (status file for DICOM conversion)\n");
  printf("  --sdcmlist (list of DICOM files for conversion)\n");
  printf("  -ti, --template_info : dump info about template\n");
  printf("  -gis <gdf image file stem>\n");
  printf("  -cg, --crop_gdf: apply GDF cropping\n");
  printf("  -zgez, --zero_ge_z_offset\n");
  printf("            set c_s=0 (appropriate for dicom "
         "files from GE machines with isocenter scanning)\n");
  //E/  printf("  -nozgez, --no_zero_ge_z_offset\n");
  //E/  printf("            don't set c_s=0, even if a GE volume\n");
  printf("\n");
  printf(
    "SPECIFYING THE ORIENTATION\n"
    "\n"
    "NOTE: DO NOT USE THIS TO TRY TO CHANGE THE ORIENTATION FOR FSL.\n"
    "THIS IS ONLY TO BE USED WHEN THE ORIENTATION INFORMATION IN\n"
    "THE INPUT FILE IS *WRONG*. IF IT IS CORRECT, THIS WILL MAKE\n"
    "IT WRONG! IF YOU ARE HAVING PROBLEMS WITH FSLVIEW DISPLAYING\n"
    "YOUR DATA, CONSULT THE FSL WEBSITE FOR METHODS TO REORIENT.\n"
    "\n"
    "Ideally, the orientation information is derived from a "
    "DICOM file so that\n"
    "you have some confidence that it is correct. "
    "It is generally pretty easy\n"
    "to determine which direction Anterior/Posterior or "
    "Inferior/Superior are.\n"
    "Left/Right is very difficult. However, if you have "
    "some way of knowing which\n"
    "direction is which, you can use these options to "
    "incorporate this information \n"
    "into the header of the output format. For analyze files, "
    "it will be stored in\n"
    "the output.mat file. For NIFTI, it is stored as the "
    "qform matrix. For bshort/ \n"
    "bfloat, it is stored in the .bhdr file. "
    "For mgh/mgz it is internal.\n"
    "\n"
    "First of all, determining and setting the orientation "
    "is hard. Don't fool yourself\n"
    "into thinking otherwise. Second, don't think you are "
    "going to learn all you need\n"
    "to know from this documentation. Finally, you risk "
    "incorporating a left-right\n"
    "flip in your data if you do it incorrectly. OK, "
    "there are two ways to specify this \n"
    "information on the command-line. "
    "(1) explicitly specify the direction cosines\n"
    "with -iid, -ijd, -ikd. If you don't know what "
    "a direction cosine is, don't use\n"
    "this method. Instead, (2) specify an orientation string. \n"
    "\n"
    "--in_orientation  ostring\n"
    "--out_orientation ostring\n"
    "  \n"
    "  Supply the orientation information in the form "
    "of an orientation string \n"
    "  (ostring). The ostring is three letters that "
    "roughly describe how the volume\n"
    "  is oriented. This is usually described by the direction "
    "cosine information\n"
    "  as originally derived from the dicom but might not "
    "be available in all data\n"
    "  sets. You'll have to determine the correct "
    "ostring for your data.\n"
    "  The first  character of ostring determines the "
    "direction of increasing column.\n"
    "  The second character of ostring determines the "
    "direction of increasing row.\n"
    "  The third  character of ostring determines the "
    "direction of increasing slice.\n"
    "  Eg, if the volume is axial starting inferior and "
    "going superior the slice \n"
    "  is oriented such that nose is pointing up and "
    "the right side of the subject\n"
    "  is on the left side of the image, then this "
    "would correspond to LPS, ie,\n"
    "  as the column increases, you move to the patients left; "
    "as the row increases,\n"
    "  you move posteriorly, and as the slice increases, "
    "you move superiorly. Valid\n"
    "  letters are L, R, P, A, I, and S. There are 48 "
    "valid combinations (eg, RAS\n"
    "  LPI, SRI). Some invalid ones are DPS (D is not a "
    "valid letter), RRS (can't\n"
    "  specify R twice), RAP (A and P refer to the same axis). "
    "Invalid combinations\n"
    "  are detected immediately, an error printed, "
    "and the program exits. Case-insensitive.\n"
    "  Note: you can use tkregister2 to help determine "
    "the correct orientation string.\n"
    "\n"
    "--sphinx\n"
    "\n"
    "reorient to sphinx the position. This function is applicable when \n"
    "the input geometry information is correct but the  subject was in the \n"
    "scanner in the 'sphinx' position "
    "(ie, AP in line with the bore) instead \n"
    "of head-first-supine (HFS). This is often the  case with monkeys.\n"
    "Note that the assumption is that the geometry information in the input\n"
    "file is otherwise accurate.\n"
    "\n"
    );

  printf("\n");
  printf("Notes: \n");
  printf("\n");
  printf("If the user specifies any of the direction "
         "cosines or orientation string,\n");
  printf("the ras_good_flag is set.\n");
  printf("\n");
#endif // GREGT

} /* end usage() */

float findMinSize(MRI *mri, int *conform_width) {
  double xsize, ysize, zsize, minsize;
  double fwidth, fheight, fdepth, fmax;
  xsize = mri->xsize;
  ysize = mri->ysize;
  zsize = mri->zsize;
  // there are 3! = 6 ways of ordering
  //             xy  yz  zx
  // x > y > z    z min
  // x > z > y    y min
  // z > x > y    y min
  //////////////////////////
  // y > x > z    z min
  // y > z > x    x min
  // z > y > x    x min
  if (xsize > ysize)
    minsize = (ysize > zsize) ? zsize : ysize;
  else
    minsize = (zsize > xsize) ? xsize : zsize;

  // now decide the conformed_width
  // algorighm ----------------------------------------------
  // calculate the size in mm for all three directions
  fwidth = mri->xsize*mri->width;
  fheight = mri->ysize*mri->height;
  fdepth = mri->zsize*mri->depth;
  // pick the largest
  if (fwidth> fheight)
    fmax = (fwidth > fdepth) ? fwidth : fdepth;
  else
    fmax = (fdepth > fheight) ? fdepth : fheight;

  *conform_width = (int) ceil(fmax/minsize);
  // just to make sure that if smaller than 256, use 256 anyway
  if (*conform_width < 256)
    *conform_width = 256;

  return (float) minsize;
}
/* EOF */

// this function is called when conform is done
int findRightSize(MRI *mri, float conform_size) {
  // user gave the conform_size
  double xsize, ysize, zsize;
  double fwidth, fheight, fdepth, fmax;
  int conform_width;

  xsize = mri->xsize;
  ysize = mri->ysize;
  zsize = mri->zsize;

  // now decide the conformed_width
  // calculate the size in mm for all three directions
  fwidth = mri->xsize*mri->width;
  fheight = mri->ysize*mri->height;
  fdepth = mri->zsize*mri->depth;
  // pick the largest
  if (fwidth> fheight)
    fmax = (fwidth > fdepth) ? fwidth : fdepth;
  else
    fmax = (fdepth > fheight) ? fdepth : fheight;
  // get the width with conform_size
  conform_width = (int) ceil(fmax/conform_size);

  // just to make sure that if smaller than 256, use 256 anyway
  if (conform_width < 256)
    conform_width = 256;
  // conform_width >= 256.   allow 10% leeway
  else if ((conform_width -256.)/256. < 0.1)
    conform_width = 256;

  // if more than 256, warn users
  if (conform_width > 256) {
    fprintf(stderr, "WARNING =================="
            "++++++++++++++++++++++++"
            "=======================================\n");
    fprintf(stderr, "The physical sizes are "
            "(%.2f mm, %.2f mm, %.2f mm), "
            "which cannot fit in 256^3 mm^3 volume.\n",
            fwidth, fheight, fdepth);
    fprintf(stderr, "The resulting volume will have %d slices.\n",
            conform_width);
    fprintf(stderr, "If you find problems, please let us know "
            "(freesurfer@nmr.mgh.harvard.edu).\n");
    fprintf(stderr, "=================================================="
            "++++++++++++++++++++++++"
            "===============\n\n");
  }
  return conform_width;
}
