#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
// MacOSX does not have malloc.h but declaration is in stdlib.h
#ifndef __APPLE__
#include <malloc.h>
#endif   
#include <errno.h>
#include <ctype.h>
#include "mri.h"
#include "diag.h"
#include "error.h"
#include "utils.h"
#include "DICOMRead.h"
#include "fio.h"
#include "version.h"

// This should be defined in ctype.h but is not (at least on centos)
int isblank(int c);

int main(int argc, char *argv[]) ;

static char vcid[] = "$Id: mri_parse_sdcmdir.c,v 1.10.2.1 2005/05/25 20:20:41 greve Exp $";
char *Progname = NULL;

static int  parse_commandline(int argc, char **argv);
static void check_options(void);
static void print_usage(void) ;
static void usage_exit(void);
static void print_help(void) ;
static void print_version(void) ;
static void argnerr(char *option, int n);
static int  singledash(char *flag);

static int   strip_leading_slashes(char *s);
static char *strip_white_space(char *s);

int debug, verbose;
char* sdicomdir = NULL; 
char *outfile;
FILE *outstream;
int summarize = 0;
int sortbyrun = 0;
int TRSlice = 0;

/*---------------------------------------------------------------*/
int main(int argc, char **argv)
{
  SDCMFILEINFO **sdfi_list;
  SDCMFILEINFO  *sdfi;
  int nlist;
  int NRuns;
  int nthfile;
  char *fname, *psname, *protoname;
  int PrevRunNo;

  Progname = argv[0] ;
  argc --;
  argv++;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  if(argc == 0) usage_exit();

  parse_commandline(argc, argv);
  check_options();

  strip_leading_slashes(sdicomdir);

  /* open the output file, or set output stream to stdout */
  if(outfile != NULL){
    outstream = fopen(outfile,"w");
    if(outstream == NULL){
      fprintf(stderr,"ERROR: could not open %s for writing\n",outfile);
      exit(1);
    }
  }
  else outstream = stdout;

  /* Get all the Siemens DICOM files from this directory */
  fprintf(stderr,"INFO: scanning path to Siemens DICOM DIR:\n   %s\n",
	  sdicomdir);
  sdfi_list = ScanSiemensDCMDir(sdicomdir, &nlist);
  if(sdfi_list == NULL){
    fprintf(stderr,"ERROR: scanning directory %s\n",sdicomdir);
    exit(1);
  }
  printf("INFO: found %d Siemens files\n",nlist);

  /* Sort the files by Series, Slice Position, and Image Number */
  printf("Sorting\n");
  fflush(stdout);fflush(stderr);
  SortSDCMFileInfo(sdfi_list,nlist);

  /* Assign run numbers to each file (count number of runs)*/
  if(sortbyrun){
    fprintf(stderr,"Assigning Run Numbers\n");
    NRuns = sdfiAssignRunNo2(sdfi_list, nlist);
    if(NRuns == 0){
      fprintf(stderr,"ERROR: sorting runs\n");
      exit(1);
    }
  }

  PrevRunNo = -1;
  for(nthfile = 0; nthfile < nlist; nthfile++){

    sdfi = sdfi_list[nthfile];

    if(summarize && PrevRunNo == sdfi->RunNo) continue;
    PrevRunNo = sdfi->RunNo;

    sdfi->RepetitionTime /= 1000.0;

    // Old version of Siemens software had reptime as time between
    // slices. New has reptime as time between volumes. When unpacking
    // old version, use --tr-slice to make TRSlice=1. The default
    // is for TRSlice=0. This is also the behavior with --tr-vol
    if(sdfi->IsMosaic && TRSlice) sdfi->RepetitionTime *= sdfi->VolDim[2];

    fname     = fio_basename(sdfi->FileName,NULL);
    psname    = strip_white_space(sdfi->PulseSequence);
    protoname = strip_white_space(sdfi->ProtocolName);

    fprintf(outstream,
      "%4d %s  %3d %d  %4d %d  %3d %3d %3d %3d  %8.4f %8.4f %s\n",
      nthfile+1, fname,
      sdfi->SeriesNo, sdfi->ErrorFlag,
      sdfi->ImageNo, sdfi->IsMosaic,
      sdfi->VolDim[0], sdfi->VolDim[1], sdfi->VolDim[2], sdfi->NFrames, 
      sdfi->RepetitionTime, sdfi->EchoTime,
      protoname);
    fflush(outstream);
    free(fname);
    free(psname);
    free(protoname);
  }

  while(nlist--)
  {
    // free strings
    free(sdfi_list[nlist]->FileName);
    free(sdfi_list[nlist]->StudyDate);
    free(sdfi_list[nlist]->StudyTime);
    free(sdfi_list[nlist]->PatientName);
    free(sdfi_list[nlist]->SeriesTime);
    free(sdfi_list[nlist]->AcquisitionTime);
    free(sdfi_list[nlist]->ScannerModel);
    free(sdfi_list[nlist]->NumarisVer);
    free(sdfi_list[nlist]->PulseSequence);
    free(sdfi_list[nlist]->ProtocolName);
    free(sdfi_list[nlist]->PhEncDir);
    //
    free(sdfi_list[nlist]);
  }
  free(sdfi_list);

  if(outfile != NULL) fclose(outstream);

  return(0);
}
/*---------------------------------------------------------------*/
/* -----//////////////////<<<<<<<<.>>>>>>>>>>\\\\\\\\\\\\\\\\--- */
/*---------------------------------------------------------------*/

/* --------------------------------------------- */
static int parse_commandline(int argc, char **argv)
{
  int  nargc , nargsused;
  char **pargv, *option ;
  FILE *fptmp;
  int nargs;

  /* rkt: check for and handle version tag */
  nargs = handle_version_option (argc, argv, "$Id: mri_parse_sdcmdir.c,v 1.10.2.1 2005/05/25 20:20:41 greve Exp $", "$Name:  $");
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  if(argc < 1) usage_exit();

  nargc   = argc;
  pargv = argv;
  while(nargc > 0){

    option = pargv[0];
    if(debug) printf("%d %s\n",nargc,option);
    nargc -= 1;
    pargv += 1;

    nargsused = 0;

    if      (!strcasecmp(option, "--help"))    print_help() ;
    else if (!strcasecmp(option, "--version")) print_version() ;
    else if (!strcasecmp(option, "--debug"))   debug = 1;
    else if (!strcasecmp(option, "--verbose")) verbose = 1;
    else if (!strcasecmp(option, "--tr-slice")) TRSlice = 1;
    else if (!strcasecmp(option, "--tr-vol"))   TRSlice = 0;
    else if (!strcmp(option, "--d")){
      if(nargc < 1) argnerr(option,1);
      sdicomdir = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--o")){
      if(nargc < 1) argnerr(option,1);
      outfile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--summarize") || !strcmp(option, "--sum")){
      summarize = 1;
    }
    else if (!strcmp(option, "--status")){
      if(nargc < 1) argnerr(option,1);
      SDCMStatusFile = (char *) calloc(strlen(pargv[0])+1,sizeof(char));
      memcpy(SDCMStatusFile,pargv[0],strlen(pargv[0]));
      fptmp = fopen(SDCMStatusFile,"w");
      if(fptmp == NULL){
  fprintf(stderr,"ERROR: could not open %s for writing\n",
    SDCMStatusFile);
  exit(1);
      }
      fprintf(fptmp,"0\n");
      fclose(fptmp);
      nargsused = 1;
    }
    else if (!strcmp(option, "--sortbyrun")){
      sortbyrun = 1;
    }
    else{
      fprintf(stderr,"ERROR: Option %s unknown\n",option);
      if(singledash(option))
  fprintf(stderr,"       Did you really mean -%s ?\n",option);
      exit(-1);
    }
    nargc -= nargsused;
    pargv += nargsused;
  }
  return(0);
}
/* ------------------------------------------------------ */
static void usage_exit(void)
{
  print_usage() ;
  exit(1) ;
}
/* --------------------------------------------- */
static void print_usage(void)
{
  fprintf(stdout, "USAGE: %s \n",Progname) ;
  fprintf(stdout, "\n");
  fprintf(stdout, "   --d sdicomdir  : path to siemens dicom directory \n");
  fprintf(stdout, "   --o outfile    : write results to outfile (default is stdout)\n");
  fprintf(stdout, "   --sortbyrun    : assign run numbers\n");
  fprintf(stdout, "   --summarize    : only print out info for run leaders\n");
  fprintf(stdout, "   --help         : how to use this program \n");
  fprintf(stdout, "   --tr-vol       : TR defined to be time bet volumes (default)\n");
  fprintf(stdout, "   --tr-slice     : TR defined to be time bet slices\n");
  fprintf(stdout, "\n");
}
/* --------------------------------------------- */
static void print_help(void)
{
  printf("\n");
  print_usage() ;
  printf("\n");

  printf(
  "This program parses the Siemens DICOM files in a given directory, and \n"
  "prints out information about each file. The output is printed to stdout\n"
  "unless a file name is passed with the --o flag.\n"
  "\n"
  "The most useful information is that which cannot be easily obtained by \n"
  "probing a dicom file. This includes: the run number, number of frames \n"
  "in the run, number of slices in the run, and, for mosaics, the number \n"
  "of rows and cols in the volume.\n");

  printf("\n");

  printf("There are 14 columns in the output:\n");
  printf("  1. File Number\n");
  printf("  2. File Name \n");
  printf("  3. Series Number\n");
  printf("  4. Series Error Flag (1 for error)\n");
  printf("  5. Image Number\n");
  printf("  6. Mosaic Flag   (1 for mosaics)\n");
  printf("  7. Number of Rows    in the Volume\n");
  printf("  8. Number of Columns in the Volume\n");
  printf("  9. Number of Slices  in the Volume for the Series\n");
  printf(" 10. Number of Frames in the Series\n");
  printf(" 11. Repetition Time (sec)\n");
  printf(" 12. Echo Time (ms)\n");
  printf(" 13. Protocol Name  - white space stripped (but see BUGS)\n");

  printf("\n");
  printf("Arguments:\n");
  printf("\n");
  printf("  --d sdicomdir : this is the name of the directory where the \n");
  printf("      dicom files are located (required).\n");

  printf("\n");
  printf("  --o outfile : this is the name of a file to which the results will be \n");
  printf("      printed. If unspecified, the results will be printed to stdout.\n");

  printf("\n");
  printf("  --summarize : forces print out of information for the first file in the run.\n");
  printf("\n");
  printf("--tr-slice : interpret the repetition time for mosaics as the \n");
  printf("  time between slices. This was the case for older NUMARIS \n");
  printf("  versions. Default is to interpret as time between volumes. \n");
  printf("  Note: this only applies to mosaics.\n");
  printf("\n");
  printf("--tr-vol : interpret the repetition time for mosaics as the \n");
  printf("  time between volumes. See --tr-slice for more info. Only \n");
  printf("  applies to mosaics.\n");
  printf("\n");
  printf(
	 "BUGS:\n"
	 "Prior to 5/25/05, the protocol name was stripped of anything that\n"
	 "was not a number or letter. After 5/25/05 it is only stripped of\n"
	 "white space.\n");

  printf("\n");

  exit(1);
}
/* --------------------------------------------- */
static void check_options(void)
{

  if(sdicomdir == NULL){
    fprintf(stderr,"ERROR: no directory name supplied\n");
    exit(1);
  }

  if(summarize && ! sortbyrun ) {
    fprintf(stderr,"ERROR: must specify --sortbyrun with --summarize\n");
    exit(1);
  }
    

}
/* --------------------------------------------- */
static void print_version(void)
{
  fprintf(stderr, "%s\n", vcid) ;
  exit(1) ;
}
/* --------------------------------------------- */
static void argnerr(char *option, int n)
{
  if(n==1)
    fprintf(stderr,"ERROR: %s flag needs %d argument\n",option,n);
  else
    fprintf(stderr,"ERROR: %s flag needs %d arguments\n",option,n);
  exit(-1);
}
/*---------------------------------------------------------------*/
static int singledash(char *flag)
{
  int len;
  len = strlen(flag);
  if(len < 2) return(0);

  if(flag[0] == '-' && flag[1] != '-') return(1);
  return(0);
}
/*---------------------------------------------------------------*/
static int strip_leading_slashes(char *s)
{
  while(strlen(s) > 0 && s[strlen(s)-1] == '/') s[strlen(s)-1] = '\0';
  if(strlen(s) == 0) s[0] = '/';
  return(0);
}
/*---------------------------------------------------------------
  strip_white_space() - strips white space as well as anything
  that is not alpha-numeric from s. This is needed because
  when Siemens converts the protocol and pulse sequence names
  to DICOM, there's a lot of garbage that gets appended to the
  end. It looks like they forget to add a null terminator to
  the string. Prior to 5/25/05, it would strip out anything
  that was not a number or letter (eg, dashes and underscores).
---------------------------------------------------------------*/
static char * strip_white_space(char *s)
{
  char *s2;
  int l,n,m;

  if(s==NULL) return(NULL);

  l = strlen(s);
  s2 = (char *) calloc(l+1,sizeof(char));
  
  m = 0;
  for(n=0; n<l; n++){
    if(isascii(s[n]) && !isblank(s[n])){
      s2[m] = s[n];
      m++;
    }
  }

  /* Here's the "old" way to strip (ie, pre 5/25/05):
       if( isalnum(s[n]) && s[n] != ' ' && s[n] != '\t' ){
     The problem here is that it strips out too much, eg,
     dashes and underscores are stripped.
  */
  return(s2);
}
