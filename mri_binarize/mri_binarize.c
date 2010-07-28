/**
 * @file  mri_binarize.c
 * @brief binarizes an image
 *
 * Program to binarize a volume (or volume-encoded surface file). Can also
 * be used to merge with other binarizations. Binarization can be done
 * based on threshold or on matched values.
 */
/*
 * Original Author: Douglas N. Greve
 * CVS Revision Info:
 *    $Author: greve $
 *    $Date: 2010/07/28 20:07:29 $
 *    $Revision: 1.23.2.1 $
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


// $Id: mri_binarize.c,v 1.23.2.1 2010/07/28 20:07:29 greve Exp $

/*
  BEGINHELP

Program to binarize a volume (or volume-encoded surface file). Can also
be used to merge with other binarizations. Binarization can be done
based on threshold or on matched values.

--i invol

Input volume to be binarized.

--min min
--max max
--rmin rmin
--rmax rmax

Minimum and maximum thresholds. If the value at a voxel is >= min and
<= max, then its value in the output will be 1 (or --binval),
otherwise it is 0 (or --binvalnot) or the value of the merge volume at
that voxel.  By default, min = -infinity and max = +infinity, but you
must set one of the thresholds. Cannot be used with --match. If --rmin
or --rmax are specified, then min (or max) is computed as rmin (or
rmax) times the global mean of the input.

--pct P

Set min threshold so that the top P percent of the voxels are captured
in the output mask. The percent will be computed based on the number of
voxels in the volume (if not input mask is specified) or within the
input mask.

--match matchvalue <--match matchvalue>

Binarize based on matching values. Any number of match values can be 
specified. Cannot be used with --min/--max.

--o outvol

Path to output volume.

--count countfile

Save number of voxels that meet match criteria in ascii
countefile. Four numbers are saved: the number of voxels that match
(nhits), the volume of the voxels that match, the total number of
voxels in the volume (nvoxtot), and the percent matching
(100*nhits/nvoxtot).

--binval    binval
--binvalnot binvalnot

Value to use for those voxels that are in the threshold/match
(--binval) or out of the range (--binvalnot). These must be integer
values. binvalnot only applies when a merge volume is NOT specified.

--frame frameno

Use give frame of the input. 0-based. Default is 0.

--frame-sum

Sum the frames together before applying threshold.

--frame-and

Treat the multi-frame volume as binary 'AND' the frames together. This
takes an intersection of the individual frames. You do not need to 
specify a --min (the min will be set to nframes-0.5).

--merge mergevol

Merge binarization with the mergevol. If the voxel is within the threshold
range (or matches), then its value will be binval. If not, then it will 
inherit its value from the value at that voxel in mergevol. mergevol must 
be the same dimension as the input volume. Combining this with --binval 
allows you to construct crude segmentations.

--mask maskvol
--mask-thresh thresh

Mask input with mask. The mask volume is itself binarized at thresh
(default is 0.5). If a voxel is not in the mask, then it will be assigned
binvalnot or the value from the merge volume.

--zero-edges

Set the first and last planes in all dimensions to 0 (or --binvalnot). This
makes sure that all the voxels on the edge of the imaging volume are 0.

--zero-slice-edges

Same as --zero-edges, but only for slices.

--uchar

Save output file in 'uchar' format.

  ENDHELP
*/

/*
  BEGINUSAGE

  ENDUSAGE
*/


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
double round(double x);
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <float.h>

#include "macros.h"
#include "utils.h"
#include "mrisurf.h"
#include "mrisutils.h"
#include "error.h"
#include "diag.h"
#include "mri.h"
#include "mri2.h"
#include "fio.h"
#include "version.h"
#include "label.h"
#include "matrix.h"
#include "annotation.h"
#include "fmriutils.h"
#include "cmdargs.h"
#include "fsglm.h"
#include "pdf.h"
#include "fsgdf.h"
#include "timer.h"
#include "matfile.h"
#include "volcluster.h"
#include "surfcluster.h"
#include "randomfields.h"


static int  parse_commandline(int argc, char **argv);
static void check_options(void);
static void print_usage(void) ;
static void usage_exit(void);
static void print_help(void) ;
static void print_version(void) ;
static void dump_options(FILE *fp);
int main(int argc, char *argv[]) ;

static char vcid[] = "$Id: mri_binarize.c,v 1.23.2.1 2010/07/28 20:07:29 greve Exp $";
char *Progname = NULL;
char *cmdline, cwd[2000];
int debug=0;
int checkoptsonly=0;
struct utsname uts;

char *InVolFile=NULL;
char *OutVolFile=NULL;
char *MergeVolFile=NULL;
char *MaskVolFile=NULL;
double MinThresh, MaxThresh;
int MinThreshSet=0, MaxThreshSet=0;
double RMinThresh, RMaxThresh;
int RMinThreshSet=0, RMaxThreshSet=0;
char *CountFile = NULL;

int BinVal=1;
int BinValNot=0;
int frame=0;
int DoAbs=0;
int DoNeg=0;
int ZeroColEdges = 0;
int ZeroRowEdges = 0;
int ZeroSliceEdges = 0;

int DoMatch = 0;
int nMatch = 0;
int MatchValues[1000];
int Matched = 0;

MRI *InVol,*OutVol,*MergeVol,*MaskVol=NULL;
double MaskThresh = 0.5;

int nErode2d = 0;
int nErode3d = 0;
int nDilate3d = 0;
int DoBinCol = 0;

int mriTypeUchar = 0;
int DoFrameSum = 0;
int DoFrameAnd = 0;
int DoPercent = 0;
double TopPercent = -1;

/*---------------------------------------------------------------*/
int main(int argc, char *argv[]) {
  int nargs, c, r, s, nhits, InMask, n, mriType,nvox;
  double val,maskval,mergeval,gmean,gstd,gmax,voxvol;
  FILE *fp;

  nargs = handle_version_option (argc, argv, vcid, "$Name:  $");
  if (nargs && argc - nargs == 1) exit (0);
  argc -= nargs;
  cmdline = argv2cmdline(argc,argv);
  uname(&uts);
  getcwd(cwd,2000);

  Progname = argv[0] ;
  argc --;
  argv++;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;
  if (argc == 0) usage_exit();
  parse_commandline(argc, argv);
  check_options();
  if (checkoptsonly) return(0);
  dump_options(stdout);

  // Load the input volume
  InVol = MRIread(InVolFile);
  if (InVol==NULL) exit(1);
  if (frame >= InVol->nframes) {
    printf("ERROR: requested frame=%d >= nframes=%d\n",
           frame,InVol->nframes);
    exit(1);
  }

  if(DoFrameSum || DoFrameAnd) {
    printf("Summing over %d frames\n",InVol->nframes);
    InVol = MRIframeSum(InVol,InVol);
    frame = 0;
  }
  if(DoFrameAnd) MinThresh = InVol->nframes - 0.5;

  if(DoAbs) {
    printf("Removing sign from input\n");
    MRIabs(InVol,InVol);
  }
  if(DoNeg) {
    printf("Negating input\n");
    MRImultiplyConst(InVol,-1.0,InVol);
  }

  if(RMinThreshSet || RMaxThreshSet){
    printf("Computing global statistics \n");
    RFglobalStats(InVol, NULL, &gmean, &gstd, &gmax);
    printf("mean = %g, std = %g, max = %g\n",gmean, gstd, gmax);
    if(RMinThreshSet)  {
      MinThresh = gmean * RMinThresh;
      MinThreshSet = 1;
      printf("Setting min thresh to %g\n",MinThresh);
    }
    if(RMaxThreshSet) {
      MaxThresh = gmean * RMaxThresh;
      MaxThreshSet = 1;
      printf("Setting max thresh to %g\n",MinThresh);
    }
  }


  // Load the merge volume (if needed)
  if (MergeVolFile) {
    MergeVol = MRIread(MergeVolFile);
    if (MergeVol==NULL) exit(1);
    if (MergeVol->width != InVol->width) {
      printf("ERROR: dimension mismatch between input and merge volumes\n");
      exit(1);
    }
    if (MergeVol->height != InVol->height) {
      printf("ERROR: dimension mismatch between input and merge volumes\n");
      exit(1);
    }
    if (MergeVol->depth != InVol->depth) {
      printf("ERROR: dimension mismatch between input and merge volumes\n");
      exit(1);
    }
  }

  // Load the mask volume (if needed)
  if (MaskVolFile) {
    printf("Loading mask %s\n",MaskVolFile);
    MaskVol = MRIread(MaskVolFile);
    if (MaskVol==NULL) exit(1);
    if (MaskVol->width != InVol->width) {
      printf("ERROR: dimension mismatch between input and mask volumes\n");
      exit(1);
    }
    if (MaskVol->height != InVol->height) {
      printf("ERROR: dimension mismatch between input and mask volumes\n");
      exit(1);
    }
    if (MaskVol->depth != InVol->depth) {
      printf("ERROR: dimension mismatch between input and mask volumes\n");
      exit(1);
    }
  }

  if(DoPercent) {
    printf("Computing threshold based on top %g percent\n",TopPercent);
    MinThresh = MRIpercentThresh(InVol, MaskVol, frame, TopPercent);
    printf("  Threshold set to %g\n",MinThresh);
  }

  // Prepare the output volume
  mriType = MRI_INT;
  if (mriTypeUchar) mriType = MRI_UCHAR;
  OutVol = MRIalloc(InVol->width,InVol->height,InVol->depth,mriType);
  if (OutVol == NULL) exit(1);
  MRIcopyHeader(InVol, OutVol);

  // Binarize
  mergeval = BinValNot;
  InMask = 1;
  nhits = 0;
  for (c=0; c < InVol->width; c++) {
    for (r=0; r < InVol->height; r++) {
      for (s=0; s < InVol->depth; s++) {
	if(MergeVol) mergeval = MRIgetVoxVal(MergeVol,c,r,s,0);

	// Skip if on the edge
	if( (ZeroColEdges &&   (c == 0 || c == InVol->width-1))  ||
	    (ZeroRowEdges &&   (r == 0 || r == InVol->height-1)) ||
	    (ZeroSliceEdges && (s == 0 || s == InVol->depth-1)) ){
	  MRIsetVoxVal(OutVol,c,r,s,0,mergeval);
	  continue;
	}

	// Skip if not in the mask
        if(MaskVol) {
          maskval = MRIgetVoxVal(MaskVol,c,r,s,0);
          if(maskval < MaskThresh){
	    MRIsetVoxVal(OutVol,c,r,s,0,mergeval);
            continue;
	  }
        }

	// Get the value at this voxel
        val = MRIgetVoxVal(InVol,c,r,s,frame);

	if(DoMatch){
	  // Check for a match
	  Matched = 0;
	  for(n=0; n < nMatch; n++){
	    if(fabs(val - MatchValues[n]) < 2*FLT_MIN){
	      MRIsetVoxVal(OutVol,c,r,s,0,BinVal);
	      Matched = 1;
	      nhits ++;
	      break;
	    }
	  }
	  if(!Matched) MRIsetVoxVal(OutVol,c,r,s,0,mergeval);
	}
	else{
	  // Determine whether it is in range
	  if((MinThreshSet && (val < MinThresh)) ||
	     (MaxThreshSet && (val > MaxThresh))){
	    // It is NOT in the Range
	    MRIsetVoxVal(OutVol,c,r,s,0,mergeval);
	  }
	  else {
	    // It is in the Range
	    MRIsetVoxVal(OutVol,c,r,s,0,BinVal);
	    nhits ++;
	  }
        }

      } // slice
    } // row
  } // col

  printf("Found %d values in range\n",nhits);

  if(nDilate3d > 0){
    printf("Dilating %d voxels in 3d\n",nDilate3d);
    for(n=0; n<nDilate3d; n++) MRIdilate(OutVol,OutVol);
  }
  if(nErode3d > 0){
    printf("Eroding %d voxels in 3d\n",nErode3d);
    for(n=0; n<nErode3d; n++) MRIerode(OutVol,OutVol);
  }
  if(nErode2d > 0){
    printf("Eroding %d voxels in 2d\n",nErode2d);
    for(n=0; n<nErode2d; n++) MRIerode2D(OutVol,OutVol);
  }

  
  printf("Counting number of voxels\n");
  nhits = 0;
  for (c=0; c < OutVol->width; c++) {
    for (r=0; r < OutVol->height; r++) {
      for (s=0; s < OutVol->depth; s++) {
	// Get the value at this voxel
        val = MRIgetVoxVal(OutVol,c,r,s,frame);
	if(fabs(val-BinVal) < .00001) nhits ++;
      } // slice
    } // row
  } // col
  printf("Found %d voxels in final mask\n",nhits);

  if(DoBinCol){
    printf("Filling mask with column number\n");
    MRIbinMaskToCol(OutVol, OutVol);
  }

  // Save output
  MRIwrite(OutVol,OutVolFile);

  if(CountFile){
    fp = fopen(CountFile,"w");
    if(fp == NULL){
      printf("ERROR: could not open %s\n",CountFile);
      exit(1);
    }
    nvox = OutVol->width * OutVol->height * OutVol->depth;
    voxvol = OutVol->xsize * OutVol->ysize * OutVol->zsize;
    fprintf(fp,"%d %lf %d %lf\n",nhits,nhits*voxvol,nvox,(double)100*nhits/nvox);
    fclose(fp);
  }

  printf("mri_binarize done\n");
  exit(0);
}
/* --------------------------------------------- */
static int parse_commandline(int argc, char **argv) {
  int  nargc , nargsused;
  char **pargv, *option ;

  if (argc < 1) usage_exit();

  nargc   = argc;
  pargv = argv;
  while (nargc > 0) {

    option = pargv[0];
    if (debug) printf("%d %s\n",nargc,option);
    nargc -= 1;
    pargv += 1;

    nargsused = 0;

    if (!strcasecmp(option, "--help"))  print_help() ;
    else if (!strcasecmp(option, "--version")) print_version() ;
    else if (!strcasecmp(option, "--debug"))   debug = 1;
    else if (!strcasecmp(option, "--checkopts"))   checkoptsonly = 1;
    else if (!strcasecmp(option, "--nocheckopts")) checkoptsonly = 0;
    else if (!strcasecmp(option, "--abs")) DoAbs = 1;
    else if (!strcasecmp(option, "--neg")) DoNeg = 1;
    else if (!strcasecmp(option, "--bincol")) DoBinCol = 1;
    else if (!strcasecmp(option, "--uchar")) mriTypeUchar = 1;
    else if (!strcasecmp(option, "--zero-edges")){
      ZeroColEdges = 1;
      ZeroRowEdges = 1;
      ZeroSliceEdges = 1;
    }
    else if (!strcasecmp(option, "--zero-slice-edges")) ZeroSliceEdges = 1;
    else if (!strcasecmp(option, "--wm")){
      MatchValues[0] =  2;
      MatchValues[1] = 41;
      MatchValues[2] = 82;
      MatchValues[3] = 251;
      MatchValues[4] = 252;
      MatchValues[5] = 253;
      MatchValues[6] = 254;
      MatchValues[7] = 255;
      nMatch = 8;
      DoMatch = 1;
    }
    else if (!strcasecmp(option, "--ventricles")){
      MatchValues[0] =  4; // Left-Lateral-Ventricle
      MatchValues[1] =  5; // Left-Inf-Lat-Vent
      MatchValues[2] = 14; // 3rd-Ventricle
      MatchValues[3] = 43; // Right-Lateral-Ventricle
      MatchValues[4] = 44; // Right-Inf-Lat-Vent
      MatchValues[5] = 72; // 5th-Ventricle
      MatchValues[6] = 31; // Left-choroid-plexus 
      MatchValues[7] = 63; // Right-choroid-plexus 
      //MatchValues[3] = 15; // 4th-Ventricle 
      nMatch = 8;
      DoMatch = 1;
    }

    else if (!strcasecmp(option, "--wm+vcsf")){
      MatchValues[0] =  2;
      MatchValues[1] = 41;
      MatchValues[2] = 82;
      MatchValues[3] = 251;
      MatchValues[4] = 252;
      MatchValues[5] = 253;
      MatchValues[6] = 254;
      MatchValues[7] = 255;
      MatchValues[8] =  4; // Left-Lateral-Ventricle
      MatchValues[9] =  5; // Left-Inf-Lat-Vent
      MatchValues[10] = 14; // 3rd-Ventricle
      MatchValues[11] = 43; // Right-Lateral-Ventricle
      MatchValues[12] = 44; // Right-Inf-Lat-Vent
      MatchValues[13] = 72; // 5th-Ventricle
      MatchValues[14] = 31; // Left-choroid-plexus 
      MatchValues[15] = 63; // Right-choroid-plexus 
      nMatch = 16;
      DoMatch = 1;
    }

    else if (!strcasecmp(option, "--i")) {
      if (nargc < 1) CMDargNErr(option,1);
      InVolFile = pargv[0];
      nargsused = 1;
    } else if (!strcasecmp(option, "--o")) {
      if (nargc < 1) CMDargNErr(option,1);
      OutVolFile = pargv[0];
      nargsused = 1;
    } else if (!strcasecmp(option, "--merge")) {
      if (nargc < 1) CMDargNErr(option,1);
      MergeVolFile = pargv[0];
      nargsused = 1;
    } else if (!strcasecmp(option, "--mask")) {
      if (nargc < 1) CMDargNErr(option,1);
      MaskVolFile = pargv[0];
      nargsused = 1;
    } else if (!strcasecmp(option, "--mask-thresh")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%lf",&MaskThresh);
      nargsused = 1;
    } else if (!strcasecmp(option, "--pct")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%lf",&TopPercent);
      MinThreshSet = 1;
      DoPercent = 1;
      nargsused = 1;
    } else if (!strcasecmp(option, "--min")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%lf",&MinThresh);
      MinThreshSet = 1;
      nargsused = 1;
    } else if (!strcasecmp(option, "--max")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%lf",&MaxThresh);
      MaxThreshSet = 1;
      nargsused = 1;
    } else if (!strcasecmp(option, "--rmin")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%lf",&RMinThresh);
      RMinThreshSet = 1;
      nargsused = 1;
    } else if (!strcasecmp(option, "--max")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%lf",&RMaxThresh);
      RMaxThreshSet = 1;
      nargsused = 1;
    } else if (!strcasecmp(option, "--binval")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%d",&BinVal);
      nargsused = 1;
    } else if (!strcasecmp(option, "--binvalnot")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%d",&BinValNot);
      nargsused = 1;
    } else if (!strcasecmp(option, "--inv")) {
      BinVal = 0;
      BinValNot = 1;
    } else if (!strcasecmp(option, "--match")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%d",&MatchValues[nMatch]);
      nMatch ++;
      DoMatch = 1;
      nargsused = 1;
    } 
    else if (!strcasecmp(option, "--frame")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%d",&frame);
      nargsused = 1;
    } 
    else if (!strcasecmp(option, "--frame-sum")) {
      DoFrameSum = 1;
    } 
    else if (!strcasecmp(option, "--frame-and")) {
      DoFrameAnd = 1;
      MinThreshSet = 1;
    } 
    else if (!strcasecmp(option, "--dilate")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%d",&nDilate3d);
      nargsused = 1;
    } else if (!strcasecmp(option, "--erode")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%d",&nErode3d);
      nargsused = 1;
    } else if (!strcasecmp(option, "--erode2d")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%d",&nErode2d);
      nargsused = 1;
    } else if (!strcasecmp(option, "--count")) {
      if (nargc < 1) CMDargNErr(option,1);
      CountFile = pargv[0];
      nargsused = 1;
    } else {
      fprintf(stderr,"ERROR: Option %s unknown\n",option);
      if (CMDsingleDash(option))
        fprintf(stderr,"       Did you really mean -%s ?\n",option);
      exit(-1);
    }
    nargc -= nargsused;
    pargv += nargsused;
  }
  return(0);
}
/* ------------------------------------------------------ */
static void usage_exit(void) {
  print_usage() ;
  exit(1) ;
}
/* --------------------------------------------- */
static void print_usage(void) {
  printf("USAGE: %s \n",Progname) ;
  printf("\n");
  printf("   --i invol  : input volume \n");
  printf("   \n");
  printf("   --min min  : min thresh (def is -inf)\n");
  printf("   --max max  : max thresh (def is +inf)\n");
  printf("   --pct P : set threshold to capture top P%% (in mask or total volume)\n");
  printf("   --rmin rmin  : compute min based on rmin*globalmean\n");
  printf("   --rmax rmax  : compute max based on rmax*globalmean\n");
  printf("   --match matchval <--match matchval>  : match instead of threshold\n");
  printf("   --wm : set match vals to 2 and 41 (aseg for cerebral WM)\n");
  printf("   --ventricles : set match vals those for aseg ventricles+choroid (not 4th)\n");
  printf("   --wm+vcsf : WM and ventricular CSF, including choroid (not 4th)\n");
  printf("   \n");
  printf("   --o outvol : output volume \n");
  printf("   --count countfile : save number of hits in ascii file (hits,ntotvox,pct)\n");
  printf("   \n");
  printf("   --binval    val    : set vox within thresh to val (default is 1) \n");
  printf("   --binvalnot notval : set vox outside range to notval (default is 0) \n");
  printf("   --inv              : set binval=0, binvalnot=1\n");
  printf("   --frame frameno    : use 0-based frame of input (default is 0) \n");
  printf("   --frame-sum : sum frames together before binarizing\n");
  printf("   --frame-and : take intersection (AND) of frames. No --min needed.\n");
  printf("   --merge mergevol   : merge with mergevolume \n");
  printf("   --mask maskvol       : must be within mask \n");
  printf("   --mask-thresh thresh : set thresh for mask (def is 0.5) \n");
  printf("   --abs : take abs of invol first (ie, make unsigned)\n");
  printf("   --bincol : set binarized voxel value to its column number\n");
  printf("   --zero-edges : zero the edge voxels\n");
  printf("   --zero-slice-edges : zero the edge slice voxels\n");
  printf("   --dilate ndilate: dilate binarization in 3D\n");
  printf("   --erode  nerode: erode binarization in 3D (after any dilation)\n");
  printf("   --erode2d nerode2d: erode binarization in 2D (after any 3D erosion)\n");
  printf("\n");
  printf("   --debug     turn on debugging\n");
  printf("   --checkopts don't run anything, just check options and exit\n");
  printf("   --help      print out information on how to use this program\n");
  printf("   --version   print out version and exit\n");
  printf("\n");
  printf("%s\n", vcid) ;
  printf("\n");
}
/* --------------------------------------------- */
static void print_help(void) {
  print_usage() ;
printf("\n");
printf("Program to binarize a volume (or volume-encoded surface file). Can also\n");
printf("be used to merge with other binarizations. Binarization can be done\n");
printf("based on threshold or on matched values.\n");
printf("\n");
printf("--i invol\n");
printf("\n");
printf("Input volume to be binarized.\n");
printf("\n");
printf("--min min\n");
printf("--max max\n");
printf("--rmin rmin\n");
printf("--rmax rmax\n");
printf("\n");
printf("Minimum and maximum thresholds. If the value at a voxel is >= min and\n");
printf("<= max, then its value in the output will be 1 (or --binval),\n");
printf("otherwise it is 0 (or --binvalnot) or the value of the merge volume at\n");
printf("that voxel.  By default, min = -infinity and max = +infinity, but you\n");
printf("must set one of the thresholds. Cannot be used with --match. If --rmin\n");
printf("or --rmax are specified, then min (or max) is computed as rmin (or\n");
printf("rmax) times the global mean of the input.\n");
printf("\n");
printf("--pct P\n");
printf("\n");
printf("Set min threshold so that the top P percent of the voxels are captured\n");
printf("in the output mask. The percent will be computed based on the number of\n");
printf("voxels in the volume (if not input mask is specified) or within the\n");
printf("input mask.\n");
printf("\n");
printf("--match matchvalue <--match matchvalue>\n");
printf("\n");
printf("Binarize based on matching values. Any number of match values can be \n");
printf("specified. Cannot be used with --min/--max.\n");
printf("\n");
printf("--o outvol\n");
printf("\n");
printf("Path to output volume.\n");
printf("\n");
printf("--count countfile\n");
printf("\n");
printf("Save number of voxels that meet match criteria in ascii\n");
printf("countefile. Four numbers are saved: the number of voxels that match\n");
printf("(nhits), the volume of the voxels that match, the total number of\n");
printf("voxels in the volume (nvoxtot), and the percent matching\n");
printf("(100*nhits/nvoxtot).\n");
printf("\n");
printf("--binval    binval\n");
printf("--binvalnot binvalnot\n");
printf("\n");
printf("Value to use for those voxels that are in the threshold/match\n");
printf("(--binval) or out of the range (--binvalnot). These must be integer\n");
printf("values. binvalnot only applies when a merge volume is NOT specified.\n");
printf("\n");
printf("--frame frameno\n");
printf("\n");
printf("Use give frame of the input. 0-based. Default is 0.\n");
printf("\n");
printf("--frame-sum\n");
printf("\n");
printf("Sum the frames together before applying threshold.\n");
printf("\n");
printf("--frame-and\n");
printf("\n");
printf("Treat the multi-frame volume as binary 'AND' the frames together. This\n");
printf("takes an intersection of the individual frames. You do not need to \n");
printf("specify a --min (the min will be set to nframes-0.5).\n");
printf("\n");
printf("--merge mergevol\n");
printf("\n");
printf("Merge binarization with the mergevol. If the voxel is within the threshold\n");
printf("range (or matches), then its value will be binval. If not, then it will \n");
printf("inherit its value from the value at that voxel in mergevol. mergevol must \n");
printf("be the same dimension as the input volume. Combining this with --binval \n");
printf("allows you to construct crude segmentations.\n");
printf("\n");
printf("--mask maskvol\n");
printf("--mask-thresh thresh\n");
printf("\n");
printf("Mask input with mask. The mask volume is itself binarized at thresh\n");
printf("(default is 0.5). If a voxel is not in the mask, then it will be assigned\n");
printf("binvalnot or the value from the merge volume.\n");
printf("\n");
printf("--zero-edges\n");
printf("\n");
printf("Set the first and last planes in all dimensions to 0 (or --binvalnot). This\n");
printf("makes sure that all the voxels on the edge of the imaging volume are 0.\n");
printf("\n");
printf("--zero-slice-edges\n");
printf("\n");
printf("Same as --zero-edges, but only for slices.\n");
printf("\n");
printf("--uchar\n");
printf("\n");
printf("Save output file in 'uchar' format.\n");
printf("\n");
  exit(1) ;
}
/* --------------------------------------------- */
static void print_version(void) {
  printf("%s\n", vcid) ;
  exit(1) ;
}
/* --------------------------------------------- */
static void check_options(void) {
  if (InVolFile == NULL) {
    printf("ERROR: must specify input volume\n");
    exit(1);
  }
  if (OutVolFile == NULL) {
    printf("ERROR: must specify output volume\n");
    exit(1);
  }
  if(MinThreshSet == 0  && MaxThreshSet == 0 &&
     RMinThreshSet == 0 && RMaxThreshSet == 0 &&
     !DoMatch ) {
    printf("ERROR: must specify minimum and/or maximum threshold or match values\n");
    exit(1);
  }
  if((MinThreshSet || MaxThreshSet || RMinThreshSet || RMaxThreshSet) && DoMatch ) {
    printf("ERROR: cannot specify threshold and match values\n");
    exit(1);
  }
  if(MinThreshSet && RMinThreshSet){
    printf("ERROR: cannot --rmin and --min \n");
    exit(1);
  }
  if(MaxThreshSet && RMaxThreshSet){
    printf("ERROR: cannot --rmax and --max \n");
    exit(1);
  }
  if(!DoMatch){
    if (MaxThreshSet && MinThreshSet && MaxThresh < MinThresh) {
      printf("ERROR: max thresh = %g < min thresh = %g\n",
	     MaxThresh,MinThresh);
      exit(1);
    }
  }
  return;
}

/* --------------------------------------------- */
static void dump_options(FILE *fp) {
  int n;

  fprintf(fp,"\n");
  fprintf(fp,"%s\n",vcid);
  fprintf(fp,"cwd %s\n",cwd);
  fprintf(fp,"cmdline %s\n",cmdline);
  fprintf(fp,"sysname  %s\n",uts.sysname);
  fprintf(fp,"hostname %s\n",uts.nodename);
  fprintf(fp,"machine  %s\n",uts.machine);
  fprintf(fp,"user     %s\n",VERuser());
  fprintf(fp,"\n");
  fprintf(fp,"input      %s\n",InVolFile);
  fprintf(fp,"frame      %d\n",frame);
  fprintf(fp,"nErode3d   %d\n",nErode3d);
  fprintf(fp,"nErode2d   %d\n",nErode2d);
  fprintf(fp,"output     %s\n",OutVolFile);

  if(!DoMatch){
    fprintf(fp,"Binarizing based on threshold\n");
    if(MinThreshSet)        fprintf(fp,"min        %g\n",MinThresh);
    else if(RMinThreshSet)  fprintf(fp,"rmin       %g\n",RMinThresh);
    else                    fprintf(fp,"min        -infinity\n");
    if(MaxThreshSet)        fprintf(fp,"max        %g\n",MaxThresh);
    else if(RMaxThreshSet)  fprintf(fp,"rmax       %g\n",RMaxThresh);
    else                    fprintf(fp,"max        +infinity\n");
  }
  else {
    fprintf(fp,"Binarizing based on matching values\n");
    fprintf(fp,"nMatch %d\n",nMatch);
    for(n=0; n < nMatch; n++)
      fprintf(fp,"%2d  %4d\n",n,MatchValues[n]);
  }
  fprintf(fp,"binval        %d\n",BinVal);
  fprintf(fp,"binvalnot     %d\n",BinValNot);
  if (MergeVolFile)
    fprintf(fp,"merge      %s\n",MergeVolFile);
  if (MaskVolFile) {
    fprintf(fp,"mask       %s\n",MaskVolFile);
    fprintf(fp,"maskthresh %lf\n",MaskThresh);
  }
  return;
}




