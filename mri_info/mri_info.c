/**
 * @file  mri_info.c
 * @brief Prints mri volume information found in .mgz file.
 *
 * Dumps information about the volume to stdout.
 */
/*
 * Original Author: Bruce Fischl
 * CVS Revision Info:
 *    $Author: greve $
 *    $Date: 2010/07/21 20:21:59 $
 *    $Revision: 1.71.2.1 $
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

char *MRI_INFO_VERSION = "$Revision: 1.71.2.1 $";

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "const.h"
#include "machine.h"
#include "fio.h"
#include "utils.h"
#include "mri.h"
#include "gcamorph.h"
#include "volume_io.h"
#include "analyze.h"
#include "mri_identify.h"
#include "error.h"
#include "diag.h"
#include "version.h"
#include "mghendian.h"
#include "fio.h"
#include "cmdargs.h"
#include "macros.h"

static void do_file(char *fname);
static int  parse_commandline(int argc, char **argv);
static void check_options(void);
static void print_usage(void) ;
static void usage_exit(void);
static void print_help(void) ;
static void print_version(void) ;

static char vcid[] = "$Id: mri_info.c,v 1.71.2.1 2010/07/21 20:21:59 greve Exp $";

char *Progname ;
static char *inputlist[100];
static int nthinput=0;
static int PrintTR=0;
static int PrintTE=0;
static int PrintTI=0;
static int PrintFlipAngle=0;
static int PrintPEDir = 0;
static int PrintCRes = 0;
static int PrintType = 0 ;
static int PrintConformed = 0 ;
static int PrintRRes = 0;
static int PrintSRes = 0;
static int PrintVoxVol  = 0;
static int PrintNCols = 0;
static int PrintNRows = 0;
static int PrintNSlices = 0;
static int PrintDOF = 0;
static int PrintNFrames = 0;
static int PrintMidFrame = 0;
static int PrintFormat = 0;
static int PrintColDC   = 0;
static int PrintRowDC   = 0;
static int PrintSliceDC = 0;
static int PrintVox2RAS = 0;
static int PrintRAS2Vox = 0;
static int PrintRASGood = 0;
static int PrintVox2RAStkr = 0;
static int PrintRAS2Voxtkr = 0;
static int PrintVox2RASfsl = 0;
static int PrintTkr2Scanner = 0;
static int PrintScanner2Tkr = 0;
static int PrintDet = 0;
static int PrintOrientation = 0;
static int PrintSliceDirection = 0;
static int PrintCRAS = 0;
static int PrintEntropy = 0;
static int PrintVoxel = 0;
static int PrintAutoAlign = 0;
static int PrintCmds = 0;
static int VoxelCRS[3];
static FILE *fpout;
static int PrintToFile = 0;
static char *outfile = NULL;
static int debug = 0;
static int intype=MRI_VOLUME_TYPE_UNKNOWN;
static char *intypestr=NULL;


/***-------------------------------------------------------****/
int main(int argc, char *argv[]) {
  int nargs, index;

  /* rkt: check for and handle version tag */
  nargs = handle_version_option (argc, argv, vcid, "$Name:  $");
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  Progname = argv[0] ;
  argc --;
  argv++;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;
  if (argc == 0) usage_exit();

  parse_commandline(argc, argv);
  check_options();

  if (PrintToFile) {
    fpout = fopen(outfile,"w");
    if (fpout == NULL) {
      printf("ERROR: could not open %s\n",outfile);
      exit(1);
    }
  } else fpout = stdout;

  for (index=0;index<nthinput;index++) {
    if (debug) printf("%d %s ----- \n",index,inputlist[index]);
    do_file(inputlist[index]);
  }
  if (PrintToFile) fclose(fpout);

  exit(0);

} /* end main() */

/* ------------------------------------------------------------------ */
static int parse_commandline(int argc, char **argv) {
  int  nargc , nargsused;
  char **pargv, *option ;

  if (argc < 1) usage_exit();

  nargc = argc;
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
    else if (!strcasecmp(option, "--tr"))   PrintTR = 1;
    else if (!strcasecmp(option, "--te"))   PrintTE = 1;
    else if (!strcasecmp(option, "--ti"))   PrintTI = 1;
    else if (!strcasecmp(option, "--fa"))   PrintFlipAngle = 1;
    else if (!strcasecmp(option, "--flip_angle"))   PrintFlipAngle = 1;
    else if (!strcasecmp(option, "--pedir"))   PrintPEDir = 1;

    else if (!strcasecmp(option, "--cres"))    PrintCRes = 1;
    else if (!strcasecmp(option, "--xsize"))   PrintCRes = 1;
    else if (!strcasecmp(option, "--rres"))    PrintRRes = 1;
    else if (!strcasecmp(option, "--ysize"))   PrintCRes = 1;
    else if (!strcasecmp(option, "--sres"))    PrintSRes = 1;
    else if (!strcasecmp(option, "--zsize"))   PrintCRes = 1;
    else if (!strcasecmp(option, "--voxvol"))  PrintVoxVol = 1;
    else if (!strcasecmp(option, "--type"))    PrintType = 1;
    else if (!strcasecmp(option, "--conformed")) PrintConformed = 1;

    else if (!strcasecmp(option, "--ncols"))     PrintNCols = 1;
    else if (!strcasecmp(option, "--width"))     PrintNCols = 1;
    else if (!strcasecmp(option, "--nrows"))     PrintNRows = 1;
    else if (!strcasecmp(option, "--height"))    PrintNRows = 1;
    else if (!strcasecmp(option, "--nslices"))   PrintNSlices = 1;
    else if (!strcasecmp(option, "--depth"))     PrintNSlices = 1;
    else if (!strcasecmp(option, "--dof"))       PrintDOF = 1;

    else if (!strcasecmp(option, "--cdc"))       PrintColDC = 1;
    else if (!strcasecmp(option, "--rdc"))       PrintRowDC = 1;
    else if (!strcasecmp(option, "--sdc"))       PrintSliceDC = 1;
    else if (!strcasecmp(option, "--vox2ras"))   PrintVox2RAS = 1;
    else if (!strcasecmp(option, "--ras2vox"))   PrintRAS2Vox = 1;
    else if (!strcasecmp(option, "--vox2ras-tkr")) PrintVox2RAStkr = 1;
    else if (!strcasecmp(option, "--ras2vox-tkr")) PrintRAS2Voxtkr = 1;
    else if (!strcasecmp(option, "--vox2ras-fsl")) PrintVox2RASfsl = 1;
    else if (!strcasecmp(option, "--tkr2scanner")) PrintTkr2Scanner = 1;
    else if (!strcasecmp(option, "--scanner2tkr")) PrintScanner2Tkr = 1;
    else if (!strcasecmp(option, "--ras_good"))   PrintRASGood = 1;
    else if (!strcasecmp(option, "--cras"))      PrintCRAS = 1;

    else if (!strcasecmp(option, "--det"))     PrintDet = 1;

    else if (!strcasecmp(option, "--nframes"))   PrintNFrames = 1;
    else if (!strcasecmp(option, "--mid-frame"))   PrintMidFrame = 1;
    else if (!strcasecmp(option, "--format")) PrintFormat = 1;
    else if (!strcasecmp(option, "--orientation")) PrintOrientation = 1;
    else if (!strcasecmp(option, "--slicedirection")) PrintSliceDirection = 1;
    else if (!strcasecmp(option, "--autoalign")) PrintAutoAlign = 1;
    else if (!strcasecmp(option, "--entropy")) PrintEntropy = 1;
    else if (!strcasecmp(option, "--cmds")) PrintCmds = 1;
    else if (!strcasecmp(option, "--o")) {
      PrintToFile = 1;
      outfile = pargv[0];
      nargc --;
      pargv ++;
    } else if (!strcasecmp(option, "-it") || 
               !strcasecmp(option, "--in_type")) {
      intypestr = pargv[0];
      intype = string_to_type(intypestr);
      nargc --;
      pargv ++;
    } else if (!strcasecmp(option, "--voxel")) {
      if (nargc < 3) {
        CMDargNErr(option, 3);
        exit(1);
      }
      PrintVoxel = 1;
      sscanf(pargv[0],"%d",&VoxelCRS[0]);
      sscanf(pargv[1],"%d",&VoxelCRS[1]);
      sscanf(pargv[2],"%d",&VoxelCRS[2]);
      if (Gdiag_no > 1) 
        printf("%d %d %d\n",VoxelCRS[0],VoxelCRS[1],VoxelCRS[2]);
      nargc -= 3;
      pargv += 3;
    } else {
      // Must be an input volume
      inputlist[nthinput] = option;
      nthinput++;
    }
    nargc -= nargsused;
    pargv += nargsused;
  }
  return(0);
}
/* --------------------------------------------- */
static void print_usage(void) {
  printf("USAGE: %s fname1 <fname2> <options> \n",Progname) ;
  printf("\n");
  printf("   --conformed : print whether a volume is conformed stdout\n");
  printf("   --type : print the voxel type (e.g. FLOAT) to stdout\n");
  printf("   --tr : print TR to stdout\n");
  printf("   --te : print TE to stdout\n");
  printf("   --ti : print TI to stdout\n");
  printf("   --fa : print flip angle to stdout\n");
  printf("   --pedir : print phase encode direction\n");
  printf("   --cres : print column voxel size (xsize)\n");
  printf("   --rres : print row    voxel size (ysize)\n");
  printf("   --sres : print slice  voxel size (zsize)\n");
  printf("   --voxvol : print voxel volume\n");
  printf("   --ncols : print number of columns (width) to stdout\n");
  printf("   --nrows : print number of rows (height) to stdout\n");
  printf("   --nslices : print number of columns (depth) to stdout\n");
  printf("   --cdc : print column direction cosine (x_{r,a,s})\n");
  printf("   --rdc : print row    direction cosine (y_{r,a,s})\n");
  printf("   --sdc : print slice  direction cosine (z_{r,a,s})\n");
  printf("   --vox2ras : print the native/qform vox2ras matrix\n");
  printf("   --ras2vox : print the native/qform ras2vox matrix\n");
  printf("   --vox2ras-tkr : print the tkregister vox2ras matrix\n");
  printf("   --ras2vox-tkr : print the tkregister ras2vox matrix\n");
  printf("   --vox2ras-fsl : print the FSL/FLIRT vox2ras matrix\n");
  printf("   --tkr2scanner : print tkrRAS-to-scannerRAS matrix \n");
  printf("   --scanner2tkr : print scannerRAS-to-tkrRAS matrix \n");
  printf("   --ras_good : print the ras_good_flag\n");
  printf("   --cras : print the RAS at the 'center' of the volume\n");
  printf("   --det : print the determinant of the vox2ras matrix\n");
  printf("   --dof : print the dof stored in the header\n");
  printf("   --nframes : print number of frames to stdout\n");
  printf("   --mid-frame : print number of middle frame to stdout\n");
  printf("   --format : file format\n");
  printf("   --orientation : orientation string (eg, LPS, RAS, RPI)\n");
  printf("   --slicedirection : primary slice direction (eg, axial)\n");
  printf("   --autoalign : print auto align matrix (if it exists)\n");
  printf("   --cmds : print command-line provenance info\n");
  printf("   --voxel c r s : dump voxel value from col row slice "
         "(0-based, all frames)\n");
  printf("   --entropy : compute and print entropy \n");
  printf("   --o file : print flagged results to file \n");
  printf("   --in_type type : explicitly specify file type "
         "(see mri_convert) \n");
  printf("\n");
  //printf("   --svol svol.img (structural volume)\n");
}
/* --------------------------------------------- */
static void print_help(void) {
  print_usage() ;
  printf(
    "\n"
    "Dumps information about the volume to stdout. Specific pieces \n"
    "of information can be printed out as well by specifying the proper\n"
    "flag (eg, --tr for TR). Time is in msec. Distance is in mm. Angles\n"
    "are in radians.\n"
    "\n"
    "The direction cosine outputs (--cdc, --rdc, --sdc) correspond to \n"
    "mri_convert flags -iid, -ijd, -ikd.\n"
  );


  exit(1) ;
}
/* --------------------------------------------- */
static void print_version(void) {
  printf("%s\n", vcid) ;
  exit(1) ;
}
/* --------------------------------------------- */
static void check_options(void) {
  if (nthinput == 0) {
    printf("ERROR: no input volume supplied\n");
    exit(1);
  }
  return;
}
/* ------------------------------------------------------ */
static void usage_exit(void) {
  print_usage() ;
  exit(1) ;
}

/***-------------------------------------------------------****/
int PrettyMatrixPrint(MATRIX *mat) {
  int row;

  if (mat == NULL)
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM, "mat = NULL!")) ;

  if (mat->type != MATRIX_REAL)
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM, "mat is not Real type")) ;

  if (mat->rows != 4 || mat->cols != 4)
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM, "mat is not of 4 x 4")) ;

  for (row=1; row < 5; ++row)
    printf("              %8.4f %8.4f %8.4f %10.4f\n",
           mat->rptr[row][1], mat->rptr[row][2],
           mat->rptr[row][3], mat->rptr[row][4]);
  return (NO_ERROR);
}

/***-------------------------------------------------------****/
static void do_file(char *fname) {
  MRI *mri ;
  MATRIX *m, *minv, *m2, *m2inv, *p ;
  int r,c,s,f;
  char ostr[5];
  GCA_MORPH *gcam;
  ostr[4] = '\0';

  if (!(strstr(fname, ".m3d") == 0 && strstr(fname, ".m3z") == 0
        && strstr(fname, ".M3D") == 0 && strstr(fname, ".M3Z") == 0)
     ) {
    fprintf(fpout,"Input file is a 3D morph.\n");

    gcam = NULL;
    gcam = GCAMread(fname);
    if (!gcam) return;
    fprintf(fpout,"3D morph source geometry:\n");
    vg_print(&gcam->image);
    fprintf(fpout,"3D morph atlas geometry:\n");
    vg_print(&gcam->atlas);
    GCAMfree(&gcam);
    return;
  }

  if (PrintFormat) {
    fprintf(fpout,"%s\n", type_to_string(mri_identify(fname)));
    return;
  }
  if (!PrintVoxel && !PrintEntropy)  mri = MRIreadHeader(fname, intype) ;
  else             mri = MRIread(fname);
  if(!mri) exit(1); // should exit with error here

  if (PrintTR) {
    fprintf(fpout,"%g\n",mri->tr);
    return;
  }
  if (PrintTE) {
    fprintf(fpout,"%g\n",mri->te);
    return;
  }
  if (PrintConformed) {
    fprintf(fpout,"%s\n",mriConformed(mri) ? "yes" : "no");
    return;
  }

  if (PrintType) {
    switch (mri->type) {
    case MRI_UCHAR:
      fprintf(fpout,"uchar\n") ;
      break ;
    case MRI_FLOAT:
      fprintf(fpout,"float\n") ;
      break ;
    case MRI_LONG:
      fprintf(fpout,"long\n") ;
      break ;
    case MRI_SHORT:
      fprintf(fpout,"short\n") ;
      break ;
    case MRI_INT:
      fprintf(fpout,"int\n") ;
      break ;
    case MRI_TENSOR:
      fprintf(fpout,"tensor\n") ;
      break ;
    default:
      break ;
    }
    return;
  }
  if (PrintTI) {
    fprintf(fpout,"%g\n",mri->ti);
    return;
  }

  if (PrintFlipAngle) {
    fprintf(fpout,"%g\n",mri->flip_angle);
    return;
  }
  if(PrintPEDir) {
    if(mri->pedir) fprintf(fpout,"%s\n",mri->pedir);
    else           fprintf(fpout,"UNKNOWN\n");
    return;
  }
  if (PrintCRes) {
    fprintf(fpout,"%g\n",mri->xsize);
    return;
  }
  if (PrintRRes) {
    fprintf(fpout,"%g\n",mri->ysize);
    return;
  }
  if (PrintSRes) {
    fprintf(fpout,"%g\n",mri->zsize);
    return;
  }
  if (PrintVoxVol) {
    fprintf(fpout,"%g\n",mri->xsize*mri->ysize*mri->zsize);
    return;
  }
  if (PrintNCols) {
    fprintf(fpout,"%d\n",mri->width);
    return;
  }
  if (PrintNRows) {
    fprintf(fpout,"%d\n",mri->height);
    return;
  }
  if (PrintNSlices) {
    fprintf(fpout,"%d\n",mri->depth);
    return;
  }
  if (PrintDOF) {
    fprintf(fpout,"%d\n",mri->dof);
    return;
  }
  if (PrintNFrames) {
    fprintf(fpout,"%d\n",mri->nframes);
    return;
  }
  if (PrintMidFrame) {
    fprintf(fpout,"%d\n",nint(mri->nframes/2));
    return;
  }
  if (PrintColDC) {
    fprintf(fpout,"%g %g %g\n",mri->x_r,mri->x_a,mri->x_s);
    return;
  }
  if (PrintRowDC) {
    fprintf(fpout,"%g %g %g\n",mri->y_r,mri->y_a,mri->y_s);
    return;
  }
  if (PrintSliceDC) {
    fprintf(fpout,"%g %g %g\n",mri->z_r,mri->z_a,mri->z_s);
    return;
  }
  if (PrintCRAS) {
    fprintf(fpout,"%g %g %g\n",mri->c_r,mri->c_a,mri->c_s);
    return;
  }
  if (PrintDet) {
    m = MRIgetVoxelToRasXform(mri) ;
    fprintf(fpout,"%g\n",MatrixDeterminant(m));
    MatrixFree(&m) ;
    return;
  }
  if (PrintVox2RAS) {
    m = MRIgetVoxelToRasXform(mri) ;
    for (r=1; r<=4; r++) {
      for (c=1; c<=4; c++) {
        fprintf(fpout,"%10.5f ",m->rptr[r][c]);
      }
      fprintf(fpout,"\n");
    }
    MatrixFree(&m) ;
    return;
  }
  if (PrintRASGood) {
    fprintf(fpout,"%d\n",mri->ras_good_flag);
    return;
  }
  if (PrintRAS2Vox) {
    m = MRIgetVoxelToRasXform(mri) ;
    minv = MatrixInverse(m,NULL);
    for (r=1; r<=4; r++) {
      for (c=1; c<=4; c++) {
        fprintf(fpout,"%10.5f ",minv->rptr[r][c]);
      }
      fprintf(fpout,"\n");
    }
    MatrixFree(&m) ;
    MatrixFree(&minv) ;
    return;
  }
  if (PrintVox2RAStkr) {
    m = MRIxfmCRS2XYZtkreg(mri);
    for (r=1; r<=4; r++) {
      for (c=1; c<=4; c++) {
        fprintf(fpout,"%10.5f ",m->rptr[r][c]);
      }
      fprintf(fpout,"\n");
    }
    MatrixFree(&m) ;
    return;
  }
  if (PrintRAS2Voxtkr) {
    m = MRIxfmCRS2XYZtkreg(mri);
    m = MatrixInverse(m,m);
    for (r=1; r<=4; r++) {
      for (c=1; c<=4; c++) {
        fprintf(fpout,"%10.5f ",m->rptr[r][c]);
      }
      fprintf(fpout,"\n");
    }
    MatrixFree(&m) ;
    return;
  }
  if (PrintTkr2Scanner) {
    m = MRIxfmCRS2XYZ(mri,0);
    m2 = MRIxfmCRS2XYZtkreg(mri);
    m2inv = MatrixInverse(m2,NULL);
    p = MatrixMultiply(m,m2inv,NULL);
    for (r=1; r<=4; r++) {
      for (c=1; c<=4; c++) {
        fprintf(fpout,"%10.5f ",p->rptr[r][c]);
      }
      fprintf(fpout,"\n");
    }
    MatrixFree(&m) ;
    MatrixFree(&m2) ;
    MatrixFree(&m2inv) ;
    MatrixFree(&p) ;
    return;
  }
  if(PrintScanner2Tkr) {
    p = surfaceRASFromRAS_(mri);
    for (r=1; r<=4; r++) {
      for (c=1; c<=4; c++) {
        fprintf(fpout,"%10.5f ",p->rptr[r][c]);
      }
      fprintf(fpout,"\n");
    }
    MatrixFree(&p) ;
    return;
  }
  if (PrintVox2RASfsl) {
    m = MRIxfmCRS2XYZfsl(mri);
    for (r=1; r<=4; r++) {
      for (c=1; c<=4; c++) {
        fprintf(fpout,"%10.5f ",m->rptr[r][c]);
      }
      fprintf(fpout,"\n");
    }
    MatrixFree(&m) ;
    return;
  }
  if (PrintOrientation) {
    MRIdircosToOrientationString(mri,ostr);
    fprintf(fpout,"%s\n",ostr);
    return;
  }
  if (PrintSliceDirection) {
    fprintf(fpout,"%s\n",MRIsliceDirectionName(mri));
    return;
  }
  if (PrintAutoAlign) {
    if(mri->AutoAlign == NULL){
      fprintf(fpout,"No auto align matrix present\n");
      return;
    }
    MatrixPrintFmt(fpout,"%10f",mri->AutoAlign);
    return;
  }
	if (PrintEntropy) {
	  HISTOGRAM * h = MRIhistogram(mri, 256) ;
		double e = HISTOgetEntropy(h);
    fprintf(fpout,"%f\n",e);
		HISTOfree(&h);
		return;
	}
  if (PrintVoxel) {
    c = VoxelCRS[0];
    r = VoxelCRS[1];
    s = VoxelCRS[2];
    for (f=0; f<mri->nframes; f++)
      fprintf(fpout,"%f\n",MRIgetVoxVal(mri,c,r,s,f));
    return;
  }
  if (PrintCmds) {
    int i;
    //if (mri->ncmds) printf("\nCommand history (provenance):\n");
    printf("\n");
    for (i=0; i < mri->ncmds; i++) {
      printf("%s\n\n",mri->cmdlines[i]);
    }
    return;
  }

  fprintf(fpout,"Volume information for %s\n", fname);
  // mri_identify has been called but the result is not stored
  // and thus I have to call it again
  printf("          type: %s\n", type_to_string(mri_identify(fname)));
  if (mri->nframes > 1)
    printf("    dimensions: %d x %d x %d x %d\n",
           mri->width, mri->height, mri->depth, mri->nframes) ;
  else
    printf("    dimensions: %d x %d x %d\n",
           mri->width, mri->height, mri->depth) ;
  printf("   voxel sizes: %6.4f, %6.4f, %6.4f\n",
         mri->xsize, mri->ysize, mri->zsize) ;
  printf("          type: %s (%d)\n",
         mri->type == MRI_UCHAR   ? "UCHAR" :
         mri->type == MRI_SHORT   ? "SHORT" :
         mri->type == MRI_INT     ? "INT" :
         mri->type == MRI_LONG    ? "LONG" :
         mri->type == MRI_BITMAP  ? "BITMAP" :
         mri->type == MRI_TENSOR  ? "TENSOR" :
         mri->type == MRI_FLOAT   ? "FLOAT" : "UNKNOWN", mri->type) ;
  printf("           fov: %2.3f\n", mri->fov) ;
  printf("           dof: %d\n", mri->dof) ;
#if 0
  printf("        xstart: %2.1f, xend: %2.1f\n",
         mri->xstart*mri->xsize, mri->xend*mri->xsize) ;
  printf("        ystart: %2.1f, yend: %2.1f\n",
         mri->ystart*mri->ysize, mri->yend*mri->ysize) ;
  printf("        zstart: %2.1f, zend: %2.1f\n",
         mri->zstart*mri->zsize, mri->zend*mri->zsize) ;
#else
  printf("        xstart: %2.1f, xend: %2.1f\n",mri->xstart, mri->xend) ;
  printf("        ystart: %2.1f, yend: %2.1f\n",mri->ystart, mri->yend) ;
  printf("        zstart: %2.1f, zend: %2.1f\n", mri->zstart, mri->zend) ;
#endif
  printf("            TR: %2.2f msec, TE: %2.2f msec, TI: %2.2f msec, "
         "flip angle: %2.2f degrees\n",
         mri->tr, mri->te, mri->ti, DEGREES(mri->flip_angle)) ;
  printf("       nframes: %d\n", mri->nframes) ;
  if(mri->pedir) printf("       PhEncDir: %s\n", mri->pedir);
  else           printf("       PhEncDir: UNKNOWN\n");
  printf("ras xform %spresent\n", mri->ras_good_flag ? "" : "not ") ;
  printf("    xform info: x_r = %8.4f, y_r = %8.4f, z_r = %8.4f, "
         "c_r = %10.4f\n",
         mri->x_r, mri->y_r, mri->z_r, mri->c_r);
  printf("              : x_a = %8.4f, y_a = %8.4f, z_a = %8.4f, "
         "c_a = %10.4f\n",
         mri->x_a, mri->y_a, mri->z_a, mri->c_a);
  printf("              : x_s = %8.4f, y_s = %8.4f, z_s = %8.4f, "
         "c_s = %10.4f\n",
         mri->x_s, mri->y_s, mri->z_s, mri->c_s);

  if (fio_IsDirectory(fname))
    printf("\ntalairach xfm : %s\n", mri->transform_fname);
  else {
    char *ext = 0;
    ext = fio_extension(fname);
    if (ext) {
      if (strcmp(ext, "mgz") == 0 || strcmp(ext, "mgh")==0)
        printf("\ntalairach xfm : %s\n", mri->transform_fname);
      free(ext);
    }
  }
  MRIdircosToOrientationString(mri,ostr);
  printf("Orientation   : %s\n",ostr);
  printf("Primary Slice Direction: %s\n",MRIsliceDirectionName(mri));
  m = MRIgetVoxelToRasXform(mri) ; // extract_i_to_r(mri) (just macto)
  printf("\nvoxel to ras transform:\n") ;
  PrettyMatrixPrint(m) ;
  printf("\nvoxel-to-ras determinant %g\n",MatrixDeterminant(m));
  MatrixFree(&m) ;
  m = extract_r_to_i(mri);
  printf("\nras to voxel transform:\n");
  PrettyMatrixPrint(m);
  MatrixFree(&m);
  MRIfree(&mri);

  return;

} /* end do_file */

