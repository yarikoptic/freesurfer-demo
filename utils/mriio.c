/*
 *       FILE NAME:   mriio.c
 *
 *       DESCRIPTION: utilities for reading/writing MRI data structure
 *
 *       AUTHOR:      Bruce Fischl
 *       DATE:        4/12/97
 *
 */

/*-----------------------------------------------------
  INCLUDE FILES
  -------------------------------------------------------*/
#define USE_ELECTRIC_FENCE 1
#define _MRIIO_SRC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#include "utils.h"
#include "error.h"
#include "proto.h"
#include "mri.h"
#include "macros.h"
#include "diag.h"
#include "volume_io.h"
#include "region.h"
#include "machine.h"
#include "analyze.h"
#include "fio.h"
#include "mri_identify.h"
#include "signa.h"
#include "fio.h"
#include "matfile.h"
#include "math.h"
#include "matrix.h"
#include "diag.h"
#include "chklc.h"
#include "mriColorLookupTable.h"
#include "DICOMRead.h"
#include "imautils.h"
#include "cma.h"
#include "Bruker.h"
#include "bfileio.h"
#include "AFNI.h"
#include "mghendian.h"
#include "tags.h"
#ifdef Darwin
// /usr/include/zconf.h should typedef Byte, but doesnt on the Mac:
typedef unsigned char  Byte;  /* 8 bits */
#endif
#include "nifti1.h"
#include "nifti1_io.h"
#include "znzlib.h"
#include "NrrdIO.h"
static int niiPrintHdr(FILE *fp, struct nifti_1_header *hdr);

// unix director separator
#define DIR_SEPARATOR '/'
#define CURDIR "./"

#define MM_PER_METER  1000.0f
#define INFO_FNAME    "COR-.info"

#ifdef Linux
extern void swab(const void *from, void *to, size_t n);
#endif

#if 0
static int NormalizeVector(float *v, int n);
static MRI *mincRead2(char *fname, int read_volume);
static int mincWrite2(MRI *mri, char *fname);
static int GetMINCInfo(MRI *mri,
                       char *dim_names[4],
                       int   dim_sizes[4],
                       Real separations[4],
                       Real dircos[3][3],
                       Real VolCenterVox[3],
                       Real VolCenterWorld[3]);
#endif

static MRI *mri_read
(char *fname, int type, int volume_flag, int start_frame, int end_frame);
static MRI *corRead(char *fname, int read_volume);
static int corWrite(MRI *mri, char *fname);
static MRI *siemensRead(char *fname, int read_volume);
static MRI *readGCA(char *fname) ;

static MRI *mincRead(char *fname, int read_volume);
static int mincWrite(MRI *mri, char *fname);
static int bvolumeWrite(MRI *vol, char *fname_passed, int type);
//static int bshortWrite(MRI *mri, char *fname_passed);
//static int bfloatWrite(MRI *mri, char *stem);
static int write_bhdr(MRI *mri, FILE *fp);
static int read_bhdr(MRI *mri, FILE *fp);

static MRI *bvolumeRead(char *fname_passed, int read_volume, int type);
//static MRI *bshortRead(char *fname_passed, int read_volume);
//static MRI *bfloatRead(char *fname_passed, int read_volume);
static MRI *genesisRead(char *stem, int read_volume);
static MRI *gelxRead(char *stem, int read_volume);

static int CountAnalyzeFiles(char *analyzefname, int nzpad, char **ppstem);
static MRI *analyzeRead(char *fname, int read_volume);
static dsr *ReadAnalyzeHeader(char *hdrfile, int *swap,
                              int *mritype, int *bytes_per_voxel);
static int DumpAnalyzeHeader(FILE *fp, dsr *hdr);


static int analyzeWrite(MRI *mri, char *fname);
static int analyzeWriteFrame(MRI *mri, char *fname, int frame);
static int analyzeWriteSeries(MRI *mri, char *fname);
static int analyzeWrite4D(MRI *mri, char *fname);

static void swap_analyze_header(dsr *hdr);

#if 0
static int orient_with_register(MRI *mri);
#endif
static int nan_inf_check(MRI *mri);
#ifdef VC_TO_CV
static int voxel_center_to_center_voxel
(MRI *mri, float *x, float *y, float *z);
#endif
static MRI *gdfRead(char *fname, int read_volume);
static int gdfWrite(MRI *mri, char *fname);

static MRI *ximgRead(char *fname, int read_volume);

static MRI *nifti1Read(char *fname, int read_volume);
static int nifti1Write(MRI *mri, char *fname);
static MRI *niiRead(char *fname, int read_volume);
static int niiWrite(MRI *mri, char *fname);
static int niftiQformToMri(MRI *mri, struct nifti_1_header *hdr);
static int mriToNiftiQform(MRI *mri, struct nifti_1_header *hdr);
static int mriToNiftiSform(MRI *mri, struct nifti_1_header *hdr);
static int niftiSformToMri(MRI *mri, struct nifti_1_header *hdr);
static void swap_nifti_1_header(struct nifti_1_header *hdr);
static MRI *MRISreadCurvAsMRI(char *curvfile, int read_volume);

static MRI *mriNrrdRead(char *fname, int read_volume);
static int mriNrrdWrite(MRI *mri, char *fname);

/********************************************/

static void short_local_buffer_to_image
(short *buf, MRI *mri, int slice, int frame) ;

static void int_local_buffer_to_image
(int *buf, MRI *mri, int slice, int frame) ;

static void long32_local_buffer_to_image
(long32 *buf, MRI *mri, int slice, int frame) ;

static void float_local_buffer_to_image
(float *buf, MRI *mri, int slice, int frame) ;

static void local_buffer_to_image
(BUFTYPE *buf, MRI *mri, int slice, int frame) ;

static MRI *sdtRead(char *fname, int read_volume);
static MRI *mghRead(char *fname, int read_volume, int frame) ;
static int mghWrite(MRI *mri, char *fname, int frame) ;
static int mghAppend(MRI *mri, char *fname, int frame) ;

/********************************************/

extern int errno;

extern char *Progname;
/*char *Progname;*/

static char *command_line;
static char *subject_name;

#define MAX_UNKNOWN_LABELS  100

static short cma_field[512][512];
static char unknown_labels[MAX_UNKNOWN_LABELS][STRLEN];
static int n_unknown_labels;

//////////////////////////////////////////////////////////////////////
// this is a one way of setting direction cosine
// when the direction cosine is not provided in the volume.
// may not agree with the volume. what can we do?  Let them set by themselves.
int setDirectionCosine(MRI *mri, int orientation)
{
  switch(orientation)
    {
    case MRI_CORONAL: // x is from right to left.
                      // y is from top to neck, z is from back to front
      mri->x_r = -1; mri->y_r =  0; mri->z_r =  0; mri->c_r = 0;
      mri->x_a =  0; mri->y_a =  0; mri->z_a =  1; mri->c_a = 0;
      mri->x_s =  0; mri->y_s = -1; mri->z_s =  0; mri->c_s = 0;
      break;
    case MRI_SAGITTAL: // x is frp, back to front,
                       // y is from top to neck, z is from left to right
      mri->x_r =  0; mri->y_r =  0; mri->z_r =  1; mri->c_r = 0;
      mri->x_a =  1; mri->y_a =  0; mri->z_a =  0; mri->c_a = 0;
      mri->x_s =  0; mri->y_s = -1; mri->z_s =  0; mri->c_s = 0;
      break;
    case MRI_HORIZONTAL: // x is from right to left,
                         // y is from front to back, z is from neck to top
      mri->x_r = -1; mri->y_r =  0; mri->z_r =  0; mri->c_r = 0;
      mri->x_a =  0; mri->y_a = -1; mri->z_a =  0; mri->c_a = 0;
      mri->x_s =  0; mri->y_s =  0; mri->z_s =  1; mri->c_s = 0;
      break;
    default:
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM, "setDirectionCosine():unknown slice direction"));
      break; // should not reach here (handled at the conversion)
    }
  return (NO_ERROR);
}

#define isOne(a)  (fabs(fabs(a)-1)<0.00001)

// here I take the narrow view of slice_direction
int getSliceDirection(MRI *mri)
{
  int direction = MRI_UNDEFINED;

  if (isOne(mri->x_r) && isOne(mri->y_s) && isOne(mri->z_a))
    direction = MRI_CORONAL;
  else if (isOne(mri->x_a) && isOne(mri->y_s) && isOne(mri->z_r))
    direction = MRI_SAGITTAL;
  else if (isOne(mri->x_r) && isOne(mri->y_a) && isOne( mri->z_s))
    direction = MRI_HORIZONTAL;
  return direction;
}

// For surface, we currently cannot handle volumes with general slice direction
// nor we cannot handle non-conformed volumes
int mriOKforSurface(MRI *mri)
{
  // first check slice direction
  if (getSliceDirection(mri) != MRI_CORONAL)
    return 0;
  // remove slice size limitation
  // else if (mri->width != 256 || mri->height != 256 || mri->depth != 256)
  //  return 0;
  // remove check for 1 mm size
  //  else if (mri->xsize != 1 || mri->ysize != 1 || mri->zsize != 1)
  //  return 0;
  else
    return 1;
}

int mriConformed(MRI *mri)
{
  // first check slice direction
  if (getSliceDirection(mri) != MRI_CORONAL)
    return 0;
  else if (mri->width != 256 || mri->height != 256 || mri->depth != 256)
    return 0;
  else if (mri->xsize != 1 || mri->ysize != 1 || mri->zsize != 1)
    return 0;
  else
    return 1;
}

void setMRIforSurface(MRI *mri)
{
  if (!mriOKforSurface(mri))
    ErrorExit(ERROR_BADPARM,
              "%s: the volume is not conformed, that is, "
              "the volume must be in CORONAL direction.\n", Progname) ;
#if 0
  else
    {
      // we checked conformed in mriOKforSurface().
      // The only thing missing is c_(r,a,s) = 0
      // for surface creation assume that the
      // volume is conformed and c_(r,a,s) = 0
      mri->c_r=mri->c_a=mri->c_s = 0;
      if (mri->i_to_r__)
        MatrixFree(&mri->i_to_r__) ;
      if (mri->r_to_i__)
        MatrixFree(&mri->r_to_i__) ;
      mri->i_to_r__ = extract_i_to_r(mri);
      mri->r_to_i__ = extract_r_to_i(mri);
    }
#endif
}

int mriio_command_line(int argc, char *argv[])
{

  int i;
  int length;
  char *c;

  length = 0;
  for(i = 0;i < argc;i++)
    length += strlen(argv[i]);

  /* --- space for spaces and \0 --- */
  length += argc;

  command_line = (char *)malloc(length);

  c = command_line;
  for(i = 0;i < argc;i++)
    {
      strcpy(c, argv[i]);
      c += strlen(argv[i]);
      *c = (i == argc-1 ? '\0' : ' ');
      c++;
    }

  return(NO_ERROR);

} /* end mriio_command_line() */

int mriio_set_subject_name(char *name)
{

  if(subject_name == NULL)
    subject_name = (char *)malloc(STRLEN);

  if(subject_name == NULL)
    {
      errno = 0;
      ErrorReturn(ERROR_NO_MEMORY, (ERROR_NO_MEMORY,
                                    "mriio_set_subject_name(): "
                                    "could't allocate %d bytes...!", STRLEN));
    }

  if(name == NULL)
    strcpy(subject_name, name);
  else
    {
      free(subject_name);
      subject_name = NULL;
    }

  return(NO_ERROR);

} /* end mriio_set_subject_name() */

int MRIgetVolumeName(char *string, char *name_only)
{

  char *at, *pound;

  strcpy(name_only, string);

  if((at = strrchr(name_only, '@')) != NULL)
    *at = '\0';

  if((pound = strrchr(name_only, '#')) != NULL)
    *pound = '\0';

  return(NO_ERROR);

} /* end MRIgetVolumeName() */

static MRI *mri_read
(char *fname, int type, int volume_flag, int start_frame, int end_frame)
{
  MRI *mri, *mri2;
  IMAGE *I;
  char fname_copy[STRLEN];
  char *ptmpstr;
  char *at, *pound, *colon;
  char *ep;
  int i, j, k, t;
  int volume_frames;

  // sanity-checks
  if(fname == NULL)
    {
      errno = 0;
      ErrorReturn(NULL, (ERROR_BADPARM, "mri_read(): null fname!\n"));
    }
  if(fname[0] == 0)
    {
      errno = 0;
      ErrorReturn(NULL, (ERROR_BADPARM, "mri_read(): empty fname!\n"));
    }

  // if filename does not contain any directory separator, then add cwd
  if (!strchr(fname,DIR_SEPARATOR))
    {
      char *cwd = getcwd(NULL, 0); // posix 1 extension
                                   // (allocate as much space needed)
      if (cwd)
        {
          strcpy(fname_copy, cwd);
          strcat(fname_copy, "/");
          strcat(fname_copy, fname);
          free(cwd);
        }
      else // why fail?
        strcpy(fname_copy, fname);
    }
  else
    strcpy(fname_copy, fname);

  at = strrchr(fname_copy, '@');
  pound = strrchr(fname_copy, '#');

  if(at != NULL)
    {
      *at = '\0';
      at++;
    }

  if(pound != NULL)
    {
      *pound = '\0';
      pound++;
    }

  if(at != NULL)
    {
      type = string_to_type(at);
      if(type == MRI_VOLUME_TYPE_UNKNOWN)
        {
          errno = 0;
          ErrorReturn
            (NULL, (ERROR_BADPARM, "mri_read(): unknown type '%s'\n", at));
        }
    }
  else if(type == MRI_VOLUME_TYPE_UNKNOWN)
    {
      type = mri_identify(fname_copy);
      if(type == MRI_VOLUME_TYPE_UNKNOWN)
        {
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADPARM,
              "mri_read(): couldn't determine type of file %s", fname_copy));
        }
    }

  if(pound != NULL)
    {
      colon = strchr(pound, ':');
      if(colon != NULL)
        {
          *colon = '\0';
          colon++;
          if(*colon == '\0')
            {
              errno = 0;
              ErrorReturn
                (NULL,
                 (ERROR_BADPARM,
                  "mri_read(): bad frame specification ('%s:')\n", pound));
            }

          start_frame = strtol(pound, &ep, 10);
          if(*ep != '\0')
            {
              errno = 0;
              ErrorReturn
                (NULL,
                 (ERROR_BADPARM,
                  "mri_read(): bad start frame ('%s')\n", pound));
            }

          end_frame = strtol(colon, &ep, 10);
          if(*ep != '\0')
            {
              errno = 0;
              ErrorReturn
                (NULL,
                 (ERROR_BADPARM,
                  "mri_read(): bad end frame ('%s')\n", colon));
            }

        }
      else
        {
          start_frame = end_frame = strtol(pound, &ep, 10);
          if(*ep != '\0')
            {
              errno = 0;
              ErrorReturn
                (NULL,
                 (ERROR_BADPARM,
                  "mri_read(): bad frame ('%s')\n", pound));
            }
        }

      if(start_frame < 0)
        {
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADPARM,
              "mri_read(): frame (%d) is less than zero\n", start_frame));
        }

      if(end_frame < 0)
        {
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADPARM,
              "mri_read(): frame (%d) is less than zero\n", end_frame));
        }

      if(start_frame > end_frame)
        {
          errno = 0;
          ErrorReturn(NULL, (ERROR_BADPARM,
                             "mri_read(): the start frame (%d) is "
                             "greater than the end frame (%d)\n",
                             start_frame, end_frame));
        }

    }

  if(type == MRI_CORONAL_SLICE_DIRECTORY)
    {
      mri = corRead(fname_copy, volume_flag);
    }
  else if(type == SIEMENS_FILE)
    {
      mri = siemensRead(fname_copy, volume_flag);
    }
  else if (type == MRI_GCA_FILE)
    {
      mri = readGCA(fname_copy) ;
    }
  else if(type == BHDR)
    {
      ptmpstr = bhdr_firstslicefname(fname_copy);
      t = bhdr_precision(fname_copy);
      mri = bvolumeRead(ptmpstr, volume_flag, t);
      free(ptmpstr);
    }
  else if(type == BSHORT_FILE)
    {
      //mri = bshortRead(fname_copy, volume_flag);
      mri = bvolumeRead(fname_copy, volume_flag, MRI_SHORT);
    }
  else if(type == BFLOAT_FILE)
    {
      //mri = bfloatRead(fname_copy, volume_flag);
      mri = bvolumeRead(fname_copy, volume_flag, MRI_FLOAT);
    }
  else if(type == GENESIS_FILE)
    {
      mri = genesisRead(fname_copy, volume_flag);
    }
  else if(type == SIGNA_FILE)
    {
      mri = signaRead(fname_copy, volume_flag);
    }
  else if(type == GE_LX_FILE)
    {
      mri = gelxRead(fname_copy, volume_flag);
    }
  else if(type == MRI_ANALYZE_FILE || type == MRI_ANALYZE4D_FILE)
    {
      mri = analyzeRead(fname_copy, volume_flag);
    }
  else if(type == BRIK_FILE)
    {
      mri = afniRead(fname_copy, volume_flag);
    }
  else if(type == MRI_MINC_FILE)
    {
      //mri = mincRead2(fname_copy, volume_flag);
      mri = mincRead(fname_copy, volume_flag);
    }
  else if(type == SDT_FILE)
    {
      mri = sdtRead(fname_copy, volume_flag);
    }
  else if(type == MRI_MGH_FILE)
    {
      mri = mghRead(fname_copy, volume_flag, -1);
    }
  else if(type == GDF_FILE)
    {
      mri = gdfRead(fname_copy, volume_flag);
    }
  else if(type == DICOM_FILE)
    {
      DICOMRead(fname_copy, &mri, volume_flag);
    }
  else if(type == SIEMENS_DICOM_FILE)
    {
      // mri_convert -nth option sets start_frame = nth.  otherwise -1
      mri = sdcmLoadVolume(fname_copy, volume_flag, start_frame);
      start_frame = -1;
      // in order to avoid the later processing on start_frame and end_frame
      // read the comment later on
      end_frame = 0;
    }
  else if (type == BRUKER_FILE)
    {
      mri = brukerRead(fname_copy, volume_flag);
    }
  else if(type == XIMG_FILE)
    {
      mri = ximgRead(fname_copy, volume_flag);
    }
  else if(type == NIFTI1_FILE)
    {
      mri = nifti1Read(fname_copy, volume_flag);
    }
  else if(type == NII_FILE)
    {
      mri = niiRead(fname_copy, volume_flag);
    }
  else if(type == NRRD_FILE)
    {
      mri = mriNrrdRead(fname_copy, volume_flag);
    }
  else if(type == MRI_CURV_FILE)
    mri = MRISreadCurvAsMRI(fname_copy, volume_flag);
  else if (type == IMAGE_FILE) {
    I = ImageRead(fname_copy);
    mri = ImageToMRI(I);
    ImageFree(&I);
  }
  else
    {
      fprintf(stderr,"mri_read(): type = %d\n",type);
      errno = 0;
      ErrorReturn(NULL, (ERROR_BADPARM, "mri_read(): code inconsistency "
                         "(file type recognized but not caught)"));
    }

  if (mri == NULL)  return(NULL) ;

  // update/cache the transform
  if (mri->i_to_r__)
    MatrixFree(&(mri->i_to_r__));
  mri->i_to_r__ = extract_i_to_r(mri);

  if (mri->r_to_i__)
    MatrixFree(&(mri->r_to_i__));
  mri->r_to_i__ = extract_r_to_i(mri);

  /* Compute the FOV from the vox2ras matrix (don't rely on what
     may or may not be in the file).*/

  if(start_frame == -1) return(mri);

  /* --- select frames --- */

  if(start_frame >= mri->nframes)
    {
      volume_frames = mri->nframes;
      MRIfree(&mri);
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADPARM,
          "mri_read(): start frame (%d) is "
          "out of range (%d frames in volume)",
          start_frame, volume_frames));
    }

  if(end_frame >= mri->nframes)
    {
      volume_frames = mri->nframes;
      MRIfree(&mri);
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADPARM,
          "mri_read(): end frame (%d) is out of range (%d frames in volume)",
          end_frame, volume_frames));
    }

  if(!volume_flag)
    {

      if(nan_inf_check(mri) != NO_ERROR)
        {
          MRIfree(&mri);
          return(NULL);
        }
      mri2 = MRIcopy(mri, NULL);
      MRIfree(&mri);
      mri2->nframes = (end_frame - start_frame + 1);
      return(mri2);

    }

  if(start_frame == 0 && end_frame == mri->nframes-1)
    {
      if(nan_inf_check(mri) != NO_ERROR)
        {
          MRIfree(&mri);
          return(NULL);
        }
      return(mri);
    }

  /* reading the whole volume and then copying only
     some frames is a bit inelegant,
     but it's easier for now
     to do this than to insert the appropriate
     code into the read function for each format... */
  /////////////////////////////////////////////////////////////////
  // This method does not work, because
  // 1. each frame has different acq parameters
  // 2. when making frames, it takes the info only from the first frame
  // Thus taking a frame out must be done at each frame extraction
  ////////////////////////////////////////////////////////////////////
  mri2 = MRIallocSequence(mri->width,
                          mri->height,
                          mri->depth,
                          mri->type,
                          (end_frame - start_frame + 1));
  MRIcopyHeader(mri, mri2);
  mri2->nframes = (end_frame - start_frame + 1);
  mri2->imnr0 = 1;
  mri2->imnr0 = mri2->nframes;

  if(mri2->type == MRI_UCHAR)
    for(t = 0;t < mri2->nframes;t++)
      for(i = 0;i < mri2->width;i++)
        for(j = 0;j < mri2->height;j++)
          for(k = 0;k < mri2->depth;k++)
            MRIseq_vox(mri2, i, j, k, t) =
              MRIseq_vox(mri, i, j, k, t + start_frame);

  if(mri2->type == MRI_SHORT)
    for(t = 0;t < mri2->nframes;t++)
      for(i = 0;i < mri2->width;i++)
        for(j = 0;j < mri2->height;j++)
          for(k = 0;k < mri2->depth;k++)
            MRISseq_vox(mri2, i, j, k, t) =
              MRISseq_vox(mri, i, j, k, t + start_frame);

  if(mri2->type == MRI_INT)
    for(t = 0;t < mri2->nframes;t++)
      for(i = 0;i < mri2->width;i++)
        for(j = 0;j < mri2->height;j++)
          for(k = 0;k < mri2->depth;k++)
            MRIIseq_vox(mri2, i, j, k, t) =
              MRIIseq_vox(mri, i, j, k, t + start_frame);

  if(mri2->type == MRI_LONG)
    for(t = 0;t < mri2->nframes;t++)
      for(i = 0;i < mri2->width;i++)
        for(j = 0;j < mri2->height;j++)
          for(k = 0;k < mri2->depth;k++)
            MRILseq_vox(mri2, i, j, k, t) =
              MRILseq_vox(mri, i, j, k, t + start_frame);

  if(mri2->type == MRI_FLOAT)
    for(t = 0;t < mri2->nframes;t++)
      for(i = 0;i < mri2->width;i++)
        for(j = 0;j < mri2->height;j++)
          for(k = 0;k < mri2->depth;k++)
            MRIFseq_vox(mri2, i, j, k, t) =
              MRIFseq_vox(mri, i, j, k, t + start_frame);

  if(nan_inf_check(mri) != NO_ERROR)
    {
      MRIfree(&mri);
      return(NULL);
    }

  MRIfree(&mri);

  return(mri2);

} /* end mri_read() */

static int nan_inf_check(MRI *mri)
{

  int i, j, k, t;

  if(mri->type != MRI_FLOAT)
    return(NO_ERROR);

  for(i = 0;i < mri->width;i++)
    for(j = 0;j < mri->height;j++)
      for(k = 0;k < mri->depth;k++)
        for(t = 0;t < mri->nframes;t++)
          if(!devFinite((MRIFseq_vox(mri, i, j, k, t))))
            {
              if(devIsinf((MRIFseq_vox(mri, i, j, k, t))) != 0)
                {
                  errno = 0;
                  ErrorReturn
                    (ERROR_BADPARM,
                     (ERROR_BADPARM,
                      "nan_inf_check(): Inf at voxel %d, %d, %d, %d",
                      i, j, k, t));
                }
              else if(devIsnan((MRIFseq_vox(mri, i, j, k, t))))
                {
                  errno = 0;
                  ErrorReturn
                    (ERROR_BADPARM,
                     (ERROR_BADPARM,
                      "nan_inf_check(): NaN at voxel %d, %d, %d, %d",
                      i, j, k, t));
                }
              else
                {
                  errno = 0;
                  ErrorReturn
                    (ERROR_BADPARM,
                     (ERROR_BADPARM,
                      "nan_inf_check(): bizarre value (not Inf, "
                      "not NaN, but not finite) at %d, %d, %d, %d",
                      i, j, k, t));
                }
            }

  return(NO_ERROR);

} /* end nan_inf_check() */

MRI *MRIreadType(char *fname, int type)
{

  MRI *mri;

  chklc();

  mri = mri_read(fname, type, TRUE, -1, -1);

  return(mri);

} /* end MRIreadType() */

MRI *MRIread(char *fname)
{
  char  buf[STRLEN] ;
  MRI *mri = NULL;

  chklc() ;

  FileNameFromWildcard(fname, buf) ; fname = buf ;
  mri = mri_read(fname, MRI_VOLUME_TYPE_UNKNOWN, TRUE, -1, -1);

  /* some volume format needs to read many
     different files for slices (GE DICOM or COR).
     we make sure that mri_read() read the slices, not just one   */
  if (mri==NULL)
    return NULL;

  MRIremoveNaNs(mri, mri) ;
  return(mri);

} /* end MRIread() */

// allow picking one frame out of many frame
// currently implemented only for Siemens dicom file
MRI *MRIreadEx(char *fname, int nthframe)
{
  char  buf[STRLEN] ;
  MRI *mri = NULL;

  chklc() ;

  FileNameFromWildcard(fname, buf) ; fname = buf ;
  mri = mri_read(fname, MRI_VOLUME_TYPE_UNKNOWN, TRUE, nthframe, nthframe);

  /* some volume format needs to read many
     different files for slices (GE DICOM or COR).
     we make sure that mri_read() read the slices, not just one   */
  if (mri==NULL)
    return NULL;

  MRIremoveNaNs(mri, mri) ;
  return(mri);

} /* end MRIread() */

MRI *MRIreadInfo(char *fname)
{

  MRI *mri = NULL;

  mri = mri_read(fname, MRI_VOLUME_TYPE_UNKNOWN, FALSE, -1, -1);

  return(mri);

} /* end MRIreadInfo() */

/*---------------------------------------------------------------
  MRIreadHeader() - reads the MRI header of the given file name.
  If type is MRI_VOLUME_TYPE_UNKNOWN, then the type will be
  inferred from the file name.
  ---------------------------------------------------------------*/
MRI *MRIreadHeader(char *fname, int type)
{
  int usetype;
  MRI *mri = NULL;
  char modFname[STRLEN];
  struct stat stat_buf;

  usetype = type;

  if (!strchr(fname,DIR_SEPARATOR))
    {
      char *cwd = getcwd(NULL, 0); // posix 1 extension
                                   // (allocate as much space needed)
      if (cwd)
        {
          strcpy(modFname, cwd);
          strcat(modFname, "/");
          strcat(modFname, fname);
          free(cwd);
        }
      else // why fail?
        strcpy(modFname, fname);
    }
  else
    strcpy(modFname, fname);

  if(usetype == MRI_VOLUME_TYPE_UNKNOWN){
    usetype = mri_identify(modFname);
    if(usetype == MRI_VOLUME_TYPE_UNKNOWN)
      {
        // just check again
        if (stat(fname, &stat_buf) < 0)
          printf("ERROR: cound not find volume %s.  Does it exist?\n", fname);
        else
          printf("ERROR: could not determine type of %s\n",fname);
        return(NULL);
      }
  }
  mri = mri_read(modFname, usetype, FALSE, -1, -1);

  return(mri);

} /* end MRIreadInfo() */

int MRIwriteType(MRI *mri, char *fname, int type)
{
  struct stat stat_buf;
  int error=0;
  char *fstem;
  char tmpstr[STRLEN];

  if(type == MRI_CORONAL_SLICE_DIRECTORY)
    {
      error = corWrite(mri, fname);
    }
  else
    {
      /* ----- all remaining types should write to
         a filename, not to within a directory,
         so check that it isn't an existing directory name we've been passed.
         failure to check will result in a segfault
         when attempting to write file data
         to a 'file' that is actually an existing directory ----- */
      if(stat(fname, &stat_buf) == 0){  // if can stat, then fname exists
        if(S_ISDIR(stat_buf.st_mode)){ // if is directory...
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "MRIwriteType(): %s is an existing directory. (type=%d)\n"
              "                A filename should be specified instead.",
              fname, type));
        }
      }
    }

  /* ----- continue file writing... ----- */

  if(type == MRI_MINC_FILE)
    {
      error = mincWrite(mri, fname);
    }
  else if(type == BHDR)
    {
      fstem = bhdr_stem(fname);
      if(mri->type == MRI_SHORT){
        sprintf(tmpstr,"%s_000.bshort",fstem);
        error = bvolumeWrite(mri, tmpstr, MRI_SHORT);
      }
      else{
        sprintf(tmpstr,"%s_000.bfloat",fstem);
        error = bvolumeWrite(mri, tmpstr, MRI_FLOAT);
      }
      free(fstem);
    }
  else if(type == BSHORT_FILE)
    {
      error = bvolumeWrite(mri, fname, MRI_SHORT);
    }
  else if(type == BFLOAT_FILE)
    {
      error = bvolumeWrite(mri, fname, MRI_FLOAT);
    }
  else if(type == MRI_ANALYZE_FILE)
    {
      error = analyzeWrite(mri, fname);
    }
  else if(type == MRI_ANALYZE4D_FILE)
    {
      error = analyzeWrite4D(mri, fname);
    }
  else if(type == BRIK_FILE)
    {
      error = afniWrite(mri, fname);
    }
  else if(type == MRI_MGH_FILE)
    {
      error = mghWrite(mri, fname, -1);
    }
  else if(type == GDF_FILE)
    {
      error = gdfWrite(mri, fname);
    }
  else if(type == NIFTI1_FILE)
    {
      error = nifti1Write(mri, fname);
    }
  else if(type == NII_FILE)
    {
      error = niiWrite(mri, fname);
    }
  else if(type == NRRD_FILE)
    {
      error = mriNrrdWrite(mri, fname);
    }
  else if(type == GENESIS_FILE)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "MRIwriteType(): writing of GENESIS file type not supported"));
    }
  else if(type == GE_LX_FILE)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "MRIwriteType(): writing of GE LX file type not supported"));
    }
  else if(type == SIEMENS_FILE)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "MRIwriteType(): writing of SIEMENS file type not supported"));
    }
  else if(type == SDT_FILE)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "MRIwriteType(): writing of SDT file type not supported"));
    }
  else if(type == OTL_FILE)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "MRIwriteType(): writing of OTL file type not supported"));
    }
  else if(type == RAW_FILE)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "MRIwriteType(): writing of RAW file type not supported"));
    }
  else if(type == SIGNA_FILE)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "MRIwriteType(): writing of SIGNA file type not supported"));
    }
  else if(type == DICOM_FILE)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "MRIwriteType(): writing of DICOM file type not supported"));
    }
  else if(type == SIEMENS_DICOM_FILE)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "MRIwriteType(): writing of SIEMENS DICOM file type not supported"));
    }
  else if(type == BRUKER_FILE)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "MRIwriteType(): writing of BRUKER file type not supported"));
    }
  else if(type == XIMG_FILE)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "MRIwriteType(): writing of XIMG file type not supported"));
    }
  else if(type == MRI_CORONAL_SLICE_DIRECTORY)
    {
      // already processed above
    }
  else
    {
      errno = 0;
      ErrorReturn(ERROR_BADPARM,
                  (ERROR_BADPARM,
                   "MRIwriteType(): code inconsistency "
                   "(file type recognized but not caught)"));
    }

  return(error);

} /* end MRIwriteType() */

int
MRIwriteFrame(MRI *mri, char *fname, int frame)
{
  MRI *mri_tmp ;

  mri_tmp =  MRIcopyFrame(mri, NULL, frame, 0) ;
  MRIwrite(mri_tmp, fname) ;
  MRIfree(&mri_tmp) ;
  return(NO_ERROR) ;
}

int MRIwrite(MRI *mri, char *fname)
{

  int int_type = -1;
  int error;

  chklc() ;
  if((int_type = mri_identify(fname)) < 0)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM, "unknown file type for file (%s)", fname));
    }

  error = MRIwriteType(mri, fname, int_type);

  return(error);

} /* end MRIwrite() */


/* ----- required header fields ----- */

#define COR_ALL_REQUIRED 0x00001fff

#define IMNR0_FLAG   0x00000001
#define IMNR1_FLAG   0x00000002
#define PTYPE_FLAG   0x00000004
#define X_FLAG       0x00000008
#define Y_FLAG       0x00000010
#define THICK_FLAG   0x00000020
#define PSIZ_FLAG    0x00000040
#define STRTX_FLAG   0x00000080
#define ENDX_FLAG    0x00000100
#define STRTY_FLAG   0x00000200
#define ENDY_FLAG    0x00000400
#define STRTZ_FLAG   0x00000800
#define ENDZ_FLAG    0x00001000

/* trivially time course clean */
static MRI *corRead(char *fname, int read_volume)
{

  MRI *mri;
  struct stat stat_buf;
  char fname_use[STRLEN];
  char *fbase;
  FILE *fp;
  int i, j;
  char line[STRLEN];
  int imnr0, imnr1, x, y, ptype;
  double fov, thick, psiz, locatn; /* using floats to read creates problems
                                      when checking values
                                      (e.g. thick = 0.00100000005) */
  float strtx, endx, strty, endy, strtz, endz;
  float tr, te, ti, flip_angle ;
  int ras_good_flag;
  float x_r, x_a, x_s;
  float y_r, y_a, y_s;
  float z_r, z_a, z_s;
  float c_r, c_a, c_s;
  char xform[STRLEN];
  long gotten;
  char xform_use[STRLEN];
  char *cur_char;
  char *last_slash;

  /* ----- check that it is a directory we've been passed ----- */
  strcpy(fname_use, fname);
  if(stat(fname_use, &stat_buf) < 0)
    {
      errno = 0;
      ErrorReturn(NULL,
                  (ERROR_BADFILE, "corRead(): can't stat %s", fname_use));
    }

  if(!S_ISDIR(stat_buf.st_mode))
    {
      /* remove the last bit and try again */
      cur_char = fname_use;
      last_slash = cur_char;
      while( *(cur_char+1) != '\0' )
        {
          if(*cur_char == '/')
            last_slash = cur_char;
          cur_char++;
        }
      *last_slash = '\0';
      if(stat(fname_use, &stat_buf) < 0)
        {
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADFILE, "corRead(): can't stat %s", fname_use));
        }
      if(!S_ISDIR(stat_buf.st_mode))
        {
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADFILE, "corRead(): %s isn't a directory", fname_use));
        }
    }

  /* ----- copy the directory name and remove any trailing '/' ----- */
  fbase = &fname_use[strlen(fname_use)];
  if(*(fbase-1) != '/')
    {
      *fbase = '/';
      fbase++;
    }

  /* ----- read the header file ----- */
  sprintf(fbase, "COR-.info");
  if((fp = fopen(fname_use, "r")) == NULL)
    {
      errno = 0;
      ErrorReturn
        (NULL, (ERROR_BADFILE, "corRead(): can't open file %s", fname_use));
    }

  /* ----- defaults (a good idea for non-required values...) ----- */
  xform[0] = '\0';
  ras_good_flag = 0;
  x_r = x_a = x_s = 0.0;
  y_r = y_a = y_s = 0.0;
  z_r = z_a = z_s = 0.0;
  c_r = c_a = c_s = 0.0;
  flip_angle = tr = te = ti = 0.0;
  fov = 0.0;
  locatn = 0.0;

  gotten = 0x00;

  while(fgets(line, STRLEN, fp) != NULL)
    {
      if(strncmp(line, "imnr0 ", 6) == 0)
        {
          sscanf(line, "%*s %d", &imnr0);
          gotten = gotten | IMNR0_FLAG;
        }
      else if(strncmp(line, "imnr1 ", 6) == 0)
        {
          sscanf(line, "%*s %d", &imnr1);
          gotten = gotten | IMNR1_FLAG;
        }
      else if(strncmp(line, "ptype ", 6) == 0)
        {
          sscanf(line, "%*s %d", &ptype);
          gotten = gotten | PTYPE_FLAG;
        }
      else if(strncmp(line, "x ", 2) == 0)
        {
          sscanf(line, "%*s %d", &x);
          gotten = gotten | X_FLAG;
        }
      else if(strncmp(line, "y ", 2) == 0)
        {
          sscanf(line, "%*s %d", &y);
          gotten = gotten | Y_FLAG;
        }
      else if(strncmp(line, "fov ", 4) == 0)
        {
          sscanf(line, "%*s %lf", &fov);
        }
      else if(strncmp(line, "thick ", 6) == 0)
        {
          sscanf(line, "%*s %lf", &thick);
          gotten = gotten | THICK_FLAG;
        }
      else if(strncmp(line, "flip ", 5) == 0)
        {
          sscanf(line+11, "%f", &flip_angle);
          flip_angle = RADIANS(flip_angle) ;
        }
      else if(strncmp(line, "psiz ", 5) == 0)
        {
          sscanf(line, "%*s %lf", &psiz);
          gotten = gotten | PSIZ_FLAG;
        }
      else if(strncmp(line, "locatn ", 7) == 0)
        {
          sscanf(line, "%*s %lf", &locatn);
        }
      else if(strncmp(line, "strtx ", 6) == 0)
        {
          sscanf(line, "%*s %f", &strtx);
          gotten = gotten | STRTX_FLAG;
        }
      else if(strncmp(line, "endx ", 5) == 0)
        {
          sscanf(line, "%*s %f", &endx);
          gotten = gotten | ENDX_FLAG;
        }
      else if(strncmp(line, "strty ", 6) == 0)
        {
          sscanf(line, "%*s %f", &strty);
          gotten = gotten | STRTY_FLAG;
        }
      else if(strncmp(line, "endy ", 5) == 0)
        {
          sscanf(line, "%*s %f", &endy);
          gotten = gotten | ENDY_FLAG;
        }
      else if(strncmp(line, "strtz ", 6) == 0)
        {
          sscanf(line, "%*s %f", &strtz);
          gotten = gotten | STRTZ_FLAG;
        }
      else if(strncmp(line, "endz ", 5) == 0)
        {
          sscanf(line, "%*s %f", &endz);
          gotten = gotten | ENDZ_FLAG;
        }
      else if(strncmp(line, "tr ", 3) == 0)
        {
          sscanf(line, "%*s %f", &tr);
        }
      else if(strncmp(line, "te ", 3) == 0)
        {
          sscanf(line, "%*s %f", &te);
        }
      else if(strncmp(line, "ti ", 3) == 0)
        {
          sscanf(line, "%*s %f", &ti);
        }
      else if(strncmp(line, "ras_good_flag ", 14) == 0)
        {
          sscanf(line, "%*s %d", &ras_good_flag);
        }
      else if(strncmp(line, "x_ras ", 6) == 0)
        {
          sscanf(line, "%*s %f %f %f", &x_r, &x_a, &x_s);
        }
      else if(strncmp(line, "y_ras ", 6) == 0)
        {
          sscanf(line, "%*s %f %f %f", &y_r, &y_a, &y_s);
        }
      else if(strncmp(line, "z_ras ", 6) == 0)
        {
          sscanf(line, "%*s %f %f %f", &z_r, &z_a, &z_s);
        }
      else if(strncmp(line, "c_ras ", 6) == 0)
        {
          sscanf(line, "%*s %f %f %f", &c_r, &c_a, &c_s);
        }
      else if(strncmp(line, "xform", 5) == 0 ||
              strncmp(line, "transform", 9) == 0)
        {
          sscanf(line, "%*s %s", xform);
        }
    }

  fclose(fp);

  /* ----- check for required fields ----- */
  if((gotten & COR_ALL_REQUIRED) != COR_ALL_REQUIRED)
    {
      errno = 0;
      ErrorPrintf(ERROR_BADFILE, "missing fields in file %s:", fname_use);
      if(!(gotten & IMNR0_FLAG))
        ErrorPrintf(ERROR_BADFILE, "  imnr0 field missing");
      if(!(gotten & IMNR1_FLAG))
        ErrorPrintf(ERROR_BADFILE, "  imnr1 field missing");
      if(!(gotten & PTYPE_FLAG))
        ErrorPrintf(ERROR_BADFILE, "  ptype field missing");
      if(!(gotten & X_FLAG))
        ErrorPrintf(ERROR_BADFILE, "  x field missing");
      if(!(gotten & Y_FLAG))
        ErrorPrintf(ERROR_BADFILE, "  y field missing");
      if(!(gotten & THICK_FLAG))
        ErrorPrintf(ERROR_BADFILE, "  thick field missing");
      if(!(gotten & PSIZ_FLAG))
        ErrorPrintf(ERROR_BADFILE, "  psiz field missing");
      if(!(gotten & STRTX_FLAG))
        ErrorPrintf(ERROR_BADFILE, "  strtx field missing");
      if(!(gotten & ENDX_FLAG))
        ErrorPrintf(ERROR_BADFILE, "  endx field missing");
      if(!(gotten & STRTY_FLAG))
        ErrorPrintf(ERROR_BADFILE, "  strty field missing");
      if(!(gotten & ENDY_FLAG))
        ErrorPrintf(ERROR_BADFILE, "  endy field missing");
      if(!(gotten & STRTZ_FLAG))
        ErrorPrintf(ERROR_BADFILE, "  strtz field missing");
      if(!(gotten & ENDZ_FLAG))
        ErrorPrintf(ERROR_BADFILE, "  endz field missing");
      return(NULL);
    }

  /* ----- check for required but forced (constant) values ----- */

  if(imnr0 != 1)
    {
      printf
        ("warning: non-standard value for imnr0 "
         "(%d, usually 1) in file %s\n",
         imnr0, fname_use);
    }

  if(imnr1 != 256)
    {
      printf
        ("warning: non-standard value for imnr1 "
         "(%d, usually 256) in file %s\n",
         imnr1, fname_use);
    }

  if(ptype != 2)
    {
      printf("warning: non-standard value for ptype "
             "(%d, usually 2) in file %s\n",
             ptype, fname_use);
    }

  if(x != 256)
    {
      printf("warning: non-standard value for x "
             "(%d, usually 256) in file %s\n",
             x, fname_use);
    }

  if(y != 256)
    {
      printf("warning: non-standard value for y "
             "(%d, usually 256) in file %s\n",
             y, fname_use);
    }

  if(thick != 0.001)
    {
      printf("warning: non-standard value for thick "
             "(%g, usually 0.001) in file %s\n",
             thick, fname_use);
    }

  if(psiz != 0.001)
    {
      printf("warning: non-standard value for psiz "
             "(%g, usually 0.001) in file %s\n",
             psiz, fname_use);
    }

  /* ----- copy header information to an mri structure ----- */

  if(read_volume)
    mri = MRIalloc(x, y, imnr1 - imnr0 + 1, MRI_UCHAR);
  else
    mri = MRIallocHeader(x, y, imnr1 - imnr0 + 1, MRI_UCHAR);

  /* hack */
  /*
    printf("%g, %g, %g\n", x_r, x_a, x_s);
    printf("%g, %g, %g\n", y_r, y_a, y_s);
    printf("%g, %g, %g\n", z_r, z_a, z_s);
  */
  if(x_r == 0.0 && x_a == 0.0 && x_s == 0.0 && y_r == 0.0 \
     && y_a == 0.0 && y_s == 0.0 && z_r == 0.0 && z_a == 0.0 && z_s == 0.0)
    {
      fprintf(stderr,
              "----------------------------------------------------\n"
              "Could not find the direction cosine information.\n"
              "Will use the CORONAL orientation.\n"
              "If not suitable, please provide the information "
              "in COR-.info file\n"
              "----------------------------------------------------\n"
              );
      x_r = -1.0;
      y_s = -1.0;
      z_a = 1.0;
      ras_good_flag = 0;
    }

  mri->imnr0 = imnr0;
  mri->imnr1 = imnr1;
  mri->fov = (float)(fov * 1000);
  mri->thick = (float)(thick * 1000);
  mri->ps = (float)(psiz * 1000);
  mri->xsize = mri->ps;
  mri->ysize = mri->ps;
  mri->zsize = (float)(mri->thick);
  mri->xstart = strtx * 1000;
  mri->xend = endx * 1000;
  mri->ystart = strty * 1000;
  mri->yend = endy * 1000;
  mri->zstart = strtz * 1000;
  mri->zend = endz * 1000;
  strcpy(mri->fname, fname);
  mri->tr = tr;
  mri->te = te;
  mri->ti = ti;
  mri->flip_angle = flip_angle ;
  mri->ras_good_flag = ras_good_flag;
  mri->x_r = x_r;  mri->x_a = x_a;  mri->x_s = x_s;
  mri->y_r = y_r;  mri->y_a = y_a;  mri->y_s = y_s;
  mri->z_r = z_r;  mri->z_a = z_a;  mri->z_s = z_s;
  mri->c_r = c_r;  mri->c_a = c_a;  mri->c_s = c_s;

  if(strlen(xform) > 0)
    {

      if(xform[0] == '/')
        strcpy(mri->transform_fname, xform);
      else
        sprintf(mri->transform_fname, "%s/%s", fname, xform);

      strcpy(xform_use, mri->transform_fname);

      if(!FileExists(xform_use))
        {

          sprintf(xform_use, "%s/../transforms/talairach.xfm", fname);

          if(!FileExists(xform_use))
            printf("can't find talairach file '%s'\n", xform_use);

        }

      if(FileExists(xform_use))
        {
          if(input_transform_file(xform_use, &(mri->transform)) == NO_ERROR)
            {
              mri->linear_transform =
                get_linear_transform_ptr(&mri->transform);
              mri->inverse_linear_transform =
                get_inverse_linear_transform_ptr(&mri->transform);
              mri->free_transform = 1;
              strcpy(mri->transform_fname, xform_use);
              if (DIAG_VERBOSE_ON)
                fprintf
                  (stderr,
                   "INFO: loaded talairach xform : %s\n",
                   mri->transform_fname);
            }
          else
            {
              errno = 0;
              ErrorPrintf
                (ERROR_BAD_FILE, "error loading transform from %s", xform_use);
              mri->linear_transform = NULL;
              mri->inverse_linear_transform = NULL;
              mri->free_transform = 1;
              (mri->transform_fname)[0] = '\0';
            }
        }

    }

  if(!read_volume)
    return(mri);

  /* ----- read the data files ----- */
  for(i = mri->imnr0;i <= imnr1;i++)
    {
      sprintf(fbase, "COR-%03d", i);
      if((fp = fopen(fname_use, "r")) == NULL)
        {
          MRIfree(&mri);
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADFILE, "corRead(): can't open file %s", fname_use));
        }
      for(j = 0;j < mri->height;j++)
        {
          if(fread(mri->slices[i-mri->imnr0][j], 1, mri->width, fp) <
             mri->width)
            {
              MRIfree(&mri);
              errno = 0;
              ErrorReturn
                (NULL,
                 (ERROR_BADFILE,
                  "corRead(): error reading from file %s", fname_use));
            }
        }
      fclose(fp);
    }

  return(mri);

} /* end corRead() */

static int corWrite(MRI *mri, char *fname)
{

  struct stat stat_buf;
  char fname_use[STRLEN];
  char *fbase;
  FILE *fp;
  int i, j;
  int rv;

  /* ----- check the mri structure for COR file compliance ----- */

  if(mri->slices == NULL)
    {
      errno = 0;
      ErrorReturn(ERROR_BADPARM, (ERROR_BADPARM,
                                  "corWrite(): mri structure to be "
                                  "written contains no voxel data"));
    }

  if(mri->imnr0 != 1)
    {
      printf
        ("non-standard value for imnr0 (%d, usually 1) in volume structure\n",
         mri->imnr0);
    }

  if(mri->imnr1 != 256)
    {
      printf
        ("non-standard value for imnr1 (%d, "
         "usually 256) in volume structure\n",
         mri->imnr1);
    }

  if(mri->type != MRI_UCHAR)
    {
      printf("non-standard value for type "
             "(%d, usually %d) in volume structure\n",
             mri->type, MRI_UCHAR);
    }

  if(mri->width != 256)
    {
      printf("non-standard value for width "
             "(%d, usually 256) in volume structure\n",
             mri->width);
    }

  if(mri->height != 256)
    {
      printf("non-standard value for height "
             "(%d, usually 256) in volume structure\n",
             mri->height);
    }

  if(mri->thick != 1)
    {
      printf("non-standard value for thick "
             "(%g, usually 1) in volume structure\n",
             mri->thick);
    }

  if(mri->ps != 1){
    printf("non-standard value for ps "
           "(%g, usually 1) in volume structure\n",
           mri->ps);
  }

  /* ----- copy the directory name and remove any trailing '/' ----- */
  strcpy(fname_use, fname);
  fbase = &fname_use[strlen(fname_use)];
  if(*(fbase-1) != '/')
    {
      *fbase = '/';
      fbase++;
      *fbase = '\0';
    }

  /* Create the directory */
  rv = mkdir(fname_use,(mode_t)-1);
  if(rv != 0 && errno != EEXIST){
    printf("ERROR: creating directory %s\n",fname_use);
    perror(NULL);
    return(1);
  }

  /* ----- check that it is a directory we've been passed ----- */
  if(stat(fname, &stat_buf) < 0){
    errno = 0;
    ErrorReturn
      (ERROR_BADFILE, (ERROR_BADFILE, "corWrite(): can't stat %s", fname));
  }

  if(!S_ISDIR(stat_buf.st_mode)){
    errno = 0;
    ErrorReturn
      (ERROR_BADFILE,
       (ERROR_BADFILE, "corWrite(): %s isn't a directory", fname));
  }

  sprintf(fbase, "COR-.info");
  if((fp = fopen(fname_use, "w")) == NULL)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADFILE,
         (ERROR_BADFILE,
          "corWrite(): can't open file %s for writing", fname_use));
    }

  fprintf(fp, "imnr0 %d\n", mri->imnr0);
  fprintf(fp, "imnr1 %d\n", mri->imnr1);
  fprintf(fp, "ptype %d\n", 2);
  fprintf(fp, "x %d\n", mri->width);
  fprintf(fp, "y %d\n", mri->height);
  fprintf(fp, "fov %g\n", mri->fov / 1000.0);
  fprintf(fp, "thick %g\n", mri->zsize / 1000.0); /* was xsize 11/1/01 */
  fprintf(fp, "psiz %g\n", mri->xsize / 1000.0);  /* was zsize 11/1/01 */
  fprintf(fp, "locatn %g\n", 0.0);
  fprintf(fp, "strtx %g\n", mri->xstart / 1000.0);
  fprintf(fp, "endx %g\n", mri->xend / 1000.0);
  fprintf(fp, "strty %g\n", mri->ystart / 1000.0);
  fprintf(fp, "endy %g\n", mri->yend / 1000.0);
  fprintf(fp, "strtz %g\n", mri->zstart / 1000.0);
  fprintf(fp, "endz %g\n", mri->zend / 1000.0);
  fprintf(fp, "tr %f\n", mri->tr);
  fprintf(fp, "te %f\n", mri->te);
  fprintf(fp, "ti %f\n", mri->ti);
  fprintf(fp, "flip angle %f\n", DEGREES(mri->flip_angle));
  fprintf(fp, "ras_good_flag %d\n", mri->ras_good_flag);
  fprintf(fp, "x_ras %f %f %f\n", mri->x_r, mri->x_a, mri->x_s);
  fprintf(fp, "y_ras %f %f %f\n", mri->y_r, mri->y_a, mri->y_s);
  fprintf(fp, "z_ras %f %f %f\n", mri->z_r, mri->z_a, mri->z_s);
  fprintf(fp, "c_ras %f %f %f\n", mri->c_r, mri->c_a, mri->c_s);
  if(strlen(mri->transform_fname) > 0)
    fprintf(fp, "xform %s\n", mri->transform_fname);

  fclose(fp);

  for(i = mri->imnr0;i <= mri->imnr1;i++)
    {
      sprintf(fbase, "COR-%03d", i);
      if((fp = fopen(fname_use, "w")) == NULL)
        {
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "corWrite(): can't open file %s for writing", fname_use));
        }
      for(j = 0;j < mri->height;j++)
        {
          if(fwrite(mri->slices[i-mri->imnr0][j], 1, mri->width, fp) <
             mri->width)
            {
              fclose(fp);
              errno = 0;
              ErrorReturn
                (ERROR_BADFILE,
                 (ERROR_BADFILE,
                  "corWrite(): error writing to file %s ", fname_use));
            }
        }
      fclose(fp);
    }

  return(NO_ERROR);

} /* end corWrite() */

static MRI *siemensRead(char *fname, int read_volume_flag)
{
  int file_n, n_low, n_high;
  char fname_use[STRLEN];
  MRI *mri;
  FILE *fp;
  char *c, *c2;
  short rows, cols;
  int n_slices;
  double d, d2;
  double im_c_r, im_c_a, im_c_s;
  int i, j;
  int n_files, base_raw_matrix_size, number_of_averages;
  int mosaic_size;
  int mosaic;
  int mos_r, mos_c;
  char pulse_sequence_name[STRLEN], ps2[STRLEN];
  int n_t;
  int n_dangling_images, n_full_mosaics, mosaics_per_volume;
  int br, bc;
  MRI *mri_raw;
  int t, s;
  int slice_in_mosaic;
  int file;
  char ima[4];
  IMAFILEINFO *ifi;

  /* ----- stop compiler complaints ----- */
  mri = NULL;
  mosaic_size = 0;

  ifi = imaLoadFileInfo(fname);
  if(ifi == NULL){
    printf("ERROR: siemensRead(): %s\n",fname);
    return(NULL);
  }

  strcpy(fname_use, fname);

  /* ----- point to ".ima" ----- */
  c = fname_use + (strlen(fname_use) - 4);

  if(c < fname_use)
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADPARM,
          "siemensRead(): bad file name %s (must end in '.ima' or '.IMA')",
          fname_use));
    }
  if(strcmp(".ima", c) == 0)
    sprintf(ima, "ima");
  else if(strcmp(".IMA", c) == 0)
    sprintf(ima, "IMA");
  else
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADPARM,
          "siemensRead(): bad file name %s (must end in '.ima' or '.IMA')",
          fname_use));
    }

  c2 = c;
  for(c--;isdigit((int)(*c));c--);
  c++;

  /* ----- c now points to the first digit in the last number set
     (e.g. to the "5" in 123-4-567.ima) ----- */

  /* ----- count down and up from this file --
     max and min image number within the series ----- */
  *c2 = '\0';
  file_n = atol(c);
  *c2 = '.';

  if(!FileExists(fname_use))
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE, "siemensRead(): file %s doesn't exist", fname_use));
    }

  /* --- get the low image number --- */
  n_low = file_n - 1;
  sprintf(c, "%d.%s", n_low, ima);
  while(FileExists(fname_use))
    {
      n_low--;
      sprintf(c, "%d.%s", n_low, ima);
    }
  n_low++;

  /* --- get the high image number --- */
  n_high = file_n + 1;
  sprintf(c, "%d.%s", n_high, ima);
  while(FileExists(fname_use))
    {
      n_high++;
      sprintf(c, "%d.%s", n_high, ima);
    }
  n_high--;

  n_files = n_high - n_low + 1;

  sprintf(c, "%d.%s", n_low, ima);
  if((fp = fopen(fname_use, "r")) == NULL)
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE,
          "siemensRead(): can't open file %s (low = %d, this = %d, high = %d)",
          fname_use, n_low, file_n, n_high));
    }

  fseek(fp, 4994, SEEK_SET);
  fread(&rows, 2, 1, fp);
  rows = orderShortBytes(rows);
  fseek(fp, 4996, SEEK_SET);
  fread(&cols, 2, 1, fp);
  cols = orderShortBytes(cols);
  fseek(fp, 4004, SEEK_SET);
  fread(&n_slices, 4, 1, fp);
  n_slices = orderIntBytes(n_slices);
  if (n_slices==0)
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE,
          "\n\nPlease try with the option '-it siemens_dicom'.\n "
          "The handling failed assuming the old siemens format.\n"))
        }
  fseek(fp, 2864, SEEK_SET);
  fread(&base_raw_matrix_size, 4, 1, fp);
  base_raw_matrix_size = orderIntBytes(base_raw_matrix_size);
  fseek(fp, 1584, SEEK_SET);
  fread(&number_of_averages, 4, 1, fp);
  number_of_averages = orderIntBytes(number_of_averages);
  memset(pulse_sequence_name, 0x00, STRLEN);
  fseek(fp, 3009, SEEK_SET);
  fread(&pulse_sequence_name, 1, 65, fp);

  /* --- scout --- */
  strcpy(ps2, pulse_sequence_name);
  StrLower(ps2);
  if(strstr(ps2, "scout") != NULL)
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADPARM,
          "siemensRead(): series appears to be a scout "
          "(sequence file name is %s)", pulse_sequence_name));
    }

  /* --- structural --- */
  if(n_slices == 1 && ! ifi->IsMosaic)
    {
      n_slices = n_files;
      n_t = 1;
      if(base_raw_matrix_size != rows || base_raw_matrix_size != cols)
        {
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADPARM, "siemensRead(): bad file/base matrix sizes"));
        }
      mos_r = mos_c = 1;
      mosaic_size = 1;
    }
  else
    {

      if(rows % base_raw_matrix_size != 0)
        {
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADPARM, "siemensRead(): file rows (%hd) not"
              " divisible by image rows (%d)", rows, base_raw_matrix_size));
        }
      if(cols % base_raw_matrix_size != 0)
        {
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADPARM,
              "siemensRead(): file cols (%hd)"
              " not divisible by image cols (%d)",
              cols, base_raw_matrix_size));
        }

      mos_r = rows / base_raw_matrix_size;
      mos_c = cols / base_raw_matrix_size;
      mosaic_size = mos_r * mos_c;
      if (mosaic_size == 0)
        {
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADPARM,
              "siemensRead(): mosaic_size==0, "
              "Try '-it siemens_dicom' flag with mri_convert"));
        }

      n_dangling_images = n_slices % mosaic_size;
      n_full_mosaics = (n_slices - n_dangling_images) / mosaic_size;

      mosaics_per_volume = n_full_mosaics + (n_dangling_images == 0 ? 0 : 1);

      if(n_files % mosaics_per_volume != 0)
        {
          errno = 0;
          ErrorReturn(NULL, (ERROR_BADPARM, "siemensRead(): files in volume "
                             "(%d) not divisible by mosaics per volume (%d)",
                             n_files, mosaics_per_volume));
        }

      n_t = n_files / mosaics_per_volume;

    }

  if(read_volume_flag)
    mri = MRIallocSequence
      (base_raw_matrix_size, base_raw_matrix_size, n_slices, MRI_SHORT, n_t);
  else
    {
      mri = MRIallocHeader
        (base_raw_matrix_size, base_raw_matrix_size, n_slices, MRI_SHORT);
      mri->nframes = n_t;
    }

  /* --- pixel sizes --- */
  /* --- mos_r and mos_c factors are strange, but they're there... --- */
  fseek(fp, 5000, SEEK_SET);
  fread(&d, 8, 1, fp);
  mri->xsize = mos_r * orderDoubleBytes(d);
  fseek(fp, 5008, SEEK_SET);
  fread(&d, 8, 1, fp);
  mri->ysize = mos_c * orderDoubleBytes(d);

  /* --- slice distance factor --- */
  fseek(fp, 4136, SEEK_SET);
  fread(&d, 8, 1, fp);
  d = orderDoubleBytes(d);
  if(d == -19222) /* undefined (Siemens code) -- I assume this to mean 0 */
    d = 0.0;
  /* --- slice thickness --- */
  fseek(fp, 1544, SEEK_SET);
  fread(&d2, 8, 1, fp);
  d2 = orderDoubleBytes(d2);
  /* --- distance between slices --- */
  mri->zsize = (1.0 + d) * d2;

  /* --- field of view (larger of height, width fov) --- */
  fseek(fp, 3744, SEEK_SET);
  fread(&d, 8, 1, fp);
  d = orderDoubleBytes(d);
  fseek(fp, 3752, SEEK_SET);
  fread(&d2, 8, 1, fp);
  d2 = orderDoubleBytes(d);
  mri->fov = (d > d2 ? d : d2);

  mri->thick = mri->zsize;
  mri->ps = mri->xsize;

  strcpy(mri->fname, fname);

  mri->location = 0.0;

  fseek(fp, 2112, SEEK_SET) ;
  mri->flip_angle = RADIANS(freadDouble(fp)) ;  /* was in degrees */

  fseek(fp, 1560, SEEK_SET);
  fread(&d, 8, 1, fp);
  mri->tr = orderDoubleBytes(d);
  fseek(fp, 1568, SEEK_SET);
  fread(&d, 8, 1, fp);
  mri->te = orderDoubleBytes(d);
  fseek(fp, 1576, SEEK_SET);
  fread(&d, 8, 1, fp);
  mri->ti = orderDoubleBytes(d);

  fseek(fp, 3792, SEEK_SET);
  fread(&d, 8, 1, fp);  mri->z_r = -orderDoubleBytes(d);
  fread(&d, 8, 1, fp);  mri->z_a =  orderDoubleBytes(d);
  fread(&d, 8, 1, fp);  mri->z_s = -orderDoubleBytes(d);

  fseek(fp, 3832, SEEK_SET);
  fread(&d, 8, 1, fp);  mri->x_r = -orderDoubleBytes(d);
  fread(&d, 8, 1, fp);  mri->x_a =  orderDoubleBytes(d);
  fread(&d, 8, 1, fp);  mri->x_s = -orderDoubleBytes(d);

  fseek(fp, 3856, SEEK_SET);
  fread(&d, 8, 1, fp);  mri->y_r = -orderDoubleBytes(d);
  fread(&d, 8, 1, fp);  mri->y_a =  orderDoubleBytes(d);
  fread(&d, 8, 1, fp);  mri->y_s = -orderDoubleBytes(d);

  fseek(fp, 3768, SEEK_SET);
  fread(&im_c_r, 8, 1, fp);  im_c_r = -orderDoubleBytes(im_c_r);
  fread(&im_c_a, 8, 1, fp);  im_c_a =  orderDoubleBytes(im_c_a);
  fread(&im_c_s, 8, 1, fp);  im_c_s = -orderDoubleBytes(im_c_s);

  mri->c_r = im_c_r - (mosaic_size - 1) * mri->z_r * mri->zsize + \
    ((mri->depth) / 2.0) * mri->z_r * mri->zsize;
  mri->c_a = im_c_a - (mosaic_size - 1) * mri->z_a * mri->zsize + \
    ((mri->depth) / 2.0) * mri->z_a * mri->zsize;
  mri->c_s = im_c_s - (mosaic_size - 1) * mri->z_s * mri->zsize + \
    ((mri->depth) / 2.0) * mri->z_s * mri->zsize;

  mri->ras_good_flag = 1;

  fseek(fp, 3760, SEEK_SET);
  fread(&i, 4, 1, fp);
  i = orderIntBytes(i);

#if 0
  if(i == 1 || i == 2)
    mri->slice_direction = MRI_HORIZONTAL;
  else if(i == 3 || i == 5)
    mri->slice_direction = MRI_CORONAL;
  else if(i == 4 || i == 6)
    mri->slice_direction = MRI_SAGITTAL;
  else
#endif
    if (i < 1 || i > 6)
      {
        errno = 0;
        ErrorReturn
          (NULL,
           (ERROR_BADFILE,
            "siemensRead(): bad slice direction (%d) in file %s",
            i, fname_use));
      }

  mri->xstart = -mri->width * mri->xsize / 2.0;
  mri->xend = -mri->xstart;
  mri->ystart = -mri->height * mri->ysize / 2.0;
  mri->yend = -mri->ystart;
  mri->zstart = -mri->depth * mri->zsize / 2.0;
  mri->zend = -mri->zstart;

  /*
    printf("%d, %d; %d, %hd, %hd; %d\n", n_files, number_of_averages,
    base_raw_matrix_size, rows, cols,
    slices);
  */
  /*
    rows, cols, brms, mosaic i, j, mosaic size, n slices, n files, n_t
  */
  fclose(fp);

  mri->imnr0 = 1;
  mri->imnr1 = mri->depth;
  /*
    printf("%d, %d, %d, %d\n",
    mri->width, mri->height, mri->depth, mri->nframes);
  */
  if(read_volume_flag)
    {

      mri_raw = MRIalloc(rows, cols, n_files, MRI_SHORT);

      for(file_n = n_low;file_n <= n_high;file_n++)
        {

          sprintf(c, "%d.%s", file_n, ima);
          if((fp = fopen(fname_use, "r")) == NULL)
            {
              MRIfree(&mri);
              errno = 0;
              ErrorReturn(NULL, (ERROR_BADFILE,
                                 "siemensRead(): can't open file %s "
                                 "(low = %d, this = %d, high = %d)",
                                 fname_use, n_low, file_n, n_high));
            }

          fseek(fp, 6144, SEEK_SET);

          for(i = 0;i < rows;i++)
            {
              fread(&MRISvox(mri_raw, 0, i, file_n - n_low),
                    sizeof(short), cols, fp);
#if (BYTE_ORDER == LITTLE_ENDIAN)
              swab(&MRISvox(mri_raw, 0, i, file_n - n_low), \
                   &MRISvox(mri_raw, 0, i, file_n - n_low),
                   sizeof(short) * cols);
#endif
            }

          fclose(fp);

        }

      for(t = 0;t < mri->nframes;t++)
        {
          for(s = 0;s < mri->depth;s++)
            {
              slice_in_mosaic = s % mosaic_size;
              mosaic = (s - slice_in_mosaic) / mosaic_size;
              file = mri->nframes * mosaic + t;
              /*
                printf("s, t = %d, %d; f, sm = %d, %d\n",
                s, t, file, slice_in_mosaic);
              */
              bc = slice_in_mosaic % mos_r;
              br = (slice_in_mosaic - bc) / mos_r;

              for(i = 0;i < mri->width;i++)
                {
                  for(j = 0;j < mri->height;j++)
                    {
                      MRISseq_vox(mri, i, j, s, t) = \
                        MRISvox
                        (mri_raw, mri->width *
                         bc + i, mri->height * br + j, file);
                    }
                }

            }
        }

      MRIfree(&mri_raw);

    }

  return(mri);

} /* end siemensRead() */
/*-----------------------------------------------------------*/
static MRI *mincRead(char *fname, int read_volume)
{
  // Real wx, wy, wz;
  MRI *mri;
  Volume vol;
  VIO_Status status;
  char *dim_names[4];
  int dim_sizes[4];
  int ndims;
  int dtype;
  volume_input_struct input_info;
  Real separations[4];
  Real voxel[4];
  Real worldr, worlda, worlds;
  Real val;
  int i, j, k, t;
  float xfov, yfov, zfov;
  Real f;
  BOOLEAN sflag = TRUE ;
  Transform *pVox2WorldLin;
  General_transform *pVox2WorldGen;

  /* ----- read the volume ----- */

  /*   we no longer much around axes and thus */
  /*   it is safe to read in the standard way    */
  dim_names[0] = MIxspace;
  dim_names[1] = MIyspace;
  dim_names[2] = MIzspace;
  dim_names[3] = MItime;

#if 0
  dim_names[0] = MIzspace;
  dim_names[1] = MIyspace;
  dim_names[2] = MIxspace;
  dim_names[3] = MItime;
#endif

  if(!FileExists(fname))
    {
      errno = 0;
      ErrorReturn(NULL, (ERROR_BADFILE,
                         "mincRead(): can't find file %s", fname));
    }

  status = start_volume_input(fname, 0, dim_names, NC_UNSPECIFIED, 0, 0, 0,
                              TRUE, &vol, NULL, &input_info);

  if (Gdiag & DIAG_VERBOSE_ON && DIAG_SHOW){
    printf("status = %d\n", status);
    printf("n_dimensions = %d\n", get_volume_n_dimensions(vol));
    printf("nc_data_type = %d\n", vol->nc_data_type);
  }

  if(status != OK){
    errno = 0;
    ErrorReturn(NULL, (ERROR_BADPARM, "mincRead(): error reading "
                       "volume from file %s", fname));
  }

  /* ----- check the number of dimensions ----- */
  ndims = get_volume_n_dimensions(vol);
  if(ndims != 3 && ndims != 4){
    errno = 0;
    ErrorReturn(NULL, (ERROR_BADPARM, "mincRead(): %d dimensions "
                       "in file; expecting 3 or 4", ndims));
  }

  /* ----- get the dimension sizes ----- */
  get_volume_sizes(vol, dim_sizes);

  /* --- one time point if there are only three dimensions in the file --- */
  if(ndims == 3) dim_sizes[3] = 1;

  if ((Gdiag & DIAG_SHOW) && DIAG_VERBOSE_ON)
    printf("DimSizes: %d, %d, %d, %d, %d\n", ndims, dim_sizes[0],
           dim_sizes[1], dim_sizes[2], dim_sizes[3]);
  if ((Gdiag & DIAG_SHOW) && DIAG_VERBOSE_ON)
    printf("DataType: %d\n", vol->nc_data_type);

  dtype = get_volume_nc_data_type(vol, &sflag) ;
  dtype = orderIntBytes(vol->nc_data_type) ;

  /* ----- get the data type ----- */
  if(vol->nc_data_type == NC_BYTE || vol->nc_data_type == NC_CHAR)
    dtype = MRI_UCHAR;
  else if(vol->nc_data_type == NC_SHORT)
    dtype = MRI_SHORT;
  else if(vol->nc_data_type == NC_LONG)
    dtype = MRI_LONG;
  else if(vol->nc_data_type == NC_FLOAT || vol->nc_data_type == NC_DOUBLE)
    dtype = MRI_FLOAT;
  else
    {
      errno = 0;
      ErrorReturn(NULL, (ERROR_BADPARM, "mincRead(): bad data type "
                         "(%d) in input file %s", vol->nc_data_type, fname));
    }

  /* ----- allocate the mri structure ----- */
  if(read_volume)
    mri = MRIallocSequence(dim_sizes[0], dim_sizes[1], dim_sizes[2],
                           dtype, dim_sizes[3]);
  else{
    mri = MRIallocHeader(dim_sizes[0], dim_sizes[1], dim_sizes[2], dtype);
    mri->nframes = dim_sizes[3];
  }

  /* ----- set up the mri structure ----- */
  get_volume_separations(vol, separations);
  mri->xsize = fabs(separations[0]);
  mri->ysize = fabs(separations[1]);
  mri->zsize = fabs(separations[2]);
  mri->ps    = mri->xsize;
  mri->thick = mri->zsize;

  mri->x_r = vol->direction_cosines[0][0];
  mri->x_a = vol->direction_cosines[0][1];
  mri->x_s = vol->direction_cosines[0][2];

  mri->y_r = vol->direction_cosines[1][0];
  mri->y_a = vol->direction_cosines[1][1];
  mri->y_s = vol->direction_cosines[1][2];

  mri->z_r = vol->direction_cosines[2][0];
  mri->z_a = vol->direction_cosines[2][1];
  mri->z_s = vol->direction_cosines[2][2];

  if(separations[0] < 0){
    mri->x_r = -mri->x_r;  mri->x_a = -mri->x_a;  mri->x_s = -mri->x_s;
  }
  if(separations[1] < 0){
    mri->y_r = -mri->y_r;  mri->y_a = -mri->y_a;  mri->y_s = -mri->y_s;
  }
  if(separations[2] < 0){
    mri->z_r = -mri->z_r;  mri->z_a = -mri->z_a;  mri->z_s = -mri->z_s;
  }

  voxel[0] = (mri->width) / 2.0;
  voxel[1] = (mri->height) / 2.0;
  voxel[2] = (mri->depth) / 2.0;
  voxel[3] = 0.0;
  convert_voxel_to_world(vol, voxel, &worldr, &worlda, &worlds);
  mri->c_r = worldr;
  mri->c_a = worlda;
  mri->c_s = worlds;

  mri->ras_good_flag = 1;

  mri->xend = mri->xsize * mri->width / 2.0;   mri->xstart = -mri->xend;
  mri->yend = mri->ysize * mri->height / 2.0;  mri->ystart = -mri->yend;
  mri->zend = mri->zsize * mri->depth / 2.0;   mri->zstart = -mri->zend;

  xfov = mri->xend - mri->xstart;
  yfov = mri->yend - mri->ystart;
  zfov = mri->zend - mri->zstart;

  mri->fov = ( xfov > yfov ? (xfov > zfov ? xfov : zfov ) :
               (yfov > zfov ? yfov : zfov ) );

  strcpy(mri->fname, fname);

  //////////////////////////////////////////////////////////////////////////
  // test transform their way and our way:
  // MRIvoxelToWorld(mri, voxel[0], voxel[1], voxel[2], &wx, &wy, &wz);
  // printf("MNI calculated %.2f, %.2f, %.2f
  //     vs. MRIvoxelToWorld: %.2f, %.2f, %.2f\n",
  //     worldr, worlda, worlds, wx, wy, wz);

  /* ----- copy the data from the file to the mri structure ----- */
  if(read_volume){

    while(input_more_of_volume(vol, &input_info, &f));

    for(i = 0;i < mri->width;i++) {
      for(j = 0;j < mri->height;j++) {
        for(k = 0;k < mri->depth;k++) {
          for(t = 0;t < mri->nframes;t++) {
            val = get_volume_voxel_value(vol, i, j, k, t, 0);
            if(mri->type == MRI_UCHAR)
              MRIseq_vox(mri, i, j, k, t) = (unsigned char)val;
            if(mri->type == MRI_SHORT)
              MRISseq_vox(mri, i, j, k, t) = (short)val;
            if(mri->type == MRI_LONG)
              MRILseq_vox(mri, i, j, k, t) = (long)val;
            if(mri->type == MRI_FLOAT)
              MRIFseq_vox(mri, i, j, k, t) = (float)val;
          }
        }
      }
    }
  }

  pVox2WorldGen = get_voxel_to_world_transform(vol);
  pVox2WorldLin = get_linear_transform_ptr(pVox2WorldGen);
  if ((Gdiag & DIAG_SHOW) && DIAG_VERBOSE_ON)
    {
      printf("MINC Linear Transform\n");
      for(i=0;i<4;i++){
        for(j=0;j<4;j++) printf("%7.4f ",pVox2WorldLin->m[j][i]);
        printf("\n");
      }
    }

  delete_volume_input(&input_info);
  delete_volume(vol);


  return(mri);

} /* end mincRead() */
#if 0
/*-----------------------------------------------------------*/
static MRI *mincRead2(char *fname, int read_volume)
{

  MRI *mri;
  Volume vol;
  VIO_Status status;
  char *dim_names[4];
  int dim_sizes[4];
  int ndims;
  int dtype;
  volume_input_struct input_info;
  Real separations[4];
  Real voxel[4];
  Real worldr, worlda, worlds;
  Real val;
  int i, j, k, t;
  float xfov, yfov, zfov;
  Real f;
  BOOLEAN sflag = TRUE ;
  MATRIX *T;
  Transform *pVox2WorldLin;
  General_transform *pVox2WorldGen;

  /* Make sure file exists */
  if(!FileExists(fname)){
    errno = 0;
    ErrorReturn(NULL, (ERROR_BADFILE,
                       "mincRead(): can't find file %s", fname));
  }

  /* Specify read-in order */
  dim_names[0] = MIxspace; /* cols */
  dim_names[1] = MIzspace; /* rows */
  dim_names[2] = MIyspace; /* slices */
  dim_names[3] = MItime;   /* time */

  /* Read the header info into vol. input_info needed for further reads */
  status = start_volume_input(fname, 0, dim_names, NC_UNSPECIFIED, 0, 0, 0,
                              TRUE, &vol, NULL, &input_info);

  if (Gdiag & DIAG_VERBOSE_ON && DIAG_SHOW){
    printf("status = %d\n", status);
    printf("n_dimensions = %d\n", get_volume_n_dimensions(vol));
    printf("nc_data_type = %d\n", vol->nc_data_type);
  }

  if(status != OK){
    errno = 0;
    ErrorReturn(NULL, (ERROR_BADPARM, "mincRead(): error reading "
                       "volume from file %s", fname));
  }

  /* ----- check the number of dimensions ----- */
  ndims = get_volume_n_dimensions(vol);
  if(ndims != 3 && ndims != 4){
    errno = 0;
    ErrorReturn(NULL, (ERROR_BADPARM, "mincRead(): %d dimensions "
                       "in file; expecting 3 or 4", ndims));
  }

  /* ----- get the dimension sizes ----- */
  get_volume_sizes(vol, dim_sizes);

  /* --- one time point if there are only three dimensions in the file --- */
  if(ndims == 3) dim_sizes[3] = 1;

  dtype = get_volume_nc_data_type(vol, &sflag) ;
  dtype = orderIntBytes(vol->nc_data_type) ;
  get_volume_separations(vol, separations);

  for(i=0;i<4;i++)
    printf("%d %s %3d  %7.3f %6.4f %6.4f %6.4f \n",
           i,dim_names[i],dim_sizes[i],separations[i],
           vol->direction_cosines[i][0],vol->direction_cosines[i][1],
           vol->direction_cosines[i][2]);
  if ((Gdiag & DIAG_SHOW) && DIAG_VERBOSE_ON)
    printf("DataType: %d\n", vol->nc_data_type);

  /* Translate data type to that of mri structure */
  switch(vol->nc_data_type){
  case NC_BYTE:   dtype = MRI_UCHAR; break;
  case NC_CHAR:   dtype = MRI_UCHAR; break;
  case NC_SHORT:  dtype = MRI_SHORT; break;
  case NC_LONG:   dtype = MRI_LONG;  break;
  case NC_FLOAT:  dtype = MRI_FLOAT; break;
  case NC_DOUBLE: dtype = MRI_FLOAT; break;
  default:
    errno = 0;
    ErrorReturn(NULL, (ERROR_BADPARM, "mincRead(): bad data type "
                       "(%d) in input file %s", vol->nc_data_type, fname));
    break;
  }

  /* ----- allocate the mri structure ----- */
  if(read_volume)
    mri = MRIallocSequence(dim_sizes[0], dim_sizes[1], dim_sizes[2],
                           dtype, dim_sizes[3]);
  else{
    mri = MRIallocHeader(dim_sizes[0], dim_sizes[1], dim_sizes[2], dtype);
    mri->nframes = dim_sizes[3];
  }

  /* ----- set up the mri structure ----- */
  get_volume_separations(vol, separations);
  mri->xsize = fabs(separations[0]); /* xsize = col   resolution */
  mri->ysize = fabs(separations[1]); /* ysize = row   resolution */
  mri->zsize = fabs(separations[2]); /* zsize = slice resolution */
  mri->ps    = mri->xsize;
  mri->thick = mri->zsize;

  /* column direction cosines */
  mri->x_r = vol->direction_cosines[0][0];
  mri->x_a = vol->direction_cosines[0][1];
  mri->x_s = vol->direction_cosines[0][2];

  /* row direction cosines */
  mri->y_r = vol->direction_cosines[1][0];
  mri->y_a = vol->direction_cosines[1][1];
  mri->y_s = vol->direction_cosines[1][2];

  /* slice direction cosines */
  mri->z_r = vol->direction_cosines[2][0];
  mri->z_a = vol->direction_cosines[2][1];
  mri->z_s = vol->direction_cosines[2][2];

  if(separations[0] < 0){
    mri->x_r = -mri->x_r;  mri->x_a = -mri->x_a;  mri->x_s = -mri->x_s;
  }
  if(separations[1] < 0){
    mri->y_r = -mri->y_r;  mri->y_a = -mri->y_a;  mri->y_s = -mri->y_s;
  }
  if(separations[2] < 0){
    mri->z_r = -mri->z_r;  mri->z_a = -mri->z_a;  mri->z_s = -mri->z_s;
  }

  /* Get center point */       // don't.  our convention is different
  voxel[0] = (mri->width)/2.;  // (mri->width  - 1) / 2.0;
  voxel[1] = (mri->height)/2.; // (mri->height - 1) / 2.0;
  voxel[2] = (mri->depth)/2.;  //(mri->depth  - 1) / 2.0;
  voxel[3] = 0.0;
  convert_voxel_to_world(vol, voxel, &worldr, &worlda, &worlds);
  mri->c_r = worldr;
  mri->c_a = worlda;
  mri->c_s = worlds;
  printf("Center Voxel: %7.3f %7.3f %7.3f\n",voxel[0],voxel[1],voxel[2]);
  printf("Center World: %7.3f %7.3f %7.3f\n",mri->c_r,mri->c_a,mri->c_s);

  mri->ras_good_flag = 1;

  mri->xend = mri->xsize * mri->width / 2.0; mri->xstart = -mri->xend;
  mri->yend = mri->ysize * mri->height/ 2.0; mri->ystart = -mri->yend;
  mri->zend = mri->zsize * mri->depth / 2.0; mri->zstart = -mri->zend;

  xfov = mri->xend - mri->xstart;
  yfov = mri->yend - mri->ystart;
  zfov = mri->zend - mri->zstart;

  mri->fov =
    (xfov > yfov ? (xfov > zfov ? xfov : zfov) : (yfov > zfov ? yfov : zfov));

  strcpy(mri->fname, fname);

  T = MRIxfmCRS2XYZ(mri,0);
  printf("Input Coordinate Transform (CRS2XYZ)-------\n");
  MatrixPrint(stdout,T);
  MatrixFree(&T);
  pVox2WorldGen = get_voxel_to_world_transform(vol);
  pVox2WorldLin = get_linear_transform_ptr(pVox2WorldGen);
  if ((Gdiag & DIAG_SHOW) && DIAG_VERBOSE_ON)
    {
      printf("MINC Linear Transform ----------------------\n");
      for(i=0;i<4;i++){
        for(j=0;j<4;j++) printf("%7.4f ",pVox2WorldLin->m[j][i]);
        printf("\n");
      }
      printf("-------------------------------------------\n");
    }

  /* ----- copy the data from the file to the mri structure ----- */
  if(read_volume){

    while(input_more_of_volume(vol, &input_info, &f));

    for(i = 0;i < mri->width;i++) {
      for(j = 0;j < mri->height;j++) {
        for(k = 0;k < mri->depth;k++) {
          for(t = 0;t < mri->nframes;t++) {
            val = get_volume_voxel_value(vol, i, j, k, t, 0);
            switch(mri->type){
            case MRI_UCHAR: MRIseq_vox(mri,i,j,k,t) = (unsigned char)val;break;
            case MRI_SHORT: MRISseq_vox(mri,i,j,k,t) = (short)val;break;
            case MRI_LONG:  MRILseq_vox(mri,i,j,k,t) = (long)val;break;
            case MRI_FLOAT: MRIFseq_vox(mri,i,j,k,t) = (float)val;break;
            }
          }
        }
      }
    }
  }

  delete_volume_input(&input_info);
  delete_volume(vol);

  return(mri);

} /* end mincRead2() */
/*-----------------------------------------------------------*/
static int GetMINCInfo(MRI *mri,
                       char *dim_names[4],
                       int   dim_sizes[4],
                       Real separations[4],
                       Real dircos[3][3],
                       Real VolCenterVox[3],
                       Real VolCenterWorld[3])
{
  int xspacehit, yspacehit, zspacehit;
  float col_dc_x, col_dc_y, col_dc_z;
  float row_dc_x, row_dc_y, row_dc_z;
  float slc_dc_x, slc_dc_y, slc_dc_z;
  Real col_dc_sign, row_dc_sign, slc_dc_sign;
  int err,i,j;

  col_dc_x = fabs(mri->x_r);
  col_dc_y = fabs(mri->x_a);
  col_dc_z = fabs(mri->x_s);
  col_dc_sign = 1;

  row_dc_x = fabs(mri->y_r);
  row_dc_y = fabs(mri->y_a);
  row_dc_z = fabs(mri->y_s);
  row_dc_sign = 1;

  slc_dc_x = fabs(mri->z_r);
  slc_dc_y = fabs(mri->z_a);
  slc_dc_z = fabs(mri->z_s);
  slc_dc_sign = 1;

  xspacehit = 0;
  yspacehit = 0;
  zspacehit = 0;

  /* Name the Column Axis */
  if(col_dc_x >= row_dc_x && col_dc_x >= slc_dc_x){
    dim_names[0] = MIxspace;
    VolCenterVox[0] = (mri->width-1)/2.0;
    if(mri->x_r < 0) col_dc_sign = -1;
    xspacehit = 1;
  }
  else if(col_dc_y >= row_dc_y && col_dc_y >= slc_dc_y){
    dim_names[0] = MIyspace;
    VolCenterVox[0] = (mri->height-1)/2.0;
    if(mri->x_a < 0) col_dc_sign = -1;
    yspacehit = 1;
  }
  else if(col_dc_z >= row_dc_z && col_dc_z >= slc_dc_z){
    dim_names[0] = MIzspace;
    VolCenterVox[0] = (mri->depth-1)/2.0;
    if(mri->x_s < 0) col_dc_sign = -1;
    zspacehit = 1;
  }

  /* Name the Row Axis */
  if(!xspacehit && row_dc_x >= slc_dc_x){
    dim_names[1] = MIxspace;
    VolCenterVox[1] = (mri->width-1)/2.0;
    if(mri->y_r < 0) row_dc_sign = -1;
    xspacehit = 1;
  }
  else if(!yspacehit && row_dc_y >= slc_dc_y){
    dim_names[1] = MIyspace;
    VolCenterVox[1] = (mri->height-1)/2.0;
    if(mri->y_a < 0) row_dc_sign = -1;
    yspacehit = 1;
  }
  else if(!zspacehit && row_dc_z >= slc_dc_z){
    dim_names[1] = MIzspace;
    VolCenterVox[1] = (mri->depth-1)/2.0;
    if(mri->y_s < 0) row_dc_sign = -1;
    zspacehit = 1;
  }

  /* Name the Slice Axis */
  if(!xspacehit){
    dim_names[2] = MIxspace;
    VolCenterVox[2] = (mri->width-1)/2.0;
    if(mri->z_r < 0) slc_dc_sign = -1;
    xspacehit = 1;
  }
  else if(!yspacehit){
    dim_names[2] = MIyspace;
    VolCenterVox[2] = (mri->height-1)/2.0;
    if(mri->z_a < 0) slc_dc_sign = -1;
    yspacehit = 1;
  }
  if(!zspacehit){
    dim_names[2] = MIzspace;
    VolCenterVox[2] = (mri->depth-1)/2.0;
    if(mri->z_s < 0) slc_dc_sign = -1;
    zspacehit = 1;
  }

  /* Check for errors in the Axis Naming*/
  err = 0;
  if(!xspacehit){
    printf("ERROR: could not assign xspace\n");
    err = 1;
  }
  if(!yspacehit){
    printf("ERROR: could not assign yspace\n");
    err = 1;
  }
  if(!zspacehit){
    printf("ERROR: could not assign zspace\n");
    err = 1;
  }
  if(err) return(1);

  /* Name the Frame Axis */
  dim_names[3] = MItime;

  /* Set world center */
  VolCenterWorld[0] = mri->c_r;
  VolCenterWorld[1] = mri->c_a;
  VolCenterWorld[2] = mri->c_s;

  /* Set dimension lengths */
  dim_sizes[0] = mri->width;
  dim_sizes[1] = mri->height;
  dim_sizes[2] = mri->depth;
  dim_sizes[3] = mri->nframes;

  /* Set separations */
  separations[0] = col_dc_sign * mri->xsize;
  separations[1] = row_dc_sign * mri->ysize;
  separations[2] = slc_dc_sign * mri->zsize;
  separations[3] = mri->tr;

  /* Set direction Cosines */
  dircos[0][0] = col_dc_sign * mri->x_r;
  dircos[0][1] = col_dc_sign * mri->x_a;
  dircos[0][2] = col_dc_sign * mri->x_s;

  dircos[1][0] = row_dc_sign * mri->y_r;
  dircos[1][1] = row_dc_sign * mri->y_a;
  dircos[1][2] = row_dc_sign * mri->y_s;

  dircos[2][0] = slc_dc_sign * mri->z_r;
  dircos[2][1] = slc_dc_sign * mri->z_a;
  dircos[2][2] = slc_dc_sign * mri->z_s;

  /* This is a hack for the case where the dircos are the default.
     The MINC routines will not save the direction cosines as
     NetCDF attriubtes if they correspond to the default. */
  for(i=0;i<3;i++)
    for(j=0;j<3;j++)
      if(fabs(dircos[i][j] < .00000000001)) dircos[i][j] = .0000000000000001;

  return(0);
}

/*-----------------------------------------------------------*/
static int mincWrite2(MRI *mri, char *fname)
{
  Volume minc_volume;
  nc_type nc_data_type = NC_BYTE;
  int ndim,i,j;
  Real min, max;
  float fmin, fmax;
  char *dim_names[4];
  int   dim_sizes[4];
  Real separations[4];
  Real dircos[3][3];
  Real VolCenterVox[3];
  Real VolCenterWorld[3];
  int signed_flag = 0;
  int r,c,s,f;
  Real VoxVal = 0.0;
  int return_value;
  VIO_Status status;
  MATRIX *T;
  Transform *pVox2WorldLin;
  General_transform *pVox2WorldGen;

  /* Get the min and max for the volume */
  if((return_value = MRIlimits(mri, &fmin, &fmax)) != NO_ERROR)
    return(return_value);
  min = (Real)fmin;
  max = (Real)fmax;

  /* Translate mri type to NetCDF type */
  switch(mri->type){
  case MRI_UCHAR: nc_data_type = NC_BYTE;  signed_flag = 0; break;
  case MRI_SHORT: nc_data_type = NC_SHORT; signed_flag = 1; break;
  case MRI_INT:   nc_data_type = NC_LONG;  signed_flag = 1; break;
  case MRI_LONG:  nc_data_type = NC_LONG;  signed_flag = 1; break;
  case MRI_FLOAT: nc_data_type = NC_FLOAT; signed_flag = 1; break;
  }

  /* Assign default direction cosines, if needed */
  if(mri->ras_good_flag == 0){
    setDirectionCosine(mri, MRI_CORONAL);
  }

  GetMINCInfo(mri, dim_names, dim_sizes, separations, dircos,
              VolCenterVox,VolCenterWorld);

  if(mri->nframes > 1) ndim = 4;
  else                 ndim = 3;

  minc_volume = create_volume(ndim, dim_names, nc_data_type,
                              signed_flag, min, max);
  set_volume_sizes(minc_volume, dim_sizes);
  alloc_volume_data(minc_volume);

  for(i=0;i<3;i++){
    printf("%d %s %4d %7.3f  %7.3f %7.3f %7.3f \n",
           i,dim_names[i],dim_sizes[i],separations[i],
           dircos[i][0],dircos[i][1],dircos[i][2]);
    set_volume_direction_cosine(minc_volume, i, dircos[i]);
  }
  printf("Center Voxel: %7.3f %7.3f %7.3f\n",
         VolCenterVox[0],VolCenterVox[1],VolCenterVox[2]);
  printf("Center World: %7.3f %7.3f %7.3f\n",
         VolCenterWorld[0],VolCenterWorld[1],VolCenterWorld[2]);

  set_volume_separations(minc_volume, separations);
  set_volume_translation(minc_volume, VolCenterVox, VolCenterWorld);

  T = MRIxfmCRS2XYZ(mri,0);
  printf("MRI Transform -----------------------------\n");
  MatrixPrint(stdout,T);
  pVox2WorldGen = get_voxel_to_world_transform(minc_volume);
  pVox2WorldLin = get_linear_transform_ptr(pVox2WorldGen);
  if ((Gdiag & DIAG_SHOW) && DIAG_VERBOSE_ON)
    {
      printf("MINC Linear Transform ----------------------\n");
      for(i=0;i<4;i++){
        for(j=0;j<4;j++) printf("%7.4f ",pVox2WorldLin->m[j][i]);
        printf("\n");
      }
      printf("--------------------------------------------\n");
    }
  MatrixFree(&T);

  printf("Setting Volume Values\n");
  for(f = 0; f < mri->nframes; f++) {     /* frames */
    for(c = 0; c < mri->width;   c++) {     /* columns */
      for(r = 0; r < mri->height;  r++) {     /* rows */
        for(s = 0; s < mri->depth;   s++) {     /* slices */

          switch(mri->type){
          case MRI_UCHAR: VoxVal = (Real)MRIseq_vox(mri,  c, r, s, f);break;
          case MRI_SHORT: VoxVal = (Real)MRISseq_vox(mri, c, r, s, f);break;
          case MRI_INT:   VoxVal = (Real)MRIIseq_vox(mri, c, r, s, f);break;
          case MRI_LONG:  VoxVal = (Real)MRILseq_vox(mri, c, r, s, f);break;
          case MRI_FLOAT: VoxVal = (Real)MRIFseq_vox(mri, c, r, s, f);break;
          }
          set_volume_voxel_value(minc_volume, c, r, s, f, 0, VoxVal);
        }
      }
    }
  }

  printf("Writing Volume\n");
  status = output_volume((STRING)fname, nc_data_type, signed_flag, min, max,
                         minc_volume, (STRING)"", NULL);
  printf("Cleaning Up\n");
  delete_volume(minc_volume);

  if(status) {
    printf("ERROR: mincWrite: output_volume exited with %d\n",status);
    return(1);
  }

  printf("mincWrite2: done\n");
  return(NO_ERROR);
}


/*-------------------------------------------------------
  NormalizeVector() - in-place vector normalization
  -------------------------------------------------------*/
static int NormalizeVector(float *v, int n)
{
  float sum2;
  int i;
  sum2 = 0;
  for(i=0;i<n;i++) sum2 += v[i];
  sum2 = sqrt(sum2);
  for(i=0;i<n;i++) v[i] /= sum2;
  return(0);
}
#endif
/*----------------------------------------------------------*/
/* time course clean */
static int mincWrite(MRI *mri, char *fname)
{

  Volume minc_volume;
  STRING dimension_names[4] = { "xspace", "yspace", "zspace", "time" };
  nc_type nc_data_type;
  Real min, max;
  float fmin, fmax;
  int dimension_sizes[4];
  Real separations[4];
  Real dir_cos[4];
  int return_value;
  Real voxel[4], world[4];
  int signed_flag;
  int di_x, di_y, di_z;
  int vi[4];
  /*   int r, a, s; */
  /*   float r_max; */
  VIO_Status status;

  /* di gives the bogus minc index
     di[0] is width, 1 is height, 2 is depth, 3 is time if
     there is a time dimension of length > 1
     minc wants these to be ras */

  /* here: minc volume index 0 is r, 1 is a, 2 is s */
  /* mri is lia *//* (not true for some volumes *)*/

  /* ----- get the orientation of the volume ----- */
  if(mri->ras_good_flag == 0)
    {
      setDirectionCosine(mri, MRI_CORONAL);
    }

  /* we don't muck around axes anymore */
  di_x = 0;
  di_y = 1;
  di_z = 2;

#if 0
  /*  The following remapping is only valid for COR files */
  /*  In order to handle arbitrary volumes, we don't muck around */
  /*  the axes anymore  ... tosa */

  /* orig->minc map ***********************************/
  /* r axis is mapped to (x_r, y_r, z_r) voxel coords */
  /* thus find the biggest value among x_r, y_r, z_r  */
  /* that one corresponds to the r-axis               */
  r = 0;
  r_max = fabs(mri->x_r);
  if(fabs(mri->y_r) > r_max)
    {
      r_max = fabs(mri->y_r);
      r = 1;
    }
  if(fabs(mri->z_r) > r_max)
    r = 2;

  /* a axis is mapped to (x_a, y_a, z_a) voxel coords */
  if(r == 0)
    a = (fabs(mri->y_a) > fabs(mri->z_a) ? 1 : 2);
  else if(r == 1)
    a = (fabs(mri->x_a) > fabs(mri->z_a) ? 0 : 2);
  else
    a = (fabs(mri->x_a) > fabs(mri->y_a) ? 0 : 1);

  /* s axis is mapped to (x_s, y_s, z_s) voxel coords */
  /* use the rest to figure                           */
  s = 3 - r - a;

  /* ----- set the appropriate minc axes to this orientation ----- */
  /* r gives the mri structure axis of the lr coordinate */
  /* lr = minc xspace = 0 */
  /* a ..... of the pa coordinate *//* pa = minc yspace = 1 */
  /* s ..... of the is coordinate *//* is = minc zspace = 2 */

  /* di of this axis must be set to 2 */
  /* ... and so on */

  /* minc->orig map **************************************/
  /* you don't need the routine above but do a similar thing */
  /* to get exactly the same, i.e.                           */
  /* x-axis corresponds to (x_r, x_a, x_s) minc coords */
  /* y-axis                (y_r, y_a, y_s) minc coords */
  /* z-axis                (z_r, z_a, z_s) minc coords */
  /* thus we are doing too much work                   */

  if(r == 0)
    {
      if(a == 1)
        {
          di_x = 0;
          di_y = 1;
          di_z = 2;
        }
      else
        {
          di_x = 0;
          di_y = 2;
          di_z = 1;
        }
    }
  else if(r == 1)
    {
      if(a == 0)
        {
          di_x = 1;
          di_y = 0;
          di_z = 2;
        }
      else
        {
          di_x = 2;
          di_y = 0;
          di_z = 1;
        }
    }
  else
    {
      if(a == 0)
        {
          di_x = 1;
          di_y = 2;
          di_z = 0;
        }
      else
        {
          di_x = 2;
          di_y = 1;
          di_z = 0;
        }
    }
#endif

  /* ----- set the data type ----- */
  if(mri->type == MRI_UCHAR)
    {
      nc_data_type = NC_BYTE;
      signed_flag = 0;
    }
  else if(mri->type == MRI_SHORT)
    {
      nc_data_type = NC_SHORT;
      signed_flag = 1;
    }
  else if(mri->type == MRI_INT)
    {
      nc_data_type = NC_LONG;
      signed_flag = 1;
    }
  else if(mri->type == MRI_LONG)
    {
      nc_data_type = NC_LONG;
      signed_flag = 1;
    }
  else if(mri->type == MRI_FLOAT)
    {
      nc_data_type = NC_FLOAT;
      signed_flag = 1;
    }
  else
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "mincWrite(): bad data type (%d) in mri structure",
          mri->type));
    }

  if((return_value = MRIlimits(mri, &fmin, &fmax)) != NO_ERROR)
    return(return_value);

  min = (Real)fmin;
  max = (Real)fmax;

  if(mri->nframes == 1)
    minc_volume = create_volume
      (3, dimension_names, nc_data_type, signed_flag, min, max);
  else
    minc_volume = create_volume
      (4, dimension_names, nc_data_type, signed_flag, min, max);

  /* di_(x,y,z) is the map from minc to orig */
  /* minc dimension size is that of di_x, etc. */
  dimension_sizes[di_x] = mri->width;
  dimension_sizes[di_y] = mri->height;
  dimension_sizes[di_z] = mri->depth;
  dimension_sizes[3] = mri->nframes;

  set_volume_sizes(minc_volume, dimension_sizes);

  alloc_volume_data(minc_volume);

  separations[di_x] = (Real)(mri->xsize);
  separations[di_y] = (Real)(mri->ysize);
  separations[di_z] = (Real)(mri->zsize);
  separations[3] = 1.0; // appears to do nothing
  set_volume_separations(minc_volume, separations);
  /* has side effect to change transform and thus must be set first */

  dir_cos[0] = (Real)mri->x_r;
  dir_cos[1] = (Real)mri->x_a;
  dir_cos[2] = (Real)mri->x_s;
  set_volume_direction_cosine(minc_volume, di_x, dir_cos);

  dir_cos[0] = (Real)mri->y_r;
  dir_cos[1] = (Real)mri->y_a;
  dir_cos[2] = (Real)mri->y_s;
  set_volume_direction_cosine(minc_volume, di_y, dir_cos);

  dir_cos[0] = (Real)mri->z_r;
  dir_cos[1] = (Real)mri->z_a;
  dir_cos[2] = (Real)mri->z_s;
  set_volume_direction_cosine(minc_volume, di_z, dir_cos);

  voxel[di_x] = mri->width / 2.0; // promoted to double
  voxel[di_y] = mri->height/ 2.0;
  voxel[di_z] = mri->depth / 2.0;
  voxel[3] = 0.0;
  world[0] = (Real)(mri->c_r);
  world[1] = (Real)(mri->c_a);
  world[2] = (Real)(mri->c_s);
  world[3] = 0.0;
  set_volume_translation(minc_volume, voxel, world);

  /* get the position from (vi[di_x], vi[di_y], vi[di_z]) orig position     */
  /*      put the value to (vi[0], vi[1], vi[2]) minc volume                */
  /* this will keep the orientation of axes the same as the original        */

  /* vi[n] gives the index of the variable along minc axis x */
  /* vi[di_x] gives the index of the variable
     along minc axis di_x, or along mri axis x */
  for(vi[3] = 0;vi[3] < mri->nframes;vi[3]++) {           /* frames */
    for(vi[di_x] = 0;vi[di_x] < mri->width;vi[di_x]++) {   /* columns */
      for(vi[di_y] = 0;vi[di_y] < mri->height;vi[di_y]++) { /* rows */
        for(vi[di_z] = 0;vi[di_z] < mri->depth;vi[di_z]++) { /* slices */

          if(mri->type == MRI_UCHAR)
            set_volume_voxel_value
              (minc_volume, vi[0], vi[1], vi[2], vi[3], 0,
               (Real)MRIseq_vox(mri, vi[di_x], vi[di_y], vi[di_z], vi[3]));
          if(mri->type == MRI_SHORT)
            set_volume_voxel_value
              (minc_volume, vi[0], vi[1], vi[2], vi[3], 0,
               (Real)MRISseq_vox(mri, vi[di_x], vi[di_y], vi[di_z], vi[3]));
          if(mri->type == MRI_INT)
            set_volume_voxel_value
              (minc_volume, vi[0], vi[1], vi[2], vi[3], 0,
               (Real)MRIIseq_vox(mri, vi[di_x], vi[di_y], vi[di_z], vi[3]));
          if(mri->type == MRI_LONG)
            set_volume_voxel_value
              (minc_volume, vi[0], vi[1], vi[2], vi[3], 0,
               (Real)MRILseq_vox(mri, vi[di_x], vi[di_y], vi[di_z], vi[3]));
          if(mri->type == MRI_FLOAT)
            set_volume_voxel_value
              (minc_volume, vi[0], vi[1], vi[2], vi[3], 0,
               (Real)MRIFseq_vox(mri, vi[di_x], vi[di_y], vi[di_z], vi[3]));
        }
      }
    }
  }

  status = output_volume((STRING)fname, nc_data_type, signed_flag, min, max,
                         minc_volume, (STRING)"", NULL);
  delete_volume(minc_volume);

  if(status) {
    printf("ERROR: mincWrite: output_volume exited with %d\n",status);
    return(1);
  }

  return(NO_ERROR);

} /* end mincWrite() */


/*----------------------------------------------------------------
  bvolumeWrite() - replaces bshortWrite and bfloatWrite.
  Bug: fname_passed must the the stem, not the full file name.
  -----------------------------------------------------------------*/
static int bvolumeWrite(MRI *vol, char *fname_passed, int type)
{

  int i, j, t;
  char fname[STRLEN];
  short *bufshort;
  float *buffloat;
  FILE *fp;
  int result;
  MRI *subject_info = NULL;
  char subject_volume_dir[STRLEN];
  char *subjects_dir;
  char *sn;
  char analyse_fname[STRLEN], register_fname[STRLEN];
  char output_dir[STRLEN];
  char *c;
  int od_length;
  char t1_path[STRLEN];
  MATRIX *cf, *bf, *ibf, *af, *iaf, *as, *bs, *cs, *ics, *r;
  MATRIX *r1, *r2, *r3, *r4;
  float det;
  int bad_flag;
  int l;
  char stem[STRLEN];
  char *c1, *c2, *c3;
  struct stat stat_buf;
  char subject_dir[STRLEN];
  int dealloc, nslices, nframes;
  MRI *mri;
  float min,max;
  int swap_bytes_flag, size, bufsize;
  char *ext;
  void *buf;

  /* check the type and set the extension and size*/
  switch(type) {
  case MRI_SHORT: ext = "bshort"; size = sizeof(short); break;
  case MRI_FLOAT: ext = "bfloat"; size = sizeof(float); break;
  default:
    fprintf
      (stderr,"ERROR: bvolumeWrite: type (%d) is not short or float\n", type);
    return(1);
  }

  if(vol->type != type){
    if (DIAG_VERBOSE_ON)
      printf("INFO: bvolumeWrite: changing type\n");
    nslices = vol->depth;
    nframes = vol->nframes;
    vol->depth = nslices*nframes;
    vol->nframes = 1;
    MRIlimits(vol,&min,&max);
    if (DIAG_VERBOSE_ON)
      printf("INFO: bvolumeWrite: range %g %g\n",min,max);
    mri = MRIchangeType(vol,type,min,max,1);
    if(mri == NULL) {
      fprintf(stderr,"ERROR: bvolumeWrite: MRIchangeType\n");
      return(1);
    }
    vol->depth = nslices;
    vol->nframes = nframes;
    mri->depth = nslices;
    mri->nframes = nframes;
    dealloc = 1;
  }
  else{
    mri = vol;
    dealloc = 0;
  }

  /* ----- get the stem from the passed file name ----- */
  /*
    four options:
    1. stem_xxx.bshort
    2. stem.bshort
    3. stem_xxx
    4. stem
    other possibles:
    stem_.bshort
  */
  l = strlen(fname_passed);
  c1 = fname_passed + l - 11;
  c2 = fname_passed + l - 7;
  c3 = fname_passed + l - 4;
  strcpy(stem, fname_passed);
  if(c1 > fname_passed)
    {
      if( (*c1 == '_' && strcmp(c1+4, ".bshort") == 0) ||
          (*c1 == '_' && strcmp(c1+4, ".bfloat") == 0) )
        stem[(int)(c1-fname_passed)] = '\0';
    }
  if(c2 > fname_passed)
    {
      if( (*c2 == '_' && strcmp(c2+4, ".bshort") == 0) ||
          (*c2 == '_' && strcmp(c2+4, ".bfloat") == 0) )
        stem[(int)(c2-fname_passed)] = '\0';

    }
  if(c3 > fname_passed)
    {
      if(*c3 == '_')
        stem[(int)(c3-fname_passed)] = '\0';
    }
  printf("INFO: bvolumeWrite: stem = %s\n",stem);

  c = strrchr(stem, '/');
  if(c == NULL)
    output_dir[0] = '\0';
  else
    {
      od_length = (int)(c - stem);
      strncpy(output_dir, stem, od_length);
      /* -- leaving the trailing '/' on a directory is not my
         usual convention, but here it's a load easier if there's
         no directory in stem... -ch -- */
      output_dir[od_length] = '/';
      output_dir[od_length+1] = '\0';
    }

  sprintf(analyse_fname, "%s%s", output_dir, "analyse.dat");
  sprintf(register_fname, "%s%s", output_dir, "register.dat");

  bufsize  = mri->width * size;
  bufshort = (short *)malloc(bufsize);
  buffloat = (float *)malloc(bufsize);

  buf = NULL; /* shuts up compiler */
  if(type == MRI_SHORT)  buf = bufshort;
  else                   buf = buffloat;

  swap_bytes_flag = 0;
#if (BYTE_ORDER==LITTLE_ENDIAN)
  swap_bytes_flag = 1;
#endif

  //printf("--------------------------------\n");
  //MRIdump(mri,stdout);
  //MRIdumpBuffer(mri,stdout);
  //printf("--------------------------------\n");

  for(i = 0;i < mri->depth;i++)
    {

      /* ----- write the header file ----- */
      sprintf(fname, "%s_%03d.hdr", stem, i);
      if((fp = fopen(fname, "w")) == NULL)
        {
          if(dealloc) MRIfree(&mri);
          free(bufshort);
          free(buffloat);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE, (ERROR_BADFILE,
                             "bvolumeWrite(): can't open file %s", fname));
        }
      fprintf(fp, "%d %d %d %d\n", mri->height, mri->width, mri->nframes, 0);
      fclose(fp);

      /* ----- write the data file ----- */
      sprintf(fname, "%s_%03d.%s", stem, i, ext);
      if((fp = fopen(fname, "w")) == NULL)
        {
          if(dealloc) MRIfree(&mri);
          free(bufshort);
          free(buffloat);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE, (ERROR_BADFILE,
                             "bvolumeWrite(): can't open file %s", fname));
        }

      for(t = 0;t < mri->nframes;t++) {
        for(j = 0;j < mri->height;j++) {
          memcpy(buf,mri->slices[t*mri->depth + i][j], bufsize);

          if(swap_bytes_flag){
            if(type == MRI_SHORT) byteswapbufshort((void*)buf, bufsize);
            else                  byteswapbuffloat((void*)buf, bufsize);
          }
          fwrite(buf, size, mri->width, fp);
        }
      }

      fclose(fp);
    }

  free(bufshort);
  free(buffloat);

  sn = subject_name;
  if(mri->subject_name[0] != '\0')
    sn = mri->subject_name;

  if(sn != NULL)
    {
      if((subjects_dir = getenv("SUBJECTS_DIR")) == NULL)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bvolumeWrite(): environment variable SUBJECTS_DIR unset");
          if(dealloc) MRIfree(&mri);
        }
      else
        {

          sprintf(subject_dir, "%s/%s", subjects_dir, sn);
          if(stat(subject_dir, &stat_buf) < 0)
            {
              fprintf
                (stderr,
                 "can't stat %s; writing to bhdr instead\n", subject_dir);
            }
          else
            {
              if(!S_ISDIR(stat_buf.st_mode))
                {
                  fprintf
                    (stderr,
                     "%s is not a directory; writing to bhdr instead\n",
                     subject_dir);
                }
              else
                {
                  sprintf(subject_volume_dir, "%s/mri/T1", subject_dir);
                  subject_info = MRIreadInfo(subject_volume_dir);
                  if(subject_info == NULL)
                    {
                      sprintf(subject_volume_dir, "%s/mri/orig", subject_dir);
                      subject_info = MRIreadInfo(subject_volume_dir);
                      if(subject_info == NULL)
                        fprintf(stderr,
                                "can't read the subject's orig or T1 volumes; "
                                "writing to bhdr instead\n");
                    }
                }
            }

        }

    }

  if(subject_info != NULL)
    {
      if(subject_info->ras_good_flag == 0)
        {
          setDirectionCosine(subject_info, MRI_CORONAL);
        }
    }

  cf = bf = ibf = af = iaf = as = bs = cs = ics = r = NULL;
  r1 = r2 = r3 = r4 = NULL;

  /* ----- write the register.dat and analyse.dat  or bhdr files ----- */
  if(subject_info != NULL)
    {

      bad_flag = FALSE;

      if((as = MatrixAlloc(4, 4, MATRIX_REAL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bvolumeWrite(): error creating matrix");
          bad_flag = TRUE;
        }
      stuff_four_by_four
        (as, subject_info->x_r, subject_info->y_r, subject_info->z_r, subject_info->c_r,
         subject_info->y_r, subject_info->y_r, subject_info->y_r, subject_info->c_r,
         subject_info->z_r, subject_info->z_r, subject_info->z_r, subject_info->c_r,
         0.0,               0.0,               0.0,               1.0);

      if((af = MatrixAlloc(4, 4, MATRIX_REAL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bvolumeWrite(): error creating matrix");
          bad_flag = TRUE;
        }
      stuff_four_by_four(af, mri->x_r, mri->y_r, mri->z_r, mri->c_r,
                         mri->y_r, mri->y_r, mri->y_r, mri->c_r,
                         mri->z_r, mri->z_r, mri->z_r, mri->c_r,
                         0.0,      0.0,      0.0,      1.0);

      if((bs = MatrixAlloc(4, 4, MATRIX_REAL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bvolumeWrite(): error creating matrix");
          bad_flag = TRUE;
        }
      stuff_four_by_four(bs, 1, 0, 0, (subject_info->width  - 1) / 2.0,
                         0, 1, 0, (subject_info->height - 1) / 2.0,
                         0, 0, 1, (subject_info->depth  - 1) / 2.0,
                         0, 0, 0,                              1.0);

      if((bf = MatrixAlloc(4, 4, MATRIX_REAL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bvolumeWrite(): error creating matrix");
          bad_flag = TRUE;
        }
      stuff_four_by_four(bf, 1, 0, 0, (mri->width  - 1) / 2.0,
                         0, 1, 0, (mri->height - 1) / 2.0,
                         0, 0, 1, (mri->depth  - 1) / 2.0,
                         0, 0, 0,                     1.0);

      if((cs = MatrixAlloc(4, 4, MATRIX_REAL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bvolumeWrite(): error creating matrix");
          bad_flag = TRUE;
        }
      stuff_four_by_four
        (cs,
         -subject_info->xsize, 0, 0,(subject_info->width  * mri->xsize) / 2.0,\
         0, 0, subject_info->zsize, -(subject_info->depth  * mri->zsize) / 2.0, \
         0, -subject_info->ysize, 0,  (subject_info->height * mri->ysize) / 2.0, \
         0, 0, 0, 1);

      if((cf = MatrixAlloc(4, 4, MATRIX_REAL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bvolumeWrite(): error creating matrix");
          bad_flag = TRUE;
        }
      stuff_four_by_four
        (cf,
         -mri->xsize, 0,          0,  (mri->width  * mri->xsize) / 2.0,
         0,           0, mri->zsize, -(mri->depth  * mri->zsize) / 2.0,
         0, -mri->ysize,          0,  (mri->height * mri->ysize) / 2.0,
         0,           0,          0,                                 1);

      if(bad_flag)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bvolumeWrite(): error creating one "
             "or more matrices; aborting register.dat "
             "write and writing bhdr instead");
          MRIfree(&subject_info);
        }

    }

  if(subject_info != NULL)
    {

      bad_flag = FALSE;

      if((det = MatrixDeterminant(as)) == 0.0)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bvolumeWrite(): bad determinant in "
             "matrix (check structural volume)");
          bad_flag = TRUE;
        }
      if((det = MatrixDeterminant(bs)) == 0.0)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bvolumeWrite(): bad determinant in "
             "matrix (check structural volume)");
          bad_flag = TRUE;
        }
      if((det = MatrixDeterminant(cs)) == 0.0)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bvolumeWrite(): bad determinant in "
             "matrix (check structural volume)");
          bad_flag = TRUE;
        }

      if((det = MatrixDeterminant(af)) == 0.0)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bvolumeWrite(): bad determinant in "
             "matrix (check functional volume)");
          bad_flag = TRUE;
        }
      if((det = MatrixDeterminant(bf)) == 0.0)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bvolumeWrite(): bad determinant in "
             "matrix (check functional volume)");
          bad_flag = TRUE;
        }
      if((det = MatrixDeterminant(cf)) == 0.0)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bvolumeWrite(): bad determinant in "
             "matrix (check functional volume)");
          bad_flag = TRUE;
        }

      if(bad_flag)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM, "bvolumeWrite(): one or more zero determinants;"
             " aborting register.dat write and writing bhdr instead");
          MRIfree(&subject_info);
        }

    }

  if(subject_info != NULL)
    {

      bad_flag = FALSE;

      if((iaf = MatrixInverse(af, NULL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bvolumeWrite(): error inverting matrix");
          bad_flag = TRUE;
        }
      if((ibf = MatrixInverse(bf, NULL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bvolumeWrite(): error inverting matrix");
          bad_flag = TRUE;
        }
      if((ics = MatrixInverse(cs, NULL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bvolumeWrite(): error inverting matrix");
          bad_flag = TRUE;
        }

      if(bad_flag)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM, "bvolumeWrite(): one or more zero determinants; "
             "aborting register.dat write and writing bhdr instead");
          MRIfree(&subject_info);
        }
    }

  bad_flag = FALSE;

  if(subject_info != NULL)
    {

      if((r1 = MatrixMultiply(bs, ics, NULL)) == NULL)
        {
          bad_flag = TRUE;
          MRIfree(&subject_info);
        }

    }

  if(subject_info != NULL)
    {

      if((r2 = MatrixMultiply(as, r1, NULL)) == NULL)
        {
          bad_flag = TRUE;
          MRIfree(&subject_info);
        }

    }

  if(subject_info != NULL)
    {

      if((r3 = MatrixMultiply(iaf, r2, NULL)) == NULL)
        {
          bad_flag = TRUE;
          MRIfree(&subject_info);
        }

    }

  if(subject_info != NULL)
    {

      if((r4 = MatrixMultiply(ibf, r3, NULL)) == NULL)
        {
          bad_flag = TRUE;
          MRIfree(&subject_info);
        }

    }

  if(subject_info != NULL)
    {

      if((r = MatrixMultiply(cf, r4, NULL)) == NULL)
        {
          bad_flag = TRUE;
          MRIfree(&subject_info);
        }

    }

  if(bad_flag)
    {
      errno = 0;
      ErrorPrintf
        (ERROR_BADPARM, "bvolumeWrite(): error during matrix multiplications; "
         "aborting register.dat write and writing bhdr instead");
    }

  if( as != NULL)  MatrixFree( &as);
  if( bs != NULL)  MatrixFree( &bs);
  if( cs != NULL)  MatrixFree( &cs);
  if( af != NULL)  MatrixFree( &af);
  if( bf != NULL)  MatrixFree( &bf);
  if( cf != NULL)  MatrixFree( &cf);
  if(iaf != NULL)  MatrixFree(&iaf);
  if(ibf != NULL)  MatrixFree(&ibf);
  if(ics != NULL)  MatrixFree(&ics);
  if( r1 != NULL)  MatrixFree( &r1);
  if( r2 != NULL)  MatrixFree( &r2);
  if( r3 != NULL)  MatrixFree( &r3);
  if( r4 != NULL)  MatrixFree( &r4);

  if(subject_info != NULL)
    {

      if(mri->path_to_t1[0] == '\0')
        sprintf(t1_path, ".");
      else
        strcpy(t1_path, mri->path_to_t1);

      if(FileExists(analyse_fname))
        fprintf(stderr, "warning: overwriting file %s\n", analyse_fname);

      if((fp = fopen(analyse_fname, "w")) == NULL)
        {
          MRIfree(&subject_info);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "bvolumeWrite(): couldn't open file %s for writing",
              analyse_fname));
        }

      fprintf(fp, "%s\n", t1_path);
      fprintf(fp, "%s_%%03d.bshort\n", stem);
      fprintf(fp, "%d %d\n", mri->depth, mri->nframes);
      fprintf(fp, "%d %d\n", mri->width, mri->height);

      fclose(fp);

      if(FileExists(analyse_fname))
        fprintf(stderr, "warning: overwriting file %s\n", register_fname);

      if((fp = fopen(register_fname, "w")) == NULL)
        {
          MRIfree(&subject_info);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "bvolumeWrite(): couldn't open file %s for writing",
              register_fname));
        }

      fprintf(fp, "%s\n", sn);
      fprintf(fp, "%g\n", mri->xsize);
      fprintf(fp, "%g\n", mri->zsize);
      fprintf(fp, "%g\n", 1.0);
      fprintf(fp, "%g %g %g %g\n", \
              *MATRIX_RELT(r, 1, 1),
              *MATRIX_RELT(r, 1, 2),
              *MATRIX_RELT(r, 1, 3),
              *MATRIX_RELT(r, 1, 4));
      fprintf(fp, "%g %g %g %g\n", \
              *MATRIX_RELT(r, 2, 1),
              *MATRIX_RELT(r, 2, 2),
              *MATRIX_RELT(r, 2, 3),
              *MATRIX_RELT(r, 2, 4));
      fprintf(fp, "%g %g %g %g\n", \
              *MATRIX_RELT(r, 3, 1),
              *MATRIX_RELT(r, 3, 2),
              *MATRIX_RELT(r, 3, 3),
              *MATRIX_RELT(r, 3, 4));
      fprintf(fp, "%g %g %g %g\n", \
              *MATRIX_RELT(r, 4, 1),
              *MATRIX_RELT(r, 4, 2),
              *MATRIX_RELT(r, 4, 3),
              *MATRIX_RELT(r, 4, 4));

      fclose(fp);

      MatrixFree(&r);

    }

  if(subject_info == NULL)
    {
      sprintf(fname, "%s.bhdr", stem);
      if((fp = fopen(fname, "w")) == NULL)
        {
          if(dealloc) MRIfree(&mri);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE, "bvolumeWrite(): can't open file %s", fname));
        }

      result = write_bhdr(mri, fp);

      fclose(fp);

      if(result != NO_ERROR)
        return(result);

    }
  else
    MRIfree(&subject_info);

  if(dealloc) MRIfree(&mri);

  return(NO_ERROR);

} /* end bvolumeWrite() */

static MRI *get_b_info
(char *fname_passed, int read_volume, char *directory, char *stem, int type)
{

  MRI *mri, *mri2;
  FILE *fp;
  int nslices=0, nt;
  int nx, ny;
  int result;
  char fname[STRLEN];
  char extension[STRLEN];
  char bhdr_name[STRLEN];

  if(type == MRI_SHORT)      sprintf(extension, "bshort");
  else if(type == MRI_FLOAT) sprintf(extension, "bfloat");
  else{
    errno = 0;
    ErrorReturn(NULL, (ERROR_UNSUPPORTED,
                       "internal error: get_b_info() passed type %d", type));
  }

  result = decompose_b_fname(fname_passed, directory, stem);
  if(result != NO_ERROR) return(NULL);

  if(directory[0] == '\0') sprintf(directory, ".");

#if 0

  char sn[STRLEN];
  char fname_descrip[STRLEN];
  float fov_x, fov_y, fov_z;
  MATRIX *m;
  int res;
  char register_fname[STRLEN], analyse_fname[STRLEN];
  float ipr, st, intensity;
  float m11, m12, m13, m14;
  float m21, m22, m23, m24;
  float m31, m32, m33, m34;
  float m41, m42, m43, m44;
  char t1_path[STRLEN];

  /* ----- try register.dat and analyse.dat, then bhdr, then defaults ----- */
  sprintf(register_fname, "%s/register.dat", directory);
  sprintf(analyse_fname, "%s/analyse.dat", directory);

  if((fp = fopen(register_fname, "r")) != NULL)
    {

      fscanf(fp, "%s", sn);
      fscanf(fp, "%f", &ipr);
      fscanf(fp, "%f", &st);
      fscanf(fp, "%f", &intensity);
      fscanf(fp, "%f %f %f %f", &m11, &m12, &m13, &m14);
      fscanf(fp, "%f %f %f %f", &m21, &m22, &m23, &m24);
      fscanf(fp, "%f %f %f %f", &m31, &m32, &m33, &m34);
      fscanf(fp, "%f %f %f %f", &m41, &m42, &m43, &m44);
      fclose(fp);

      if((fp = fopen(analyse_fname, "r")) != NULL)
        {

          fscanf(fp, "%s", t1_path);
          fscanf(fp, "%s", fname_descrip);
          fscanf(fp, "%d %d", &nslices, &nt);
          fscanf(fp, "%d %d", &nx, &ny);
          fclose(fp);

          if(read_volume)
            {
              mri = MRIallocSequence(nx, ny, nslices, MRI_SHORT, nt);
            }
          else
            {
              mri = MRIallocHeader(nx, ny, nslices, MRI_SHORT);
              mri->nframes = nt;
            }

          strcpy(mri->fname, fname_passed);
          mri->imnr0 = 1;
          mri->imnr1 = nslices;
          mri->xsize = mri->ysize = mri->ps = ipr;
          mri->zsize = mri->thick = st;

          fov_x = mri->xsize * mri->width;
          fov_y = mri->ysize * mri->height;
          fov_z = mri->zsize * mri->depth;

          mri->fov = (fov_x > fov_y ? (fov_x > fov_z ? fov_x : fov_z) :
                      (fov_y > fov_z ? fov_y : fov_z));

          mri->xend = fov_x / 2.0;
          mri->xstart = -mri->xend;
          mri->yend = fov_y / 2.0;
          mri->ystart = -mri->yend;
          mri->zend = fov_z / 2.0;
          mri->zstart = -mri->zend;

          mri->brightness = intensity;
          strcpy(mri->subject_name, sn);
          strcpy(mri->path_to_t1, t1_path);
          strcpy(mri->fname_format, fname_descrip);

          m = MatrixAlloc(4, 4, MATRIX_REAL);
          if(m == NULL)
            {
              MRIfree(&mri);
              errno = 0;
              ErrorReturn(NULL, (ERROR_NO_MEMORY, "error allocating "
                                 "matrix in %s read", extension));
            }
          stuff_four_by_four(m, m11, m12, m13, m14,
                             m21, m22, m23, m24,
                             m31, m32, m33, m34,
                             m41, m42, m43, m44);
          mri->register_mat = m;
          res = orient_with_register(mri);
          if(res != NO_ERROR){
            MRIfree(&mri);
            return(NULL);
          }
          return(mri);
        }
    }
#endif

  mri = MRIallocHeader(1, 1, 1, type);

  /* ----- try to read the stem.bhdr ----- */
  sprintf(bhdr_name, "%s/%s.bhdr", directory, stem);
  if((fp = fopen(bhdr_name, "r")) != NULL)
    {
      read_bhdr(mri, fp);
      sprintf(fname, "%s/%s_000.hdr", directory, stem);
      if((fp = fopen(fname, "r")) == NULL){
        MRIfree(&mri);
        errno = 0;
        ErrorReturn(NULL, (ERROR_BADFILE, "cannot open %s", fname));
      }
      fscanf(fp, "%d %d %d %*d", &ny, &nx, &nt);
      mri->nframes = nt;
      fclose(fp);
    }
  else
    { /* ----- get defaults ----- */
      fprintf
        (stderr,
         "-----------------------------------------------------------------\n"
         "Could not find the direction cosine information.\n"
         "Will use the CORONAL orientation.\n"
         "If not suitable, please provide the information in %s file\n"
         "-----------------------------------------------------------------\n",
         bhdr_name);
      sprintf(fname, "%s/%s_000.hdr", directory, stem);
      if((fp = fopen(fname, "r")) == NULL)
        {
          MRIfree(&mri);
          errno = 0;
          ErrorReturn(NULL, (ERROR_BADFILE, "can't find file %s (last resort);"
                             "bailing out on read", fname));
        }
      fscanf(fp, "%d %d %d %*d", &ny, &nx, &nt);
      fclose(fp);

      /* --- get the number of slices --- */
      sprintf(fname, "%s/%s_000.%s", directory, stem, extension);
      if(!FileExists(fname)){
        MRIfree(&mri);
        errno = 0;
        ErrorReturn(NULL, (ERROR_BADFILE, "can't find file %s; "
                           "bailing out on read", fname));
      }
      for(nslices = 0;FileExists(fname);nslices++)
        sprintf(fname, "%s/%s_%03d.%s", directory, stem, nslices, extension);
      nslices--;

      mri->width   = nx;
      mri->height  = ny;
      mri->depth   = nslices;
      mri->nframes = nt;

      mri->imnr0 = 1;
      mri->imnr1 = nslices;

      mri->thick = mri->ps = 1.0;
      mri->xsize = mri->ysize = mri->zsize = 1.0;

      setDirectionCosine(mri, MRI_CORONAL);

      mri->ras_good_flag = 0;

      strcpy(mri->fname, fname_passed);

    }

  mri->imnr0 = 1;
  mri->imnr1 = nslices;

  mri->xend = mri->width  * mri->xsize / 2.0;
  mri->xstart = -mri->xend;
  mri->yend = mri->height * mri->ysize / 2.0;
  mri->ystart = -mri->yend;
  mri->zend = mri->depth  * mri->zsize / 2.0;
  mri->zstart = -mri->zend;

  mri->fov = ((mri->xend - mri->xstart) > (mri->yend - mri->ystart) ?
              (mri->xend - mri->xstart) : (mri->yend - mri->ystart));


  if(read_volume)
    {
      mri2 = MRIallocSequence(mri->width, mri->height, mri->depth,
                              mri->type, mri->nframes);
      MRIcopyHeader(mri, mri2);
      MRIfree(&mri);
      mri = mri2;
    }

  return(mri);

} /* end get_b_info() */

/*-------------------------------------------------------------------
  bvolumeRead() - this replaces bshortRead and bfloatRead.
  -------------------------------------------------------------------*/
static MRI *bvolumeRead(char *fname_passed, int read_volume, int type)
{

  MRI *mri;
  FILE *fp;
  char fname[STRLEN];
  char directory[STRLEN];
  char stem[STRLEN];
  int swap_bytes_flag;
  int slice, frame, row, k;
  int nread;
  char *ext;
  int size;
  float min, max;

  /* check the type and set the extension and size*/
  switch(type) {
  case MRI_SHORT: ext = "bshort"; size = sizeof(short); break;
  case MRI_FLOAT: ext = "bfloat"; size = sizeof(float); break;
  default:
    fprintf(stderr,"ERROR: bvolumeRead: type (%d) is not "
            "short or float\n", type);
    return(NULL);
  }

  /* Get the header info (also allocs if needed) */
  mri = get_b_info(fname_passed, read_volume, directory, stem, type);
  if(mri == NULL) return(NULL);

  /* If not reading the volume, return now */
  if(! read_volume) return(mri);

  /* Read in the header of the first slice to get the endianness */
  sprintf(fname, "%s/%s_%03d.hdr", directory, stem, 0);
  if((fp = fopen(fname, "r")) == NULL){
    fprintf(stderr, "ERROR: can't open file %s; assuming big-endian bvolume\n",
            fname);
    swap_bytes_flag = 0;
  }
  else{
    fscanf(fp, "%*d %*d %*d %d", &swap_bytes_flag);
#if (BYTE_ORDER == LITTLE_ENDIAN)
    swap_bytes_flag = !swap_bytes_flag;
#endif
    fclose(fp);
  }

  /* Go through each slice */
  for(slice = 0;slice < mri->depth; slice++){

    /* Open the file for this slice */
    sprintf(fname, "%s/%s_%03d.%s", directory, stem, slice, ext);
    if((fp = fopen(fname, "r")) == NULL){
      MRIfree(&mri);
      errno = 0;
      ErrorReturn(NULL, (ERROR_BADFILE,
                         "bvolumeRead(): error opening file %s", fname));
    }
    //fprintf(stderr, "Reading %s ... \n", fname);
    /* Loop through the frames */
    for(frame = 0; frame < mri->nframes; frame ++){
      k = slice + mri->depth*frame;

      /* Loop through the rows */
      for(row = 0;row < mri->height; row++){

        /* read in all the columns for a row */
        nread = fread(mri->slices[k][row], size, mri->width, fp);
        if( nread != mri->width){
          fclose(fp);
          MRIfree(&mri);
          errno = 0;
          ErrorReturn(NULL, (ERROR_BADFILE, "bvolumeRead(): "
                             "error reading from file %s", fname));
        }

        if(swap_bytes_flag){
          if(type == MRI_SHORT)
            swab(mri->slices[k][row], mri->slices[k][row], mri->width * size);
          else
            byteswapbuffloat((void*)mri->slices[k][row],size*mri->width);
        }
      } /* row loop */
    } /* frame loop */

    fclose(fp);
  } /* slice loop */

  MRIlimits(mri,&min,&max);
  printf("INFO: bvolumeRead: min = %g, max = %g\n",min,max);

  mri->imnr0 = 1;
  mri->imnr1 = mri->depth;
  mri->thick = mri->zsize;

  return(mri);

} /* end bvolumeRead() */


#if 0
static int orient_with_register(MRI *mri)
{

  MRI *subject_mri;
  char *subjects_dir;
  char subject_directory[STRLEN];
  MATRIX *sa, *fa;
  MATRIX *sr, *fr;
  MATRIX *rinv, *sainv;
  MATRIX *r1, *r2;
  int res;
  float det;

  subject_mri = NULL;
  if((subjects_dir = getenv("SUBJECTS_DIR")) != NULL)
    {
      sprintf
        (subject_directory, "%s/%s/mri/T1", subjects_dir, mri->subject_name);
      if((subject_mri = MRIreadInfo(subject_directory)) == NULL)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "can't get get information from %s; ", subject_directory);
          sprintf
            (subject_directory,
             "%s/%s/mri/T1", subjects_dir, mri->subject_name);
          ErrorPrintf
            (ERROR_BADPARM, "trying %s instead...\n", subject_directory);
          if((subject_mri = MRIreadInfo(subject_directory)) == NULL)
            ErrorPrintf
              (ERROR_BADPARM,
               "can't get information from %s\n", subject_directory);
        }
    }
  else
    {
      errno = 0;
      ErrorPrintf
        (ERROR_BADPARM, "can't get environment variable SUBJECTS_DIR");
    }

  if(subject_mri == NULL)
    {

      errno = 0;
      ErrorPrintf(ERROR_BADPARM, "guessing at COR- orientation...\n");

      subject_mri = MRIallocHeader(256, 256, 256, MRI_UCHAR);
      subject_mri->fov = 256.0;
      subject_mri->thick = subject_mri->ps = 1.0;
      subject_mri->xsize = subject_mri->ysize = subject_mri->zsize = 1.0;
      setDirectionCosine(subject_mri, MRI_CORONAL);
      subject_mri->ras_good_flag = 1;
    }

  det = MatrixDeterminant(mri->register_mat);
  if(det == 0.0)
    {
      MRIfree(&subject_mri);
      errno = 0;
      ErrorPrintf
        (ERROR_BADPARM,
         "orient_with_register(): registration matrix has zero determinant");
      ErrorPrintf(ERROR_BADPARM, "matrix is:");
      MatrixPrint(stderr, mri->register_mat);
      return(ERROR_BADPARM);
    }

  rinv = MatrixInverse(mri->register_mat, NULL);

  if(rinv == NULL)
    {
      MRIfree(&subject_mri);
      errno = 0;
      ErrorPrintf
        (ERROR_BADPARM,
         "orient_with_register(): error inverting registration matrix");
      ErrorPrintf(ERROR_BADPARM, "matrix is:");
      MatrixPrint(stderr, mri->register_mat);
      return(ERROR_BADPARM);
    }

  sr = extract_i_to_r(subject_mri);

  sa = MatrixAlloc(4, 4, MATRIX_REAL);
  fa = MatrixAlloc(4, 4, MATRIX_REAL);

  if(sr == NULL || sa == NULL || fa == NULL)
    {
      if(sr != NULL)
        MatrixFree(&sr);
      if(sa != NULL)
        MatrixFree(&sa);
      if(fa != NULL)
        MatrixFree(&fa);
      MatrixFree(&rinv);
      MRIfree(&subject_mri);
      errno = 0;
      ErrorReturn
        (ERROR_NO_MEMORY,
         (ERROR_NO_MEMORY,
          "orient_with_register(): error allocating matrix"));
    }

  stuff_four_by_four
    (sa, -subject_mri->xsize, 0.0, 0.0, subject_mri->width * subject_mri->xsize / 2.0,
     0.0, 0.0, subject_mri->zsize, -subject_mri->depth * subject_mri->zsize / 2.0,
     0.0, -subject_mri->ysize, 0.0, subject_mri->height * subject_mri->ysize / 2.0,
     0.0, 0.0, 0.0, 1.0);

  MRIfree(&subject_mri);

  stuff_four_by_four(fa, -mri->xsize, 0.0, 0.0, mri->width * mri->xsize / 2.0,
                     0.0, 0.0, mri->zsize, -mri->depth * mri->zsize / 2.0,
                     0.0, -mri->ysize, 0.0, mri->height * mri->ysize / 2.0,
                     0.0, 0.0, 0.0, 1.0);
  det = MatrixDeterminant(sa);
  if(det == 0.0)
    {
      MatrixFree(&sr);
      MatrixFree(&sa);
      MatrixFree(&fa);
      MatrixFree(&rinv);
      errno = 0;
      ErrorPrintf(ERROR_BADPARM,
                  "orient_with_register(): destination (ijk) -> r "
                  "space matrix has zero determinant");
      ErrorPrintf(ERROR_BADPARM, "matrix is:");
      MatrixPrint(stderr, sa);
      return(ERROR_BADPARM);
    }

  sainv = MatrixInverse(sa, NULL);
  MatrixFree(&sa);

  if(sainv == NULL)
    {
      MatrixFree(&sr);
      MatrixFree(&sa);
      MatrixFree(&fa);
      MatrixFree(&rinv);
      errno = 0;
      ErrorPrintf
        (ERROR_BADPARM,
         "orient_with_register(): error inverting "
         "destination (ijk) -> r space matrix");
      ErrorPrintf(ERROR_BADPARM, "matrix is:");
      MatrixPrint(stderr, sainv);
      return(ERROR_BADPARM);
    }

  r1 = MatrixMultiply(rinv, fa, NULL);
  MatrixFree(&rinv);
  MatrixFree(&fa);
  if(r1 == NULL)
    {
      MatrixFree(&sr);
      MatrixFree(&sa);
      MatrixFree(&sainv);
      errno = 0;
      ErrorReturn(ERROR_BADPARM,
                  (ERROR_BADPARM,
                   "orient_with_register(): error multiplying matrices"));
    }

  r2 = MatrixMultiply(sainv, r1, NULL);
  MatrixFree(&r1);
  MatrixFree(&sainv);
  if(r2 == NULL)
    {
      MatrixFree(&sr);
      MatrixFree(&sa);
      errno = 0;
      ErrorReturn(ERROR_BADPARM,
                  (ERROR_BADPARM,
                   "orient_with_register(): error multiplying matrices"));
    }

  fr = MatrixMultiply(sr, r2, NULL);
  MatrixFree(&sr);
  MatrixFree(&r2);
  if(fr == NULL)
    {
      errno = 0;
      ErrorReturn(ERROR_BADPARM,
                  (ERROR_BADPARM,
                   "orient_with_register(): error multiplying matrices"));
    }

  res = apply_i_to_r(mri, fr);
  MatrixFree(&fr);

  if(res != NO_ERROR)
    return(res);

  return(NO_ERROR);

} /* end orient_with_register() */
#endif

int decompose_b_fname(char *fname, char *dir, char *stem)
{

  char *slash, *dot, *stem_start, *underscore;
  int fname_length;
  int und_pos;
  char fname_copy[STRLEN];

  /*

  options for file names:

  stem_xxx.bshort
  stem_xxx
  stem_.bshort
  stem_
  stem.bshort
  stem

  with or without a preceding directory

  */

  strcpy(fname_copy, fname);

  slash = strrchr(fname_copy, '/');
  dot = strrchr(fname_copy, '.');
  underscore = strrchr(fname_copy, '_');

  if(slash == NULL)
    {
      stem_start = fname_copy;
      sprintf(dir, ".");
    }
  else
    {
      *slash = '\0';
      strcpy(dir, fname_copy);
      stem_start = slash + 1;
    }

  if(*stem_start == '\0')
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "decompose_b_fname(): bad bshort/bfloat file specifier \"%s\"",
          fname));
    }

  if(dot != NULL)
    if(strcmp(dot, ".bshort") == 0 || strcmp(dot, ".bfloat") == 0)
      *dot = '\0';

  fname_length = strlen(stem_start);

  if(underscore == NULL)
    strcpy(stem, stem_start);
  else
    {
      und_pos = (underscore - stem_start);
      if(und_pos == fname_length - 1 || und_pos == fname_length - 4)
        *underscore = '\0';
      strcpy(stem, stem_start);
    }

  return(NO_ERROR);

} /* end decompose_b_fname() */

/*-------------------------------------------------------------*/
static int write_bhdr(MRI *mri, FILE *fp)
{
  float vl;            /* vector length */
  float tlr, tla, tls; /* top left coordinates */
  float trr, tra, trs; /* top right coordinates */
  float brr, bra, brs; /* bottom right coordinates */
  float nr, na, ns;    /* normal coordinates */
  MATRIX *T, *crs1, *ras1;

  crs1 = MatrixAlloc(4,1,MATRIX_REAL);
  crs1->rptr[1][4] = 1;
  ras1 = MatrixAlloc(4,1,MATRIX_REAL);

  /* Construct the matrix to convert CRS to XYZ, assuming
     that CRS is 1-based */
  T = MRIxfmCRS2XYZ(mri,0);
  //printf("------- write_bhdr: T ---------\n");
  //MatrixPrint(stdout,T);
  //printf("------------------------------\n");

  /* The "top left" is the center of the first voxel */
  tlr = T->rptr[1][4];
  tla = T->rptr[2][4];
  tls = T->rptr[3][4];

  /* The "top right" is the center of the last voxel + 1 in the
     column direction - this unfortunate situation is historical.
     It makes the difference between the TL and TR equal to the
     edge-to-edge FOV in the column direction. */
  crs1->rptr[1][1] = mri->width;
  crs1->rptr[1][2] = 0;
  crs1->rptr[1][3] = 0;
  MatrixMultiply(T,crs1,ras1);
  trr = ras1->rptr[1][1];
  tra = ras1->rptr[1][2];
  trs = ras1->rptr[1][3];

  /* The "bottom right" is the center of the last voxel + 1 in the
     column and row directions - this unfortunate situation is
     historical. It makes the difference between the TR and BR equal
     to the edge-to-edge FOV in the row direction. */
  crs1->rptr[1][1] = mri->width;
  crs1->rptr[1][2] = mri->height;
  crs1->rptr[1][3] = 0;
  MatrixMultiply(T,crs1,ras1);
  brr = ras1->rptr[1][1];
  bra = ras1->rptr[1][2];
  brs = ras1->rptr[1][3];

  MatrixFree(&T);
  MatrixFree(&crs1);
  MatrixFree(&ras1);

  /* ----- normalize this just in case ----- */
  vl = sqrt(mri->z_r*mri->z_r + mri->z_a*mri->z_a + mri->z_s*mri->z_s);
  nr = mri->z_r / vl;
  na = mri->z_a / vl;
  ns = mri->z_s / vl;

#if 0
  time_t time_now;
  float cr, ca, cs;    /* first slice center coordinates */
  cr = mri->c_r - (mri->depth - 1) / 2.0 * mri->z_r * mri->zsize;
  ca = mri->c_a - (mri->depth - 1) / 2.0 * mri->z_a * mri->zsize;
  cs = mri->c_s - (mri->depth - 1) / 2.0 * mri->z_s * mri->zsize;

  tlr = cr - mri->width / 2.0 * mri->x_r * mri->xsize - mri->height / 2.0 *
    mri->y_r * mri->ysize;
  tla = ca - mri->width / 2.0 * mri->x_a * mri->xsize - mri->height / 2.0 *
    mri->y_a * mri->ysize;
  tls = cs - mri->width / 2.0 * mri->x_s * mri->xsize - mri->height / 2.0 *
    mri->y_s * mri->ysize;

  trr = cr + mri->width / 2.0 * mri->x_r * mri->xsize - mri->height / 2.0 *
    mri->y_r * mri->ysize;
  tra = ca + mri->width / 2.0 * mri->x_a * mri->xsize - mri->height / 2.0 *
    mri->y_a * mri->ysize;
  trs = cs + mri->width / 2.0 * mri->x_s * mri->xsize - mri->height / 2.0 *
    mri->y_s * mri->ysize;

  brr = cr + mri->width / 2.0 * mri->x_r * mri->xsize + mri->height / 2.0 *
    mri->y_r * mri->ysize;
  bra = ca + mri->width / 2.0 * mri->x_a * mri->xsize + mri->height / 2.0 *
    mri->y_a * mri->ysize;
  brs = cs + mri->width / 2.0 * mri->x_s * mri->xsize + mri->height / 2.0 *
    mri->y_s * mri->ysize;

  time(&time_now);

  //fprintf(fp, "bhdr generated by %s\n", Progname);
  //fprintf(fp, "%s\n", ctime(&time_now));
  //fprintf(fp, "\n");
#endif

  fprintf(fp, "          cols: %d\n", mri->width);
  fprintf(fp, "          rows: %d\n", mri->height);
  fprintf(fp, "       nslices: %d\n", mri->depth);
  fprintf(fp, " n_time_points: %d\n", mri->nframes);
  fprintf(fp, "   slice_thick: %f\n", mri->zsize);
  fprintf(fp, "    top_left_r: %f\n", tlr);
  fprintf(fp, "    top_left_a: %f\n", tla);
  fprintf(fp, "    top_left_s: %f\n", tls);
  fprintf(fp, "   top_right_r: %f\n", trr);
  fprintf(fp, "   top_right_a: %f\n", tra);
  fprintf(fp, "   top_right_s: %f\n", trs);
  fprintf(fp, "bottom_right_r: %f\n", brr);
  fprintf(fp, "bottom_right_a: %f\n", bra);
  fprintf(fp, "bottom_right_s: %f\n", brs);
  fprintf(fp, "      normal_r: %f\n", nr);
  fprintf(fp, "      normal_a: %f\n", na);
  fprintf(fp, "      normal_s: %f\n", ns);
  fprintf(fp, "      image_te: %f\n", mri->te);
  fprintf(fp, "      image_tr: %f\n", mri->tr/1000.0); // convert to sec
  fprintf(fp, "      image_ti: %f\n", mri->ti);
  fprintf(fp, "    flip_angle: %f\n", mri->flip_angle);

  return(NO_ERROR);

} /* end write_bhdr() */

/*------------------------------------------------------*/
int read_bhdr(MRI *mri, FILE *fp)
{

  char line[STRLEN];
  char *l;
  float tlr=0.;
  float tla=0.;
  float tls=0.; /* top left coordinates */
  float trr=0.;
  float tra=0.;
  float trs=0.; /* top right coordinates */
  float brr=0.;
  float bra=0.;
  float brs=0.; /* bottom right coordinates */
  float xr=0.;
  float xa=0.;
  float xs=0.;
  float yr=0.;
  float ya=0.;
  float ys=0.;
  MATRIX *T, *CRSCenter, *RASCenter;

  while(1){  // don't use   "while (!feof(fp))"

    /* --- read the line --- */
    fgets(line, STRLEN, fp);

    if (feof(fp))  // wow, it was beyound the end of the file. get out.
      break;

    /* --- remove the newline --- */
    if(line[strlen(line)-1] == '\n')
      line[strlen(line)-1] = '\0';

    /* --- skip the initial spaces --- */
    for(l = line;isspace((int)(*l));l++);

    /* --- get the varible name and value(s) --- */
    if(strlen(l) > 0){
      if(strncmp(l, "cols: ", 6) == 0)
        sscanf(l, "%*s %d", &mri->width);
      else if(strncmp(l, "rows: ", 6) == 0)
        sscanf(l, "%*s %d", &mri->height);
      else if(strncmp(l, "nslices: ", 9) == 0)
        sscanf(l, "%*s %d", &mri->depth);
      else if(strncmp(l, "n_time_points: ", 15) == 0)
        sscanf(l, "%*s %d", &mri->nframes);
      else if(strncmp(l, "slice_thick: ", 13) == 0)
        sscanf(l, "%*s %f", &mri->zsize);
      else if(strncmp(l, "image_te: ", 10) == 0)
        sscanf(l, "%*s %f", &mri->te);
      else if(strncmp(l, "image_tr: ", 10) == 0){
        sscanf(l, "%*s %f", &mri->tr);
        mri->tr = 1000.0 * mri->tr; // convert from sec to msec
      }
      else if(strncmp(l, "image_ti: ", 10) == 0)
        sscanf(l, "%*s %f", &mri->ti);
      else if(strncmp(l, "flip_angle: ", 10) == 0)
        sscanf(l, "%*s %lf", &mri->flip_angle);
      else if(strncmp(l, "top_left_r: ", 12) == 0)
        sscanf(l, "%*s %g", &tlr);
      else if(strncmp(l, "top_left_a: ", 12) == 0)
        sscanf(l, "%*s %g", &tla);
      else if(strncmp(l, "top_left_s: ", 12) == 0)
        sscanf(l, "%*s %g", &tls);
      else if(strncmp(l, "top_right_r: ", 13) == 0)
        sscanf(l, "%*s %g", &trr);
      else if(strncmp(l, "top_right_a: ", 13) == 0)
        sscanf(l, "%*s %g", &tra);
      else if(strncmp(l, "top_right_s: ", 13) == 0)
        sscanf(l, "%*s %g", &trs);
      else if(strncmp(l, "bottom_right_r: ", 16) == 0)
        sscanf(l, "%*s %g", &brr);
      else if(strncmp(l, "bottom_right_a: ", 16) == 0)
        sscanf(l, "%*s %g", &bra);
      else if(strncmp(l, "bottom_right_s: ", 16) == 0)
        sscanf(l, "%*s %g", &brs);
      else if(strncmp(l, "normal_r: ", 10) == 0)
        sscanf(l, "%*s %g", &mri->z_r);
      else if(strncmp(l, "normal_a: ", 10) == 0)
        sscanf(l, "%*s %g", &mri->z_a);
      else if(strncmp(l, "normal_s: ", 10) == 0)
        sscanf(l, "%*s %g", &mri->z_s);
      else { /* --- ignore it --- */ }
    }
  } /* end while(!feof()) */
  // forget to close file
  fclose(fp);

  //  vox to ras matrix is
  //
  //  Xr*Sx  Yr*Sy  Zr*Sz  Tr
  //  Xa*Sx  Ya*Sy  Za*Sz  Ta
  //  Xs*Sx  Ys*Sy  Zs*Sz  Ts
  //    0      0      0    1
  //
  //  Therefore
  //
  //  trr = Xr*Sx*W + Tr, tlr = Tr
  //  tra = Xa*Sx*W + Ta, tla = Ta
  //  trs = Xs*Sx*W + Ts, tls = Ts
  //
  //  Thus
  //
  //  Sx = sqrt ( ((trr-tlr)/W)^2 + ((tra-tla)/W)^2 + ((trs-tls)/W)^2)
  //     since Xr^2 + Xa^2 + Xs^2 = 1
  //  Xr = (trr-tlr)/(W*Sx)
  //  Xa = (tra-tla)/(W*Sx)
  //  Xs = (trs-tls)/(W*Sx)
  //
  //  Similar things for others
  //
  xr = (trr - tlr) / mri->width;
  xa = (tra - tla) / mri->width;
  xs = (trs - tls) / mri->width;
  mri->xsize = sqrt(xr*xr + xa*xa + xs*xs);
  if (mri->xsize) // avoid nan
    {
      mri->x_r = xr / mri->xsize;
      mri->x_a = xa / mri->xsize;
      mri->x_s = xs / mri->xsize;
    }
  else // fake values
    {
      mri->xsize = 1;
      mri->x_r = -1;
      mri->x_a = 0;
      mri->x_s = 0;
    }
  yr = (brr - trr) / mri->height;
  ya = (bra - tra) / mri->height;
  ys = (brs - trs) / mri->height;
  mri->ysize = sqrt(yr*yr + ya*ya + ys*ys);
  if (mri->ysize) // avoid nan
    {
      mri->y_r = yr / mri->ysize;
      mri->y_a = ya / mri->ysize;
      mri->y_s = ys / mri->ysize;
    }
  else // fake values
    {
      mri->ysize = 1;
      mri->y_r = 0;
      mri->y_a = 0;
      mri->y_s = -1;
    }
  T = MRIxfmCRS2XYZ(mri,0);

  T->rptr[1][4] = tlr;
  T->rptr[2][4] = tla;
  T->rptr[3][4] = tls;

  //printf("------- read_bhdr: T ---------\n");
  //MatrixPrint(stdout,T);
  //printf("------------------------------\n");

  CRSCenter = MatrixAlloc(4,1,MATRIX_REAL);
  CRSCenter->rptr[1][1] = (mri->width)/2.0;
  CRSCenter->rptr[2][1] = (mri->height)/2.0;
  CRSCenter->rptr[3][1] = (mri->depth)/2.0;
  CRSCenter->rptr[4][1] = 1;

  RASCenter = MatrixMultiply(T,CRSCenter,NULL);
  mri->c_r = RASCenter->rptr[1][1];
  mri->c_a = RASCenter->rptr[2][1];
  mri->c_s = RASCenter->rptr[3][1];

  MatrixFree(&T);
  MatrixFree(&CRSCenter);
  MatrixFree(&RASCenter);

#if 0
  /* This computation of the center is incorrect because TL is the center of
     the first voxel (not the edge of the FOV). TR and BR are actually
     outside of the FOV.  */
  mri->c_r =
    (brr + tlr) / 2.0 + (mri->depth - 1) * mri->z_r * mri->zsize / 2.0 ;
  mri->c_a =
    (bra + tla) / 2.0 + (mri->depth - 1) * mri->z_a * mri->zsize / 2.0 ;
  mri->c_s =
    (brs + tls) / 2.0 + (mri->depth - 1) * mri->z_s * mri->zsize / 2.0 ;
#endif

  mri->ras_good_flag = 1;

  mri->thick = mri->zsize;
  mri->ps = mri->xsize;

  return(NO_ERROR);

} /* end read_bhdr() */


static MRI *genesisRead(char *fname, int read_volume)
{
  char fname_format[STRLEN];
  char fname_format2[STRLEN];
  char fname_dir[STRLEN];
  char fname_base[STRLEN];
  char *c;
  MRI *mri = NULL;
  int im_init;
  int im_low, im_high;
  int im_low2, im_high2;
  char fname_use[STRLEN];
  char temp_string[STRLEN];
  FILE *fp;
  int width, height;
  int pixel_data_offset;
  int image_header_offset;
  float tl_r, tl_a, tl_s;
  float tr_r, tr_a, tr_s;
  float br_r, br_a, br_s;
  float c_r, c_a, c_s;
  float n_r, n_a, n_s;
  float xlength, ylength, zlength;
  int i, y;
  MRI *header;
  float xfov, yfov, zfov;
  float nlength;
  int twoformats = 0, odd_only, even_only ;

  odd_only = even_only = 0 ;
  if (getenv("GE_ODD"))
    {
      odd_only = 1 ;
      printf("only using odd # GE files\n") ;
    }
  else if (getenv("GE_EVEN"))
    {
      even_only = 1 ;
      printf("only using even # GE files\n") ;
    }

  /* ----- check the first (passed) file ----- */
  if(!FileExists(fname))
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE, "genesisRead(): error opening file %s", fname));
    }

  /* ----- split the file name into name and directory ----- */
  c = strrchr(fname, '/');
  if(c == NULL)
    {
      fname_dir[0] = '\0';
      strcpy(fname_base, fname);
    }
  else
    {
      strncpy(fname_dir, fname, (c - fname + 1));
      fname_dir[c-fname+1] = '\0';
      strcpy(fname_base, c+1);
    }

  /* ----- derive the file name format (for sprintf) ----- */
  // this one fix fname_format only
  if(strncmp(fname_base, "I.", 2) == 0)
    {
      twoformats = 0;
      im_init = atoi(&fname_base[2]);
      sprintf(fname_format, "I.%%03d");
    }
  // this one fix both fname_format and fname_format2
  else if(strlen(fname_base) >= 3) /* avoid core dumps below... */
    {
      twoformats = 1;
      c = &fname_base[strlen(fname_base)-3];
      if(strcmp(c, ".MR") == 0)
        {
          *c = '\0';
          for(c--;isdigit(*c) && c >= fname_base;c--);
          c++;
          im_init = atoi(c);
          *c = '\0';
          // this is too quick to assume of this type
          // another type %s%%03d.MR" must be examined
          sprintf(fname_format, "%s%%d.MR", fname_base);
          sprintf(fname_format2, "%s%%03d.MR", fname_base);
        }
      else
        {
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADPARM,
              "genesisRead(): can't determine file name format for %s",
              fname));
        }
    }
  else
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADPARM,
          "genesisRead(): can't determine file name format for %s", fname));
    }

  if (strlen(fname_format) != 0)
    {
      strcpy(temp_string, fname_format);
      sprintf(fname_format, "%s%s", fname_dir, temp_string);
      printf("fname_format  : %s\n", fname_format);
    }
  if (strlen(fname_format2) != 0)
    {
      strcpy(temp_string, fname_format2);
      sprintf(fname_format2, "%s%s", fname_dir, temp_string);
      printf("fname_format2 : %s\n", fname_format2);
    }
  /* ----- find the low and high files ----- */
  if (odd_only || even_only)
    {
      if ((odd_only && ISEVEN(im_init)) ||
          (even_only && ISODD(im_init)))
        im_init++ ;
      im_low = im_init;
      do
        {
          im_low -=2 ;
          sprintf(fname_use, fname_format, im_low);
        } while(FileExists(fname_use));
      im_low += 2 ;

      im_high = im_init;
      do
        {
          im_high += 2 ;
          sprintf(fname_use, fname_format, im_high);
        } while(FileExists(fname_use));
      im_high -=2;
    }
  else
    {
      im_low = im_init;
      do
        {
          im_low--;
          sprintf(fname_use, fname_format, im_low);
        } while(FileExists(fname_use));
      im_low++;

      im_high = im_init;
      do
        {
          im_high++;
          sprintf(fname_use, fname_format, im_high);
        } while(FileExists(fname_use));
      im_high--;
    }

  if (twoformats)
    {
      // now test fname_format2
      im_low2 = im_init;
      do
        {
          im_low2--;
          sprintf(fname_use, fname_format2, im_low2);
        } while(FileExists(fname_use));
      im_low2++;

      im_high2 = im_init;
      do
        {
          im_high2++;
          sprintf(fname_use, fname_format2, im_high2);
        } while(FileExists(fname_use));
      im_high2--;
    }
  else
    {
      im_high2 = im_low2 = 0;
    }
  // now decide which one to pick
  if ((im_high2-im_low2) > (im_high-im_low))
    {
      // we have to use fname_format2
      strcpy(fname_format, fname_format2);
      im_high = im_high2;
      im_low = im_low2;
    }
  // otherwise the same

  /* ----- allocate the mri structure ----- */
  header = MRIallocHeader(1, 1, 1, MRI_SHORT);

  if (odd_only || even_only)
    header->depth = (im_high - im_low)/2 + 1;
  else
    header->depth = im_high - im_low + 1;
  header->imnr0 = 1;
  header->imnr1 = header->depth;

  /* ----- get the header information from the first file ----- */
  sprintf(fname_use, fname_format, im_low);
  if((fp = fopen(fname_use, "r")) == NULL)
    {
      MRIfree(&header);
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE, "genesisRead(): error opening file %s\n", fname_use));
    }

  fseek(fp, 8, SEEK_SET);
  fread(&width, 4, 1, fp);
  width = orderIntBytes(width);
  fread(&height, 4, 1, fp);
  height = orderIntBytes(height);
  fseek(fp, 148, SEEK_SET);
  fread(&image_header_offset, 4, 1, fp);
  image_header_offset = orderIntBytes(image_header_offset);

  header->width = width;
  header->height = height;

  strcpy(header->fname, fname);

  fseek(fp, image_header_offset + 26, SEEK_SET);
  fread(&(header->thick), 4, 1, fp);
  header->thick = orderFloatBytes(header->thick);
  header->zsize = header->thick;

  fseek(fp, image_header_offset + 50, SEEK_SET);
  fread(&(header->xsize), 4, 1, fp);
  header->xsize = orderFloatBytes(header->xsize);
  fread(&(header->ysize), 4, 1, fp);
  header->ysize = orderFloatBytes(header->ysize);
  header->ps = header->xsize;

  /* all in micro-seconds */
#define MICROSECONDS_PER_MILLISECOND 1e3
  fseek(fp, image_header_offset + 194, SEEK_SET);
  header->tr = freadInt(fp)/MICROSECONDS_PER_MILLISECOND ;
  fseek(fp, image_header_offset + 198, SEEK_SET);
  header->ti = freadInt(fp)/MICROSECONDS_PER_MILLISECOND  ;
  fseek(fp, image_header_offset + 202, SEEK_SET);
  header->te = freadInt(fp)/MICROSECONDS_PER_MILLISECOND  ;
  fseek(fp, image_header_offset + 254, SEEK_SET);
  header->flip_angle = RADIANS(freadShort(fp)) ;  /* was in degrees */
  fseek(fp, image_header_offset + 210, SEEK_SET);  // # of echoes
  header->nframes = freadShort(fp) ;
  if (header->nframes > 1)
    {
      printf("multi-echo genesis file detected (%d echoes)...\n",
             header->nframes) ;
    }
  else if (header->nframes == 0)
    {
      printf("zero frames specified in file - setting to 1\n") ;
      header->nframes = 1 ;
    }


  fseek(fp, image_header_offset + 130, SEEK_SET);
  fread(&c_r,  4, 1, fp);  c_r  = orderFloatBytes(c_r);
  fread(&c_a,  4, 1, fp);  c_a  = orderFloatBytes(c_a);
  fread(&c_s,  4, 1, fp);  c_s  = orderFloatBytes(c_s);
  fread(&n_r,  4, 1, fp);  n_r  = orderFloatBytes(n_r);
  fread(&n_a,  4, 1, fp);  n_a  = orderFloatBytes(n_a);
  fread(&n_s,  4, 1, fp);  n_s  = orderFloatBytes(n_s);
  fread(&tl_r, 4, 1, fp);  tl_r = orderFloatBytes(tl_r);
  fread(&tl_a, 4, 1, fp);  tl_a = orderFloatBytes(tl_a);
  fread(&tl_s, 4, 1, fp);  tl_s = orderFloatBytes(tl_s);
  fread(&tr_r, 4, 1, fp);  tr_r = orderFloatBytes(tr_r);
  fread(&tr_a, 4, 1, fp);  tr_a = orderFloatBytes(tr_a);
  fread(&tr_s, 4, 1, fp);  tr_s = orderFloatBytes(tr_s);
  fread(&br_r, 4, 1, fp);  br_r = orderFloatBytes(br_r);
  fread(&br_a, 4, 1, fp);  br_a = orderFloatBytes(br_a);
  fread(&br_s, 4, 1, fp);  br_s = orderFloatBytes(br_s);

  nlength = sqrt(n_r*n_r + n_a*n_a + n_s*n_s);
  n_r = n_r / nlength;
  n_a = n_a / nlength;
  n_s = n_s / nlength;

  if (getenv("KILLIANY_SWAP") != NULL)
    {
      printf("WARNING - swapping normal direction!\n") ;
      n_a *= -1 ;
    }

  header->x_r = (tr_r - tl_r);
  header->x_a = (tr_a - tl_a);
  header->x_s = (tr_s - tl_s);
  header->y_r = (br_r - tr_r);
  header->y_a = (br_a - tr_a);
  header->y_s = (br_s - tr_s);

  /* --- normalize -- the normal vector from the file
     should have length 1, but just in case... --- */
  xlength =
    sqrt(header->x_r*header->x_r +
         header->x_a*header->x_a +
         header->x_s*header->x_s);
  ylength =
    sqrt(header->y_r*header->y_r +
         header->y_a*header->y_a +
         header->y_s*header->y_s);
  zlength = sqrt(n_r*n_r + n_a*n_a + n_s*n_s);

  header->x_r = header->x_r / xlength;
  header->x_a = header->x_a / xlength;
  header->x_s = header->x_s / xlength;
  header->y_r = header->y_r / ylength;
  header->y_a = header->y_a / ylength;
  header->y_s = header->y_s / ylength;
  header->z_r = n_r / zlength;
  header->z_a = n_a / zlength;
  header->z_s = n_s / zlength;

  header->c_r = (tl_r + br_r) / 2.0 + n_r *
    header->zsize * (header->depth - 1.0) / 2.0;
  header->c_a = (tl_a + br_a) / 2.0 + n_a *
    header->zsize * (header->depth - 1.0) / 2.0;
  header->c_s = (tl_s + br_s) / 2.0 + n_s *
    header->zsize * (header->depth - 1.0) / 2.0;

  header->ras_good_flag = 1;

  header->xend = header->xsize * (double)header->width / 2.0;
  header->xstart = -header->xend;
  header->yend = header->ysize * (double)header->height / 2.0;
  header->ystart = -header->yend;
  header->zend = header->zsize * (double)header->depth / 2.0;
  header->zstart = -header->zend;

  xfov = header->xend - header->xstart;
  yfov = header->yend - header->ystart;
  zfov = header->zend - header->zstart;

  header->fov =
    (xfov > yfov ? (xfov > zfov ? xfov : zfov) : (yfov > zfov ? yfov : zfov));

  fclose(fp);

  if(read_volume)
    mri = MRIallocSequence(header->width, header->height,
                           header->depth, header->type, header->nframes);
  else
    mri = MRIallocHeader(header->width, header->height,
                         header->depth, header->type);

  MRIcopyHeader(header, mri);
  MRIfree(&header);

  /* ----- read the volume if required ----- */
  if(read_volume)
    {
      int slice, frame ;
      for(slice = 0, i = im_low;
          i <= im_high;
          (odd_only || even_only) ? i+=2 : i++, slice++)
        {
          frame = (i-im_low) % mri->nframes ;
          sprintf(fname_use, fname_format, i);
          if((fp = fopen(fname_use, "r")) == NULL)
            {
              MRIfree(&mri);
              errno = 0;
              ErrorReturn(NULL,
                          (ERROR_BADFILE,
                           "genesisRead(): error opening file %s", fname_use));
            }

          fseek(fp, 4, SEEK_SET);
          fread(&pixel_data_offset, 4, 1, fp);
          pixel_data_offset = orderIntBytes(pixel_data_offset);
          fseek(fp, pixel_data_offset, SEEK_SET);

          for(y = 0;y < mri->height;y++)
            {
              if(fread(&MRISseq_vox(mri, 0, y, slice, frame),
                       sizeof(short), mri->width, fp) != mri->width)
                {
                  fclose(fp);
                  MRIfree(&mri);
                  errno = 0;
                  ErrorReturn
                    (NULL,
                     (ERROR_BADFILE,
                      "genesisRead(): error reading from file file %s",
                      fname_use));
                }
#if (BYTE_ORDER == LITTLE_ENDIAN)
              //swab(mri->slices[slice][y],
              // mri->slices[slice][y], 2 * mri->width);
              swab(&MRISseq_vox(mri, 0, y, slice, frame),
                   &MRISseq_vox(mri, 0, y, slice, frame),
                   sizeof(short) * mri->width);
#endif
            }

          fclose(fp);

          if (frame != (mri->nframes-1))
            slice-- ;
        }

    }

  return(mri);

} /* end genesisRead() */

static MRI *gelxRead(char *fname, int read_volume)
{

  char fname_format[STRLEN];
  char fname_dir[STRLEN];
  char fname_base[STRLEN];
  char *c;
  MRI *mri = NULL;
  int im_init;
  int im_low, im_high;
  char fname_use[STRLEN];
  char temp_string[STRLEN];
  FILE *fp;
  int width, height;
  float tl_r, tl_a, tl_s;
  float tr_r, tr_a, tr_s;
  float br_r, br_a, br_s;
  float c_r, c_a, c_s;
  float n_r, n_a, n_s;
  float xlength, ylength, zlength;
  int i, y;
  int ecount, scount, icount;
  int good_flag;
  MRI *header;
  float xfov, yfov, zfov;

  /* ----- check the first (passed) file ----- */
  if(!FileExists(fname))
    {
      errno = 0;
      ErrorReturn
        (NULL, (ERROR_BADFILE, "genesisRead(): error opening file %s", fname));
    }

  /* ----- split the file name into name and directory ----- */
  c = strrchr(fname, '/');
  if(c == NULL)
    {
      fname_dir[0] = '\0';
      strcpy(fname_base, fname);
    }
  else
    {
      strncpy(fname_dir, fname, (c - fname + 1));
      fname_dir[c-fname+1] = '\0';
      strcpy(fname_base, c+1);
    }

  ecount = scount = icount = 0;
  good_flag = TRUE;
  for(c = fname_base;*c != '\0';c++)
    {
      if(*c == 'e')
        ecount++;
      else if(*c == 's')
        scount++;
      else if(*c == 'i')
        icount++;
      else if(!isdigit(*c))
        good_flag = FALSE;
    }
  if(good_flag && ecount == 1 && scount == 1 && icount == 1)
    {
      c = strrchr(fname_base, 'i');
      im_init = atoi(c+1);
      *c = '\0';
      sprintf(fname_format, "%si%%d", fname_base);
    }
  else
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADPARM,
          "genesisRead(): can't determine file name format for %s", fname));
    }

  strcpy(temp_string, fname_format);
  sprintf(fname_format, "%s%s", fname_dir, temp_string);

  /* ----- find the low and high files ----- */
  im_low = im_init;
  do
    {
      im_low--;
      sprintf(fname_use, fname_format, im_low);
    } while(FileExists(fname_use));
  im_low++;

  im_high = im_init;
  do
    {
      im_high++;
      sprintf(fname_use, fname_format, im_high);
    } while(FileExists(fname_use));
  im_high--;

  /* ----- allocate the mri structure ----- */
  header = MRIallocHeader(1, 1, 1, MRI_SHORT);

  header->depth = im_high - im_low + 1;
  header->imnr0 = 1;
  header->imnr1 = header->depth;

  /* ----- get the header information from the first file ----- */
  sprintf(fname_use, fname_format, im_low);
  if((fp = fopen(fname_use, "r")) == NULL)
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE, "genesisRead(): error opening file %s\n", fname_use));
    }

  fseek(fp, 3236, SEEK_SET);
  fread(&width, 4, 1, fp);  width = orderIntBytes(width);
  fread(&height, 4, 1, fp);  height = orderIntBytes(height);
  header->width = width;
  header->height = height;

  strcpy(header->fname, fname);

  fseek(fp, 2184 + 28, SEEK_SET);
  fread(&(header->thick), 4, 1, fp);
  header->thick = orderFloatBytes(header->thick);
  header->zsize = header->thick;

  fseek(fp, 2184 + 52, SEEK_SET);
  fread(&(header->xsize), 4, 1, fp);
  header->xsize = orderFloatBytes(header->xsize);
  fread(&(header->ysize), 4, 1, fp);
  header->ysize = orderFloatBytes(header->ysize);
  header->ps = header->xsize;

  fseek(fp, 2184 + 136, SEEK_SET);
  fread(&c_r,  4, 1, fp);  c_r  = orderFloatBytes(c_r);
  fread(&c_a,  4, 1, fp);  c_a  = orderFloatBytes(c_a);
  fread(&c_s,  4, 1, fp);  c_s  = orderFloatBytes(c_s);
  fread(&n_r,  4, 1, fp);  n_r  = orderFloatBytes(n_r);
  fread(&n_a,  4, 1, fp);  n_a  = orderFloatBytes(n_a);
  fread(&n_s,  4, 1, fp);  n_s  = orderFloatBytes(n_s);
  fread(&tl_r, 4, 1, fp);  tl_r = orderFloatBytes(tl_r);
  fread(&tl_a, 4, 1, fp);  tl_a = orderFloatBytes(tl_a);
  fread(&tl_s, 4, 1, fp);  tl_s = orderFloatBytes(tl_s);
  fread(&tr_r, 4, 1, fp);  tr_r = orderFloatBytes(tr_r);
  fread(&tr_a, 4, 1, fp);  tr_a = orderFloatBytes(tr_a);
  fread(&tr_s, 4, 1, fp);  tr_s = orderFloatBytes(tr_s);
  fread(&br_r, 4, 1, fp);  br_r = orderFloatBytes(br_r);
  fread(&br_a, 4, 1, fp);  br_a = orderFloatBytes(br_a);
  fread(&br_s, 4, 1, fp);  br_s = orderFloatBytes(br_s);

  header->x_r = (tr_r - tl_r);
  header->x_a = (tr_a - tl_a);
  header->x_s = (tr_s - tl_s);
  header->y_r = (br_r - tr_r);
  header->y_a = (br_a - tr_a);
  header->y_s = (br_s - tr_s);

  /* --- normalize -- the normal vector from the file
     should have length 1, but just in case... --- */
  xlength = sqrt(header->x_r*header->x_r +
                 header->x_a*header->x_a +
                 header->x_s*header->x_s);
  ylength = sqrt(header->y_r*header->y_r +
                 header->y_a*header->y_a +
                 header->y_s*header->y_s);
  zlength = sqrt(n_r*n_r + n_a*n_a + n_s*n_s);

  header->x_r = header->x_r / xlength;
  header->x_a = header->x_a / xlength;
  header->x_s = header->x_s / xlength;
  header->y_r = header->y_r / ylength;
  header->y_a = header->y_a / ylength;
  header->y_s = header->y_s / ylength;
  header->z_r = n_r / zlength;
  header->z_a = n_a / zlength;
  header->z_s = n_s / zlength;

  header->c_r = (tl_r + br_r) / 2.0 + n_r *
    header->zsize * (header->depth - 1.0) / 2.0;
  header->c_a = (tl_a + br_a) / 2.0 + n_a *
    header->zsize * (header->depth - 1.0) / 2.0;
  header->c_s = (tl_s + br_s) / 2.0 + n_s *
    header->zsize * (header->depth - 1.0) / 2.0;

  header->ras_good_flag = 1;

  header->xend = header->xsize * (double)header->width / 2.0;
  header->xstart = -header->xend;
  header->yend = header->ysize * (double)header->height / 2.0;
  header->ystart = -header->yend;
  header->zend = header->zsize * (double)header->depth / 2.0;
  header->zstart = -header->zend;

  xfov = header->xend - header->xstart;
  yfov = header->yend - header->ystart;
  zfov = header->zend - header->zstart;

  header->fov =
    ( xfov > yfov ? (xfov > zfov ? xfov : zfov) : (yfov > zfov ? yfov : zfov));

  fclose(fp);

  if(read_volume)
    mri = MRIalloc
      (header->width, header->height, header->depth, MRI_SHORT);
  else
    mri = MRIallocHeader
      (header->width, header->height, header->depth, MRI_SHORT);

  MRIcopyHeader(header, mri);
  MRIfree(&header);

  /* ----- read the volume if required ----- */
  if(read_volume)
    {
      for(i = im_low;i <= im_high; i++)
        {

          sprintf(fname_use, fname_format, i);
          if((fp = fopen(fname_use, "r")) == NULL)
            {
              MRIfree(&mri);
              errno = 0;
              ErrorReturn
                (NULL,
                 (ERROR_BADFILE,
                  "genesisRead(): error opening file %s", fname_use));
            }

          fseek(fp, 8432, SEEK_SET);

          for(y = 0;y < mri->height;y++)
            {
              if(fread(mri->slices[i-im_low][y], 2, mri->width, fp) !=
                 mri->width)
                {
                  fclose(fp);
                  MRIfree(&mri);
                  errno = 0;
                  ErrorReturn
                    (NULL,
                     (ERROR_BADFILE,
                      "genesisRead(): error reading from file file %s",
                      fname_use));
                }
#if (BYTE_ORDER == LITTLE_ENDIAN)
              swab(mri->slices[i-im_low][y], mri->slices[i-im_low][y],
                   2 * mri->width);
#endif
            }

          fclose(fp);

        }

    }

  return(mri);

} /* end gelxRead() */

/*----------------------------------------------------------------------
  GetSPMStartFrame() - gets an environment variable called
  SPM_START_FRAME and uses its value as the number of the first frame
  for an SPM series.  If this variable does not exist, then uses  1.
  ----------------------------------------------------------------------*/
int GetSPMStartFrame(void)
{
  char *s;
  int startframe;
  s = getenv("SPM_START_FRAME");
  if(s == NULL) return(1);
  sscanf(s,"%d",&startframe);
  printf("Using env var SPM_START_FRAME = %d\n",startframe);

  return(startframe);
}


/*-------------------------------------------------------------------------
  CountAnalyzeFiles() - counts the number analyze files associated with
  the given analyze file name.  The name can take several forms:
  stem.img - a single file
  stem     - stem. If nzpad < 0, then an extension of .img is implied.
  Otherwise, it looks for a series of files named stemXXX.img
  where XXX is a zero padded integer. The width of the padding
  is controlled by nzpad. The stem is returned in ppstem (unless
  ppstem == NULL).
  Note: the files must actually exist.
  -------------------------------------------------------------------------*/
int CountAnalyzeFiles(char *analyzefname, int nzpad, char **ppstem)
{
  int len, ncopy;
  char *stem, fmt[1000], fname[1000];
  int nfiles, keepcounting, startframe;
  FILE *fp;
  nfiles = 0;

  len = strlen(analyzefname);

  /* Determine whether the file name has a .img extension */
  if(len > 4 && strcmp(&(analyzefname[len-4]),".img")==0){
    ncopy = len-4;
    nfiles = 1;
    if(nzpad >= 0) {
      printf("ERROR: CountAnalyzeFiles: file with .img extension specified "
             "       with zero pad variable.\n");
      return(-1);
    }
  }
  else {
    ncopy = len;
    if(nzpad < 0) nfiles = 1;
  }

  /* Get the stem (ie, everything without the .img */
  stem = (char *) calloc(len+1,sizeof(char));
  memcpy(stem,analyzefname,ncopy);

  if(ppstem != NULL) *ppstem = stem;

  /* If there's only one file, check that it's there */
  if(nfiles == 1){
    sprintf(fname,"%s.img",stem);
    if(ppstem == NULL) free(stem);
    fp = fopen(fname,"r");
    if(fp == NULL) return(0);
    fclose(fp);
    return(1);
  }

  /* If there are multiple files, count them, starting at 1 */
  sprintf(fmt,"%s%%0%dd.img",stem,nzpad);
  keepcounting = 1;
  startframe = GetSPMStartFrame();
  nfiles = 0;
  while(keepcounting){
    sprintf(fname,fmt,nfiles+startframe);
    fp = fopen(fname,"r");
    if(fp == NULL) keepcounting = 0;
    else {
      fclose(fp);
      nfiles ++;
    }
  }

  if(ppstem == NULL) free(stem);
  return(nfiles);
}
/*-------------------------------------------------------------------------*/
static int DumpAnalyzeHeader(FILE *fp, dsr *hdr)
{
  fprintf(fp,"Header Key\n");
  fprintf(fp,"  sizeof_hdr    %d\n",hdr->hk.sizeof_hdr);
  fprintf(fp,"  data_type     %s\n",hdr->hk.data_type);
  fprintf(fp,"  db_name       %s\n",hdr->hk.db_name);
  fprintf(fp,"  extents       %d\n",hdr->hk.extents);
  fprintf(fp,"  session_error %d\n",hdr->hk.session_error);
  fprintf(fp,"  regular       %c\n",hdr->hk.regular);
  fprintf(fp,"  hkey_un0      %c\n",hdr->hk.hkey_un0);
  fprintf(fp,"Image Dimension \n");
  fprintf(fp,"  dim %d %d %d %d %d %d %d %d \n",
          hdr->dime.dim[0],hdr->dime.dim[1],hdr->dime.dim[2],hdr->dime.dim[3],
          hdr->dime.dim[4],hdr->dime.dim[5],hdr->dime.dim[6],hdr->dime.dim[7]);
  fprintf(fp,"  pixdim %f %f %f %f %f %f %f %f \n",
          hdr->dime.pixdim[0],hdr->dime.pixdim[1],hdr->dime.pixdim[2],
          hdr->dime.pixdim[3],hdr->dime.pixdim[4],hdr->dime.pixdim[5],
          hdr->dime.pixdim[6],hdr->dime.pixdim[7]);
  fprintf(fp,"  vox_units     %s\n",hdr->dime.vox_units);
  fprintf(fp,"  cal_units     %s\n",hdr->dime.cal_units);
  fprintf(fp,"  datatype      %d\n",hdr->dime.datatype);
  fprintf(fp,"  bitpix        %d\n",hdr->dime.bitpix);
  fprintf(fp,"  glmax         %g\n",(float)hdr->dime.glmax);
  fprintf(fp,"  glmin         %g\n",(float)hdr->dime.glmin);
  fprintf(fp,"  orient        %d\n",hdr->hist.orient);

  return(0);

}
/*-------------------------------------------------------------------------*/
static dsr *ReadAnalyzeHeader(char *hdrfile, int *swap,
                              int *mritype, int *bytes_per_voxel)
{
  FILE *fp;
  dsr *hdr;

  /* Open and read the header */
  fp = fopen(hdrfile,"r");
  if(fp == NULL){
    printf("ERROR: ReadAnalyzeHeader(): cannot open %s\n",hdrfile);
    return(NULL);
  }

  /* Read the header file */
  hdr = (dsr *) calloc(1, sizeof(dsr));
  fread(hdr, sizeof(dsr), 1, fp);
  fclose(fp);

  *swap = 0;
  if(hdr->hk.sizeof_hdr != sizeof(dsr))
    {
      *swap = 1;
      swap_analyze_header(hdr);
    }

  if(hdr->dime.datatype == DT_UNSIGNED_CHAR)
    {
      *mritype = MRI_UCHAR;
      *bytes_per_voxel = 1;
    }
  else if(hdr->dime.datatype == DT_SIGNED_SHORT)    {
    *mritype = MRI_SHORT;
    *bytes_per_voxel = 2;
  }
  else if(hdr->dime.datatype == DT_UINT16)    {
    // Can happen if this is nifti
    printf("Unsigned short not supported, but trying to read it \n"
	   "in as a signed short. Will be ok if no vals >= 32k.\n");
    *mritype = MRI_SHORT;
    *bytes_per_voxel = 2;
  }
  else if(hdr->dime.datatype == DT_SIGNED_INT)
    {
      *mritype = MRI_INT;
      *bytes_per_voxel = 4;
    }
  else if(hdr->dime.datatype == DT_FLOAT)
    {
      *mritype = MRI_FLOAT;
      *bytes_per_voxel = 4;
    }
  else if(hdr->dime.datatype == DT_DOUBLE)
    {
      *mritype = MRI_FLOAT;
      *bytes_per_voxel = 8;
    }
  else
    {
      free(hdr);
      errno = 0;
      ErrorReturn(NULL, (ERROR_UNSUPPORTED, "ReadAnalyzeHeader: "
                         "unsupported data type %d", hdr->dime.datatype));
    }

  return(hdr);
}
/*-------------------------------------------------------------------------
  analyzeRead() - see the end of file for the old (pre-10/11/01) analyzeRead().
  The fname can take one of several forms.
  -------------------------------------------------------------------------*/
static MRI *analyzeRead(char *fname, int read_volume)
{
  extern int N_Zero_Pad_Input;
  int nfiles, k, nread;
  char *stem;
  char imgfile[1000], hdrfile[1000], matfile[1000], fmt[1000];
  char *buf;
  FILE *fp;
  dsr *hdr;
  int swap, mritype, bytes_per_voxel, cantreadmatfile=0;
  int ncols, nrows, nslcs, nframes, row, slice, frame, startframe;
  MATRIX *T=NULL, *PcrsCenter, *PxyzCenter, *T1=NULL, *Q=NULL;
  MRI *mri, *mritmp;
  float min, max;
  struct stat StatBuf;
  int nv,nreal;
  char direction[64];
  int signX, signY, signZ;
  int thiserrno, nifticode;

  fp = NULL;
  startframe = GetSPMStartFrame();

  /* Count the number of files associated with this file name,
     and get the stem. */
  nfiles = CountAnalyzeFiles(fname,N_Zero_Pad_Input,&stem);

  /* If there are no files, return NULL */
  if(nfiles < 0) return(NULL);
  if(nfiles == 0){
    printf("ERROR: analyzeRead(): cannot find any files for %s\n",fname);
    return(NULL);
  }
  //printf("INFO: analyzeRead(): found %d files for %s\n",nfiles,fname);

  /* Create file names of header and mat files */
  if(N_Zero_Pad_Input > -1){
    sprintf(fmt,"%s%%0%dd.%%s",stem,N_Zero_Pad_Input);
    sprintf(hdrfile,fmt,startframe,"hdr");
    sprintf(matfile,fmt,startframe,"mat");
    sprintf(imgfile,fmt,startframe,"img");
  }
  else{
    sprintf(hdrfile,"%s.hdr",stem);
    sprintf(matfile,"%s.mat",stem);
    sprintf(imgfile,"%s.img",stem);
  }

  // here, nifticode can only be 0 (ANALYZE) or 2 (Two-File NIFTI)
  nifticode = is_nifti_file(hdrfile);
  if(Gdiag > 0) printf("nifticode = %d\n",nifticode);
  if(nifticode == 2){
    if(nfiles == 1){
      printf("INFO: reading as a two-file NIFTI\n");
      mri = nifti1Read(hdrfile, read_volume);
      return(mri);
    }
  }
  // It can get here if it is a multi-frame two-file nifti in 
  // which each frame is stored as a separate file. In this
  // case, it reads the header with the analyze reader, but
  // then reads in the vox2ras matrix with the nifti reader.
  // It does not look like it makes a difference.
  hdr = ReadAnalyzeHeader(hdrfile, &swap, &mritype, &bytes_per_voxel);
  if(hdr == NULL) return(NULL);
  if(Gdiag_no > 0)  DumpAnalyzeHeader(stdout,hdr);

  /* Get the number of frames as either the fourth dimension or
     the number of files. */
  if(nfiles == 1){
    nframes = hdr->dime.dim[4];
    if(nframes==0) nframes = 1;
  }
  else  nframes = nfiles;

  ncols = hdr->dime.dim[1];
  nrows = hdr->dime.dim[2];
  nslcs = hdr->dime.dim[3];
  nv = ncols*nrows*nslcs*nframes;

  if( ncols == 1 || nrows == 1 ) {
    lstat(imgfile, &StatBuf);
    if(StatBuf.st_size != nv*bytes_per_voxel){
      nreal = StatBuf.st_size / bytes_per_voxel;
      if(ncols ==1) nrows = nreal;
      else          ncols = nreal;
      printf("INFO: %s really has %d rows/cols\n",imgfile,nreal);
    }
  }

  /* Alloc the Header and/or Volume */
  if(read_volume)
    mri = MRIallocSequence(ncols, nrows, nslcs, mritype, nframes);
  else {
    mri = MRIallocHeader(ncols, nrows, nslcs, mritype);
    mri->nframes = nframes;
  }

  /* Load Variables into header */
  mri->xsize = fabs(hdr->dime.pixdim[1]);  /* col res */
  mri->ysize = fabs(hdr->dime.pixdim[2]);  /* row res */
  mri->zsize = fabs(hdr->dime.pixdim[3]);  /* slice res */
  mri->tr    = 1000*hdr->dime.pixdim[4];  /* time  res */

  signX = (hdr->dime.pixdim[1] > 0) ? 1 : -1;
  signY = (hdr->dime.pixdim[2] > 0) ? 1 : -1;
  signZ = (hdr->dime.pixdim[3] > 0) ? 1 : -1;

  // Handel the vox2ras matrix 
  if(nifticode != 2){ // Not a nifti file
    /* Read the matfile, if there */
    if(FileExists(matfile)){
      T1 = MatlabRead(matfile); // orientation info
      if(T1 == NULL){
	printf
	  ("WARNING: analyzeRead(): matfile %s exists but could not read ... \n",
	   matfile);
	printf("  may not be matlab4 mat file ... proceeding without it.\n");
	fflush(stdout);
	cantreadmatfile=1;
      }
      else{
	/* Convert from 1-based to 0-based */
	Q = MtxCRS1toCRS0(Q);
	T = MatrixMultiply(T1,Q,T);
	//printf("------- Analyze Input Matrix (zero-based) --------\n");
	//MatrixPrint(stdout,T);
	//printf("-------------------------------------\n");
	mri->ras_good_flag = 1;
	MatrixFree(&Q);
	MatrixFree(&T1);
      }
    }
    if(! FileExists(matfile) || cantreadmatfile){
      /* when not found, it is a fun exercise.                      */
      /* see http://wideman-one.com/gw/brain/analyze/formatdoc.htm  */
      /* for amgibuities.                                           */
      /* I will follow his advise                                   */
      /* hist.orient  Mayo name   Voxel[Index0, Index1, Index2] */
      /*                          Index0  Index1  Index2              */
      /* 0 transverse unflipped   R-L     P-A     I-S     LAS */
      /* 3 transverse flipped     R-L     A-P     I-S     LPS */
      /* 1 coronal unflipped      R-L     I-S     P-A     LSA */
      /* 4 coronal flipped        R-L     S-I     P-A     LIA */
      /* 2 sagittal unflipped     P-A     I-S     R-L     ASL */
      /* 5 sagittal flipped       P-A     S-I     R-L     AIL */
      //   P->A I->S L->R
      
      /* FLIRT distributes analyze format image which has a marked LR */
      /* in fls/etc/standard/avg152T1_LR-marked.img.  The convention    */
      /* is tested with this image for hdr->hist.orient== 0.              */
      /* note that flirt image has negative pixdim[1], but their software */
      /* ignores the sign anyway.                                         */
      if (hdr->hist.orient==0)  /* x = - r, y = a, z = s */
	{
	  strcpy(direction, "transverse unflipped (default)");
	  T = MatrixAlloc(4, 4, MATRIX_REAL);
	  T->rptr[1][1] =  -mri->xsize;
	  T->rptr[2][2] =  mri->ysize;
	  T->rptr[3][3] =  mri->zsize;
	  T->rptr[1][4] = mri->xsize*(mri->width/2.0);
	  T->rptr[2][4] = -mri->ysize*(mri->height/2.0);
	  T->rptr[3][4] = -mri->zsize*(mri->depth/2.0);
	  T->rptr[4][4] = 1.;
	}
      else if (hdr->hist.orient==1) /* x = -r, y = s, z = a */
	{
	  strcpy(direction, "coronal unflipped");
	  T = MatrixAlloc(4, 4, MATRIX_REAL);
	  T->rptr[1][1] = -mri->xsize;
	  T->rptr[2][3] =  mri->zsize;
	  T->rptr[3][2] =  mri->ysize;
	  T->rptr[1][4] = mri->xsize*(mri->width/2.0);
	  T->rptr[2][4] = -mri->zsize*(mri->depth/2.0);
	  T->rptr[3][4] = -mri->ysize*(mri->height/2.0);
	  T->rptr[4][4] = 1.;
	}
      else if (hdr->hist.orient==2) /* x = a, y = s, z = -r */
	{
	  strcpy(direction, "sagittal unflipped");
	  T = MatrixAlloc(4, 4, MATRIX_REAL);
	  T->rptr[1][3] =  -mri->zsize;
	  T->rptr[2][1] =  mri->xsize;
	  T->rptr[3][2] =  mri->ysize;
	  T->rptr[1][4] = mri->zsize*(mri->depth/2.0);
	  T->rptr[2][4] = -mri->xsize*(mri->width/2.0);
	  T->rptr[3][4] = -mri->ysize*(mri->height/2.0);
	  T->rptr[4][4] = 1.;
	}
      else if (hdr->hist.orient==3) /* x = -r, y = -a, z = s */
	{
	  strcpy(direction, "transverse flipped");
	  T = MatrixAlloc(4, 4, MATRIX_REAL);
	  T->rptr[1][1] =  -mri->xsize;
	  T->rptr[2][2] =  -mri->ysize;
	  T->rptr[3][3] =  mri->zsize;
	  T->rptr[1][4] = mri->xsize*(mri->width/2.0);
	  T->rptr[2][4] = mri->ysize*(mri->height/2.0);
	  T->rptr[3][4] = -mri->zsize*(mri->depth/2.0);
	  T->rptr[4][4] = 1.;
	}
      else if (hdr->hist.orient==4) /* x = -r, y = -s, z = a */
	{
	  strcpy(direction, "coronal flipped");
	  T = MatrixAlloc(4, 4, MATRIX_REAL);
	  T->rptr[1][1] = -mri->xsize;
	  T->rptr[2][3] =  mri->zsize;
	  T->rptr[3][2] = -mri->ysize;
	  T->rptr[1][4] =  mri->xsize*(mri->width/2.0);
	  T->rptr[2][4] = -mri->zsize*(mri->depth/2.0);
	  T->rptr[3][4] =  mri->ysize*(mri->height/2.0);
	  T->rptr[4][4] = 1.;
	}
      else if (hdr->hist.orient==5) /* x = a, y = -s, z = -r */
	{
	  strcpy(direction, "sagittal flipped");
	  T = MatrixAlloc(4, 4, MATRIX_REAL);
	  T->rptr[1][3] = -mri->zsize;
	  T->rptr[2][1] =  mri->xsize;
	  T->rptr[3][2] = -mri->ysize;
	  T->rptr[1][4] =  mri->zsize*(mri->depth/2.0);
	  T->rptr[2][4] = -mri->xsize*(mri->width/2.0);
	  T->rptr[3][4] =  mri->ysize*(mri->height/2.0);
	  T->rptr[4][4] = 1.;
	}
      if (hdr->hist.orient == -1){
	/* Unknown, so assume: x = -r, y = -a, z = s */
	// This is incompatible with mghRead() when rasgood=0.
	// mghRead() uses coronal dircos, not transverse.
	// I'm not changing it because don't know what will happen.
	strcpy(direction, "transverse flipped");
	T = MatrixAlloc(4, 4, MATRIX_REAL);
	T->rptr[1][1] = -mri->xsize;
	T->rptr[2][2] = -mri->ysize;
	T->rptr[3][3] =  mri->zsize;
	T->rptr[1][4] =  mri->xsize*(mri->width/2.0);
	T->rptr[2][4] =  mri->ysize*(mri->height/2.0);
	T->rptr[3][4] = -mri->zsize*(mri->depth/2.0);
	T->rptr[4][4] = 1.;
	mri->ras_good_flag = 0;
	fprintf(stderr,
		"WARNING: could not find %s file for direction cosine info.\n"
		"WARNING: Analyze 7.5 hdr->hist.orient value = -1, not valid.\n"
		"WARNING: assuming %s\n",matfile, direction);
      }
      else{
	// It's probably not a good idea to set this to 1, but setting it
	// to 0 created all kinds of problems with mghRead() which will
	// force dir cos to be Coronal if rasgood<0.
	mri->ras_good_flag = 1;
	fprintf
	  (stderr,
	   "-----------------------------------------------------------------\n"
	   "INFO: could not find %s file for direction cosine info.\n"
	   "INFO: use Analyze 7.5 hdr->hist.orient value: %d, %s.\n"
	   "INFO: if not valid, please provide the information in %s file\n"
	   "-----------------------------------------------------------------\n",
	   matfile, hdr->hist.orient, direction, matfile
	   );
      }
    }
  }
  else {
    // Just read in this one file as nifti to get vox2ras matrix
    // What a hack.
    mritmp = MRIreadHeader(imgfile,NIFTI1_FILE);
    T = MRIxfmCRS2XYZ(mritmp,0);
    MRIfree(&mritmp);
  }


  /* ---- Assign the Geometric Paramaters -----*/
  mri->x_r = T->rptr[1][1]/mri->xsize;
  mri->x_a = T->rptr[2][1]/mri->xsize;
  mri->x_s = T->rptr[3][1]/mri->xsize;

  /* Row Direction Cosines */
  mri->y_r = T->rptr[1][2]/mri->ysize;
  mri->y_a = T->rptr[2][2]/mri->ysize;
  mri->y_s = T->rptr[3][2]/mri->ysize;

  /* Slice Direction Cosines */
  mri->z_r = T->rptr[1][3]/mri->zsize;
  mri->z_a = T->rptr[2][3]/mri->zsize;
  mri->z_s = T->rptr[3][3]/mri->zsize;

  /* Center of the FOV in voxels */
  PcrsCenter = MatrixAlloc(4, 1, MATRIX_REAL);
  PcrsCenter->rptr[1][1] = mri->width/2.0;
  PcrsCenter->rptr[2][1] = mri->height/2.0;
  PcrsCenter->rptr[3][1] = mri->depth/2.0;
  PcrsCenter->rptr[4][1] = 1.0;

  /* Center of the FOV in XYZ */
  PxyzCenter = MatrixMultiply(T,PcrsCenter,NULL);
  mri->c_r = PxyzCenter->rptr[1][1];
  mri->c_a = PxyzCenter->rptr[2][1];
  mri->c_s = PxyzCenter->rptr[3][1];

  MatrixFree(&PcrsCenter);
  MatrixFree(&PxyzCenter);
  MatrixFree(&T);

  if(!read_volume) return(mri);

  /* Alloc the maximum amount of memory that a row could need */
  buf = (char *)malloc(mri->width * 8);

  /* Open the one file, if there is one file */
  if(nfiles == 1){
    fp = fopen(imgfile,"r");
    if(fp == NULL){
      printf("ERROR: analyzeRead(): could not open %s\n",imgfile);
      MRIfree(&mri);
      return(NULL);
    }
    fseek(fp, (int)(hdr->dime.vox_offset), SEEK_SET);
  }

  /*--------------------- Frame Loop --------------------------*/
  for(frame = 0; frame < nframes; frame++){

    /* Open the frame file if there is more than one file */
    if(N_Zero_Pad_Input > -1){
      sprintf(imgfile,fmt,frame+startframe,"img");
      fp = fopen(imgfile,"r");
      if(fp == NULL){
        printf("ERROR: analyzeRead(): could not open %s\n",imgfile);
        MRIfree(&mri);
        return(NULL);
      }
      fseek(fp, (int)(hdr->dime.vox_offset), SEEK_SET);
    }

    /* ----------- Slice Loop ----------------*/
    for(slice = 0;slice < mri->depth; slice++){
      k = slice + mri->depth * frame;

      /* --------- Row Loop ------------------*/
      for(row = 0; row < mri->height; row++){

	nread = fread(buf, bytes_per_voxel, mri->width, fp);
	thiserrno = errno;
	if(nread != mri->width){
	  if(feof(fp)) printf("ERROR: premature end of file\n");
	  printf("ERROR: %s (%d)\n",strerror(thiserrno),thiserrno);
	  errno = 0;
	  printf("frame = %d, slice = %d, k=%d, row = %d\n",
		 frame,slice,k,row);
	  printf("nread = %d, nexpected = %d\n",nread,mri->width);
	  MRIdump(mri, stdout);
	  fflush(stdout);
	  MRIfree(&mri);free(buf);fclose(fp);
	  ErrorReturn(NULL, (ERROR_BADFILE, "analyzeRead2(): error reading "
			     "from file %s\n", imgfile));
	}

	if(swap){
	  if(bytes_per_voxel == 2)
	    byteswapbufshort((void*)buf,bytes_per_voxel*mri->width);
	  if(bytes_per_voxel == 4)
	    byteswapbuffloat((void*)buf,bytes_per_voxel*mri->width);
	  //nflip(buf, bytes_per_voxel, mri->width); /* byte swap */
	}
	
	memcpy(mri->slices[k][row], buf, bytes_per_voxel*mri->width); /*copy*/

        } /* End Row Loop */
    }  /* End Slice Loop */

    /* Close file if each frame is in a separate file */
    if(N_Zero_Pad_Input > -1) fclose(fp);

  }  /* End Frame Loop */

  if(N_Zero_Pad_Input < 0) fclose(fp);

  free(buf);
  free(hdr);

  MRIlimits(mri,&min,&max);
  if(Gdiag_no > 0) printf("INFO: analyzeRead(): min = %g, max = %g\n",min,max);

  return(mri);
}

/*---------------------------------------------------------------
  analyzeWrite() - writes out a volume in SPM analyze format. If the
  file name has a .img extension, then the first frame is stored into
  the given file name. If it does not include the extension, then the
  volume is saved as a series of frame files using fname as a base
  followed by the zero-padded frame number (see analyzeWriteSeries).  A
  series can be saved from mri_convert by specifying the basename and
  adding "--out_type spm". See also analyzeWrite4D(). DNG
  ---------------------------------------------------------------*/
static int analyzeWrite(MRI *mri, char *fname)
{
  int len;
  int error_value;

  /* Check for .img extension */
  len = strlen(fname);
  if(len > 4){
    if(fname[len-4] == '.' && fname[len-3] == 'i' &&
       fname[len-2] == 'm' && fname[len-1] == 'g' ){
      /* There is a trailing .img - save frame 0 into fname */
      error_value = analyzeWriteFrame(mri, fname, 0);
      return(error_value);
    }
  }
  /* There is no trailing .img - save as a series */
  error_value = analyzeWriteSeries(mri, fname);
  return(error_value);
}
/*---------------------------------------------------------------
  analyzeWriteFrame() - this used to be analyzeWrite() but was modified
  by DNG to be able to save a particular frame so that it could be
  used to write out an entire series.
  ---------------------------------------------------------------*/
static void printDirCos(MRI *mri)
{
  fprintf(stderr, "Direction cosines for %s are:\n", mri->fname);
  fprintf(stderr, "  x_r = %8.4f, y_r = %8.4f, z_r = %8.4f, c_r = %10.4f\n",
          mri->x_r, mri->y_r, mri->z_r, mri->c_r);
  fprintf(stderr, "  x_a = %8.4f, y_a = %8.4f, z_a = %8.4f, c_a = %10.4f\n",
          mri->x_a, mri->y_a, mri->z_a, mri->c_a);
  fprintf(stderr, "  x_s = %8.4f, y_s = %8.4f, z_s = %8.4f, c_s = %10.4f\n",
          mri->x_s, mri->y_s, mri->z_s, mri->c_s);
}

static int analyzeWriteFrame(MRI *mri, char *fname, int frame)
{
  dsr hdr;
  float max, min, det;
  MATRIX *T, *invT;
  char hdr_fname[STRLEN];
  char mat_fname[STRLEN];
  char *c;
  FILE *fp;
  int error_value;
  int i, j, k;
  int bytes_per_voxel;
  short i1, i2, i3;
  int shortmax;
  char *orientname[7] =
    { "transverse unflipped", "coronal unflipped", "sagittal unflipped",
      "transverse flipped", "coronal flipped", "sagittal flipped",
      "unknown" };

  if(frame >= mri->nframes){
    fprintf(stderr,"ERROR: analyzeWriteFrame(): frame number (%d) exceeds "
            "number of frames (%d)\n",frame,mri->nframes);
    return(1);
  }

  shortmax = (int)(pow(2.0,15.0));
  if(mri->width > shortmax){
    printf("ANALYZE FORMAT ERROR: ncols %d in volume exceeds %d\n",
           mri->width,shortmax);
    exit(1);
  }
  if(mri->height > shortmax){
    printf("ANALYZE FORMAT ERROR: nrows %d in volume exceeds %d\n",
           mri->height,shortmax);
    exit(1);
  }
  if(mri->depth > shortmax){
    printf("ANALYZE FORMAT ERROR: nslices %d in volume exceeds %d\n",
           mri->depth,shortmax);
    exit(1);
  }
  if(mri->nframes > shortmax){
    printf("ANALYZE FORMAT ERROR:  nframes %d in volume exceeds %d\n",
           mri->nframes,shortmax);
    exit(1);
  }

  c = strrchr(fname, '.');
  if(c == NULL)
    {
      errno = 0;
      ErrorReturn(ERROR_BADPARM, (ERROR_BADPARM, "analyzeWriteFrame(): "
                                  "bad file name %s", fname));
    }
  if(strcmp(c, ".img") != 0)
    {
      errno = 0;
      ErrorReturn(ERROR_BADPARM, (ERROR_BADPARM, "analyzeWriteFrame(): "
                                  "bad file name %s", fname));
    }

  strcpy(hdr_fname, fname);
  sprintf(hdr_fname + (c - fname), ".hdr");

  strcpy(mat_fname, fname);
  sprintf(mat_fname + (c - fname), ".mat");

  memset(&hdr, 0x00, sizeof(hdr));

  hdr.hk.sizeof_hdr = sizeof(hdr);

  hdr.dime.vox_offset = 0.0;

  if(mri->type == MRI_UCHAR)
    {
      hdr.dime.datatype = DT_UNSIGNED_CHAR;
      bytes_per_voxel = 1;
    }
  else if(mri->type == MRI_SHORT)
    {
      hdr.dime.datatype = DT_SIGNED_SHORT;
      bytes_per_voxel = 2;
    }
  /* --- assuming long and int are identical --- */
  else if(mri->type == MRI_INT || mri->type == MRI_LONG)
    {
      hdr.dime.datatype = DT_SIGNED_INT;
      bytes_per_voxel = 4;
    }
  else if(mri->type == MRI_FLOAT)
    {
      hdr.dime.datatype = DT_FLOAT;
      bytes_per_voxel = 4;
    }
  else
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "analyzeWriteFrame(): bad data type %d", mri->type));
    }

  /* Added by DNG 10/2/01 */
  hdr.dime.dim[0] = 4; /* number of dimensions */
  hdr.dime.dim[4] = 1; /* time */
  hdr.dime.pixdim[4] = mri->tr/1000.0; // convert to sec
  hdr.dime.bitpix = 8*bytes_per_voxel;
  memcpy(hdr.dime.vox_units,"mm\0",3);
  /*----------------------------*/

  hdr.dime.dim[1] = mri->width;    /* ncols */
  hdr.dime.dim[2] = mri->height;   /* nrows */
  hdr.dime.dim[3] = mri->depth;    /* nslices */

  hdr.dime.pixdim[1] = mri->xsize; /* col res */
  hdr.dime.pixdim[2] = mri->ysize; /* row res */
  hdr.dime.pixdim[3] = mri->zsize; /* slice res */

  MRIlimits(mri, &min, &max);
  hdr.dime.glmin = (int)min;
  hdr.dime.glmax = (int)max;

  /* Construct the matrix to convert CRS to XYZ, assuming
     that CRS is 1-based */
  T = MRIxfmCRS2XYZ(mri,1);

  det = MatrixDeterminant(T) ;
  if(det == 0){
    printf("WARNING: cannot determine volume orientation, "
           "assuming identity. It's ok if the output is a surface.\n");
    T = MatrixIdentity(4, T) ;
    if(mri->xsize > 0) T->rptr[1][1] = mri->xsize;
    if(mri->ysize > 0) T->rptr[2][2] = mri->ysize;
    if(mri->zsize > 0) T->rptr[3][3] = mri->zsize;
  }
  if(frame == 0){
    printf("Analyze Output Matrix\n");
    MatrixPrint(stdout,T);
    printf("--------------------\n");
  }

  /* ----- write the matrix to the .mat file ----- */
  error_value = MatlabWrite(T, mat_fname, "M");
  if(error_value != NO_ERROR) return(error_value);

  /* Compute inverse ot T (converts from XYZ to CRS) */
  invT = MatrixInverse(T,NULL);
  /* If the inverse cannot be computed, set to the identity */
  if(invT == NULL) invT = MatrixIdentity(4,NULL);

  /* Load the CRS into the originator field as 3 shorts for SPM */
  /* These come from the last column of invT */
  i1 = (short)(rint(*MATRIX_RELT(invT, 1, 4)));
  memcpy( (&hdr.hist.originator[0]), &i1, sizeof(short));
  i2 = (short)(rint(*MATRIX_RELT(invT, 2, 4)));
  memcpy( (&hdr.hist.originator[0] + sizeof(short)), &i2, sizeof(short));
  i3 = (short)(rint(*MATRIX_RELT(invT, 3, 4)));
  memcpy( (&hdr.hist.originator[0] + 2*sizeof(short)), &i3, sizeof(short));

  MatrixFree(&T);
  MatrixFree(&invT);

  /* Set the hist.orient field -- this is not always correct */
  /* see http://wideman-one.com/gw/brain/analyze/formatdoc.htm  */
  if(fabs(mri->z_s) > fabs(mri->z_r) && fabs(mri->z_s) > fabs(mri->z_a))
    {
      // Transverse: Superior/Inferior > both Right and Anterior
      if (mri->x_r < 0 && mri->y_a > 0 && mri->z_s > 0)
        hdr.hist.orient = 0; // transverse unflipped  LAS
      else if (mri->x_r < 0 && mri->y_a <0 && mri->z_s > 0)
        hdr.hist.orient = 3; // transverse flipped    LPS
      else
        {
          fprintf
            (stderr,
             "No such orientation specified in Analyze7.5. "
             "Set orient to 0.\n");
          fprintf(stderr, "Not a problem as long as you use the .mat file.\n");
          printDirCos(mri);
          hdr.hist.orient = 0;
        }
    }
  if(fabs(mri->z_a) > fabs(mri->z_r) && fabs(mri->z_a) > fabs(mri->z_s))
    {
      // Cor: Anterior/Post > both Right and Superior
      if(mri->x_r < 0 && mri->y_s > 0 && mri->z_a > 0)
        hdr.hist.orient = 1; // cor unflipped   LSA
      else if (mri->x_r <0 && mri->y_s < 0 && mri->z_a > 0)
        hdr.hist.orient = 4; // cor flipped     LIA
      else
        {
          fprintf(stderr,
                  "No such orientation specified in Analyze7.5. "
                  "Set orient to 0\n");
          printDirCos(mri);
          hdr.hist.orient = 0;
        }
    }
  if(fabs(mri->z_r) > fabs(mri->z_a) && fabs(mri->z_r) > fabs(mri->z_s))
    {
      // Sag: Righ/Left > both Anterior and Superior
      if(mri->x_a > 0 && mri->y_s > 0 && mri->z_r < 0)
        hdr.hist.orient = 2; // sag unflipped   ASL
      else if (mri->x_a > 0 && mri->y_s < 0 && mri->z_r < 0)
        hdr.hist.orient = 5; // sag flipped     AIL
      else
        {
          fprintf
            (stderr,
             "No such orientation specified in Analyze7.5. Set orient to 0\n");
          printDirCos(mri);
          hdr.hist.orient = 0;
        }
    }
  printf("INFO: set hdr.hist.orient to '%s'\n",
         orientname[(int) hdr.hist.orient]);

  /* ----- open the header file ----- */
  if((fp = fopen(hdr_fname, "w")) == NULL){
    errno = 0;
    ErrorReturn
      (ERROR_BADFILE,
       (ERROR_BADFILE,
        "analyzeWriteFrame(): error opening file %s for writing",
        hdr_fname));
  }
  /* ----- write the header ----- */
  /* should write element-by-element */
  if(fwrite(&hdr, sizeof(hdr), 1, fp) != 1){
    errno = 0;
    ErrorReturn
      (ERROR_BADFILE,
       (ERROR_BADFILE,
        "analyzeWriteFrame(): error writing to file %s", hdr_fname));
  }
  fclose(fp);

  /* ----- open the data file ----- */
  if((fp = fopen(fname, "w")) == NULL){
    errno = 0;
    ErrorReturn
      (ERROR_BADFILE,
       (ERROR_BADFILE,
        "analyzeWriteFrame(): error opening file %s for writing", fname));
  }

  /* ----- write the data ----- */
  for(i = 0;i < mri->depth;i++){
    /* this is the change to make it save a given frame */
    k = i + mri->depth*frame;
    for(j = 0;j < mri->height;j++){
      if(fwrite(mri->slices[k][j], bytes_per_voxel, mri->width, fp) !=
         mri->width)
        {
          errno = 0;
          ErrorReturn(ERROR_BADFILE, (ERROR_BADFILE, "analyzeWriteFrame(): "
                                      " error writing to file %s",
                                      fname));
        } /* end if */
    } /* end row loop */
  } /* end col loop */

  fclose(fp);

  return(0);

} /* end analyzeWriteFrame() */

/*-------------------------------------------------------------*/
static int analyzeWriteSeries(MRI *mri, char *fname)
{
  extern int N_Zero_Pad_Output;
  int frame;
  int err;
  char framename[STRLEN];
  char spmnamefmt[STRLEN];

  /* NOTE: This function assumes that fname does not have a .img extension. */

  /* N_Zero_Pad_Output is a global variable used to control the name of      */
  /* files in successive frames within the series. It can be set from     */
  /* mri_convert using "--spmnzeropad N" where N is the width of the pad. */
  if(N_Zero_Pad_Output < 0) N_Zero_Pad_Output = 3;

  /* Create the format string used to create the filename for each frame.   */
  /* The frame file name format will be fname%0Nd.img where N is the amount */
  /* of zero padding. The (padded) frame numbers will go from 1 to nframes. */
  sprintf(spmnamefmt,"%%s%%0%dd.img",N_Zero_Pad_Output);

  /* loop over slices */
  for(frame = 0; frame < mri->nframes; frame ++){
    sprintf(framename,spmnamefmt,fname,frame+1);
    //printf("%3d %s\n",frame,framename);
    err = analyzeWriteFrame(mri, framename, frame);
    if(err) return(err);
  }

  return(0);
}

/*---------------------------------------------------------------
  analyzeWrite4D() - saves data in analyze 4D format.
  ---------------------------------------------------------------*/
static int analyzeWrite4D(MRI *mri, char *fname)
{
  dsr hdr;
  float max, min, det;
  MATRIX *T, *invT;
  char hdr_fname[STRLEN];
  char mat_fname[STRLEN];
  char *c;
  FILE *fp;
  int error_value;
  int i, j, k, frame;
  int bytes_per_voxel;
  short i1, i2, i3;
  int shortmax;

  shortmax = (int)(pow(2.0,15.0));
  if(mri->width > shortmax){
    printf("ANALYZE FORMAT ERROR: ncols %d in volume exceeds %d\n",
           mri->width,shortmax);
    exit(1);
  }
  if(mri->height > shortmax){
    printf("ANALYZE FORMAT ERROR: nrows %d in volume exceeds %d\n",
           mri->height,shortmax);
    exit(1);
  }
  if(mri->depth > shortmax){
    printf("ANALYZE FORMAT ERROR: nslices %d in volume exceeds %d\n",
           mri->depth,shortmax);
    exit(1);
  }
  if(mri->nframes > shortmax){
    printf("ANALYZE FORMAT ERROR:  nframes %d in volume exceeds %d\n",
           mri->nframes,shortmax);
    exit(1);
  }


  c = strrchr(fname, '.');
  if(c == NULL)    {
    errno = 0;
    ErrorReturn(ERROR_BADPARM, (ERROR_BADPARM, "analyzeWrite4D(): "
				"bad file name %s", fname));
  }
  if(strcmp(c, ".img") != 0){
    errno = 0;
    ErrorReturn(ERROR_BADPARM, (ERROR_BADPARM, "analyzeWrite4D(): "
				"bad file name %s", fname));
  }

  /* create the file name for the header */
  strcpy(hdr_fname, fname);
  sprintf(hdr_fname + (c - fname), ".hdr");

  /* create the file name for the mat file */
  strcpy(mat_fname, fname);
  sprintf(mat_fname + (c - fname), ".mat");

  memset(&hdr, 0x00, sizeof(hdr));
  hdr.hk.sizeof_hdr = sizeof(hdr);

  hdr.dime.vox_offset = 0.0;

  if(mri->type == MRI_UCHAR)
    {
      hdr.dime.datatype = DT_UNSIGNED_CHAR;
      bytes_per_voxel = 1;
    }
  else if(mri->type == MRI_SHORT)
    {
      hdr.dime.datatype = DT_SIGNED_SHORT;
      bytes_per_voxel = 2;
    }
  /* --- assuming long and int are identical --- */
  else if(mri->type == MRI_INT || mri->type == MRI_LONG)
    {
      hdr.dime.datatype = DT_SIGNED_INT;
      bytes_per_voxel = 4;
    }
  else if(mri->type == MRI_FLOAT)
    {
      hdr.dime.datatype = DT_FLOAT;
      bytes_per_voxel = 4;
    }
  else
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "analyzeWrite4D(): bad data type %d", mri->type));
    }

  hdr.dime.bitpix = 8*bytes_per_voxel;
  memcpy(hdr.dime.vox_units,"mm\0",3);

  hdr.dime.dim[1] = mri->width;    /* ncols */
  hdr.dime.dim[2] = mri->height;   /* nrows */
  hdr.dime.dim[3] = mri->depth;    /* nslices */
  hdr.dime.dim[4] = mri->nframes;
  hdr.dime.dim[0] = 4;              /* flirt expects to be always 4 */

  hdr.dime.pixdim[1] = mri->xsize; /* col res */
  hdr.dime.pixdim[2] = mri->ysize; /* row res */
  hdr.dime.pixdim[3] = mri->zsize; /* slice res */
  hdr.dime.pixdim[4] = mri->tr/1000.0; /* time res in sec*/

  MRIlimits(mri, &min, &max);
  hdr.dime.glmin = (int)min;
  hdr.dime.glmax = (int)max;

  /* Construct the matrix to convert CRS to XYZ, assuming
     that CRS is 1-based */
  T = MRIxfmCRS2XYZ(mri,1);

  det = MatrixDeterminant(T) ;
  if(det == 0){
    printf("WARNING: cannot determine volume orientation, "
           "assuming identity. It's ok if the output is a surface.\n");
    T = MatrixIdentity(4, T) ;
    if(mri->xsize > 0) T->rptr[1][1] = mri->xsize;
    if(mri->ysize > 0) T->rptr[2][2] = mri->ysize;
    if(mri->zsize > 0) T->rptr[3][3] = mri->zsize;
  }
  printf("Analyze Output Matrix\n");
  MatrixPrint(stdout,T);
  printf("--------------------\n");


  /* Set the hist.orient field -- this is not always correct */
  /* see http://wideman-one.com/gw/brain/analyze/formatdoc.htm  */
  if(fabs(mri->z_s) > fabs(mri->z_r) && fabs(mri->z_s) > fabs(mri->z_a)){
    // Transverse: Superior/Inferior > both Right and Anterior
    if(mri->x_r > 0) hdr.hist.orient = 0; // transverse unflipped
    else             hdr.hist.orient = 3; // transverse flipped
  }
  if(fabs(mri->z_a) > fabs(mri->z_r) && fabs(mri->z_a) > fabs(mri->z_s)){
    // Cor: Anterior/Post > both Right and Superior
    if(mri->x_r > 0) hdr.hist.orient = 1; // cor unflipped
    else             hdr.hist.orient = 4; // cor flipped
  }
  if(fabs(mri->z_r) > fabs(mri->z_a) && fabs(mri->z_r) > fabs(mri->z_s)){
    // Sag: Righ/Left > both Anterior and Superior
    if(mri->x_r > 0) hdr.hist.orient = 2; // sag unflipped
    else             hdr.hist.orient = 5; // sag flipped
  }
  hdr.hist.orient = -1;
  printf("INFO: set hdr.hist.orient to %d\n",hdr.hist.orient);

  /* ----- write T to the  .mat file ----- */
  error_value = MatlabWrite(T, mat_fname, "M");
  if(error_value != NO_ERROR) return(error_value);

  /* This matrix converts from XYZ to CRS */
  invT = MatrixInverse(T,NULL);
  /* If the inverse cannot be computed, set to the identity */
  if(invT == NULL) invT = MatrixIdentity(4,NULL);

  /* Load the CRS into the originator field as 3 shorts for SPM */
  /* These come from the last column of invT */
  i1 = (short)(rint(*MATRIX_RELT(invT, 1, 4)));
  memcpy( (&hdr.hist.originator[0]), &i1, sizeof(short));
  i2 = (short)(rint(*MATRIX_RELT(invT, 2, 4)));
  memcpy( (&hdr.hist.originator[0] + sizeof(short)), &i2, sizeof(short));
  i3 = (short)(rint(*MATRIX_RELT(invT, 3, 4)));
  memcpy( (&hdr.hist.originator[0] + 2*sizeof(short)), &i3, sizeof(short));

  MatrixFree(&T);
  MatrixFree(&invT);

  /* ----- write the header ----- */
  if((fp = fopen(hdr_fname, "w")) == NULL)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADFILE,
         (ERROR_BADFILE,
          "analyzeWrite4D(): error opening file %s for writing",
          hdr_fname));
    }
  if(fwrite(&hdr, sizeof(hdr), 1, fp) != 1)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADFILE,
         (ERROR_BADFILE,
          "analyzeWrite4D(): error writing to file %s",
          hdr_fname));
    }
  fclose(fp);

  /* ----- write the data ----- */
  if((fp = fopen(fname, "w")) == NULL)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADFILE,
         (ERROR_BADFILE,
          "analyzeWrite4D(): error opening file %s for writing",
          fname));
    }

  for(frame = 0; frame < mri->nframes; frame ++){
    for(i = 0;i < mri->depth;i++){
      k = i + mri->depth*frame;
      for(j = 0;j < mri->height;j++){
        if(fwrite(mri->slices[k][j], bytes_per_voxel, mri->width, fp) !=
           mri->width){
          errno = 0;
          ErrorReturn(ERROR_BADFILE, (ERROR_BADFILE, "analyzeWrite4D(): "
                                      "error writing to file %s", fname));
        }
      }
    }
  }

  fclose(fp);

  return(0);

} /* end analyzeWrite4D() */

/*-------------------------------------------------------------*/
static void swap_analyze_header(dsr *hdr)
{

  int i;
  char c;

  hdr->hk.sizeof_hdr = swapInt(hdr->hk.sizeof_hdr);
  hdr->hk.extents = swapShort(hdr->hk.extents);
  hdr->hk.session_error = swapShort(hdr->hk.session_error);

  for(i = 0;i < 5;i++)
    hdr->dime.dim[i] = swapShort(hdr->dime.dim[i]);
  hdr->dime.unused1 = swapShort(hdr->dime.unused1);
  hdr->dime.datatype = swapShort(hdr->dime.datatype);
  hdr->dime.bitpix = swapShort(hdr->dime.bitpix);
  hdr->dime.dim_un0 = swapShort(hdr->dime.dim_un0);
  hdr->dime.vox_offset = swapFloat(hdr->dime.vox_offset);
  hdr->dime.roi_scale = swapFloat(hdr->dime.roi_scale);
  hdr->dime.funused1 = swapFloat(hdr->dime.funused1);
  hdr->dime.funused2 = swapFloat(hdr->dime.funused2);
  hdr->dime.cal_max = swapFloat(hdr->dime.cal_max);
  hdr->dime.cal_min = swapFloat(hdr->dime.cal_min);
  hdr->dime.compressed = swapInt(hdr->dime.compressed);
  hdr->dime.verified = swapInt(hdr->dime.verified);
  hdr->dime.glmin = swapInt(hdr->dime.glmin);
  hdr->dime.glmax = swapInt(hdr->dime.glmax);
  for(i = 0;i < 8;i++)
    hdr->dime.pixdim[i] = swapFloat(hdr->dime.pixdim[i]);

  hdr->hist.views = swapInt(hdr->hist.views);
  hdr->hist.vols_added = swapInt(hdr->hist.vols_added);
  hdr->hist.start_field = swapInt(hdr->hist.start_field);
  hdr->hist.field_skip = swapInt(hdr->hist.field_skip);
  hdr->hist.omax = swapInt(hdr->hist.omax);
  hdr->hist.omin = swapInt(hdr->hist.omin);
  hdr->hist.smax = swapInt(hdr->hist.smax);
  hdr->hist.smin = swapInt(hdr->hist.smin);

  /* spm uses the originator char[10] as shorts */
  for(i = 0;i < 5;i++)
    {
      c = hdr->hist.originator[2*i+1];
      hdr->hist.originator[2*i+1] = hdr->hist.originator[2*i];
      hdr->hist.originator[2*i] = c;
    }

} /* end swap_analyze_header */
/*------------------------------------------------------*/

#if 0
static int bad_ras_fill(MRI *mri)
{
  if(mri->slice_direction == MRI_CORONAL)
    {
      mri->x_r = -1.0;  mri->y_r =  0.0;  mri->z_r =  0.0;
      mri->x_a =  0.0;  mri->y_a =  0.0;  mri->z_a =  1.0;
      mri->x_s =  0.0;  mri->y_s = -1.0;  mri->z_s =  0.0;
      mri->c_r = 0.0;  mri->c_a = 0.0;  mri->c_s = 0.0;
    }
  else if(mri->slice_direction == MRI_SAGITTAL)
    {
      mri->x_r =  0.0;  mri->y_r =  0.0;  mri->z_r = -1.0;
      mri->x_a =  1.0;  mri->y_a =  0.0;  mri->z_a =  0.0;
      mri->x_s =  0.0;  mri->y_s =  1.0;  mri->z_s =  0.0;
      mri->c_r = 0.0;  mri->c_a = 0.0;  mri->c_s = 0.0;
    }
  else if(mri->slice_direction == MRI_HORIZONTAL)
    {
      mri->x_r =  1.0;  mri->y_r =  0.0;  mri->z_r =  0.0;
      mri->x_a =  0.0;  mri->y_a = -1.0;  mri->z_a =  0.0;
      mri->x_s =  0.0;  mri->y_s =  0.0;  mri->z_s =  1.0;
      mri->c_r = 0.0;  mri->c_a = 0.0;  mri->c_s = 0.0;
    }
  else
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM, "bad_ras_fill(): unknown slice direction"));
    }

  return(NO_ERROR);

} /* end bad_ras_fill() */
#endif

#if 0
// #ifdef VT_TO_CV

static int voxel_center_to_center_voxel(MRI *mri, float *x, float *y, float *z)
{

  int result;
  MATRIX *m, *i, *r, *mi;

  if(!mri->ras_good_flag)
    if((result = bad_ras_fill(mri)) != NO_ERROR)
      return(result);

  if((m = extract_i_to_r(mri)) == NULL)
    return(ERROR_BADPARM);

  mi = MatrixInverse(m, NULL);
  if(mi == NULL)
    {
      MatrixFree(&m);
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "voxel_center_to_center_voxel(): error inverting matrix"));
    }

  r = MatrixAlloc(4, 1, MATRIX_REAL);
  if(r == NULL)
    {
      MatrixFree(&m);
      MatrixFree(&mi);
      errno = 0;
      ErrorReturn
        (ERROR_NOMEMORY,
         (ERROR_NOMEMORY,
          "voxel_center_to_center_voxel(): couldn't allocate matrix"));
    }

  *MATRIX_RELT(r, 1, 1) = 0.0;
  *MATRIX_RELT(r, 2, 1) = 0.0;
  *MATRIX_RELT(r, 3, 1) = 0.0;
  *MATRIX_RELT(r, 4, 1) = 1.0;

  i = MatrixMultiply(mi, r, NULL);
  if(i == NULL)
    {
      MatrixFree(&m);
      MatrixFree(&mi);
      MatrixFree(&r);
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "voxel_center_to_center_voxel(): "
          "error in matrix multiplication"));
    }

  *x = *MATRIX_RELT(i, 1, 1);
  *y = *MATRIX_RELT(i, 2, 1);
  *z = *MATRIX_RELT(i, 3, 1);

  MatrixFree(&m);
  MatrixFree(&mi);
  MatrixFree(&i);
  MatrixFree(&r);

  return(NO_ERROR);

} /* end voxel_center_to_center_voxel() */

// #endif

static int center_voxel_to_voxel_center(MRI *mri, float x, float y, float z)
{

  int result;
  MATRIX *m;

  if(!mri->ras_good_flag)
    if((result = bad_ras_fill(mri)) != NO_ERROR)
      return(result);

  if((m = extract_i_to_r(mri)) == NULL)
    return(ERROR_BADPARM);

  *MATRIX_RELT(m, 1, 4) =
    0 - (*MATRIX_RELT(m, 1, 1) * x +
         *MATRIX_RELT(m, 1, 2) * y +
         *MATRIX_RELT(m, 1, 3) * z);
  *MATRIX_RELT(m, 2, 4) =
    0 - (*MATRIX_RELT(m, 2, 1) * x +
         *MATRIX_RELT(m, 2, 2) * y +
         *MATRIX_RELT(m, 2, 3) * z);
  *MATRIX_RELT(m, 3, 4) =
    0 - (*MATRIX_RELT(m, 3, 1) * x +
         *MATRIX_RELT(m, 3, 2) * y +
         *MATRIX_RELT(m, 3, 3) * z);

  apply_i_to_r(mri, m);

  MatrixFree(&m);

  return(NO_ERROR);

} /* end center_voxel_to_voxel_center() */
#endif // if 0

static MRI *gdfRead(char *fname, int read_volume)
{

  MRI *mri;
  FILE *fp;
  char line[STRLEN];
  char *c;
  char file_path[STRLEN];
  float ipr[2];
  float st;
  char units_string[STRLEN], orientation_string[STRLEN],
    data_type_string[STRLEN];
  int size[2];
  int path_d, ipr_d, st_d, u_d, dt_d, o_d, s_d,
    x_ras_d, y_ras_d, z_ras_d, c_ras_d;
  int data_type;
  int orientation = MRI_UNDEFINED;
  char *or;
  char os_orig[STRLEN];
  float units_factor;
  char file_path_1[STRLEN], file_path_2[STRLEN];
  int i, j, k;
  short *sbuf = NULL;
  float *fbuf = NULL;
  unsigned char *ucbuf = NULL;
  int n_files;
  char fname_use[STRLEN];
  int pad_zeros_flag;
  int file_offset = 0;
  float x_r, x_a, x_s;
  float y_r, y_a, y_s;
  float z_r, z_a, z_s;
  float c_r, c_a, c_s;

  if((fp = fopen(fname, "r")) == NULL)
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE, "gdfRead(): error opening file %s", fname));
    }

  /* --- defined flags --- */
  path_d = ipr_d = st_d = u_d = dt_d =
    o_d = s_d = x_ras_d = y_ras_d = z_ras_d = c_ras_d = FALSE;

  while(fgets(line, STRLEN, fp) != NULL)
    {

      /* --- strip the newline --- */
      if((c = strrchr(line, '\n')) != NULL)
        *c = '\0';

      if(strncmp(line, "IMAGE_FILE_PATH", 15) == 0)
        {
          sscanf(line, "%*s %s", file_path);
          path_d = TRUE;
        }
      else if(strncmp(line, "IP_RES", 6) == 0)
        {
          sscanf(line, "%*s %f %f", &ipr[0], &ipr[1]);
          ipr_d = TRUE;
        }
      else if(strncmp(line, "SL_THICK", 8) == 0)
        {
          sscanf(line, "%*s %f", &st);
          st_d = TRUE;
        }
      else if(strncmp(line, "UNITS", 5) == 0)
        {
          sscanf(line, "%*s %s", units_string);
          u_d = TRUE;
        }
      else if(strncmp(line, "SIZE", 4) == 0)
        {
          sscanf(line, "%*s %d %d", &size[0], &size[1]);
          s_d = TRUE;
        }
      else if(strncmp(line, "FS_X_RAS", 8) == 0)
        {
          sscanf(line, "%*s %f %f %f", &x_r, &x_a, &x_s);
          x_ras_d = TRUE;
        }
      else if(strncmp(line, "FS_Y_RAS", 8) == 0)
        {
          sscanf(line, "%*s %f %f %f", &y_r, &y_a, &y_s);
          y_ras_d = TRUE;
        }
      else if(strncmp(line, "FS_Z_RAS", 8) == 0)
        {
          sscanf(line, "%*s %f %f %f", &z_r, &z_a, &z_s);
          z_ras_d = TRUE;
        }
      else if(strncmp(line, "FS_C_RAS", 8) == 0)
        {
          sscanf(line, "%*s %f %f %f", &c_r, &c_a, &c_s);
          c_ras_d = TRUE;
        }
      else if(strncmp(line, "DATA_TYPE", 9) == 0)
        {
          strcpy(data_type_string, &line[10]);
          dt_d = TRUE;
        }
      else if(strncmp(line, "ORIENTATION", 11) == 0)
        {
          sscanf(line, "%*s %s", orientation_string);
          strcpy(os_orig, orientation_string);
          o_d = TRUE;
        }
      else if(strncmp(line, "FILE_OFFSET", 11) == 0)
        {
          sscanf(line, "%*s %d", &file_offset);
        }
      else
        {
        }

    }

  fclose(fp);

  if(!(path_d && ipr_d && st_d && s_d))
    {
      errno = 0;
      ErrorPrintf
        (ERROR_BADPARM, "gdfRead(): missing field(s) from %s:", fname);
      if(!path_d)
        ErrorPrintf(ERROR_BADPARM, "  IMAGE_FILE_PATH");
      if(!ipr_d)
        ErrorPrintf(ERROR_BADPARM, "  IP_RES");
      if(!st_d)
        ErrorPrintf(ERROR_BADPARM, "  SL_THICK");
      if(!s_d)
        ErrorPrintf(ERROR_BADPARM, "  SIZE");
      return(NULL);
    }

  if(!(o_d))
    {
      if(x_ras_d && y_ras_d && z_ras_d)
        {
          printf("missing field ORIENTATION in file %s, "
                 "but you've got {xyz}_{ras}, so never mind\n", fname);
        }
      else
        {
          printf
            ("missing field ORIENTATION in file %s; assuming 'coronal'\n",
             fname);
          sprintf(orientation_string, "coronal");
        }
    }

  if(!(dt_d))
    {
      printf("missing field DATA_TYPE in file %s; assuming 'short'\n", fname);
      sprintf(data_type_string, "short");
    }

  if(!(u_d))
    {
      printf("missing field UNITS in file %s; assuming 'mm'\n", fname);
      sprintf(units_string, "mm");
    }

  StrLower(data_type_string);
  StrLower(orientation_string);
  StrLower(units_string);

  /* --- data type --- */
  if(strcmp(data_type_string, "short") == 0)
    data_type = MRI_SHORT;
  else if(strcmp(data_type_string, "float") == 0)
    data_type = MRI_FLOAT;
  else if(strcmp(data_type_string, "unsigned char") == 0)
    data_type = MRI_UCHAR;
  else
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADPARM,
          "gdfRead(): unknown data type '%s'", data_type_string));
    }

  /* --- orientation --- */
  or = strrchr(orientation_string, ' ');
  or = (or == NULL ? orientation_string : or+1);
  if(strncmp(or, "cor", 3) == 0)
    orientation = MRI_CORONAL;
  else if(strncmp(or, "sag", 3) == 0)
    orientation = MRI_SAGITTAL;
  else if(strncmp(or, "ax", 2) == 0 || strncmp(or, "hor", 3) == 0)
    orientation = MRI_HORIZONTAL;
  else if(!(x_ras_d && y_ras_d && z_ras_d))
    {
      errno = 0;
      ErrorReturn(NULL,
                  (ERROR_BADPARM,
                   "gdfRead(): can't determine orientation from string '%s'",
                   os_orig));
    }

  if(strcmp(units_string, "mm") == 0)
    units_factor = 1.0;
  else if(strcmp(units_string, "cm") == 0)
    units_factor = 1.0;
  else if(strcmp(units_string, "m") == 0)
    units_factor = 100.0;
  else
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADPARM, "gdfRead(): unknown units '%s'", units_string));
    }

  ipr[0] /= units_factor;
  ipr[1] /= units_factor;
  st /= units_factor;

  strcpy(file_path_1, file_path);
  c = strrchr(file_path_1, '*');
  if(c == NULL)
    {
      errno = 0;
      ErrorReturn(NULL,
                  (ERROR_BADPARM,
                   "gdfRead(): file path %s does not contain '*'\n",
                   file_path));
    }

  *c = '\0';
  c++;
  strcpy(file_path_2, c);

#if 0

  /* cardviews takes IMAGE_FILE_PATH relative to the working */
  /* directory -- so we skip this step - ch                  */

  /* ----- relative path -- go from directory with the .gdf file ----- */
  if(file_path_1[0] != '/')
    {

      char gdf_path[STRLEN];

      if(fname[0] == '/')
        sprintf(gdf_path, "%s", fname);
      else
        sprintf(gdf_path, "./%s", fname);

      c = strrchr(gdf_path, '/');
      c[1] = '\0';

      strcat(gdf_path, file_path_1);
      strcpy(file_path_1, gdf_path);

    }
#endif

  pad_zeros_flag = FALSE;

  n_files = 0;
  do
    {
      n_files++;
      sprintf(fname_use, "%s%d%s", file_path_1, n_files, file_path_2);
    } while(FileExists(fname_use));

  /* ----- try padding the zeros if no files are found ----- */
  if(n_files == 1)
    {

      pad_zeros_flag = TRUE;

      n_files = 0;
      do
        {
          n_files++;
          sprintf(fname_use, "%s%03d%s", file_path_1, n_files, file_path_2);
        } while(FileExists(fname_use));

      /* ----- still a problem? ----- */
      if(n_files == 1)
        {
          errno = 0;
          ErrorReturn(NULL, (ERROR_BADFILE,
                             "gdfRead(): can't find file %s%d%s or %s\n",
                             file_path_1, 1, file_path_2, fname_use));
        }

    }

  n_files--;

  if(read_volume)
    mri = MRIallocSequence(size[0], size[1], n_files, data_type, 1);
  else
    mri = MRIallocHeader(size[0], size[1], n_files, data_type);

  mri->xsize = ipr[0];
  mri->ysize = ipr[1];
  mri->zsize = st;

  mri->xend = mri->width  * mri->xsize / 2.0;
  mri->xstart = -mri->xend;
  mri->yend = mri->height * mri->ysize / 2.0;
  mri->ystart = -mri->yend;
  mri->zend = mri->depth  * mri->zsize / 2.0;
  mri->zstart = -mri->zend;

  strcpy(mri->fname, fname);

  /* --- set volume orientation --- */
  if(x_ras_d && y_ras_d && z_ras_d)
    {
      mri->x_r = x_r;  mri->x_a = x_a;  mri->x_s = x_s;
      mri->y_r = y_r;  mri->y_a = y_a;  mri->y_s = y_s;
      mri->z_r = z_r;  mri->z_a = z_a;  mri->z_s = z_s;
      mri->ras_good_flag = TRUE;
    }
  else
    {
      /*
         direction cosine is not set.  we pick a particular kind
         of direction cosine.  If the volume is different you have
         to modify (how?)
      */
      if (setDirectionCosine(mri, orientation) != NO_ERROR)
        {
          MRIfree(&mri);
          return NULL;
        }
      printf("warning: gdf volume may be incorrectly oriented\n");
    }

  /* --- set volume center --- */
  if(c_ras_d)
    {
      mri->c_r = c_r;  mri->c_a = c_a;  mri->c_s = c_s;
    }
  else
    {
      mri->c_r = mri->c_a = mri->c_s = 0.0;
      printf("warning: gdf volume may be incorrectly centered\n");
    }

  if(!read_volume)
    return(mri);

  if(mri->type == MRI_UCHAR)
    {
      ucbuf = (unsigned char *)malloc(mri->width);
      if(ucbuf == NULL)
        {
          MRIfree(&mri);
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_NOMEMORY,
              "gdfRead(): error allocating %d bytes for read buffer",
              mri->width));
        }
    }
  else if(mri->type == MRI_SHORT)
    {
      sbuf = (short *)malloc(mri->width * sizeof(short));
      if(sbuf == NULL)
        {
          MRIfree(&mri);
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_NOMEMORY,
              "gdfRead(): error allocating %d bytes for read buffer",
              mri->width * sizeof(short)));
        }
    }
  else if(mri->type == MRI_FLOAT)
    {
      fbuf = (float *)malloc(mri->width * sizeof(float));
      if(fbuf == NULL)
        {
          MRIfree(&mri);
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_NOMEMORY,
              "gdfRead(): error allocating %d bytes for read buffer",
              mri->width * sizeof(float)));
        }
    }
  else
    {
      MRIfree(&mri);
      errno = 0;
      ErrorReturn(NULL, (ERROR_BADPARM,
                         "gdfRead(): internal error; data type %d "
                         "accepted but not supported in read",
                         mri->type));
    }

  for(i = 1;i <= n_files;i++)
    {

      if(pad_zeros_flag)
        sprintf(fname_use, "%s%03d%s", file_path_1, i, file_path_2);
      else
        sprintf(fname_use, "%s%d%s", file_path_1, i, file_path_2);

      fp = fopen(fname_use, "r");
      if(fp == NULL)
        {
          if(mri->type == MRI_UCHAR)
            free(ucbuf);
          if(mri->type == MRI_SHORT)
            free(sbuf);
          if(mri->type == MRI_FLOAT)
            free(fbuf);
          MRIfree(&mri);
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADFILE,
              "gdfRead(): error opening file %s", fname_use));
        }

      fseek(fp, file_offset, SEEK_SET);

      if(mri->type == MRI_UCHAR)
        {
          for(j = 0;j < mri->height;j++)
            {
              if(fread(ucbuf, 1, mri->width, fp) != mri->width)
                {
                  free(ucbuf);
                  MRIfree(&mri);
                  errno = 0;
                  ErrorReturn(NULL, (ERROR_BADFILE,
                                     "gdfRead(): error reading from file %s",
                                     fname_use));
                }
              for(k = 0;k < mri->width;k++)
                MRIvox(mri, k, j, i-1) = ucbuf[k];
            }
        }

      if(mri->type == MRI_SHORT)
        {
          for(j = 0;j < mri->height;j++)
            {
              if(fread(sbuf, sizeof(short), mri->width, fp) != mri->width)
                {
                  free(sbuf);
                  MRIfree(&mri);
                  errno = 0;
                  ErrorReturn(NULL, (ERROR_BADFILE,
                                     "gdfRead(): error reading from file %s",
                                     fname_use));
                }
#if (BYTE_ORDER == LITTLE_ENDIAN)
              for(k = 0;k < mri->width;k++)
                MRISvox(mri, k, j, i-1) = orderShortBytes(sbuf[k]);
#else
              for(k = 0;k < mri->width;k++)
                MRISvox(mri, k, j, i-1) = sbuf[k];
#endif
            }
        }

      if(mri->type == MRI_FLOAT)
        {
          for(j = 0;j < mri->height;j++)
            {
              if(fread(fbuf, sizeof(float), mri->width, fp) != mri->width)
                {
                  free(fbuf);
                  MRIfree(&mri);
                  errno = 0;
                  ErrorReturn
                    (NULL,
                     (ERROR_BADFILE,
                      "gdfRead(): error reading from file %s", fname_use));
                }
#if (BYTE_ORDER == LITTLE_ENDIAN)
              for(k = 0;k < mri->width;k++)
                MRIFvox(mri, k, j, i-1) = orderFloatBytes(fbuf[k]);
#else
              for(k = 0;k < mri->width;k++)
                MRIFvox(mri, k, j, i-1) = fbuf[k];
#endif
            }
        }

      fclose(fp);

    }

  if(mri->type == MRI_UCHAR)
    free(ucbuf);
  if(mri->type == MRI_SHORT)
    free(sbuf);
  if(mri->type == MRI_FLOAT)
    free(fbuf);

  return(mri);

} /* end gdfRead() */

static int gdfWrite(MRI *mri, char *fname)
{

  FILE *fp;
  int i, j;
  char im_fname[STRLEN];
  unsigned char *buf;
  int buf_size = 0;

  if(strlen(mri->gdf_image_stem) == 0)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM, "GDF write attempted without GDF image stem"));
    }

  if(mri->nframes != 1)
    {
      errno = 0;
      ErrorReturn(ERROR_BADPARM, (ERROR_BADPARM,
                                  "GDF write attempted with %d frames "
                                  "(supported only for 1 frame)",
                                  mri->nframes));
    }

  /* ----- type checks first ----- */

  if(mri->type != MRI_UCHAR &&
     mri->type != MRI_FLOAT &&
     mri->type != MRI_SHORT)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "gdfWrite(): bad data type (%d) for GDF write "
          "(only uchar, float, short supported)", mri->type));
    }

  /* ----- then write the image files ----- */

  printf("writing GDF image files...\n");

  if(mri->type == MRI_UCHAR)
    buf_size = mri->width * sizeof(unsigned char);
  if(mri->type == MRI_FLOAT)
    buf_size = mri->width * sizeof(float);
  if(mri->type == MRI_SHORT)
    buf_size = mri->width * sizeof(short);

  buf = (unsigned char *)malloc(buf_size);
  if(buf == NULL)
    {
      errno = 0;
      ErrorReturn
        (ERROR_NO_MEMORY,
         (ERROR_NO_MEMORY,
          "gdfWrite(): no memory for voxel write buffer"));
    }

  for(i = 0;i < mri->depth;i++)
    {

      sprintf(im_fname, "%s_%d.img", mri->gdf_image_stem, i+1);
      fp = fopen(im_fname, "w");
      if(fp == NULL)
        {
          free(buf);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "gdfWrite(): error opening file %s", im_fname));
        }

      for(j = 0;j < mri->height;j++)
        {
          memcpy(buf, mri->slices[i][j], buf_size);
#if (BYTE_ORDER == LITTLE_ENDIAN)
          if(mri->type == MRI_FLOAT)
            byteswapbuffloat(buf, buf_size);
          if(mri->type == MRI_SHORT)
            byteswapbufshort(buf, buf_size);
#endif
          fwrite(buf, 1, buf_size, fp);
        }

      fclose(fp);

    }

  free(buf);

  /* ----- and finally the info file ----- */

  printf("writing GDF info file...\n");

  fp = fopen(fname, "w");
  if(fp == NULL)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADFILE,
         (ERROR_BADFILE, "gdfWrite(): error opening file %s", fname));
    }

  fprintf(fp, "GDF FILE VERSION3\n");
  fprintf(fp, "START MAIN HEADER\n");
  fprintf(fp, "IMAGE_FILE_PATH %s_*.img\n", mri->gdf_image_stem);
  fprintf(fp, "SIZE %d %d\n", mri->width, mri->height);
  fprintf(fp, "IP_RES %g %g\n", mri->xsize, mri->ysize);
  fprintf(fp, "SL_THICK %g\n", mri->zsize);
  if(mri->ras_good_flag)
    {
      fprintf(fp, "FS_X_RAS %f %f %f\n", mri->x_r, mri->x_a, mri->x_s);
      fprintf(fp, "FS_Y_RAS %f %f %f\n", mri->y_r, mri->y_a, mri->y_s);
      fprintf(fp, "FS_Z_RAS %f %f %f\n", mri->z_r, mri->z_a, mri->z_s);
      fprintf(fp, "FS_C_RAS %f %f %f\n", mri->c_r, mri->c_a, mri->c_s);
    }
  fprintf(fp, "FILE_OFFSET 0\n");
  if(mri->type == MRI_UCHAR)
    fprintf(fp, "DATA_TYPE unsigned char\n");
  if(mri->type == MRI_FLOAT)
    fprintf(fp, "DATA_TYPE float\n");
  if(mri->type == MRI_SHORT)
    fprintf(fp, "DATA_TYPE short\n");
  fprintf(fp, "END MAIN HEADER\n");

  fclose(fp);

  return(NO_ERROR);

}

static int parc_fill(short label_value, short seed_x, short seed_y)
{

  if(seed_x < 0 || seed_x >= 512 || seed_y < 0 || seed_y >= 512)
    return(NO_ERROR);

  if(cma_field[seed_x][seed_y] == label_value)
    return(NO_ERROR);

  cma_field[seed_x][seed_y] = label_value;

  parc_fill(label_value, seed_x + 1, seed_y    );
  parc_fill(label_value, seed_x - 1, seed_y    );
  parc_fill(label_value, seed_x    , seed_y + 1);
  parc_fill(label_value, seed_x    , seed_y - 1);

  return(NO_ERROR);

} /* end parc_fill() */

static int register_unknown_label(char *label)
{

  int i;

  if(n_unknown_labels == MAX_UNKNOWN_LABELS)
    return(NO_ERROR);

  for(i = 0;i < n_unknown_labels;i++)
    {
      if(strcmp(unknown_labels[i], label) == 0)
        return(NO_ERROR);
    }

  strcpy(unknown_labels[n_unknown_labels], label);
  n_unknown_labels++;

  return(NO_ERROR);

} /* end register_unknown_label() */

static int clear_unknown_labels(void)
{

  n_unknown_labels = 0;

  return(NO_ERROR);

} /* end clear_unknown_labels() */

static int print_unknown_labels(char *prefix)
{

  int i;

  for(i = 0;i < n_unknown_labels;i++)
    printf("%s%s\n", prefix, unknown_labels[i]);

  return(NO_ERROR);

} /* end print_unknown_labels() */

/* replaced 25 Feb 2003 ch */
#if 0
static int read_otl_file(FILE *fp,
                         MRI *mri,
                         int slice,
                         mriColorLookupTableRef color_table,
                         int fill_flag,
                         int translate_label_flag,
                         int zero_outlines_flag)
{
  int n_outlines = -1;
  int n_rows, n_cols;
  char label[STRLEN], label_to_compare[STRLEN];
  int seed_x, seed_y;
  char line[STRLEN];
  int main_header_flag;
  int i, j;
  int gdf_header_flag;
  char type[STRLEN], global_type[STRLEN];
  char *c;
  short *points;
  int n_read;
  short label_value;
  float scale_x, scale_y;
  int source_x, source_y;
  char alt_compare[STRLEN];
  int empty_label_flag;
  int ascii_short_flag;
  int row;
  char *translate_start;
  int internal_structures_flag = FALSE;

  for(i = 0;i < 512;i++)
    memset(cma_field[i], 0x00, 512 * 2);

  fgets(line, STRLEN, fp);
  if(strncmp(line, "GDF FILE VERSION", 15) != 0)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADFILE,
         (ERROR_BADFILE,
          "otl slice %s does not appear to be a GDF file", slice));
    }

  main_header_flag = FALSE;
  while(!main_header_flag)
    {
      if(feof(fp))
        {
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "premature EOF () in otl file %d", slice));
        }
      fgets(line, STRLEN, fp);
      if(strncmp(line, "START MAIN HEADER", 17) == 0)
        main_header_flag = TRUE;
    }

  n_cols = -1;
  type[0] = '\0';
  global_type[0] = '\0';

  while(main_header_flag)
    {
      if(feof(fp))
        {
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "premature EOF (in main header) in otl file %d", slice));
        }
      fgets(line, STRLEN, fp);
      if(strncmp(line, "END MAIN HEADER", 15) == 0)
        main_header_flag = FALSE;
      if(strncmp(line, "ONUM ", 5) == 0)
        sscanf(line, "%*s %d", &n_outlines);
      if(strncmp(line, "COL_NUM", 7) == 0)
        sscanf(line, "%*s %d", &n_cols);
      if(strncmp(line, "TYPE", 4) == 0)
        {
          strcpy(global_type, &(line[5]));
          c = strrchr(global_type, '\n');
          if(c != NULL)
            *c = '\0';
        }
    }

  if(n_outlines == -1)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "bad or undefined ONUM in otl file %d", slice));
    }

  for(i = 0;i < n_outlines;i++)
    {
      if(feof(fp))
        {
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "premature EOF (ready for next outline) in otl file %d",
              slice));
        }

      gdf_header_flag = FALSE;

      while(!gdf_header_flag)
        {
          if(feof(fp))
            {
              errno = 0;
              ErrorReturn
                (ERROR_BADFILE,
                 (ERROR_BADFILE,
                  "premature EOF (searching for gdf header) "
                  "in otl file %d", slice));
            }
          fgets(line, STRLEN, fp);
          if(strncmp(line, "START GDF HEADER", 16) == 0)
            gdf_header_flag = TRUE;
        }

      n_rows = -1;
      seed_x = seed_y = -1;
      label[0] = '\0';
      type[0] = '\0';

      empty_label_flag = 0;

      while(gdf_header_flag)
        {

          if(feof(fp))
            {
              errno = 0;
              ErrorReturn
                (ERROR_BADFILE,
                 (ERROR_BADFILE,
                  "premature EOF (in gdf header) in otl file %d",
                  slice));
            }
          fgets(line, STRLEN, fp);
          if(strncmp(line, "END GDF HEADER", 14) == 0)
            gdf_header_flag = FALSE;
          if(strncmp(line, "ROW_NUM", 7) == 0)
            sscanf(line, "%*s %d", &n_rows);
          if(strncmp(line, "COL_NUM", 7) == 0)
            sscanf(line, "%*s %d", &n_cols);
          if(strncmp(line, "TYPE", 4) == 0)
            {
              strcpy(type, &(line[5]));
              c = strrchr(type, '\n');
              if(c != NULL)
                *c = '\0';
            }
          if(strncmp(line, "SEED", 4) == 0)
            sscanf(line, "%*s %d %d", &seed_x, &seed_y);
          if(strncmp(line, "LABEL", 5) == 0)
            {

              strcpy(label, &(line[6]));
              c = strrchr(label, '\n');
              if(c != NULL)
                *c = '\0';

              /* exterior -> cortex, if desired */
              if(translate_label_flag)
                {
                  translate_start = strstr(label, "Exterior");
                  if(translate_start != NULL)
                    sprintf(translate_start, "Cortex");
                  else
                    {
                      translate_start = strstr(label, "exterior");
                      if(translate_start != NULL)
                        sprintf(translate_start, "cortex");
                    }
                }

              /* warning if there's an "exterior" or
                 "cortex" after any other label */
              if(strstr(label, "Exterior") == 0 \
                 || strstr(label, "exterior") == 0 \
                 || strstr(label, "Cortex") == 0 \
                 || strstr(label, "cortex") == 0)
                {
                  if(internal_structures_flag)
                    printf("WARNING: label \"%s\" following "
                           "non-exterior labels in slice %d\n",
                           label, slice);
                  internal_structures_flag = FALSE;
                }
              else
                internal_structures_flag = TRUE;

            }

        }

      if(n_rows < 0)
        {
          errno = 0;
          ErrorReturn(ERROR_BADPARM,
                      (ERROR_BADPARM,
                       "bad or undefined ROW_NUM in otl file %d", slice));
        }

      if(n_cols != 2)
        {
          errno = 0;
          ErrorReturn(ERROR_BADPARM,
                      (ERROR_BADPARM,
                       "bad or undefined COL_NUM in otl file %d", slice));
        }

      if(label[0] == '\0')
        {
          empty_label_flag = 1;
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "empty LABEL in otl file %d (outline %d)", slice, i);
        }

      if(seed_x < 0 || seed_x >= 512 || seed_y < 0 || seed_y >= 512)
        {
          errno = 0;
          ErrorReturn(ERROR_BADPARM,
                      (ERROR_BADPARM,
                       "bad or undefined SEED in otl file %d", slice));
        }

      if(type[0] == '\0')
        strcpy(type, global_type);

      if(strcmp(type, "ascii short") == 0)
        ascii_short_flag = TRUE;
      else if(strcmp(type, "short") == 0)
        ascii_short_flag = FALSE;
      else if(type[0] == '\0')
        {
          errno = 0;
          ErrorReturn(ERROR_UNSUPPORTED,
                      (ERROR_UNSUPPORTED,
                       "undefined TYPE in otl file %d", slice));
        }
      else
        {
          errno = 0;
          ErrorReturn(ERROR_UNSUPPORTED,
                      (ERROR_UNSUPPORTED,
                       "unsupported TYPE \"%s\" in otl file %d", type, slice));
        }

      do
        {
          fgets(line, STRLEN, fp);
          if(feof(fp))
            {
              errno = 0;
              ErrorReturn
                (ERROR_BADFILE,
                 (ERROR_BADFILE,
                  "premature EOF (searching for points) in otl file %d",
                  slice));
            }
        } while(strncmp(line, "START POINTS", 12) != 0);

      points = (short *)malloc(2 * n_rows * sizeof(short));
      if(points == NULL)
        {
          errno = 0;
          ErrorReturn
            (ERROR_NOMEMORY,
             (ERROR_NOMEMORY,
              "error allocating memory for points in otl file %d", slice));
        }

      if(ascii_short_flag)
        {
          for(row = 0;row < n_rows;row++)
            {
              fgets(line, STRLEN, fp);
              if(feof(fp))
                {
                  free(points);
                  errno = 0;
                  ErrorReturn
                    (ERROR_BADFILE,
                     (ERROR_BADFILE,
                      "premature end of file while reading points "
                      "from otl file %d",
                      slice));
                }
              sscanf(line, "%hd %hd", &(points[2*row]), &(points[2*row+1]));
            }
        }
      else
        {
          n_read = fread(points, 2, n_rows * 2, fp);
          if(n_read != n_rows * 2)
            {
              free(points);
              errno = 0;
              ErrorReturn(ERROR_BADFILE,
                          (ERROR_BADFILE,
                           "error reading points from otl file %d", slice));
            }
#if (BYTE_ORDER == LITTLE_ENDIAN)
          swab(points, points, 2 * n_rows * sizeof(short));
#endif
        }

      fgets(line, STRLEN, fp);
      if(strncmp(line, "END POINTS", 10) != 0)
        {
          free(points);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "error after points (\"END POINTS\" expected "
              "but not found) in otl file %d", slice));
        }

      if(!empty_label_flag)
        {
          strcpy(label_to_compare, label);
          for(j = 0;label_to_compare[j] != '\0';j++)
            {
              if(label_to_compare[j] == '\n')
                label_to_compare[j] = '\0';
              if(label_to_compare[j] == ' ')
                label_to_compare[j] = '-';
            }

          /* --- strip 'Left' and 'Right'; --- */
          strcpy(alt_compare, label_to_compare);
          StrLower(alt_compare);
          if(strncmp(alt_compare, "left-", 5) == 0)
            strcpy(alt_compare, &(label_to_compare[5]));
          else if(strncmp(alt_compare, "right-", 6) == 0)
            strcpy(alt_compare, &(label_to_compare[6]));

          /* --- strip leading and trailing spaces (now dashes) --- */

          /* leading */
          for(j = 0;label_to_compare[j] == '-';j++);
          if(label_to_compare[j] != '\0')
            {
              for(c = label_to_compare;*(c+j) != '\0';c++)
                *c = *(c+j);
              *c = *(c+j);
            }

          /* trailing */
          for(j = strlen(label_to_compare) - 1;
              j >= 0 && label_to_compare[j] == '-';
              j--);
          if(j < 0) /* all dashes! */
            {
              /* for now, let this fall through to an unknown label */
            }
          else /* j is the index of the last non-dash character */
            {
              label_to_compare[j+1] = '\0';
            }


          label_value = -1;
          for(j = 0;j < color_table->mnNumEntries;j++)
            if(strcmp(color_table->maEntries[j].msLabel,
                      label_to_compare) == 0 ||
               strcmp(color_table->maEntries[j].msLabel, alt_compare) == 0)
              label_value = j;

          if(label_value == -1)
            {
              register_unknown_label(label);
            }
          else
            {

              for(j = 0;j < n_rows;j++)
                cma_field[points[2*j]][points[2*j+1]] = label_value;

              if(fill_flag && label_value != 0)
                parc_fill(label_value, seed_x, seed_y);

              if(zero_outlines_flag)
                {
                  for(j = 0;j < n_rows;j++)
                    cma_field[points[2*j]][points[2*j+1]] = 0;
                }


            }

        }

      free(points);

    }

  scale_x = 512 / mri->width;
  scale_y = 512 / mri->height;

  for(i = 0;i < mri->width;i++)
    for(j = 0;j < mri->height;j++)
      {
        source_x = (int)floor(scale_x * (float)i);
        source_y = (int)floor(scale_y * (float)j);
        if(source_x < 0)
          source_x = 0;
        if(source_x > 511)
          source_x = 511;
        if(source_y < 0)
          source_y = 0;
        if(source_y > 511)
          source_y = 511;
        MRISvox(mri, i, j, slice-1) = cma_field[source_x][source_y];
      }

  return(NO_ERROR);

} /* end read_otl_file() */
#endif

static int read_otl_file(FILE *fp,
                         MRI *mri,
                         int slice,
                         mriColorLookupTableRef color_table,
                         int fill_flag,
                         int translate_label_flag,
                         int zero_outlines_flag)
{

  int n_outlines = -1;
  int n_rows, n_cols;
  char label[STRLEN], label_to_compare[STRLEN];
  int seed_x, seed_y;
  char line[STRLEN];
  int main_header_flag;
  int i, j;
  int gdf_header_flag;
  char type[STRLEN], global_type[STRLEN];
  char *c;
  short *points;
  int n_read;
  short label_value;
  float scale_x, scale_y;
  int source_x, source_y;
  char alt_compare[STRLEN];
  int empty_label_flag;
  int ascii_short_flag;
  int row;
  char *translate_start;
  int internal_structures_flag = FALSE;
  CMAoutlineField *of;

  fgets(line, STRLEN, fp);
  if(strncmp(line, "GDF FILE VERSION", 15) != 0)
    {
      errno = 0;
      ErrorReturn(ERROR_BADFILE,
                  (ERROR_BADFILE,
                   "otl slice %s does not appear to be a GDF file", slice));
    }

  main_header_flag = FALSE;
  while(!main_header_flag)
    {
      if(feof(fp))
        {
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE, "premature EOF () in otl file %d", slice));
        }
      fgets(line, STRLEN, fp);
      if(strncmp(line, "START MAIN HEADER", 17) == 0)
        main_header_flag = TRUE;
    }

  n_cols = -1;
  type[0] = '\0';
  global_type[0] = '\0';

  while(main_header_flag)
    {
      if(feof(fp))
        {
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "premature EOF (in main header) in otl file %d", slice));
        }
      fgets(line, STRLEN, fp);
      if(strncmp(line, "END MAIN HEADER", 15) == 0)
        main_header_flag = FALSE;
      if(strncmp(line, "ONUM ", 5) == 0)
        sscanf(line, "%*s %d", &n_outlines);
      if(strncmp(line, "COL_NUM", 7) == 0)
        sscanf(line, "%*s %d", &n_cols);
      if(strncmp(line, "TYPE", 4) == 0)
        {
          strcpy(global_type, &(line[5]));
          c = strrchr(global_type, '\n');
          if(c != NULL)
            *c = '\0';
        }
    }

  if(n_outlines == -1)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,
          "bad or undefined ONUM in otl file %d", slice));
    }

  of = CMAoutlineFieldAlloc(2*mri->width, 2*mri->height);
  if(of == NULL)
    return(ERROR_NOMEMORY);

  for(i = 0;i < n_outlines;i++)
    {
      if(feof(fp))
        {
          CMAfreeOutlineField(&of);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "premature EOF (ready for next outline) in otl file %d", slice));
        }

      gdf_header_flag = FALSE;

      while(!gdf_header_flag)
        {
          if(feof(fp))
            {
              CMAfreeOutlineField(&of);
              errno = 0;
              ErrorReturn
                (ERROR_BADFILE,
                 (ERROR_BADFILE,
                  "premature EOF (searching for gdf header) in otl file %d",
                  slice));
            }
          fgets(line, STRLEN, fp);
          if(strncmp(line, "START GDF HEADER", 16) == 0)
            gdf_header_flag = TRUE;
        }

      n_rows = -1;
      seed_x = seed_y = -1;
      label[0] = '\0';
      type[0] = '\0';

      empty_label_flag = 0;

      while(gdf_header_flag)
        {

          if(feof(fp))
            {
              CMAfreeOutlineField(&of);
              errno = 0;
              ErrorReturn
                (ERROR_BADFILE,
                 (ERROR_BADFILE,
                  "premature EOF (in gdf header) in otl file %d", slice));
            }
          fgets(line, STRLEN, fp);
          if(strncmp(line, "END GDF HEADER", 14) == 0)
            gdf_header_flag = FALSE;
          // getting rows, cols, type
          if(strncmp(line, "ROW_NUM", 7) == 0)
            sscanf(line, "%*s %d", &n_rows);
          if(strncmp(line, "COL_NUM", 7) == 0)
            sscanf(line, "%*s %d", &n_cols);
          if(strncmp(line, "TYPE", 4) == 0)
            {
              strcpy(type, &(line[5]));
              c = strrchr(type, '\n');
              if(c != NULL)
                *c = '\0';
            }
          if(strncmp(line, "SEED", 4) == 0)
            sscanf(line, "%*s %d %d", &seed_x, &seed_y);
          if(strncmp(line, "LABEL", 5) == 0)
            {

              strcpy(label, &(line[6]));
              c = strrchr(label, '\n');
              if(c != NULL)
                *c = '\0';

              /* exterior -> cortex, if desired */
              if(translate_label_flag)
                {
                  translate_start = strstr(label, "Exterior");
                  if(translate_start != NULL)
                    sprintf(translate_start, "Cortex");
                  else
                    {
                      translate_start = strstr(label, "exterior");
                      if(translate_start != NULL)
                        sprintf(translate_start, "cortex");
                    }
                }

              /* warning if there's an "exterior" or
                 "cortex" after any other label */
              if(strstr(label, "Exterior") == 0 \
                 || strstr(label, "exterior") == 0 \
                 || strstr(label, "Cortex") == 0 \
                 || strstr(label, "cortex") == 0)
                {
                  if(internal_structures_flag)
                    printf("WARNING: label \"%s\" following "
                           "non-exterior labels in slice %d\n",
                           label, slice);
                  internal_structures_flag = FALSE;
                }
              else
                internal_structures_flag = TRUE;

            }

        }

      if(n_rows < 0)
        {
          CMAfreeOutlineField(&of);
          errno = 0;
          ErrorReturn(ERROR_BADPARM,
                      (ERROR_BADPARM,
                       "bad or undefined ROW_NUM in otl file %d", slice));
        }

      if(n_cols != 2)
        {
          CMAfreeOutlineField(&of);
          errno = 0;
          ErrorReturn(ERROR_BADPARM,
                      (ERROR_BADPARM,
                       "bad or undefined COL_NUM in otl file %d", slice));
        }

      if(label[0] == '\0')
        {
          empty_label_flag = 1;
          errno = 0;
          ErrorPrintf(ERROR_BADPARM,
                      "empty LABEL in otl file %d (outline %d)", slice, i);
        }

      if(seed_x < 0 || seed_x >= 512 || seed_y < 0 || seed_y >= 512)
        {
          CMAfreeOutlineField(&of);
          errno = 0;
          ErrorReturn(ERROR_BADPARM,
                      (ERROR_BADPARM,
                       "bad or undefined SEED in otl file %d", slice));
        }

      if(type[0] == '\0')
        strcpy(type, global_type);

      if(strcmp(type, "ascii short") == 0)
        ascii_short_flag = TRUE;
      else if(strcmp(type, "short") == 0)
        ascii_short_flag = FALSE;
      else if(type[0] == '\0')
        {
          CMAfreeOutlineField(&of);
          errno = 0;
          ErrorReturn(ERROR_UNSUPPORTED,
                      (ERROR_UNSUPPORTED,
                       "undefined TYPE in otl file %d", slice));
        }
      else
        {
          CMAfreeOutlineField(&of);
          errno = 0;
          ErrorReturn(ERROR_UNSUPPORTED,
                      (ERROR_UNSUPPORTED,
                       "unsupported TYPE \"%s\" in otl file %d", type, slice));
        }

      do
        {
          fgets(line, STRLEN, fp);
          if(feof(fp))
            {
              CMAfreeOutlineField(&of);
              errno = 0;
              ErrorReturn
                (ERROR_BADFILE,
                 (ERROR_BADFILE,
                  "premature EOF (searching for points) in otl file %d",
                  slice));
            }
        } while(strncmp(line, "START POINTS", 12) != 0);

      points = (short *)malloc(2 * n_rows * sizeof(short));
      if(points == NULL)
        {
          CMAfreeOutlineField(&of);
          errno = 0;
          ErrorReturn
            (ERROR_NOMEMORY,
             (ERROR_NOMEMORY,
              "error allocating memory for points in otl file %d", slice));
        }

      if(ascii_short_flag)
        {
          for(row = 0;row < n_rows;row++)
            {
              fgets(line, STRLEN, fp);
              if(feof(fp))
                {
                  free(points);
                  CMAfreeOutlineField(&of);
                  errno = 0;
                  ErrorReturn
                    (ERROR_BADFILE,
                     (ERROR_BADFILE,
                      "premature end of file while reading "
                      "points from otl file %d",
                      slice));
                }
              sscanf(line, "%hd %hd", &(points[2*row]), &(points[2*row+1]));
            }
        }
      else
        {
          n_read = fread(points, 2, n_rows * 2, fp);
          if(n_read != n_rows * 2)
            {
              free(points);
              CMAfreeOutlineField(&of);
              errno = 0;
              ErrorReturn(ERROR_BADFILE,
                          (ERROR_BADFILE,
                           "error reading points from otl file %d", slice));
            }
#if (BYTE_ORDER == LITTLE_ENDIAN)
          swab(points, points, 2 * n_rows * sizeof(short));
#endif
        }

      fgets(line, STRLEN, fp);
      if(strncmp(line, "END POINTS", 10) != 0)
        {
          free(points);
          CMAfreeOutlineField(&of);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "error after points (\"END POINTS\" expected "
              "but not found) in otl file %d", slice));
        }

      if(!empty_label_flag)
        {
          strcpy(label_to_compare, label);
          for(j = 0;label_to_compare[j] != '\0';j++)
            {
              if(label_to_compare[j] == '\n')
                label_to_compare[j] = '\0';
              if(label_to_compare[j] == ' ')
                label_to_compare[j] = '-';
            }

          /* --- strip 'Left' and 'Right'; --- */
          strcpy(alt_compare, label_to_compare);
          StrLower(alt_compare);
          if(strncmp(alt_compare, "left-", 5) == 0)
            strcpy(alt_compare, &(label_to_compare[5]));
          else if(strncmp(alt_compare, "right-", 6) == 0)
            strcpy(alt_compare, &(label_to_compare[6]));

          /* --- strip leading and trailing spaces (now dashes) --- */

          /* leading */
          for(j = 0;label_to_compare[j] == '-';j++);
          if(label_to_compare[j] != '\0')
            {
              for(c = label_to_compare;*(c+j) != '\0';c++)
                *c = *(c+j);
              *c = *(c+j);
            }

          /* trailing */
          for(j = strlen(label_to_compare) - 1;
              j >= 0 && label_to_compare[j] == '-';
              j--);
          if(j < 0) /* all dashes! */
            {
              /* for now, let this fall through to an unknown label */
            }
          else /* j is the index of the last non-dash character */
            {
              label_to_compare[j+1] = '\0';
            }


          label_value = -1;
          for(j = 0;j < color_table->mnNumEntries;j++)
            if(strcmp(color_table->maEntries[j].msLabel,
                      label_to_compare) == 0 ||
               strcmp(color_table->maEntries[j].msLabel, alt_compare) == 0)
              label_value = j;

          if(label_value == -1)
            {
              register_unknown_label(label);
            }
          else
            {

              for(j = 0;j < n_rows;j++)
                cma_field[points[2*j]][points[2*j+1]] = label_value;

              CMAclaimPoints(of, label_value, points, n_rows, seed_x, seed_y);

            }

        }

      free(points);

    }

  CMAassignLabels(of);

  if(zero_outlines_flag)
    CMAzeroOutlines(of);

  scale_x = 512 / mri->width;
  scale_y = 512 / mri->height;

  for(i = 0;i < mri->width;i++)
    for(j = 0;j < mri->height;j++)
      {
        source_x = (int)floor(scale_x * (float)i);
        source_y = (int)floor(scale_y * (float)j);
        if(source_x < 0)
          source_x = 0;
        if(source_x > 511)
          source_x = 511;
        if(source_y < 0)
          source_y = 0;
        if(source_y > 511)
          source_y = 511;
        MRISvox(mri, i, j, slice-1) = of->fill_field[source_y][source_x];
      }

  CMAfreeOutlineField(&of);

  return(NO_ERROR);

} /* end read_otl_file() */

MRI *MRIreadOtl
(char *fname, int width, int height, int slices,
 char *color_file_name, int flags)
{

  char stem[STRLEN];
  int i;
  MRI *mri;
  char *c;
  int one_file_exists;
  char first_name[STRLEN], last_name[STRLEN];
  FILE *fp;
  mriColorLookupTableRef color_table;
  int read_volume_flag, fill_flag, translate_labels_flag, zero_outlines_flag;

  /* ----- set local flags ----- */

  read_volume_flag = FALSE;
  fill_flag = FALSE;
  translate_labels_flag = FALSE;
  zero_outlines_flag = FALSE;

  if(flags & READ_OTL_READ_VOLUME_FLAG)
    read_volume_flag = TRUE;

  if(flags & READ_OTL_FILL_FLAG)
    fill_flag = TRUE;

  if(flags & READ_OTL_TRANSLATE_LABELS_FLAG)
    translate_labels_flag = TRUE;

  if(flags & READ_OTL_ZERO_OUTLINES_FLAG)
    zero_outlines_flag = TRUE;

  /* ----- reset our unknown label list ----- */

  clear_unknown_labels();

  /* ----- defaults to width and height ----- */
  if(width <= 0)
    width = 512;
  if(height <= 0)
    height = 512;

  /* ----- strip the stem of the otl file name ----- */
  strcpy(stem, fname);
  c = strrchr(stem, '.');
  if(c == NULL)
    {
      errno = 0;
      ErrorReturn
        (NULL, (ERROR_BADPARM, "MRIreadOtl(): bad file name: %s", fname));
    }

  for(c--;c >= stem && isdigit(*c);c--);

  if(c < stem)
    {
      errno = 0;
      ErrorReturn
        (NULL, (ERROR_BADPARM, "MRIreadOtl(): bad file name: %s", fname));
    }

  c++;

  one_file_exists = FALSE;
  for(i = 1;i <= slices;i++)
    {
      sprintf(c, "%d.otl", i);
      if(FileExists(stem))
        one_file_exists = TRUE;
      if(i == 1)
        strcpy(first_name, stem);
    }

  strcpy(last_name, stem);

  if(!one_file_exists)
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADPARM,
          "MRIreadOtl(): couldn't find any file between %s and %s",
          first_name, last_name));
    }

  if(!read_volume_flag)
    {
      mri = MRIallocHeader(width, height, slices, MRI_SHORT);
      if(mri == NULL)
        {
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_NOMEMORY,
              "MRIreadOtl(): error allocating MRI structure"));
        }
      return(mri);
    }
  mri = MRIalloc(width, height, slices, MRI_SHORT);
  if(mri == NULL)
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_NOMEMORY,
          "MRIreadOtl(): error allocating MRI structure"));
    }

  if(CLUT_NewFromFile(&color_table, color_file_name) != CLUT_tErr_NoErr)
    {
      MRIfree(&mri);
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE,
          "MRIreadOtl(): error reading color file %s", color_file_name));
    }

  one_file_exists = FALSE;
  for(i = 1;i <= slices;i++)
    {
      sprintf(c, "%d.otl", i);
      if((fp = fopen(stem, "r")) != NULL)
        {
          if(read_otl_file
             (fp, mri, i, color_table, fill_flag, \
              translate_labels_flag, zero_outlines_flag) != NO_ERROR)
            {
              MRIfree(&mri);
              return(NULL);
            }
          one_file_exists = TRUE;
        }
    }

  CLUT_Delete(&color_table);

  if(!one_file_exists)
    {
      MRIfree(&mri);
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE,
          "MRIreadOtl(): found at least one file "
          "between %s and %s but couldn't open it!", first_name, last_name));
    }

  if(n_unknown_labels == 0)
    {
      printf("no unknown labels\n");
    }
  else
    {
      printf("unknown labels:\n");
      print_unknown_labels("  ");
    }
  clear_unknown_labels();

  // default direction cosine set here to be CORONAL
  // no orientation info is given and thus set to coronal
  setDirectionCosine(mri, MRI_CORONAL);

  return(mri);

} /* end MRIreadOtl() */

#define XIMG_PIXEL_DATA_OFFSET    8432
#define XIMG_IMAGE_HEADER_OFFSET  2308

static MRI *ximgRead(char *fname, int read_volume)
{

  char fname_format[STRLEN];
  char fname_dir[STRLEN];
  char fname_base[STRLEN];
  char *c;
  MRI *mri = NULL;
  int im_init;
  int im_low, im_high;
  char fname_use[STRLEN];
  char temp_string[STRLEN];
  FILE *fp;
  int width, height;
  int pixel_data_offset;
  int image_header_offset;
  float tl_r, tl_a, tl_s;
  float tr_r, tr_a, tr_s;
  float br_r, br_a, br_s;
  float c_r, c_a, c_s;
  float n_r, n_a, n_s;
  float xlength, ylength, zlength;
  int i, y;
  MRI *header;
  float xfov, yfov, zfov;
  float nlength;

  printf("XIMG: using XIMG header corrections\n");

  /* ----- check the first (passed) file ----- */
  if(!FileExists(fname))
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE, "genesisRead(): error opening file %s", fname));
    }

  /* ----- split the file name into name and directory ----- */
  c = strrchr(fname, '/');
  if(c == NULL)
    {
      fname_dir[0] = '\0';
      strcpy(fname_base, fname);
    }
  else
    {
      strncpy(fname_dir, fname, (c - fname + 1));
      fname_dir[c-fname+1] = '\0';
      strcpy(fname_base, c+1);
    }

  /* ----- derive the file name format (for sprintf) ----- */
  if(strncmp(fname_base, "I.", 2) == 0)
    {
      im_init = atoi(&fname_base[2]);
      sprintf(fname_format, "I.%%03d");
    }
  else if(strlen(fname_base) >= 3) /* avoid core dumps below... */
    {
      c = &fname_base[strlen(fname_base)-3];
      if(strcmp(c, ".MR") == 0)
        {
          *c = '\0';
          for(c--;isdigit(*c) && c >= fname_base;c--);
          c++;
          im_init = atoi(c);
          *c = '\0';
          sprintf(fname_format, "%s%%d.MR", fname_base);
        }
      else
        {
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADPARM,
              "genesisRead(): can't determine file name format for %s",
              fname));
        }
    }
  else
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADPARM,
          "genesisRead(): can't determine file name format for %s", fname));
    }

  strcpy(temp_string, fname_format);
  sprintf(fname_format, "%s%s", fname_dir, temp_string);

  /* ----- find the low and high files ----- */
  im_low = im_init;
  do
    {
      im_low--;
      sprintf(fname_use, fname_format, im_low);
    } while(FileExists(fname_use));
  im_low++;

  im_high = im_init;
  do
    {
      im_high++;
      sprintf(fname_use, fname_format, im_high);
    } while(FileExists(fname_use));
  im_high--;

  /* ----- allocate the mri structure ----- */
  header = MRIallocHeader(1, 1, 1, MRI_SHORT);

  header->depth = im_high - im_low + 1;
  header->imnr0 = 1;
  header->imnr1 = header->depth;

  /* ----- get the header information from the first file ----- */
  sprintf(fname_use, fname_format, im_low);
  if((fp = fopen(fname_use, "r")) == NULL)
    {
      MRIfree(&header);
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE, "genesisRead(): error opening file %s\n", fname_use));
    }

  fseek(fp, 4, SEEK_SET);
  fread(&pixel_data_offset, 4, 1, fp);
  pixel_data_offset = orderIntBytes(pixel_data_offset);
  printf("XIMG: pixel data offset is %d, ", pixel_data_offset);
  if(pixel_data_offset != XIMG_PIXEL_DATA_OFFSET)
    pixel_data_offset = XIMG_PIXEL_DATA_OFFSET;
  printf("using offset %d\n", pixel_data_offset);

  fseek(fp, 8, SEEK_SET);
  fread(&width, 4, 1, fp);  width = orderIntBytes(width);
  fread(&height, 4, 1, fp);  height = orderIntBytes(height);

  fseek(fp, 148, SEEK_SET);
  fread(&image_header_offset, 4, 1, fp);
  image_header_offset = orderIntBytes(image_header_offset);
  printf("XIMG: image header offset is %d, ", image_header_offset);
  if(image_header_offset != XIMG_IMAGE_HEADER_OFFSET)
    image_header_offset = XIMG_IMAGE_HEADER_OFFSET;
  printf("using offset %d\n", image_header_offset);

  header->width = width;
  header->height = height;

  strcpy(header->fname, fname);

  fseek(fp, image_header_offset + 26 + 2, SEEK_SET);
  fread(&(header->thick), 4, 1, fp);
  header->thick = orderFloatBytes(header->thick);
  header->zsize = header->thick;

  fseek(fp, image_header_offset + 50 + 2, SEEK_SET);
  fread(&(header->xsize), 4, 1, fp);
  header->xsize = orderFloatBytes(header->xsize);
  fread(&(header->ysize), 4, 1, fp);
  header->ysize = orderFloatBytes(header->ysize);
  header->ps = header->xsize;

  /* all in micro-seconds */
#define MICROSECONDS_PER_MILLISECOND 1e3
  fseek(fp, image_header_offset + 194 + 6, SEEK_SET);
  header->tr = freadInt(fp)/MICROSECONDS_PER_MILLISECOND ;
  fseek(fp, image_header_offset + 198, SEEK_SET);
  header->ti = freadInt(fp)/MICROSECONDS_PER_MILLISECOND  ;
  fseek(fp, image_header_offset + 202, SEEK_SET);
  header->te = freadInt(fp)/MICROSECONDS_PER_MILLISECOND  ;
  fseek(fp, image_header_offset + 254, SEEK_SET);
  header->flip_angle = RADIANS(freadShort(fp)) ;  /* was in degrees */

  fseek(fp, image_header_offset + 130 + 6, SEEK_SET);
  fread(&c_r,  4, 1, fp);  c_r  = orderFloatBytes(c_r);
  fread(&c_a,  4, 1, fp);  c_a  = orderFloatBytes(c_a);
  fread(&c_s,  4, 1, fp);  c_s  = orderFloatBytes(c_s);
  fread(&n_r,  4, 1, fp);  n_r  = orderFloatBytes(n_r);
  fread(&n_a,  4, 1, fp);  n_a  = orderFloatBytes(n_a);
  fread(&n_s,  4, 1, fp);  n_s  = orderFloatBytes(n_s);
  fread(&tl_r, 4, 1, fp);  tl_r = orderFloatBytes(tl_r);
  fread(&tl_a, 4, 1, fp);  tl_a = orderFloatBytes(tl_a);
  fread(&tl_s, 4, 1, fp);  tl_s = orderFloatBytes(tl_s);
  fread(&tr_r, 4, 1, fp);  tr_r = orderFloatBytes(tr_r);
  fread(&tr_a, 4, 1, fp);  tr_a = orderFloatBytes(tr_a);
  fread(&tr_s, 4, 1, fp);  tr_s = orderFloatBytes(tr_s);
  fread(&br_r, 4, 1, fp);  br_r = orderFloatBytes(br_r);
  fread(&br_a, 4, 1, fp);  br_a = orderFloatBytes(br_a);
  fread(&br_s, 4, 1, fp);  br_s = orderFloatBytes(br_s);

  nlength = sqrt(n_r*n_r + n_a*n_a + n_s*n_s);
  n_r = n_r / nlength;
  n_a = n_a / nlength;
  n_s = n_s / nlength;

  if (getenv("KILLIANY_SWAP") != NULL)
    {
      printf("WARNING - swapping normal direction!\n") ;
      n_a *= -1 ;
    }

  header->x_r = (tr_r - tl_r);
  header->x_a = (tr_a - tl_a);
  header->x_s = (tr_s - tl_s);
  header->y_r = (br_r - tr_r);
  header->y_a = (br_a - tr_a);
  header->y_s = (br_s - tr_s);

  /* --- normalize -- the normal vector from the file
     should have length 1, but just in case... --- */
  xlength = sqrt(header->x_r*header->x_r +
                 header->x_a*header->x_a +
                 header->x_s*header->x_s);
  ylength = sqrt(header->y_r*header->y_r +
                 header->y_a*header->y_a +
                 header->y_s*header->y_s);
  zlength = sqrt(n_r*n_r + n_a*n_a + n_s*n_s);

  header->x_r = header->x_r / xlength;
  header->x_a = header->x_a / xlength;
  header->x_s = header->x_s / xlength;
  header->y_r = header->y_r / ylength;
  header->y_a = header->y_a / ylength;
  header->y_s = header->y_s / ylength;
  header->z_r = n_r / zlength;
  header->z_a = n_a / zlength;
  header->z_s = n_s / zlength;

  header->c_r = (tl_r + br_r) / 2.0 + n_r * header->zsize *
    (header->depth - 1.0) / 2.0;
  header->c_a = (tl_a + br_a) / 2.0 + n_a * header->zsize *
    (header->depth - 1.0) / 2.0;
  header->c_s = (tl_s + br_s) / 2.0 + n_s * header->zsize *
    (header->depth - 1.0) / 2.0;

  header->ras_good_flag = 1;

  header->xend = header->xsize * (double)header->width / 2.0;
  header->xstart = -header->xend;
  header->yend = header->ysize * (double)header->height / 2.0;
  header->ystart = -header->yend;
  header->zend = header->zsize * (double)header->depth / 2.0;
  header->zstart = -header->zend;

  xfov = header->xend - header->xstart;
  yfov = header->yend - header->ystart;
  zfov = header->zend - header->zstart;

  header->fov =
    (xfov > yfov ? (xfov > zfov ? xfov : zfov) : (yfov > zfov ? yfov : zfov));

  fclose(fp);

  if(read_volume)
    mri = MRIalloc
      (header->width, header->height, header->depth, header->type);
  else
    mri = MRIallocHeader
      (header->width, header->height, header->depth, header->type);

  MRIcopyHeader(header, mri);
  MRIfree(&header);

  /* ----- read the volume if required ----- */
  if(read_volume)
    {

      for(i = im_low;i <= im_high;i++)
        {

          sprintf(fname_use, fname_format, i);
          if((fp = fopen(fname_use, "r")) == NULL)
            {
              MRIfree(&mri);
              errno = 0;
              ErrorReturn
                (NULL,
                 (ERROR_BADFILE,
                  "genesisRead(): error opening file %s", fname_use));
            }

          /*
            fseek(fp, 4, SEEK_SET);
            fread(&pixel_data_offset, 4, 1, fp);
            pixel_data_offset = orderIntBytes(pixel_data_offset);
          */
          pixel_data_offset = XIMG_PIXEL_DATA_OFFSET;
          fseek(fp, pixel_data_offset, SEEK_SET);

          for(y = 0;y < mri->height;y++)
            {
              if(fread(mri->slices[i-im_low][y], 2, mri->width, fp) !=
                 mri->width)
                {
                  fclose(fp);
                  MRIfree(&mri);
                  errno = 0;
                  ErrorReturn
                    (NULL,
                     (ERROR_BADFILE,
                      "genesisRead(): error reading from file file %s",
                      fname_use));
                }
#if (BYTE_ORDER == LITTLE_ENDIAN)
              swab(mri->slices[i-im_low][y],
                   mri->slices[i-im_low][y], 2 * mri->width);
#endif
            }

          fclose(fp);

        }

    }

  return(mri);

} /* end ximgRead() */

/*-----------------------------------------------------------
  MRISreadCurvAsMRI() - reads freesurfer surface curv format
  as an MRI.
  -----------------------------------------------------------*/
static MRI *MRISreadCurvAsMRI(char *curvfile, int read_volume)
{
  int    magno,k,vnum,fnum, vals_per_vertex ;
  float  curv;
  FILE   *fp;
  MRI *curvmri;

  if(!IDisCurv(curvfile)) return(NULL);

  fp = fopen(curvfile,"r");
  fread3(&magno,fp);

  vnum = freadInt(fp);
  fnum = freadInt(fp);
  vals_per_vertex = freadInt(fp) ;
  if (vals_per_vertex != 1){
    fclose(fp) ;
    printf("ERROR: MRISreadCurvAsMRI: %s, vals/vertex %d unsupported\n",
           curvfile,vals_per_vertex);
    return(NULL);
  }

  if(!read_volume){
    curvmri = MRIallocHeader(vnum, 1,1,MRI_FLOAT);
    curvmri->nframes = 1;
    return(curvmri);
  }

  curvmri = MRIalloc(vnum,1,1,MRI_FLOAT);
  for (k=0;k<vnum;k++){
    curv = freadFloat(fp) ;
    MRIsetVoxVal(curvmri,k,0,0,0,curv);
  }
  fclose(fp);

  return(curvmri) ;
}

/*------------------------------------------------------------------
  nifti1Read() - note: there is also an niiRead(). Make sure to
  edit both. NIFTI1 is defined to be the two-file NIFTI standard, ie,
  there must be a .img and .hdr file. (see is_nifti1(char *fname))
  -----------------------------------------------------------------*/
static MRI *nifti1Read(char *fname, int read_volume)
{

  char hdr_fname[STRLEN];
  char img_fname[STRLEN];
  char fname_stem[STRLEN];
  char *dot;
  FILE *fp;
  MRI *mri;
  struct nifti_1_header hdr;
  int nslices;
  int fs_type;
  float time_units_factor, space_units_factor;
  int swapped_flag;
  int n_read, i, j, k, t;
  int bytes_per_voxel, time_units, space_units;

  strcpy(fname_stem, fname);
  dot = strrchr(fname_stem, '.');
  if(dot != NULL)
    if(strcmp(dot, ".img") == 0 || strcmp(dot, ".hdr") == 0)
      *dot = '\0';

  sprintf(hdr_fname, "%s.hdr", fname_stem);
  sprintf(img_fname, "%s.img", fname_stem);

  fp = fopen(hdr_fname, "r");
  if(fp == NULL)
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE, "nifti1Read(): error opening file %s", hdr_fname));
    }

  if(fread(&hdr, sizeof(hdr), 1, fp) != 1)
    {
      fclose(fp);
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE,
          "nifti1Read(): error reading header from %s", hdr_fname));
    }

  fclose(fp);

  swapped_flag = FALSE;
  if(hdr.dim[0] < 1 || hdr.dim[0] > 7){
    swapped_flag = TRUE;
    swap_nifti_1_header(&hdr);
    if(hdr.dim[0] < 1 || hdr.dim[0] > 7){
      ErrorReturn
        (NULL,
         (ERROR_BADFILE,
          "nifti1Read(): bad number of dimensions (%hd) in %s",
          hdr.dim[0], hdr_fname));
    }
  }

  if(memcmp(hdr.magic, NIFTI1_MAGIC, 4) != 0)
    ErrorReturn
      (NULL,
       (ERROR_BADFILE,
        "nifti1Read(): bad magic number in %s", hdr_fname));

  if(hdr.dim[0] != 3 && hdr.dim[0] != 4)
    ErrorReturn
      (NULL,
       (ERROR_UNSUPPORTED,
        "nifti1Read(): %hd dimensions in %s; unsupported",
        hdr.dim[0], hdr_fname));

  if(hdr.datatype == DT_NONE || hdr.datatype == DT_UNKNOWN)
    ErrorReturn
      (NULL,
       (ERROR_UNSUPPORTED,
        "nifti1Read(): unknown or no data type in %s; bailing out",
        hdr_fname));

  space_units  = XYZT_TO_SPACE(hdr.xyzt_units) ;
  if(space_units ==NIFTI_UNITS_METER)       space_units_factor = 1000.0;
  else if(space_units ==NIFTI_UNITS_MM)     space_units_factor = 1.0;
  else if(space_units ==NIFTI_UNITS_MICRON) space_units_factor = 0.001;
  else
    {
      ErrorReturn
        (NULL,
         (ERROR_BADFILE,
          "nifti1Read(): unknown space units %d in %s",
          space_units,hdr_fname));
    }

  time_units = XYZT_TO_TIME (hdr.xyzt_units) ;
  if(time_units == NIFTI_UNITS_SEC)       time_units_factor = 1000.0;
  else if(time_units == NIFTI_UNITS_MSEC) time_units_factor = 1.0;
  else if(time_units == NIFTI_UNITS_USEC) time_units_factor = 0.001;
  else {
    if(hdr.dim[4] > 1){
      ErrorReturn
        (NULL,
         (ERROR_BADFILE,
          "nifti1Read(): unknown time units %d in %s",
          time_units,hdr_fname));
    }
    else time_units_factor = 0;
  }

  /*
    nifti1.h says: slice_code = If this is nonzero, AND
    if slice_dim is nonzero, AND
    if slice_duration is positive, indicates the timing
    pattern of the slice acquisition.
    not yet supported here
  */

  if(hdr.slice_code != 0 &&
     DIM_INFO_TO_SLICE_DIM(hdr.dim_info) != 0 &&
     hdr.slice_duration > 0.0)
    ErrorReturn
      (NULL,
       (ERROR_UNSUPPORTED,
        "nifti1Read(): unsupported timing pattern in %s", hdr_fname));

  if(hdr.dim[0] == 3) nslices = 1;
  else                nslices = hdr.dim[4];

  if(hdr.scl_slope == 0)
    // voxel values are unscaled -- we use the file's data type
    {

      if(hdr.datatype == DT_UNSIGNED_CHAR)
        {
          fs_type = MRI_UCHAR;
          bytes_per_voxel = 1;
        }
      else if(hdr.datatype == DT_SIGNED_SHORT)
        {
          fs_type = MRI_SHORT;
          bytes_per_voxel = 2;
        }
      else if(hdr.datatype == DT_UINT16){
	// This will not always work ...
	printf("INFO: this is an unsiged short. I'll try to read it, but\n");
	printf("      it might not work if there are values over 32k\n");
	fs_type = MRI_SHORT;
	bytes_per_voxel = 2;
      }
      else if(hdr.datatype == DT_SIGNED_INT)
        {
          fs_type = MRI_INT;
          bytes_per_voxel = 4;
        }
      else if(hdr.datatype == DT_FLOAT)
        {
          fs_type = MRI_FLOAT;
          bytes_per_voxel = 4;
        }
      else
        {
          ErrorReturn
            (NULL,
             (ERROR_UNSUPPORTED,
              "nifti1Read(): unsupported datatype %d "
              "(with scl_slope = 0) in %s",
              hdr.datatype, hdr_fname));
        }
    }
  else // we must scale the voxel values
    {
      if(   hdr.datatype != DT_UNSIGNED_CHAR
            && hdr.datatype != DT_SIGNED_SHORT
            && hdr.datatype != DT_SIGNED_INT
            && hdr.datatype != DT_FLOAT
            && hdr.datatype != DT_DOUBLE
            && hdr.datatype != DT_INT8
            && hdr.datatype != DT_UINT16
            && hdr.datatype != DT_UINT32)
        {
          ErrorReturn
            (NULL,
             (ERROR_UNSUPPORTED,
              "nifti1Read(): unsupported datatype %d "
              "(with scl_slope != 0) in %s",
              hdr.datatype, hdr_fname));
        }
      fs_type = MRI_FLOAT;
      bytes_per_voxel = 0; /* set below -- this line is to
                              avoid the compiler warning */
    }

  if(read_volume)
    mri = MRIallocSequence
      (hdr.dim[1], hdr.dim[2], hdr.dim[3], fs_type, nslices);
  else
    {
      mri = MRIallocHeader(hdr.dim[1], hdr.dim[2], hdr.dim[3], fs_type);
      mri->nframes = nslices;
    }

  if(mri == NULL)
    return(NULL);

  mri->xsize = hdr.pixdim[1];
  mri->ysize = hdr.pixdim[2];
  mri->zsize = hdr.pixdim[3];
  // Keep in msec as NIFTI_UNITS_MSEC is specified
  if(hdr.dim[0] == 4) mri->tr = hdr.pixdim[4];

  // Set the vox2ras matrix
  if(hdr.sform_code != 0){
    // First, use the sform, if that is ok. Using the sform
    // first makes it more compatible with FSL.
    printf("INFO: using NIfTI-1 sform \n");
    if(niftiSformToMri(mri, &hdr) != NO_ERROR){
      MRIfree(&mri);
      return(NULL);
    }
    mri->ras_good_flag = 1;
  } else if(hdr.qform_code != 0){
    // Then, try the qform, if that is ok
    printf("INFO: using NIfTI-1 qform \n");
    if(niftiQformToMri(mri, &hdr) != NO_ERROR){
      MRIfree(&mri);
      return(NULL);
    }
    mri->ras_good_flag = 1;
  } else {
    // Should probably just die here.
    printf("WARNING: neither NIfTI-1 qform or sform are valid\n");
    printf("WARNING: your volume will probably be incorrectly oriented\n");
    mri->x_r = -1.0;  mri->x_a = 0.0;  mri->x_s = 0.0;
    mri->y_r = 0.0;  mri->y_a = 1.0;  mri->y_s = 0.0;
    mri->z_r = 0.0;  mri->z_a = 0.0;  mri->z_s = 1.0;
    mri->c_r = mri->xsize * mri->width / 2.0;
    mri->c_a = mri->ysize * mri->height / 2.0;
    mri->c_s = mri->zsize * mri->depth / 2.0;
    mri->ras_good_flag = 0;
  }

  mri->xsize = mri->xsize * space_units_factor;
  mri->ysize = mri->ysize * space_units_factor;
  mri->zsize = mri->zsize * space_units_factor;
  mri->c_r = mri->c_r * space_units_factor;
  mri->c_a = mri->c_a * space_units_factor;
  mri->c_s = mri->c_s * space_units_factor;
  if(hdr.dim[0] == 4)
    mri->tr = mri->tr * time_units_factor;

  if(!read_volume)
    return(mri);

  fp = fopen(img_fname, "r");
  if(fp == NULL){
    MRIfree(&mri);
    errno = 0;
    ErrorReturn
      (NULL,
       (ERROR_BADFILE, "nifti1Read(): error opening file %s", img_fname));
  }

  if(hdr.scl_slope == 0) // no voxel value scaling needed
    {

      void *buf;

      for(t = 0;t < mri->nframes;t++)
        for(k = 0;k < mri->depth;k++)
          for(j = 0;j < mri->height;j++)
            {

              buf = &MRIseq_vox(mri, 0, j, k, t);

              n_read = fread(buf, bytes_per_voxel, mri->width, fp);
              if(n_read != mri->width)
                {
                  fclose(fp);
                  MRIfree(&mri);
                  errno = 0;
                  ErrorReturn
                    (NULL,
                     (ERROR_BADFILE,
                      "nifti1Read(): error reading from %s", img_fname));
                }

              if(swapped_flag)
                {
                  if(bytes_per_voxel == 2)
                    byteswapbufshort(buf, bytes_per_voxel * mri->width);
                  if(bytes_per_voxel == 4)
                    byteswapbuffloat(buf, bytes_per_voxel * mri->width);
                }

            }

    }
  else // voxel value scaling needed
    {

      if(hdr.datatype == DT_UNSIGNED_CHAR)
        {
          unsigned char *buf;
          bytes_per_voxel = 1;
          buf = (unsigned char *)malloc(mri->width * bytes_per_voxel);
          for(t = 0;t < mri->nframes;t++)
            for(k = 0;k < mri->depth;k++)
              for(j = 0;j < mri->height;j++)
                {
                  n_read = fread(buf, bytes_per_voxel, mri->width, fp);
                  if(n_read != mri->width)
                    {
                      free(buf);
                      fclose(fp);
                      MRIfree(&mri);
                      errno = 0;
                      ErrorReturn
                        (NULL,
                         (ERROR_BADFILE,
                          "nifti1Read(): error reading from %s", img_fname));
                    }
                  for(i = 0;i < mri->width;i++)
                    MRIFseq_vox(mri, i, j, k, t) =
                      hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
                }
          free(buf);
        }

      if(hdr.datatype == DT_SIGNED_SHORT)
        {
          short *buf;
          bytes_per_voxel = 2;
          buf = (short *)malloc(mri->width * bytes_per_voxel);
          for(t = 0;t < mri->nframes;t++)
            for(k = 0;k < mri->depth;k++)
              for(j = 0;j < mri->height;j++)
                {
                  n_read = fread(buf, bytes_per_voxel, mri->width, fp);
                  if(n_read != mri->width)
                    {
                      free(buf);
                      fclose(fp);
                      MRIfree(&mri);
                      errno = 0;
                      ErrorReturn
                        (NULL,
                         (ERROR_BADFILE,
                          "nifti1Read(): error reading from %s", img_fname));
                    }
                  if(swapped_flag)
                    byteswapbufshort(buf, bytes_per_voxel * mri->width);
                  for(i = 0;i < mri->width;i++)
                    MRIFseq_vox(mri, i, j, k, t) =
                      hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
                }
          free(buf);
        }

      if(hdr.datatype == DT_SIGNED_INT)
        {
          int *buf;
          bytes_per_voxel = 4;
          buf = (int *)malloc(mri->width * bytes_per_voxel);
          for(t = 0;t < mri->nframes;t++)
            for(k = 0;k < mri->depth;k++)
              for(j = 0;j < mri->height;j++)
                {
                  n_read = fread(buf, bytes_per_voxel, mri->width, fp);
                  if(n_read != mri->width)
                    {
                      free(buf);
                      fclose(fp);
                      MRIfree(&mri);
                      errno = 0;
                      ErrorReturn
                        (NULL,
                         (ERROR_BADFILE,
                          "nifti1Read(): error reading from %s", img_fname));
                    }
                  if(swapped_flag)
                    byteswapbuffloat(buf, bytes_per_voxel * mri->width);
                  for(i = 0;i < mri->width;i++)
                    MRIFseq_vox(mri, i, j, k, t) =
                      hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
                }
          free(buf);
        }

      if(hdr.datatype == DT_FLOAT)
        {
          float *buf;
          bytes_per_voxel = 4;
          buf = (float *)malloc(mri->width * bytes_per_voxel);
          for(t = 0;t < mri->nframes;t++)
            for(k = 0;k < mri->depth;k++)
              for(j = 0;j < mri->height;j++)
                {
                  n_read = fread(buf, bytes_per_voxel, mri->width, fp);
                  if(n_read != mri->width)
                    {
                      free(buf);
                      fclose(fp);
                      MRIfree(&mri);
                      errno = 0;
                      ErrorReturn
                        (NULL,
                         (ERROR_BADFILE,
                          "nifti1Read(): error reading from %s", img_fname));
                    }
                  if(swapped_flag)
                    byteswapbuffloat(buf, bytes_per_voxel * mri->width);
                  for(i = 0;i < mri->width;i++)
                    MRIFseq_vox(mri, i, j, k, t) =
                      hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
                }
          free(buf);
        }

      if(hdr.datatype == DT_DOUBLE)
        {
          double *buf;
          unsigned char *cbuf, ccbuf[8];
          bytes_per_voxel = 8;
          buf = (double *)malloc(mri->width * bytes_per_voxel);
          for(t = 0;t < mri->nframes;t++)
            for(k = 0;k < mri->depth;k++)
              for(j = 0;j < mri->height;j++)
                {
                  n_read = fread(buf, bytes_per_voxel, mri->width, fp);
                  if(n_read != mri->width)
                    {
                      free(buf);
                      fclose(fp);
                      MRIfree(&mri);
                      errno = 0;
                      ErrorReturn
                        (NULL,
                         (ERROR_BADFILE,
                          "nifti1Read(): error reading from %s", img_fname));
                    }
                  if(swapped_flag)
                    {
                      for(i = 0;i < mri->width;i++)
                        {
                          cbuf = (unsigned char *)&buf[i];
                          memcpy(ccbuf, cbuf, 8);
                          cbuf[0] = ccbuf[7];
                          cbuf[1] = ccbuf[6];
                          cbuf[2] = ccbuf[5];
                          cbuf[3] = ccbuf[4];
                          cbuf[4] = ccbuf[3];
                          cbuf[5] = ccbuf[2];
                          cbuf[6] = ccbuf[1];
                          cbuf[7] = ccbuf[0];
                        }
                    }
                  for(i = 0;i < mri->width;i++)
                    MRIFseq_vox(mri, i, j, k, t) =
                      hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
                }
          free(buf);
        }

      if(hdr.datatype == DT_INT8)
        {
          char *buf;
          bytes_per_voxel = 1;
          buf = (char *)malloc(mri->width * bytes_per_voxel);
          for(t = 0;t < mri->nframes;t++)
            for(k = 0;k < mri->depth;k++)
              for(j = 0;j < mri->height;j++)
                {
                  n_read = fread(buf, bytes_per_voxel, mri->width, fp);
                  if(n_read != mri->width)
                    {
                      free(buf);
                      fclose(fp);
                      MRIfree(&mri);
                      errno = 0;
                      ErrorReturn
                        (NULL,
                         (ERROR_BADFILE,
                          "nifti1Read(): error reading from %s",
                          img_fname));
                    }
                  for(i = 0;i < mri->width;i++)
                    MRIFseq_vox(mri, i, j, k, t) =
                      hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
                }
          free(buf);
        }

      if(hdr.datatype == DT_UINT16)
        {
          unsigned short *buf;
          bytes_per_voxel = 2;
          buf = (unsigned short *)malloc(mri->width * bytes_per_voxel);
          for(t = 0;t < mri->nframes;t++)
            for(k = 0;k < mri->depth;k++)
              for(j = 0;j < mri->height;j++)
                {
                  n_read = fread(buf, bytes_per_voxel, mri->width, fp);
                  if(n_read != mri->width)
                    {
                      free(buf);
                      fclose(fp);
                      MRIfree(&mri);
                      errno = 0;
                      ErrorReturn
                        (NULL,
                         (ERROR_BADFILE,
                          "nifti1Read(): error reading from %s",
                          img_fname));
                    }
                  if(swapped_flag)
                    byteswapbufshort(buf, bytes_per_voxel * mri->width);
                  for(i = 0;i < mri->width;i++)
                    MRIFseq_vox(mri, i, j, k, t) =
                      hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
                }
          free(buf);
        }

      if(hdr.datatype == DT_UINT32)
        {
          unsigned int *buf;
          bytes_per_voxel = 4;
          buf = (unsigned int *)malloc(mri->width * bytes_per_voxel);
          for(t = 0;t < mri->nframes;t++)
            for(k = 0;k < mri->depth;k++)
              for(j = 0;j < mri->height;j++)
                {
                  n_read = fread(buf, bytes_per_voxel, mri->width, fp);
                  if(n_read != mri->width)
                    {
                      free(buf);
                      fclose(fp);
                      MRIfree(&mri);
                      errno = 0;
                      ErrorReturn
                        (NULL,
                         (ERROR_BADFILE,
                          "nifti1Read(): error reading from %s",
                          img_fname));
                    }
                  if(swapped_flag)
                    byteswapbuffloat(buf, bytes_per_voxel * mri->width);
                  for(i = 0;i < mri->width;i++)
                    MRIFseq_vox(mri, i, j, k, t) =
                      hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
                }
          free(buf);
        }

    }

  fclose(fp);

  return(mri);

} /* end nifti1Read() */


/*------------------------------------------------------------------
  nifti1Write() - note: there is also an niiWrite(). Make sure to
  edit both.
  -----------------------------------------------------------------*/
static int nifti1Write(MRI *mri, char *fname)
{

  FILE *fp;
  int j, k, t;
  BUFTYPE *buf;
  struct nifti_1_header hdr;
  char fname_stem[STRLEN];
  char hdr_fname[STRLEN];
  char img_fname[STRLEN];
  char *dot;
  int error;
  int shortmax;

  shortmax = (int)(pow(2.0,15.0));
  if(mri->width > shortmax){
    printf("NIFTI FORMAT ERROR: ncols %d in volume exceeds %d\n",
           mri->width,shortmax);
    exit(1);
  }
  if(mri->height > shortmax){
    printf("NIFTI FORMAT ERROR: nrows %d in volume exceeds %d\n",
           mri->height,shortmax);
    exit(1);
  }
  if(mri->depth > shortmax){
    printf("NIFTI FORMAT ERROR: nslices %d in volume exceeds %d\n",
           mri->depth,shortmax);
    exit(1);
  }
  if(mri->nframes > shortmax){
    printf("NIFTI FORMAT ERROR:  nframes %d in volume exceeds %d\n",
           mri->nframes,shortmax);
    exit(1);
  }

  memset(&hdr, 0x00, sizeof(hdr));

  hdr.sizeof_hdr = 348;
  hdr.dim_info = 0;

  for(t=0; t<8; t++){hdr.dim[t] = 1;hdr.pixdim[t] = 1;} // needed for afni
  if(mri->nframes == 1){
    hdr.dim[0] = 3;
    hdr.dim[1] = mri->width;
    hdr.dim[2] = mri->height;
    hdr.dim[3] = mri->depth;
    hdr.pixdim[1] = mri->xsize;
    hdr.pixdim[2] = mri->ysize;
    hdr.pixdim[3] = mri->zsize;
  }
  else{
    hdr.dim[0] = 4;
    hdr.dim[1] = mri->width;
    hdr.dim[2] = mri->height;
    hdr.dim[3] = mri->depth;
    hdr.dim[4] = mri->nframes;
    hdr.pixdim[1] = mri->xsize;
    hdr.pixdim[2] = mri->ysize;
    hdr.pixdim[3] = mri->zsize;
    hdr.pixdim[4] = mri->tr/1000.0; // sec, see also xyzt_units
  }

  if(mri->type == MRI_UCHAR){
    hdr.datatype = DT_UNSIGNED_CHAR;
    hdr.bitpix = 8;
  }
  else if(mri->type == MRI_INT)
    {
      hdr.datatype = DT_SIGNED_INT;
      hdr.bitpix = 32;
    }
  else if(mri->type == MRI_LONG)
    {
      hdr.datatype = DT_SIGNED_INT;
      hdr.bitpix = 32;
    }
  else if(mri->type == MRI_FLOAT)
    {
      hdr.datatype = DT_FLOAT;
      hdr.bitpix = 32;
    }
  else if(mri->type == MRI_SHORT)
    {
      hdr.datatype = DT_SIGNED_SHORT;
      hdr.bitpix = 16;
    }
  else if(mri->type == MRI_BITMAP)
    {
      ErrorReturn(ERROR_UNSUPPORTED,
                  (ERROR_UNSUPPORTED,
                   "nifti1Write(): data type MRI_BITMAP unsupported"));
    }
  else if(mri->type == MRI_TENSOR)
    {
      ErrorReturn(ERROR_UNSUPPORTED,
                  (ERROR_UNSUPPORTED,
                   "nifti1Write(): data type MRI_TENSOR unsupported"));
    }
  else
    {
      ErrorReturn(ERROR_BADPARM,
                  (ERROR_BADPARM,
                   "nifti1Write(): unknown data type %d", mri->type));
    }

  hdr.intent_code = NIFTI_INTENT_NONE;
  hdr.intent_name[0] = '\0';
  hdr.vox_offset = 0;
  hdr.scl_slope = 0.0;
  hdr.slice_code = 0;
  hdr.xyzt_units = NIFTI_UNITS_MM | NIFTI_UNITS_SEC; // This may be wrong
  hdr.cal_max = 0.0;
  hdr.cal_min = 0.0;
  hdr.toffset = 0;
  sprintf(hdr.descrip,"FreeSurfer %s",__DATE__);

  /* set the nifti header qform values */
  error = mriToNiftiQform(mri, &hdr);
  if(error != NO_ERROR) return(error);

  /* set the nifti header sform values */
  // This just copies the vox2ras into the sform
  mriToNiftiSform(mri, &hdr);

  memcpy(hdr.magic, NIFTI1_MAGIC, 4);

  strcpy(fname_stem, fname);
  dot = strrchr(fname_stem, '.');
  if(dot != NULL)
    if(strcmp(dot, ".img") == 0 || strcmp(dot, ".hdr") == 0)
      *dot = '\0';

  sprintf(hdr_fname, "%s.hdr", fname_stem);
  sprintf(img_fname, "%s.img", fname_stem);

  fp = fopen(hdr_fname, "w");
  if(fp == NULL)
    {
      errno = 0;
      ErrorReturn(ERROR_BADFILE,
                  (ERROR_BADFILE,
                   "nifti1Write(): error opening file %s", hdr_fname));
    }

  if(fwrite(&hdr, sizeof(hdr), 1, fp) != 1)
    {
      fclose(fp);
      errno = 0;
      ErrorReturn(ERROR_BADFILE,
                  (ERROR_BADFILE,
                   "nifti1Write(): error writing header to %s", hdr_fname));
    }

  fclose(fp);

  fp = fopen(img_fname, "w");
  if(fp == NULL)
    {
      errno = 0;
      ErrorReturn(ERROR_BADFILE,
                  (ERROR_BADFILE,
                   "nifti1Write(): error opening file %s", img_fname));
    }

  for(t = 0;t < mri->nframes;t++)
    for(k = 0;k < mri->depth;k++)
      for(j = 0;j < mri->height;j++)
        {
          buf = &MRIseq_vox(mri, 0, j, k, t);
          if(fwrite(buf, hdr.bitpix/8, mri->width, fp) != mri->width)
            {
              fclose(fp);
              errno = 0;
              ErrorReturn
                (ERROR_BADFILE,
                 (ERROR_BADFILE,
                  "nifti1Write(): error writing data to %s", img_fname));
            }
        }

  fclose(fp);

  return(NO_ERROR);

} /* end nifti1Write() */

/*------------------------------------------------------------------
  niiRead() - note: there is also an nifti1Read(). Make sure to
  edit both.
  -----------------------------------------------------------------*/
static MRI *niiRead(char *fname, int read_volume)
{

  znzFile fp;
  MRI *mri;
  struct nifti_1_header hdr;
  int nslices;
  int fs_type;
  float time_units_factor, space_units_factor;
  int swapped_flag;
  int n_read, i, j, k, t;
  int bytes_per_voxel,time_units,space_units ;
  int use_compression, fnamelen;

  use_compression = 0;
  fnamelen = strlen(fname);
  if(fname[fnamelen-1] == 'z') use_compression = 1;
  if(Gdiag_no > 0) printf("niiWrite: use_compression = %d\n",use_compression);

  fp = znzopen(fname, "r", use_compression);
  if(fp == NULL){
    errno = 0;
    ErrorReturn(NULL, (ERROR_BADFILE,
                       "niiRead(): error opening file %s", fname));
  }

  if(znzread(&hdr, sizeof(hdr), 1, fp) != 1){
    znzclose(fp);
    errno = 0;
    ErrorReturn(NULL, (ERROR_BADFILE,
                       "niiRead(): error reading header from %s", fname));
  }

  znzclose(fp);

  swapped_flag = FALSE;
  if(hdr.dim[0] < 1 || hdr.dim[0] > 7){
    swapped_flag = TRUE;
    swap_nifti_1_header(&hdr);
    if(hdr.dim[0] < 1 || hdr.dim[0] > 7){
      ErrorReturn(NULL, (ERROR_BADFILE,
                         "niiRead(): bad number of dimensions (%hd) in %s",
                         hdr.dim[0], fname));
    }
  }

  if(memcmp(hdr.magic, NII_MAGIC, 4) != 0){
    ErrorReturn(NULL, (ERROR_BADFILE, "niiRead(): bad magic number in %s",
                       fname));
  }

  if(hdr.dim[0] != 3 && hdr.dim[0] != 4){
    ErrorReturn(NULL,
                (ERROR_UNSUPPORTED,
                 "niiRead(): %hd dimensions in %s; unsupported",
                 hdr.dim[0], fname));
  }

  if(hdr.datatype == DT_NONE || hdr.datatype == DT_UNKNOWN){
    ErrorReturn(NULL,
                (ERROR_UNSUPPORTED,
                 "niiRead(): unknown or no data type in %s; bailing out",
                 fname));
  }

  space_units  = XYZT_TO_SPACE(hdr.xyzt_units) ;
  if(space_units == NIFTI_UNITS_METER)       space_units_factor = 1000.0;
  else if(space_units == NIFTI_UNITS_MM)     space_units_factor = 1.0;
  else if(space_units == NIFTI_UNITS_MICRON) space_units_factor = 0.001;
  else
    ErrorReturn
      (NULL,
       (ERROR_BADFILE,
        "niiRead(): unknown space units %d in %s", space_units,
        fname));

  time_units = XYZT_TO_TIME (hdr.xyzt_units) ;
  if(time_units == NIFTI_UNITS_SEC)       time_units_factor = 1000.0;
  else if(time_units == NIFTI_UNITS_MSEC) time_units_factor = 1.0;
  else if(time_units == NIFTI_UNITS_USEC) time_units_factor = 0.001;
  else{
    if(hdr.dim[4] > 1){
      ErrorReturn
        (NULL,
         (ERROR_BADFILE,
          "niiRead(): unknown time units %d in %s",
          time_units,fname));
    }
    else time_units_factor = 0;
  }
  //printf("hdr.xyzt_units = %d, time_units = %d, %g, %g\n",
  // hdr.xyzt_units,time_units,hdr.pixdim[4],time_units_factor);

  /*
    nifti1.h says: slice_code = If this is nonzero, AND
    if slice_dim is nonzero, AND
    if slice_duration is positive, indicates the timing
    pattern of the slice acquisition.
    not yet supported here
  */

  if(hdr.slice_code != 0 && DIM_INFO_TO_SLICE_DIM(hdr.dim_info) != 0
     && hdr.slice_duration > 0.0){
    ErrorReturn(NULL,
                (ERROR_UNSUPPORTED,
                 "niiRead(): unsupported timing pattern in %s", fname));
  }

  if(hdr.dim[0] == 3) nslices = 1;
  else                nslices = hdr.dim[4];

  if(hdr.scl_slope == 0){
    // voxel values are unscaled -- we use the file's data type
    if(hdr.datatype == DT_UNSIGNED_CHAR){
      fs_type = MRI_UCHAR;
      bytes_per_voxel = 1;
    }
    else if(hdr.datatype == DT_SIGNED_SHORT){
      fs_type = MRI_SHORT;
      bytes_per_voxel = 2;
    }
    else if(hdr.datatype == DT_UINT16){
      // This will not always work ...
      printf("INFO: this is an unsiged short. I'll try to read it, but\n");
      printf("      it might not work if there are values over 32k\n");
      fs_type = MRI_SHORT;
      bytes_per_voxel = 2;
    }
    else if(hdr.datatype == DT_SIGNED_INT){
      fs_type = MRI_INT;
      bytes_per_voxel = 4;
    }
    else if(hdr.datatype == DT_FLOAT){
      fs_type = MRI_FLOAT;
      bytes_per_voxel = 4;
    }
    else{
      ErrorReturn
        (NULL,
         (ERROR_UNSUPPORTED,
          "niiRead(): unsupported datatype %d (with scl_slope = 0) in %s",
          hdr.datatype, fname));
    }
  }
  else{
    // we must scale the voxel values
    if(   hdr.datatype != DT_UNSIGNED_CHAR
          && hdr.datatype != DT_SIGNED_SHORT
          && hdr.datatype != DT_SIGNED_INT
          && hdr.datatype != DT_FLOAT
          && hdr.datatype != DT_DOUBLE
          && hdr.datatype != DT_INT8
          && hdr.datatype != DT_UINT16
          && hdr.datatype != DT_UINT32){
      ErrorReturn
        (NULL,
         (ERROR_UNSUPPORTED,
          "niiRead(): unsupported datatype %d (with scl_slope != 0) in %s",
          hdr.datatype, fname));
    }
    fs_type = MRI_FLOAT;
    bytes_per_voxel = 0; /* set below -- avoid the compiler warning */
  }

  if(read_volume)
    mri = MRIallocSequence(hdr.dim[1],hdr.dim[2],hdr.dim[3],fs_type,nslices);
  else{
    mri = MRIallocHeader(hdr.dim[1], hdr.dim[2], hdr.dim[3], fs_type);
    mri->nframes = nslices;
  }

  if(mri == NULL) return(NULL);

  mri->xsize = hdr.pixdim[1];
  mri->ysize = hdr.pixdim[2];
  mri->zsize = hdr.pixdim[3];
  if(hdr.dim[0] == 4) mri->tr = hdr.pixdim[4];

  // Set the vox2ras matrix
  // Set the vox2ras matrix
  if(hdr.sform_code != 0){
    // First, use the sform, if that is ok. Using the sform
    // first makes it more compatible with FSL.
    printf("INFO: using NIfTI-1 sform \n");
    if(niftiSformToMri(mri, &hdr) != NO_ERROR){
      MRIfree(&mri);
      return(NULL);
    }
    mri->ras_good_flag = 1;
  } else if(hdr.qform_code != 0){
    // Then, try the qform, if that is ok
    printf("INFO: using NIfTI-1 qform \n");
    if(niftiQformToMri(mri, &hdr) != NO_ERROR){
      MRIfree(&mri);
      return(NULL);
    }
    mri->ras_good_flag = 1;
  } else {
    // Should probably just die here.
    printf("WARNING: neither NIfTI-1 qform or sform are valid\n");
    printf("WARNING: your volume will probably be incorrectly oriented\n");
    mri->x_r = -1.0;  mri->x_a = 0.0;  mri->x_s = 0.0;
    mri->y_r = 0.0;  mri->y_a = 1.0;  mri->y_s = 0.0;
    mri->z_r = 0.0;  mri->z_a = 0.0;  mri->z_s = 1.0;
    mri->c_r = mri->xsize * mri->width  / 2.0;
    mri->c_a = mri->ysize * mri->height / 2.0;
    mri->c_s = mri->zsize * mri->depth  / 2.0;
    mri->ras_good_flag = 0;
  }

  mri->xsize = mri->xsize * space_units_factor;
  mri->ysize = mri->ysize * space_units_factor;
  mri->zsize = mri->zsize * space_units_factor;
  mri->c_r = mri->c_r * space_units_factor;
  mri->c_a = mri->c_a * space_units_factor;
  mri->c_s = mri->c_s * space_units_factor;
  if(hdr.dim[0] == 4)  mri->tr = mri->tr * time_units_factor;

  if(Gdiag_no > 0){
    printf("nifti header ---------------------------------\n");
    niiPrintHdr(stdout, &hdr);
    printf("-----------------------------------------\n");
  }

  if(!read_volume) return(mri);

  fp = znzopen(fname, "r", use_compression);
  if(fp == NULL) {
    MRIfree(&mri);
    errno = 0;
    ErrorReturn(NULL, (ERROR_BADFILE,
                       "niiRead(): error opening file %s", fname));
  }

  if(znzseek(fp, (long)(hdr.vox_offset), SEEK_SET) == -1){
    znzclose(fp);
    MRIfree(&mri);
    errno = 0;
    ErrorReturn(NULL, (ERROR_BADFILE,
                       "niiRead(): error finding voxel data in %s", fname));
  }

  if(hdr.scl_slope == 0){
    // no voxel value scaling needed
    void *buf;

    for(t = 0;t < mri->nframes;t++)
      for(k = 0;k < mri->depth;k++)
        for(j = 0;j < mri->height;j++) {
          buf = &MRIseq_vox(mri, 0, j, k, t);

          n_read = znzread(buf, bytes_per_voxel, mri->width, fp);
          if(n_read != mri->width) {
            znzclose(fp);
            MRIfree(&mri);
            errno = 0;
            ErrorReturn(NULL, (ERROR_BADFILE,
                               "niiRead(): error reading from %s", fname));
          }

          if(swapped_flag) {
            if(bytes_per_voxel == 2)
              byteswapbufshort(buf, bytes_per_voxel * mri->width);
            if(bytes_per_voxel == 4)
              byteswapbuffloat(buf, bytes_per_voxel * mri->width);
          }

        }

  }
  else{
    // voxel value scaling needed
    if(hdr.datatype == DT_UNSIGNED_CHAR) {
      unsigned char *buf;
      bytes_per_voxel = 1;
      buf = (unsigned char *)malloc(mri->width * bytes_per_voxel);
      for(t = 0;t < mri->nframes;t++)
        for(k = 0;k < mri->depth;k++)
          for(j = 0;j < mri->height;j++) {
            n_read = znzread(buf, bytes_per_voxel, mri->width, fp);
            if(n_read != mri->width) {
              free(buf);
              znzclose(fp);
              MRIfree(&mri);
              errno = 0;
              ErrorReturn(NULL,
                          (ERROR_BADFILE,
                           "niiRead(): error reading from %s", fname));
            }
            for(i = 0;i < mri->width;i++)
              MRIFseq_vox(mri, i, j, k, t) =
                hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
          }
      free(buf);
    }

    if(hdr.datatype == DT_SIGNED_SHORT){
      short *buf;
      bytes_per_voxel = 2;
      buf = (short *)malloc(mri->width * bytes_per_voxel);
      for(t = 0;t < mri->nframes;t++)
        for(k = 0;k < mri->depth;k++)
          for(j = 0;j < mri->height;j++) {
            n_read = znzread(buf, bytes_per_voxel, mri->width, fp);
            if(n_read != mri->width) {
              free(buf);
              znzclose(fp);
              MRIfree(&mri);
              errno = 0;
              ErrorReturn(NULL,
                          (ERROR_BADFILE,
                           "niiRead(): error reading from %s", fname));
            }
            if(swapped_flag)
              byteswapbufshort(buf, bytes_per_voxel * mri->width);
            for(i = 0;i < mri->width;i++)
              MRIFseq_vox(mri, i, j, k, t) =
                hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
          }
      free(buf);
    }

    if(hdr.datatype == DT_SIGNED_INT) {
      int *buf;
      bytes_per_voxel = 4;
      buf = (int *)malloc(mri->width * bytes_per_voxel);
      for(t = 0;t < mri->nframes;t++)
        for(k = 0;k < mri->depth;k++)
          for(j = 0;j < mri->height;j++) {
            n_read = znzread(buf, bytes_per_voxel, mri->width, fp);
            if(n_read != mri->width){
              free(buf);
              znzclose(fp);
              MRIfree(&mri);
              errno = 0;
              ErrorReturn(NULL,
                          (ERROR_BADFILE,
                           "niiRead(): error reading from %s", fname));
            }
            if(swapped_flag)
              byteswapbuffloat(buf, bytes_per_voxel * mri->width);
            for(i = 0;i < mri->width;i++)
              MRIFseq_vox(mri, i, j, k, t) =
                hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
          }
      free(buf);
    }

    if(hdr.datatype == DT_FLOAT) {
      float *buf;
      bytes_per_voxel = 4;
      buf = (float *)malloc(mri->width * bytes_per_voxel);
      for(t = 0;t < mri->nframes;t++)
        for(k = 0;k < mri->depth;k++)
          for(j = 0;j < mri->height;j++) {
            n_read = znzread(buf, bytes_per_voxel, mri->width, fp);
            if(n_read != mri->width) {
              free(buf);
              znzclose(fp);
              MRIfree(&mri);
              errno = 0;
              ErrorReturn(NULL,
                          (ERROR_BADFILE,
                           "niiRead(): error reading from %s", fname));
            }
            if(swapped_flag)
              byteswapbuffloat(buf, bytes_per_voxel * mri->width);
            for(i = 0;i < mri->width;i++)
              MRIFseq_vox(mri, i, j, k, t) =
                hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
          }
      free(buf);
    }

    if(hdr.datatype == DT_DOUBLE) {
      double *buf;
      unsigned char *cbuf, ccbuf[8];
      bytes_per_voxel = 8;
      buf = (double *)malloc(mri->width * bytes_per_voxel);
      for(t = 0;t < mri->nframes;t++)
        for(k = 0;k < mri->depth;k++)
          for(j = 0;j < mri->height;j++) {
            n_read = znzread(buf, bytes_per_voxel, mri->width, fp);
            if(n_read != mri->width) {
              free(buf);
              znzclose(fp);
              MRIfree(&mri);
              errno = 0;
              ErrorReturn(NULL,
                          (ERROR_BADFILE,
                           "niiRead(): error reading from %s", fname));
            }
            if(swapped_flag) {
              for(i = 0;i < mri->width;i++)
                {
                  cbuf = (unsigned char *)&buf[i];
                  memcpy(ccbuf, cbuf, 8);
                  cbuf[0] = ccbuf[7];
                  cbuf[1] = ccbuf[6];
                  cbuf[2] = ccbuf[5];
                  cbuf[3] = ccbuf[4];
                  cbuf[4] = ccbuf[3];
                  cbuf[5] = ccbuf[2];
                  cbuf[6] = ccbuf[1];
                  cbuf[7] = ccbuf[0];
                }
            }
            for(i = 0;i < mri->width;i++)
              MRIFseq_vox(mri, i, j, k, t) =
                hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
          }
      free(buf);
    }

    if(hdr.datatype == DT_INT8) {
      char *buf;
      bytes_per_voxel = 1;
      buf = (char *)malloc(mri->width * bytes_per_voxel);
      for(t = 0;t < mri->nframes;t++)
        for(k = 0;k < mri->depth;k++)
          for(j = 0;j < mri->height;j++){
            n_read = znzread(buf, bytes_per_voxel, mri->width, fp);
            if(n_read != mri->width) {
              free(buf);
              znzclose(fp);
              MRIfree(&mri);
              errno = 0;
              ErrorReturn(NULL,
                          (ERROR_BADFILE,
                           "niiRead(): error reading from %s", fname));
            }
            for(i = 0;i < mri->width;i++)
              MRIFseq_vox(mri, i, j, k, t) =
                hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
          }
      free(buf);
    }

    if(hdr.datatype == DT_UINT16) {
      unsigned short *buf;
      bytes_per_voxel = 2;
      buf = (unsigned short *)malloc(mri->width * bytes_per_voxel);
      for(t = 0;t < mri->nframes;t++)
        for(k = 0;k < mri->depth;k++)
          for(j = 0;j < mri->height;j++) {
            n_read = znzread(buf, bytes_per_voxel, mri->width, fp);
            if(n_read != mri->width){
              free(buf);
              znzclose(fp);
              MRIfree(&mri);
              errno = 0;
              ErrorReturn(NULL,
                          (ERROR_BADFILE,
                           "niiRead(): error reading from %s", fname));
            }
            if(swapped_flag)
              byteswapbufshort(buf, bytes_per_voxel * mri->width);
            for(i = 0;i < mri->width;i++)
              MRIFseq_vox(mri, i, j, k, t) =
                hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
          }
      free(buf);
    }

    if(hdr.datatype == DT_UINT32) {
      unsigned int *buf;
      bytes_per_voxel = 4;
      buf = (unsigned int *)malloc(mri->width * bytes_per_voxel);
      for(t = 0;t < mri->nframes;t++)
        for(k = 0;k < mri->depth;k++)
          for(j = 0;j < mri->height;j++)
            {
              n_read = znzread(buf, bytes_per_voxel, mri->width, fp);
              if(n_read != mri->width)
                {
                  free(buf);
                  znzclose(fp);
                  MRIfree(&mri);
                  errno = 0;
                  ErrorReturn(NULL,
                              (ERROR_BADFILE,
                               "niiRead(): error reading from %s", fname));
                }
              if(swapped_flag)
                byteswapbuffloat(buf, bytes_per_voxel * mri->width);
              for(i = 0;i < mri->width;i++)
                MRIFseq_vox(mri, i, j, k, t) =
                  hdr.scl_slope * (float)(buf[i]) + hdr.scl_inter;
            }
      free(buf);
    }
  }
  znzclose(fp);

  return(mri);

} /* end niiRead() */

/*------------------------------------------------------------------
  niiWrite() - note: there is also an nifti1Write(). Make sure to
  edit both.
  -----------------------------------------------------------------*/
static int niiWrite(MRI *mri, char *fname)
{

  znzFile fp;
  int j, k, t;
  BUFTYPE *buf;
  struct nifti_1_header hdr;
  char *chbuf;
  int error, shortmax, use_compression, fnamelen, nfill;

  use_compression = 0;
  fnamelen = strlen(fname);
  if(fname[fnamelen-1] == 'z') use_compression = 1;
  if(Gdiag_no > 0) printf("niiWrite: use_compression = %d\n",use_compression);

  shortmax = (int)(pow(2.0,15.0));
  if(mri->width > shortmax){
    printf("NIFTI FORMAT ERROR: ncols %d in volume exceeds %d\n",
           mri->width,shortmax);
    exit(1);
  }
  if(mri->height > shortmax){
    printf("NIFTI FORMAT ERROR: nrows %d in volume exceeds %d\n",
           mri->height,shortmax);
    exit(1);
  }
  if(mri->depth > shortmax){
    printf("NIFTI FORMAT ERROR: nslices %d in volume exceeds %d\n",
           mri->depth,shortmax);
    exit(1);
  }
  if(mri->nframes > shortmax){
    printf("NIFTI FORMAT ERROR:  nframes %d in volume exceeds %d\n",
           mri->nframes,shortmax);
    exit(1);
  }

  memset(&hdr, 0x00, sizeof(hdr));

  hdr.sizeof_hdr = 348;
  hdr.dim_info = 0;

  for(t=0; t<8; t++){hdr.dim[t] = 1;hdr.pixdim[t] = 1;} // for afni
  if(mri->nframes == 1){
    hdr.dim[0] = 3;
    hdr.dim[1] = mri->width;
    hdr.dim[2] = mri->height;
    hdr.dim[3] = mri->depth;
    hdr.pixdim[1] = mri->xsize;
    hdr.pixdim[2] = mri->ysize;
    hdr.pixdim[3] = mri->zsize;
  }
  else{
    hdr.dim[0] = 4;
    hdr.dim[1] = mri->width;
    hdr.dim[2] = mri->height;
    hdr.dim[3] = mri->depth;
    hdr.dim[4] = mri->nframes;
    hdr.pixdim[1] = mri->xsize;
    hdr.pixdim[2] = mri->ysize;
    hdr.pixdim[3] = mri->zsize;
    hdr.pixdim[4] = mri->tr/1000.0; // see also xyzt_units
  }

  if(mri->type == MRI_UCHAR){
    hdr.datatype = DT_UNSIGNED_CHAR;
    hdr.bitpix = 8;
  }
  else if(mri->type == MRI_INT){
    hdr.datatype = DT_SIGNED_INT;
    hdr.bitpix = 32;
  }
  else if(mri->type == MRI_LONG){
    hdr.datatype = DT_SIGNED_INT;
    hdr.bitpix = 32;
  }
  else if(mri->type == MRI_FLOAT){
    hdr.datatype = DT_FLOAT;
    hdr.bitpix = 32;
  }
  else if(mri->type == MRI_SHORT){
    hdr.datatype = DT_SIGNED_SHORT;
    hdr.bitpix = 16;
  }
  else if(mri->type == MRI_BITMAP){
    ErrorReturn(ERROR_UNSUPPORTED,
                (ERROR_UNSUPPORTED,
                 "niiWrite(): data type MRI_BITMAP unsupported"));
  }
  else if(mri->type == MRI_TENSOR){
    ErrorReturn(ERROR_UNSUPPORTED,
                (ERROR_UNSUPPORTED,
                 "niiWrite(): data type MRI_TENSOR unsupported"));
  }
  else{
    ErrorReturn(ERROR_BADPARM,
                (ERROR_BADPARM,
                 "niiWrite(): unknown data type %d", mri->type));
  }

  hdr.intent_code = NIFTI_INTENT_NONE;
  hdr.intent_name[0] = '\0';
  hdr.vox_offset = 352; // 352 is the min, dont use sizeof(hdr); See below
  hdr.scl_slope = 0.0;
  hdr.slice_code = 0;
  hdr.xyzt_units = NIFTI_UNITS_MM | NIFTI_UNITS_SEC;
  hdr.cal_max = 0.0;
  hdr.cal_min = 0.0;
  hdr.toffset = 0;
  sprintf(hdr.descrip,"FreeSurfer %s",__DATE__);

  /* set the nifti header qform values */
  error = mriToNiftiQform(mri, &hdr);
  if(error != NO_ERROR) return(error);

  /* set the nifti header sform values */
  // This just copies the vox2ras into the sform
  mriToNiftiSform(mri, &hdr);

  memcpy(hdr.magic, NII_MAGIC, 4);

  // Open the file
  fp = znzopen(fname, "w", use_compression);
  if(fp == NULL){
    errno = 0;
    ErrorReturn(ERROR_BADFILE,
                (ERROR_BADFILE,
                 "niiWrite(): error opening file %s", fname));
  }

  // White the header
  if(znzwrite(&hdr, sizeof(hdr), 1, fp) != 1) {
    znzclose(fp);
    errno = 0;
    ErrorReturn(ERROR_BADFILE,
                (ERROR_BADFILE,
                 "niiWrite(): error writing header to %s", fname));
  }

  // Fill in space to the voxel offset
  nfill = hdr.vox_offset-sizeof(hdr);
  chbuf = (char *) calloc(nfill,sizeof(char));
  if(znzwrite(chbuf, sizeof(char), nfill, fp) != nfill){
    znzclose(fp);
    errno = 0;
    ErrorReturn(ERROR_BADFILE,
		(ERROR_BADFILE,
		 "niiWrite(): error writing data to %s", fname));
  }
  free(chbuf);



  for(t = 0;t < mri->nframes;t++)
    for(k = 0;k < mri->depth;k++)
      for(j = 0;j < mri->height;j++){
        buf = &MRIseq_vox(mri, 0, j, k, t);
        if(znzwrite(buf, hdr.bitpix/8, mri->width, fp) != mri->width){
          znzclose(fp);
          errno = 0;
          ErrorReturn(ERROR_BADFILE,
                      (ERROR_BADFILE,
                       "niiWrite(): error writing data to %s", fname));
        }
      }

  znzclose(fp);

  return(NO_ERROR);

} /* end niiWrite() */


static int niftiSformToMri(MRI *mri, struct nifti_1_header *hdr)
{

  /*

  x = srow_x[0] * i + srow_x[1] * j + srow_x[2] * k + srow_x[3]
  y = srow_y[0] * i + srow_y[1] * j + srow_y[2] * k + srow_y[3]
  z = srow_z[0] * i + srow_z[1] * j + srow_z[2] * k + srow_z[3]


  [ r ]   [ mri->xsize * mri->x_r
  mri->ysize * mri->y_r    mri->zsize * mri->z_r  r_offset ]   [ i ]
  [ a ] = [ mri->xsize * mri->x_a
  mri->ysize * mri->y_a    mri->zsize * mri->z_a  a_offset ] * [ j ]
  [ s ]   [ mri->xsize * mri->x_s
  mri->ysize * mri->y_s    mri->zsize * mri->z_s  s_offset ]   [ k ]
  [ 1 ]   [            0
  0                        0              1     ]   [ 1 ]



  */

  mri->xsize = sqrt(hdr->srow_x[0]*hdr->srow_x[0] \
                    + hdr->srow_y[0]*hdr->srow_y[0] \
                    + hdr->srow_z[0]*hdr->srow_z[0]);
  mri->x_r = hdr->srow_x[0] / mri->xsize;
  mri->x_a = hdr->srow_y[0] / mri->xsize;
  mri->x_s = hdr->srow_z[0] / mri->xsize;

  mri->ysize = sqrt(hdr->srow_x[1]*hdr->srow_x[1] \
                    + hdr->srow_y[1]*hdr->srow_y[1] \
                    + hdr->srow_z[1]*hdr->srow_z[1]);
  mri->y_r = hdr->srow_x[1] / mri->ysize;
  mri->y_a = hdr->srow_y[1] / mri->ysize;
  mri->y_s = hdr->srow_z[1] / mri->ysize;

  mri->zsize = sqrt(hdr->srow_x[2]*hdr->srow_x[2] \
                    + hdr->srow_y[2]*hdr->srow_y[2] \
                    + hdr->srow_z[2]*hdr->srow_z[2]);
  mri->z_r = hdr->srow_x[2] / mri->zsize;
  mri->z_a = hdr->srow_y[2] / mri->zsize;
  mri->z_s = hdr->srow_z[2] / mri->zsize;

  mri->c_r =
    hdr->srow_x[0] * (mri->width/2.0)  +
    hdr->srow_x[1] * (mri->height/2.0) +
    hdr->srow_x[2] * (mri->depth/2.0)  +
    hdr->srow_x[3];

  mri->c_a =
    hdr->srow_y[0] * (mri->width/2.0)  +
    hdr->srow_y[1] * (mri->height/2.0) +
    hdr->srow_y[2] * (mri->depth/2.0)  +
    hdr->srow_y[3];

  mri->c_s =
    hdr->srow_z[0] * (mri->width/2.0)  +
    hdr->srow_z[1] * (mri->height/2.0) +
    hdr->srow_z[2] * (mri->depth/2.0)  +
    hdr->srow_z[3];

  return(NO_ERROR);

} /* end niftiSformToMri() */

static int niftiQformToMri(MRI *mri, struct nifti_1_header *hdr)
{

  float a, b, c, d;
  float r11, r12, r13;
  float r21, r22, r23;
  float r31, r32, r33;

  b = hdr->quatern_b;
  c = hdr->quatern_c;
  d = hdr->quatern_d;

  /* following quatern_to_mat44() */

  a = 1.0 - (b*b + c*c + d*d);
  if(a < 1.0e-7)
    {
      a = 1.0 / sqrt(b*b + c*c + d*d);
      b *= a;
      c *= a;
      d *= a;
      a = 0.0;
    }
  else
    a = sqrt(a);

  r11 = a*a + b*b - c*c - d*d;
  r12 = 2.0*b*c - 2.0*a*d;
  r13 = 2.0*b*d + 2.0*a*c;
  r21 = 2.0*b*c + 2.0*a*d;
  r22 = a*a + c*c - b*b - d*d;
  r23 = 2.0*c*d - 2.0*a*b;
  r31 = 2.0*b*d - 2*a*c;
  r32 = 2.0*c*d + 2*a*b;
  r33 = a*a + d*d - c*c - b*b;

  if(hdr->pixdim[0] < 0.0)
    {
      r13 = -r13;
      r23 = -r23;
      r33 = -r33;
    }

  mri->x_r = r11;
  mri->y_r = r12;
  mri->z_r = r13;
  mri->x_a = r21;
  mri->y_a = r22;
  mri->z_a = r23;
  mri->x_s = r31;
  mri->y_s = r32;
  mri->z_s = r33;


  /**/
  /* c_ras */
  /*

  [ r ]   [ mri->xsize * mri->x_r
  mri->ysize * mri->y_r    mri->zsize * mri->z_r  r_offset ]   [ i ]
  [ a ] = [ mri->xsize * mri->x_a
  mri->ysize * mri->y_a    mri->zsize * mri->z_a  a_offset ] * [ j ]
  [ s ]   [ mri->xsize * mri->x_s
  mri->ysize * mri->y_s    mri->zsize * mri->z_s  s_offset ]   [ k ]
  [ 1 ]   [            0
  0                        0              1     ]   [ 1 ]


  */

  mri->c_r =   (mri->xsize * mri->x_r) * (mri->width / 2.0)
    + (mri->ysize * mri->y_r) * (mri->height / 2.0)
    + (mri->zsize * mri->z_r) * (mri->depth / 2.0)
    + hdr->qoffset_x;

  mri->c_a =   (mri->xsize * mri->x_a) * (mri->width / 2.0)
    + (mri->ysize * mri->y_a) * (mri->height / 2.0)
    + (mri->zsize * mri->z_a) * (mri->depth / 2.0)
    + hdr->qoffset_y;

  mri->c_s =   (mri->xsize * mri->x_s) * (mri->width / 2.0)
    + (mri->ysize * mri->y_s) * (mri->height / 2.0)
    + (mri->zsize * mri->z_s) * (mri->depth / 2.0)
    + hdr->qoffset_z;


  return(NO_ERROR);

} /* end niftiQformToMri() */

/*---------------------------------------------------------------------
  mriToNiftiSform() - just copies the vox2ras to sform and sets the
  xform code to scanner anat.
  ---------------------------------------------------------------------*/
static int mriToNiftiSform(MRI *mri, struct nifti_1_header *hdr)
{
  MATRIX *vox2ras;
  int c;
  vox2ras = MRIxfmCRS2XYZ(mri, 0);
  for(c = 0; c<4; c++){
    hdr->srow_x[c] = vox2ras->rptr[1][c+1];
    hdr->srow_y[c] = vox2ras->rptr[2][c+1];
    hdr->srow_z[c] = vox2ras->rptr[3][c+1];
  }
  hdr->sform_code = NIFTI_XFORM_SCANNER_ANAT;
  MatrixFree(&vox2ras);
  return(0);
}

/*---------------------------------------------------------------------*/
static int mriToNiftiQform(MRI *mri, struct nifti_1_header *hdr)
{
  MATRIX *i_to_r;
  float r11, r12, r13;
  float r21, r22, r23;
  float r31, r32, r33;
  float qfac;
  float a, b, c, d;
  float xd, yd, zd;
  float r_det;

  /*

  nifti1.h (x, y, z are r, a, s, respectively):

  [ x ]   [ R11 R12 R13 ] [        pixdim[1] * i ]   [ qoffset_x ]
  [ y ] = [ R21 R22 R23 ] [        pixdim[2] * j ] + [ qoffset_y ]
  [ z ]   [ R31 R32 R33 ] [ qfac * pixdim[3] * k ]   [ qoffset_z ]

  mri.c extract_r_to_i():

  [ r ]   [ mri->xsize * mri->x_r
  mri->ysize * mri->y_r    mri->zsize * mri->z_r  r_offset ]   [ i ]
  [ a ] = [ mri->xsize * mri->x_a
  mri->ysize * mri->y_a    mri->zsize * mri->z_a  a_offset ] * [ j ]
  [ s ]   [ mri->xsize * mri->x_s
  mri->ysize * mri->y_s    mri->zsize * mri->z_s  s_offset ]   [ k ]
  [ 1 ]   [            0
  0                        0              1     ]   [ 1 ]

  where [ras]_offset are calculated by satisfying (r, a, s)
  = (mri->c_r, mri->c_a, mri->c_s)
  when (i, j, k) = (mri->width/2, mri->height/2, mri->depth/2)

  so our mapping is simple:

  (done outside this function)
  pixdim[1] mri->xsize
  pixdim[2] mri->ysize
  pixdim[3] mri->zsize

  R11 = mri->x_r  R12 = mri->y_r  R13 = mri->z_r
  R21 = mri->x_a  R22 = mri->y_a  R23 = mri->z_a
  R31 = mri->x_s  R32 = mri->y_s  R33 = mri->z_s

  qoffset_x = r_offset
  qoffset_y = a_offset
  qoffset_z = s_offset

  qfac (pixdim[0]) = 1

  unless det(R) == -1, in which case we substitute:

  R13 = -mri->z_r
  R23 = -mri->z_a
  R33 = -mri->z_s

  qfac (pixdim[0]) = -1

  we follow mat44_to_quatern() from nifti1_io.c

  for now, assume orthonormality in mri->[xyz]_[ras]

  */


  r11 = mri->x_r;  r12 = mri->y_r;  r13 = mri->z_r;
  r21 = mri->x_a;  r22 = mri->y_a;  r23 = mri->z_a;
  r31 = mri->x_s;  r32 = mri->y_s;  r33 = mri->z_s;

  r_det = + r11 * (r22*r33 - r32*r23)
    - r12 * (r21*r33 - r31*r23)
    + r13 * (r21*r32 - r31*r22);

  if(r_det == 0.0)
    {
      ErrorReturn(ERROR_BADFILE,
                  (ERROR_BADFILE,
                   "bad orientation matrix (determinant = 0) in nifti1 file"));
    }
  else if(r_det > 0.0)
    qfac = 1.0;
  else
    {
      r13 = -r13;
      r23 = -r23;
      r33 = -r33;
      qfac = -1.0;
    }

  /* following mat44_to_quatern() */

  a = r11 + r22 + r33 + 1.0;

  if(a > 0.5)
    {
      a = 0.5 * sqrt(a);
      b = 0.25 * (r32-r23) / a;
      c = 0.25 * (r13-r31) / a;
      d = 0.25 * (r21-r12) / a;
    }
  else
    {

      xd = 1.0 + r11 - (r22+r33);
      yd = 1.0 + r22 - (r11+r33);
      zd = 1.0 + r33 - (r11+r22);

      if(xd > 1.0)
        {
          b = 0.5 * sqrt(xd);
          c = 0.25 * (r12+r21) / b;
          d = 0.25 * (r13+r31) / b;
          a = 0.25 * (r32-r23) / b;
        }
      else if( yd > 1.0 )
        {
          c = 0.5 * sqrt(yd);
          b = 0.25 * (r12+r21) / c;
          d = 0.25 * (r23+r32) / c;
          a = 0.25 * (r13-r31) / c;
        }
      else
        {
          d = 0.5 * sqrt(zd);
          b = 0.25 * (r13+r31) / d;
          c = 0.25 * (r23+r32) / d;
          a = 0.25 * (r21-r12) / d;
        }

      if(a < 0.0)
        {
          a = -a;
          b = -b;
          c = -c;
          d = -d;
        }

    }

  hdr->qform_code = NIFTI_XFORM_SCANNER_ANAT;

  hdr->pixdim[0] = qfac;

  hdr->quatern_b = b;
  hdr->quatern_c = c;
  hdr->quatern_d = d;

  i_to_r = extract_i_to_r(mri);
  if(i_to_r == NULL)
    return(ERROR_BADPARM);

  hdr->qoffset_x = *MATRIX_RELT(i_to_r, 1, 4);
  hdr->qoffset_y = *MATRIX_RELT(i_to_r, 2, 4);
  hdr->qoffset_z = *MATRIX_RELT(i_to_r, 3, 4);

  MatrixFree(&i_to_r);

  return(NO_ERROR);

} /* end mriToNiftiQform() */

static void swap_nifti_1_header(struct nifti_1_header *hdr)
{

  int i;

  hdr->sizeof_hdr = swapInt(hdr->sizeof_hdr);

  for(i = 0;i < 8;i++)
    hdr->dim[i] = swapShort(hdr->dim[i]);

  hdr->intent_p1 = swapFloat(hdr->intent_p1);
  hdr->intent_p2 = swapFloat(hdr->intent_p2);
  hdr->intent_p3 = swapFloat(hdr->intent_p3);
  hdr->intent_code = swapShort(hdr->intent_code);
  hdr->datatype = swapShort(hdr->datatype);
  hdr->bitpix = swapShort(hdr->bitpix);
  hdr->slice_start = swapShort(hdr->slice_start);

  for(i = 0;i < 8;i++)
    hdr->pixdim[i] = swapFloat(hdr->pixdim[i]);

  hdr->vox_offset = swapFloat(hdr->vox_offset);
  hdr->scl_slope = swapFloat(hdr->scl_slope);
  hdr->scl_inter = swapFloat(hdr->scl_inter);
  hdr->slice_end = swapShort(hdr->slice_end);
  hdr->cal_max = swapFloat(hdr->cal_max);
  hdr->cal_min = swapFloat(hdr->cal_min);
  hdr->slice_duration = swapFloat(hdr->slice_duration);
  hdr->toffset = swapFloat(hdr->toffset);
  hdr->qform_code = swapShort(hdr->qform_code);
  hdr->sform_code = swapShort(hdr->sform_code);
  hdr->quatern_b = swapFloat(hdr->quatern_b);
  hdr->quatern_c = swapFloat(hdr->quatern_c);
  hdr->quatern_d = swapFloat(hdr->quatern_d);
  hdr->qoffset_x = swapFloat(hdr->qoffset_x);
  hdr->qoffset_y = swapFloat(hdr->qoffset_y);
  hdr->qoffset_z = swapFloat(hdr->qoffset_z);

  for(i = 0;i < 4;i++)
    hdr->srow_x[i] = swapFloat(hdr->srow_x[i]);

  for(i = 0;i < 4;i++)
    hdr->srow_y[i] = swapFloat(hdr->srow_y[i]);

  for(i = 0;i < 4;i++)
    hdr->srow_z[i] = swapFloat(hdr->srow_z[i]);

  return;

} /* end swap_nifti_1_header */

MRI *MRIreadGeRoi(char *fname, int n_slices)
{

  MRI *mri;
  int i;
  char prefix[STRLEN], postfix[STRLEN];
  int n_digits;
  FILE *fp;
  int width, height;
  char fname_use[STRLEN];
  int read_one_flag;
  int pixel_data_offset;
  int y;

  if((fp = fopen(fname, "r")) == NULL)
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE, "MRIreadGeRoi(): error opening file %s", fname));
    }

  fseek(fp, 8, SEEK_SET);
  fread(&width, 4, 1, fp);  width = orderIntBytes(width);
  fread(&height, 4, 1, fp);  height = orderIntBytes(height);

  fclose(fp);

  for(i = strlen(fname);i >= 0 && fname[i] != '/';i--);
  i++;

  n_digits = 0;
  for(;fname[i] != '\0' && n_digits < 3;i++)
    {
      if(isdigit(fname[i]))
        n_digits++;
      else
        n_digits = 0;
    }

  if(n_digits < 3)
    {
      errno = 0;
      ErrorReturn(NULL, (ERROR_BADPARM,
                         "MRIreadGeRoi(): bad GE file name (couldn't find "
                         "three consecutive digits in the base file name)"));
    }

  strcpy(prefix, fname);
  prefix[i-3] = '\0';

  strcpy(postfix, &(fname[i]));

  printf("%s---%s\n", prefix, postfix);

  mri = MRIalloc(width, height, n_slices, MRI_SHORT);

  if(mri == NULL)
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_NOMEMORY, "MRIreadGeRoi(): couldn't allocate MRI structure"));
    }

  MRIclear(mri);

  read_one_flag = FALSE;

  for(i = 0;i < n_slices;i++)
    {

      sprintf(fname_use, "%s%03d%s", prefix, i, postfix);
      if((fp = fopen(fname_use, "r")) != NULL)
        {

          fseek(fp, 4, SEEK_SET);
          fread(&pixel_data_offset, 4, 1, fp);
          pixel_data_offset = orderIntBytes(pixel_data_offset);
          fseek(fp, pixel_data_offset, SEEK_SET);

          for(y = 0;y < mri->height;y++)
            {
              if(fread(mri->slices[i][y], 2, mri->width, fp) != mri->width)
                {
                  fclose(fp);
                  MRIfree(&mri);
                  errno = 0;
                  ErrorReturn
                    (NULL,
                     (ERROR_BADFILE,
                      "MRIreadGeRoi(): error reading from file file %s",
                      fname_use));
                }
#if (BYTE_ORDER == LITTLE_ENDIAN)
              swab(mri->slices[i][y], mri->slices[i][y], 2 * mri->width);
#endif
            }

          fclose(fp);
          read_one_flag = TRUE;

        }
    }

  if(!read_one_flag)
    {
      MRIfree(&mri);
      errno = 0;
      ErrorReturn
        (NULL, (ERROR_BADFILE, "MRIreadGeRoi(): didn't read any ROI files"));
    }

  return(mri);

} /* end MRIreadGeRoi() */

/*******************************************************************/
/*******************************************************************/
/*******************************************************************/

static int data_size[] = { 1, 4, 4, 4, 2 };

static MRI *sdtRead(char *fname, int read_volume)
{

  char header_fname[STR_LEN];
  char line[STR_LEN];
  char *colon, *dot;
  FILE *fp;
  MRI *mri;
  int ndim = -1, data_type = -1;
  int dim[4];
  float xsize = 1.0, ysize = 1.0, zsize = 1.0, dummy_size;
  int orientation = MRI_CORONAL;

  dim[0] = -1;
  dim[1] = -1;
  dim[2] = -1;
  dim[3] = -1;

  /* form the header file name */
  strcpy(header_fname, fname);
  if((dot = strrchr(header_fname, '.')))
    sprintf(dot+1, "spr");
  else
    strcat(header_fname, ".spr");

  /* open the header */
  if((fp = fopen(header_fname, "r")) == NULL)
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE,
          "sdtRead(%s): could not open header file %s\n",
          fname, header_fname));
    }

  while(1) // !feof(fp))
    {
      fgets(line, STR_LEN, fp);
      if (feof(fp)) // wow.  we read too many.  get out
        break;

      if((colon = strchr(line, ':')))
        {
          *colon = '\0';
          colon++;
          if(strcmp(line, "numDim") == 0)
            {
              sscanf(colon, "%d", &ndim);
              if(ndim < 3 || ndim > 4)
                {
                  fclose(fp);
                  errno = 0;
                  ErrorReturn
                    (NULL,
                     (ERROR_UNSUPPORTED,
                      "sdtRead(%s): only 3 or 4 dimensions "
                      "supported (numDim = %d)\n",
                      fname, ndim));
                }
            }
          else if(strcmp(line, "dim") == 0)
            {
              if(ndim == -1)
                {
                  fclose(fp);
                  errno = 0;
                  ErrorReturn
                    (NULL,
                     (ERROR_BADFILE,
                      "sdtRead(%s): 'dim' before 'numDim' in header file %s\n",
                      fname, header_fname));
                }
              if(ndim == 3)
                {
                  sscanf(colon, "%d %d %d", &dim[0], &dim[1], &dim[2]);
                  dim[3] = 1;
                }
              else
                {
                  sscanf(colon, "%d %d %d %d",
                         &dim[0], &dim[1], &dim[2], &dim[3]);
                  if(dim[3] != 1)
                    {
                      fclose(fp);
                      errno = 0;
                      ErrorReturn
                        (NULL,
                         (ERROR_UNSUPPORTED,
                          "sdtRead(%s): nframes != 1 unsupported "
                          "for sdt (dim(4) = %d)\n",
                          fname, dim[3]));
                    }
                }
            }
          else if(strcmp(line, "dataType") == 0)
            {
              while(isspace((int)*colon))
                colon++;
              if(strncmp(colon, "BYTE", 4) == 0)
                data_type = MRI_UCHAR;
              else if(strncmp(colon, "WORD", 4) == 0)
                data_type = MRI_SHORT;
              else if(strncmp(colon, "LWORD", 5) == 0)
                data_type = MRI_INT;
              else if(strncmp(colon, "REAL", 4) == 0)
                data_type = MRI_FLOAT;
              else if(strncmp(colon, "COMPLEX", 7) == 0)
                {
                  fclose(fp);
                  errno = 0;
                  ErrorReturn
                    (NULL,
                     (ERROR_UNSUPPORTED,
                      "sdtRead(%s): unsupported data type '%s'\n",
                      fname, colon));
                }
              else
                {
                  fclose(fp);
                  errno = 0;
                  ErrorReturn
                    (NULL,
                     (ERROR_BADFILE,
                      "sdtRead(%s): unknown data type '%s'\n", fname, colon));
                }
            }
          else if(strcmp(line, "interval") == 0)
            {
              if(ndim == 3)
                sscanf(colon, "%f %f %f", &xsize, &ysize, &zsize);
              else
                sscanf(colon, "%f %f %f %f",
                       &xsize, &ysize, &zsize, &dummy_size);
              xsize *= 10.0;
              ysize *= 10.0;
              zsize *= 10.0;
            }
          else if(strcmp(line, "sdtOrient") == 0)
            {
              while(isspace((int)*colon))
                colon++;
              if(strncmp(colon, "sag", 3) == 0)
                orientation = MRI_SAGITTAL;
              else if(strncmp(colon, "ax", 2) == 0)
                orientation = MRI_HORIZONTAL;
              else if(strncmp(colon, "cor", 3) == 0)
                orientation = MRI_CORONAL;
              else
                {
                  fclose(fp);
                  errno = 0;
                  ErrorReturn
                    (NULL,
                     (ERROR_BADFILE,
                      "sdtRead(%s): unknown orientation %s\n", fname, colon));
                }
            }
          else
            {
            }
        }
    }

  fclose(fp);

  if(data_type == -1)
    {
      errno = 0;
      ErrorReturn
        (NULL, (ERROR_BADFILE, "sdtRead(%s): data type undefined\n", fname));
    }
  if(dim[0] == -1 || dim[1] == -1 || dim[2] == -1)
    {
      errno = 0;
      ErrorReturn
        (NULL,
         (ERROR_BADFILE,
          "sdtRead(%s): one or more dimensions undefined\n", fname));
    }

  if(read_volume)
    {
      if((fp = fopen(fname, "r")) == NULL)
        {
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADFILE,
              "sdtRead(%s): error opening data file %s\n", fname, fname));
        }

      mri = MRIreadRaw(fp, dim[0], dim[1], dim[2], data_type);

      if(mri == NULL)
        return(NULL);

      fclose(fp);

    }
  else
    {
      mri = MRIallocHeader(dim[0], dim[1], dim[2], data_type);
      if(mri == NULL)
        return(NULL);
    }

  mri->xsize = xsize;
  mri->ysize = ysize;
  mri->zsize = zsize;

  setDirectionCosine(mri, orientation);

  mri->thick = mri->zsize;
  mri->xend = mri->xsize * mri->width / 2.;
  mri->yend = mri->ysize * mri->height / 2.;
  mri->zend = mri->zsize * mri->depth / 2.;
  mri->xstart = -mri->xend;
  mri->ystart = -mri->yend;
  mri->zstart = -mri->zend;

  mri->imnr0 = 1;
  mri->imnr1 = dim[2];

  mri->ps = 1.0 /*0.001*/;
  mri->tr = 0 ;
  mri->te = 0 ;
  mri->ti = 0 ;

  strcpy(mri->fname, fname) ;

  return(mri);

} /* end sdtRead() */

MRI *MRIreadRaw(FILE *fp, int width, int height, int depth, int type)
{
  MRI     *mri ;
  BUFTYPE *buf ;
  int     slice, pixels ;
  int     i;

  mri = MRIalloc(width, height, depth, type) ;
  if (!mri)
    return(NULL) ;

  pixels = width*height ;
  buf = (BUFTYPE *)calloc(pixels, data_size[type]) ;

  /* every width x height pixels should be another slice */
  for (slice = 0 ; slice < depth ; slice++)
    {
      if (fread(buf, data_size[type], pixels, fp) != pixels)
        {
          errno = 0;
          ErrorReturn(NULL,
                      (ERROR_BADFILE, "%s: could not read %dth slice (%d)",
                       Progname, slice, pixels)) ;
        }
      if(type == 0)
        local_buffer_to_image(buf, mri, slice, 0) ;
      if(type == 1)
        {
          for(i = 0;i < pixels;i++)
            ((int *)buf)[i] = orderIntBytes(((int *)buf)[i]);
          int_local_buffer_to_image((int *)buf, mri, slice, 0);
        }
      if(type == 2)
        {
          for(i = 0;i < pixels;i++)
            ((long32 *)buf)[i] = orderLong32Bytes(((long32 *)buf)[i]);
          long32_local_buffer_to_image((long32 *)buf, mri, slice, 0);
        }
      if(type == 3)
        {
          for(i = 0;i < pixels;i++)
            ((float *)buf)[i] = orderFloatBytes(((float *)buf)[i]);
          float_local_buffer_to_image((float *)buf, mri, slice, 0);
        }
      if(type == 4)
        {
          for(i = 0;i < pixels;i++)
            ((short *)buf)[i] = orderShortBytes(((short *)buf)[i]);
          short_local_buffer_to_image((short *)buf, mri, slice, 0);
        }
    }

  MRIinitHeader(mri) ;
  free(buf) ;
  return(mri) ;
}

static void
int_local_buffer_to_image(int *buf, MRI *mri, int slice, int frame)
{
  int           y, width, height ;
  int           *pslice ;

  width = mri->width ;
  height = mri->height ;
  for (y = 0 ; y < height ; y++)
    {
      pslice = &MRIIseq_vox(mri, 0, y, slice, frame) ;
      memcpy(pslice, buf, width*sizeof(int)) ;
      buf += width ;
    }
}

#if 0
static void
image_to_int_buffer(int *buf, MRI *mri, int slice)
{
  int y, x, width, height, depth ;

  width = mri->width ;
  height = mri->height ;
  depth = mri->depth;
  for (y=0; y < height ; y++)
    {
      if(mri->type == MRI_UCHAR)
        {
          for(x = 0;x < depth;x++)
            buf[x] = (int)MRIvox(mri, x, y, slice);
        }
      else if(mri->type == MRI_SHORT)
        {
          for(x = 0;x < depth;x++)
            buf[x] = (int)MRISvox(mri, x, y, slice);
        }
      else if(mri->type == MRI_LONG)
        {
          for(x = 0;x < depth;x++)
            buf[x] = (int)MRILvox(mri, x, y, slice);
        }
      else if(mri->type == MRI_FLOAT)
        {
          for(x = 0;x < depth;x++)
            buf[x] = (int)MRIFvox(mri, x, y, slice);
        }
      else
        {
          memcpy(buf, mri->slices[slice][y], width*sizeof(int)) ;
        }

      buf += width ;
    }
}
static void
image_to_long_buffer(long *buf, MRI *mri, int slice)
{
  int y, x, width, height, depth ;

  width = mri->width ;
  height = mri->height ;
  depth = mri->depth;
  for (y=0; y < height ; y++)
    {
      if(mri->type == MRI_UCHAR)
        {
          for(x = 0;x < depth;x++)
            buf[x] = (long)MRIvox(mri, x, y, slice);
        }
      else if(mri->type == MRI_INT)
        {
          for(x = 0;x < depth;x++)
            buf[x] = (long)MRIIvox(mri, x, y, slice);
        }
      else if(mri->type == MRI_SHORT)
        {
          for(x = 0;x < depth;x++)
            buf[x] = (long)MRISvox(mri, x, y, slice);
        }
      else if(mri->type == MRI_FLOAT)
        {
          for(x = 0;x < depth;x++)
            buf[x] = (long)MRIFvox(mri, x, y, slice);
        }
      else
        {
          memcpy(buf, mri->slices[slice][y], width*sizeof(long)) ;
        }

      buf += width ;
    }
}
static void
image_to_float_buffer(float *buf, MRI *mri, int slice)
{
  int y, x, width, height, depth ;

  width = mri->width ;
  height = mri->height ;
  depth = mri->depth;
  for (y=0; y < height ; y++)
    {
      if(mri->type == MRI_UCHAR)
        {
          for(x = 0;x < depth;x++)
            buf[x] = (float)MRIvox(mri, x, y, slice);
        }
      else if(mri->type == MRI_INT)
        {
          for(x = 0;x < depth;x++)
            buf[x] = (float)MRIIvox(mri, x, y, slice);
        }
      else if(mri->type == MRI_LONG)
        {
          for(x = 0;x < depth;x++)
            buf[x] = (float)MRILvox(mri, x, y, slice);
        }
      else if(mri->type == MRI_SHORT)
        {
          for(x = 0;x < depth;x++)
            buf[x] = (float)MRISvox(mri, x, y, slice);
        }
      else
        {
          memcpy(buf, mri->slices[slice][y], width*sizeof(float)) ;
        }

      buf += width ;
    }
}
#endif

static void
long32_local_buffer_to_image(long32 *buf, MRI *mri, int slice, int frame)
{
  int           y, width, height ;
  long32          *pslice ;

  width = mri->width ;
  height = mri->height ;
  for (y = 0 ; y < height ; y++)
    {
      pslice = &MRILseq_vox(mri, 0, y, slice, frame) ;
      memcpy(pslice, buf, width*sizeof(long)) ;
      buf += width ;
    }
}


static void
float_local_buffer_to_image(float *buf, MRI *mri, int slice, int frame)
{
  int           y, width, height ;
  float         *pslice ;

  width = mri->width ;
  height = mri->height ;
  for (y = 0 ; y < height ; y++)
    {
      pslice = &MRIFseq_vox(mri, 0, y, slice, frame) ;
      memcpy(pslice, buf, width*sizeof(float)) ;
      buf += width ;
    }
}

static void
short_local_buffer_to_image(short *buf, MRI *mri, int slice, int frame)
{
  int           y, width, height ;
  short         *pslice ;

  width = mri->width ;
  height = mri->height ;
  for (y = 0 ; y < height ; y++)
    {
      pslice = &MRISseq_vox(mri, 0, y, slice, frame) ;
      memcpy(pslice, buf, width*sizeof(short)) ;
      buf += width ;
    }
}

static void
local_buffer_to_image(BUFTYPE *buf, MRI *mri, int slice, int frame)
{
  int           y, width, height ;
  BUFTYPE       *pslice ;

  width = mri->width ;
  height = mri->height ;
  for (y = 0 ; y < height ; y++)
    {
      pslice = &MRIseq_vox(mri, 0, y, slice, frame) ;
      memcpy(pslice, buf, width*sizeof(BUFTYPE)) ;
      buf += width ;
    }
}

#define UNUSED_SPACE_SIZE 256
#define USED_SPACE_SIZE   (3*sizeof(float)+4*3*sizeof(float))

#define MGH_VERSION       1

// declare function pointer
static int (*myclose)(FILE *stream);

static MRI *
mghRead(char *fname, int read_volume, int frame)
{
  MRI  *mri ;
  FILE  *fp = 0;
  int   start_frame, end_frame, width, height, depth, nframes, type, x, y, z,
    bpv, dof, bytes, version, ival, unused_space_size, good_ras_flag, i ;
  BUFTYPE *buf ;
  char   unused_buf[UNUSED_SPACE_SIZE+1] ;
  float  fval, xsize, ysize, zsize, x_r, x_a, x_s, y_r, y_a, y_s,
    z_r, z_a, z_s, c_r, c_a, c_s, xfov, yfov, zfov ;
  short  sval ;
  //  int tag_data_size;
  char *ext;
  int gzipped=0;
  char command[STRLEN];
  int nread;
  int tag;

  ext = strrchr(fname, '.') ;
  if (ext){
    ++ext;
    // if mgz, then it is compressed
    if (!stricmp(ext, "mgz") || strstr(fname, "mgh.gz")){
      gzipped = 1;
      myclose = pclose;  // assign function pointer for closing
#ifdef Darwin
      // zcat on Max OS always appends and assumes a .Z extention,
      // whereas we want .m3z
      strcpy(command, "gunzip -c ");
#else
      strcpy(command, "zcat ");
#endif
      strcat(command, fname);

      errno = 0;
      fp = popen(command, "r");
      if (!fp){
        errno = 0;
        ErrorReturn(NULL, (ERROR_BADPARM,
                           "mghRead: encountered error executing: '%s'"
                           ",frame %d",
                           command,frame)) ;
      }
      if (errno){
        pclose(fp);
        errno = 0;
        ErrorReturn(NULL, (ERROR_BADPARM,
                           "mghRead: encountered error executing: '%s'"
                           ",frame %d",
                           command,frame)) ;
      }
    }
    else if (!stricmp(ext, "mgh")){
      myclose = fclose; // assign function pointer for closing
      fp = fopen(fname, "rb") ;
      if (!fp)
        {
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADPARM,
	      "mghRead(%s, %d): could not open file",
              fname, frame)) ;
        }
    }
  }

  // sanity-check that a file was actually opened
  if (!fp)
    ErrorReturn(NULL,(ERROR_BADPARM,
		      "mghRead(%s, %d): could not open file.\n"
		      "Filename extension must be .mgh, .mgh.gz or .mgz",
		      fname, frame));

  /* keep the compiler quiet */
  xsize = ysize = zsize = 0;
  x_r = x_a = x_s = 0;
  y_r = y_a = y_s = 0;
  z_r = z_a = z_s = 0;
  c_r = c_a = c_s = 0;

  nread = freadIntEx(&version, fp) ;
  if (!nread)
    ErrorReturn(NULL, (ERROR_BADPARM,"mghRead(%s, %d): read error",
                       fname, frame)) ;

  width = freadInt(fp) ;
  height = freadInt(fp) ;
  depth =  freadInt(fp) ;
  nframes = freadInt(fp) ;
  type = freadInt(fp) ;
  dof = freadInt(fp) ;

  unused_space_size = UNUSED_SPACE_SIZE-sizeof(short) ;

  good_ras_flag = freadShort(fp) ;
  if (good_ras_flag > 0){     /* has RAS and voxel size info */
    unused_space_size -= USED_SPACE_SIZE ;
    xsize = freadFloat(fp) ;
    ysize = freadFloat(fp) ;
    zsize = freadFloat(fp) ;

    x_r = freadFloat(fp) ; x_a = freadFloat(fp) ; x_s = freadFloat(fp) ;
    y_r = freadFloat(fp) ; y_a = freadFloat(fp) ; y_s = freadFloat(fp) ;

    z_r = freadFloat(fp) ; z_a = freadFloat(fp) ; z_s = freadFloat(fp) ;
    c_r = freadFloat(fp) ; c_a = freadFloat(fp) ; c_s = freadFloat(fp) ;
  }
  /* so stuff can be added to the header in the future */
  fread(unused_buf, sizeof(char), unused_space_size, fp) ;

  switch (type){
  default:
  case MRI_FLOAT:  bpv = sizeof(float) ; break ;
  case MRI_UCHAR:  bpv = sizeof(char)  ; break ;
  case MRI_SHORT:  bpv = sizeof(short) ; break ;
  case MRI_INT:    bpv = sizeof(int) ; break ;
  case MRI_TENSOR:  bpv = sizeof(float) ; nframes = 9 ; break ;
  }
  bytes = width * height * bpv ;  /* bytes per slice */
  if(!read_volume){
    mri = MRIallocHeader(width, height, depth, type) ;
    mri->dof = dof ; mri->nframes = nframes ;
    if(gzipped){ // pipe cannot seek
      int count;
      for (count=0; count < mri->nframes*width*height*depth*bpv; count++)
        fgetc(fp);
    }
    else
      fseek(fp, mri->nframes*width*height*depth*bpv, SEEK_CUR) ;
  }
  else{
    if (frame >= 0) {
      start_frame = end_frame = frame ;
      if (gzipped){ // pipe cannot seek
        int count;
        for (count=0; count < frame*width*height*depth*bpv; count++)
          fgetc(fp);
      }
      else
        fseek(fp, frame*width*height*depth*bpv, SEEK_CUR) ;
      nframes = 1 ;
    }
    else{  /* hack - # of frames < -1 means to only read in that
              many frames. Otherwise I would have had to change the whole
              MRIread interface and that was too much of a pain. Sorry.
           */
      if (frame < -1)
        nframes = frame*-1 ;

      start_frame = 0 ; end_frame = nframes-1 ;
      if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
        fprintf(stderr, "read %d frames\n", nframes);
    }
    buf = (BUFTYPE *)calloc(bytes, sizeof(BUFTYPE)) ;
    mri = MRIallocSequence(width, height, depth, type, nframes) ;
    mri->dof = dof ;
    for (frame = start_frame ; frame <= end_frame ; frame++){
      for (z = 0 ; z < depth ; z++){
        if (fread(buf, sizeof(char), bytes, fp) != bytes){
          // fclose(fp) ;
          myclose(fp);
          free(buf) ;
          ErrorReturn
            (NULL,
             (ERROR_BADFILE,
              "mghRead(%s): could not read %d bytes at slice %d",
              fname, bytes, z)) ;
        }
        switch (type)
          {
          case MRI_INT:
            for (i = y = 0 ; y < height ; y++)
              {
                for (x = 0 ; x < width ; x++, i++)
                  {
                    ival = orderIntBytes(((int *)buf)[i]) ;
                    MRIIseq_vox(mri,x,y,z,frame-start_frame) = ival ;
                  }
              }
            break ;
          case MRI_SHORT:
            for (i = y = 0 ; y < height ; y++)
              {
                for (x = 0 ; x < width ; x++, i++)
                  {
                    sval = orderShortBytes(((short *)buf)[i]) ;
                    MRISseq_vox(mri,x,y,z,frame-start_frame) = sval ;
                  }
              }
            break ;
          case MRI_TENSOR:
          case MRI_FLOAT:
            for (i = y = 0 ; y < height ; y++)
              {
                for (x = 0 ; x < width ; x++, i++)
                  {
                    fval = orderFloatBytes(((float *)buf)[i]) ;
                    MRIFseq_vox(mri,x,y,z,frame-start_frame) = fval ;
                  }
              }
            break ;
          case MRI_UCHAR:
            local_buffer_to_image(buf, mri, z, frame-start_frame) ;
            break ;
          default:
            errno = 0;
            ErrorReturn(NULL,
                        (ERROR_UNSUPPORTED, "mghRead: unsupported type %d",
                         mri->type)) ;
            break ;
          }
      }
    }
    if (buf) free(buf) ;
  }

  if(good_ras_flag > 0){
    mri->xsize =     xsize ;
    mri->ysize =     ysize ;
    mri->zsize =     zsize ;

    mri->x_r = x_r  ;
    mri->x_a = x_a  ;
    mri->x_s = x_s  ;

    mri->y_r = y_r  ;
    mri->y_a = y_a  ;
    mri->y_s = y_s  ;

    mri->z_r = z_r  ;
    mri->z_a = z_a  ;
    mri->z_s = z_s  ;

    mri->c_r = c_r  ;
    mri->c_a = c_a  ;
    mri->c_s = c_s  ;
    if (good_ras_flag > 0)
      mri->ras_good_flag = 1 ;
  }
  else{
    fprintf
      (stderr,
       "-----------------------------------------------------------------\n"
       "Could not find the direction cosine information.\n"
       "Will use the CORONAL orientation.\n"
       "If not suitable, please provide the information in %s.\n"
       "-----------------------------------------------------------------\n",
       fname
       );
    setDirectionCosine(mri, MRI_CORONAL);
  }
  // read TR, Flip, TE, TI, FOV
  if (freadFloatEx(&(mri->tr), fp)){
    if (freadFloatEx(&fval, fp))
      {
        mri->flip_angle = fval;
        // flip_angle is double. I cannot use the same trick.
        if (freadFloatEx(&(mri->te), fp))
          if (freadFloatEx(&(mri->ti), fp))
            if (freadFloatEx(&(mri->fov), fp))
              ;
      }
  }
  // tag reading
  {
    long long len ;
    char *fnamedir;

    while ((tag = TAGreadStart(fp, &len)) != 0){
      switch (tag){
      case TAG_OLD_MGH_XFORM:
      case TAG_MGH_XFORM:
        fgets(mri->transform_fname, len+1, fp);
        // if this file exists then read the transform
        if(!FileExists(mri->transform_fname)){
          fnamedir = fio_dirname(fname);
	  // If the transform name is auto, don't report that it cannot be found
	  if(strcmp(mri->transform_fname,"auto"))
	    printf("  Talairach transform %s does not exist ...\n",
		   mri->transform_fname);
          sprintf(mri->transform_fname,"%s/transforms/talairach.xfm",fnamedir);
	  if(!FileExists(mri->transform_fname))
	    printf("Cannot load tal xfm file %s\n",mri->transform_fname);
	  else
	    printf("Loading tal xfm file %s\n",mri->transform_fname);

          free(fnamedir);
        }
        if(FileExists(mri->transform_fname)){
          // copied from corRead()
          if(input_transform_file
             (mri->transform_fname, &(mri->transform)) == NO_ERROR){
            mri->linear_transform = get_linear_transform_ptr(&mri->transform);
            mri->inverse_linear_transform =
              get_inverse_linear_transform_ptr(&mri->transform);
            mri->free_transform = 1;
            if (DIAG_VERBOSE_ON)
              fprintf
                (stderr,
                 "INFO: loaded talairach xform : %s\n", mri->transform_fname);
          }
          else{
            errno = 0;
            ErrorPrintf
              (ERROR_BAD_FILE,
               "error loading transform from %s",mri->transform_fname);
            mri->linear_transform = NULL;
            mri->inverse_linear_transform = NULL;
            mri->free_transform = 1;
            (mri->transform_fname)[0] = '\0';
          }
        }
        else{
          fprintf(stderr,
                  "Can't find the talairach xform '%s'\n",
                  mri->transform_fname);
          fprintf(stderr, "Transform is not loaded into mri\n");
        }
        break ;
      case TAG_CMDLINE:
        if (mri->ncmds > MAX_CMDS)
          ErrorExit(ERROR_NOMEMORY,
                    "mghRead(%s): too many commands (%d) in file",
                    fname,mri->ncmds);
        mri->cmdlines[mri->ncmds] = calloc(len+1, sizeof(char)) ;
        fread(mri->cmdlines[mri->ncmds], sizeof(char), len, fp) ;
        mri->cmdlines[mri->ncmds][len] = 0 ;
        mri->ncmds++ ;
        break ;
      default:
        TAGskip(fp, tag, (long long)len) ;
        break ;
      }
    }
  }


  // fclose(fp) ;
  myclose(fp);

  // xstart, xend, ystart, yend, zstart, zend are not stored
  mri->xstart = - mri->width/2.*mri->xsize;
  mri->xend = mri->width/2. * mri->xsize;
  mri->ystart = - mri->height/2.*mri->ysize;
  mri->yend = mri->height/2.*mri->ysize;
  mri->zstart = - mri->depth/2.*mri->zsize;
  mri->zend = mri->depth/2.*mri->zsize;
  xfov = mri->xend - mri->xstart;
  yfov = mri->yend - mri->ystart;
  zfov = mri->zend - mri->zstart;
  mri->fov =
    (xfov > yfov ? (xfov > zfov ? xfov : zfov) : (yfov > zfov ? yfov : zfov));

  strcpy(mri->fname, fname);
  return(mri) ;
}

static int
mghWrite(MRI *mri, char *fname, int frame)
{
  FILE  *fp =0;
  int   ival, start_frame, end_frame, x, y, z, width, height, depth,
    unused_space_size, flen ;
  char  buf[UNUSED_SPACE_SIZE+1] ;
  float fval ;
  short sval ;
  int gzipped = 0;
  char *ext;

  if (frame >= 0)
    start_frame = end_frame = frame ;
  else
    {
      start_frame = 0 ; end_frame = mri->nframes-1 ;
    }
  ////////////////////////////////////////////////////////////
  ext = strrchr(fname, '.') ;
  if (ext)
    {
      char command[STRLEN];
      ++ext;
      // if mgz, then it is compressed
      if (!stricmp(ext, "mgz") || strstr(fname, "mgh.gz"))
        {
          // route stdout to a file
          gzipped = 1;
          myclose = pclose; // assign function pointer for closing
          // pipe writeto "gzip" open
          // pipe can executed under shell and thus understands >
          strcpy(command, "gzip -f -c > ");
          strcat(command, fname);
          errno = 0;
          fp = popen(command, "w");
          if (!fp)
            {
              errno = 0;
              ErrorReturn
                (ERROR_BADPARM,
                 (ERROR_BADPARM,"mghWrite(%s, %d): could not open file",
                  fname, frame)) ;
            }
          if (errno)
            {
              pclose(fp);
              errno = 0;
              ErrorReturn
                (ERROR_BADPARM,
                 (ERROR_BADPARM,"mghWrite(%s, %d): gzip had error",
                  fname, frame)) ;
            }
        }
      else if (!stricmp(ext, "mgh"))
        {
          fp = fopen(fname, "wb") ;
          myclose = fclose; // assign function pointer for closing
          if (!fp)
            {
              errno = 0;
              ErrorReturn
                (ERROR_BADPARM,
                 (ERROR_BADPARM,"mghWrite(%s, %d): could not open file",
                  fname, frame)) ;
            }
        }
    }
  else
    {
      errno = 0;
      ErrorReturn(ERROR_BADPARM,
                  (ERROR_BADPARM,"mghWrite: filename '%s' "
                   "needs to have an extension of .mgh or .mgz",
                   fname)) ;
    }

  // sanity-check: make sure a file pointer was assigned
  if (fp==0)
    {
      errno = 0;
      ErrorReturn
        (ERROR_BADPARM,
         (ERROR_BADPARM,"mghWrite(%s, %d): could not open file: fp==0",
          fname, frame)) ;
    }

  /* WARNING - adding or removing anything before nframes will
     cause mghAppend to fail.
  */
  width = mri->width ; height = mri->height ; depth = mri->depth ;
  fwriteInt(MGH_VERSION, fp) ;
  fwriteInt(mri->width, fp) ;
  fwriteInt(mri->height, fp) ;
  fwriteInt(mri->depth, fp) ;
  fwriteInt(mri->nframes, fp) ;
  fwriteInt(mri->type, fp) ;
  fwriteInt(mri->dof, fp) ;

  unused_space_size = UNUSED_SPACE_SIZE - USED_SPACE_SIZE - sizeof(short) ;

  /* write RAS and voxel size info */
  fwriteShort(mri->ras_good_flag ? 1 : -1, fp) ;
  fwriteFloat(mri->xsize, fp) ;
  fwriteFloat(mri->ysize, fp) ;
  fwriteFloat(mri->zsize, fp) ;

  fwriteFloat(mri->x_r, fp) ;
  fwriteFloat(mri->x_a, fp) ;
  fwriteFloat(mri->x_s, fp) ;

  fwriteFloat(mri->y_r, fp) ;
  fwriteFloat(mri->y_a, fp) ;
  fwriteFloat(mri->y_s, fp) ;

  fwriteFloat(mri->z_r, fp) ;
  fwriteFloat(mri->z_a, fp) ;
  fwriteFloat(mri->z_s, fp) ;

  fwriteFloat(mri->c_r, fp) ;
  fwriteFloat(mri->c_a, fp) ;
  fwriteFloat(mri->c_s, fp) ;

  /* so stuff can be added to the header in the future */
  memset(buf, 0, UNUSED_SPACE_SIZE*sizeof(char)) ;
  fwrite(buf, sizeof(char), unused_space_size, fp) ;

  for (frame = start_frame ; frame <= end_frame ; frame++)
    {
      for (z = 0 ; z < depth ; z++)
        {
          for (y = 0 ; y < height ; y++)
            {
              switch (mri->type)
                {
                case MRI_SHORT:
                  for (x = 0 ; x < width ; x++)
                    {
                      if (z == 74 && y == 16 && x == 53)
                        DiagBreak() ;
                      sval = MRISseq_vox(mri,x,y,z,frame) ;
                      fwriteShort(sval, fp) ;
                    }
                  break ;
                case MRI_INT:
                  for (x = 0 ; x < width ; x++)
                    {
                      if (z == 74 && y == 16 && x == 53)
                        DiagBreak() ;
                      ival = MRIIseq_vox(mri,x,y,z,frame) ;
                      fwriteInt(ival, fp) ;
                    }
                  break ;
                case MRI_FLOAT:
                  for (x = 0 ; x < width ; x++)
                    {
                      if (z == 74 && y == 16 && x == 53)
                        DiagBreak() ;
                      fval = MRIFseq_vox(mri,x,y,z,frame) ;
                      //if(x==10 && y == 0 && z == 0 && frame == 67)
                      // printf("MRIIO: %g\n",fval);
                      fwriteFloat(fval, fp) ;
                    }
                  break ;
                case MRI_UCHAR:
                  if (fwrite(&MRIseq_vox(mri,0,y,z,frame),
                             sizeof(BUFTYPE), width, fp)
                      != width)
                    {
                      errno = 0;
                      ErrorReturn
                        (ERROR_BADFILE,
                         (ERROR_BADFILE,
                          "mghWrite: could not write %d bytes to %s",
                          width, fname)) ;
                    }
                  break ;
                default:
                  errno = 0;
                  ErrorReturn
                    (ERROR_UNSUPPORTED,
                     (ERROR_UNSUPPORTED, "mghWrite: unsupported type %d",
                      mri->type)) ;
                  break ;
                }
            }
        }
    }

  fwriteFloat(mri->tr, fp) ;
  fwriteFloat(mri->flip_angle, fp) ;
  fwriteFloat(mri->te, fp) ;
  fwriteFloat(mri->ti, fp) ;
  fwriteFloat(mri->fov, fp); // somebody forgot this

  // if mri->transform_fname has non-zero length
  // I write a tag with strlength and write it
  // I increase the tag_datasize with this amount
  if ((flen=strlen(mri->transform_fname))> 0)
    {
#if 0
      fwriteInt(TAG_MGH_XFORM, fp);
      fwriteInt(flen+1, fp); // write the size + 1 (for null) of string
      fputs(mri->transform_fname, fp);
#else
      TAGwrite(fp, TAG_MGH_XFORM, mri->transform_fname, flen+1) ;
#endif
    }
  // If we have any saved tag data, write it.
  if( NULL != mri->tag_data ) {
    // Int is 32 bit on 32 bit and 64 bit os and thus it is safer
    fwriteInt(mri->tag_data_size, fp);
    fwrite( mri->tag_data, mri->tag_data_size, 1, fp );
  }


  // write other tags
  {
    int i ;

    for (i = 0 ; i < mri->ncmds ; i++)
      TAGwrite(fp, TAG_CMDLINE, mri->cmdlines[i], strlen(mri->cmdlines[i])+1) ;
  }


  // fclose(fp) ;
  myclose(fp);

  return(NO_ERROR) ;
}


MRI *
MRIreorder(MRI *mri_src, MRI *mri_dst, int xdim, int ydim, int zdim)
{
  // note that allowed xdim, ydim, zdim values are only +/- 1,2,3 only
  // xdim tells the original x-axis goes to which axis of the new, etc.
  int  width, height, depth, xs, ys, zs, xd, yd, zd, x, y, z, f ;
  float ras_sign;

  width = mri_src->width ; height = mri_src->height ; depth = mri_src->depth;
  if (!mri_dst)
    mri_dst = MRIclone(mri_src, NULL) ;

  /* check that the source ras coordinates are good and
     that each direction is used once and only once */
  if(mri_src->ras_good_flag)
    if(abs(xdim) * abs(ydim) * abs(zdim) != 6 ||
       abs(xdim) + abs(ydim) + abs(zdim) != 6)
      mri_dst->ras_good_flag = 0;

  xd = yd = zd = 0 ;

  ras_sign = (xdim < 0 ? -1.0 : 1.0);
  // check xdim direction size (XDIM=1, YDIM=2, ZDIM=3)
  // xdim tells original x-axis goes to where
  switch (abs(xdim))
    {
    default:
    case XDIM:
      if (mri_dst->width != width)
        {
          errno = 0;
          ErrorReturn(NULL,(ERROR_BADPARM, "MRIreorder: incorrect dst width"));
        }
      break ;
    case YDIM:
      if (mri_dst->height != width)
        {
          errno = 0;
          ErrorReturn(NULL,(ERROR_BADPARM, "MRIreorder: incorrect dst width"));
        }
      break ;
    case ZDIM:
      if (mri_dst->depth != width)
        {
          errno = 0;
          ErrorReturn(NULL,(ERROR_BADPARM, "MRIreorder: incorrect dst width"));
        }
      break ;
    }
  ras_sign = (ydim < 0 ? -1.0 : 1.0);
  // check ydim
  // ydim tells original y-axis goes to where
  switch (abs(ydim))
    {
    default:
    case XDIM:
      if (mri_dst->width != height)
        {
          errno = 0;
          ErrorReturn
            (NULL,(ERROR_BADPARM, "MRIreorder: incorrect dst height"));
        }
      break ;
    case YDIM:
      if (mri_dst->height != height)
        {
          errno = 0;
          ErrorReturn
            (NULL,(ERROR_BADPARM, "MRIreorder: incorrect dst height"));
        }
      break ;
    case ZDIM:
      if (mri_dst->depth != height)
        {
          errno = 0;
          ErrorReturn
            (NULL,(ERROR_BADPARM, "MRIreorder: incorrect dst height"));
        }
      break ;
    }
  ras_sign = (zdim < 0 ? -1.0 : 1.0);
  // check zdim
  // zdim tells original z-axis goes to where
  switch (abs(zdim))
    {
    default:
    case XDIM:
      if (mri_dst->width != depth)
        {
          errno = 0;
          ErrorReturn(NULL,(ERROR_BADPARM, "MRIreorder: incorrect dst depth"));
        }
      break ;
    case YDIM:
      if (mri_dst->height != depth)
        {
          errno = 0;
          ErrorReturn(NULL,(ERROR_BADPARM, "MRIreorder: incorrect dst depth"));
        }
      break ;
    case ZDIM:
      if (mri_dst->depth != depth)
        {
          errno = 0;
          ErrorReturn(NULL,(ERROR_BADPARM, "MRIreorder: incorrect dst depth"));
        }
      break ;
    }

  for(f = 0; f < mri_src->nframes; f++){
    for (zs = 0 ; zs < depth ; zs++)
      {
        if (zdim < 0) z = depth - zs - 1 ;
        else          z = zs ;
        switch (abs(zdim))
          {
          case XDIM:  xd = z ; break ;
          case YDIM:  yd = z ; break ;
          case ZDIM:
          default:    zd = z ; break ;
          }
        for (ys = 0 ; ys < height ; ys++)
          {
            if (ydim < 0) y = height - ys - 1 ;
            else          y = ys ;
            switch (abs(ydim))
              {
              case XDIM: xd = y ; break ;
              case YDIM: yd = y ; break ;
              case ZDIM:
              default: zd = y ; break ;
              }
            for (xs = 0 ; xs < width ; xs++)
              {
                if (xdim < 0) x = width - xs - 1 ;
                else          x = xs ;
                switch (abs(xdim))
                  {
                  case XDIM: xd = x ; break ;
                  case YDIM: yd = x ; break ;
                  case ZDIM:
                  default:   zd = x ; break ;
                  }
                switch (mri_src->type)
                  {
                  case MRI_SHORT:
                    MRISseq_vox(mri_dst,xd,yd,zd,f) =
                      MRISseq_vox(mri_src,xs,ys,zs,f) ;
                    break ;
                  case MRI_FLOAT:
                    MRIFseq_vox(mri_dst,xd,yd,zd,f) =
                      MRIFseq_vox(mri_src,xs,ys,zs,f);
                    break ;
                  case MRI_INT:
                    MRIIseq_vox(mri_dst,xd,yd,zd,f) =
                      MRIIseq_vox(mri_src,xs,ys,zs,f);
                    break ;
                  case MRI_UCHAR:
                    MRIseq_vox(mri_dst,xd,yd,zd,f)  =
                      MRIseq_vox(mri_src,xs,ys,zs,f);
                    break ;
                  default:
                    errno = 0;
                    ErrorReturn
                      (NULL,(ERROR_UNSUPPORTED,
                             "MRIreorder: unsupported voxel format %d",
                             mri_src->type)) ;
                    break ;
                  }
              }
          }
      }
  }/* end loop over frames */
  return(mri_dst) ;
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  Write the MRI header information to the file
  COR-.info in the directory specified by 'fpref'
  ------------------------------------------------------*/
int
MRIwriteInfo(MRI *mri, char *fpref)
{
  FILE    *fp;
  char    fname[STRLEN];
  int     slice_direction;
  sprintf(fname,"%s/%s",fpref, INFO_FNAME);
  fp = fopen(fname,"w");
  if (fp == NULL)
    {
      errno = 0;
      ErrorReturn(ERROR_NO_FILE,
                  (ERROR_NO_FILE,
                   "MRIwriteInfo(%s): could not open %s.\n", fpref, fname)) ;
    }

  fprintf(fp, "%s %d\n", "imnr0", mri->imnr0);
  fprintf(fp, "%s %d\n", "imnr1", mri->imnr1);
  slice_direction = getSliceDirection(mri);
  fprintf(fp, "%s %d\n", "ptype",
          slice_direction == MRI_CORONAL ? 2 :
          slice_direction == MRI_HORIZONTAL ? 0 : 1) ;
  fprintf(fp, "%s %d\n", "x", mri->width);
  fprintf(fp, "%s %d\n", "y", mri->height);
  fprintf(fp, "%s %f\n", "fov", mri->fov/MM_PER_METER);
  fprintf(fp, "%s %f\n", "thick", mri->ps/MM_PER_METER);
  fprintf(fp, "%s %f\n", "psiz", mri->ps/MM_PER_METER);
  fprintf(fp, "%s %f\n", "locatn", mri->location); /* locatn */
  fprintf(fp, "%s %f\n", "strtx", mri->xstart/MM_PER_METER); /* strtx */
  fprintf(fp, "%s %f\n", "endx", mri->xend/MM_PER_METER); /* endx */
  fprintf(fp, "%s %f\n", "strty", mri->ystart/MM_PER_METER); /* strty */
  fprintf(fp, "%s %f\n", "endy", mri->yend/MM_PER_METER); /* endy */
  fprintf(fp, "%s %f\n", "strtz", mri->zstart/MM_PER_METER); /* strtz */
  fprintf(fp, "%s %f\n", "endz", mri->zend/MM_PER_METER); /* endz */
  fprintf(fp, "%s %f\n", "tr", mri->tr) ;
  fprintf(fp, "%s %f\n", "te", mri->te) ;
  fprintf(fp, "%s %f\n", "ti", mri->ti) ;
  if (mri->linear_transform)
    {
      char fname[STRLEN] ;

      /*
         this won't work for relative paths which are not the same for the
         destination directory as for the the source directory.
      */
      sprintf(fname,"%s", mri->transform_fname);
      fprintf(fp, "xform %s\n", fname) ;

#if 0
      /* doesn't work - I don't know why */
      if (output_transform_file(fname, "talairach xfm", &mri->transform) != OK)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADFILE, "MRIwriteInfo(%s): xform write failed",fpref);
        }
#endif

    }

  fprintf(fp, "%s %d\n", "ras_good_flag", mri->ras_good_flag);
  fprintf(fp, "%s %f %f %f\n", "x_ras", mri->x_r, mri->x_a, mri->x_s);
  fprintf(fp, "%s %f %f %f\n", "y_ras", mri->y_r, mri->y_a, mri->y_s);
  fprintf(fp, "%s %f %f %f\n", "z_ras", mri->z_r, mri->z_a, mri->z_s);
  fprintf(fp, "%s %f %f %f\n", "c_ras", mri->c_r, mri->c_a, mri->c_s);

  fclose(fp);

  return(NO_ERROR) ;
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  Write an MRI header and a set of data files to
  the directory specified by 'fpref'
  ------------------------------------------------------*/
int
MRIappend(MRI *mri, char *fpref)
{
  int      type, frame ;
  char     fname[STRLEN] ;

  MRIunpackFileName(fpref, &frame, &type, fname) ;
  if (type == MRI_MGH_FILE)
    return(mghAppend(mri, fname, frame)) ;
  else
    {
      errno = 0;
      ErrorReturn(ERROR_UNSUPPORTED,
                  (ERROR_UNSUPPORTED, "MRIappend(%s): file type not supported",
                   fname)) ;
    }

  return(NO_ERROR) ;
}

static int
mghAppend(MRI *mri, char *fname, int frame)
{
  FILE  *fp ;
  int   start_frame, end_frame, x, y, z, width, height, depth, nframes ;

  if (frame >= 0)
    start_frame = end_frame = frame ;
  else
    {
      start_frame = 0 ; end_frame = mri->nframes-1 ;
    }
  fp = fopen(fname, "rb") ;
  if (!fp)   /* doesn't exist */
    return(mghWrite(mri, fname, frame)) ;
  fclose(fp) ;
  fp = fopen(fname, "r+b") ;
  if (!fp)
    {
      errno = 0;
      ErrorReturn(ERROR_BADPARM,
                  (ERROR_BADPARM,"mghAppend(%s, %d): could not open file",
                   fname, frame)) ;
    }

  /* WARNING - this is dependent on the order of writing in mghWrite */
  width = mri->width ; height = mri->height ; depth = mri->depth ;
  fseek(fp, 4*sizeof(int), SEEK_SET) ;
  nframes = freadInt(fp) ;
  fseek(fp, 4*sizeof(int), SEEK_SET) ;
  fwriteInt(nframes+end_frame-start_frame+1, fp) ;
  fseek(fp, 0, SEEK_END) ;

  for (frame = start_frame ; frame <= end_frame ; frame++)
    {
      for (z = 0 ; z < depth ; z++)
        {
          for (y = 0 ; y < height ; y++)
            {
              switch (mri->type)
                {
                case MRI_FLOAT:
                  for (x = 0 ; x < width ; x++)
                    {
                      fwriteFloat(MRIFseq_vox(mri,x,y,z,frame), fp) ;
                    }
                  break ;
                case MRI_UCHAR:
                  if (fwrite(&MRIseq_vox(mri,0,y,z,frame),
                             sizeof(BUFTYPE), width, fp)
                      != width)
                    {
                      errno = 0;
                      ErrorReturn
                        (ERROR_BADFILE,
                         (ERROR_BADFILE,
                          "mghAppend: could not write %d bytes to %s",
                          width, fname)) ;
                    }
                  break ;
                default:
                  errno = 0;
                  ErrorReturn
                    (ERROR_UNSUPPORTED,
                     (ERROR_UNSUPPORTED, "mghAppend: unsupported type %d",
                      mri->type)) ;
                  break ;
                }
            }
        }
    }

  fclose(fp) ;
  return(NO_ERROR) ;
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int
MRIunpackFileName(char *inFname, int *pframe, int *ptype, char *outFname)
{
  char *number = NULL, *at = NULL, buf[STRLEN] ;
  struct stat stat_buf;

  strcpy(outFname, inFname) ;
  number = strrchr(outFname, '#') ;
  at = strrchr(outFname, '@');

  if(at)
    *at = '\0';

  if (number)   /* '#' in filename indicates frame # */
    {
      if (sscanf(number+1, "%d", pframe) < 1)
        *pframe = -1 ;
      *number = 0 ;
    }
  else
    *pframe = -1 ;

  if (at)
    {
      at = StrUpper(strcpy(buf, at+1)) ;
      if (!strcmp(at, "MNC"))
        *ptype = MRI_MINC_FILE ;
      else if (!strcmp(at, "MINC"))
        *ptype = MRI_MINC_FILE ;
      else if (!strcmp(at, "BRIK"))
        *ptype = BRIK_FILE ;
      else if (!strcmp(at, "SIEMENS"))
        *ptype = SIEMENS_FILE ;
      else if (!strcmp(at, "MGH"))
        *ptype = MRI_MGH_FILE ;
      else if (!strcmp(at, "MR"))
        *ptype = GENESIS_FILE ;
      else if (!strcmp(at, "GE"))
        *ptype = GE_LX_FILE ;
      else if (!strcmp(at, "IMG"))
        *ptype = MRI_ANALYZE_FILE ;
      else if (!strcmp(at, "COR"))
        *ptype = MRI_CORONAL_SLICE_DIRECTORY ;
      else if (!strcmp(at, "BSHORT"))
        *ptype = BSHORT_FILE;
      else if (!strcmp(at, "SDT"))
        *ptype = SDT_FILE;
      else
        {
          errno = 0;
          ErrorExit(ERROR_UNSUPPORTED, "unknown file type %s", at);
        }
    }
  else  /* no '@' found */
    {

      *ptype = -1;


      if(is_genesis(outFname))
        *ptype = GENESIS_FILE;
      else if(is_ge_lx(outFname))
        *ptype = GE_LX_FILE;
      else if(is_brik(outFname))
        *ptype = BRIK_FILE;
      else if(is_siemens(outFname))
        *ptype = SIEMENS_FILE;
      else if(is_analyze(outFname))
        *ptype = MRI_ANALYZE_FILE;
      else if (is_signa(outFname))
        *ptype = SIGNA_FILE ;
      else if(is_sdt(outFname))
        *ptype = SDT_FILE;
      else if(is_mgh(outFname))
        *ptype = MRI_MGH_FILE;
      else if(is_mnc(outFname))
        *ptype = MRI_MINC_FILE;
      else if(is_bshort(outFname))
        *ptype = BSHORT_FILE;
      else
        {
          if(stat(outFname, &stat_buf) < 0)
            {
              errno = 0;
              ErrorReturn
                (ERROR_BADFILE,
                 (ERROR_BAD_FILE, "can't stat file %s", outFname));
            }
          if(S_ISDIR(stat_buf.st_mode))
            *ptype = MRI_CORONAL_SLICE_DIRECTORY;
        }

      if(*ptype == -1)
        {
          errno = 0;
          ErrorReturn
            (ERROR_BADPARM,
             (ERROR_BADPARM,
              "unrecognized file type for file %s", outFname));
        }

    }

  return(NO_ERROR) ;
}

/*---------------------------------------------------------------
  MRIwriteAnyFormat() - saves the data in the given mri structure to
  "any" format, where "any" is defined as anything writable by
  MRIwrite and wfile format. If wfile format is used, then mriframe
  must be a valid frame number. The val field of the surf is
  preserved. If another format is used, then, if mriframe = -1, then
  all frames are stored, otherwise the given frame is stored. If
  fmt is NULL, then it will attempt to infer the format from the
  name. surf only needs to be supplied when format is wfile. Legal
  formats include: wfile, paint, w, bshort, bfloat, COR, analyze,
  analyze4d, spm.
  ---------------------------------------------------------------*/
int MRIwriteAnyFormat(MRI *mri, char *fileid, char *fmt,
                      int mriframe, MRIS *surf)
{
  int fmtid, err, n, r, c, s;
  float *v=NULL,f;
  MRI *mritmp=NULL;

  if(fmt != NULL && (!strcmp(fmt,"paint") || !strcmp(fmt,"w") ||
                     !strcmp(fmt,"wfile")) ){

    /* Save as a wfile */
    if(surf == NULL){
      printf("ERROR: MRIwriteAnyFormat: need surf with paint format\n");
      return(1);
    }
    if(mriframe >= mri->nframes){
      printf("ERROR: MRIwriteAnyFormat: frame (%d) exceeds number of\n"
             "       frames\n",mriframe >= mri->nframes);
      return(1);
    }

    /* Copy current surf values into v (temp storage) */
    v = (float *)calloc(surf->nvertices,sizeof(float));
    for(n=0; n < surf->nvertices; n++) v[n] = surf->vertices[n].val;

    /* Copy the mri values into the surf values */
    err = MRIScopyMRI(surf, mri, mriframe, "val");
    if(err){
      printf("ERROR: MRIwriteAnyFormat: could not copy MRI to MRIS\n");
      return(1);
    }

    /* Write the surf values */
    err = MRISwriteValues(surf,fileid);

    /* Copy v back into surf values */
    for(n=0; n < surf->nvertices; n++) surf->vertices[n].val = v[n];
    free(v);

    /* Check for errors from write of values */
    if(err){
      printf("ERROR: MRIwriteAnyFormat: MRISwriteValues\n");
      return(1);
    }

    return(0);
  }

  /* Copy the desired frame if necessary */
  if(mriframe > -1){
    if(mriframe >= mri->nframes){
      printf("ERROR: MRIwriteAnyFormat: frame (%d) exceeds number of\n"
             "       frames\n",mriframe >= mri->nframes);
      return(1);
    }
    mritmp = MRIallocSequence(mri->width, mri->height, mri->depth,
                              mri->type, 1);
    for(c=0; c < mri->width; c++){
      for(r=0; r < mri->height; r++){
        for(s=0; s < mri->depth; s++){
          f = MRIgetVoxVal(mri, c, r, s, mriframe);
          MRIsetVoxVal(mritmp, c, r, s, 0, f);
        }
      }
    }
  }
  else mritmp = mri;

  /*------------ Save using MRIwrite or MRIwriteType ---------*/
  if(fmt != NULL){
    /* Save as the given format */
    fmtid = string_to_type(fmt);
    if(fmtid == MRI_VOLUME_TYPE_UNKNOWN){
      printf("ERROR: format string %s unrecognized\n",fmt);
      return(1);
    }
    err = MRIwriteType(mritmp,fileid,fmtid);
    if(err){
      printf("ERROR: MRIwriteAnyFormat: could not write to %s\n",fileid);
      return(1);
    }
  }
  else{
    /* Try to infer the type and save (format is NULL) */
    err = MRIwrite(mritmp,fileid);
    if(err){
      printf("ERROR: MRIwriteAnyFormat: could not write to %s\n",fileid);
      return(1);
    }
  }

  if(mri != mritmp) MRIfree(&mritmp);

  return(0);
}



/* EOF */


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/* ----------- Obsolete functions below. --------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/


#if 0
/*-------------------------------------------------------------------
  bfloatWrite() - obsolete. Use bvolumeWrite.
  -------------------------------------------------------------------*/
static int bfloatWrite(MRI *vol, char *stem)
{

  int i, j, t;
  char fname[STRLEN];
  float *buf;
  FILE *fp;
  int result;
  MRI *mri = NULL;
  int dealloc;
  int nslices,nframes;

  if(vol->type != MRI_FLOAT){
    printf("INFO: bfloatWrite: changing type\n");
    nslices = vol->depth;
    nframes = vol->nframes;
    vol->depth = nslices*nframes;
    vol->nframes = 1;
    mri = MRIchangeType(vol,MRI_FLOAT,0,0,0);
    if(mri == NULL) {
      fprintf(stderr,"ERROR: bfloatWrite: MRIchangeType\n");
      return(1);
    }
    vol->depth = nslices;
    vol->nframes = nframes;
    mri->depth = nslices;
    mri->nframes = nframes;
    dealloc = 1;
  }
  else{
    mri = vol;
    dealloc = 0;
  }

  buf = (float *)malloc(mri->width * sizeof(float));

  for(i = 0;i < mri->depth;i++)
    {
      /* ----- write the header file ----- */
      sprintf(fname, "%s_%03d.hdr", stem, i);
      if((fp = fopen(fname, "w")) == NULL)
        {
          free(buf);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "bfloatWrite(): can't open file %s", fname));
        }
#if (BYTE_ORDER == LITTLE_ENDIAN)
      fprintf(fp, "%d %d %d %d\n", mri->height, mri->width, mri->nframes, 1);
#else
      fprintf(fp, "%d %d %d %d\n", mri->height, mri->width, mri->nframes, 0);
#endif
      fclose(fp);

      /* ----- write the data file ----- */
      sprintf(fname, "%s_%03d.bfloat", stem, i);
      if((fp = fopen(fname, "w")) == NULL)
        {
          if(dealloc) MRIfree(&mri);
          free(buf);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "bfloatWrite(): can't open file %s", fname));
        }

      for(t = 0;t < mri->nframes;t++)
        {
          for(j = 0;j < mri->height;j++)
            {
              memcpy(buf, mri->slices[t*mri->depth + i][j],
                     mri->width * sizeof(float));
#if 0
              /* this byte swapping routine does not seem to work */
              /* now uses endian flag in .hdr (above) */
              for(pos = 0; pos < mri->width * sizeof(float);
                  pos += sizeof(float))
                {
                  c = (char *) (&(buf[pos]));
                  memcpy(&(swap_buf[0]), c, 4);
                  c[0] = swap_buf[3];
                  c[1] = swap_buf[2];
                  c[2] = swap_buf[1];
                  c[3] = swap_buf[0];
                }
#endif
              fwrite(buf, sizeof(float), mri->width, fp);

            }

        }

      fclose(fp);

    }

  free(buf);

  /* ----- write the bhdr file ----- */
  sprintf(fname, "%s.bhdr", stem);
  if((fp = fopen(fname, "w")) == NULL)
    {
      if(dealloc) MRIfree(&mri);
      errno = 0;
      ErrorReturn
        (ERROR_BADFILE,
         (ERROR_BADFILE,
          "bfloatWrite(): can't open file %s", fname));
    }

  result = write_bhdr(mri, fp);

  fclose(fp);

  if(dealloc) MRIfree(&mri);
  return(result);

} /* end bfloatWrite() */

/*-------------------------------------------------------------------
  bshortWrite() - obsolete. Use bvolumeWrite.
  -------------------------------------------------------------------*/
static int bshortWrite(MRI *vol, char *fname_passed)
{

  int i, j, t;
  char fname[STRLEN];
  short *buf;
  FILE *fp;
  int result;
  MRI *subject_info = NULL;
  char subject_volume_dir[STRLEN];
  char *subjects_dir;
  char *sn;
  char analyse_fname[STRLEN], register_fname[STRLEN];
  char output_dir[STRLEN];
  char *c;
  int od_length;
  char t1_path[STRLEN];
  MATRIX *cf, *bf, *ibf, *af, *iaf, *as, *bs, *cs, *ics, *r;
  MATRIX *r1, *r2, *r3, *r4;
  float det;
  int bad_flag;
  int l;
  char stem[STRLEN];
  char *c1, *c2, *c3;
  struct stat stat_buf;
  char subject_dir[STRLEN];
  int dealloc, nslices, nframes;
  MRI *mri;
  float min,max;

  if(vol->type != MRI_SHORT){
    printf("INFO: bshortWrite: changing type\n");
    nslices = vol->depth;
    nframes = vol->nframes;
    vol->depth = nslices*nframes;
    vol->nframes = 1;
    MRIlimits(vol,&min,&max);
    printf("INFO: bshortWrite: range %g %g\n",min,max);
    mri = MRIchangeType(vol,MRI_SHORT,min,max,1);
    if(mri == NULL) {
      fprintf(stderr,"ERROR: bshortWrite: MRIchangeType\n");
      return(1);
    }
    vol->depth = nslices;
    vol->nframes = nframes;
    mri->depth = nslices;
    mri->nframes = nframes;
    dealloc = 1;
  }
  else{
    mri = vol;
    dealloc = 0;
  }

  /* ----- get the stem from the passed file name ----- */
  /*
    four options:
    1. stem_xxx.bshort
    2. stem.bshort
    3. stem_xxx
    4. stem
    other possibles:
    stem_.bshort
  */

  l = strlen(fname_passed);

  c1 = fname_passed + l - 11;
  c2 = fname_passed + l - 7;
  c3 = fname_passed + l - 4;

  strcpy(stem, fname_passed);

  if(c1 > fname_passed)
    {
      if(*c1 == '_' && strcmp(c1+4, ".bshort") == 0)
        stem[(int)(c1-fname_passed)] = '\0';
    }
  if(c2 > fname_passed)
    {
      if(strcmp(c2, ".bshort") == 0)
        stem[(int)(c2-fname_passed)] = '\0';
    }
  if(c3 > fname_passed)
    {
      if(*c3 == '_')
        stem[(int)(c3-fname_passed)] = '\0';
    }

  c = strrchr(stem, '/');
  if(c == NULL)
    output_dir[0] = '\0';
  else
    {
      od_length = (int)(c - stem);
      strncpy(output_dir, stem, od_length);
      /* -- leaving the trailing '/' on a directory is not my
         usual convention, but here it's a load easier
         if there's no directory in stem... -ch -- */
      output_dir[od_length] = '/';
      output_dir[od_length+1] = '\0';
    }

  sprintf(analyse_fname, "%s%s", output_dir, "analyse.dat");
  sprintf(register_fname, "%s%s", output_dir, "register.dat");

  buf = (short *)malloc(mri->width * mri->height * sizeof(short));

  for(i = 0;i < mri->depth;i++)
    {

      /* ----- write the header file ----- */
      sprintf(fname, "%s_%03d.hdr", stem, i);
      if((fp = fopen(fname, "w")) == NULL)
        {
          if(dealloc) MRIfree(&mri);
          free(buf);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "bshortWrite(): can't open file %s", fname));
        }
      fprintf(fp, "%d %d %d %d\n", mri->height, mri->width, mri->nframes, 0);
      fclose(fp);

      /* ----- write the data file ----- */
      sprintf(fname, "%s_%03d.bshort", stem, i);
      if((fp = fopen(fname, "w")) == NULL)
        {
          if(dealloc) MRIfree(&mri);
          free(buf);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE, "bshortWrite(): can't open file %s", fname));
        }

      for(t = 0;t < mri->nframes;t++)
        {
          for(j = 0;j < mri->height;j++)
            {

#if (BYTE_ORDER == LITTLE_ENDIAN)
              swab(mri->slices[t*mri->depth + i][j], buf,
                   mri->width * sizeof(short));
#else
              memcpy(buf, mri->slices[t*mri->depth + i][j],
                     mri->width * sizeof(short));
#endif

              fwrite(buf, sizeof(short), mri->width, fp);

            }

        }

      fclose(fp);

    }

  free(buf);

  sn = subject_name;
  if(mri->subject_name[0] != '\0')
    sn = mri->subject_name;

  if(sn != NULL)
    {
      if((subjects_dir = getenv("SUBJECTS_DIR")) == NULL)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bshortWrite(): environment variable SUBJECTS_DIR unset");
          if(dealloc) MRIfree(&mri);
        }
      else
        {

          sprintf(subject_dir, "%s/%s", subjects_dir, sn);
          if(stat(subject_dir, &stat_buf) < 0)
            {
              fprintf
                (stderr,
                 "can't stat %s; writing to bhdr instead\n", subject_dir);
            }
          else
            {
              if(!S_ISDIR(stat_buf.st_mode))
                {
                  fprintf
                    (stderr,
                     "%s is not a directory; writing to bhdr instead\n",
                     subject_dir);
                }
              else
                {
                  sprintf(subject_volume_dir, "%s/mri/T1", subject_dir);
                  subject_info = MRIreadInfo(subject_volume_dir);
                  if(subject_info == NULL)
                    {
                      sprintf(subject_volume_dir, "%s/mri/orig", subject_dir);
                      subject_info = MRIreadInfo(subject_volume_dir);
                      if(subject_info == NULL)
                        fprintf(stderr,
                                "can't read the subject's orig or T1 volumes; "
                                "writing to bhdr instead\n");
                    }
                }
            }

        }

    }

  if(subject_info != NULL)
    {
      if(subject_info->ras_good_flag == 0)
        {
          subject_info->x_r = -1.0;
          subject_info->x_a = 0.0;
          subject_info->x_s =  0.0;
          subject_info->y_r =  0.0;
          subject_info->y_a = 0.0;
          subject_info->y_s = -1.0;
          subject_info->z_r =  0.0;
          subject_info->z_a = 1.0;
          subject_info->z_s =  0.0;
          subject_info->c_r =  0.0;
          subject_info->c_a = 0.0;
          subject_info->c_s =  0.0;
        }
    }

  cf = bf = ibf = af = iaf = as = bs = cs = ics = r = NULL;
  r1 = r2 = r3 = r4 = NULL;

  /* ----- write the register.dat and analyse.dat  or bhdr files ----- */
  if(subject_info != NULL)
    {

      bad_flag = FALSE;

      if((as = MatrixAlloc(4, 4, MATRIX_REAL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bshortWrite(): error creating matrix");
          bad_flag = TRUE;
        }
      stuff_four_by_four
        (as,
         subject_info->x_r,
         subject_info->y_r,
         subject_info->z_r,
         subject_info->c_r,
         subject_info->y_r,
         subject_info->y_r,
         subject_info->y_r,
         subject_info->c_r,
         subject_info->z_r,
         subject_info->z_r,
         subject_info->z_r,
         subject_info->c_r,
         0.0,               0.0,               0.0,               1.0);

      if((af = MatrixAlloc(4, 4, MATRIX_REAL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bshortWrite(): error creating matrix");
          bad_flag = TRUE;
        }
      stuff_four_by_four(af, mri->x_r, mri->y_r, mri->z_r, mri->c_r,
                         mri->y_r, mri->y_r, mri->y_r, mri->c_r,
                         mri->z_r, mri->z_r, mri->z_r, mri->c_r,
                         0.0,      0.0,      0.0,      1.0);

      if((bs = MatrixAlloc(4, 4, MATRIX_REAL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bshortWrite(): error creating matrix");
          bad_flag = TRUE;
        }
      stuff_four_by_four(bs, 1, 0, 0, (subject_info->width  - 1) / 2.0,
                         0, 1, 0, (subject_info->height - 1) / 2.0,
                         0, 0, 1, (subject_info->depth  - 1) / 2.0,
                         0, 0, 0,                              1.0);

      if((bf = MatrixAlloc(4, 4, MATRIX_REAL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bshortWrite(): error creating matrix");
          bad_flag = TRUE;
        }
      stuff_four_by_four(bf, 1, 0, 0, (mri->width  - 1) / 2.0,
                         0, 1, 0, (mri->height - 1) / 2.0,
                         0, 0, 1, (mri->depth  - 1) / 2.0,
                         0, 0, 0,                     1.0);

      if((cs = MatrixAlloc(4, 4, MATRIX_REAL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bshortWrite(): error creating matrix");
          bad_flag = TRUE;
        }
      stuff_four_by_four
        (cs,
         -subject_info->xsize, 0, 0,
         (subject_info->width  * mri->xsize) / 2.0,
         0, 0, subject_info->zsize, -(subject_info->depth  * mri->zsize) / 2.0,
         0, -subject_info->ysize, 0,
         (subject_info->height * mri->ysize) / 2.0,
         0, 0, 0, 1);

      if((cf = MatrixAlloc(4, 4, MATRIX_REAL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bshortWrite(): error creating matrix");
          bad_flag = TRUE;
        }
      stuff_four_by_four
        (cf,
         -mri->xsize, 0,          0,  (mri->width  * mri->xsize) / 2.0,
         0,           0, mri->zsize, -(mri->depth  * mri->zsize) / 2.0,
         0, -mri->ysize,          0,  (mri->height * mri->ysize) / 2.0,
         0,           0,          0,                                 1);

      if(bad_flag)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bshortWrite(): error creating one "
             "or more matrices; aborting register.dat "
             "write and writing bhdr instead");
          MRIfree(&subject_info);
        }

    }

  if(subject_info != NULL)
    {

      bad_flag = FALSE;

      if((det = MatrixDeterminant(as)) == 0.0)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bshortWrite(): bad determinant in matrix "
             "(check structural volume)");
          bad_flag = TRUE;
        }
      if((det = MatrixDeterminant(bs)) == 0.0)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bshortWrite(): bad determinant in matrix "
             "(check structural volume)");
          bad_flag = TRUE;
        }
      if((det = MatrixDeterminant(cs)) == 0.0)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bshortWrite(): bad determinant in matrix "
             "(check structural volume)");
          bad_flag = TRUE;
        }

      if((det = MatrixDeterminant(af)) == 0.0)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bshortWrite(): bad determinant in matrix "
             "(check functional volume)");
          bad_flag = TRUE;
        }
      if((det = MatrixDeterminant(bf)) == 0.0)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bshortWrite(): bad determinant in matrix "
             "(check functional volume)");
          bad_flag = TRUE;
        }
      if((det = MatrixDeterminant(cf)) == 0.0)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bshortWrite(): bad determinant in matrix "
             "(check functional volume)");
          bad_flag = TRUE;
        }

      if(bad_flag)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bshortWrite(): one or more zero "
             "determinants; aborting register.dat write and "
             "writing bhdr instead");
          MRIfree(&subject_info);
        }

    }

  if(subject_info != NULL)
    {

      bad_flag = FALSE;

      if((iaf = MatrixInverse(af, NULL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bshortWrite(): error inverting matrix");
          bad_flag = TRUE;
        }
      if((ibf = MatrixInverse(bf, NULL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bshortWrite(): error inverting matrix");
          bad_flag = TRUE;
        }
      if((ics = MatrixInverse(cs, NULL)) == NULL)
        {
          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "bshortWrite(): error inverting matrix");
          bad_flag = TRUE;
        }

      if(bad_flag)
        {
          errno = 0;
          ErrorPrintf
            (ERROR_BADPARM,
             "bshortWrite(): one or more zero "
             "determinants; aborting register.dat write and "
             "writing bhdr instead");
          MRIfree(&subject_info);
        }
    }

  bad_flag = FALSE;

  if(subject_info != NULL)
    {

      if((r1 = MatrixMultiply(bs, ics, NULL)) == NULL)
        {
          bad_flag = TRUE;
          MRIfree(&subject_info);
        }

    }

  if(subject_info != NULL)
    {

      if((r2 = MatrixMultiply(as, r1, NULL)) == NULL)
        {
          bad_flag = TRUE;
          MRIfree(&subject_info);
        }

    }

  if(subject_info != NULL)
    {

      if((r3 = MatrixMultiply(iaf, r2, NULL)) == NULL)
        {
          bad_flag = TRUE;
          MRIfree(&subject_info);
        }

    }

  if(subject_info != NULL)
    {

      if((r4 = MatrixMultiply(ibf, r3, NULL)) == NULL)
        {
          bad_flag = TRUE;
          MRIfree(&subject_info);
        }

    }

  if(subject_info != NULL)
    {

      if((r = MatrixMultiply(cf, r4, NULL)) == NULL)
        {
          bad_flag = TRUE;
          MRIfree(&subject_info);
        }

    }

  if(bad_flag)
    {
      errno = 0;
      ErrorPrintf
        (ERROR_BADPARM,
         "bshortWrite(): error during matrix "
         "multiplications; aborting register.dat write and "
         "writing bhdr instead");
    }

  if( as != NULL)  MatrixFree( &as);
  if( bs != NULL)  MatrixFree( &bs);
  if( cs != NULL)  MatrixFree( &cs);
  if( af != NULL)  MatrixFree( &af);
  if( bf != NULL)  MatrixFree( &bf);
  if( cf != NULL)  MatrixFree( &cf);
  if(iaf != NULL)  MatrixFree(&iaf);
  if(ibf != NULL)  MatrixFree(&ibf);
  if(ics != NULL)  MatrixFree(&ics);
  if( r1 != NULL)  MatrixFree( &r1);
  if( r2 != NULL)  MatrixFree( &r2);
  if( r3 != NULL)  MatrixFree( &r3);
  if( r4 != NULL)  MatrixFree( &r4);

  if(subject_info != NULL)
    {

      if(mri->path_to_t1[0] == '\0')
        sprintf(t1_path, ".");
      else
        strcpy(t1_path, mri->path_to_t1);

      if(FileExists(analyse_fname))
        fprintf(stderr, "warning: overwriting file %s\n", analyse_fname);

      if((fp = fopen(analyse_fname, "w")) == NULL)
        {
          MRIfree(&subject_info);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "bshortWrite(): couldn't open file %s for writing",
              analyse_fname));
        }

      fprintf(fp, "%s\n", t1_path);
      fprintf(fp, "%s_%%03d.bshort\n", stem);
      fprintf(fp, "%d %d\n", mri->depth, mri->nframes);
      fprintf(fp, "%d %d\n", mri->width, mri->height);

      fclose(fp);

      if(FileExists(analyse_fname))
        fprintf(stderr, "warning: overwriting file %s\n", register_fname);

      if((fp = fopen(register_fname, "w")) == NULL)
        {
          MRIfree(&subject_info);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE,
              "bshortWrite(): couldn't open file %s for writing",
              register_fname));
        }

      fprintf(fp, "%s\n", sn);
      fprintf(fp, "%g\n", mri->xsize);
      fprintf(fp, "%g\n", mri->zsize);
      fprintf(fp, "%g\n", 1.0);
      fprintf(fp, "%g %g %g %g\n",
              *MATRIX_RELT(r, 1, 1),
              *MATRIX_RELT(r, 1, 2),
              *MATRIX_RELT(r, 1, 3),
              *MATRIX_RELT(r, 1, 4));
      fprintf(fp, "%g %g %g %g\n",
              *MATRIX_RELT(r, 2, 1),
              *MATRIX_RELT(r, 2, 2),
              *MATRIX_RELT(r, 2, 3),
              *MATRIX_RELT(r, 2, 4));
      fprintf(fp, "%g %g %g %g\n",
              *MATRIX_RELT(r, 3, 1),
              *MATRIX_RELT(r, 3, 2),
              *MATRIX_RELT(r, 3, 3),
              *MATRIX_RELT(r, 3, 4));
      fprintf(fp, "%g %g %g %g\n",
              *MATRIX_RELT(r, 4, 1),
              *MATRIX_RELT(r, 4, 2),
              *MATRIX_RELT(r, 4, 3),
              *MATRIX_RELT(r, 4, 4));

      fclose(fp);

      MatrixFree(&r);

    }

  if(subject_info == NULL)
    {
      sprintf(fname, "%s.bhdr", stem);
      if((fp = fopen(fname, "w")) == NULL)
        {
          if(dealloc) MRIfree(&mri);
          errno = 0;
          ErrorReturn
            (ERROR_BADFILE,
             (ERROR_BADFILE, "bshortWrite(): can't open file %s", fname));
        }

      result = write_bhdr(mri, fp);

      fclose(fp);

      if(result != NO_ERROR)
        return(result);

    }
  else
    MRIfree(&subject_info);

  if(dealloc) MRIfree(&mri);

  return(NO_ERROR);

} /* end bshortWrite() */

/*-------------------------------------------------------------------
  bshortRead() - obsolete. Use bvolumeRead.
  -------------------------------------------------------------------*/
static MRI *bshortRead(char *fname_passed, int read_volume)
{

  MRI *mri;
  FILE *fp;
  char fname[STRLEN];
  char directory[STRLEN];
  char stem[STRLEN];
  int swap_bytes_flag;
  int slice, frame, row, k;
  int nread;

  mri = get_b_info(fname_passed, read_volume, directory, stem, MRI_SHORT);
  if(mri == NULL)
    return(NULL);

  if(read_volume)
    {

      sprintf(fname, "%s/%s_%03d.hdr", directory, stem, 0);
      if((fp = fopen(fname, "r")) == NULL){
        fprintf
          (stderr, "can't open file %s; assuming big-endian bvolume\n", fname);
        swap_bytes_flag = 0;
      }
      else
        {
          fscanf(fp, "%*d %*d %*d %d", &swap_bytes_flag);
#if (BYTE_ORDER == LITTLE_ENDIAN)
          swap_bytes_flag = !swap_bytes_flag;
#endif
          fclose(fp);
        }

      for(slice = 0;slice < mri->depth; slice++){

        sprintf(fname, "%s/%s_%03d.bshort", directory, stem, slice);
        if((fp = fopen(fname, "r")) == NULL){
          MRIfree(&mri);
          errno = 0;
          ErrorReturn(NULL, (ERROR_BADFILE,
                             "bshortRead(): error opening file %s", fname));
        }

        for(frame = 0; frame < mri->nframes; frame ++){
          k = slice + mri->depth*frame;
          for(row = 0;row < mri->height; row++){

            /* read in a column */
            nread = fread(mri->slices[k][row], sizeof(short), mri->width, fp);
            if( nread != mri->width){
              fclose(fp);
              MRIfree(&mri);
              errno = 0;
              ErrorReturn
                (NULL,
                 (ERROR_BADFILE,
                  "bshortRead(): error reading from file %s", fname));
            }

            if(swap_bytes_flag)
              swab(mri->slices[k][row], mri->slices[k][row],
                   mri->width * sizeof(short));

          } /* row loop */
        } /* frame loop */
        fclose(fp);
      }
    }

  return(mri);

} /* end bshortRead() */


/*-------------------------------------------------------------------
  bfloatRead() - obsolete. Use bvolumeRead.
  -------------------------------------------------------------------*/
static MRI *bfloatRead(char *fname_passed, int read_volume)
{

  MRI *mri;
  FILE *fp;
  char fname[STRLEN];
  char directory[STRLEN];
  char stem[STRLEN];
  int swap_bytes_flag;
  int i, j, k;

  mri = get_b_info(fname_passed, read_volume, directory, stem, MRI_FLOAT);
  if(mri == NULL)
    return(NULL);

  if(read_volume)
    {

      sprintf(fname, "%s/%s_%03d.hdr", directory, stem, 0);
      if((fp = fopen(fname, "r")) == NULL)
        {
          fprintf
            (stderr,
             "INFO: Can't open file %s; assuming big-endian bshorts\n",
             fname);
          swap_bytes_flag = 0;
        }
      else
        {
          fscanf(fp, "%*d %*d %*d %d", &swap_bytes_flag);
#if (BYTE_ORDER == LITTLE_ENDIAN)
          swap_bytes_flag = !swap_bytes_flag;
#endif
          fclose(fp);
        }

      printf("swap = %d\n",swap_bytes_flag);

      for(i = 0;i < mri->depth;i++)
        {

          sprintf(fname, "%s/%s_%03d.bfloat", directory, stem, i);

          if((fp = fopen(fname, "r")) == NULL)
            {
              MRIfree(&mri);
              errno = 0;
              ErrorReturn
                (NULL,
                 (ERROR_BADFILE,
                  "bfloatRead(): error opening file %s", fname));
            }

          for(j = 0;j < mri->height;j++)
            {
              if(fread(mri->slices[i][j], sizeof(float), mri->width, fp) !=
                 mri->width)
                {
                  fclose(fp);
                  MRIfree(&mri);
                  errno = 0;
                  ErrorReturn
                    (NULL,
                     (ERROR_BADFILE,
                      "bfloatRead(): error reading from file %s", fname));
                }
              if(swap_bytes_flag)
                {
                  for(k = 0;k < mri->depth;k++)
                    mri->slices[i][j][k] = swapFloat(mri->slices[i][j][k]);
                }
            }

          fclose(fp);

        }

    }

  return(mri);

} /* end bfloatRead() */


/*-------------------------------------------------------------------------*/
static MRI *analyzeReadOld(char *fname, int read_volume)
{

  MRI *mri = NULL;
  FILE *fp;
  char hdr_fname[STRLEN];
  char mat_fname[STRLEN];
  char *c;
  dsr hdr;
  int dtype;
  int flip_flag = 0;
  int i, j, k;
  float dx, dy, dz;
  int nread;
  unsigned char *buf;
  int bytes_per_voxel;
  int bufsize;
  MATRIX *m;
  MATRIX *center_index_mat;
  MATRIX *center_ras_mat;
  float xfov, yfov, zfov;

  c = strrchr(fname, '.');
  if(c == NULL)
    {
      errno = 0;
      ErrorReturn
        (NULL, (ERROR_BADPARM, "analyzeRead(): bad file name %s", fname));
    }
  if(strcmp(c, ".img") != 0)
    {
      errno = 0;
      ErrorReturn
        (NULL, (ERROR_BADPARM, "analyzeRead(): bad file name %s", fname));
    }

  strcpy(hdr_fname, fname);
  sprintf(hdr_fname + (c - fname), ".hdr");

  strcpy(mat_fname, fname);
  sprintf(mat_fname + (c - fname), ".mat");

  /* Open the header file */
  if((fp = fopen(hdr_fname, "r")) == NULL)
    {
      errno = 0;
      ErrorReturn(NULL, (ERROR_BADFILE, "read_analyze_header(): "
                         "error opening file %s", fname));
    }

  /* Read the header file */
  fread(&hdr, sizeof(hdr), 1, fp);
  fclose(fp);

  if(hdr.hk.sizeof_hdr != sizeof(hdr))
    {
      flip_flag = 1;
      swap_analyze_header(&hdr);
    }

  if(hdr.dime.datatype == DT_UNSIGNED_CHAR)
    {
      dtype = MRI_UCHAR;
      bytes_per_voxel = 1;
    }
  else if(hdr.dime.datatype == DT_SIGNED_SHORT)
    {
      dtype = MRI_SHORT;
      bytes_per_voxel = 2;
    }
  else if(hdr.dime.datatype == DT_SIGNED_INT)
    {
      dtype = MRI_INT;
      bytes_per_voxel = 4;
    }
  else if(hdr.dime.datatype == DT_FLOAT)
    {
      dtype = MRI_FLOAT;
      bytes_per_voxel = 4;
    }
  else if(hdr.dime.datatype == DT_DOUBLE)
    {
      dtype = MRI_FLOAT;
      bytes_per_voxel = 8;
    }
  else
    {
      errno = 0;
      ErrorReturn(NULL, (ERROR_UNSUPPORTED, "analyzeRead: "
                         "unsupported data type %d", hdr.dime.datatype));
    }

  /* ----- allocate the mri structure ----- */
  if(read_volume)
    mri = MRIalloc(hdr.dime.dim[1], hdr.dime.dim[2], hdr.dime.dim[3], dtype);
  else
    mri = MRIalloc(hdr.dime.dim[1], hdr.dime.dim[2], hdr.dime.dim[3], dtype);

  mri->xsize = hdr.dime.pixdim[1];
  mri->ysize = hdr.dime.pixdim[2];
  mri->zsize = hdr.dime.pixdim[3];

  mri->thick = mri->zsize;
  mri->ps = mri->xsize;
  mri->xend = mri->width * mri->xsize / 2.0;   mri->xstart = -mri->xend;
  mri->yend = mri->height * mri->ysize / 2.0;  mri->ystart = -mri->yend;
  mri->zend = mri->depth * mri->zsize / 2.0;   mri->zstart = -mri->zend;
  xfov = mri->xend - mri->xstart;
  yfov = mri->yend - mri->ystart;
  zfov = mri->zend - mri->zstart;

  mri->fov =
    (xfov > yfov ? (xfov > zfov ? xfov : zfov) : (yfov > zfov ? yfov : zfov));

  /* --- default (no .mat file) --- */
  mri->x_r =  1.0;  mri->x_a = 0.0;  mri->x_s = 0.0;
  mri->y_r =  0.0;  mri->y_a = 1.0;  mri->y_s = 0.0;
  mri->z_r =  0.0;  mri->z_a = 0.0;  mri->z_s = 1.0;

  /* --- originator gives the voxel index of (r, a, s) = (0, 0, 0) --- */
  dx = (mri->width  - 1.0) / 2. - (float)(((short *)hdr.hist.originator)[0]);
  dy = (mri->height - 1.0) / 2. - (float)(((short *)hdr.hist.originator)[1]);
  dz = (mri->depth  - 1.0) / 2. - (float)(((short *)hdr.hist.originator)[2]);

  mri->c_r = (dx * mri->x_r) + (dy * mri->y_r) + (dz * mri->z_r);
  mri->c_a = (dx * mri->x_a) + (dy * mri->y_a) + (dz * mri->z_a);
  mri->c_s = (dx * mri->x_s) + (dy * mri->y_s) + (dz * mri->z_s);

  mri->ras_good_flag = 1;

  strcpy(mri->fname, fname);

  if(read_volume)
    {

      if((fp = fopen(fname, "r")) == NULL)
        {
          MRIfree(&mri);
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADFILE, "analyzeRead: error opening file %s", fname));
        }

      fseek(fp, (int)(hdr.dime.vox_offset), SEEK_SET);

      bufsize = mri->width * bytes_per_voxel;
      buf = (unsigned char *)malloc(bufsize);

      for(k = 0;k < mri->depth;k++)
        {
          for(j = 0;j < mri->height;j++)
            {

              nread = fread(buf, bytes_per_voxel, mri->width, fp);
              if(nread != mri->width)
                {
                  free(buf);
                  fclose(fp);
                  errno = 0;
                  ErrorReturn
                    (NULL,
                     (ERROR_BADFILE,
                      "analyzeRead: error reading from file %s\n", fname));
                }

              if(flip_flag)
                nflip(buf, bytes_per_voxel, mri->width);


              for(i = 0;i < mri->width;i++)
                {
                  if(hdr.dime.datatype == DT_UNSIGNED_CHAR)
                    MRIvox(mri, i, j, k) = buf[i];
                  if(hdr.dime.datatype == DT_SIGNED_SHORT)
                    MRISvox(mri, i, j, k) = ((short *)buf)[i];
                  if(hdr.dime.datatype == DT_SIGNED_INT)
                    MRIIvox(mri, i, j, k) = ((int *)buf)[i];
                  if(hdr.dime.datatype == DT_FLOAT)
                    MRIFvox(mri, i, j, k) = ((float *)buf)[i];
                  if(hdr.dime.datatype == DT_DOUBLE)
                    MRIFvox(mri, i, j, k) = (float)(((double *)buf)[i]);
                }

            }
        }

      free(buf);
      fclose(fp);

    }

  /* ----- read mat file ----- */
  if(FileExists(mat_fname))
    {

      m = MatlabRead(mat_fname);

      if(m == NULL)
        {
          MRIfree(&mri);
          return(NULL);
        }

      if(m->rows != 4 || m->cols != 4)
        {
          MRIfree(&mri);
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADFILE,
              "analyzeRead(): not a 4 by 4 matrix in file %s", mat_fname));
        }

      /* swap y and z here ?*/
      mri->x_r = *MATRIX_RELT(m, 1, 1);
      mri->y_r = *MATRIX_RELT(m, 1, 2);
      mri->z_r = *MATRIX_RELT(m, 1, 3);
      mri->x_a = *MATRIX_RELT(m, 2, 1);
      mri->y_a = *MATRIX_RELT(m, 2, 2);
      mri->z_a = *MATRIX_RELT(m, 2, 3);
      mri->x_s = *MATRIX_RELT(m, 3, 1);
      mri->y_s = *MATRIX_RELT(m, 3, 2);
      mri->z_s = *MATRIX_RELT(m, 3, 3);

      mri->xsize =
        sqrt(mri->x_r * mri->x_r + mri->x_a * mri->x_a + mri->x_s * mri->x_s);
      mri->ysize =
        sqrt(mri->y_r * mri->y_r + mri->y_a * mri->y_a + mri->y_s * mri->y_s);
      mri->zsize =
        sqrt(mri->z_r * mri->z_r + mri->z_a * mri->z_a + mri->z_s * mri->z_s);

      mri->x_r = mri->x_r / mri->xsize;
      mri->x_a = mri->x_a / mri->xsize;
      mri->x_s = mri->x_s / mri->xsize;
      mri->y_r = mri->y_r / mri->ysize;
      mri->y_a = mri->y_a / mri->ysize;
      mri->y_s = mri->y_s / mri->ysize;
      mri->z_r = mri->z_r / mri->zsize;
      mri->z_a = mri->z_a / mri->zsize;
      mri->z_s = mri->z_s / mri->zsize;

      center_index_mat = MatrixAlloc(4, 1, MATRIX_REAL);

      /* Is this right?? */
      /* --- matlab matrices start at 1, so the middle index is
         [(width, height, depth)+(1, 1, 1)]/2, (not -) --- */
      *MATRIX_RELT(center_index_mat, 1, 1) = (mri->width + 1.0) / 2.0;
      *MATRIX_RELT(center_index_mat, 2, 1) = (mri->height + 1.0) / 2.0;
      *MATRIX_RELT(center_index_mat, 3, 1) = (mri->depth + 1.0) / 2.0;
      *MATRIX_RELT(center_index_mat, 4, 1) = 1.0;

      center_ras_mat = MatrixMultiply(m, center_index_mat, NULL);
      if(center_ras_mat == NULL)
        {

          errno = 0;
          ErrorPrintf(ERROR_BADPARM, "multiplying: m * cim:\n");
          ErrorPrintf(ERROR_BADPARM, "m = \n");
          MatrixPrint(stderr, m);
          ErrorPrintf(ERROR_BADPARM, "cim = \n");
          MatrixPrint(stderr, center_index_mat);

          MatrixFree(&m);
          MatrixFree(&center_index_mat);
          MatrixFree(&center_ras_mat);
          MRIfree(&mri);
          errno = 0;
          ErrorReturn
            (NULL,
             (ERROR_BADPARM, "analyzeRead(): error in matrix multiplication"));
        }

      mri->c_r = *MATRIX_RELT(center_ras_mat, 1, 1);
      mri->c_a = *MATRIX_RELT(center_ras_mat, 2, 1);
      mri->c_s = *MATRIX_RELT(center_ras_mat, 3, 1);

      MatrixFree(&m);
      MatrixFree(&center_index_mat);
      MatrixFree(&center_ras_mat);

    }

  return(mri);

} /* end analyzeRead() */

static void nflip(unsigned char *buf, int b, int n)
{
  int i, j;
  unsigned char *copy;

  copy = (unsigned char *)malloc(b);
  for(i = 0;i < n;i++)
    {
      memcpy(copy, &buf[i*b], b);
      for(j = 0;j < b;j++)
        buf[i*b+j] = copy[b-j-1];
    }
  free(copy);

} /* end nflip() */

#endif
#include "gca.h"
static MRI *
readGCA(char *fname)
{
  GCA *gca ;
  MRI *mri ;

  gca = GCAread(fname) ;
  if (!gca)
    return(NULL) ;
  mri = GCAbuildMostLikelyVolume(gca, NULL) ;
  GCAfree(&gca) ;
  return(mri) ;
}

MRI *
MRIremoveNaNs(MRI *mri_src, MRI *mri_dst)
{
  int x, y, z, nans=0 ;
  float val ;

  if (mri_dst != mri_src)
    mri_dst = MRIcopy(mri_src, mri_dst) ;

  for (x = 0 ; x < mri_dst->width ; x++)
    {
      for (y = 0 ; y < mri_dst->height ; y++)
        {
          for (z = 0 ; z < mri_dst->depth ; z++)
            {
              val = MRIgetVoxVal(mri_dst, x, y, z, 0) ;
              if (!finite(val))
                {
                  nans++ ;
                  MRIsetVoxVal(mri_dst, x, y, z, 0, 0) ;
                }
            }
        }
    }
  if (nans > 0)
    ErrorPrintf
      (ERROR_BADPARM,
       "WARNING: %d NaNs found in volume %s...\n", nans, mri_src->fname) ;
  return(mri_dst) ;
}

int
MRIaddCommandLine(MRI *mri, char *cmdline)
{
  int i ;
  if (mri->ncmds >= MAX_CMDS)
    ErrorExit
      (ERROR_NOMEMORY,
       "MRIaddCommandLine: can't add cmd %s (%d)", cmdline, mri->ncmds) ;

  i = mri->ncmds++ ;
  mri->cmdlines[i] = (char *)calloc(strlen(cmdline)+1, sizeof(char)) ;
  strcpy(mri->cmdlines[i], cmdline) ;
  return(NO_ERROR) ;
}


static MRI *mriNrrdRead(char *fname, int read_volume)
{
  Nrrd *nrrd = nrrdNew();
  MRI *mri = NULL;
  int mriDataType = MRI_UCHAR; //so compiler won't complain about initialization
  size_t nFrames;

  unsigned int rangeAxisNum, rangeAxisIdx[NRRD_DIM_MAX];

  int errorType = NO_ERROR;
  char errorString[50];

  //just give an error until read function is complete and tested
  ErrorReturn(NULL, (ERROR_UNSUPPORTED, "mriNrrdRead(): Nrrd input not yet supported"));

  //from errno.h?
  errno = 0; //is this neccesary because of error.c:ErrorPrintf's use of errno?

  if (nrrdLoad(nrrd, fname, NULL) != 0)
    {
      char *err = biffGetDone(NRRD);
      ErrorPrintf(ERROR_BADFILE, "mriNrrdRead(): error opening file %s:\n%s", fname, err);
      free(err);
      return NULL;
    }

  if ((nrrd->dim != 3) && (nrrd->dim != 4)) {
    ErrorPrintf(ERROR_UNSUPPORTED, "mriNrrdRead(): %hd dimensions in %s; unspported",
		nrrd->dim, fname);
    nrrdNuke(nrrd);
    return NULL;
  }

  //errorString is only length = 50. careful with sprintf 
  switch (nrrd->type) {

    //Does nrrdLoad() generate an error for nrrdTypeUnkown and nrrdTypeDefault?
  case nrrdTypeUnknown: //fall through to next
    //case nrrdTypeDefault:
    errorType = ERROR_BADFILE;
    sprintf(errorString, "unset/unknown");
    break;

  //types we don't support. should/can we convert?
  case nrrdTypeChar:
    errorType = ERROR_UNSUPPORTED;
    sprintf(errorString, "signed char");
    break;
  case nrrdTypeUShort:
    errorType = ERROR_UNSUPPORTED;
    sprintf(errorString, "unsigned short");
    break;
  case nrrdTypeUInt: 
    errorType = ERROR_UNSUPPORTED;
    sprintf(errorString, "unsigned int");
    break;
  case nrrdTypeLLong:
    errorType = ERROR_UNSUPPORTED;
    sprintf(errorString, "long long int");
    break;
  case nrrdTypeULLong: 
    errorType = ERROR_UNSUPPORTED;
    sprintf(errorString, "unsigned long long int");
    break;
  case nrrdTypeDouble:
    errorType = ERROR_UNSUPPORTED;
    sprintf(errorString, "double");
    break;
  case nrrdTypeBlock:
    errorType = ERROR_UNSUPPORTED;
    sprintf(errorString, "user defined block");
    break;

    //supported types. is MRI_INT 32 bits? is MRI_LONG 32 or 64?
    //what size is MRI_FLOAT?
  case nrrdTypeUChar:
    mriDataType = MRI_UCHAR;
    break;
  case nrrdTypeShort:
    mriDataType = MRI_SHORT;
    break;
  case nrrdTypeInt:
    mriDataType = MRI_INT;
    break;
  case nrrdTypeFloat:
    mriDataType = MRI_FLOAT;
    break;
  }

  if (errorType != NO_ERROR) {
    nrrdNuke(nrrd);
    ErrorPrintf(errorType, "mriNrrdRead(): unsupported type: %s", errorString);
    return NULL;
  }

  rangeAxisNum = nrrdRangeAxesGet(nrrd, rangeAxisIdx);

  if (rangeAxisNum > 1) {
    nrrdNuke(nrrd);
    ErrorPrintf(ERROR_UNSUPPORTED,
		"mriNrrdRead(): handling more than one non-scalar axis not currently supported");
    return NULL;
  }

  //if the range (dependent variable, i.e. time point, diffusion dir,
  //anything non-spatial) is not on the 4th axis, then permute
  //so that it is
  if ((rangeAxisNum == 1) && (rangeAxisIdx[0] != 3)) {
    Nrrd *ntmp = nrrdNew();
    unsigned int axmap[NRRD_DIM_MAX];
    int axis;
    //axmap[i] = j means: axis i in the output will be the input's axis j

    axmap[nrrd->dim - 1] = rangeAxisIdx[0];
    for (axis = 0; axis < nrrd->dim - 1; axis++) {
      axmap[axis] = axis + (axis >= rangeAxisIdx[0]);
    }

    // The memory size of the input and output of nrrdAxesPermute is
    // the same; the existing nrrd->data is re-used.
    if (nrrdCopy(ntmp, nrrd) || nrrdAxesPermute(nrrd, ntmp, axmap)) {
      char *err =  biffGetDone(NRRD);
      //doesn't seem to be an appropriate error code for this case
      ErrorPrintf(ERROR_BADFILE,
		  "mriNrrdRead(): error permuting independent axis in %s: \n%s",
		  fname, err);
      nrrdNuke(ntmp);
      free(err);
      return NULL;
    }
    nrrdNuke(ntmp);
  }

    
  //data in nrrd have been permuted so first 3 axes are spatial
  //and next if present is non-spatial
  if (nrrd->dim == 4) nFrames = nrrd->axis[3].size;
  else nFrames = 1;
  mri = MRIallocSequence(nrrd->axis[0].size, nrrd->axis[1].size,
			 nrrd->axis[2].size, mriDataType, nFrames);

  if (mri == NULL) {
    nrrdNuke(nrrd);
    //error message
    return NULL;
  }

  //error if nrrd->space_units is present and != "mm"?

  mri->xsize = nrrd->axis[0].spacing;
  mri->ysize = nrrd->axis[1].spacing;
  mri->zsize = nrrd->axis[2].spacing;

/*   if (nrrdTypeBlock == nrrd->type) */
/*     { */
/*       ErrorReturn */
/* 	(NULL, (ERROR_BADFILE, "nrrdRead(): cannot currently handle nrrdTypeBlock")); */
/*     } */

/*   if (nio->endian == airEndianLittle) */
/*     { */
/* #if (BYTE_ORDER != LITTLE_ENDIAN) */
/*       swap_bytes_flag = 1; */
/* #endif */
/*     } */
/*   else if (nio->endian == airEndianBig) */
/*     { */
/* #if (BYTE_ORDER == LITTLE_ENDIAN) */
/*       swap_bytes_flag = 1; */
/* #endif */
/*     } */

/*   if (nio->encoding == nrrdEncodingAscii) */
/*     { */
/*       mode = "rt"; */
/*     } */
/*   else */
/*     { */
/*       mode = "rb"; */
/*     } */

  return mri;
}

static int mriNrrdWrite(MRI *mri, char *fname)
{
  //just give an error until write function is complete and tested
  ErrorReturn(ERROR_UNSUPPORTED, (ERROR_UNSUPPORTED, "mriNrrdWrite(): Nrrd output not yet supported"));
}
/*------------------------------------------------------------------
  niiPrintHdr() - this dumps (most of) the nifti header to the given
  stream.
  ------------------------------------------------------------------*/
static int niiPrintHdr(FILE *fp, struct nifti_1_header *hdr)
{
  int n;
  fprintf(fp,"sizeof_hdr %d \n",hdr->sizeof_hdr);
  fprintf(fp,"data_type  %s \n",hdr->data_type);
  fprintf(fp,"db_name    %s \n",hdr->db_name);
  fprintf(fp,"extents    %d \n",hdr->extents);
  fprintf(fp,"session_error %d \n",hdr->session_error);
  fprintf(fp,"regular    %c \n",hdr->regular);
  fprintf(fp,"dim_info   %c \n",hdr->dim_info);
  for(n=0; n<8; n++)  fprintf(fp,"dim[%d] %d\n",n,hdr->dim[n]);
  fprintf(fp,"intent_p1  %f \n",hdr->intent_p1);
  fprintf(fp,"intent_p2  %f \n",hdr->intent_p2);
  fprintf(fp,"intent_p3  %f \n",hdr->intent_p3);
  fprintf(fp,"intent_code  %d \n",hdr->intent_code);
  fprintf(fp,"datatype      %d \n",hdr->datatype);
  fprintf(fp,"bitpix        %d \n",hdr->bitpix);
  fprintf(fp,"slice_start   %d \n",hdr->slice_start);
  for(n=0; n<8; n++)  fprintf(fp,"pixdim[%d] %f\n",n,hdr->pixdim[n]);
  fprintf(fp,"vox_offset    %f\n",hdr->vox_offset);
  fprintf(fp,"scl_slope     %f\n",hdr->scl_slope);
  fprintf(fp,"scl_inter     %f\n",hdr->scl_inter);
  fprintf(fp,"slice_end     %d\n",hdr->slice_end);
  fprintf(fp,"slice_code    %c\n",hdr->slice_code);
  fprintf(fp,"xyzt_units    %c\n",hdr->xyzt_units);
  fprintf(fp,"cal_max       %f\n",hdr->cal_max);
  fprintf(fp,"cal_min       %f\n",hdr->cal_min);
  fprintf(fp,"slice_duration %f\n",hdr->slice_duration);
  fprintf(fp,"toffset        %f \n",hdr->toffset);
  fprintf(fp,"glmax          %d \n",hdr->glmax);
  fprintf(fp,"glmin          %d \n",hdr->glmin);
  fprintf(fp,"descrip        %s \n",hdr->descrip);
  fprintf(fp,"aux_file       %s \n",hdr->aux_file);
  fprintf(fp,"qform_code     %d \n",hdr->qform_code);
  fprintf(fp,"sform_code     %d \n",hdr->sform_code);
  // There's more, but I ran out of steam ...
  //fprintf(fp,"          %d \n",hdr->);
  return(0);
}
