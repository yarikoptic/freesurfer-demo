/**
 * @file  mri_mcsim.c
 * @brief REPLACE_WITH_ONE_LINE_SHORT_DESCRIPTION
 *
 * REPLACE_WITH_LONG_DESCRIPTION_OR_REFERENCE
 */
/*
 * Original Author: REPLACE_WITH_FULL_NAME_OF_CREATING_AUTHOR 
 * CVS Revision Info:
 *    $Author: greve $
 *    $Date: 2011/01/04 14:13:09 $
 *    $Revision: 1.10.2.4 $
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
double round(double x);
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

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

static char vcid[] = "$Id: mri_mcsim.c,v 1.10.2.4 2011/01/04 14:13:09 greve Exp $";
char *Progname = NULL;
char *cmdline, cwd[2000];
int debug=0;
int checkoptsonly=0;
struct utsname uts;

char *OutTop = NULL;
char *csdbase = NULL;
char *subject = NULL;
char *surfname = "white";
char *hemi = NULL;
char *MaskFile = NULL;
char *LabelFile = "cortex";
int nRepetitions = -1;
int SynthSeed = -1;

int nThreshList;
double ThreshList[100];
int nFWHMList;
double FWHMList[100];
int SignList[3] = {-1,0,1}, nSignList=3;
char *DoneFile = NULL;
char *LogFile = NULL;
int SaveMask = 1;
int UseAvgVtxArea = 0;

/*---------------------------------------------------------------*/
int main(int argc, char *argv[]) {
  int nargs, n, msecTime, err;
  char tmpstr[2000], *signstr=NULL,*SUBJECTS_DIR, fname[2000];
  //char *OutDir = NULL;
  MRIS *surf;
  RFS *rfs;
  int *nSmoothsList, nSmoothsPrev, nSmoothsDelta, nthRep;
  MRI *z, *zabs=NULL, *sig=NULL, *p=NULL, *mask=NULL;
  int FreeMask = 0;
  int nthSign, nthFWHM, nthThresh;
  CSD *csdList[100][100][3], *csd;
  double sigmax, zmax, threshadj, csize, csizeavg, searchspace,avgvtxarea;
  int csizen;
  int nClusters, cmax,rmax,smax, nmask;
  SURFCLUSTERSUM *SurfClustList;
  struct timeb  mytimer;
  LABEL *clabel;
  FILE *fp, *fpLog=NULL;

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

  if(LogFile){
    fpLog = fopen(LogFile,"w");
    if(fpLog == NULL){
      printf("ERROR: opening %s\n",LogFile);
      exit(1);
    }
    dump_options(fpLog);
  } 

  if(SynthSeed < 0) SynthSeed = PDFtodSeed();
  srand48(SynthSeed);

  SUBJECTS_DIR = getenv("SUBJECTS_DIR");

  // Create output directory
  printf("Creating %s\n",OutTop);
  err = fio_mkdirp(OutTop,0777);
  if(err) exit(1);
  for(nthFWHM=0; nthFWHM < nFWHMList; nthFWHM++){
    for(nthThresh = 0; nthThresh < nThreshList; nthThresh++){
      for(nthSign = 0; nthSign < nSignList; nthSign++){
	if(SignList[nthSign] ==  0) signstr = "abs"; 
	if(SignList[nthSign] == +1) signstr = "pos"; 
	if(SignList[nthSign] == -1) signstr = "neg"; 
	sprintf(tmpstr,"%s/fwhm%02d/%s/th%02d",
		OutTop,(int)round(FWHMList[nthFWHM]),
		signstr,(int)round(10*ThreshList[nthThresh]));
	sprintf(fname,"%s/%s.csd",tmpstr,csdbase);
	if(fio_FileExistsReadable(fname)){
	  printf("ERROR: output file %s exists\n",fname);
	  if(fpLog) fprintf(fpLog,"ERROR: output file %s exists\n",fname);
          exit(1);
	}
	err = fio_mkdirp(tmpstr,0777);
	if(err) exit(1);
      }
    }
  }

  // Load the target surface
  sprintf(tmpstr,"%s/%s/surf/%s.%s",SUBJECTS_DIR,subject,hemi,surfname);
  printf("Loading %s\n",tmpstr);
  surf = MRISread(tmpstr);
  if(!surf) return(1);

  // Handle masking
  if(LabelFile){
    printf("Loading label file %s\n",LabelFile);
    sprintf(tmpstr,"%s/%s/label/%s.%s.label",
	    SUBJECTS_DIR,subject,hemi,LabelFile);
    if(!fio_FileExistsReadable(tmpstr)){
      printf(" Cannot find label file %s\n",tmpstr);
      sprintf(tmpstr,"%s",LabelFile);
      printf(" Trying label file %s\n",tmpstr);
      if(!fio_FileExistsReadable(tmpstr)){
	printf("  ERROR: cannot read or find label file %s\n",LabelFile);
	exit(1);
      }
    }
    printf("Loading %s\n",tmpstr);
    clabel = LabelRead(NULL, tmpstr);
    mask = MRISlabel2Mask(surf, clabel, NULL);
    FreeMask = 1;
  }
  if(MaskFile){
    printf("Loading %s\n",MaskFile);
    mask = MRIread(MaskFile);
    if(mask == NULL) exit(1);
  }
  if(mask && SaveMask){
    sprintf(tmpstr,"%s/mask.mgh",OutTop);
    printf("Saving mask to %s\n",tmpstr);
    err = MRIwrite(mask,tmpstr);
    if(err) exit(1);
  }

  // Compute search space
  searchspace = 0;
  nmask = 0;
  for(n=0; n < surf->nvertices; n++){
    if(mask && MRIgetVoxVal(mask,n,0,0,0) < 0.5) continue;
    searchspace += surf->vertices[n].area;
    nmask++;
  }
  printf("Found %d voxels in mask\n",nmask);
  if(surf->group_avg_surface_area > 0)
    searchspace *= (surf->group_avg_surface_area/surf->total_area);
  printf("search space %g mm2\n",searchspace);
  avgvtxarea = searchspace/nmask;
  printf("average vertex area %g mm2\n",avgvtxarea);

  // Determine how many iterations are needed for each FWHM
  nSmoothsList = (int *) calloc(sizeof(int),nFWHMList);
  for(nthFWHM=0; nthFWHM < nFWHMList; nthFWHM++){
    nSmoothsList[nthFWHM] = MRISfwhm2niters(FWHMList[nthFWHM], surf);
    printf("%2d %5.1f  %4d\n",nthFWHM,FWHMList[nthFWHM],nSmoothsList[nthFWHM]);
    if(fpLog) fprintf(fpLog,"%2d %5.1f  %4d\n",nthFWHM,FWHMList[nthFWHM],nSmoothsList[nthFWHM]);
  }
  printf("\n");

  // Allocate the CSDs
  for(nthFWHM=0; nthFWHM < nFWHMList; nthFWHM++){
    for(nthThresh = 0; nthThresh < nThreshList; nthThresh++){
      for(nthSign = 0; nthSign < nSignList; nthSign++){
	csd = CSDalloc();
	sprintf(csd->simtype,"%s","null-z");
	sprintf(csd->anattype,"%s","surface");
	sprintf(csd->subject,"%s",subject);
	sprintf(csd->hemi,"%s",hemi);
	sprintf(csd->contrast,"%s","NA");
	csd->seed = SynthSeed;
	csd->nreps = nRepetitions;
	csd->thresh = ThreshList[nthThresh];
	csd->threshsign = SignList[nthSign];
	csd->nullfwhm = FWHMList[nthFWHM];
	csd->varfwhm = -1;
	csd->searchspace = searchspace;
	CSDallocData(csd);
	csdList[nthFWHM][nthThresh][nthSign] = csd;
      }
    }
  }

  // Alloc the z map
  z = MRIallocSequence(surf->nvertices, 1,1, MRI_FLOAT, 1);

  // Set up the random field specification
  rfs = RFspecInit(SynthSeed,NULL);
  rfs->name = strcpyalloc("gaussian");
  rfs->params[0] = 0;
  rfs->params[1] = 1;

  printf("Thresholds (%d): ",nThreshList);
  for(n=0; n < nThreshList; n++) printf("%5.2f ",ThreshList[n]);
  printf("\n");
  printf("Signs (%d): ",nSignList);
  for(n=0; n < nSignList; n++)  printf("%2d ",SignList[n]);
  printf("\n");
  printf("FWHM (%d): ",nFWHMList);
  for(n=0; n < nFWHMList; n++) printf("%5.2f ",FWHMList[n]);
  printf("\n");

  // Start the simulation loop
  printf("\n\nStarting Simulation over %d Repetitions\n",nRepetitions);
  if(fpLog) fprintf(fpLog,"\n\nStarting Simulation over %d Repetitions\n",nRepetitions);
  TimerStart(&mytimer) ;
  for(nthRep = 0; nthRep < nRepetitions; nthRep++){
    msecTime = TimerStop(&mytimer) ;
    printf("%5d %7.1f ",nthRep,(msecTime/1000.0)/60);
    if(fpLog) {
      fprintf(fpLog,"%5d %7.1f ",nthRep,(msecTime/1000.0)/60);
      fflush(fpLog);
    }
    // Synthesize an unsmoothed z map
    RFsynth(z,rfs,mask); 
    nSmoothsPrev = 0;
    
    // Loop through FWHMs
    for(nthFWHM=0; nthFWHM < nFWHMList; nthFWHM++){
      printf("%d ",nthFWHM);
      if(fpLog) {
	fprintf(fpLog,"%d ",nthFWHM);
	fflush(fpLog);
      }
      nSmoothsDelta = nSmoothsList[nthFWHM] - nSmoothsPrev;
      nSmoothsPrev = nSmoothsList[nthFWHM];
      // Incrementally smooth z
      MRISsmoothMRI(surf, z, nSmoothsDelta, mask, z); // smooth z
      // Rescale
      RFrescale(z,rfs,mask,z);
      // Slightly tortured way to get the right p-values because
      //   RFstat2P() computes one-sided, but I handle sidedness
      //   during thresholding.
      // First, use zabs to get a two-sided pval bet 0 and 0.5
      zabs = MRIabs(z,zabs);
      p = RFstat2P(zabs,rfs,mask,0,p);
      // Next, mult pvals by 2 to get two-sided bet 0 and 1
      MRIscalarMul(p,p,2.0);
      sig = MRIlog10(p,NULL,sig,1); // sig = -log10(p)
      for(nthThresh = 0; nthThresh < nThreshList; nthThresh++){
	for(nthSign = 0; nthSign < nSignList; nthSign++){
	  csd = csdList[nthFWHM][nthThresh][nthSign];

	  // If test is not ABS then apply the sign
	  if(csd->threshsign != 0) MRIsetSign(sig,z,0);
	  // Get the max stats
	  sigmax = MRIframeMax(sig,0,mask,csd->threshsign,
			       &cmax,&rmax,&smax);
	  zmax = MRIgetVoxVal(z,cmax,rmax,smax,0);
	  if(csd->threshsign == 0){
	    zmax = fabs(zmax);
	    sigmax = fabs(sigmax);
	  }
	  // Mask
	  if(mask) MRImask(sig,mask,sig,0.0,0.0);

	  // Surface clustering
	  MRIScopyMRI(surf, sig, 0, "val");
	  if(csd->threshsign == 0) threshadj = csd->thresh;
	  else threshadj = csd->thresh - log10(2.0); // one-sided test
	  SurfClustList = sclustMapSurfClusters(surf,threshadj,-1,csd->threshsign,
						0,&nClusters,NULL);
	  // Actual area of cluster with max area
	  csize  = sclustMaxClusterArea(SurfClustList, nClusters);
	  // Number of vertices of cluster with max number of vertices. 
	  // Note: this may be a different cluster from above!
	  csizen = sclustMaxClusterCount(SurfClustList, nClusters);
	  // Area of this cluster based on average vertex area. This just scales
	  // the number of vertices.
	  csizeavg = csizen * avgvtxarea;
	  if(UseAvgVtxArea) csize = csizeavg;
	  // Store results
	  csd->nClusters[nthRep] = nClusters;
	  csd->MaxClusterSize[nthRep] = csize;
	  csd->MaxSig[nthRep] = sigmax;
	  csd->MaxStat[nthRep] = zmax;
	} // Sign
      } // Thresh
    } // FWHM
    printf("\n");
    if(fpLog) fprintf(fpLog,"\n");
  } // Simulation Repetition

  msecTime = TimerStop(&mytimer) ;
  printf("Total Sim Time %g min (%g per rep)\n",
	 msecTime/(1000*60.0),(msecTime/(1000*60.0))/nRepetitions);
  if(fpLog) fprintf(fpLog,"Total Sim Time %g min (%g per rep)\n",
		    msecTime/(1000*60.0),(msecTime/(1000*60.0))/nRepetitions);

  // Save output
  printf("Saving results\n");
  for(nthFWHM=0; nthFWHM < nFWHMList; nthFWHM++){
    for(nthThresh = 0; nthThresh < nThreshList; nthThresh++){
      for(nthSign = 0; nthSign < nSignList; nthSign++){
	csd = csdList[nthFWHM][nthThresh][nthSign];
	if(csd->threshsign ==  0) signstr = "abs"; 
	if(csd->threshsign == +1) signstr = "pos"; 
	if(csd->threshsign == -1) signstr = "neg"; 
	sprintf(tmpstr,"%s/fwhm%02d/%s/th%02d/%s.csd",
		OutTop,(int)round(FWHMList[nthFWHM]),
		signstr,(int)round(10*ThreshList[nthThresh]),
		csdbase);
	fp = fopen(tmpstr,"w");
	fprintf(fp,"# ClusterSimulationData 2\n");
	fprintf(fp,"# mri_mcsim\n");
	fprintf(fp,"# %s\n",cmdline);
	fprintf(fp,"# %s\n",vcid);
	fprintf(fp,"# hostname %s\n",uts.nodename);
	fprintf(fp,"# machine  %s\n",uts.machine);
	fprintf(fp,"# runtime_min %g\n",msecTime/(1000*60.0));
	fprintf(fp,"# nvertices-total %d\n",surf->nvertices);
	fprintf(fp,"# nvertices-search %d\n",nmask);
	if(mask) fprintf(fp,"# masking 1\n");
	else     fprintf(fp,"# masking 0\n");
	fprintf(fp,"# SmoothLevel %d\n",nSmoothsList[nthFWHM]);
	fprintf(fp,"# UseAvgVtxArea %d\n",UseAvgVtxArea);
	CSDprint(fp, csd);
	fclose(fp);
      }
    }
  }
  if(DoneFile){
    fp = fopen(DoneFile,"w");
    fprintf(fp,"%g\n",msecTime/(1000*60.0));
    fclose(fp);
  }
  printf("mri_mcsim done\n");
  if(fpLog) fprintf(fpLog,"mri_mcsim done\n");
  exit(0);
}
/* --------------------------------------------- */
static int parse_commandline(int argc, char **argv) {
  int  nargc , nargsused;
  char **pargv, *option ;
  double fwhm;

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
    else if (!strcasecmp(option, "--test")){
      double fwhm;
      nFWHMList = 0;
      for(fwhm = 3; fwhm <= 6; fwhm++){
	FWHMList[nFWHMList] = fwhm;
	nFWHMList++;
      }
      nThreshList = 2;
      ThreshList[0] = 2.0;
      ThreshList[1] = 3.0; 
      nSignList = 2;
      subject = "fsaverage5";
      hemi = "lh";
      nRepetitions = 2;
      csdbase = "junk";
      SynthSeed = 53;
    }
    else if (!strcasecmp(option, "--o")) {
      if(nargc < 1) CMDargNErr(option,1);
      OutTop = pargv[0];
      nargsused = 1;
    } 
    else if (!strcasecmp(option, "--done")) {
      if(nargc < 1) CMDargNErr(option,1);
      DoneFile = pargv[0];
      nargsused = 1;
    } 
    else if (!strcasecmp(option, "--log")) {
      if(nargc < 1) CMDargNErr(option,1);
      LogFile = pargv[0];
      nargsused = 1;
    } 
    else if (!strcasecmp(option, "--base")) {
      if(nargc < 1) CMDargNErr(option,1);
      csdbase = pargv[0];
      nargsused = 1;
    } 
    else if (!strcasecmp(option, "--surf") || 
	     !strcasecmp(option, "--surface")) {
      if(nargc < 2) CMDargNErr(option,1);
      subject = pargv[0];
      hemi = pargv[1];
      nargsused = 2;
    } 
    else if (!strcasecmp(option, "--surfname")) {
      if(nargc < 1) CMDargNErr(option,1);
      surfname = pargv[0];
      nargsused = 1;
    } 
    else if (!strcasecmp(option, "--avgvtxarea"))    UseAvgVtxArea = 1;
    else if (!strcasecmp(option, "--no-avgvtxarea")) UseAvgVtxArea = 0;
    else if (!strcasecmp(option, "--seed")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%d",&SynthSeed);
      nargsused = 1;
    } 
    else if (!strcasecmp(option, "--mask")) {
      if(nargc < 1) CMDargNErr(option,1);
      MaskFile = pargv[0];
      LabelFile = NULL;
      nargsused = 2;
    } 
    else if (!strcasecmp(option, "--label")) {
      if(nargc < 1) CMDargNErr(option,1);
      LabelFile = pargv[0];
      MaskFile = NULL;
      nargsused = 1;
    } 
    else if (!strcasecmp(option, "--no-label")) {
      LabelFile = NULL;
    }
    else if (!strcasecmp(option, "--no-save-mask")) SaveMask = 0;
    else if (!strcasecmp(option, "--nreps")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%d",&nRepetitions);
      nargsused = 1;
    } 
    else if (!strcasecmp(option, "--fwhm")) {
      if (nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%lf",&fwhm);
      FWHMList[nFWHMList] = fwhm;
      nFWHMList++;
      nargsused = 1;
    } 
    else {
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
  printf("%s \n",Progname) ;
  printf("\n");
  printf("   --o top-output-dir\n");
  printf("   --base csdbase\n");
  printf("   --surface subjectname hemi\n");
  printf("   --nreps nrepetitions\n");
  printf("   --fwhm FWHM <--fwhm FWHM ...>\n");
  printf("   \n");
  printf("   --avgvtxarea : report cluster area based on average vtx area\n");
  printf("   --seed randomseed : default is to choose based on ToD\n");
  printf("   --label labelfile : default is ?h.cortex.label \n");
  printf("   --mask maskfile : instead of label\n");
  printf("   --no-label : do not use a label to mask\n");
  printf("   --no-save-mask : do not save mask to output (good for mult jobs)\n");
  printf("   --surfname surfname : default is white\n");
  printf("\n");
  printf("   --log  LogFile \n");
  printf("   --done DoneFile : will create DoneFile when finished\n");
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
  exit(1) ;
}
/* --------------------------------------------- */
static void print_version(void) {
  printf("%s\n", vcid) ;
  exit(1) ;
}
/* --------------------------------------------- */
static void check_options(void) {
  if(subject == NULL) {
    printf("ERROR: must specify a surface\n");
    exit(1);
  }
  if(OutTop == NULL){
    printf("ERROR: need to spec an output directory\n");
    exit(1);
  }
  if(csdbase==NULL) {
    printf("ERROR: need to specify a csd base\n");
    exit(1);
  }
  if(MaskFile && LabelFile) {
    printf("ERROR: cannot specify both a mask and a label\n");
    exit(1);
  }
  if(nRepetitions < 1) {
    printf("ERROR: need to specify number of simulation repitions\n");
    exit(1);
  }
  if(nFWHMList == 0){
    double fwhm;
    nFWHMList = 0;
    for(fwhm = 1; fwhm <= 30; fwhm++){
      FWHMList[nFWHMList] = fwhm;
      nFWHMList++;
    }
  }
  if(nThreshList == 0){
    nThreshList = 6;
    ThreshList[0] = 1.3; 
    ThreshList[1] = 2.0;
    ThreshList[2] = 2.3; 
    ThreshList[3] = 3.0; 
    ThreshList[4] = 3.3; 
    ThreshList[5] = 4.0;
  }
  return;
}

/* --------------------------------------------- */
static void dump_options(FILE *fp) {
  fprintf(fp,"\n");
  fprintf(fp,"%s\n",vcid);
  fprintf(fp,"cwd %s\n",cwd);
  fprintf(fp,"cmdline %s\n",cmdline);
  fprintf(fp,"sysname  %s\n",uts.sysname);
  fprintf(fp,"hostname %s\n",uts.nodename);
  fprintf(fp,"machine  %s\n",uts.machine);
  fprintf(fp,"user     %s\n",VERuser());
  fprintf(fp,"OutTop  %s\n",OutTop);
  fprintf(fp,"CSDBase  %s\n",csdbase);
  fprintf(fp,"nreps    %d\n",nRepetitions);
  fprintf(fp,"subject  %s\n",subject);
  fprintf(fp,"hemi     %s\n",hemi);
  if(MaskFile) fprintf(fp,"mask     %s\n",MaskFile);
  fprintf(fp,"UseAvgVtxArea %d\n",UseAvgVtxArea);
  fflush(fp);
  return;
}
