/*******************************************************
   DICOM 3.0 reading functions
   Author: Sebastien Gicquel and Douglas Greve
   Date: 06/04/2001
   $Id: DICOMRead.c,v 1.54.2.1 2004/10/20 21:44:33 kteich Exp $
*******************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/file.h>
#include <sys/types.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include <ctype.h>
#include <dirent.h>
#ifndef Darwin
#include <malloc.h>
#else
void *malloc(size_t size);

#endif
#include <math.h>
#include "mri.h"
#include "mri_identify.h"
#include "fio.h"
#include "mosaic.h"
#include "diag.h"

#define _DICOMRead_SRC
#include "DICOMRead.h"
#undef _DICOMRead_SRC

static int DCMPrintCond(CONDITION cond);
void *ReadDICOMImage2(int nfiles, DICOMInfo **aDicomInfo, int startIndex);

static BOOL IsTagPresent[NUMBEROFTAGS];
static int sliceDirCosPresent;

//#define _DEBUG

/*-----------------------------------------------------------------
  sdcmLoadVolume() - this loads a volume stored in Siemens DICOM
  format. It scans in the directory of dcmfile searching for all
  Siemens DICOM files. It then determines which of these files belong
  to the same run as dcmfile. If LoadVolume=1, then the pixel data is
  loaded, otherwise, only the header info is assigned.

  Determining which files belong in the same with dcmfile is a 
  tricky process. It requires that all files be queried. It also
  requires that all runs be complete. A missing file will cause
  the process to crash even if the file does not belong to the 
  same run as dcmfile.

  Notes: 
    1. This currently works only for unsigned short data. 
    2. It handles mosaics but not supermosaics. 
    3. It assumes that slices within a mosaic are sorted in 
       anatomical order.
    4. It handles multiple frames for mosaics and non-mosaics.

  SDCMListFile is a file with a list of Siemens DICOM files to
  be unpacked as one run. If using mri_convert, it can be passed
  with the --sdcmlist.
  -----------------------------------------------------------------*/
MRI * sdcmLoadVolume(char *dcmfile, int LoadVolume, int nthonly)
{
  SDCMFILEINFO *sdfi;
  DCM_ELEMENT *element;
  SDCMFILEINFO **sdfi_list;
  int nthfile;
  int nlist;
  int ncols, nrows, nslices, nframes;
  int nmoscols, nmosrows;
  int mosrow, moscol, mosindex;
  int err,OutOfBounds,IsMosaic;
  int row, col, slice, frame;
  unsigned short *pixeldata;
  MRI *vol;
  char **SeriesList;
  char *tmpstring;
  int Maj, Min, MinMin;
  double xs,ys,zs,xe,ye,ze;
  double sign;
  int nnlist;

  xs=ys=zs=xe=ye=ze=sign=0.; /* to avoid compiler warnings */
  slice = 0; frame = 0; /* to avoid compiler warnings */

  sliceDirCosPresent = 0; // assume not present

  if(SDCMListFile != NULL)
    SeriesList = ReadSiemensSeries(SDCMListFile, &nlist, dcmfile);
  else
    SeriesList = ScanSiemensSeries(dcmfile,&nlist);

  if(nlist == 0){
    fprintf(stderr,"ERROR: could not find any files\n");
    return(NULL);
  }

  //for(n=0; n<nlist; n++) printf("%3d  %s\n",n,SeriesList[n]);
  //fflush(stdout);

  fprintf(stderr,"INFO: loading series header info.\n");
  sdfi_list = LoadSiemensSeriesInfo(SeriesList, nlist);

  // free memory
  nnlist = nlist;
  while (nnlist--)
    free(SeriesList[nnlist]);
  free(SeriesList);
  
  fprintf(stderr,"INFO: sorting.\n");
  SortSDCMFileInfo(sdfi_list,nlist);
  
  sdfiAssignRunNo2(sdfi_list, nlist);

  /* First File in the Run */
  if (nthonly < 0)
    sdfi = sdfi_list[0];
  else
    sdfi = sdfi_list[nthonly];

  /* there are some Siemens files don't have the slice dircos */
  if (sliceDirCosPresent == 0)
  {
    xs = sdfi_list[0]->ImgPos[0]; ys = sdfi_list[0]->ImgPos[1]; zs = sdfi_list[0]->ImgPos[2];
    xe = sdfi_list[nlist-1]->ImgPos[0]; ye = sdfi_list[nlist-1]->ImgPos[1]; ze = sdfi_list[nlist-1]->ImgPos[2];
    /* check sign. inner product of what we have so far with the vector we found */
    /* using two slices.  They should be parallel and thus we can use the sign.  */
    /* no need to normalize the vector, since we are interested in only sign.    */
    /* Note that DICOM is LPS ... we need RAS                                    */
    /* this means that first two must use xs-xe, ys-ye, but ze-zs                */
    sign = sdfi->Vs[0]*(xs-xe) + sdfi->Vs[1]*(ys-ye) + sdfi->Vs[2]*(ze-zs);
    if (sign < 0)
    {
      fprintf(stderr, "INFO: Handedness changed to left-handed.\n");
      sdfi->Vs[0] = -sdfi->Vs[0];
      sdfi->Vs[1] = -sdfi->Vs[1];
      sdfi->Vs[2] = -sdfi->Vs[2];
    }
  }

  sdfiFixImagePosition(sdfi);
  sdfiVolCenter(sdfi);

  /* for easy access */
  ncols   = sdfi->VolDim[0];
  nrows   = sdfi->VolDim[1];
  nslices = sdfi->VolDim[2];
  nframes = sdfi->NFrames;
  IsMosaic = sdfi->IsMosaic;

  // verify nthframe is within the range
  if (nthonly >= 0)
  {
    if (nthonly > nframes-1)
    {
      fprintf(stderr, "ERROR: only has %d frames (%d - %d) but called for %d\n",
	      nframes, 0, nframes-1, nthonly);
      fflush(stderr);
      return NULL;
    }
  }

  printf("INFO: (%3d %3d %3d), nframes = %d, ismosaic=%d\n",
	 ncols,nrows,nslices,nframes,IsMosaic);
  fflush(stdout);
  fflush(stdout);

  /** Allocate an MRI structure **/
  if(LoadVolume)
  {
    if (nthonly < 0)
      vol = MRIallocSequence(ncols,nrows,nslices,MRI_SHORT,nframes);
    else
      vol = MRIallocSequence(ncols,nrows,nslices,MRI_SHORT,1);

    if(vol==NULL){
      fprintf(stderr,"ERROR: could not alloc MRI volume\n");
      fflush(stderr);
      return(NULL);
    }
  }
  else{
    vol = MRIallocHeader(ncols,nrows,nslices,MRI_SHORT);
    if(vol==NULL){
      fprintf(stderr,"ERROR: could not alloc MRI header \n");
      fflush(stderr);
      return(NULL);
    }
  }

  /* set the various paramters for the mri structure */
  vol->xsize = sdfi->VolRes[0]; /* x = col */
  vol->ysize = sdfi->VolRes[1]; /* y = row */
  vol->zsize = sdfi->VolRes[2]; /* z = slice */
  vol->x_r   = sdfi->Vc[0];
  vol->x_a   = sdfi->Vc[1];
  vol->x_s   = sdfi->Vc[2];
  vol->y_r   = sdfi->Vr[0];
  vol->y_a   = sdfi->Vr[1];
  vol->y_s   = sdfi->Vr[2];
  vol->z_r   = sdfi->Vs[0];
  vol->z_a   = sdfi->Vs[1];
  vol->z_s   = sdfi->Vs[2];
  vol->c_r   = sdfi->VolCenter[0];
  vol->c_a   = sdfi->VolCenter[1];
  vol->c_s   = sdfi->VolCenter[2];
  vol->ras_good_flag = 1;
  vol->te    = sdfi->EchoTime;
  vol->ti    = sdfi->InversionTime;
  vol->flip_angle  = sdfi->FlipAngle;
  if(! sdfi->IsMosaic )
    vol->tr    = sdfi->RepetitionTime;
  else{
    /* The TR definition will depend upon the software version */
    tmpstring = sdcmExtractNumarisVer(sdfi->NumarisVer, &Maj, &Min, &MinMin);
    if(tmpstring == NULL) return(NULL);
    free(tmpstring);
    if(Min == 1 && MinMin <= 6)
      vol->tr = sdfi->RepetitionTime * (sdfi->VolDim[2]) / 1000.0; 
    else
      vol->tr  = sdfi->RepetitionTime / 1000.0; 
    /* Need to add any gap (eg, as in a hammer sequence */
  }

  /* Return now if we're not loading pixel data */
  if(!LoadVolume) return(vol);

  /* Dump info about the first file to stdout */
  DumpSDCMFileInfo(stdout,sdfi);
  // verification of number of files vs. slices
  if (nlist > nslices*nframes)
  {
    fprintf(stderr, "ERROR: nlist (%d) > nslices (%d) x frames (%d)\n",
	    nlist, nslices, nframes);
    fprintf(stderr, "ERROR: dump file list into fileinfo.txt.\n");
    fprintf(stderr, "ERROR: check for consistency\n");
    {
      FILE *fp;
      int i;
      fp = fopen("./fileinfo.txt", "w");
      for (i=0; i < nlist;++i)
	DumpSDCMFileInfo(fp, sdfi_list[i]);
      fclose(fp);
    }
    fprintf(stderr, "ERROR: set nlist = nslices*nframes.\n");
    nlist = nslices*nframes;
  }
  /* ------- Go through each file in the Run ---------*/
  for(nthfile = 0; nthfile < nlist; nthfile ++){

    sdfi = sdfi_list[nthfile];

    /* Get the pixel data */
    element = GetElementFromFile(sdfi->FileName,0x7FE0,0x10);
    if(element == NULL){
      fprintf(stderr,"ERROR: reading pixel data from %s\n",sdfi->FileName);
      MRIfree(&vol);
    }
    pixeldata = (unsigned short *)(element->d.string);

    if(!IsMosaic){/*---------------------------------------------*/
      /* It's not a mosaic -- load rows and cols from pixel data */
      if(nthfile == 0)
      {
	frame = 0;
	slice = 0;
      }
#ifdef _DEBUG      
      printf("%3d %3d %3d %s \n",nthfile,slice,frame,sdfi->FileName);
      fflush(stdout);
#endif
      if (nthonly <0)
      {
	for(row=0; row < nrows; row++)
	{
	  for(col=0; col < ncols; col++)
	  {
	    MRISseq_vox(vol,col,row,slice,frame) = *(pixeldata++);
	  }
	}
      }
      // only copy a particular frame
      else if (frame == nthonly)
      {
	for(row=0; row < nrows; row++)
	{
	  for(col=0; col < ncols; col++)
	  {
	    MRISvox(vol,col,row,slice) = *(pixeldata++);
	  }
	}
      }
      frame ++;
      if(frame >= nframes)
      {
	frame = 0;
	slice ++;
      }
    }
    else{/*---------------------------------------------*/
      /* It is a mosaic -- load entire volume for this frame from pixel data */
      frame = nthfile;
      nmoscols = sdfi->NImageCols;
      nmosrows = sdfi->NImageRows;
      for(row=0; row < nrows; row++){
	for(col=0; col < ncols; col++){
	  for(slice=0; slice < nslices; slice++){
	    /* compute the mosaic col and row from the volume
	       col, row , and slice */
	    err = VolSS2MosSS(col, row, slice, 
			      ncols, nrows, 
			      nmoscols, nmosrows,
			      &moscol, &mosrow, &OutOfBounds);
	    if(err || OutOfBounds){
	      FreeElementData(element);
	      free(element);
	      MRIfree(&vol);
	      exit(1);
	    }
	    /* Compute the linear index into the block of pixel data */
	    mosindex = moscol + mosrow * nmoscols;
	    MRISseq_vox(vol,col,row,slice,frame) = *(pixeldata + mosindex);
	  }
	}
      }
    }
    
    FreeElementData(element);
    free(element);

  }/* for nthfile */

  while (nlist--)
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
    // free struct
    free(sdfi_list[nlist]);
  }
  free(sdfi_list);

  return(vol);
}
/*---------------------------------------------------------------
  GetElementFromFile() - gets an element from a DICOM file. Returns
  a pointer to the object (or NULL upon failure).
  Author: Douglas Greve 9/6/2001
---------------------------------------------------------------*/
DCM_ELEMENT *GetElementFromFile(char *dicomfile, long grpid, long elid)
{
  DCM_OBJECT *object=0;
  CONDITION cond;
  DCM_ELEMENT *element;
  DCM_TAG tag;
  unsigned int rtnLength;
  void * Ctx = NULL;

  element = (DCM_ELEMENT *) calloc(1,sizeof(DCM_ELEMENT));

  object = GetObjectFromFile(dicomfile, 0);
  if(object == NULL) exit(1);

  tag=DCM_MAKETAG(grpid,elid);
  cond = DCM_GetElement(&object, tag, element);
  if(cond != DCM_NORMAL){
    DCM_CloseObject(&object);
    free(element);
    return(NULL);
  }
  AllocElementData(element);
  cond = DCM_GetElementValue(&object, element, &rtnLength, &Ctx);
  /* Does Ctx have to be freed? */
  if(cond != DCM_NORMAL){
    DCM_CloseObject(&object);
    FreeElementData(element);
    free(element);
    return(NULL);
  }
  DCM_CloseObject(&object);

  COND_PopCondition(1); /********************************/

  return(element);
}
/*---------------------------------------------------------------
  GetObjectFromFile() - gets an object from a DICOM file. Returns
  a pointer to the object (or NULL upon failure).
  Author: Douglas Greve
---------------------------------------------------------------*/
DCM_OBJECT *GetObjectFromFile(char *fname, unsigned long options)
{
  CONDITION cond;
  DCM_OBJECT *object=0;
  int ok;
  
  //printf("     GetObjectFromFile(): %s %ld\n",fname,options);
  fflush(stdout);
  fflush(stderr);

  ok = IsDICOM(fname);
  if(! ok){
    fprintf(stderr,"ERROR: %s is not a dicom file\n",fname);
    COND_DumpConditions();
    return(NULL);
  }
  options = options | DCM_ACCEPTVRMISMATCH;

  cond=DCM_OpenFile(fname, DCM_PART10FILE|options, &object);
  if (cond != DCM_NORMAL)
  {
    DCM_CloseObject(&object);
    cond=DCM_OpenFile(fname, DCM_ORDERLITTLEENDIAN|options, &object);
  }
  if (cond != DCM_NORMAL)
  {
    DCM_CloseObject(&object);
    cond=DCM_OpenFile(fname, DCM_ORDERBIGENDIAN|options, &object);
  }
  if (cond != DCM_NORMAL)
  {
    DCM_CloseObject(&object);
    cond=DCM_OpenFile(fname, DCM_FORMATCONVERSION|options, &object);
  }
  if(cond != DCM_NORMAL){
    DCM_CloseObject(&object);
    COND_DumpConditions();
    return(NULL);
  }

  return(object);
}
/*-------------------------------------------------------------------
  AllocElementData() - allocates memory for the data portion of
    the element structure. Returns 1 if there's an error (otherwise 0). 
    Use FreeElementData() to free the data portion.
  Author: Douglas N. Greve, 9/6/2001
-------------------------------------------------------------------*/
int AllocElementData(DCM_ELEMENT *e)
{
  switch(e->representation){

  case DCM_AE: 
  case DCM_AS: 
  case DCM_CS: 
  case DCM_DA: 
  case DCM_DS: 
  case DCM_DT: 
  case DCM_IS: 
  case DCM_LO: 
  case DCM_LT: 
  case DCM_OB: 
  case DCM_OW: 
  case DCM_PN: 
  case DCM_SH: 
  case DCM_ST: 
  case DCM_TM: 
  case DCM_UI: 
    e->d.string = (char *) calloc(e->length+1,sizeof(char));
    e->d.string[e->length] = '\0'; /* add null terminator */
    break;
  case DCM_SS: 
    e->d.ss = (short *) calloc(1,sizeof(short));
    break;
  case DCM_SL:
    e->d.sl = (int *) calloc(1, sizeof(int));
    break;
  case DCM_SQ: 
    fprintf(stderr,"Element is of type dcm_sq, not supported\n");
    return(1);
    break;
  case DCM_UL: 
    e->d.ul = (unsigned int *) calloc(1,sizeof(unsigned int));
    break;
  case DCM_US:
    e->d.us = (unsigned short *) calloc(1, sizeof(unsigned short)); // e->length is the byte count
    break;
  case DCM_AT: 
    e->d.at = (DCM_TAG *) calloc(1, sizeof(DCM_TAG));
    break;
  case DCM_FD: 
    fprintf(stderr,"Element is of type double, not supported by CTN\n");
    return(1);
    break;
  case DCM_FL: 
    fprintf(stderr,"Element is of type float, not supported by CTN\n");
    return(1);
    break;
  default: 
    fprintf(stderr,"AllocElementData: %d unrecognized",e->representation);
    return(1);
  }

  return(0);
}
/*-------------------------------------------------------------------
  FreeElementData() - frees memory allocated for the data portion of
    the element structure. Returns 1 if there's an error (otherwise 0). 
    See also AllocElementData().
  Author: Douglas N. Greve, 9/6/2001
-------------------------------------------------------------------*/
int FreeElementData(DCM_ELEMENT *e)
{
  switch(e->representation){

  case DCM_AE: 
  case DCM_AS: 
  case DCM_CS: 
  case DCM_DA: 
  case DCM_DS: 
  case DCM_DT: 
  case DCM_IS: 
  case DCM_LO: 
  case DCM_LT: 
  case DCM_OB: 
  case DCM_OW: 
  case DCM_PN: 
  case DCM_SH: 
  case DCM_ST: 
  case DCM_TM: 
  case DCM_UI: 
    free(e->d.string);
    e->d.string = NULL;
    break;
  case DCM_SS: 
    // free(&e->d.ss);
    free(e->d.ss);
    e->d.ss = NULL;
    break;
  case DCM_SL:
    // free(&e->d.sl);
    free(e->d.sl);
    e->d.sl = NULL;
    break;
  case DCM_SQ: 
    //free(&e->d.sq);
    free(e->d.sq);
    e->d.sq = NULL;
    break;
  case DCM_UL: 
    // free(&e->d.ul);
    free(e->d.ul);
    e->d.ul = NULL;
    break;
  case DCM_US:
    // free(&e->d.us);
    free(e->d.us);
    e->d.us = NULL;
    break;
  case DCM_AT: 
    // free(&e->d.at);
    free(e->d.at);
    e->d.at = NULL;
    break;
  case DCM_FD: 
    fprintf(stderr,"Element is of type double, not supported by CTN\n");
    return(1);
    break;
  case DCM_FL: 
    fprintf(stderr,"Element is of type float, not supported by CTN\n");
    return(1);
    break;
  default: 
    fprintf(stderr,"FreeElementData: %d unrecognized",e->representation);
    return(1);
  }

  return(0);
}
/*-------------------------------------------------------------------
  IsSiemensDICOM() - returns 1 if the file is a Siemens DICOM File.
  Returns 0 is its not a dicom file or not a Siemsn DICOM File. 
  Checks DICOM tag (8,70). 
  Author: Douglas N. Greve, 9/6/2001
-------------------------------------------------------------------*/
int IsSiemensDICOM(char *dcmfile)
{
  DCM_ELEMENT *e;

  //printf("Entering IsSiemensDICOM (%s)\n",dcmfile);
  fflush(stdout);fflush(stderr);

  if(! IsDICOM(dcmfile) ){
    fflush(stdout);fflush(stderr);
    //printf("Leaving IsSiemensDICOM (%s)\n",dcmfile);
    return(0);
  }
  fflush(stdout);fflush(stderr);
  
  e = GetElementFromFile(dcmfile, 0x8, 0x70);
  if(e == NULL){
    printf("WARNING: searching dicom file %s for Manufacturer tag 0x8, 0x70\n",dcmfile);
    printf("WARNING: the result could be a mess.\n");
    return(0);
  }
  fflush(stdout);fflush(stderr);

  /* Siemens appears to add a space onto the end of their
     Manufacturer string*/
  if(strcmp(e->d.string,"SIEMENS")  != 0 && 
     strcmp(e->d.string,"SIEMENS ") != 0){ 
    FreeElementData(e); 
    free(e);
    return(0);
  }
  FreeElementData(e); free(e);

  //printf("Leaving IsSiemensDICOM (%s)\n",dcmfile);
  return(1);
}



/* The original SiemensQsciiTag() is too slow         */
/* make sure that returned value be freed if non-null */
char *SiemensAsciiTagEx(char *dcmfile, char *TagString, int cleanup)
{
  static char filename[1024] = "";
  static char **lists = 0;
  static int count = 0;
  static int startOfAscii = 0;
  static int MAX_ASCIILIST = 512;
  static int INCREMENT = 64;

  int i;
  FILE *fp;
  char buf[1024];
  char command[1024+32];
  char *plist = 0;
  char VariableName[512];
  char *VariableValue = 0;
  char tmpstr2[512];
  int newSize;
  char **newlists=0;

  // cleanup section.  Make sure to set cleanup =1 at the final call
  // don't rely on TagString but the last flag only
  if (cleanup == 1)
  {
    for (i = 0; i < count; ++i)
    { 
      if (lists[i])
      {
	free(lists[i]); lists[i] = 0;
      }
    }
    free(lists); lists = 0;
    count = 0;
    startOfAscii = 0;
    strcpy(filename, "");
    return ((char *) 0);
  }

  // if the filename changed, then cache the ascii strings
  if (strcmp(dcmfile, filename) !=0 )
  {
    if (lists == 0)
      lists = (char **) calloc(sizeof(char*), MAX_ASCIILIST); // initialized to be zero

    strcpy(filename, dcmfile);
    // free allocated list of strings
    for (i=0; i < count; ++i)
    {
      if (lists[i])
      {
	free(lists[i]); lists[i] =0;
      }
    }
    // now build up string lists ///////////////////////////////////
    startOfAscii = 0;
    strcpy(command, "strings ");
    strcat(command, filename);
    if ((fp = popen(command, "r")) == NULL)
    {
      fprintf(stderr, "could not open pipe for %s\n", filename);
      return 0;
    }
    count = 0;
    while (fgets(buf, 1024, fp))
    {
      // replace CR with null
      char *p = strrchr(buf, '\n');
      if (p)
	*p = '\0';
      
      // check the region
      if (strncmp(buf, "### ASCCONV BEGIN ###", 21)==0)
	startOfAscii = 1;
      else if (strncmp(buf, "### ASCCONV END ###", 19)==0)
	startOfAscii = 0;
      
      if (startOfAscii == 1)
      {
	// printf("%d:%d, %s\n", count, strlen(buf), buf);
	plist = (char *) malloc(strlen(buf)+1);
	strcpy(plist, buf);
	lists[count] = plist;
	++count;
	// if we exhaused the list pointer
	// we have to realloc
	if (count == MAX_ASCIILIST)
	{
	  newSize = MAX_ASCIILIST + INCREMENT;
	  newlists = (char **) realloc(lists, newSize*sizeof(char *));
	  if (newlists != 0)  // if not failed
	  {
	    lists = newlists; // update the pointer
	    // initialize uninitialized list
	    for (i=0; i < INCREMENT; ++i)
	      lists[MAX_ASCIILIST+i] = (char *) 0;
	    MAX_ASCIILIST = newSize;
	  }
	  else
	  {
	    // this should not happen, but hey just in case.
	    // Ascii tag is not essential and thus allow it to pass.
	    fprintf(stderr, "cannot store any more ASCII tags. Tagname is %s\n", TagString);
	  }
	}
      }
    }
    pclose(fp);
  }
  // build up string lists available
  // search the tag
  for (i=0; i < count; ++i)
  {
    // get the variable name (the first string)
    sscanf(lists[i],"%s %*s %*s",VariableName);
    if( strcmp(VariableName,TagString)==0 ) 
    { 
      /* match found. get the value (the third string) */
      sscanf(lists[i], "%*s %*s %s",tmpstr2);
      VariableValue = (char *) calloc(strlen(tmpstr2)+1,sizeof(char));
      memcpy(VariableValue, tmpstr2, strlen(tmpstr2));
    }
    else
      continue;
  }
  // show any errors present
  fflush(stdout);fflush(stderr);

  return VariableValue;
}

/*-----------------------------------------------------------------
  SiemensAsciiTag() - siemens dicom files have some data stored as a
  block of ASCII text. Each line in the block has the form:
    VariableName = VariableValue
  The begining of the block is delineated by the line
      ### ASCCONV BEGIN ###
  The end of the block is delineated by the line
      ### ASCCONV END ###
  This function searches this block for a variable named TagString
  and returns the Value as a string.

  It returns NULL if 
    1. It's not a Siemens DICOM File
    2. The begining of the ASCII block cannot be found
    3. There is no match with the TagString

  Author: Douglas N. Greve, 9/6/2001
  -----------------------------------------------------------------*/
char *SiemensAsciiTag(char *dcmfile, char *TagString)
{
  char linestr[1000]; 
  char tmpstr2[500];
  FILE *fp;
  int dumpline, nthchar;
  char *rt;
  char *BeginStr;
  int LenBeginStr;
  char *TestStr;
  int nTest;
  char VariableName[500];
  char *VariableValue;
  
  //printf("Entering SiemensAsciiTag() \n");fflush(stdout);fflush(stderr);
  //printf("dcmfile = %s, tagstr = %s\n",dcmfile,TagString);
  fflush(stdout);fflush(stderr);

  BeginStr = "### ASCCONV BEGIN ###";
  LenBeginStr = strlen(BeginStr);
  TestStr = (char *) calloc(LenBeginStr+1,sizeof(char));

  if(!IsSiemensDICOM(dcmfile)) return(NULL);

  fp = fopen(dcmfile,"r");
  if(fp == NULL){
    printf("ERROR: could not open dicom file %s\n",dcmfile);
    exit(1);
  }

  /* This section steps through the file char-by-char until
     the BeginStr is matched */
  dumpline = 0;
  nthchar = 0;
  while(1){
    fseek(fp,nthchar, SEEK_SET);
    nTest = fread(TestStr,sizeof(char),LenBeginStr,fp);
    if(nTest != LenBeginStr) break;
    if(strcmp(TestStr,BeginStr)==0){
      fseek(fp,nthchar, SEEK_SET);
      dumpline = 1;
      break;
    }
    nthchar ++;
  }
  free(TestStr);

  
  if(! dumpline) 
  {
    fclose(fp);    // must close
    fflush(stdout); fflush(stderr);
    return(NULL); /* Could not match Begin String */
  }

  /* Once the Begin String has been matched, this section 
     searches each line until the TagString is matched
     or until the End String is matched */
  VariableValue = NULL;
  while(1){
    rt = fgets(linestr,1000,fp);
    if(rt == NULL) break;

    if(strncmp(linestr,"### ASCCONV END ###",19)==0) break;

    sscanf(linestr,"%s %*s %*s",VariableName);

    if(strlen(VariableName) != strlen(TagString)) continue;

    if( strcmp(VariableName,TagString)==0 ) { 
      /* match found */
      sscanf(linestr,"%*s %*s %s",tmpstr2);
      VariableValue = (char *) calloc(strlen(tmpstr2)+1,sizeof(char));
      memcpy(VariableValue, tmpstr2, strlen(tmpstr2));
      break;
    }
  }
  fclose(fp);

  //printf("Leaving SiemensAsciiTag() \n");fflush(stdout);fflush(stderr);
  fflush(stdout);fflush(stderr);

  return(VariableValue);
}
/*-----------------------------------------------------------------------
  dcmGetVolRes - Gets the volume resolution (mm) from a DICOM File. The
  column and row resolution is obtained from tag (28,30). This tag is stored 
  as a string of the form "ColRes\RowRes". The slice resolution is obtained
  from tag (18,50). The slice thickness may not be correct for mosaics.
  See sdcmMosaicSliceRes().
  Author: Douglas N. Greve, 9/6/2001
  -----------------------------------------------------------------------*/
int dcmGetVolRes(char *dcmfile, float *ColRes, float *RowRes, float *SliceRes)
{
  DCM_ELEMENT *e;
  char *s;
  int ns,n;
  int slash_not_found;
  int tag_not_found = 0;

  /* Load the Pixel Spacing - this is a string of the form:
     ColRes\RowRes   */
  e = GetElementFromFile(dcmfile, 0x28, 0x30);
  if(e == NULL) return(1);

  /* Put it in a temporary sting */
  s = e->d.string;

  /* Go through each character looking for the backslash */
  slash_not_found = 1;
  ns = strlen(s);
  for(n=0;n<ns;n++) {
    if(s[n] == '\\'){
      s[n] = ' ';
      slash_not_found = 0;
      break;
    }
  }
  if(slash_not_found) return(1);

  sscanf(s,"%f %f",ColRes,RowRes);

  FreeElementData(e); free(e);


  /*E* // Load the Spacing Between Slices
    e = GetElementFromFile(dcmfile, 0x18, 0x88);
  if(e == NULL){
    // If 18,88 does not exist, load the slice thickness
    e = GetElementFromFile(dcmfile, 0x18, 0x50);
    if(e == NULL) return(1);
  }
  sscanf(e->d.string,"%f",SliceRes);
  FreeElementData(e); free(e);
  */

  e = GetElementFromFile(dcmfile, 0x18, 0x88);
  if (e == NULL) //tag not found
    tag_not_found =1;
  else
  {
    sscanf(e->d.string,"%f",SliceRes);
    if (*SliceRes == 0) //tag found but was zero
      tag_not_found = 1;
    // FreeElementData(e);  // freed here
    // free(e);
  }
  if (e)
  {
    FreeElementData(e); free(e);
  }
  if (tag_not_found) //so either no tag or tag was zero
  {
    e = GetElementFromFile(dcmfile, 0x18, 0x50);
    if(e == NULL) return(1); //no tag
    sscanf(e->d.string,"%f",SliceRes);
    FreeElementData(e); free(e);
    if (*SliceRes == 0) return(1); //tag exists but zero
  }
  // fprintf(stderr, "SliceRes=%f\n",*SliceRes);
  // FreeElementData(e); free(e);

  return(0);
}
/*-----------------------------------------------------------------------
  dcmGetSeriesNo - Gets the series number from tag (20,11). 
  Returns -1 if error.
  Author: Douglas N. Greve, 9/25/2001
  -----------------------------------------------------------------------*/
int dcmGetSeriesNo(char *dcmfile)
{
  DCM_ELEMENT *e;
  int SeriesNo;

  e = GetElementFromFile(dcmfile, 0x20, 0x11);
  if(e == NULL) return(-1);

  sscanf(e->d.string,"%d",&SeriesNo);

  FreeElementData(e); 
  free(e);

  return(SeriesNo);
}
/*-----------------------------------------------------------------------
  dcmGetNRows - Gets the number of rows in the image from tag (28,10). 
    Note that this is the number of rows in the image regardless of 
    whether its a mosaic.
  Returns -1 if error.
  Author: Douglas N. Greve, 9/6/2001
  -----------------------------------------------------------------------*/
int dcmGetNRows(char *dcmfile)
{
  DCM_ELEMENT *e;
  int NRows;

  e = GetElementFromFile(dcmfile, 0x28, 0x10);
  if(e == NULL)  return(-1);

  NRows = *(e->d.us);

  if (e->representation != DCM_US)
    printf("bad element for %s\n", dcmfile);

  FreeElementData(e); 
  free(e);

  return(NRows);
}
/*-----------------------------------------------------------------------
  dcmGetNCols - Gets the number of columns in the image from tag (28,11). 
    Note that this is the number of columns in the image regardless of 
    whether its a mosaic.
  Returns -1 if error.
  Author: Douglas N. Greve, 9/6/2001
  -----------------------------------------------------------------------*/
int dcmGetNCols(char *dcmfile)
{
  DCM_ELEMENT *e;
  int NCols;

  e = GetElementFromFile(dcmfile, 0x28, 0x11);
  if(e == NULL) return(-1);

  NCols = *(e->d.us);

  FreeElementData(e); 
  free(e);

  return(NCols);
}
/*-----------------------------------------------------------------------
  dcmImageDirCos - Gets the RAS direction cosines for the col and row of 
    the image based on DICOM tag (20,37). Vcx is the x-component of the
    unit vector that points from the center of one voxel to the center of
    an adjacent voxel in the next higher column within the same row and
    slice.
  Returns 1 if error.
  Author: Douglas N. Greve, 9/10/2001
  -----------------------------------------------------------------------*/
int dcmImageDirCos(char *dcmfile, 
       float *Vcx, float *Vcy, float *Vcz,
       float *Vrx, float *Vry, float *Vrz)
{
  DCM_ELEMENT *e;
  char *s;
  int n, nbs;
  float rms;

  /* Load the direction cosines - this is a string of the form:
     Vcx\Vcy\Vcz\Vrx\Vry\Vrz */
  e = GetElementFromFile(dcmfile, 0x20, 0x37);
  if(e == NULL) return(1);
  s = e->d.string;

  /* replace back slashes with spaces */
  nbs = 0;
  for(n = 0; n < strlen(s); n++){
    if(s[n] == '\\'){
      s[n] = ' ';
      nbs ++;
    }
  }

  if(nbs != 5) return(1);

  sscanf(s,"%f %f %f %f %f %f ",Vcx,Vcy,Vcz,Vrx,Vry,Vrz);

  /* Convert Vc from LPS to RAS and Normalize */
  rms = sqrt((*Vcx)*(*Vcx) + (*Vcy)*(*Vcy) + (*Vcz)*(*Vcz)) ;
  (*Vcx) /= -rms;
  (*Vcy) /= -rms;
  (*Vcz) /= +rms;

  /* Convert Vr from LPS to RAS and Normalize */
  rms = sqrt((*Vrx)*(*Vrx) + (*Vry)*(*Vry) + (*Vrz)*(*Vrz)) ;
  (*Vrx) /= -rms;
  (*Vry) /= -rms;
  (*Vrz) /= +rms;

  FreeElementData(e); 
  free(e);

  e = GetElementFromFile(dcmfile, 0x20, 0x20);
  if (e==NULL) return (1);
  s = e->d.string;
  
  FreeElementData(e);
  free(e);

  return(0);
}
/*-----------------------------------------------------------------------
  dcmImagePosition - Gets the RAS position of the center of the CRS=0
    voxel based on DICOM tag (20,32). Note that only the z component 
    is valid for mosaics. See also sdfiFixImagePosition().
  Returns 1 if error.
  Author: Douglas N. Greve, 9/10/2001
  -----------------------------------------------------------------------*/
int dcmImagePosition(char *dcmfile, float *x, float *y, float *z)
{
  DCM_ELEMENT *e;
  char *s;
  int n, nbs;

  /* Load the Image Position: this is a string of the form:
     x\y\z  */
  e = GetElementFromFile(dcmfile, 0x20, 0x32);
  if(e == NULL)  return(1);
  s = e->d.string;

  /* replace back slashes with spaces */
  nbs = 0;
  for(n = 0; n < strlen(s); n++){
    if(s[n] == '\\'){
      s[n] = ' ';
      nbs ++;
    }
  }

  if(nbs != 2) return(1);

  sscanf(s,"%f %f %f ",x,y,z);

  /* Convert from LPS to RAS */
  (*x) *= -1.0;
  (*y) *= -1.0;

  FreeElementData(e); 
  free(e);

  return(0);
}
/*-----------------------------------------------------------------------
  sdcmSliceDirCos - Gets the RAS direction cosines for the slice base on
    the Siemens ASCII header. In the ASCII header, there are components
    of the form sSliceArray.asSlice[0].sNormal.dXXX, where XXX is Sag, Cor, 
    and/or Tra. These form the vector that is perpendicular to the 
    slice plane in the direction of increasing slice number. If absent, 
    the component is set to zero. 

  Notes:
    1. Converts from Siemens/DICOM LIS to RAS.
    2. Normalizes the vector.

  Returns 1 if error.
  Author: Douglas N. Greve, 9/10/2001
  -----------------------------------------------------------------------*/
int sdcmSliceDirCos(char *dcmfile, float *Vsx, float *Vsy, float *Vsz)
{
  char *tmpstr;
  float rms;

  if(! IsSiemensDICOM(dcmfile) ) return(1);

  tmpstr = SiemensAsciiTagEx(dcmfile, "sSliceArray.asSlice[0].sNormal.dSag", 0);
  if(tmpstr != NULL){
    sscanf(tmpstr,"%f",Vsx);
    free(tmpstr);
  }

  tmpstr = SiemensAsciiTagEx(dcmfile, "sSliceArray.asSlice[0].sNormal.dCor", 0);
  if(tmpstr != NULL){
    sscanf(tmpstr,"%f",Vsy);
    free(tmpstr);
  }

  tmpstr = SiemensAsciiTagEx(dcmfile, "sSliceArray.asSlice[0].sNormal.dTra", 0);
  if(tmpstr != NULL){
    sscanf(tmpstr,"%f",Vsz);
    free(tmpstr);
  }

  if( *Vsx == 0 && *Vsy == 0 && *Vsz == 0 ) 
  {
    sliceDirCosPresent = 0;
    return (1);
  }

  /* Convert from LPS to RAS */
  (*Vsx) *= -1.0;
  (*Vsy) *= -1.0;

  /* Normalize */
  rms = sqrt((*Vsx)*(*Vsx) + (*Vsy)*(*Vsy) + (*Vsz)*(*Vsz)) ;

  (*Vsx) /= rms;
  (*Vsy) /= rms;
  (*Vsz) /= rms;

  sliceDirCosPresent = 1;

  return(0);
}

/*-----------------------------------------------------------------------
  sdcmIsMosaic() - tests whether a siemens dicom file has a mosaic image.
    If it is a mosaic it returns 1 and puts the dimension of the volume
    into pNcols, pNrows, pNslices, pNframes. These pointer arguments can
    be NULL, in which case they are ignored.

  This function works by computing the expected number of rows and columns
  assuming that the image is not a mosaic.  This is done by dividing the
  Phase Encode FOV by the image resolution in the phase encode direction
  (and same with Readout FOV).

  Author: Douglas N. Greve, 9/6/2001
  -----------------------------------------------------------------------*/
int sdcmIsMosaic(char *dcmfile, int *pNcols, int *pNrows, int *pNslices, int *pNframes)
{
  DCM_ELEMENT *e;
  char PhEncDir[4];
  int Nrows, Ncols;
  float ColRes, RowRes, SliceRes;
  int NrowsExp, NcolsExp;
  float PhEncFOV, ReadOutFOV;
  int err, IsMosaic;
  char *tmpstr;

  if(! IsSiemensDICOM(dcmfile) ) return(0);

  IsMosaic = 0;

  /* Get the phase encode direction: should be COL or ROW */
  /* COL means that each row is a different phase encode */
  e = GetElementFromFile(dcmfile, 0x18, 0x1312);
  if(e == NULL) return(0);
  memcpy(PhEncDir,e->d.string,3);
  FreeElementData(e); free(e);

  Nrows = dcmGetNRows(dcmfile);
  if(Nrows == -1) return(0);

  Ncols = dcmGetNCols(dcmfile);
  if(Ncols == -1) return(0);

  tmpstr = SiemensAsciiTagEx(dcmfile, "sSliceArray.asSlice[0].dPhaseFOV", 0);
  if(tmpstr == NULL) return(0);
  sscanf(tmpstr,"%f",&PhEncFOV);
  free(tmpstr);

  tmpstr = SiemensAsciiTagEx(dcmfile, "sSliceArray.asSlice[0].dReadoutFOV", 0);
  if(tmpstr == NULL) return(0);
  sscanf(tmpstr,"%f",&ReadOutFOV);
  free(tmpstr);

  err = dcmGetVolRes(dcmfile, &ColRes, &RowRes, &SliceRes);
  if(err) return(-1);

  if(strncmp(PhEncDir,"COL",3)==0){
    /* Each row is a different phase encode */
    NrowsExp = (int)(rint(PhEncFOV/RowRes));
    NcolsExp = (int)(rint(ReadOutFOV/ColRes));
  }
  else{
    /* Each column is a different phase encode */
    NrowsExp = (int)(rint(ReadOutFOV/RowRes));
    NcolsExp = (int)(rint(PhEncFOV/ColRes));
  }

  if(NrowsExp != Nrows || NcolsExp != Ncols){
    IsMosaic = 1;
    if(pNrows != NULL) *pNrows = NrowsExp;
    if(pNcols != NULL) *pNcols = NcolsExp;
    if(pNslices != NULL){
      tmpstr = SiemensAsciiTagEx(dcmfile, "sSliceArray.lSize", 0);
      if(tmpstr == NULL) return(0);
      sscanf(tmpstr,"%d",pNslices);
      free(tmpstr);
    }
    if(pNframes != NULL){
      tmpstr = SiemensAsciiTagEx(dcmfile, "lRepetitions", 0);
      if(tmpstr == NULL) return(0);
      sscanf(tmpstr,"%d",pNframes);
      (*pNframes)++;
      free(tmpstr);
    }
  }

  return(IsMosaic);
}
/*----------------------------------------------------------------
  GetSDCMFileInfo() - this fills a SDCMFILEINFO structure for a 
  single Siemens DICOM file. Some of the data are filled from
  the DICOM header and some from the Siemens ASCII header. The
  pixel data are not loaded.
  ----------------------------------------------------------------*/
SDCMFILEINFO *GetSDCMFileInfo(char *dcmfile)
{
  DCM_OBJECT *object=0;
  SDCMFILEINFO *sdcmfi;
  CONDITION cond;
  DCM_TAG tag;
  int l;
  unsigned short ustmp;
  double dtmp;
  char *strtmp;
  int retval;
  double xr,xa,xs,yr,ya,ys, zr,za,zs;
  static int warn=0; // warn user once about slice dir not stored

  if(! IsSiemensDICOM(dcmfile) ) return(NULL);

  sdcmfi = (SDCMFILEINFO *) calloc(1,sizeof(SDCMFILEINFO));

  fflush(stdout);
  fflush(stderr);
  object = GetObjectFromFile(dcmfile, 0);
  fflush(stdout);
  fflush(stderr);
  if(object == NULL) exit(1);

  l = strlen(dcmfile);
  sdcmfi->FileName = (char *) calloc(l+1,sizeof(char));
  memcpy(sdcmfi->FileName,dcmfile,l);

  tag=DCM_MAKETAG(0x10, 0x10);
  cond=GetString(&object, tag, &sdcmfi->PatientName);

  tag=DCM_MAKETAG(0x8, 0x20);
  cond=GetString(&object, tag, &sdcmfi->StudyDate);

  tag=DCM_MAKETAG(0x8, 0x30);
  cond=GetString(&object, tag, &sdcmfi->StudyTime);

  tag=DCM_MAKETAG(0x8, 0x31);
  cond=GetString(&object, tag, &sdcmfi->SeriesTime);

  tag=DCM_MAKETAG(0x8, 0x32);
  cond=GetString(&object, tag, &sdcmfi->AcquisitionTime);

  tag=DCM_MAKETAG(0x8, 0x1090);
  cond=GetString(&object, tag, &sdcmfi->ScannerModel);

  tag=DCM_MAKETAG(0x18, 0x1020);
  cond=GetString(&object, tag, &sdcmfi->NumarisVer);

  tag=DCM_MAKETAG(0x18, 0x24);
  cond=GetString(&object, tag, &sdcmfi->PulseSequence);
  if(cond != DCM_NORMAL){ sdcmfi->ErrorFlag = 1; }

  tag=DCM_MAKETAG(0x18, 0x1030);
  cond=GetString(&object, tag, &sdcmfi->ProtocolName);
  if(cond != DCM_NORMAL){ sdcmfi->ErrorFlag = 1; }

  tag=DCM_MAKETAG(0x20, 0x11);
  cond=GetUSFromString(&object, tag, &ustmp);
  if(cond != DCM_NORMAL){ sdcmfi->ErrorFlag = 1; }
  sdcmfi->SeriesNo = (int) ustmp;
  sdcmfi->RunNo = sdcmfi->SeriesNo - 1;

  tag=DCM_MAKETAG(0x20, 0x13);
  cond=GetUSFromString(&object, tag, &ustmp);
  if(cond != DCM_NORMAL){ sdcmfi->ErrorFlag = 1; }
  sdcmfi->ImageNo = (int) ustmp;

  tag=DCM_MAKETAG(0x18, 0x86);
  cond=GetUSFromString(&object, tag, &ustmp);
  sdcmfi->EchoNo = (int) ustmp;

  tag=DCM_MAKETAG(0x18, 0x1020);
  cond=GetDoubleFromString(&object, tag, &dtmp);

  tag=DCM_MAKETAG(0x18, 0x1314);
  cond=GetDoubleFromString(&object, tag, &dtmp);
  sdcmfi->FlipAngle = (float) M_PI*dtmp/180.0;

  tag=DCM_MAKETAG(0x18, 0x81);
  cond=GetDoubleFromString(&object, tag, &dtmp);
  sdcmfi->EchoTime = (float) dtmp;

  tag=DCM_MAKETAG(0x18, 0x82);
  cond=GetDoubleFromString(&object, tag, &dtmp);
  sdcmfi->InversionTime = (float) dtmp;

  tag=DCM_MAKETAG(0x18, 0x80);
  cond=GetDoubleFromString(&object, tag, &dtmp);
  sdcmfi->RepetitionTime = (float) dtmp;

  tag=DCM_MAKETAG(0x18, 0x1312);
  cond=GetString(&object, tag, &sdcmfi->PhEncDir);
  if(cond != DCM_NORMAL){ sdcmfi->ErrorFlag = 1; }

  strtmp = SiemensAsciiTagEx(dcmfile, "lRepetitions", 0);
  if(strtmp != NULL){
    sscanf(strtmp,"%d",&(sdcmfi->lRepetitions));
    free(strtmp);
  }
  else sdcmfi->lRepetitions = 0;
  sdcmfi->NFrames = sdcmfi->lRepetitions + 1;
  /* This is not the last word on NFrames. See sdfiAssignRunNo().*/

  strtmp = SiemensAsciiTagEx(dcmfile, "sSliceArray.lSize", 0);
  if(strtmp != NULL){
    sscanf(strtmp,"%d",&(sdcmfi->SliceArraylSize));
    free(strtmp);
  }
  else sdcmfi->SliceArraylSize = 0;

  strtmp = SiemensAsciiTagEx(dcmfile, "sSliceArray.asSlice[0].dPhaseFOV", 0);
  if(strtmp != NULL){
    sscanf(strtmp,"%f",&(sdcmfi->PhEncFOV));
    free(strtmp);
  }
  else sdcmfi->PhEncFOV = 0;

  strtmp = SiemensAsciiTagEx(dcmfile, "sSliceArray.asSlice[0].dReadoutFOV", 0);
  if(strtmp != NULL){
    sscanf(strtmp,"%f",&(sdcmfi->ReadoutFOV));
    free(strtmp);
  }
  else sdcmfi->ReadoutFOV = 0;

  sdcmfi->NImageRows = dcmGetNRows(dcmfile);
  if(sdcmfi->NImageRows < 0){ sdcmfi->ErrorFlag = 1; }
  sdcmfi->NImageCols = dcmGetNCols(dcmfile);
  if(sdcmfi->NImageCols < 0){ sdcmfi->ErrorFlag = 1; }

  dcmImagePosition(dcmfile, &(sdcmfi->ImgPos[0]), &(sdcmfi->ImgPos[1]), 
       &(sdcmfi->ImgPos[2]) );

  dcmImageDirCos(dcmfile,&(sdcmfi->Vc[0]),&(sdcmfi->Vc[1]),&(sdcmfi->Vc[2]),
             &(sdcmfi->Vr[0]),&(sdcmfi->Vr[1]),&(sdcmfi->Vr[2]));
 
  /* the following may return 1 (Vs[i] = 0 for all i) */
  /* we have to fix  */
  retval = sdcmSliceDirCos(dcmfile,&(sdcmfi->Vs[0]),&(sdcmfi->Vs[1]),&(sdcmfi->Vs[2]));
  
  sdcmfi->IsMosaic = sdcmIsMosaic(dcmfile, NULL, NULL, NULL, NULL);

  // if could not get sliceDirCos, then we calculate
  if (retval == 1 && sdcmfi->IsMosaic == 0)
  {
    if (warn == 0)
    {
      fprintf(stderr, "INFO: slice direction constructed from image direction cosines.\n");
      warn = 1;
    }
    /* we have x_(r,a,s) and y_(r,a,s).  z_(r,a,s) must be orthogonal to these */
    /* get the cross product of x_(r,a,s) and y_(r,a,s) is in proportion to z_(r,a,s) */
    /* also x_(r,a,s) and y_(r,a,s) are normalized and thus cross product is also normalized */
    /* here right-handedness is assumed. must be compared with the image positions           */
    xr = sdcmfi->Vc[0]; xa = sdcmfi->Vc[1]; xs = sdcmfi->Vc[2]; 
    yr = sdcmfi->Vr[0]; ya = sdcmfi->Vr[1]; ys = sdcmfi->Vr[2];
    zr = xa*ys - xs*ya; za = xs*yr - xr*ys; zs = xr*ya - xa*yr;
    /* confirm sign by two files later  */
    sdcmfi->Vs[0] = zr; sdcmfi->Vs[1] = za; sdcmfi->Vs[2] = zs;
  }

  dcmGetVolRes(dcmfile,&(sdcmfi->VolRes[0]),&(sdcmfi->VolRes[1]),
         &(sdcmfi->VolRes[2]));

  if(sdcmfi->IsMosaic){
    sdcmIsMosaic(dcmfile, &(sdcmfi->VolDim[0]), &(sdcmfi->VolDim[1]),
     &(sdcmfi->VolDim[2]), &(sdcmfi->NFrames) );
  }
  else{
    sdcmfi->VolDim[0] = sdcmfi->NImageCols;
    sdcmfi->VolDim[1] = sdcmfi->NImageRows;
  }

  // cleanup Ascii storage
  SiemensAsciiTagEx(dcmfile, (char *) 0, 1);
  
  cond=DCM_CloseObject(&object);

  /* Clear the condition stack to prevent overflow */
  COND_PopCondition(1);

  return(sdcmfi);

}
/*----------------------------------------------------------*/
int DumpSDCMFileInfo(FILE *fp, SDCMFILEINFO *sdcmfi)
{
  fprintf(fp,"FileName \t\t%s\n",sdcmfi->FileName);
  fprintf(fp,"Identification\n");
  fprintf(fp,"\tNumarisVer        %s\n",sdcmfi->NumarisVer);
  fprintf(fp,"\tScannerModel      %s\n",sdcmfi->ScannerModel);
  fprintf(fp,"\tPatientName       %s\n",sdcmfi->PatientName);
  fprintf(fp, "Date and time\n");
  fprintf(fp,"\tStudyDate         %s\n",sdcmfi->StudyDate);
  fprintf(fp,"\tStudyTime         %s\n",sdcmfi->StudyTime);
  fprintf(fp,"\tSeriesTime        %s\n",sdcmfi->SeriesTime);
  fprintf(fp,"\tAcqTime           %s\n",sdcmfi->AcquisitionTime);
  fprintf(fp,"Acquisition parameters\n");
  fprintf(fp,"\tPulseSeq          %s\n",sdcmfi->PulseSequence);
  fprintf(fp,"\tProtocol          %s\n",sdcmfi->ProtocolName);
  fprintf(fp,"\tPhEncDir          %s\n",sdcmfi->PhEncDir);
  fprintf(fp,"\tEchoNo            %d\n",sdcmfi->EchoNo);
  fprintf(fp,"\tFlipAngle         %g\n",DEGREES(sdcmfi->FlipAngle));
  fprintf(fp,"\tEchoTime          %g\n",sdcmfi->EchoTime);
  fprintf(fp,"\tInversionTime     %g\n",sdcmfi->InversionTime);
  fprintf(fp,"\tRepetitionTime    %g\n",sdcmfi->RepetitionTime);
  fprintf(fp,"\tPhEncFOV          %g\n",sdcmfi->PhEncFOV);
  fprintf(fp,"\tReadoutFOV        %g\n",sdcmfi->ReadoutFOV);
  fprintf(fp,"Image information\n");
  fprintf(fp,"\tRunNo             %d\n",sdcmfi->RunNo);
  fprintf(fp,"\tSeriesNo          %d\n",sdcmfi->SeriesNo);
  fprintf(fp,"\tImageNo           %d\n",sdcmfi->ImageNo);
  fprintf(fp,"\tNImageRows        %d\n",sdcmfi->NImageRows);
  fprintf(fp,"\tNImageCols        %d\n",sdcmfi->NImageCols);
  fprintf(fp,"\tNFrames           %d\n",sdcmfi->NFrames);
  fprintf(fp,"\tSliceArraylSize   %d\n",sdcmfi->SliceArraylSize);
  fprintf(fp,"\tIsMosaic          %d\n",sdcmfi->IsMosaic);

  fprintf(fp,"\tImgPos            %8.4f %8.4f %8.4f \n",
    sdcmfi->ImgPos[0],sdcmfi->ImgPos[1],sdcmfi->ImgPos[2]);

  fprintf(fp,"\tVolRes            %8.4f %8.4f %8.4f \n",sdcmfi->VolRes[0],
    sdcmfi->VolRes[1],sdcmfi->VolRes[2]);
  fprintf(fp,"\tVolDim            %3d %3d %3d \n",sdcmfi->VolDim[0],
    sdcmfi->VolDim[1],sdcmfi->VolDim[2]);
  fprintf(fp,"\tVc                %8.4f %8.4f %8.4f \n",
    sdcmfi->Vc[0],sdcmfi->Vc[1],sdcmfi->Vc[2]);
  fprintf(fp,"\tVr                %8.4f %8.4f %8.4f \n",
    sdcmfi->Vr[0],sdcmfi->Vr[1],sdcmfi->Vr[2]);
  fprintf(fp,"\tVs                %8.4f %8.4f %8.4f \n",
    sdcmfi->Vs[0],sdcmfi->Vs[1],sdcmfi->Vs[2]);

  fprintf(fp,"\tVolCenter         %8.4f %8.4f %8.4f \n",
    sdcmfi->VolCenter[0],sdcmfi->VolCenter[1],sdcmfi->VolCenter[2]);

  return(0);
}

/*-----------------------------------------------------------------*/
int FreeSDCMFileInfo(SDCMFILEINFO **ppsdcmfi)
{
  SDCMFILEINFO *p;

  p = *ppsdcmfi;

  if(p->FileName != NULL)        free(p->FileName);
  if(p->PatientName != NULL)     free(p->PatientName);
  if(p->StudyDate != NULL)       free(p->StudyDate);
  if(p->StudyTime != NULL)       free(p->StudyTime);
  if(p->SeriesTime != NULL)      free(p->SeriesTime);
  if(p->AcquisitionTime != NULL) free(p->AcquisitionTime);
  if(p->PulseSequence != NULL)   free(p->PulseSequence);
  if(p->ProtocolName != NULL)    free(p->ProtocolName);
  if(p->PhEncDir != NULL)        free(p->PhEncDir);

  free(*ppsdcmfi);
  return(0);
}
/*-----------------------------------------------------------------
  sdcmExtractNumarisVer() - extacts the NUMARIS version string. The
  string can be something like "syngo MR 2002B 4VA21A", in which case
  only the last string is the actual version string. Or it can look
  like "4VA12B". In either case, the last string is the version
  string. The input is the string obtained from DICOM element
  (18,1020). The Major number is the first number in the string. The
  Minor number is the number after VA.  The MinorMinor number is the
  number the Minor.
  -------------------------------------------------------------------*/
char *sdcmExtractNumarisVer(char *e_18_1020, int *Maj, int *Min, int *MinMin)
{
  int l,n,m;
  char *ver;

  l = strlen(e_18_1020);
  if(l < 6){
    printf("ERROR: incorreclty formatted version string %s\n"
	   "found in dicom tag 18,1020 (string len < 6)\n",e_18_1020);
    return(NULL); 
  }

  /* dont copy blanks at the end */
  n=l-1;
  while(n >= 0 && e_18_1020[n] == ' ') n--;
  if(n < 0){
    printf("ERROR: incorreclty formatted version string %s\n"
	   "found in dicom tag 18,1020 (all blanks)\n",e_18_1020);
    return(NULL);
  }

  /* count length of the first non-blank string */
  m=0;
  while(n >= 0 && e_18_1020[n] != ' '){
    m++;
    n--;
  }
  n++;

  if(m != 6){
    printf("ERROR: incorreclty formatted version string %s\n"
	   "found in dicom tag 18,1020 (len = %d != 6)\n",e_18_1020,m);
    return(NULL);
  }

  ver = (char *) calloc(sizeof(char),m+1);
  strncpy(ver,&e_18_1020[n],m);

  /* Now determine major and minor release numbers */
  if(ver[1] != 'V' || !(ver[2] == 'A' || ver[2] == 'B')){
    printf("ERROR: incorreclty formatted version string %s\n"
	   "found in dicom tag 18,1020 (VA or VB not in 2nd and 3rd)\n",
	   e_18_1020);
    return(NULL);
  }

  *Maj    = ver[0] - 48;
  *Min    = ver[3] - 48;
  *MinMin = ver[4] - 48;
  //printf("Maj = %d, Min = %d, MinMin = %d\n",*Maj,*Min,*MinMin);

  return(ver);
}
/*--------------------------------------------------------------------
  ScanSiemensDCMDir() - similar to ScanDir but returns only files that
  are Siemens DICOM Files. It also returns a pointer to an array of
  SDCMFILEINFO structures.

  Author: Douglas Greve.
  Date: 09/10/2001
 *------------------------------------------------------------------*/
SDCMFILEINFO **ScanSiemensDCMDir(char *PathName, int *NSDCMFiles)
{
  struct dirent **NameList;
  int i, pathlength;
  int NFiles;
  char tmpstr[1000];
  SDCMFILEINFO **sdcmfi_list;
  int pct, sumpct;
  FILE *fp;

  /* Remove all trailing forward slashes from PathName */
  pathlength=strlen(PathName);
  if(PathName[pathlength-1] == '/'){
    for(i = pathlength-1; i >= 0; i--){
      if(PathName[i] == '/'){
	PathName[i] = ' ';
	break;
      }
    }
  }
  pathlength=strlen(PathName);
  
  /* select all directory entries, and sort them by name */
  NFiles = scandir(PathName, &NameList, 0, alphasort);
  
  if( NFiles < 0 ){
    fprintf(stderr,"WARNING: No files found in %s\n",PathName);
    return(NULL);
  }
  fprintf(stderr,"INFO: Found %d files in %s\n",NFiles,PathName);
  
  /* Count the number of Siemens DICOM Files */
  fprintf(stderr,"INFO: counting Siemens Files\n");
  (*NSDCMFiles) = 0;
  for(i = 0; i < NFiles; i++){
    sprintf(tmpstr,"%s/%s", PathName, NameList[i]->d_name);
    if(IsSiemensDICOM(tmpstr)) (*NSDCMFiles)++;
  }

  fprintf(stderr,"INFO: found %d Siemens Files\n",*NSDCMFiles);

  if(*NSDCMFiles == 0) return(NULL);

  sdcmfi_list = (SDCMFILEINFO **)calloc(*NSDCMFiles, sizeof(SDCMFILEINFO *));

  fprintf(stderr,"INFO: scanning info from Siemens Files\n");

  if(SDCMStatusFile != NULL)
    fprintf(stderr,"INFO: status file is %s\n",SDCMStatusFile);

  fprintf(stderr,"%2d ",0);
  sumpct = 0;
  (*NSDCMFiles) = 0;
  for(i = 0; i < NFiles; i++){

    //fprintf(stderr,"%4d ",i);
    pct = rint(100*(i+1)/NFiles) - sumpct;
    if(pct >= 2) {
      sumpct += pct;
      fprintf(stderr,"%3d ",sumpct);
      fflush(stderr);
      if(SDCMStatusFile != NULL){
	fp = fopen(SDCMStatusFile,"w");
	if(fp != NULL){
	  fprintf(fp,"%3d\n",sumpct);
	  fclose(fp);
	}
      }
    }
    
    sprintf(tmpstr,"%s/%s", PathName, NameList[i]->d_name);
    if(IsSiemensDICOM(tmpstr)) {
      sdcmfi_list[*NSDCMFiles] = GetSDCMFileInfo(tmpstr);
      if(sdcmfi_list[*NSDCMFiles] == NULL) return(NULL);
      (*NSDCMFiles) ++;
    }
  }
  fprintf(stderr,"\n");

  // free memory
  while(NFiles--) 
  {
    free(NameList[NFiles]);
  }
  free(NameList);

  return( sdcmfi_list );
}
/*--------------------------------------------------------------------
  LoadSiemensSeriesInfo() - loads header info from each of the nList 
  files listed in SeriesList. This list is obtained from either
  ReadSiemensSeries() or ScanSiemensSeries().
  Author: Douglas Greve.
  Date: 09/10/2001
 *------------------------------------------------------------------*/
SDCMFILEINFO **LoadSiemensSeriesInfo(char **SeriesList, int nList)
{
  SDCMFILEINFO **sdfi_list;
  int n;

  //printf("LoadSiemensSeriesInfo()\n");

  sdfi_list = (SDCMFILEINFO **)calloc(nList, sizeof(SDCMFILEINFO *));

  for(n = 0; n < nList; n++){
    fflush(stdout);fflush(stderr);

    //printf("%3d %s ---------------\n",n,SeriesList[n]);
    if(! IsSiemensDICOM(SeriesList[n]) ) {
      fprintf(stderr,"ERROR: %s is not a Siemens DICOM File\n",
        SeriesList[n]);
      fflush(stderr);
      free(sdfi_list);
      return(NULL);
    }

    //printf("Getting file info %s ---------------\n",SeriesList[n]);
    fflush(stdout);
    fflush(stderr);
    sdfi_list[n] = GetSDCMFileInfo(SeriesList[n]);
    if(sdfi_list[n] == NULL) {
      fprintf(stderr,"ERROR: reading %s \n",SeriesList[n]);
      fflush(stderr);
      free(sdfi_list);
      return(NULL);
    }
  }
  fprintf(stderr,"\n");
  fflush(stdout); fflush(stderr);

  return( sdfi_list );
}
/*--------------------------------------------------------------------
  ReadSiemensSeries() - returns a list of file names as found in the
  given ListFile. This file should contain a list of file names
  separated by white space. Any directory names are stripped off and
  replaced with the dirname of dcmfile. If dcmfile is not part of the
  list, it is added to the list.  No attempt is made to assure that
  the files are Siemens DICOM files or that they even exist. The
  resulting list is of the same form produced by ScanSiemensSeries();

  Author: Douglas Greve.
  Date: 09/25/2001
 *------------------------------------------------------------------*/
char **ReadSiemensSeries(char *ListFile, int *nList, char *dcmfile)
{
  FILE *fp;
  char **SeriesList;
  char tmpstr[1000];
  int n;
  char *dcmbase, *dcmdir, *fbase;
  int AddDCMFile;

  if(!IsSiemensDICOM(dcmfile)){
    fprintf(stderr,"ERROR: %s is not a siemens DICOM file\n",dcmfile);
    return(NULL);
  }

  dcmdir  = fio_dirname(dcmfile);
  dcmbase = fio_basename(dcmfile,NULL);

  fp = fopen(ListFile,"r");
  if(fp == NULL){
    fprintf(stderr,"ERROR: could not open %s for reading\n",ListFile);
    return(NULL);
  }

  /* Count the numer of files in the list. Look for dcmfile and
     turn off the flag if it is found.*/
  AddDCMFile = 1;
  (*nList) = 0;
  while( fscanf(fp,"%s",tmpstr) != EOF ){
    fbase = fio_basename(tmpstr,NULL);
    if(strcmp(dcmbase,fbase)==0) AddDCMFile = 0;
    (*nList) ++;
    free(fbase);
  }

  if( (*nList) == 0 ){
    fprintf(stderr,"ERROR: no files found in %s\n",ListFile);
    fflush(stderr);
    return(NULL);
  }
  fseek(fp,0,SEEK_SET); /* go back to the begining */

  if(AddDCMFile){
    fprintf(stderr,"INFO: adding dcmfile to list.\n");
    fflush(stderr);
    (*nList) ++;
  }

  fprintf(stderr,"INFO: found %d files in list file\n",(*nList));
  fflush(stderr);

  SeriesList = (char **) calloc((*nList), sizeof(char *));

  for(n=0; n < ((*nList)-AddDCMFile) ; n++){
    fscanf(fp,"%s",tmpstr);
    fbase = fio_basename(tmpstr,NULL);
    sprintf(tmpstr,"%s/%s",dcmdir,fbase);
    SeriesList[n] = (char *) calloc(strlen(tmpstr)+1,sizeof(char));
    memcpy(SeriesList[n],tmpstr,strlen(tmpstr));
    free(fbase);
  }
  if(AddDCMFile){
    SeriesList[n] = (char *) calloc(strlen(dcmfile)+1,sizeof(char));
    memcpy(SeriesList[n],dcmfile,strlen(dcmfile));
  }

  fclose(fp);
  free(dcmdir);
  free(dcmbase);
  return(SeriesList);
}
/*--------------------------------------------------------------------
  ScanSiemensSeries() - scans a directory for Siemens DICOM files with
  the same Series Number as the given Siemens DICOM file. Returns a
  list of file names (including dcmfile), including the path. The
  resulting list is of the same form produced by ReadSiemensSeries();

  Author: Douglas Greve.
  Date: 09/25/2001
 *------------------------------------------------------------------*/
char **ScanSiemensSeries(char *dcmfile, int *nList)
{
  int SeriesNo, SeriesNoTest;
  char *PathName;
  int NFiles,i;
  struct dirent **NameList;
  char **SeriesList;
  char tmpstr[1000];

  if(!IsSiemensDICOM(dcmfile)){
    fprintf(stderr,"ERROR: %s is not a siemens DICOM file\n",dcmfile);
    fflush(stderr);
    return(NULL);
  }

  printf("Getting Series No \n");
  SeriesNo = dcmGetSeriesNo(dcmfile);
  if(SeriesNo == -1){
    fprintf(stderr,"ERROR: reading series number from %s\n",dcmfile);
    fflush(stderr);
    return(NULL);
  }

  printf("Scanning Directory \n");
  /* select all directory entries, and sort them by name */
  PathName = fio_dirname(dcmfile);  
  NFiles = scandir(PathName, &NameList, 0, alphasort);

  if( NFiles < 0 ){
    fprintf(stderr,"WARNING: No files found in %s\n",PathName);
    fflush(stderr);
    return(NULL);
  }
  fprintf(stderr,"INFO: Found %d files in %s\n",NFiles,PathName);
  fprintf(stderr,"INFO: Scanning for Series Number %d\n",SeriesNo);
  fflush(stderr);

  /* Alloc enough memory for everyone */
  SeriesList = (char **) calloc(NFiles, sizeof(char *));
  (*nList) = 0;
  for(i = 0; i < NFiles; i++){
    sprintf(tmpstr,"%s/%s", PathName, NameList[i]->d_name);
    //printf("Testing %s ----------------------------------\n",tmpstr);
    IsSiemensDICOM(tmpstr);
    if(IsSiemensDICOM(tmpstr)){
      SeriesNoTest = dcmGetSeriesNo(tmpstr);
      if(SeriesNoTest == SeriesNo){
	SeriesList[*nList] = (char *) calloc(strlen(tmpstr)+1,sizeof(char));
	memcpy(SeriesList[*nList], tmpstr, strlen(tmpstr));
	//printf("%3d  %s\n",*nList,SeriesList[*nList]);
	(*nList)++;
      }
    }
  }
  fprintf(stderr,"INFO: found %d files in series\n",*nList);
  fflush(stderr);

  // free memory
  while(NFiles--) 
  {
    free(NameList[NFiles]);
  }
  free(NameList);
  
  if(*nList == 0){
    free(SeriesList);
    return(NULL);
  }
  free(PathName);

  return( SeriesList );
}

/*-----------------------------------------------------------*/
int SortSDCMFileInfo(SDCMFILEINFO **sdcmfi_list, int nlist)
{
  qsort(sdcmfi_list,nlist,sizeof(SDCMFILEINFO **),CompareSDCMFileInfo);
  return(0);
}

/*-----------------------------------------------------------
  CompareSDCMFileInfo() - compares two siemens dicom files for
  the purposes of sorting them. They are sorted with qsort 
  (via SortSDCMFileInfo()) which sorts them in ascending order.
  If CompareSDCMFileInfo() returns a -1, is means that the 
  first file will appear ahead of the second file in the list,
  and vice versa if a +1 is returned.

  Overall, files are sorted so that the run number always increases,
  regardless of whether a run is a mosaic or non-mosaic. The assignment
  of run number to a file is accomplished with sdfiAssignRunNo().

  Within a non-mosaic ran the first file will sort to that of the
  first frame of the first slice of the first run. If there are
  multiple frames in the run, then the next file will be that of the
  second frame of the first slice of the first run. When all the
  frames of the first slice have been exhausted, the next file will
  contain the first frame of the second slice, and so on.

  Within mosaic runs, the files are sorted in order of temporal
  acquision.

  The first comparison is based on Series Number. If the first file
  has a lower series number, a -1 is returned (ie, those with lower
  series numbers will appear ahead of those with higher numbers in the
  sorted list).  If they both have the same series number, the next
  comparison is evaluated.

  The second comparison is based on relative Slice Position. This is
  determined from the slice direction cosine (SDC) and the XYZ
  coordinates of the first voxel in each file. The SDC points in the
  direction of increasing slice number and is compared to the vector
  pointing from the first voxel of the first file to the first voxel
  of the second file. If they are parallel, then the first file has
  a lower slice number than the second, and it will appear earlier
  in the sorted list (ie, a -1 is returned). The dot product between
  the vector and the SDC is used to determine whether it parallel
  (dot = 1) or anti-parallel (dot = -1). To avoid floating point
  errors, the dot must be greater than 0.5 (parallel) or less than
  -0.5 (anti-parallel), otherwise the next comparison is evaluated.

  The third comparison is the Image Number, which indicates the temporal
  sequence (except in mosaics). Those that occur earlier in time (ie,
  have a lower image number) will appear earlier in the sorted list.

  If the two files cannot be discriminated from these comparisions,
  a warning is printed and a 0 is returned.

  Notes and Assumptions:

  1. The series number always changes from one run to another.
  2. For mosaics, the series number increments with each frame.
  3. For mosaics, the image number is meaningless. Because of
     note 2, the image number comparison should never be reached.
  4. For non-mosaics, image number increases for later acquisitions.
  -----------------------------------------------------------*/
int CompareSDCMFileInfo(const void *a, const void *b)
{

  SDCMFILEINFO *sdcmfi1;
  SDCMFILEINFO *sdcmfi2;
  int n;
  float dv[3], dvsum2, dot;
  //float actm1, actm2;

  sdcmfi1 = *((SDCMFILEINFO **) a);
  sdcmfi2 = *((SDCMFILEINFO **) b);

  if(sdcmfi1->ErrorFlag) return(-1);
  if(sdcmfi2->ErrorFlag) return(+1);

  /* Sort by Series Number */
  if(sdcmfi1->SeriesNo < sdcmfi2->SeriesNo) return(-1);
  if(sdcmfi1->SeriesNo > sdcmfi2->SeriesNo) return(+1);

  /* ------ Sort by Slice Position -------- */
  /* Compute vector from the first to the second */
  dvsum2 = 0;
  for(n=0; n < 3; n++){
    dv[n] = sdcmfi2->ImgPos[n] - sdcmfi1->ImgPos[n];
    dvsum2 += (dv[n]*dv[n]);
  }
  for(n=0; n < 3; n++) dv[n] /= sqrt(dvsum2); /* normalize */
  /* Compute dot product with Slice Normal vector */
  dot = 0;
  for(n=0; n < 3; n++) dot += (dv[n] * sdcmfi1->Vs[n]);
  if(dot > +0.5) return(-1);
  if(dot < -0.5) return(+1);

  /* Sort by Image Number (Temporal Sequence) */
  if(sdcmfi1->ImageNo < sdcmfi2->ImageNo) return(-1);
  if(sdcmfi1->ImageNo > sdcmfi2->ImageNo) return(+1);
  
  /* Sort by Acquisition Time */
  /* This has been commented out because it should be
     redundant with ImageNo */
  //sscanf(sdcmfi1->AcquisitionTime,"%f",&actm1);
  //sscanf(sdcmfi2->AcquisitionTime,"%f",&actm2);
  //if(actm1 < actm2) return(-1);
  //if(actm1 > actm2) return(+1);

  printf("WARNING: files are not found to be different and cannot be sorted\n");
  printf("File1: %s\n",sdcmfi1->FileName);
  printf("File2: %s\n",sdcmfi2->FileName);

  return(0);
}
/*-----------------------------------------------------------
  sdfiAssignRunNo2() - assigns run number based on series number
  -----------------------------------------------------------*/
int sdfiAssignRunNo2(SDCMFILEINFO **sdfi_list, int nlist)
{
  SDCMFILEINFO *sdfi, *sdfitmp, *sdfi0;
  int nthfile, NRuns, nthrun, nthslice, nthframe;
  int nfilesperrun, firstpass, nframes;
  char *FirstFileName = 0;
  int *RunList=0, *RunNoList=0;

  nframes = 0; /* to stop compiler warnings */

  RunNoList = sdfiRunNoList(sdfi_list,nlist,&NRuns);
  if(NRuns==0) 
    return(NRuns);

  for(nthrun = 0; nthrun < NRuns; nthrun ++)
  {
    FirstFileName = sdfiFirstFileInRun(RunNoList[nthrun], sdfi_list, nlist);
    RunList = sdfiRunFileList(FirstFileName,sdfi_list, nlist, &nfilesperrun);

    sdfi0 = sdfi_list[RunList[0]];

    if(sdfi0->IsMosaic){ /* It is a mosaic */
      sdfi0->NFrames = nfilesperrun;
      if(nfilesperrun != (sdfi0->lRepetitions+1)){
	fprintf(stderr,"WARNING: Run %d appears to be truncated\n",nthrun+1);
	fprintf(stderr,"  Files Found: %d, Files Expected (lRep+1): %d\n",
		nfilesperrun, (sdfi0->lRepetitions+1));
	DumpSDCMFileInfo(stderr,sdfi0);
	fflush(stderr);
	sdfi0->ErrorFlag = 1;
      }
    }

    else { /* It is NOT a mosaic */

      nthfile = 0;
      nthslice = 0;
      firstpass = 1;

      while(nthfile < nfilesperrun){
	sdfi    = sdfi_list[RunList[nthfile]];
	sdfitmp = sdfi_list[RunList[nthfile]];
	nthframe = 0;
	while(sdfiSameSlicePos(sdfi,sdfitmp)){
	  nthframe++;
	  nthfile++;
	  if(nthfile < nfilesperrun) 
	    sdfitmp = sdfi_list[RunList[nthfile]];
	  else                   
	    break;
	}
	if(firstpass){
	  firstpass = 0;
	  nframes = nthframe;
	}
	if(nthframe != nframes){
	  fprintf(stderr,"WARNING: Run %d appears to be truncated\n",
		  RunNoList[nthrun]);
	  fprintf(stderr,"  Slice = %d, nthframe = %d, nframes = %d, %d\n",
		  nthslice,nthframe,nframes,firstpass);
	  fflush(stderr);
	  sdfi0->ErrorFlag = 1;
	  break;
	}
	nthslice++;
      }/* end loop over files in the run */
      
      sdfi0->VolDim[2] = nthslice;
      sdfi0->NFrames   = nframes;
    }/* end if it is NOT a mosaic */

    /* Update the parameters for all files in the run */
    for(nthfile = 0; nthfile < nfilesperrun; nthfile ++){
      sdfi = sdfi_list[RunList[nthfile]];
      sdfi->VolDim[2] = sdfi0->VolDim[2];
      sdfi->NFrames   = sdfi0->NFrames;
      sdfi->ErrorFlag = sdfi0->ErrorFlag;
    }

    if (RunList)
    {
      free(RunList); RunList = 0;
    }
    if (FirstFileName)
    {
      free(FirstFileName); FirstFileName = 0;
    }
  } /* end loop over runs */
  if (RunNoList)
  {
    free(RunNoList);  RunNoList = 0;
  }
  return(NRuns);
}
/*-----------------------------------------------------------
  sdfiRunNoList() - returns a list of run numbers
  -----------------------------------------------------------*/
int *sdfiRunNoList(SDCMFILEINFO **sdfi_list, int nlist, int *NRuns)
{
  SDCMFILEINFO *sdfi;
  int nthfile, PrevRunNo;
  int *RunNoList;
  int nthrun;

  *NRuns = sdfiCountRuns(sdfi_list, nlist);
  if(*NRuns == 0) 
    return(NULL);

  RunNoList = (int *) calloc(*NRuns, sizeof(int));

  nthrun = 0;
  PrevRunNo = -1;
  for(nthfile = 0; nthfile < nlist; nthfile ++)
  {
    sdfi = sdfi_list[nthfile];
    if(PrevRunNo == sdfi->RunNo) 
      continue;
    PrevRunNo = sdfi->RunNo;
    RunNoList[nthrun] = sdfi->RunNo;
    fprintf(stderr, "RunNo = %d\n", sdfi->RunNo);
    nthrun++;
  }
  return(RunNoList);
}
/*-----------------------------------------------------------
  sdfiCountRuns() - counts the number of runs in the list
  -----------------------------------------------------------*/
int sdfiCountRuns(SDCMFILEINFO **sdfi_list, int nlist)
{
  SDCMFILEINFO *sdfi;
  int nthfile, NRuns, PrevRunNo;

  NRuns = 0;
  PrevRunNo = -1;
  for(nthfile = 0; nthfile < nlist; nthfile ++)
  {
    sdfi = sdfi_list[nthfile];
    if(PrevRunNo == sdfi->RunNo) 
      continue;
    PrevRunNo = sdfi->RunNo;
    NRuns ++;
  }
  return(NRuns);
}
/*-----------------------------------------------------------
  sdfiCountFilesInRun() - counts the number of files in a 
  given run. This differs from sdfiNFilesInRun() in that
  this takes a run number instead of a file name.
  -----------------------------------------------------------*/
int sdfiCountFilesInRun(int RunNo, SDCMFILEINFO **sdfi_list, int nlist)
{
  SDCMFILEINFO *sdfi;
  int nthfile, NFilesInRun;

  NFilesInRun = 0;
  for(nthfile = 0; nthfile < nlist; nthfile ++){
    sdfi = sdfi_list[nthfile];
    if(sdfi->RunNo == RunNo) NFilesInRun ++;
  }
  return(NFilesInRun);
}
/*-----------------------------------------------------------
  sdfiFirstFileInRun() - returns the name of the first file
  in the given run.
  -----------------------------------------------------------*/
char *sdfiFirstFileInRun(int RunNo, SDCMFILEINFO **sdfi_list, int nlist)
{
  SDCMFILEINFO *sdfi;
  int nthfile, len;
  char *FirstFileName;

  for(nthfile = 0; nthfile < nlist; nthfile ++){
    sdfi = sdfi_list[nthfile];
    if(sdfi->RunNo == RunNo){
      len = strlen(sdfi->FileName);
      FirstFileName = (char *) calloc(len+1,sizeof(char));
      memcpy(FirstFileName,sdfi->FileName,len);
      return(FirstFileName);
    }
  }
  fprintf(stderr,"WARNING: could not find Run %d in list\n",RunNo);
  return(NULL);
}
/*-----------------------------------------------------------
  sdfiAssignRunNo() - assigns a run number to each file. It is assumed
  that the list has already been ordered by SortSDCMFileInfo(), which
  sorts (according to CompareSDCMFileInfo()) by Series Number, Slice
  Position, and Image Number.

  The run number that each file is associated with is determined
  incrementally, starting with the first file in the list.

  If the first file IS NOT A MOSAIC, then all the subsequent files
  in the list with the same series number are assigned to the
  same run as the first file. In this case, the total number of
  files in the run is equal to the number of slices times the number
  of frames. The number of frames is determined by the number of
  files with the same Image Position. The NFrames element of the
  structure for each file in the run is changed to reflect this.

  If the first file IS A MOSAIC, then the number of files in the run
  is assumed to equal the number of frames as indicated by 
  NFrames = lRepeptitions + 1. 

  The process is repeated with the first file of the second run, and
  so on, until all the files are accounted for.

  Returns:
    Number of Runs Found (no error)
    0 if there was an error

  Notes:
    1. List must have been sorted by SortSDCMFileInfo().
    2. For non-mosaics, the number of slices is determined here. 
    2. For non-mosaics with multiple frames, the number of frames
       (ie, ->NFrames) is also determined here.
  -----------------------------------------------------------*/
int sdfiAssignRunNo(SDCMFILEINFO **sdcmfi_list, int nfiles)
{
  int nthfile, nthrun, nthslice, nthframe, serno, sernotest;
  int nfilesperrun;
  int ncols, nrows, nslices, nframes;
  SDCMFILEINFO *sdfi, *sdfitmp;
  char *FirstFile;
  int  FirstFileNo, n, nthfileperrun;
  char *tmpstr;

  tmpstr=NULL;

  nthfile = 0;
  nthrun = 0;

#ifdef _DEBUG
  printf("    File    NthFl Ser Img  NFrs   Run NthFlRun\n");
#endif

  while(nthfile < nfiles){
    nfilesperrun = 0;
    
    sdfi = sdcmfi_list[nthfile];
    FirstFile = sdfi->FileName;
    FirstFileNo = nthfile;

    if(! sdfi->IsMosaic ){
      /*--------------------------------------------------*/
      /* Its not a mosaic --- sort by series no and frame */
      ncols = sdfi->NImageCols;
      nrows = sdfi->NImageRows;
      serno = sdfi->SeriesNo;
      sernotest = serno;
      nthslice = 0;

      while(sernotest == serno){
  sdfitmp = sdfi;
  nthframe = 0;
  
  while(sdfiSameSlicePos(sdfitmp,sdfi)){
    sdfi->RunNo = nthrun;
#ifdef _DEBUG
    tmpstr = fio_basename(sdfi->FileName,NULL);
    printf("%10s %4d  %2d  %3d  %3d   %3d   %3d  (%g,%g,%g)\n",
     tmpstr, nthfile, sdfi->SeriesNo, sdfi->ImageNo, 
     sdfi->NFrames, nthrun, nfilesperrun+1,
     sdfi->ImgPos[0],sdfi->ImgPos[1],sdfi->ImgPos[2]);
    fflush(stdout);
    free(tmpstr);
#endif
    nthframe ++;
    nthfile ++;
    nfilesperrun ++;
    if(nthfile < nfiles){
      sdfi = sdcmfi_list[nthfile];
      sernotest = sdfi->SeriesNo;
    }
    else{
      sernotest = -1;
      break;
    }
  }
  nthslice ++;
      }
      nslices = nthslice;
      nframes = nthframe;
      if(nfilesperrun != (nslices*nframes)){
  printf("ERROR: not enough files for run %d\n",nthrun);
  printf("nslices = %d, nframes = %d\n",nslices,nframes);
  printf("nexpected = %d, nfound = %d\n",(nslices*nframes),
         nfilesperrun);
  printf("FirstFile: %s\n",FirstFile);
  return(0);
      }
    }/* if(mosaic) */
    else{
      /*--------------------------------------------------*/
      /* It is a mosaic -- count out the number of frames */
      nfilesperrun = sdfi->NFrames;
      nframes = sdfi->NFrames; /* this is for compat with non-mos */
      nthfileperrun = 0;
      sdcmIsMosaic(FirstFile, &ncols, &nrows, &nslices,NULL);
      for(nthframe = 0; nthframe < sdfi->NFrames; nthframe++){
  if(nthfile >= nfiles){
    fprintf(stdout,"ERROR: not enough files for %d frames of run %d\n",
      sdfi->NFrames,nthrun);
    fprintf(stdout,"%s\n",FirstFile);
    fflush(stdout);
    return(0);
  }
  sdfi = sdcmfi_list[nthfile];
  sdfi->RunNo = nthrun;
  
#ifdef _DEBUG
  tmpstr = fio_basename(sdfi->FileName,NULL);
  printf("%10s %4d  %2d  %3d  %3d   %3d   %3d \n",
         tmpstr, nthfile, sdfi->SeriesNo, sdfi->ImageNo, 
         sdfi->NFrames, sdfi->RunNo, nthfileperrun+1);
  fflush(stdout);
  free(tmpstr);
#endif
  
  nthfileperrun ++;
  nthfile ++;
      }
    }/* if(not mosaic) */
    
    /* Make sure each fileinfo in the run has a complete
       set of volume dimension information */
    for(n=FirstFileNo; n < FirstFileNo+nfilesperrun; n++){
      sdfi = sdcmfi_list[n];
      sdfi->VolDim[0] = ncols;
      sdfi->VolDim[1] = nrows;
      sdfi->VolDim[2] = nslices;
      sdfi->NFrames   = nframes; /* has no effect for mosaic */
    }

#ifdef _DEBUG
    tmpstr = fio_basename(FirstFile,NULL);
    printf("%2d %10s %3d   (%3d %3d %3d %3d)\n",
     nthrun, tmpstr, nfilesperrun, ncols, nrows, 
     nslices, sdfi->NFrames);
    free(tmpstr);
#endif
    
    nthrun ++;
  }

  return(nthrun);
}
/*------------------------------------------------------------------
  sdfiRunNo() - this returns the Run Number of the given file name. 
  The list is searched until the FileName member matches that of
  dcmfile. The RunNo member is then returned. The sdfi_list must
  have been sorted into runs with sdfiAssignRunNo().
  ------------------------------------------------------------------*/
int sdfiRunNo(char *dcmfile, SDCMFILEINFO **sdfi_list, int nlist)
{
  int nthfile;
  SDCMFILEINFO *sdfi;
  
  for(nthfile = 0; nthfile < nlist; nthfile++){
    sdfi = sdfi_list[nthfile];
    if(strcmp(dcmfile,sdfi->FileName) == 0) return(sdfi->RunNo);
  }

  fprintf(stderr,"WARNING: file %s not found in list\n", dcmfile);

  return(-1);
}
/*------------------------------------------------------------------
  sdfiNFilesInRun() - this returns the number of files in the run
  with dcmfile. The Run Number for dcmfile is obtained using sdfiRunNo().
  The list is searched for all the files with the same Run Number.
  ------------------------------------------------------------------*/
int sdfiNFilesInRun(char *dcmfile, SDCMFILEINFO **sdfi_list, int nlist)
{
  int nthfile;
  int RunNo;
  int NFilesInRun;
  SDCMFILEINFO *sdfi;
  
  RunNo = sdfiRunNo(dcmfile,sdfi_list,nlist);
  if(RunNo < 0) return(1);

  NFilesInRun = 0;
  for(nthfile = 0; nthfile < nlist; nthfile++){
    sdfi = sdfi_list[nthfile];
    if(sdfi->RunNo == RunNo) NFilesInRun ++;
  }

  return(NFilesInRun);
}
/*------------------------------------------------------------------
  sdfiRunFileList() - returns a list of indices from sdfi_list that 
  share the same Run Number. The number in the list is passed as
  NRunList.
  ------------------------------------------------------------------*/
int *sdfiRunFileList(char *dcmfile, SDCMFILEINFO **sdfi_list, 
         int nlist, int *NRunList)
{
  int nthfile, nthfileinrun;
  SDCMFILEINFO *sdfi;
  int *RunList;
  int RunNo;
  
  /* get the run number for this dcmfile */
  RunNo = sdfiRunNo(dcmfile,sdfi_list,nlist);
  if(RunNo < 0) return(NULL);

  /* get the number of files in this run */
  *NRunList = sdfiNFilesInRun(dcmfile, sdfi_list,nlist);

  /* alloc the list of file numbers */
  RunList = (int *) calloc(*NRunList, sizeof(int));

  nthfileinrun = 0;
  for(nthfile = 0; nthfile < nlist; nthfile++){
    sdfi = sdfi_list[nthfile];
    if(sdfi->RunNo == RunNo){
      if(nthfileinrun >= *NRunList){
  free(RunList);
  fprintf(stderr,"ERROR: found more than %d files "
    "in the run for DICOM file %s\n",*NRunList,
    sdfi->FileName);
        return(NULL);
      }
      RunList[nthfileinrun] = nthfile;
      //printf("%3d  %3d\n",nthfileinrun,RunList[nthfileinrun]);
      nthfileinrun ++;
    }
  }
  return(RunList);
}
#if 0
/*-----------------------------------------------------------------
  sdcmLoadVolume() - this loads a volume stored in Siemens DICOM
  format. It scans in the directory of dcmfile searching for all
  Siemens DICOM files. It then determines which of these files belong
  to the same run as dcmfile. If LoadVolume=1, then the pixel data is
  loaded, otherwise, only the header info is assigned.

  Determining which files belong in the same with dcmfile is a 
  tricky process. It requires that all files be queried. It also
  requires that all runs be complete. A missing file will cause
  the process to crash even if the file does not belong to the 
  same run as dcmfile.

  Notes: 
    1. This works only for short data. 
    2. It handles mosaics but not supermosaics. 
    3. It assumes that slices within a mosaic are sorted in 
       anatomical order.
    4. It handles multiple frames for mosaics and non-mosaics.
  -----------------------------------------------------------------*/
MRI * sdcmLoadVolume(char *dcmfile, int LoadVolume)
{
  char *dcmpath;
  SDCMFILEINFO *sdfi;
  DCM_ELEMENT *element;
  SDCMFILEINFO **sdfi_list;
  int nthfile;
  int NRuns, *RunList, NRunList;
  int nlist;
  int ncols, nrows, nslices, nframes;
  int nmoscols, nmosrows;
  int mosrow, moscol, mosindex;
  int err,OutOfBounds,IsMosaic;
  int row, col, slice, frame;
  unsigned short *pixeldata;
  MRI *vol;

  /* Get the directory of the DICOM data from dcmfile */
  dcmpath = fio_dirname(dcmfile);  

  /* Get all the Siemens DICOM files from this directory */
  printf("INFO: scanning path to Siemens DICOM DIR:\n   %s\n",dcmpath);
  sdfi_list = ScanSiemensDCMDir(dcmpath, &nlist);
  if(sdfi_list == NULL){
    printf("ERROR: scanning directory %s\n",dcmpath);
    return(NULL);
  }
  printf("INFO: found %d Siemens files\n",nlist);

  /* Sort the files by Series, Slice Position, and Image Number */
  SortSDCMFileInfo(sdfi_list,nlist);

  /* Assign run numbers to each file (count number of runs)*/
  NRuns = sdfiAssignRunNo2(sdfi_list, nlist);
  if(NRunList == 0){
    printf("ERROR: sorting runs\n");
    return(NULL);
  }

  /* Get a list of indices in sdfi_list of files that belong to dcmfile */
  RunList = sdfiRunFileList(dcmfile, sdfi_list, nlist, &NRunList);
  if(RunList == NULL) return(NULL);

  printf("INFO: found %d runs\n",NRuns);
  printf("INFO: RunNo %d\n",sdfiRunNo(dcmfile,sdfi_list,nlist));
  printf("INFO: found %d files in run\n",NRunList);

  /* First File in the Run */
  sdfi = sdfi_list[RunList[0]];
  sdfiFixImagePosition(sdfi);
  sdfiVolCenter(sdfi);

  /* for easy access */
  ncols   = sdfi->VolDim[0];
  nrows   = sdfi->VolDim[1];
  nslices = sdfi->VolDim[2];
  nframes = sdfi->NFrames;
  IsMosaic = sdfi->IsMosaic;

  /** Allocate an MRI structure **/
  if(LoadVolume){
    vol = MRIallocSequence(ncols,nrows,nslices,MRI_SHORT,nframes);
    if(vol==NULL){
      fprintf(stderr,"ERROR: could not alloc MRI volume\n");
      return(NULL);
    }
  }
  else{
    vol = MRIallocHeader(ncols,nrows,nslices,MRI_SHORT);
    if(vol==NULL){
      fprintf(stderr,"ERROR: could not alloc MRI header \n");
      return(NULL);
    }
  }

  /* set the various paramters for the mri structure */
  vol->xsize = sdfi->VolRes[0];
  vol->ysize = sdfi->VolRes[1];
  vol->zsize = sdfi->VolRes[2];
  vol->x_r   = sdfi->Vc[0];
  vol->x_a   = sdfi->Vc[1];
  vol->x_s   = sdfi->Vc[2];
  vol->y_r   = sdfi->Vr[0];
  vol->y_a   = sdfi->Vr[1];
  vol->y_s   = sdfi->Vr[2];
  vol->z_r   = sdfi->Vs[0];
  vol->z_a   = sdfi->Vs[1];
  vol->z_s   = sdfi->Vs[2];
  vol->c_r   = sdfi->VolCenter[0];
  vol->c_a   = sdfi->VolCenter[1];
  vol->c_s   = sdfi->VolCenter[2];
  vol->ras_good_flag = 1;
  vol->te    = sdfi->EchoTime;
  vol->ti    = sdfi->InversionTime;
  vol->flip_angle  = sdfi->FlipAngle;
  if(! sdfi->IsMosaic )
    vol->tr    = sdfi->RepetitionTime;
  else
    vol->tr    = sdfi->RepetitionTime * sdfi->VolDim[2] / 1000.0;

  /* Return now if we're not loading pixel data */
  if(!LoadVolume) {
    free(RunList);
    return(vol);
  }

  /* Dump info to stdout */
  DumpSDCMFileInfo(stdout,sdfi);

  /* ------- Go through each file in the Run ---------*/
  for(nthfile = 0; nthfile < NRunList; nthfile ++){

    sdfi = sdfi_list[RunList[nthfile]];

    /* Get the pixel data */
    element = GetElementFromFile(sdfi->FileName,0x7FE0,0x10);
    if(element == NULL){
      fprintf(stderr,"ERROR: reading pixel data from %s\n",sdfi->FileName);
      MRIfree(&vol);
    }
    pixeldata = (unsigned short *)(element->d.string);

    if(!IsMosaic){/*---------------------------------------------*/
      /* It's not a mosaic -- load rows and cols from pixel data */
      if(nthfile == 0){
  frame = 0;
  slice = 0;
      }
      printf("%3d %3d %3d %s \n",nthfile,slice,frame,sdfi->FileName);
      for(row=0; row < nrows; row++){
  for(col=0; col < ncols; col++){
    MRISseq_vox(vol,col,row,slice,frame) = *(pixeldata++);
  }
      }
      frame ++;
      if(frame >= nframes){
  frame = 0;
  slice ++;
      }
    }
    else{/*---------------------------------------------*/
      /* It is a mosaic -- load entire volume for this frame from pixel data */
      frame = nthfile;
      nmoscols = sdfi->NImageCols;
      nmosrows = sdfi->NImageRows;
      for(row=0; row < nrows; row++){
  for(col=0; col < ncols; col++){
    for(slice=0; slice < nslices; slice++){
      /* compute the mosaic col and row from the volume
         col, row , and slice */
      err = VolSS2MosSS(col, row, slice, 
            ncols, nrows, 
            nmoscols, nmosrows,
            &moscol, &mosrow, &OutOfBounds);
      if(err || OutOfBounds){
        FreeElementData(element);
        free(element);
        free(RunList);
        MRIfree(&vol);
        exit(1);
      }
      /* Compute the linear index into the block of pixel data */
      mosindex = moscol + mosrow * nmoscols;
      MRISseq_vox(vol,col,row,slice,frame) = *(pixeldata + mosindex);
    }
  }
      }
    }

    FreeElementData(element);
    free(element);

  }/* for nthfile */

  free(RunList);

  return(vol);
}
#endif
/*-----------------------------------------------------------------------
  sdfiFixImagePosition() - Fixes the (RAS) Image Position of a Siemens mosaic.
  In Siemens DICOM files, the Image Position (20,32) is incorrect for
  mosaics. The Image Position is supposed to be the XYZ location of
  the first voxel. It is actually what would be the location of the first
  voxel if a single image the size of the mosaic had been collected centered
  at the first slice in the mosaic. The correction is done by computing the
  center of the first slice (which coincides with the center of the mosaic),
  and then computing the actual XYZ of the first slice based on its center,
  in-plane resolution, in-plane dimension, and direction cosines.
  Author: Douglas N. Greve, 9/12/2001
  -----------------------------------------------------------------------*/
int sdfiFixImagePosition(SDCMFILEINFO *sdfi)
{
  float MosColFoV, MosRowFoV; 
  float ColFoV, RowFoV; 
  float ImgCenter[3], NewImgPos[3];
  int n, nctmos, nrtmos;

  if(!sdfi->IsMosaic) return(0);

  /* Field-of-View, Center-to-Center, of a single slice */
  ColFoV = (sdfi->VolDim[0]-1)*sdfi->VolRes[0];
  RowFoV = (sdfi->VolDim[1]-1)*sdfi->VolRes[1];

  /* Field-of-View, Center-to-Center, of the entire mosaic */
  MosColFoV = (sdfi->NImageCols-1)*sdfi->VolRes[0];
  MosRowFoV = (sdfi->NImageRows-1)*sdfi->VolRes[1];

  /* Number of column and row tiles in the mosaic */
  nctmos = sdfi->NImageCols/sdfi->VolDim[0];
  nrtmos = sdfi->NImageRows/sdfi->VolDim[1];

#ifdef _DEBUG
  printf("--------- sdfiFixImagePosition() -------------\n");
  printf("Old ImgPos:   ");
  for(n=0;n<3;n++) printf("%7.1f ",sdfi->ImgPos[n]);
  printf("\n");
#endif

  for(n=0;n<3;n++){
    /* Compute the center of the mosaic */
    ImgCenter[n] = sdfi->ImgPos[n] + 
      sdfi->Vc[n] * MosColFoV/2 + 
      sdfi->Vr[n] * MosRowFoV/2;
    /* Compute the new Image Position */
    NewImgPos[n] = ImgCenter[n] - 
      sdfi->Vc[n] * MosColFoV/(2*nctmos) - 
      sdfi->Vr[n] * MosRowFoV/(2*nrtmos);
    /* Replace */
    sdfi->ImgPos[n] = NewImgPos[n];
  }

#ifdef _DEBUG
  printf("Image Center: ");
  for(n=0;n<3;n++) printf("%7.1f ",ImgCenter[n]);
  printf("\n");

  printf("New ImgPos:   ");
  for(n=0;n<3;n++) printf("%7.1f ",sdfi->ImgPos[n]);
  printf("\n");
  printf("--------- ---------------------  -------------\n");
#endif

  return(0);
}
/*-----------------------------------------------------------------------
  sdfiVolCenter() - Computes the RAS XYZ "center" of the volume for
  the given the first Siemens DICOM file of a run. For mosaics, this
  assumes that the Image Position has been fixed (see
  sdfiFixImagePosition()). Center is in quotes because it is not
  really the center. Rather it is the RAS XYZ position at voxel CRS =
  [Nc Nr Ns]/2. The true center would be at CRS = ([Nc Nr Ns]-1)/2.
  This definition of the "center" is consistent with the c_r, c_a,
  c_s in the MRI struct.

  Author: Douglas N. Greve, 9/12/2001. Updated 12/19/02.
  -----------------------------------------------------------------------*/
int sdfiVolCenter(SDCMFILEINFO *sdfi)
{
  int r,c;
  float Mdc[3][3], FoV[3];

  for(r=0;r<3;r++){
    Mdc[r][0] = sdfi->Vc[r]; Mdc[r][1] = sdfi->Vr[r]; Mdc[r][2] = sdfi->Vs[r];
  }

  /* Note: for true center use (sdfi->VolDim[r]-1) */
  for(r=0;r<3;r++) FoV[r] = sdfi->VolRes[r] * sdfi->VolDim[r];

  /* sdfi->Volcenter[] is actually the RAS value of the corner voxel */
  /* (0,0,z).  Thus, if you use the first slice, then the value is   */
  /* the same as the translation part                                */
  /* Thus, you just add the Mdc*fov + translation gives c_(r,a,s)    */
  for(r=0;r<3;r++){
    sdfi->VolCenter[r] = sdfi->ImgPos[r];
    for(c=0;c<3;c++){
      sdfi->VolCenter[r] += Mdc[r][c] * FoV[c]/2.0;
    }
  }
  return(0);
}

/*-----------------------------------------------------------------------
  sdfiSameSlicePos() - Determines whether the images in the two files
  have the same slice position.
  Author: Douglas N. Greve, 9/12/2001
  -----------------------------------------------------------------------*/
int sdfiSameSlicePos(SDCMFILEINFO *sdfi1, SDCMFILEINFO *sdfi2)
{
  float eps = .000001;

  if( fabs(sdfi1->ImgPos[0] - sdfi2->ImgPos[0]) > eps ) return(0);
  if( fabs(sdfi1->ImgPos[1] - sdfi2->ImgPos[1]) > eps ) return(0);
  if( fabs(sdfi1->ImgPos[2] - sdfi2->ImgPos[2]) > eps ) return(0);
  return(1);
}

/*--------------------------------------------------------------*/
/* Sebastien's Routines are below */
/*--------------------------------------------------------------*/


/*******************************************************
   PrintDICOMInfo
   Author: Sebastien Gicquel
   Date: 06/04/2001
   input: structure DICOMInfo
   output: prints specific DICOM fields
*******************************************************/
void PrintDICOMInfo(DICOMInfo *dcminfo)
{
  int i;
  char str[256];

  printf("-------------------------------------------------\n");
  printf("DICOM meta-header\n\n");

  printf("file name\t\t\t%s\n", dcminfo->FileName);

  printf("Date and time\n");
  if (IsTagPresent[DCM_StudyDate])
    sprintf(str, "%s", dcminfo->StudyDate);
  else
    strcpy(str, "not found");
  printf("\tstudy date\t\t%s\n", str);

  if (IsTagPresent[DCM_StudyTime])
    sprintf(str, "%s", dcminfo->StudyTime);
  else
    strcpy(str, "not found");
  printf("\tstudy time\t\t%s\n", str);

  if (IsTagPresent[DCM_SeriesTime])
    sprintf(str, "%s", dcminfo->SeriesTime);
  else
    strcpy(str, "not found");
  printf("\tseries time\t\t%s\n", str);

  if (IsTagPresent[DCM_AcquisitionTime])
    sprintf(str, "%s", dcminfo->AcquisitionTime);
  else
    strcpy(str, "not found");
  printf("\tacquisition time\t%s\n", str);

  printf("Identification\n");
  if (IsTagPresent[DCM_PatientName])
    sprintf(str, "%s", dcminfo->PatientName);
  else
    strcpy(str, "not found");
  printf("\tpatient name\t\t%s\n", str);

  if (IsTagPresent[DCM_Manufacturer])
    sprintf(str, "%s", dcminfo->Manufacturer);
  else
    strcpy(str, "not found");
  printf("\tmanufacturer\t\t%s\n", str);

  printf("Dimensions\n");
  if (IsTagPresent[DCM_Rows])
    sprintf(str, "%d", dcminfo->Rows);
  else
    strcpy(str, "not found");
  printf("\tnumber of rows\t\t%s\n", str);

  if (IsTagPresent[DCM_Columns])
    sprintf(str, "%d", dcminfo->Columns);
  else
    strcpy(str, "not found");
  printf("\tnumber of columns\t%s\n", str);

  printf("\tnumber of frames\t%d\n", dcminfo->NumberOfFrames);

  if (IsTagPresent[DCM_xsize])
    sprintf(str, "%g", dcminfo->xsize);
  else
    strcpy(str, "not found");
  printf("\tpixel width\t\t%s\n",  str);

  if (IsTagPresent[DCM_ysize])
    sprintf(str, "%g", dcminfo->ysize);
  else
    strcpy(str, "not found");
  printf("\tpixel height\t\t%s\n",  str);

  if (IsTagPresent[DCM_SliceThickness])
    sprintf(str, "%g", dcminfo->SliceThickness);
  else
    strcpy(str, "not found");
  printf("\tslice thickness\t\t%s\n",  str);

  printf("\tfield of view\t\t%g\n", dcminfo->FieldOfView);

  if (IsTagPresent[DCM_ImageNumber])
    sprintf(str, "%d", dcminfo->ImageNumber);
  else
    strcpy(str, "not found");
  printf("\timage number\t\t%s (might be not reliable)\n", str);

  printf("Acquisition parameters\n");
  if (IsTagPresent[DCM_EchoTime])
    sprintf(str, "%g", dcminfo->EchoTime);
  else
    strcpy(str, "not found");
  printf("\techo time\t\t%s\n",  str);

  if (IsTagPresent[DCM_RepetitionTime])
    sprintf(str, "%g", dcminfo->RepetitionTime);
  else
    strcpy(str, "not found");
  printf("\trepetition time\t\t%s\n", str);

  if (IsTagPresent[DCM_InversionTime])
    sprintf(str, "%g", dcminfo->InversionTime);
  else
    strcpy(str, "not found");
  printf("\tinversion time\t\t%s\n", str);

  if (IsTagPresent[DCM_EchoNumber])
    sprintf(str, "%d", dcminfo->EchoNumber);
  else
    strcpy(str, "not found");
  printf("\techo number\t\t%s\n", str);

  if (IsTagPresent[DCM_FlipAngle])
    sprintf(str, "%g", dcminfo->FlipAngle);
  else
    strcpy(str, "not found");
  printf("\tflip angle\t\t%s\n", str);

  if (IsTagPresent[DCM_BitsAllocated])
    sprintf(str, "%d", dcminfo->BitsAllocated);
  else
    strcpy(str, "not found");
  printf("\tbits allocated\t\t%s\n", str);

  printf("Spatial informations\n");
  
  printf("\tfirst image position\t");
  if (IsTagPresent[DCM_ImagePosition])
    {
      for (i=0; i<3; i++)
  {
    printf("%g ", dcminfo->FirstImagePosition[i]);
  }
      printf("\n");
    }
  else
    printf("notfound\n");

  printf("\tlast image position\t");
  if (IsTagPresent[DCM_ImagePosition])
    {
      for (i=0; i<3; i++)
  {
    printf("%g ", dcminfo->LastImagePosition[i]);
  }
      printf("\n");
    }
  else
    printf("notfound\n");

  printf("\timage orientation\t");
  if (IsTagPresent[DCM_ImageOrientation])
    {
      for (i=0; i<6; i++)
  {
    printf("%g ", dcminfo->ImageOrientation[i]);
  }
      printf("\n");
    }
  else
    printf("notfound\n");

  printf("-------------------------------------------------\n\n");

}


CONDITION GetString(DCM_OBJECT** object, DCM_TAG tag, char **st)
{
  DCM_ELEMENT attribute;
  CONDITION cond;
  void* ctx;

  attribute.tag=tag;
  cond=DCM_GetElement(object, tag, &attribute);
  if (cond != DCM_NORMAL)
    return cond;
  *st=(char*)calloc(attribute.length+1, sizeof(char));
  attribute.d.string=*st;
  ctx=NULL;
  cond=DCM_GetElementValue(object, &attribute, &attribute.length, &ctx);
  return cond;

}

CONDITION GetUSFromString(DCM_OBJECT** object, DCM_TAG tag, unsigned short *us)
{
  DCM_ELEMENT attribute;
  CONDITION cond;
  char* s;
  void* ctx;

  attribute.tag=tag;
  cond=DCM_GetElement(object, tag, &attribute);
  if (cond != DCM_NORMAL)
    return cond;
  s=(char*)calloc(attribute.length+1, sizeof(char));
  attribute.d.string=s;
  ctx=NULL;
  cond=DCM_GetElementValue(object, &attribute, &attribute.length, &ctx);
  *us=(unsigned short)atoi(s);
  free(s);
  return cond;
}

CONDITION GetShortFromString(DCM_OBJECT** object, DCM_TAG tag, short *sh)
{
  DCM_ELEMENT attribute;
  CONDITION cond;
  char* s;
  void* ctx;

  attribute.tag=tag;
  cond=DCM_GetElement(object, tag, &attribute);
  if (cond != DCM_NORMAL)
    return cond;
  s=(char*)calloc(attribute.length+1, sizeof(char));
  attribute.d.string=s;
  ctx=NULL;
  cond=DCM_GetElementValue(object, &attribute, &attribute.length, &ctx);
  *sh=(short)atoi(s);
  free(s);
  return cond;
}

CONDITION GetUSFromUS(DCM_OBJECT** object, DCM_TAG tag, unsigned short *us)
{
  DCM_ELEMENT attribute;
  CONDITION cond;
  void* ctx, *ot;

  attribute.tag=tag;
  cond=DCM_GetElement(object, tag, &attribute);
  if (cond != DCM_NORMAL)
    return cond;
  ot=(void*)malloc(attribute.length+1);
  attribute.d.ot=ot;
  ctx=NULL;
  cond=DCM_GetElementValue(object, &attribute, &attribute.length, &ctx);
  *us=*attribute.d.us;
  free(ot);
  return cond;
}

CONDITION GetShortFromShort(DCM_OBJECT** object, DCM_TAG tag, short *ss)
{
  DCM_ELEMENT attribute;
  CONDITION cond;
  void* ctx, *ot;

  attribute.tag=tag;
  cond=DCM_GetElement(object, tag, &attribute);
  if (cond != DCM_NORMAL)
    return cond;
  ot=(void*)malloc(attribute.length+1);
  attribute.d.ot=ot;
  ctx=NULL;
  cond=DCM_GetElementValue(object, &attribute, &attribute.length, &ctx);
  *ss=*attribute.d.ss;
  free(ot);
  return cond;
}

CONDITION GetPixelData_Save(DCM_OBJECT** object, DCM_TAG tag, unsigned short **ad)
{
  DCM_ELEMENT attribute;
  CONDITION cond;
  void *ctx, *ot;

  attribute.tag=tag;
  cond=DCM_GetElement(object, tag, &attribute);
  if (cond != DCM_NORMAL)
    return cond;
  ot=(void*)malloc(attribute.length+1);
  attribute.d.ot=ot;
  ctx=NULL;
  cond=DCM_GetElementValue(object, &attribute, &attribute.length, &ctx);
  *ad=(unsigned short *)ot;
  return cond;
}

CONDITION GetPixelData(DCM_OBJECT** object, DCM_TAG tag, void **ad)
{
  DCM_ELEMENT attribute;
  CONDITION cond;
  void *ctx, *ot;

  attribute.tag=tag;
  cond=DCM_GetElement(object, tag, &attribute);
  if (cond != DCM_NORMAL)
    return cond;
  ot=(void*)malloc(attribute.length+1);
  attribute.d.ot=ot;
  ctx=NULL;
  cond=DCM_GetElementValue(object, &attribute, &attribute.length, &ctx);
  *ad=ot;
  return cond;
}

CONDITION GetDoubleFromString(DCM_OBJECT** object, DCM_TAG tag, double *d)
{
  DCM_ELEMENT attribute;
  CONDITION cond;
  char* s;
  void* ctx;

  attribute.tag=tag;
  cond=DCM_GetElement(object, tag, &attribute);
  if (cond != DCM_NORMAL)
    return cond;
  s=(char*)calloc(attribute.length+1, sizeof(char));
  attribute.d.string=s;
  ctx=NULL;
  cond=DCM_GetElementValue(object, &attribute, &attribute.length, &ctx);
  *d=atof(s);
  free(s);
  return cond;
}

CONDITION GetMultiDoubleFromString(DCM_OBJECT** object, DCM_TAG tag, double *d[], int multiplicity)
{
  DCM_ELEMENT attribute;
  CONDITION cond;
  char *s, *ss;
  void* ctx;
  int i, j, mult;

  attribute.tag=tag;
  cond=DCM_GetElement(object, tag, &attribute);
  if (cond != DCM_NORMAL)
    return cond;
  s=(char*)calloc(attribute.length+1, sizeof(char));
  ss=(char*)calloc(attribute.length+1, sizeof(char));
  attribute.d.string=s;
  ctx=NULL;
  cond=DCM_GetElementValue(object, &attribute, &attribute.length, &ctx);


  i=0;
  j=0;
  mult=0;
  while (mult<multiplicity && i<attribute.length) {
    j=0;
    while (s[i]!='\\' && i<attribute.length)
      ss[j++]=s[i++];
    i++;
    ss[j]='\0';
    (*d)[mult++]=atof(ss);
  }
  free(s);
  free(ss);
  return cond;
}

CONDITION GetMultiShortFromString(DCM_OBJECT** object, DCM_TAG tag, short *us[], int multiplicity)
{
  DCM_ELEMENT attribute;
  CONDITION cond;
  char *s, *ss;
  void* ctx;
  int i, j, mult;

  attribute.tag=tag;
  cond=DCM_GetElement(object, tag, &attribute);
  if (cond != DCM_NORMAL)
    return cond;
  s=(char*)calloc(attribute.length+1, sizeof(char));
  ss=(char*)calloc(attribute.length+1, sizeof(char));
  attribute.d.string=s;
  ctx=NULL;
  cond=DCM_GetElementValue(object, &attribute, &attribute.length, &ctx);

  i=0;
  j=0;
  mult=0;
  while (mult<multiplicity && i<attribute.length) {
    j=0;
    while (s[i]!='\\' && i<attribute.length)
      ss[j++]=s[i++];
    i++;
    ss[j]='\0';
    (*us)[mult++]=atoi(ss);
  }
  free(s);
  free(ss);
  return cond;
}

/*******************************************************
   GetDICOMInfo
   Author: Sebastien Gicquel
   Date: 06/04/2001
   input: file name, structure DICOMInfo, boolean, image number
   output: fills in structure DICOMInfo with DICOM meta-header fields
*******************************************************/

CONDITION GetDICOMInfo(char *fname, DICOMInfo *dcminfo, BOOL ReadImage, int ImageNumber)
{
  DCM_OBJECT** object=(DCM_OBJECT**)calloc(1, sizeof(DCM_OBJECT*));
  DCM_TAG tag;
  CONDITION cond, cond2=DCM_NORMAL;
  double *tmp=(double*)calloc(10, sizeof(double));
  short *itmp=(short*)calloc(3, sizeof(short));

  int i;

  cond=DCM_OpenFile(fname, DCM_PART10FILE|DCM_ACCEPTVRMISMATCH, object);
  if (cond != DCM_NORMAL)
    cond=DCM_OpenFile(fname, DCM_ORDERLITTLEENDIAN|DCM_ACCEPTVRMISMATCH, object);
  if (cond != DCM_NORMAL)
    cond=DCM_OpenFile(fname, DCM_ORDERBIGENDIAN|DCM_ACCEPTVRMISMATCH, object);
  if (cond != DCM_NORMAL)
    cond=DCM_OpenFile(fname, DCM_FORMATCONVERSION|DCM_ACCEPTVRMISMATCH, object);
  if (cond != DCM_NORMAL)
    {
      printf("not DICOM images, sorry...\n");
      exit(1);
    }

  dcminfo->FileName=(char *)calloc(strlen(fname)+1, sizeof(char));
  strcpy(dcminfo->FileName, fname);
  dcminfo->FileName[strlen(fname)]='\0';

  // manufacturer
  tag=DCM_MAKETAG(0x8, 0x70);
  cond=GetString(object, tag, &dcminfo->Manufacturer);
  if (cond != DCM_NORMAL) {
    dcminfo->Manufacturer=(char *)calloc(256, sizeof(char));
    strcpy(dcminfo->Manufacturer, "UNKNOWN");
    cond2=cond;
#ifdef _DEBUG
    printf("WARNING: tag Manufacturer not found\n"); 
#endif  
  }
  else
    IsTagPresent[DCM_Manufacturer]=true;


  // patient name
  tag=DCM_MAKETAG(0x10, 0x10);
  cond=GetString(object, tag, &dcminfo->PatientName);
  if (cond != DCM_NORMAL) {
    dcminfo->PatientName=(char *)calloc(256, sizeof(char));
    strcpy(dcminfo->PatientName, "UNKNOWN");
    cond2=cond;
#ifdef _DEBUG
    printf("WARNING: tag PatientName not found\n"); 
#endif  
  }
  else
    IsTagPresent[DCM_PatientName]=true;

  // study date
  tag=DCM_MAKETAG(0x8, 0x20);
  cond=GetString(object, tag, &dcminfo->StudyDate);
  if (cond != DCM_NORMAL || strcmp(dcminfo->StudyDate, "00000000")==0) {
    dcminfo->StudyDate=(char *)calloc(256, sizeof(char));
    strcpy(dcminfo->StudyDate, "UNKNOWN");
    cond2=cond;
#ifdef _DEBUG
    printf("WARNING: tag StudyDate not found\n"); 
#endif  
  }
  else
    IsTagPresent[DCM_StudyDate]=true;

  // study time
  tag=DCM_MAKETAG(0x8, 0x30);
  cond=GetString(object, tag, &dcminfo->StudyTime);
  if (cond != DCM_NORMAL || strcmp(dcminfo->StudyTime, "00000000")==0) {
    dcminfo->StudyTime=(char *)calloc(256, sizeof(char));
    strcpy(dcminfo->StudyTime, "UNKNOWN");
    cond2=cond;
#ifdef _DEBUG
    printf("WARNING: tag StudyTime not found\n"); 
#endif  
  }
  else
    IsTagPresent[DCM_StudyTime]=true;

  // series time
  tag=DCM_MAKETAG(0x8, 0x31);
  cond=GetString(object, tag, &dcminfo->SeriesTime);
  if (cond != DCM_NORMAL || strcmp(dcminfo->SeriesTime, "00000000")==0) {
    dcminfo->SeriesTime=(char *)calloc(256, sizeof(char));
    strcpy(dcminfo->SeriesTime, "UNKNOWN");
    cond2=cond;
#ifdef _DEBUG
    printf("WARNING: tag SeriesTime not found\n"); 
#endif  
  }
  else
    IsTagPresent[DCM_SeriesTime]=true;

  // acquisition time
  tag=DCM_MAKETAG(0x8, 0x31);
  cond=GetString(object, tag, &dcminfo->AcquisitionTime);
  if (cond != DCM_NORMAL || strcmp(dcminfo->AcquisitionTime, "00000000")==0) {
    dcminfo->AcquisitionTime=(char *)calloc(256, sizeof(char));
    strcpy(dcminfo->AcquisitionTime, "UNKNOWN");
    cond2=cond;
#ifdef _DEBUG
    printf("WARNING: tag AcquisitionTime not found\n"); 
#endif  
  }
  else
    IsTagPresent[DCM_AcquisitionTime]=true;

  // slice thickness (use Slice Spacing)
  tag=DCM_MAKETAG(0x18, 0x88);
  cond=GetDoubleFromString(object, tag, &dcminfo->SliceThickness);
  if (cond != DCM_NORMAL || dcminfo->SliceThickness==0.0) 
  {
#ifdef _DEBUG
    printf("WARNING: tag Slice Spacing not found\n"); 
#endif  
    // try Slice Thickness tag 
    tag=DCM_MAKETAG(0x18, 0x50);
    cond=GetDoubleFromString(object, tag, &dcminfo->SliceThickness);
    if (cond != DCM_NORMAL || dcminfo->SliceThickness==0.0)
    {
      dcminfo->SliceThickness = 0.0;
      cond2=cond;
#ifdef _DEBUG
      printf("WARNING: tag Slice Thickness not found\n");
#endif
    }
    else
      IsTagPresent[DCM_SliceThickness]=true;
  }
  else
    IsTagPresent[DCM_SliceThickness]=true;

  // image number
  tag=DCM_MAKETAG(0x20, 0x13);
  cond=GetUSFromString(object, tag, &dcminfo->ImageNumber);
  if ((cond != DCM_NORMAL || dcminfo->ImageNumber==0) && ImageNumber!=-1)
    {
      dcminfo->ImageNumber=ImageNumber;
      cond2=cond;
#ifdef _DEBUG
      printf("WARNING: tag ImageNumber not found\n"); 
#endif  
    }
  else
    IsTagPresent[DCM_ImageNumber]=true;

  // rows
  tag=DCM_MAKETAG(0x28, 0x10);
  cond=GetUSFromUS(object, tag, &dcminfo->Rows);
  if (cond != DCM_NORMAL) {
    dcminfo->Rows=0;
    cond2=cond;
#ifdef _DEBUG
    printf("WARNING: tag Rows not found\n"); 
#endif  
  }
  else
    IsTagPresent[DCM_Rows]=true;

  // columns
  tag=DCM_MAKETAG(0x28, 0x11);
  cond=GetUSFromUS(object, tag, &dcminfo->Columns);
  if (cond != DCM_NORMAL) {
    dcminfo->Columns=0;
    cond2=cond;
#ifdef _DEBUG
    printf("WARNING: tag Columns not found\n"); 
#endif  
  }
  else
    IsTagPresent[DCM_Columns]=true;

  // pixel spacing
  tag=DCM_MAKETAG(0x28, 0x30);
  cond=GetMultiDoubleFromString(object, tag, &tmp, 2);
  if (cond != DCM_NORMAL || tmp[0]==0.0 || tmp[1]==0.0) {
    dcminfo->xsize=0.0;
    dcminfo->ysize=0.0;
    cond2=cond;
#ifdef _DEBUG
    printf("WARNING: tag Pixel spacing not found\n"); 
#endif  
  }
  else {
    dcminfo->xsize=tmp[0];
    dcminfo->ysize=tmp[1];
    IsTagPresent[DCM_xsize]=true;
    IsTagPresent[DCM_ysize]=true;
  }

  // bits allocated
  tag=DCM_MAKETAG(0x28, 0x100);
  cond=GetUSFromUS(object, tag, &dcminfo->BitsAllocated);
  if (cond != DCM_NORMAL) {
    dcminfo->BitsAllocated=0;
    cond2=cond;
#ifdef _DEBUG
    printf("WARNING: tag BitsAllocated not found\n"); 
#endif  
  }
  else
    IsTagPresent[DCM_BitsAllocated]=true;

  // repetition time
  tag=DCM_MAKETAG(0x18, 0x80);
  cond=GetDoubleFromString(object, tag, &dcminfo->RepetitionTime);
  if (cond != DCM_NORMAL) 
    {
      dcminfo->RepetitionTime=0;
      cond2=cond;
#ifdef _DEBUG
      printf("WARNING: tag RepetitionTime not found\n"); 
#endif  
    }
  else
    IsTagPresent[DCM_RepetitionTime]=true;
  
  // echo time
  tag=DCM_MAKETAG(0x18, 0x81);
  cond=GetDoubleFromString(object, tag, &dcminfo->EchoTime);
  if (cond != DCM_NORMAL)
    {
      dcminfo->EchoTime=0;
      cond2=cond;
#ifdef _DEBUG
      printf("WARNING: tag EchoTime not found\n"); 
#endif  
    }
  else
    IsTagPresent[DCM_EchoTime]=true;

  // flip angle
  tag=DCM_MAKETAG(0x18, 0x1314);
  cond=GetDoubleFromString(object, tag, &dcminfo->FlipAngle);
  if (cond != DCM_NORMAL)
    {
      dcminfo->FlipAngle=0;
      cond2=cond;
#ifdef _DEBUG
      printf("WARNING: tag FlipAngle not found\n"); 
#endif  
    }
  else{
    dcminfo->FlipAngle = M_PI*dcminfo->FlipAngle/180.0;
    IsTagPresent[DCM_FlipAngle]=true;
  }

  // inversion time
  tag=DCM_MAKETAG(0x18, 0x82);
  cond=GetDoubleFromString(object, tag, &dcminfo->InversionTime);
  if (cond != DCM_NORMAL)
    {
      dcminfo->InversionTime=0;
      cond2=cond;
#ifdef _DEBUG
      printf("WARNING: tag InversionTime not found\n"); 
#endif  
    }
  else
    IsTagPresent[DCM_InversionTime]=true;

  // echo number
  tag=DCM_MAKETAG(0x18, 0x86);
  cond=GetShortFromString(object, tag, &dcminfo->EchoNumber);
  if (cond != DCM_NORMAL)
    {
      dcminfo->EchoNumber=0;
      cond2=cond;
#ifdef _DEBUG
      printf("WARNING: tag EchoNumber not found\n"); 
#endif  
    }
  else
    IsTagPresent[DCM_EchoNumber]=true;

  // image position
  // watch out: DICOM gives the image position in LPS coordinates
  //            We are interested in RAS coordinates (-1,-1,1)!
  tag=DCM_MAKETAG(0x20, 0x32);
  cond=GetMultiDoubleFromString(object, tag, &tmp, 3);
  if (cond != DCM_NORMAL) 
    {
      for (i=0; i<3; i++)
	dcminfo->ImagePosition[i]=0;
      cond2=cond;
#ifdef _DEBUG
      printf("WARNING: tag image position not found\n"); 
#endif  
    }
  else 
    {
      IsTagPresent[DCM_ImagePosition]=true;
      for (i=0; i<3; i++)
	dcminfo->ImagePosition[i]=tmp[i];
    }
  
  // image orientation
  tag=DCM_MAKETAG(0x20, 0x37);
  cond=GetMultiDoubleFromString(object, tag, &tmp, 6);
  if (cond != DCM_NORMAL) {
    for (i=0; i<6; i++)
      dcminfo->ImageOrientation[i]=0;
    cond2=cond;
#ifdef _DEBUG
    printf("WARNING: tag image orientation not found\n"); 
#endif  
  }
  else {
    IsTagPresent[DCM_ImageOrientation]=true;
    for (i=0; i<6; i++)
      dcminfo->ImageOrientation[i]=tmp[i];
  }



  // pixel data
  if (ReadImage) {
    tag=DCM_MAKETAG(0x7FE0, 0x10);
    cond=GetPixelData(object, tag, &dcminfo->PixelData);
    if (cond != DCM_NORMAL)
      {
  dcminfo->PixelData=NULL;
  cond2=cond;
#ifdef _DEBUG
  printf("WARNING: tag pixel data not found\n"); 
#endif  
      }
  }

  cond=DCM_CloseObject(object);
  if (cond != DCM_NORMAL)
    cond2=cond;
  
  dcminfo->FieldOfView=dcminfo->xsize*dcminfo->Rows;

  /* Clear the condition stack to prevent overflow */
  COND_PopCondition(1);

  free(tmp);
  free(itmp);
  return cond2;
}


/*******************************************************
   ReadDICOMImage
   Author: Sebastien Gicquel
   Date: 06/04/2001
   input: array of file names, number of elements
   output: pixels image (8 or 16 bits), and DICOM informations stored in first array element
*******************************************************/

void *ReadDICOMImage(int nfiles, DICOMInfo **aDicomInfo)
{
  int n, i, j, bitsAllocated, numberOfFrames, offset, nvox;
  DICOMInfo dcminfo;
  unsigned char *PixelData8, *v8=NULL;
  unsigned short *PixelData16, *v16=NULL;
  int npix;
  double firstPosition[3], lastPosition[3];
  unsigned short int min16=65535, max16=0;
  unsigned short int min8=255, max8=0;

  npix=aDicomInfo[0]->Columns*aDicomInfo[0]->Rows;
  numberOfFrames=nfiles;
  nvox=npix*numberOfFrames;
  aDicomInfo[0]->NumberOfFrames=numberOfFrames;
  bitsAllocated=aDicomInfo[0]->BitsAllocated;

  printf("reading DICOM image...\n");

  switch (bitsAllocated)
  {
  case 8:
    v8=(unsigned char *)calloc(nvox, sizeof(unsigned char));
    if (v8==NULL) {
      printf("ReadDICOMImage: allocation problem\n");
      exit(1);
    }
    break;
  case 16:
    v16=(unsigned short *)calloc(nvox, sizeof(unsigned short));
    if (v16==NULL) {
      printf("ReadDICOMImage: allocation problem\n");
      exit(1);
    }
    break;
  } // switch
  
  for (n=0; n<nfiles; n++) 
  {
    GetDICOMInfo(aDicomInfo[n]->FileName, &dcminfo, TRUE, n);
    
    if (n==0)
      for (i=0; i<3; i++)
	aDicomInfo[0]->FirstImagePosition[i]=dcminfo.ImagePosition[i];
    if (n==nfiles-1)
      for (i=0; i<3; i++)
	aDicomInfo[0]->LastImagePosition[i]=dcminfo.ImagePosition[i];
    
    offset=npix*n;
    
    switch (dcminfo.BitsAllocated) 
    {
    case 8:  
      PixelData8=(unsigned char*)dcminfo.PixelData;
      for (j=0; j<npix; j++)
      {
        v8[offset+j]=PixelData8[j];
        if (PixelData8[j]>max8)
	  max8=PixelData8[j];
        else if (PixelData8[j]<min8)
	  min8=PixelData8[j];
      }
      aDicomInfo[0]->min8=min8;
      aDicomInfo[0]->max8=max8;    
      free(PixelData8);
      break;
      
    case 16:  
      PixelData16=(unsigned short*)dcminfo.PixelData;
      for (j=0; j<npix; j++)
      {
        v16[offset+j]=PixelData16[j];
        if (PixelData16[j]>max16)
	  max16=PixelData16[j];
        else if (PixelData16[j]<min16)
	  min16=PixelData16[j];
      }
      aDicomInfo[0]->min16=min16;
      aDicomInfo[0]->max16=max16;
      free(PixelData16);
      break;
    }
  }

  for (i=0; i<3; i++)
    aDicomInfo[0]->ImagePosition[i]=(firstPosition[i]+lastPosition[i])/2.;
  
  switch (bitsAllocated)
  {
  case 8:
    return((void *)v8);
    break;
  case 16:
    return((void *)v16);
    break;
  default:
    return NULL;
  }
}

// ReadDICOMImage2
//
///////////////////////////////////////////////////////////////////
void *ReadDICOMImage2(int nfiles, DICOMInfo **aDicomInfo, int startIndex)
{
  int n, i, j, bitsAllocated, numberOfFrames, offset, nvox;
  DICOMInfo dcminfo;
  unsigned char *PixelData8, *v8=NULL;
  unsigned short *PixelData16, *v16=NULL;
  int npix;
  double firstPosition[3], lastPosition[3];
  unsigned short int min16=65535, max16=0;
  unsigned short int min8=255, max8=0;

  npix=aDicomInfo[startIndex]->Columns*aDicomInfo[startIndex]->Rows;
  numberOfFrames=nfiles;
  nvox=npix*numberOfFrames;
  // filling missing info
  aDicomInfo[startIndex]->NumberOfFrames=numberOfFrames;
  bitsAllocated=aDicomInfo[startIndex]->BitsAllocated;

  printf("reading DICOM image...\n");

  switch (bitsAllocated)
  {
  case 8:
    v8=(unsigned char *)calloc(nvox, sizeof(unsigned char));
    if (v8==NULL) {
      printf("ReadDICOMImage: allocation problem\n");
      exit(1);
    }
    break;
  case 16:
    v16=(unsigned short *)calloc(nvox, sizeof(unsigned short));
    if (v16==NULL) {
      printf("ReadDICOMImage: allocation problem\n");
      exit(1);
    }
    break;
  } // switch
  
  for (n=0; n<nfiles; n++) 
  {
    GetDICOMInfo(aDicomInfo[n+startIndex]->FileName, &dcminfo, TRUE, n);
    
    if (n==0)
      // fill missing info
      for (i=0; i<3; i++)
	aDicomInfo[startIndex]->FirstImagePosition[i]=dcminfo.ImagePosition[i];
    if (n==nfiles-1)
      // fill missing info
      for (i=0; i<3; i++)
	aDicomInfo[startIndex]->LastImagePosition[i]=dcminfo.ImagePosition[i];
    
    offset=npix*n;
    
    switch (dcminfo.BitsAllocated) 
    {
    case 8:  
      PixelData8=(unsigned char*)dcminfo.PixelData;
      for (j=0; j<npix; j++)
      {
        v8[offset+j]=PixelData8[j];
        if (PixelData8[j]>max8)
	  max8=PixelData8[j];
        else if (PixelData8[j]<min8)
	  min8=PixelData8[j];
      }
      // fill missing info
      aDicomInfo[startIndex]->min8=min8;
      aDicomInfo[startIndex]->max8=max8;    
      free(PixelData8);
      break;
      
    case 16:  
      PixelData16=(unsigned short*)dcminfo.PixelData;
      for (j=0; j<npix; j++)
      {
        v16[offset+j]=PixelData16[j];
        if (PixelData16[j]>max16)
	  max16=PixelData16[j];
        else if (PixelData16[j]<min16)
	  min16=PixelData16[j];
      }
      // fill missing info
      aDicomInfo[startIndex]->min16=min16;
      aDicomInfo[startIndex]->max16=max16;
      free(PixelData16);
      break;
    }
  }
  // fill missing info
  for (i=0; i<3; i++)
    aDicomInfo[startIndex]->ImagePosition[i]=(firstPosition[i]+lastPosition[i])/2.;
  
  switch (bitsAllocated)
  {
  case 8:
    return((void *)v8);
    break;
  case 16:
    return((void *)v16);
    break;
  default:
    return NULL;
  }
}


/*******************************************************
   SortFiles
   Author: Sebastien Gicquel
   Date: 06/04/2001
   input: array of file names, number of elements
   output: array of structures DICOMInfo sorted by study date and image number, number of studies encountered in the list of files
   comments: conventional MR scans can usually be sorted by study date, then image number. Other data type (i.e. mosaic functional data)
             may have different discrimination fields.

*******************************************************/

void SortFiles(char *fNames[], int nFiles, DICOMInfo ***ptrDicomArray, int *nStudies)
{
  int n, npermut;
  BOOL done;
  
  DICOMInfo **dicomArray, *storage;
  dicomArray=(DICOMInfo **)calloc(nFiles, sizeof(DICOMInfo *));
  *ptrDicomArray=dicomArray;
  
  for (n=0; n<nFiles; n++)
  {
    if ((dicomArray[n]=(DICOMInfo *)calloc(1, sizeof(DICOMInfo)))==NULL)
    {
      printf("DICOM conversion (SortFiles): can not allocate %d bytes\n", (int)sizeof(DICOMInfo));
      exit(1);
    }
    GetDICOMInfo(fNames[n], dicomArray[n], FALSE, 1);
  }

  // sort by acquisition time, then image number
  done=false;
  while (!done)
  {
    npermut=0;
    for (n=0; n<nFiles-1; n++)
      if (strcmp(dicomArray[n]->AcquisitionTime, dicomArray[n+1]->AcquisitionTime)>0)  // 2nd time inferior to first
      {
	storage=dicomArray[n];
	dicomArray[n]=dicomArray[n+1];
	dicomArray[n+1]=storage;
	npermut++;
      }
    done=(npermut==0);      
  }  

  // sort by image number
  done=false;
  while (!done)
  {
    npermut=0;
    for (n=0; n<nFiles-1; n++)
      if (strcmp(dicomArray[n]->AcquisitionTime, dicomArray[n+1]->AcquisitionTime)==0
	  && dicomArray[n]->ImageNumber>dicomArray[n+1]->ImageNumber)
      {
	storage=dicomArray[n];
	dicomArray[n]=dicomArray[n+1];
	dicomArray[n+1]=storage;
	npermut++;
      }
    done=(npermut==0);      
  }  
  
  // check studies number
  *nStudies=1;
  for (n=0; n<nFiles-1; n++)
    if (strcmp(dicomArray[n]->AcquisitionTime, 
	       dicomArray[n+1]->AcquisitionTime)!=0)
      (*nStudies)++;
  
  if (*nStudies > 1) 
  {
    printf("WARNING: DICOM conversion, %d different acquisition times "
	   "have been identified\n", *nStudies);
  }
}

/*******************************************************
   IsDICOM
   Author: Sebastien Gicquel
   Date: 06/04/2001
   input: DICOM file name
   output: true if file is DICOM part 10 (version 3.0) compliant
*******************************************************/

int IsDICOM(char *fname)
{
  int d;
  FILE *fp;
  CONDITION cond;
  DCM_OBJECT *object = 0;
  static int yes = 0;          // statically initialized
  static char file[1024] = ""; // statically initialized
  
  // use the cached value if the fname is the same as privious one
  if (!strcmp(fname, file)) // used before
    return yes;
  else
  {
    yes = 0;                // initialize
    strcpy(file, fname);    // save the current filename
  }

  d = 0;

  if(d) printf("Entering IsDICOM (%s)\n",fname);
  fflush(stdout);fflush(stderr);

  /* DCM_ACCEPTVRMISMATCH appears to be needed with data produced 
     by a recent (2/10/03) Siemens upgrade */

  /* check that the file exists */
  fp = fopen(fname,"r");
  if(fp == NULL) return(0);
  fclose(fp);

  /* check that file is not a directory*/
  if(fio_IsDirectory(fname)) return(0);

  if(d) printf("Opening %s as part10\n",fname);
  COND_PopCondition(1); /* Clears the Condition Stack */
  fflush(stdout);fflush(stderr);
  cond=DCM_OpenFile(fname, DCM_PART10FILE|DCM_ACCEPTVRMISMATCH, &object);
  fflush(stdout); fflush(stderr);
  if (cond != DCM_NORMAL && d) {
    DCMPrintCond(cond);
    COND_DumpConditions(); 
  }

  if (cond != DCM_NORMAL){
    DCM_CloseObject(&object);
    if(d) printf("Opening as littleendian\n");
    cond=DCM_OpenFile(fname, DCM_ORDERLITTLEENDIAN|DCM_ACCEPTVRMISMATCH, 
		      &object);
    if (cond != DCM_NORMAL && d) 
      DCMPrintCond(cond);
  }

  if (cond != DCM_NORMAL){
    DCM_CloseObject(&object);
    if(d) printf("Opening as bigendian\n");
    cond=DCM_OpenFile(fname, DCM_ORDERBIGENDIAN|DCM_ACCEPTVRMISMATCH, 
		      &object);
    if (cond != DCM_NORMAL && d) 
      DCMPrintCond(cond);
  }

  if (cond != DCM_NORMAL){
    DCM_CloseObject(&object);
    if(d) printf("Opening as format conversion\n");
    cond=DCM_OpenFile(fname, DCM_FORMATCONVERSION|DCM_ACCEPTVRMISMATCH, 
		      &object);
    if (cond != DCM_NORMAL && d) 
      DCMPrintCond(cond);
  }
  // if(cond == DCM_NORMAL) 
  DCM_CloseObject(&object);

  fflush(stdout);fflush(stderr);

  if(d) printf("Leaving IsDICOM (%s)\n",fname);

  COND_PopCondition(1); /* Clears the Condition Stack */

  // cache the current value
  if (cond==DCM_NORMAL)
    yes = 1;
  else
    yes = 0;

  return(cond == DCM_NORMAL);
}
/*---------------------------------------------------------------*/
static int DCMPrintCond(CONDITION cond)
{
  switch (cond){
    case DCM_NORMAL: printf("DCM_NORMAL\n"); break;
    case DCM_ILLEGALOPTION: printf("DCM_ILLEGALOPTION\n"); break;
    case DCM_OBJECTCREATEFAILED: printf("DCM_OBJECTCREATEFAILED\n"); break;
    case DCM_FILEOPENFAILED: printf("DCM_FILEOPENFAILED\n"); break;
    case DCM_FILEACCESSERROR: printf("DCM_FILEACCESSERROR\n"); break;
    case DCM_ELEMENTOUTOFORDER: printf("DCM_ELEMENTOUTOFORDER\n"); break;
    default: printf("DCMPrintCond: %d unrecognized\n",(int)cond); break;
  }
  return(0);
}

/*******************************************************
   ScanDir
   Author: Sebastien Gicquel
   Date: 06/04/2001
   input: directory name
   output: array of files listed in directory, and number of files
*******************************************************/

int ScanDir(char *PathName, char ***FileNames, int *NumberOfFiles)
{
  char **pfn;
  struct dirent **NameList;
  int i, length, pathlength;

  pathlength=strlen(PathName);
  /* select all directory entries, and sort them by name */
  *NumberOfFiles = scandir(PathName, &NameList, 0, alphasort);

  if (*NumberOfFiles < 0)
    return -1;

  pfn = (char **)calloc(*NumberOfFiles, sizeof(char *));
  for (i=0; i<*NumberOfFiles; i++)
    {
      length=pathlength+strlen(NameList[i]->d_name)+1;
      pfn[i]=(char *)calloc(length, sizeof(char));
      sprintf(pfn[i], "%s%s", PathName, NameList[i]->d_name);
    }

  free(NameList);
  *FileNames=pfn;
  return 0;
}

#ifdef SunOS
/* added by kteich for solaris, since it doesn't have them by default. */
/* these funcs Copyright (c) Joerg-R. Hill, December 2000 */
int scandir(const char *dir, struct_dirent ***namelist,
            int (*select)(const struct_dirent *),
            int (*compar)(const void *, const void *))
{
  DIR *d;
  struct_dirent *entry;
  register int i=0;
  size_t entrysize;

  if ((d=opendir(dir)) == NULL)
    return(-1);

  *namelist=NULL;
  while ((entry=readdir(d)) != NULL)
    {
      if (select == NULL || (select != NULL && (*select)(entry)))
  {
    *namelist=(struct_dirent **)realloc((void *)(*namelist),
                (size_t)((i+1)*sizeof(struct_dirent *)));
    if (*namelist == NULL) return(-1);
    entrysize=sizeof(struct_dirent)-sizeof(entry->d_name)+strlen(entry->d_name)+1;
    (*namelist)[i]=(struct_dirent *)malloc(entrysize);
    if ((*namelist)[i] == NULL) return(-1);
    memcpy((*namelist)[i], entry, entrysize);
    i++;
  }
    }
  if (closedir(d)) return(-1);
  if (i == 0) return(-1);
  if (compar != NULL)
    qsort((void *)(*namelist), (size_t)i, sizeof(struct_dirent *), compar);
    
  return(i);
}
int alphasort(const void *a, const void *b)
{
  struct_dirent **da = (struct_dirent **)a;
  struct_dirent **db = (struct_dirent **)b;
  return(strcmp((*da)->d_name, (*db)->d_name));
}
#endif


/*******************************************************
   CleanFileNames
   Author: Sebastien Gicquel
   Date: 06/04/2001
   input: array of file names, number of elements
   output: array of file names that are DICOM 3.0 compliant 
*******************************************************/

int CleanFileNames(char **FileNames, int NumberOfDICOMFiles, char ***CleanedFileNames)
{
  
  char **pfn;
  int i, j, length;

  pfn = (char **)calloc(NumberOfDICOMFiles, sizeof(char *));
  for (i=0, j=0; j<NumberOfDICOMFiles; i++){
    if (IsDICOM(FileNames[i])){
      length=strlen(FileNames[i])+1;
      pfn[j]=(char *)calloc(length, sizeof(char));
      strcpy(pfn[j], FileNames[i]);
      j++;
    }
  }

  printf("Found %d DICOM Files\n",j);

  *CleanedFileNames=pfn;
  return DCM_NOERROR;
}

/* kteich - renamed this to myRound because round() was causing
   versioning problems */
int myRound(double d)
{
  double c, f, ddown, dup; 

  c=ceil(d);
  f=floor(d);
  ddown=d-f;
  dup=c-d;
  if (ddown<dup)
    return (int)f;
  else
    return (int)c;
}

/*******************************************************
   RASFromOrientation
   Author: E'beth Haley
   Date: 08/01/2002
   input: structure MRI, structure DICOMInfo
   output: fill in MRI structure RAS-related fields
   called-by: DICOMInfo2MRI
*******************************************************/

void DebugPrint(FILE *fp, const char *msg, ...)
{
#ifndef __OPTIMIZE
  va_list ap;
  va_start(ap, msg);
  vfprintf(fp, msg, ap);
  va_end(ap);
#endif
}


int RASFromOrientation(MRI *mri, DICOMInfo *dcm)

     /*E* 

       This code is intended to repair and generalize
       RASFromOrientation, which is used for GE DICOM scans, to make
       it more like the Doug Greve Siemens DICOM code, in that it
       should refer to the direction-cosines matrix and not just try
       to match up r/a/s with x/y/z.

       meta: Doug Greve notes that we'd eventually like to be able to
       load n frames at a time, and in that case, we'd probably have
       to rewrite Sebastien's code from scratch - the least reason for
       that is S uses "frames" to refer to slices=files.  In the
       meantime, I'm just going to reimplement the function
       RASFromOrientation, Dougstyle.

       Note to self: the DICOMInfo struct is a Sebastienism.  Only
       Sebastien code uses it.  The Siemens stuff struct is
       SDCMFILEINFO.

       Note to self: okay, the DICOMInfo knows the fname it was
       created from, so we can use that to get things there are tags
       for that don't appear in the DICOMInfo itself.

       Siemens version: sdcmLoadVolume returns an MRI which it gets by
       passing a dcmfilename to ScanSiemensDCMDir which returns a list
       of SDCMFILEINFO's (that it gets from GetSDCMFileInfo which gets
       stuff from a dcmfile)

       RASFromOrientation is passed a DICOMInfo (that has been partway
       filled in by DICOMInfo2MRI() - any other info we need we can
       get from dcm->fname), and sets these elements of the mri
       struct:

       mri->x_r, mri->y_r, mri->z_r,
       mri->x_a, mri->y_a, mri->z_a,
       mri->x_s, mri->y_s, mri->z_s,
       mri->c_r, mri->c_a, mri->c_s,
       mri->ras_good_flag=1

       By the time that calls RASFromOrientation(), the mri is already
       partly filled in by DICOMInfo2MRI().  So it already has:

       mri->width = dcm->Columns;
       mri->height = dcm->Rows;
       mri->depth = dcm->NumberOfFrames;
       nvox = dcm->Rows*dcm->Columns*dcm->NumberOfFrames;
       mri->fov = dcm->FieldOfView;
       mri->thick = dcm->SliceThickness;
       mri->xsize = dcm->xsize;
       mri->ysize = dcm->ysize;
       mri->zsize = dcm->SliceThickness;
       mri->nframes = 1;
       mri->tr = dcm->RepetitionTime;
       mri->te = dcm->EchoTime;
       mri->ti = dcm->InversionTime;
       mri->flip_angle = dcm->FlipAngle;
       mri->imnr0 = 1;
       mri->imnr1 = dcm->NumberOfFrames;

       We'll need e.g. dcm->NumberOfFrames (NSlices).

       We'll more or less use:

       T =  [ (Mdc * Delta)    P_o ]
            [    0    0    0    1  ]

       where Mdc = [ c r s ] is the matrix of direction cosines, which
       we'll get from dcmImageDirCos(dcm->FileName, cx,cy,cz,
       rx,ry,rz) plus a few lines (that should probably become a
       dcmSliceDirCos()) to get the slice column, sx,sy,sz.

       Well, okay, we're not using the 4x4 T, but rather
       T3x3=Mdc*Delta and P_o separately.

       Delta = pixel size in mm = get from dcmGetVolRes() (or use {
       dcm->xsize, dcm->ysize, dcm->SliceThickness }.  BTW, this
       code's Delta is the vector that the matrix Delta should be the
       diag of: diag(Delta_c, Delta_r, Delta_s).

       get P_o = [ P_ox P_oy P_oz ] from dcmImagePosition(char
       *dcmfile, float *x, float *y, float *z) on first file - or
       dcm->FirstImagePosition - wait, those are off by factors of -1,
       -1, 1 from each other.

       The reason is that DICOM coordinate system is LPS, meanwhile
       we use RAS coordinate system. that is why (-1,-1,1) multiplication
       occurs.   

       Note that FirstImagePosition and LastImagePosition are
       in LPS coordinate system.  In order to obtain the RAS direction cosine,
       you have to change them to RAS by mutliplying (-1,-1,1).

       n[3] = [ width, height, depth ] = { dcm->Columns -1, dcm->Rows
       -1, dcm->NumberOfFrames -1 }

       and finally c_ras = center = T3x3 * n/2

     */
{
  float c[3], r[3], s[3], Delta[3], c_ras[3],
    Mdc[3][3], T3x3[3][3], rms; // absIO[6];

  int i,j, n[3];

  // note Doug does the conversion from LPS to RAS and thus
  // these have the correct values
  dcmImageDirCos(dcm->FileName, c, c+1, c+2, r, r+1, r+2);

  // slice direction is not given in dicom
  //
  // we had to calculate
  // one way is to use the image position to do so
  // if number of slice is greater than one.
  // 
  // The image position is given in DICOM coordinate system, which is a 
  // LPS (left-posterior-superior) coordinate system.  We are interested in 
  // RAS (right-anterior-superior) coordinate system.  
  // The first two coordinates sign flips, but not the last.  
  //
  // Actually if you used dcmImagePosition(), then this would be the case!
  //
  // Mixing Sebastian routines with Doug routines is a bad thing to do :-(...
  // 
  if (dcm->NumberOfFrames > 1) // means = NSlices > 0
  {
    rms = 0.0;
    for (i=0; i<3; i++)
    {
      if (i < 2) // convert LP to RA
	s[i] = dcm->FirstImagePosition[i] - dcm->LastImagePosition[i];
      else       // S stays the same
	s[i] = dcm->LastImagePosition[i] - dcm->FirstImagePosition[i];
      rms += s[i]*s[i];
    }
    rms = sqrt(rms);
    for (i=0; i<3; i++)
      s[i]/=rms;
  }
  else
  {
    // build s[i] from c[i] and r[i] 
    // there is an ambiguity in sign due to left-handed or right-handed coords
    // we pick right-handed.  It does not matter anyway.
    // note that c[] and r[] are normalized already and thus no need to calculate
    // the normalization factor.
    s[0] = c[1]*r[2] - c[2]*r[1];
    s[1] = c[2]*r[0] - c[0]*r[2];
    s[2] = c[0]*r[1] - c[1]*r[0];
  }

  for (i=0; i<3; i++)
  {
    Mdc[i][0] = c[i];
    Mdc[i][1] = r[i];
    Mdc[i][2] = s[i];
  }

  // when one slice is given, it is zero
  dcmGetVolRes(dcm->FileName, Delta, Delta+1, Delta+2);
  for (i=0; i<3; i++)
  {
    for (j=0; j<3; j++)
    { 
      T3x3[i][j] = Mdc[i][j] * Delta[j];
    }
  }
  
  /* fill in n, number */
  n[0] = dcm->Columns -1;
  n[1] = dcm->Rows -1;
  n[2] = dcm->NumberOfFrames -1;  /*E* This is Slices :( */

  /*E* The -1 is for how many center-to-center lengths there are.  */
  /*E*
    Doug: c_ras = center = T * n /2.0
    
    i.e. P_o + Mdc * Delta * n/2
    
    Okay, that's: take first voxel and go to center by adding on half
    of [ reorder and rescale the nc/nr/ns ] = lengths of the sides of
    the volume.
    
    dcmImagePosition differs from DICOM file first image position by
    factors of -1 -1 1.  Sebastien code also multiplies by
    diag{-1,-1,1}, so it should be fine to use dcmImagePosition.
  */
  dcmImagePosition(dcm->FileName, c_ras, c_ras+1, c_ras+2); /*E* = P_o */

  for (i=0; i<3; i++)
  {
    for (j=0; j<3; j++)
    {
      c_ras[i] += T3x3[i][j] * (float) n[j] /2.;
    }
  }
  // printing a matrix, you can call call MatrixPrint(stdout, m) in gdb
  // no need for #ifdef ..
  /*E* Now set mri values - the direction cosines.  I stole them
    straight from Sebastien's code, but - Are these backwards??  Ok,
    05aug02, transposed them: */

  mri->x_r = Mdc[0][0];
  mri->y_r = Mdc[0][1];
  mri->z_r = Mdc[0][2];
  mri->x_a = Mdc[1][0];
  mri->y_a = Mdc[1][1];
  mri->z_a = Mdc[1][2];
  mri->x_s = Mdc[2][0];
  mri->y_s = Mdc[2][1];
  mri->z_s = Mdc[2][2];
  mri->c_r = c_ras[0];
  mri->c_a = c_ras[1];
  mri->c_s = c_ras[2];
  mri->ras_good_flag = 1;

  return 0;
}

/*******************************************************
   DICOM16To8
   Author: Sebastien Gicquel
   Date: 06/06/2001
   input: array of 16-bit pixels, number of elements
   output: array of 8-bit pixels
*******************************************************/
unsigned char *DICOM16To8(unsigned short *v16, int nvox)
{
  unsigned char *v8;
  int i;
  double min16, max16, min8, max8, ratio;

  min8=0; 
  max8=255;
  
  v8=(unsigned char *)calloc(nvox, sizeof(unsigned char));
  if (v8==NULL)
    {
      printf("DICOMInfo2MRI: can't allocate %d bytes\n", nvox);
      exit(1);
    }
  
  for (i=0, min16=65535, max16=0; i<nvox; i++) {
    if (v16[i]>max16)
      max16=(double)v16[i];
    if (v16[i]<min16)
      min16=(double)v16[i];
  }

  ratio = (max8-min8)/(max16-min16); 
  for (i=0; i<nvox; i++)  
    v8[i]=(unsigned char)((double)(v16[i])*ratio);
  
  return v8;
}
/*******************************************************
   DICOMInfo2MRI
   Author: Sebastien Gicquel
   Date: 06/05/2001
   input: structure DICOMInfo, pixel data 
   output: fill in MRI structure, including the image
*******************************************************/

int DICOMInfo2MRI(DICOMInfo *dcm, void *data, MRI *mri)
{
  long n, nvox;
  int i, j, k;
  unsigned char *data8;
  unsigned short *data16;

  // fill in the fields
  strcpy(mri->fname, dcm->FileName);

  mri->width=dcm->Columns;
  mri->height=dcm->Rows;
  mri->depth=dcm->NumberOfFrames;
  nvox=dcm->Rows*dcm->Columns*dcm->NumberOfFrames;

  mri->fov=dcm->FieldOfView;
  mri->thick=dcm->SliceThickness;
  mri->xsize=dcm->xsize;
  mri->ysize=dcm->ysize;
  mri->zsize=dcm->SliceThickness;
  mri->nframes=1;

  mri->tr=dcm->RepetitionTime;
  mri->te=dcm->EchoTime;
  mri->ti=dcm->InversionTime;
  mri->flip_angle=dcm->FlipAngle;

  mri->imnr0=1;
  mri->imnr1=dcm->NumberOfFrames;

  RASFromOrientation(mri, dcm);
  
  switch (dcm->BitsAllocated) {
  case 8:
    data8=(unsigned char *)data;
    for (k=0, n=0; k<dcm->NumberOfFrames; k++)
      for (j=0; j<dcm->Rows; j++)
  for (i=0; i<dcm->Columns; i++, n++)
    MRIvox(mri, i, j, k) = data8[n];
    break;
  case 16:
    data16=(unsigned short *)data;
    for (k=0, n=0; k<dcm->NumberOfFrames; k++)
      for (j=0; j<dcm->Rows; j++)
  for (i=0; i<dcm->Columns; i++, n++)
    MRISvox(mri, i, j, k) = data16[n];
    break;
  }
 
  return 0;
}


/*******************************************************
   DICOMRead
   Author: Sebastien Gicquel
   Date: 06/04/2001
   input: directory name, boolean 
   output: MRI structure, including the image if input boolean is true
   FileName points to a DICOM filename. All DICOM files in same directory will be read

   This routine is used for non-Siemens DICOM files
*******************************************************/

int DICOMRead(char *FileName, MRI **mri, int ReadImage)
{
  MRI *pmri=NULL;
  char **CleanedFileNames, **FileNames, *c, PathName[256];
  int i, NumberOfFiles, NumberOfDICOMFiles, nStudies, error;
  int length;
  DICOMInfo **aDicomInfo;
  unsigned char *v8=NULL;
  unsigned short *v16=NULL;
  FILE *fp;
  int numFiles;
  int inputIndex;
  int nextIndex;
  int inputImageNumber;
  int *startIndices;
  int count;

  for (i=0; i< NUMBEROFTAGS; i++)
    IsTagPresent[i]=false;

  // find pathname from filename
  c = strrchr(FileName, '/');
  if(c == NULL)
    PathName[0] = '\0';
  else
    {
      length = (int)(c - FileName);
      strncpy(PathName, FileName, length);
      PathName[length] = '/';
      PathName[length+1] = '\0';
    }

  // first check whether this is a siemens dicom file or not
  if (IsSiemensDICOM(FileName))
    *mri=sdcmLoadVolume(FileName, 1, -1);

  // scan directory
  error=ScanDir(PathName, &FileNames, &NumberOfFiles);

  for (i=0, NumberOfDICOMFiles=0; i<NumberOfFiles; i++)
  {
    if (IsDICOM(FileNames[i]))
    {
      NumberOfDICOMFiles++;
    }
  }
  // no DICOM files in directory
  if (NumberOfDICOMFiles==0)
  {
    fprintf(stderr, "no DICOM files found. Exit\n");
    exit(1);
  }
  else
    printf("%d DICOM 3.0 files in list\n", NumberOfDICOMFiles);


  // remove non DICOM file names
  CleanFileNames(FileNames, NumberOfDICOMFiles, &CleanedFileNames);
  
  // sort DICOM files by study date, then image number
  SortFiles(CleanedFileNames, NumberOfDICOMFiles, &aDicomInfo, &nStudies);
  // if more than 1 studies in the file then do more work to create
  // a list of files which to make an output file.   Then use that
  // to print out the information.
  inputIndex = 0;
  inputImageNumber = 1;
  count = 0;
  if (nStudies>1)
  {
    // create an array of starting indices
    startIndices = (int *) calloc(nStudies, sizeof(int));

    printf("Generating log file dicom.log\n");
    fp = fopen("dicom.log", "w");
    if (fp==NULL) {
      printf("Can not create file dicom.log\n");
    }
    else 
    {
      printf("Starting filenames for the studies\n");
      for (i=0; i<NumberOfDICOMFiles; i++)
      {
	fprintf(fp, "%s\t%d\n", aDicomInfo[i]->FileName, aDicomInfo[i]->ImageNumber);
	// find the index for the input file
	if (!strcmp(FileName, aDicomInfo[i]->FileName))
	{
	  inputIndex = i;
	  inputImageNumber = aDicomInfo[i]->ImageNumber;
	}
	// print starting filename for the studies
	if (i == 0)
	{
	  startIndices[count] = i;
	  count++;
	  printf(" %s\n", aDicomInfo[i]->FileName);
	}
	// get a different studies
	else if (i != 0 && strcmp(aDicomInfo[i]->AcquisitionTime,aDicomInfo[i-1]->AcquisitionTime)!=0)
	{
	  startIndices[count] = i;
	  count++;
	  printf(" %s\n", aDicomInfo[i]->FileName);
	}
      }
      fclose(fp);
    }
    // ok found the Dicominfo for the input file and its image number.
    // look for the starting index for this selected list
    nextIndex = inputIndex;
    for (i=0; i < count; i++)
    {
      // startIndices is bigger than inputIndex, then it is not the
      // starting index of the selected file.
      if (startIndices[i] > inputIndex)
      {
	if (i > 0)
	  inputIndex = startIndices[i-1]; // inputIndex is the previous one
	nextIndex = startIndices[i];      // next series starting here
	break;
      }
    }
    // could end up nextIndex = inputIndex
    if (inputIndex == nextIndex)
      nextIndex = inputIndex+1; 

    free((void *) startIndices);
  }
  else // only 1 studies
    nextIndex = inputIndex + NumberOfDICOMFiles;
  // make sure that it is within the range
  if (inputIndex < 0 || inputIndex > NumberOfDICOMFiles-1)
  {
    fprintf(stderr, "Failed to find the right starting index for the image %s.  Exit.\n", 
	    FileName);
    exit(1);
  }
  // no longer relies on the image number.
  // verify ImageNumber to be 1
  // if (aDicomInfo[inputIndex]->ImageNumber != 1)
  // {
  //   fprintf(stderr, "The image number of the first image was not 1 for the list.  Exit.\n");
  //   exit(1);
  // }
  if (nStudies > 1)
  {
    printf("Use %s as the starting filename for this extraction.\n", aDicomInfo[inputIndex]->FileName);
    printf("If like to use the different one, please pick one of the files listed above.\n");
  }
  numFiles = nextIndex-inputIndex;
  // verify with aDicomInfo->NumberOfFrames;
  if (aDicomInfo[inputIndex]->NumberOfFrames != numFiles)
  {
    printf("WARNING: NumberOfFrames %d != Found Count of slices %d.\n",
	   aDicomInfo[inputIndex]->NumberOfFrames,
	   numFiles);
  }
  // find the total number of files belonging to this list
  // the list has been sorted and thus look for the end.
  // numFiles = aDicomInfo[inputIndex]->NumberOfFrames; this is not reliable
  // use the ImageNumber directly
  //
  // Watch out!!!!!!
  // Note that some dicom image consists of many images put into one
  // the increment of image number can be 30
  // If the image number increament is greater than one, we bail out
  // 
  // non siemens dicom file cannot handle mosaic image
  // nextIndex - inputIndex is the number of files.  
  // last image number - first image number > numfiles then must be mosaic
  if ((aDicomInfo[nextIndex-1]->ImageNumber - aDicomInfo[inputIndex]->ImageNumber)
       > numFiles)
  {
    fprintf(stderr, "Currently non-Siemens mosaic image cannot be handled.  Exit\n");
    exit(1);
  }
  switch (aDicomInfo[inputIndex]->BitsAllocated)
  {
    case 8:
      v8=(unsigned char *)ReadDICOMImage2(numFiles, aDicomInfo, inputIndex);
      pmri=MRIallocSequence(aDicomInfo[inputIndex]->Columns, 
			    aDicomInfo[inputIndex]->Rows, 
			    numFiles, MRI_UCHAR, 1);
      DICOMInfo2MRI(aDicomInfo[inputIndex], (void *)v8, pmri);
      free(v8);
      break;
    case 16:
      v16=(unsigned short *)ReadDICOMImage2(numFiles, aDicomInfo, inputIndex);
      pmri=MRIallocSequence(aDicomInfo[inputIndex]->Columns, 
			    aDicomInfo[inputIndex]->Rows, 
			    numFiles, MRI_SHORT, 1);
      DICOMInfo2MRI(aDicomInfo[inputIndex], (void *)v16, pmri);
      free(v16);
      break;
  }
  
  // display only first DICOM header
  PrintDICOMInfo(aDicomInfo[inputIndex]);

  *mri=pmri;

  return 0;
}

#if 0
/* 9/25/01 This version does not assume that the series number is good */
MRI * sdcmLoadVolume(char *dcmfile, int LoadVolume)
{
  char *dcmpath;
  SDCMFILEINFO *sdfi;
  DCM_ELEMENT *element;
  SDCMFILEINFO **sdfi_list;
  int nthfile;
  int NRuns, *RunList, NRunList;
  int nlist,n;
  int ncols, nrows, nslices, nframes;
  int nmoscols, nmosrows;
  int mosrow, moscol, mosindex;
  int err,OutOfBounds,IsMosaic;
  int row, col, slice, frame;
  unsigned short *pixeldata;
  MRI *vol;
  char **SeriesList;

  slice = 0; frame = 0; /* to avoid compiler warnings */

  /* Get the directory of the DICOM data from dcmfile */
  dcmpath = fio_dirname(dcmfile);  

  /* Get all the Siemens DICOM files from this directory */
  printf("INFO: scanning path to Siemens DICOM DIR:\n   %s\n",dcmpath);
  sdfi_list = ScanSiemensDCMDir(dcmpath, &nlist);
  if(sdfi_list == NULL){
    printf("ERROR: scanning directory %s\n",dcmpath);
    return(NULL);
  }
  printf("INFO: found %d Siemens files\n",nlist);

  /* Sort the files by Series, Slice Position, and Image Number */

  /* Assign run numbers to each file (count number of runs)*/
  printf("Assigning Run Numbers\n");
  NRuns = sdfiAssignRunNo2(sdfi_list, nlist);
  if(NRunList == 0){
    printf("ERROR: sorting runs\n");
    return(NULL);
  }

  /* Get a list of indices in sdfi_list of files that belong to dcmfile */
  RunList = sdfiRunFileList(dcmfile, sdfi_list, nlist, &NRunList);
  if(RunList == NULL) return(NULL);

  printf("INFO: found %d runs\n",NRuns);
  printf("INFO: RunNo %d\n",sdfiRunNo(dcmfile,sdfi_list,nlist));
  printf("INFO: found %d files in run\n",NRunList);
  fflush(stdout);

  /* First File in the Run */
  sdfi = sdfi_list[RunList[0]];
  sdfiFixImagePosition(sdfi);
  sdfiVolCenter(sdfi);

  /* for easy access */
  ncols   = sdfi->VolDim[0];
  nrows   = sdfi->VolDim[1];
  nslices = sdfi->VolDim[2];
  nframes = sdfi->NFrames;
  IsMosaic = sdfi->IsMosaic;

  printf("INFO: (%3d %3d %3d), nframes = %d, ismosaic=%d\n",
   ncols,nrows,nslices,nframes,IsMosaic);
  fflush(stdout);
  fflush(stdout);

  /** Allocate an MRI structure **/
  if(LoadVolume){
    vol = MRIallocSequence(ncols,nrows,nslices,MRI_SHORT,nframes);
    if(vol==NULL){
      fprintf(stderr,"ERROR: could not alloc MRI volume\n");
      return(NULL);
    }
  }
  else{
    vol = MRIallocHeader(ncols,nrows,nslices,MRI_SHORT);
    if(vol==NULL){
      fprintf(stderr,"ERROR: could not alloc MRI header \n");
      return(NULL);
    }
  }

  /* set the various paramters for the mri structure */
  vol->xsize = sdfi->VolRes[0];
  vol->ysize = sdfi->VolRes[1];
  vol->zsize = sdfi->VolRes[2];
  vol->x_r   = sdfi->Vc[0];
  vol->x_a   = sdfi->Vc[1];
  vol->x_s   = sdfi->Vc[2];
  vol->y_r   = sdfi->Vr[0];
  vol->y_a   = sdfi->Vr[1];
  vol->y_s   = sdfi->Vr[2];
  vol->z_r   = sdfi->Vs[0];
  vol->z_a   = sdfi->Vs[1];
  vol->z_s   = sdfi->Vs[2];
  vol->c_r   = sdfi->VolCenter[0];
  vol->c_a   = sdfi->VolCenter[1];
  vol->c_s   = sdfi->VolCenter[2];
  vol->ras_good_flag = 1;
  vol->te    = sdfi->EchoTime;
  vol->ti    = sdfi->InversionTime;
  vol->flip_angle  = sdfi->FlipAngle;
  if(! sdfi->IsMosaic )
    vol->tr    = sdfi->RepetitionTime;
  else
    vol->tr    = sdfi->RepetitionTime * (sdfi->VolDim[2]+1) / 1000.0;

  /* Return now if we're not loading pixel data */
  if(!LoadVolume) {
    free(RunList);
    return(vol);
  }

  /* Dump info to stdout */
  DumpSDCMFileInfo(stdout,sdfi);

  /* ------- Go through each file in the Run ---------*/
  for(nthfile = 0; nthfile < NRunList; nthfile ++){

    sdfi = sdfi_list[RunList[nthfile]];

    /* Get the pixel data */
    element = GetElementFromFile(sdfi->FileName,0x7FE0,0x10);
    if(element == NULL){
      fprintf(stderr,"ERROR: reading pixel data from %s\n",sdfi->FileName);
      MRIfree(&vol);
    }
    pixeldata = (unsigned short *)(element->d.string);

    if(!IsMosaic){/*---------------------------------------------*/
      /* It's not a mosaic -- load rows and cols from pixel data */
      if(nthfile == 0){
  frame = 0;
  slice = 0;
      }
#ifdef _DEBUG      
      printf("%3d %3d %3d %s \n",nthfile,slice,frame,sdfi->FileName);
#endif
      for(row=0; row < nrows; row++){
  for(col=0; col < ncols; col++){
    MRISseq_vox(vol,col,row,slice,frame) = *(pixeldata++);
  }
      }
      frame ++;
      if(frame >= nframes){
  frame = 0;
  slice ++;
      }
    }
    else{/*---------------------------------------------*/
      /* It is a mosaic -- load entire volume for this frame from pixel data */
      frame = nthfile;
      nmoscols = sdfi->NImageCols;
      nmosrows = sdfi->NImageRows;
      for(row=0; row < nrows; row++){
  for(col=0; col < ncols; col++){
    for(slice=0; slice < nslices; slice++){
      /* compute the mosaic col and row from the volume
         col, row , and slice */
      err = VolSS2MosSS(col, row, slice, 
            ncols, nrows, 
            nmoscols, nmosrows,
            &moscol, &mosrow, &OutOfBounds);
      if(err || OutOfBounds){
        FreeElementData(element);
        free(element);
        free(RunList);
        MRIfree(&vol);
        exit(1);
      }
      /* Compute the linear index into the block of pixel data */
      mosindex = moscol + mosrow * nmoscols;
      MRISseq_vox(vol,col,row,slice,frame) = *(pixeldata + mosindex);
    }
  }
      }
    }
    
    FreeElementData(element);
    free(element);

  }/* for nthfile */

  free(RunList);

  return(vol);
}
#endif

#if 0
/* This has been replaced by probing tag 18,88 */
/*-----------------------------------------------------------------------
  sdcmMosaicSliceRes() - computes the slice resolution as the distance
  between the first two slices. The slice locations are obtained from
  the Siemens ASCII header. One can get the slice thickness from tag
  (18,50), but this will not include a skip. See also dcmGetVolRes().
  -----------------------------------------------------------------------*/
float sdcmMosaicSliceRes(char *dcmfile)
{
  char * strtmp;
  float x0, y0, z0;
  float x1, y1, z1;
  float thickness;

  /* --- First Slice ---- */
  strtmp = SiemensAsciiTag(dcmfile,"sSliceArray.asSlice[0].sPosition.dSag");
  if(strtmp != NULL){
    sscanf(strtmp,"%f",&x0);
    free(strtmp);
  }
  else x0 = 0;

  strtmp = SiemensAsciiTag(dcmfile,"sSliceArray.asSlice[0].sPosition.dCor");
  if(strtmp != NULL){
    sscanf(strtmp,"%f",&y0);
    free(strtmp);
  }
  else y0 = 0;

  strtmp = SiemensAsciiTag(dcmfile,"sSliceArray.asSlice[0].sPosition.dTra");
  if(strtmp != NULL){
    sscanf(strtmp,"%f",&z0);
    free(strtmp);
  }
  else z0 = 0;

  /* --- Second Slice ---- */
  strtmp = SiemensAsciiTag(dcmfile,"sSliceArray.asSlice[1].sPosition.dSag");
  if(strtmp != NULL){
    sscanf(strtmp,"%f",&x1);
    free(strtmp);
  }
  else x1 = 0;

  strtmp = SiemensAsciiTag(dcmfile,"sSliceArray.asSlice[1].sPosition.dCor");
  if(strtmp != NULL){
    sscanf(strtmp,"%f",&y1);
    free(strtmp);
  }
  else y1 = 0;

  strtmp = SiemensAsciiTag(dcmfile,"sSliceArray.asSlice[1].sPosition.dTra");
  if(strtmp != NULL){
    sscanf(strtmp,"%f",&z1);
    free(strtmp);
  }
  else z1 = 0;

  /* Compute distance between slices */
  thickness = sqrt( (x0-x1)*(x0-x1) + (y0-y1)*(y0-y1) + (z0-z1)*(z0-z1) );

  return(thickness);
}

#endif
