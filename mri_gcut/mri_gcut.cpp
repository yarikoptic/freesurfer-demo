/**
 * @file  mri_gcut.cpp
 * @input T1.mgz; output brainmask.auto.mgz
 *
 * the main idea of the program:
 * the program first estimates the white matter mask, then does a graph cut.
 *
 * two ways to set white matter mask:
 * you can either use the program to estimate the white matter for you,
 * or you can use intensity 110 (T1 image from FreeSurfer) voxels as 
 * the white matter mask.
 *
 * the input and output:
 * input and output files are in .mgz format. 
 * the output file is "brainmask_auto.mgz".
 * if "-mult" is applied, the original "brainmask_auto.mgz" will be saved as 
 * "brainmask_auto_old.mgz".
 *
 * expected memory requirements:
 * the memory needed to process a standard 256*256*256 .mgz file is
 * about 1GB ~ 1.5GB.
 * 
 * usage:
 * ./mri_gcut [-110|-mult|-T (value)] input_filename
 * -110: use voxels with intensity 110 as white matter mask (FreeSurfer only)
 * -mult: apply existing "brainmask.auto.mgz"; when this option is present and
 *        there is already an existing "brainmask_auto.mgz" in the same folder,
 *        then program load the existing mask, binarize graph cutted mask and 
 *        multiply the two together. the old mask will be saved as 
 *        "brainmask_auto_old.mgz".
 * -T (value): set threshold to value% of white matter intensity, 
 *             value should be >0 and <1;
 *             larger values would correspond to cleaner skull strip but 
 *             higher chance of brain erosion.
 *
 * Notes:
 * 1) If parameter -110 is chosen but the largest connected component of 110 
 * intensity voxels is too small to be used as front seed, then we default on 
 * our own region growing procedure that is initiated within the largest
 * connected component. 
 * 2) If the volume of gcut mask is less than 75% of hwa mask and -mult option
 * is present, then gcut result is ignored and hwa mask is produced at the
 * output. This is to prevent occasional catastrophic results.
 *
 */
/*
 * Original Authors: Vitali Zagorodnov, ZHU Jiaqi
 * CVS Revision Info: 1.4
 *    $Author: Vitali Zagorodnov, ZHU Jiaqi
 *    $Date: Feb. 2010
 *    $Revision: 1.4
 *
 * Copyright (C) 2009-2010
 * Nanyang Technological University, Singapore
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
#include <math.h>

extern "C"
{
#include "mri.h"
#include "error.h"
#include "diag.h"
#include "version.h"
#include "utils.h"
}

#include "pre_pro.cpp"
#include "graphcut.cpp"

const char *Progname;
static char vcid[] =
  "$Id: mri_gcut.cpp,v 1.7.2.2 2010/12/08 14:16:10 nicks Exp $";
static char in_filename[STRLEN];
static char out_filename[STRLEN];
static char mask_filename[STRLEN];
static char diff_filename[STRLEN];
static bool bNeedPreprocessing = 1;
static bool bNeedMasking = 0;
static double _t = 0.40;

static const char help[] =
  {
    "Usage:\n"
    "\n"
    "./mri_gcut [-110|-mult <filename>|-T <value>] in_filename out_filename [diff_filename]\n"
    "\n"
    "  in_filename - name of the file that contains brain volume, eg. T1.mgz\n"
    "\n"
    "  out_filename - name given to output file, eg. brainmask.auto.mgz\n"
    "\n"
    "  diff_filename - output file containing cuts made, eg. brainmask.gcuts.mgz\n"
    "\n"
    "  -110: use voxels with intensity 110 as white matter mask\n"
    "  (when applied on T1.mgz, FreeSurfer only)\n"
    "\n"
    "  -mult <filename>: If there exists a skull-stripped volume specified\n"
    "  by the filename arg, such as that created by 'mri_watershed', the\n"
    "  skull-stripped 'in_filename' and 'filename' will be intersected\n"
    "  and the intersection stored in 'out_filename'. This approach will\n"
    "  produce cleaner skull-strip, that is less likely to result in\n"
    "  subsequent pial surface overgrowth, see [1].\n"
    "\n"
    "  -T <value>: set threshold to value (%) of WM intensity, the value\n"
    "  should be >0 and <1; larger values would correspond to cleaner\n"
    "  skull-strip but higher chance of brain erosion. Default is set\n"
    "  conservatively at 0.40, which provide approx. the same negligible\n"
    "  level of brain erosion as 'mri_watershed'.\n"
    "\n"
    "The memory needed to process a standard 256*256*256 .mgz file\n"
    "is about 1GB ~ 1.5GB.\n"
    "\n"
    "Description:\n"
    "Skull stripping algorithm based on graph cuts.\n"
    "\n"
    "The algorithm consists of four main steps. In step 1, a conservative\n"
    "white matter (WM) mask is estimated using region growing. In step 2,\n"
    "the image is thresholded at the level proportional to intensity of WM,\n"
    "where the latter is estimated by averaging voxels with WM mask\n"
    "obtained in step 1. The thresholded image will contain brain and\n"
    "non-brain structures connected to each other by a set of (hopefully)\n"
    "narrow connections. In step 3, an undirected graph is defined on\n"
    "the image and subsequently partitioned into two portions using\n"
    "graph cuts approach. In the last step, post processing is applied\n"
    "to regain CSF and partial volume voxels that were lost during\n"
    "thresholding. For more details, see [1]\n"
    "\n"
    "When the algorithm is applied in the context of FreeSurfer pipeline,\n"
    "eg. T1.mgz, one can choose to use voxels with intensity 110 in T1.mgz\n"
    "as the WM mask, rather than estimating the mask from the image by\n"
    "region growing.\n"
    "\n"
    "[1] S.A. Sadananthan, W. Zheng, M.W.L. Chee, and V. Zagorodnov,\n"
    "'Skull Stripping Using Graph Cuts', Neuroimage, 2009\n"
  };

bool matrix_alloc(int ****pointer, int z, int y, int x)
{
  (*pointer) = new int**[z];
  for (int i = 0; i < z; i++)
  {
    (*pointer)[i] = new int*[y];
    for (int j = 0; j < y; j++)
    {
      (*pointer)[i][j] = new int[x];
      for (int k = 0; k < x; k++)
      {
        (*pointer)[i][j][k] = 0;
      }
    }
  }
  return 1;
}

bool matrix_free(int ***pointer, int z, int y, int x)
{
  // -- free memory
  for (int i = 0; i < z; i++)
  {
    for (int j = 0; j < y; j++)
    {
      delete[] pointer[i][j];
    }
    delete[] pointer[i];
  }
  delete[] pointer;
  return 1;
}

static void print_help(void)
{
  char *name= (char*)malloc(strlen(Progname));
  strcpy(name,Progname);
  free(name);
  
  printf("%s",help);

  exit(1);
}

/* --------------------------------------------- */
static void print_version(void)
{
  printf("%s\n", vcid) ;
  exit(1) ;
}

/* --------------------------------------------- */
static int parse_commandline(int argc, char **argv)
{
  int  nargc , nargsused;
  char **pargv, *option ;

  if (argc < 3)
  {
    printf("\nMissing arguments!\n\n");
    print_help();
  }

  // skip first arg (the name of the program)
  argc--;
  argv++;

  nargc = argc;
  pargv = argv;
  while (nargc > 0)
  {
    option = pargv[0];

    nargc -= 1;
    pargv += 1;

    nargsused = 0;

    if (!strcasecmp(option, "--help")||
        !strcasecmp(option, "--usage")) print_help() ;
    else if (!strcasecmp(option, "--version")) print_version() ;
    else if (!strcmp(option, "-110") || !strcmp(option, "--110"))
    {
      bNeedPreprocessing = 0;
    }
    else if (!strcmp(option, "-mult") ||
             !strcmp(option, "--mult") ||
             !strcmp(option, "--mask"))
    {
      bNeedMasking = 1;
      strcpy(mask_filename, pargv[0]);
      nargsused = 1;
    }
    else if (!strcmp(option, "-T"))
    {
      _t = atof(pargv[0]);
      if ( _t <= 0 || _t >= 1 )
      {
        printf("-T (value): value range (0 ~ 1) !\n");
        exit(1);
      }
      nargsused = 1;
    }
    else
    {
      if (option[0] == '-')
      {
        printf("\n%s: unknown flag \"%s\"\n", Progname, option);
        print_help();
        exit(1);
      }
      else
      {
        if (in_filename[0] == '\0')
        {
          strcpy(in_filename, option);
        }
        else if (out_filename[0] == '\0')
        {
          strcpy(out_filename, option);
        }
        else if (diff_filename[0] == '\0')
        {
          strcpy(diff_filename, option);
        }
        else
        {
          printf("Error: extra arguments!\n\n");
          print_help();
          exit(1);
        }
      }
    }
    nargc -= nargsused;
    pargv += nargsused;
  }
  return(0);
}


/*-------------------------------------------------------------*/
int main(int argc, char *argv[])
{
  /* check for and handle version tag */
  int nargs = handle_version_option
              (argc, argv,
               "$Id: mri_gcut.cpp,v 1.7.2.2 2010/12/08 14:16:10 nicks Exp $",
               "$Name:  $");
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  Progname = argv[0] ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  parse_commandline(argc, argv);

  MRI *mri, *mri2, *mri3, *mri_mask=NULL;
  mri3  = MRIread(in_filename);
  if ( mri3 == NULL )
  {
    printf("can't read file %s\nexit!\n", in_filename);
    exit(0);
  }
  mri   = MRISeqchangeType(mri3, MRI_UCHAR, 0.0, 0.999, FALSE);
  mri2  = MRISeqchangeType(mri3, MRI_UCHAR, 0.0, 0.999, FALSE);
  //MRI* mri4 = MRIread("gcutted.mgz");

  if (bNeedMasking == 1)
  {
    printf("reading mask...\n");
    mri_mask = MRIread(mask_filename);
    if ( mri_mask == NULL )
    {
      printf("can't read %s, omit -mult option!\n", mask_filename);
      print_help();
      exit(1);
    }
    else
    {
      if ( mri_mask->width != mri->width ||
           mri_mask->height != mri->height ||
           mri_mask->depth != mri->depth )
      {
        printf("Two masks are of different size, omit -mult option!\n");
        print_help();
        exit(1);
      }
    }
  }

  int w = mri->width;
  int h = mri->height;
  int d = mri->depth;

  // -- copy of mri matrix
  unsigned char ***label;
  label = new unsigned char**[d];
  for (int i = 0; i < d; i++)
  {
    label[i] = new unsigned char*[h];
    for (int j = 0; j < h; j++)
    {
      label[i][j] = new unsigned char[w];
      for (int k = 0; k < w; k++)
      {
        label[i][j][k] = 0;
      }
    }
  }
  // -- gcut image
  int ***im_gcut;
  im_gcut = new int**[d];
  for (int i = 0; i < d; i++)
  {
    im_gcut[i] = new int*[h];
    for (int j = 0; j < h; j++)
    {
      im_gcut[i][j] = new int[w];
      for (int k = 0; k < w; k++)
      {
        im_gcut[i][j][k] = 0;
      }
    }
  }
  // -- diluted
  int ***im_diluteerode;
  im_diluteerode = new int**[d];
  for (int i = 0; i < d; i++)
  {
    im_diluteerode[i] = new int*[h];
    for (int j = 0; j < h; j++)
    {
      im_diluteerode[i][j] = new int[w];
      for (int k = 0; k < w; k++)
      {
        im_diluteerode[i][j][k] = 0;
      }
    }
  }
  //int w, h, d;
  //int x, y, z;
  double whitemean;
  if (bNeedPreprocessing == 0)
  {
    // pre-processed: 110 intensity voxels are the WM
    if (LCC_function(mri ->slices, 
                     label,
                     mri->width,
                     mri->height,
                     mri->depth,
                     whitemean) == 1)
    {
      if ( whitemean < 0 )
      {
        printf("whitemean < 0 error!\n");
        exit(0);
      }
    }
    else
    {
      whitemean = 110;
      printf("use voxels with intensity 110 as WM mask\n");
    }
  }
  else
  {
    printf("estimating WM mask\n");
    whitemean = pre_processing(mri ->slices,
                               label,
                               mri->width,
                               mri->height,
                               mri->depth);
    if ( whitemean < 0 )
    {
      printf("whitemean < 0 error!\n");
      exit(0);
    }
  }
  double threshold = whitemean * _t;
  printf("threshold set to: %f*%f=%f\n", whitemean, _t, threshold);

  for (int z = 0 ; z < mri->depth ; z++)
  {
    for (int y = 0 ; y < mri->height ; y++)
    {
      for (int x = 0 ; x < mri->width ; x++)
      {
        if ( mri->slices[z][y][x] < threshold + 1 )
          mri->slices[z][y][x] = 0;
      }
    }//end of for
  }

  //new code
  int ***foregroundseedwt;
  int ***backgroundseedwt;
  matrix_alloc(&foregroundseedwt, d, h, w);
  matrix_alloc(&backgroundseedwt, d, h, w);

  double kval = 2.3;
  graphcut(mri->slices, label, im_gcut,
           foregroundseedwt, backgroundseedwt,
           w, h, d, kval, threshold, whitemean);
  printf("g-cut done!\npost-processing...\n");
  //printf("_test: %f\n", _test);

  post_processing(mri2->slices,
                  mri->slices,
                  threshold,
                  im_gcut,
                  im_diluteerode,
                  w, h, d);
  printf("post-processing done!\n");

  if (bNeedMasking == 1)//masking
  {
    printf("masking...\n");
    for (int z = 0 ; z < mri_mask->depth ; z++)
    {
      for (int y = 0 ; y < mri_mask->height ; y++)
      {
        for (int x = 0 ; x < mri_mask->width ; x++)
        {
          if ( mri_mask->slices[z][y][x] == 0 )
            im_diluteerode[z][y][x] = 0;
        }
      }
    }
  }

  //if the output might have some problem
  int numGcut = 0, numMask = 0;
  double _ratio = 0;
  int error_Hurestic = 0;
  if (bNeedMasking == 1)//-110 and masking are both set
  {
    for (int z = 0 ; z < mri_mask->depth ; z++)
    {
      for (int y = 0 ; y < mri_mask->height ; y++)
      {
        for (int x = 0 ; x < mri_mask->width ; x++)
        {
          if ( im_diluteerode[z][y][x] != 0 )
            numGcut++;
          if ( mri_mask->slices[z][y][x] != 0 )
            numMask++;
        }
      }
    }
    _ratio = (double)numGcut / numMask;
    if (_ratio <= 0.75)
      error_Hurestic = 1;
  }

  if (error_Hurestic == 1)
  {
    printf("** Gcutted brain is much smaller than the mask!\n");
    printf("** Using the mask as the output instead!\n");
    //printf("** Gcutted output is written as: 'error_gcutted_sample'\n");
  }

  for (int z = 0 ; z < mri->depth ; z++)
  {
    for (int y = 0 ; y < mri->height ; y++)
    {
      for (int x = 0 ; x < mri->width ; x++)
      {
        if (error_Hurestic == 0)
        {
          if ( im_diluteerode[z][y][x] == 0 )
            mri2 ->slices[z][y][x] = 0;
        }
        else
        {
          if ( mri_mask->slices[z][y][x] == 0 )
            mri2 ->slices[z][y][x] = 0;
          //if( im_diluteerode[z][y][x] == 0 )
          //mri ->slices[z][y][x] = 0;
        }
      }
    }//end of for 2
  }//end of for 1

  MRIwrite(mri2, out_filename);

  // if user supplied a filename to which to write diffs, then write-out
  // volume file containing where cuts were made (for debug)
  if (diff_filename[0])
  {
    MRI *mri_diff = MRISeqchangeType(mri3, MRI_UCHAR, 0.0, 0.999, FALSE);
    for (int z = 0 ; z < mri3->depth ; z++)
    {
      for (int y = 0 ; y < mri3->height ; y++)
      {
        for (int x = 0 ; x < mri3->width ; x++)
        {
          if (mri_mask) 
          {
            mri_diff->slices[z][y][x] = 
              mri2->slices[z][y][x] - mri_mask->slices[z][y][x];
          }
          else
          {
            mri_diff->slices[z][y][x] = 
              mri2->slices[z][y][x] - mri3->slices[z][y][x];
          }
        }
      }//end of for 2
    }//end of for 1
    MRIwrite(mri_diff, diff_filename);
    MRIfree(&mri_diff);
  }

  if (mri) MRIfree(&mri);
  if (mri2) MRIfree(&mri2);
  if (mri3) MRIfree(&mri3);
  if (mri_mask) MRIfree(&mri_mask);
  for (int i = 0; i < d; i++)
  {
    for (int j = 0; j < h; j++)
    {
      delete[] im_diluteerode[i][j];
      delete[] im_gcut[i][j];
      delete[] label[i][j];
    }
    delete[] im_diluteerode[i];
    delete[] im_gcut[i];
    delete[] label[i];
  }
  delete[] im_diluteerode;
  delete[] im_gcut;
  delete[] label;

  return 0;
}
