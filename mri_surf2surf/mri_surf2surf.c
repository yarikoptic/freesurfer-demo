/*----------------------------------------------------------
  Name: mri_surf2surf.c
  $Id: mri_surf2surf.c,v 1.38.2.1 2006/11/29 20:28:13 nicks Exp $
  Author: Douglas Greve
  Purpose: Resamples data from one surface onto another. If
  both the source and target subjects are the same, this is
  just a format conversion. The source or target subject may
  be ico.  Can handle data with multiple frames.
  -----------------------------------------------------------*/

/*
BEGINHELP

This program will resample one surface onto another. The source and 
target subjects can be any subject in $SUBJECTS_DIR and/or the  
icosahedron (ico). The source and target file formats can be anything 
supported by mri_convert. The source format can also be a curvature 
file or a paint (.w) file. The user also has the option of smoothing 
on the surface. 

OPTIONS

  --srcsubject subjectname

    Name of source subject as found in $SUBJECTS_DIR or ico for icosahedron.
    The input data must have been sampled onto this subject's surface (eg, 
    using mri_vol2surf)

  --sval sourcefile

    Name of file where the data on the source surface is located.

  --sval-xyz     surfname
  --sval-tal-xyz surfname
  --sval-area    surfname
  --sval-nxyz    surfname

    Use measures from the input surface as the source (instead of specifying
    a source file explicitly with --sval). --sval-xyz extracts the x, y, and
    z of each vertex. --sval-tal-xyz is the same as --sval-xyz, but applies
    the talairach transform from srcsubject/mri/transforms/talairach.xfm.
    --sval-area extracts the vertex area. --sval-nxyz extracts the surface
    normals at each vertex. See also --tval-xyz.

  --sfmt typestring

    Format type string. Can be either curv (for FreeSurfer curvature file), 
    paint or w (for FreeSurfer paint files), or anything accepted by 
    mri_convert. If no type string  is given, then the type is determined 
    from the sourcefile (if possible). If curv is used, then the curvature
    file will be looked for in $SUBJECTS_DIR/srcsubject/surf/hemi.sourcefile.

  --srcicoorder order

    Icosahedron order of the source. Normally, this can be detected based
    on the number of verticies, but this will fail with a .w file as input.
    This is only needed when the source is a .w file.

  --trgsubject subjectname

    Name of target subject as found in $SUBJECTS_DIR or ico for icosahedron.

  --trgicoorder order

    Icosahedron order number. This specifies the size of the
    icosahedron according to the following table: 
              Order  Number of Vertices
                0              12 
                1              42 
                2             162 
                3             642 
                4            2562 
                5           10242 
                6           40962 
                7          163842 
    In general, it is best to use the largest size available.

  --tval targetfile

    Name of file where the data on the target surface will be stored.
    BUG ALERT: for trg_type w or paint, use the full path.

  --tval-xyz

    Flag to indicate that the source surface xyz as a binary surface file
    given by the target file. This requires that --sval-xyz or --sval-tal-xyz.
    This is a good way to map the surface of one subject to an average
    (talairach) subject. Note: it will save targetfile as
    trgsubject/surf/targetfile unless targetfile has a path.

  --tfmt typestring

    Format type string. Can be paint or w (for FreeSurfer paint files) or anything
    accepted by mri_convert. NOTE: output cannot be stored in curv format
    If no type string  is given, then the type is determined from the sourcefile
    (if possible). If using paint or w, see also --frame.

  --hemi hemifield (lh or rh)

  --surfreg registration_surface

    If the source and target subjects are not the same, this surface is used 
    to register the two surfaces. sphere.reg is used as the default. Don't change
    this unless you know what you are doing.

  --mapmethod methodname

    Method used to map from the vertices in one subject to those of another.
    Legal values are: nnfr (neighest-neighbor, forward and reverse) and nnf
    (neighest-neighbor, forward only). Default is nnfr. The mapping is done
    in the following way. For each vertex on the target surface, the closest
    vertex in the source surface is found, based on the distance in the 
    registration space (this is the forward map). If nnf is chosen, then the
    the value at the target vertex is set to that of the closest source vertex.
    This, however, can leave some source vertices unrepresented in target (ie,
    'holes'). If nnfr is chosen, then each hole is assigned to the closest
    target vertex. If a target vertex has multiple source vertices, then the
    source values are averaged together. It does not seem to make much difference.

  --fwhm-src fwhmsrc
  --fwhm-trg fwhmtrg (can also use --fwhm)

    Smooth the source or target with a gaussian with the given fwhm (mm). This is
    actually an approximation done using iterative nearest neighbor smoothing.
    The number of iterations is computed based on the white surface. This
    method is similar to heat kernel smoothing. This will give the same
    results as --nsmooth-{in,out}, but automatically computes the the 
    number of iterations based on the desired fwhm.

  --nsmooth-in  niterations
  --nsmooth-out niterations  [note: same as --smooth]

    Number of smoothing iterations. Each iteration consists of averaging each
    vertex with its neighbors. When only smoothing is desired, just set the 
    the source and target subjects to the same subject. --smooth-in smooths
    the input surface values prior to any resampling. --smooth-out smooths
    after any resampling. See also --fwhm-src and --fwhm-trg.

  --frame framenumber

    When using paint/w output format, this specifies which frame to output. This
    format can store only one frame. The frame number is zero-based (default is 0).

  --noreshape

    By default, mri_surf2surf will save the output as multiple
    'slices'; has no effect for paint/w output format. For ico, the output
    will appear to be a 'volume' with Nv/R colums, 1 row, R slices and Nf 
    frames, where Nv is the number of vertices on the surface. For icosahedrons, 
    R=6. For others, R will be the prime factor of Nv closest to 6. Reshaping 
    is for logistical purposes (eg, in the analyze format the size of a dimension 
    cannot exceed 2^15). Use this flag to prevent this behavior. This has no 
    effect when the output type is paint.

EXAMPLES:

1. Resample a subject's thickness of the left cortical hemisphere on to a 
   7th order icosahedron and save in analyze4d format:

   mri_surf2surf --hemi lh --srcsubject bert 
      --srcsurfval thickness --src_type curv 
      --trgsubject ico --trgicoorder 7 
      --trgsurfval bert-thickness-lh.img --trg_type analyze4d 

2. Resample data on the icosahedron to the right hemisphere of subject bert.
   Note that both the source and target data are stored in mgh format
   as "volume-encoded suface" data.

   mri_surf2surf --hemi rh --srcsubject ico --srcsurfval icodata-rh.mgh
      --trgsubject bert --trgsurfval ./bert-ico-rh.mgh 

3. Convert the surface coordinates of the lh.white of a subject to a
   (talairach) average (ie, a subject created by make_average_subject):

   mri_surf2surf --s yoursubject --hemi lh --sval-tal-xyz white 
      --tval lh.white.yoursubject --tval-xyz --trgsubject youraveragesubject

   This will create youraveragesubject/surf/lh.white.yoursubject

4. Extract surface normals of the white surface and save in a
   volume-encoded file:

   mri_surf2surf --s yoursubject --hemi lh --sval-nxyz white 
      --tval lh.white.norm.mgh 

   This will create youraveragesubject/surf/lh.white.yoursubject


BUG REPORTS: send bugs to analysis-bugs@nmr.mgh.harvard.edu. Make sure 
    to include the version and full command-line and enough information to
    be able to recreate the problem. Not that anyone does.

BUGS:

  When the output format is paint, the output file must be specified with
  a partial path (eg, ./data-lh.w) or else the output will be written into
  the subject's anatomical directory.

ENDHELP
*/

#undef X
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "mri.h"
#include "icosahedron.h"
#include "fio.h"
#include "pdf.h"

#include "MRIio_old.h"
#include "error.h"
#include "diag.h"
#include "mrisurf.h"
#include "mrisutils.h"
#include "mri2.h"
#include "mri_identify.h"

#include "bfileio.h"
#include "registerio.h"
//  extern char *ResampleVtxMapFile;
#include "resample.h"
#include "selxavgio.h"
#include "prime.h"
#include "version.h"

int DumpSurface(MRIS *surf, char *outfile);
MRI *MRIShksmooth(MRIS *Surf, MRI *Src, double sigma, int nSmoothSteps, MRI *Targ);
MRI *MRISheatkernel(MRIS *surf, double sigma);

double MRISareaTriangle(double x0, double y0, double z0, 
			double x1, double y1, double z1, 
			double x2, double y2, double z2);
int MRIStriangleAngles(double x0, double y0, double z0, 
		       double x1, double y1, double z1, 
		       double x2, double y2, double z2,
		       double *a0, double *a1, double *a2);
MRI *MRISdiffusionWeights(MRIS *surf);
MRI *MRISdiffusionSmooth(MRIS *Surf, MRI *Src, double GStd, MRI *Targ);
int MRISareNeighbors(MRIS *surf, int vtxno1, int vtxno2);

double MRISdiffusionEdgeWeight(MRIS *surf, int vtxno0, int vtxnonbr);
double MRISsumVertexFaceArea(MRIS *surf, int vtxno);
int MRIScommonNeighbors(MRIS *surf, int vtxno1, int vtxno2, 
			int *cvtxno1, int *cvtxno2);
int MRISdumpVertexNeighborhood(MRIS *surf, int vtxno);

static int  parse_commandline(int argc, char **argv);
static void check_options(void);
static void print_usage(void) ;
static void usage_exit(void);
static void print_help(void) ;
static void print_version(void) ;
static void argnerr(char *option, int n);
static void dump_options(FILE *fp);
static int  singledash(char *flag);
int GetNVtxsFromWFile(char *wfile);
int GetICOOrderFromValFile(char *filename, char *fmt);
int GetNVtxsFromValFile(char *filename, char *fmt);
int dump_surf(char *fname, MRIS *surf, MRI *mri);

int main(int argc, char *argv[]) ;

static char vcid[] = "$Id: mri_surf2surf.c,v 1.38.2.1 2006/11/29 20:28:13 nicks Exp $";
char *Progname = NULL;

char *surfregfile = NULL;
char *srchemi    = NULL;
char *trghemi    = NULL;

char *srcsubject = NULL;
char *srcvalfile = NULL;
char *srctypestring = "";
int   srctype = MRI_VOLUME_TYPE_UNKNOWN;
MRI  *SrcVals, *SrcHits, *SrcDist;
MRI_SURFACE *SrcSurfReg;
char *SrcHitFile = NULL;
char *SrcDistFile = NULL;
int nSrcVtxs = 0;
int SrcIcoOrder = -1;

int UseSurfSrc=0; // Get source values from surface, eg, xyz
char *SurfSrcName=NULL;
MRI_SURFACE *SurfSrc;
MATRIX *XFM=NULL;
#define SURF_SRC_XYZ     1
#define SURF_SRC_TAL_XYZ 2
#define SURF_SRC_AREA    3
#define SURF_SRC_NXYZ    4 // surface normals
#define SURF_SRC_RIP     5 // rip flag
int UseSurfTarg=0; // Put Src XYZ into a target surface

char *trgsubject = NULL;
char *trgvalfile = NULL;
char *trgtypestring = "";
int   trgtype = MRI_VOLUME_TYPE_UNKNOWN;
MRI  *TrgVals, *TrgValsSmth, *TrgHits, *TrgDist;
MRI_SURFACE *TrgSurfReg;
char *TrgHitFile = NULL;
char *TrgDistFile = NULL;
int TrgIcoOrder;

MRI  *mritmp;
int  reshape = 1;
int  reshapefactor;

char *mapmethod = "nnfr";

int UseHash = 1;
int framesave = 0;
float IcoRadius = 100.0;
int nthstep, nnbrs, nthnbr, nbrvtx, frame;
int nSmoothSteps = 0;
double fwhm=0, gstd;

double fwhm_Input=0, gstd_Input=0;
int nSmoothSteps_Input = 0;
int usediff = 0;

int debug = 0;
char *SrcDumpFile  = NULL;
char *TrgDumpFile = NULL;

char *SUBJECTS_DIR = NULL;
char *FREESURFER_HOME = NULL;
SXADAT *sxa;
FILE *fp;

char tmpstr[2000];

int ReverseMapFlag = 0;
int cavtx = 0; /* command-line vertex -- for debugging */
int jac = 0;

MRI *sphdist;

int SynthPDF = 0;
int SynthSeed = -1;

/*---------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
  int f,tvtx,svtx,n;
  float *framepower = NULL;
  char fname[2000];
  int nTrg121,nSrc121,nSrcLost;
  int nTrgMulti,nSrcMulti;
  float MnTrgMultiHits,MnSrcMultiHits, val;
  int nargs;
  FACE *face;
  VERTEX *vtx0,*vtx1,*vtx2;
  double area, a0, a1, a2;

  /* rkt: check for and handle version tag */
  nargs = handle_version_option (argc, argv, "$Id: mri_surf2surf.c,v 1.38.2.1 2006/11/29 20:28:13 nicks Exp $", "$Name:  $");
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  Progname = argv[0] ;
  argc --;
  argv++;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  //SrcSurfReg = ReadIcoByOrder(7,100);
  //MRISwrite(SrcSurfReg,"lh.ico7");
  //exit(1);

  if(argc == 0) usage_exit();

  parse_commandline(argc, argv);
  check_options();
  dump_options(stdout);

  SUBJECTS_DIR = getenv("SUBJECTS_DIR");
  if(SUBJECTS_DIR==NULL){
    fprintf(stderr,"ERROR: SUBJECTS_DIR not defined in environment\n");
    exit(1);
  }
  FREESURFER_HOME = getenv("FREESURFER_HOME") ;
  if(FREESURFER_HOME==NULL){
    fprintf(stderr,"ERROR: FREESURFER_HOME not defined in environment\n");
    exit(1);
  }

  /* --------- Load the registration surface for source subject --------- */
  if(!strcmp(srcsubject,"ico")){ /* source is ico */
    if(SrcIcoOrder == -1)
      SrcIcoOrder = GetICOOrderFromValFile(srcvalfile,srctypestring);
    sprintf(fname,"%s/lib/bem/ic%d.tri",FREESURFER_HOME,SrcIcoOrder);
    SrcSurfReg = ReadIcoByOrder(SrcIcoOrder, IcoRadius);
    printf("Source Ico Order = %d\n",SrcIcoOrder);
  }
  else{
    // use srchemi.sphere.reg regardless of whether hemis are diff
    sprintf(fname,"%s/%s/surf/%s.%s",SUBJECTS_DIR,
	    srcsubject,srchemi,surfregfile);
    printf("Reading source surface reg %s\n",fname);
    SrcSurfReg = MRISread(fname) ;
    if(cavtx > 0) 
      printf("cavtx = %d, srcsurfregfile: %g, %g, %g\n",cavtx,
	   SrcSurfReg->vertices[cavtx].x,
	   SrcSurfReg->vertices[cavtx].y,
	   SrcSurfReg->vertices[cavtx].z);
  }
  if (!SrcSurfReg)
    ErrorExit(ERROR_NOFILE, "%s: could not read surface %s", Progname, fname) ;

  MRIScomputeMetricProperties(SrcSurfReg);
  for(n = 0; n < SrcSurfReg->nfaces && 0; n++){
    face = &SrcSurfReg->faces[n];
    vtx0 = &SrcSurfReg->vertices[face->v[0]];
    vtx1 = &SrcSurfReg->vertices[face->v[1]];
    vtx2 = &SrcSurfReg->vertices[face->v[2]];
    area = MRISareaTriangle(vtx0->x,vtx0->y,vtx0->z,
			    vtx1->x,vtx1->y,vtx1->z,
			    vtx2->x,vtx2->y,vtx2->z);
    MRIStriangleAngles(vtx0->x,vtx0->y,vtx0->z,
		       vtx1->x,vtx1->y,vtx1->z,
		       vtx2->x,vtx2->y,vtx2->z,&a0,&a1,&a2);
    printf("n=%d, area = %f, %f %f %f   %f\n",n,face->area,a0,a1,a2,a0+a1+a2);
  }

  /* ------------------ load the source data ----------------------------*/
  printf("Loading source data\n");
  if(!strcmp(srctypestring,"curv")){ /* curvature file */
    if(fio_FileExistsReadable(srcvalfile)){
      memset(fname,0,strlen(fname));
      memcpy(fname,srcvalfile,strlen(srcvalfile));
    }
    else
      sprintf(fname,"%s/%s/surf/%s.%s",SUBJECTS_DIR,srcsubject,srchemi,srcvalfile);
    printf("Reading curvature file %s\n",fname);
    if(MRISreadCurvatureFile(SrcSurfReg, fname) != 0){
      printf("ERROR: reading curvature file\n");
      exit(1);
    }
    SrcVals = MRIcopyMRIS(NULL, SrcSurfReg, 0, "curv");
  }
  else if(!strcmp(srctypestring,"paint") || !strcmp(srctypestring,"w")){
    MRISreadValues(SrcSurfReg,srcvalfile);
    SrcVals = MRIcopyMRIS(NULL, SrcSurfReg, 0, "val");
  }
  else if(UseSurfSrc){
    sprintf(fname,"%s/%s/surf/%s.%s",SUBJECTS_DIR,srcsubject,srchemi,SurfSrcName);
    SurfSrc = MRISread(fname);
    if(SurfSrc==NULL) exit(1);
    MRIScomputeMetricProperties(SurfSrc);
    if(UseSurfSrc == SURF_SRC_XYZ || UseSurfSrc == SURF_SRC_TAL_XYZ){
      if(UseSurfSrc == SURF_SRC_TAL_XYZ){
	XFM = DevolveXFM(srcsubject, NULL, NULL);
	if(XFM == NULL) exit(1);
	printf("Applying MNI305 talairach transform\n");
	MatrixPrint(stdout,XFM);
	MRISmatrixMultiply(SurfSrc,XFM);
      }
      SrcVals = MRIcopyMRIS(NULL, SurfSrc, 2, "z"); // start at z to autoalloc 
      MRIcopyMRIS(SrcVals, SurfSrc, 0, "x");
      MRIcopyMRIS(SrcVals, SurfSrc, 1, "y");
    }
    if(UseSurfSrc == SURF_SRC_NXYZ){
      printf("Extracting surface normals\n");
      SrcVals = MRIcopyMRIS(NULL, SurfSrc, 2, "nz"); // start at z to autoalloc 
      MRIcopyMRIS(SrcVals, SurfSrc, 0, "nx");
      MRIcopyMRIS(SrcVals, SurfSrc, 1, "ny");
    }
    if(UseSurfSrc == SURF_SRC_AREA){
      SrcVals = MRIcopyMRIS(NULL, SurfSrc, 0, "area"); 
      if(SurfSrc->group_avg_surface_area > 0){
	if(getenv("FIX_VERTEX_AREA") != NULL){
	  printf("INFO: Fixing group surface area\n");
	  val = SurfSrc->group_avg_surface_area / SurfSrc->total_area;
	  MRIscalarMul(SrcVals,SrcVals,val);
	}
      }
    }
    if(UseSurfSrc == SURF_SRC_RIP){
      SrcVals = MRIcopyMRIS(NULL, SurfSrc, 0, "ripflag"); 
    }
    else printf("INFO: surfcluster: NOT fixing group surface area\n");
    MRISfree(&SurfSrc);
  }
  else { /* Use MRIreadType */
    SrcVals =  MRIreadType(srcvalfile,srctype);
    if(SrcVals == NULL){
      printf("ERROR: could not read %s as type %d\n",srcvalfile,srctype);
      exit(1);
    }
    if(SrcVals->height != 1 || SrcVals->depth != 1){
      reshapefactor = SrcVals->height * SrcVals->depth;
      printf("Reshaping %d\n",reshapefactor);
      mritmp = mri_reshape(SrcVals, reshapefactor*SrcVals->width, 
         1, 1, SrcVals->nframes);
      MRIfree(&SrcVals);
      SrcVals = mritmp;
      reshapefactor = 0; /* reset for output */
    }

    if(SrcVals->width != SrcSurfReg->nvertices){
      fprintf(stderr,"ERROR: dimesion inconsitency in source data\n");
      fprintf(stderr,"       Number of surface vertices = %d\n",
        SrcSurfReg->nvertices);
      fprintf(stderr,"       Number of value vertices = %d\n",SrcVals->width);
      exit(1);
    }
    if(is_sxa_volume(srcvalfile)){
      printf("INFO: Source volume detected as selxavg format\n");
      sxa = ld_sxadat_from_stem(srcvalfile);
      if(sxa == NULL) exit(1);
      framepower = sxa_framepower(sxa,&f);
      if(f != SrcVals->nframes){
	fprintf(stderr," number of frames is incorrect (%d,%d)\n",
		f,SrcVals->nframes);
	exit(1);
      }
      printf("INFO: Adjusting Frame Power\n");  fflush(stdout);
      mri_framepower(SrcVals,framepower);
    }
  }
  if(SrcVals == NULL){
    fprintf(stderr,"ERROR loading source values from %s\n",srcvalfile);
    exit(1);
  }
  if(SynthPDF != 0){
    if(SynthSeed < 0) SynthSeed = PDFtodSeed();
    printf("INFO: synthesizing, pdf = %d, seed = %d\n",SynthPDF,SynthSeed);
    srand48(SynthSeed);
    MRIrandn(SrcVals->width, SrcVals->height, SrcVals->depth,
	     SrcVals->nframes,0, 1, SrcVals);
  }

  if(SrcDumpFile != NULL){
    printf("Dumping input to %s\n",SrcDumpFile);
    DumpSurface(SrcSurfReg, SrcDumpFile);
  }

  /* Smooth input if desired */
  if(fwhm_Input > 0){
    nSmoothSteps_Input = 
      MRISfwhm2nitersSubj(fwhm_Input,srcsubject,srchemi,"white");
    if(nSmoothSteps_Input == -1) exit(1);
    printf("Approximating gaussian smoothing of source with fwhm = %lf,\n"
	   "std = %lf, with %d iterations of nearest-neighbor smoothing\n",
	   fwhm_Input,gstd_Input,nSmoothSteps_Input);
  }

  if(nSmoothSteps_Input > 0){
    printf("NN smoothing input with n = %d\n",nSmoothSteps_Input);
    MRISsmoothMRI(SrcSurfReg, SrcVals, nSmoothSteps_Input, NULL, SrcVals);
  }

  if(strcmp(srcsubject,trgsubject) || strcmp(srchemi,trghemi)){
    /* ------- Source and Target Subjects or Hemis are different -------------- */
    /* ------- Load the registration surface for target subject ------- */
    if(!strcmp(trgsubject,"ico")){
      sprintf(fname,"%s/lib/bem/ic%d.tri",FREESURFER_HOME,TrgIcoOrder);
      TrgSurfReg = ReadIcoByOrder(TrgIcoOrder, IcoRadius);
      reshapefactor = 6;
    }
    else{
      if(!strcmp(srchemi,trghemi)) // hemis are the same
	sprintf(fname,"%s/%s/surf/%s.%s",SUBJECTS_DIR,trgsubject,trghemi,surfregfile);
      else // hemis are the different
	sprintf(fname,"%s/%s/surf/%s.%s.%s",SUBJECTS_DIR,
		trgsubject,trghemi,srchemi,surfregfile);
      printf("Reading target surface reg %s\n",fname);
      TrgSurfReg = MRISread(fname) ;
    }
    if (!TrgSurfReg)
      ErrorExit(ERROR_NOFILE, "%s: could not read surface %s", 
    Progname, fname) ;
    printf("Done\n");
    
    if(!strcmp(mapmethod,"nnfr")) ReverseMapFlag = 1;
    else                          ReverseMapFlag = 0;
    
    /*-------------------------------------------------------------*/
    /* Map the values from the surface to surface */
    if(!jac){
      printf("Mapping Source Volume onto Source Subject Surface\n");
      TrgVals = surf2surf_nnfr(SrcVals, SrcSurfReg,TrgSurfReg,
			       &SrcHits,&SrcDist,&TrgHits,&TrgDist,
			       ReverseMapFlag,UseHash);
    } else {
      printf("Mapping Source Volume onto Source Subject Surface with Jacobian Correction\n");
      TrgVals = surf2surf_nnfr_jac(SrcVals, SrcSurfReg,TrgSurfReg,
				   &SrcHits,&SrcDist,&TrgHits,&TrgDist,
				   ReverseMapFlag,UseHash);
    }
    
    /* Compute some stats on the mapping number of srcvtx mapping to a 
       target vtx*/
    nTrg121 = 0;
    MnTrgMultiHits = 0.0;
    for(tvtx = 0; tvtx < TrgSurfReg->nvertices; tvtx++){
      n = MRIFseq_vox(TrgHits,tvtx,0,0,0);
      if(n == 1) nTrg121++;
      else MnTrgMultiHits += n;
    }
    nTrgMulti = TrgSurfReg->nvertices - nTrg121;
    if(nTrgMulti > 0) MnTrgMultiHits = (MnTrgMultiHits/nTrgMulti);
    else              MnTrgMultiHits = 0;
    printf("nTrg121 = %5d, nTrgMulti = %5d, MnTrgMultiHits = %g\n",
     nTrg121,nTrgMulti,MnTrgMultiHits);
    
    /* Compute some stats on the mapping number of trgvtxs mapped from a 
       source vtx*/
    nSrc121 = 0;
    nSrcLost = 0;
    MnSrcMultiHits = 0.0;
    for(svtx = 0; svtx < SrcSurfReg->nvertices; svtx++){
      n = MRIFseq_vox(SrcHits,svtx,0,0,0);
      if(n == 1)      nSrc121++;
      else if(n == 0) nSrcLost++;
      else MnSrcMultiHits += n;
    }
    nSrcMulti = SrcSurfReg->nvertices - nSrc121;
    if(nSrcMulti > 0) MnSrcMultiHits = (MnSrcMultiHits/nSrcMulti);
    else              MnSrcMultiHits = 0;
    
    printf("nSrc121 = %5d, nSrcLost = %5d, nSrcMulti = %5d, "
     "MnSrcMultiHits = %g\n", nSrc121,nSrcLost,nSrcMulti,
     MnSrcMultiHits);
    
    /* save the Source Hits into a .w file */
    if(SrcHitFile != NULL){
      printf("INFO: saving source hits to %s\n",SrcHitFile);
      MRIScopyMRI(SrcSurfReg, SrcHits, 0, "val");
      //for(vtx = 0; vtx < SrcSurfReg->nvertices; vtx++)
      //SrcSurfReg->vertices[vtx].val = MRIFseq_vox(SrcHits,vtx,0,0,0) ;
      MRISwriteValues(SrcSurfReg, SrcHitFile) ;
    }
    /* save the Source Distance into a .w file */
    if(SrcDistFile != NULL){
      printf("INFO: saving source distance to %s\n",SrcDistFile);
      MRIScopyMRI(SrcSurfReg, SrcDist, 0, "val");
      MRISwriteValues(SrcSurfReg, SrcDistFile) ;
      //for(vtx = 0; vtx < SrcSurfReg->nvertices; vtx++)
      //SrcSurfReg->vertices[vtx].val = MRIFseq_vox(SrcDist,vtx,0,0,0) ;
    }
    /* save the Target Hits into a .w file */
    if(TrgHitFile != NULL){
      printf("INFO: saving target hits to %s\n",TrgHitFile);
      MRIScopyMRI(TrgSurfReg, TrgHits, 0, "val");
      MRISwriteValues(TrgSurfReg, TrgHitFile) ;
      //for(vtx = 0; vtx < TrgSurfReg->nvertices; vtx++)
      //TrgSurfReg->vertices[vtx].val = MRIFseq_vox(TrgHits,vtx,0,0,0) ;
    }
    /* save the Target Hits into a .w file */
    if(TrgDistFile != NULL){
      printf("INFO: saving target distance to %s\n",TrgDistFile);
      MRIScopyMRI(TrgSurfReg, TrgDist, 0, "val");
      MRISwriteValues(TrgSurfReg, TrgDistFile) ;
      //for(vtx = 0; vtx < TrgSurfReg->nvertices; vtx++)
      //TrgSurfReg->vertices[vtx].val = MRIFseq_vox(TrgDist,vtx,0,0,0) ;
    }
  }
  else{
    /* --- Source and Target Subjects are the same --- */
    printf("INFO: trgsubject = srcsubject\n");
    TrgSurfReg = SrcSurfReg;
    TrgVals = SrcVals;
  }
       
  /* Smooth output if desired */
  if(fwhm > 0){
    nSmoothSteps = MRISfwhm2nitersSubj(fwhm,trgsubject,trghemi,"white");
    if(nSmoothSteps == -1) exit(1);
    printf("Approximating gaussian smoothing of target with fwhm = %lf,\n"
	   " std = %lf, with %d iterations of nearest-neighbor smoothing\n",
	   fwhm,gstd,nSmoothSteps);
  }
  if(nSmoothSteps > 0)
    MRISsmoothMRI(TrgSurfReg, TrgVals, nSmoothSteps, NULL, TrgVals);

  /* readjust frame power if necessary */
  if(is_sxa_volume(srcvalfile)){
    printf("INFO: Readjusting Frame Power\n");  fflush(stdout);
    for(f=0; f < TrgVals->nframes; f++) framepower[f] = 1.0/framepower[f];
    mri_framepower(TrgVals,framepower);
    sxa->nrows = 1;
    sxa->ncols = TrgVals->width;
  }

  if(TrgDumpFile != NULL){
    /* Dump before reshaping */
    printf("Dumping output to %s\n",TrgDumpFile);
    dump_surf(TrgDumpFile,TrgSurfReg,TrgVals);
  }

  /* ------------ save the target data -----------------------------*/
  printf("Saving target data\n");
  if(!strcmp(trgtypestring,"paint") || !strcmp(trgtypestring,"w")){
    MRIScopyMRI(TrgSurfReg, TrgVals, framesave, "val");
    MRISwriteValues(TrgSurfReg,trgvalfile);
  }
  else if(UseSurfTarg){
    MRIScopyMRI(TrgSurfReg,TrgVals,0,"x");
    MRIScopyMRI(TrgSurfReg,TrgVals,1,"y");
    MRIScopyMRI(TrgSurfReg,TrgVals,2,"z");
    MRISwrite(TrgSurfReg, trgvalfile);
  }
  else {
    if(reshape){
      if(reshapefactor == 0) 
	reshapefactor = GetClosestPrimeFactor(TrgVals->width,6);
      
      printf("Reshaping %d (nvertices = %d)\n",reshapefactor,TrgVals->width);
      mritmp = mri_reshape(TrgVals, TrgVals->width / reshapefactor, 
			   1, reshapefactor,TrgVals->nframes);
      if(mritmp == NULL){
	printf("ERROR: mri_reshape could not alloc\n");
	return(1);
      }
      MRIfree(&TrgVals);
      TrgVals = mritmp;
    }
    MRIwriteType(TrgVals,trgvalfile,trgtype);
    if(is_sxa_volume(srcvalfile)) sv_sxadat_by_stem(sxa,trgvalfile);
  }

  return(0);
}
/* --------------------------------------------- */
static int parse_commandline(int argc, char **argv)
{
  int  nargc , nargsused;
  char **pargv, *option ;

  if(argc < 1) usage_exit();

  nargc   = argc;
  pargv = argv;
  while(nargc > 0){

    option = pargv[0];
    if(debug) printf("%d %s\n",nargc,option);
    nargc -= 1;
    pargv += 1;

    nargsused = 0;

    if (!strcasecmp(option, "--help"))  print_help() ;

    else if (!strcasecmp(option, "--version")) print_version() ;

    else if (!strcasecmp(option, "--debug"))   debug = 1;
    else if (!strcasecmp(option, "--usehash")) UseHash = 1;
    else if (!strcasecmp(option, "--hash")) UseHash = 1;
    else if (!strcasecmp(option, "--dontusehash")) UseHash = 0;
    else if (!strcasecmp(option, "--nohash")) UseHash = 0;
    else if (!strcasecmp(option, "--noreshape")) reshape = 0;
    else if (!strcasecmp(option, "--reshape"))   reshape = 1;
    else if (!strcasecmp(option, "--usediff"))   usediff = 1;
    else if (!strcasecmp(option, "--nousediff")) usediff = 0;
    else if (!strcasecmp(option, "--synth"))     SynthPDF = 1;
    else if (!strcasecmp(option, "--jac"))       jac = 1;

    else if (!strcmp(option, "--seed")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%d",&SynthSeed);
      nargsused = 1;
    }
    else if (!strcmp(option, "--s")){
      if(nargc < 1) argnerr(option,1);
      srcsubject = pargv[0];
      trgsubject = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--srcsubject")){
      if(nargc < 1) argnerr(option,1);
      srcsubject = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--srcsurfval") || !strcmp(option, "--sval")){
      if(nargc < 1) argnerr(option,1);
      srcvalfile = pargv[0];
      nargsused = 1;
    }
    else if (!strcasecmp(option, "--sval-xyz")){
      if(nargc < 1) argnerr(option,1);
      SurfSrcName = pargv[0];
      UseSurfSrc = SURF_SRC_XYZ;
      nargsused = 1;
    }
    else if (!strcasecmp(option, "--sval-nxyz")){
      if(nargc < 1) argnerr(option,1);
      SurfSrcName = pargv[0];
      UseSurfSrc = SURF_SRC_NXYZ;
      nargsused = 1;
    }
    else if (!strcasecmp(option, "--sval-tal-xyz")){
      if(nargc < 1) argnerr(option,1);
      SurfSrcName = pargv[0];
      UseSurfSrc = SURF_SRC_TAL_XYZ;
      nargsused = 1;
    }
    else if (!strcasecmp(option, "--sval-area")){
      if(nargc < 1) argnerr(option,1);
      SurfSrcName = pargv[0];
      UseSurfSrc = SURF_SRC_AREA;
      nargsused = 1;
    }
    else if (!strcasecmp(option, "--sval-rip")){
      if(nargc < 1) argnerr(option,1);
      SurfSrcName = pargv[0];
      UseSurfSrc = SURF_SRC_RIP;
      nargsused = 1;
    }
    else if (!strcmp(option, "--srcdump")){
      if(nargc < 1) argnerr(option,1);
      SrcDumpFile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--srcfmt") || !strcmp(option, "--sfmt") ||
       !strcmp(option, "--src_type")){
      if(nargc < 1) argnerr(option,1);
      srctypestring = pargv[0];
      srctype = string_to_type(srctypestring);
      nargsused = 1;
    }
    else if (!strcmp(option, "--srcicoorder")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%d",&SrcIcoOrder);
      nargsused = 1;
    }
    else if (!strcmp(option, "--nsmooth-in")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%d",&nSmoothSteps_Input);
      if(nSmoothSteps_Input < 1){
	fprintf(stderr,"ERROR: number of smooth steps (%d) must be >= 1\n",
		nSmoothSteps_Input);
      }
      nargsused = 1;
    }
    else if (!strcmp(option, "--nsmooth-out") ||
	     !strcmp(option, "--nsmooth")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%d",&nSmoothSteps);
      if(nSmoothSteps < 1){
	fprintf(stderr,"ERROR: number of smooth steps (%d) must be >= 1\n",
		nSmoothSteps);
      }
      nargsused = 1;
    }
    else if (!strcmp(option, "--fwhm-src")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%lf",&fwhm_Input);
      gstd_Input = fwhm_Input/sqrt(log(256.0));
      nargsused = 1;
    }
    else if (!strcmp(option, "--fwhm") ||
	     !strcmp(option, "--fwhm-trg")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%lf",&fwhm);
      gstd = fwhm/sqrt(log(256.0));
      nargsused = 1;
    }

    /* -------- target value inputs ------ */
    else if (!strcmp(option, "--trgsubject")){
      if(nargc < 1) argnerr(option,1);
      trgsubject = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--trgicoorder")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%d",&TrgIcoOrder);
      nargsused = 1;
    }
    else if (!strcmp(option, "--trgsurfval")  || !strcmp(option, "--tval")){
      if(nargc < 1) argnerr(option,1);
      trgvalfile = pargv[0];
      nargsused = 1;
    }
    else if (!strcasecmp(option, "--tval-xyz")){
      UseSurfTarg = 1;
    }
    else if (!strcmp(option, "--trgdump")){
      if(nargc < 1) argnerr(option,1);
      TrgDumpFile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--trgfmt") ||!strcmp(option, "--tfmt") ||
       !strcmp(option, "--trg_type")){
      if(nargc < 1) argnerr(option,1);
      trgtypestring = pargv[0];
      if(!strcmp(trgtypestring,"curv")){
  fprintf(stderr,"ERROR: Cannot select curv as target format\n");
  exit(1);
      }
      trgtype = string_to_type(trgtypestring);
      nargsused = 1;
    }

    else if (!strcmp(option, "--frame")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%d",&framesave);
      nargsused = 1;
    }
    else if (!strcmp(option, "--cavtx")){
      /* command-line vertex -- for debugging */
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%d",&cavtx);
      nargsused = 1;
    }
    else if(!strcmp(option, "--hemi") || !strcmp(option, "--h")){
      if(nargc < 1) argnerr(option,1);
      srchemi = pargv[0];
      trghemi = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--srchemi")){
      if(nargc < 1) argnerr(option,1);
      srchemi = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--trghemi")){
      if(nargc < 1) argnerr(option,1);
      trghemi = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--surfreg")){
      if(nargc < 1) argnerr(option,1);
      surfregfile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--mapmethod")){
      if(nargc < 1) argnerr(option,1);
      mapmethod = pargv[0];
      if(strcmp(mapmethod,"nnfr") && strcmp(mapmethod,"nnf")){
	fprintf(stderr,"ERROR: mapmethod must be nnfr or nnf\n");
	exit(1);
      }
      nargsused = 1;
    }
    else if (!strcmp(option, "--srchits")){
      if(nargc < 1) argnerr(option,1);
      SrcHitFile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--srcdist")){
      if(nargc < 1) argnerr(option,1);
      SrcDistFile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--trghits")){
      if(nargc < 1) argnerr(option,1);
      TrgHitFile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--trgdist")){
      if(nargc < 1) argnerr(option,1);
      TrgDistFile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--vtxmap")){
      if(nargc < 1) argnerr(option,1);
      ResampleVtxMapFile = pargv[0];
      nargsused = 1;
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
  fprintf(stdout, "   --srcsubject source subject\n");
  fprintf(stdout, "   --sval path of file with input values \n");
  fprintf(stdout, "   --sval-xyz  surfname : use xyz of surfname as input \n");
  fprintf(stdout, "   --sval-tal-xyz  surfname : use tal xyz of surfname as input \n");
  fprintf(stdout, "   --sval-area surfname : use vertex area of surfname as input \n");
  fprintf(stdout, "   --sval-nxyz surfname : use surface normals of surfname as input \n");
  fprintf(stdout, "   --sfmt   source format\n");
  fprintf(stdout, "   --srcicoorder when srcsubject=ico and src is .w\n");
  fprintf(stdout, "   --trgsubject target subject\n");
  fprintf(stdout, "   --trgicoorder when trgsubject=ico\n");
  fprintf(stdout, "   --tval path of file in which to store output values\n");
  fprintf(stdout, "   --tval-xyz : save as surface with source xyz \n");
  fprintf(stdout, "   --tfmt target format\n");
  fprintf(stdout, "   --s subject : use subject as src and target\n");
  fprintf(stdout, "   --hemi       hemisphere (lh or rh) \n");
  fprintf(stdout, "   --surfreg    surface registration (sphere.reg)  \n");
  fprintf(stdout, "   --mapmethod  nnfr or nnf\n");
  fprintf(stdout, "   --frame      save only nth frame (with --trg_type paint)\n");
  fprintf(stdout, "   --fwhm-src fwhmsrc: smooth the source to fwhmsrc\n");  
  fprintf(stdout, "   --fwhm-trg fwhmtrg: smooth the target to fwhmtrg\n");  
  fprintf(stdout, "   --nsmooth-in N  : smooth the input\n");  
  fprintf(stdout, "   --nsmooth-out N : smooth the output\n");  

  fprintf(stdout, "   --noreshape  do not reshape output to multiple 'slices'\n");  
  fprintf(stdout, "   --synth : replace input with WGN\n");  
  fprintf(stdout, "   --seed seed : seed for synth (default is auto)\n");  

  fprintf(stdout, "\n");
  printf("%s\n", vcid) ;
  printf("\n");

}
/* --------------------------------------------- */
static void print_help(void)
{
  print_usage() ;
printf("\n");
printf("This program will resample one surface onto another. The source and \n");
printf("target subjects can be any subject in $SUBJECTS_DIR and/or the  \n");
printf("icosahedron (ico). The source and target file formats can be anything \n");
printf("supported by mri_convert. The source format can also be a curvature \n");
printf("file or a paint (.w) file. The user also has the option of smoothing \n");
printf("on the surface. \n");
printf("\n");
printf("OPTIONS\n");
printf("\n");
printf("  --srcsubject subjectname\n");
printf("\n");
printf("    Name of source subject as found in $SUBJECTS_DIR or ico for icosahedron.\n");
printf("    The input data must have been sampled onto this subject's surface (eg, \n");
printf("    using mri_vol2surf)\n");
printf("\n");
printf("  --sval sourcefile\n");
printf("\n");
printf("    Name of file where the data on the source surface is located.\n");
printf("\n");
printf("  --sval-xyz     surfname\n");
printf("  --sval-tal-xyz surfname\n");
printf("  --sval-area    surfname\n");
printf("  --sval-nxyz    surfname\n");
printf("\n");
printf("    Use measures from the input surface as the source (instead of specifying\n");
printf("    a source file explicitly with --sval). --sval-xyz extracts the x, y, and\n");
printf("    z of each vertex. --sval-tal-xyz is the same as --sval-xyz, but applies\n");
printf("    the talairach transform from srcsubject/mri/transforms/talairach.xfm.\n");
printf("    --sval-area extracts the vertex area. --sval-nxyz extracts the surface\n");
printf("    normals at each vertex. See also --tval-xyz.\n");
printf("\n");
printf("  --sfmt typestring\n");
printf("\n");
printf("    Format type string. Can be either curv (for FreeSurfer curvature file), \n");
printf("    paint or w (for FreeSurfer paint files), or anything accepted by \n");
printf("    mri_convert. If no type string  is given, then the type is determined \n");
printf("    from the sourcefile (if possible). If curv is used, then the curvature\n");
printf("    file will be looked for in $SUBJECTS_DIR/srcsubject/surf/hemi.sourcefile.\n");
printf("\n");
printf("  --srcicoorder order\n");
printf("\n");
printf("    Icosahedron order of the source. Normally, this can be detected based\n");
printf("    on the number of verticies, but this will fail with a .w file as input.\n");
printf("    This is only needed when the source is a .w file.\n");
printf("\n");
printf("  --trgsubject subjectname\n");
printf("\n");
printf("    Name of target subject as found in $SUBJECTS_DIR or ico for icosahedron.\n");
printf("\n");
printf("  --trgicoorder order\n");
printf("\n");
printf("    Icosahedron order number. This specifies the size of the\n");
printf("    icosahedron according to the following table: \n");
printf("              Order  Number of Vertices\n");
printf("                0              12 \n");
printf("                1              42 \n");
printf("                2             162 \n");
printf("                3             642 \n");
printf("                4            2562 \n");
printf("                5           10242 \n");
printf("                6           40962 \n");
printf("                7          163842 \n");
printf("    In general, it is best to use the largest size available.\n");
printf("\n");
printf("  --tval targetfile\n");
printf("\n");
printf("    Name of file where the data on the target surface will be stored.\n");
printf("    BUG ALERT: for trg_type w or paint, use the full path.\n");
printf("\n");
printf("  --tval-xyz\n");
printf("\n");
printf("    Flag to indicate that the source surface xyz as a binary surface file\n");
printf("    given by the target file. This requires that --sval-xyz or --sval-tal-xyz.\n");
printf("    This is a good way to map the surface of one subject to an average\n");
printf("    (talairach) subject. Note: it will save targetfile as\n");
printf("    trgsubject/surf/targetfile unless targetfile has a path.\n");
printf("\n");
printf("  --tfmt typestring\n");
printf("\n");
printf("    Format type string. Can be paint or w (for FreeSurfer paint files) or anything\n");
printf("    accepted by mri_convert. NOTE: output cannot be stored in curv format\n");
printf("    If no type string  is given, then the type is determined from the sourcefile\n");
printf("    (if possible). If using paint or w, see also --frame.\n");
printf("\n");
printf("  --hemi hemifield (lh or rh)\n");
printf("\n");
printf("  --surfreg registration_surface\n");
printf("\n");
printf("    If the source and target subjects are not the same, this surface is used \n");
printf("    to register the two surfaces. sphere.reg is used as the default. Don't change\n");
printf("    this unless you know what you are doing.\n");
printf("\n");
printf("  --mapmethod methodname\n");
printf("\n");
printf("    Method used to map from the vertices in one subject to those of another.\n");
printf("    Legal values are: nnfr (neighest-neighbor, forward and reverse) and nnf\n");
printf("    (neighest-neighbor, forward only). Default is nnfr. The mapping is done\n");
printf("    in the following way. For each vertex on the target surface, the closest\n");
printf("    vertex in the source surface is found, based on the distance in the \n");
printf("    registration space (this is the forward map). If nnf is chosen, then the\n");
printf("    the value at the target vertex is set to that of the closest source vertex.\n");
printf("    This, however, can leave some source vertices unrepresented in target (ie,\n");
printf("    'holes'). If nnfr is chosen, then each hole is assigned to the closest\n");
printf("    target vertex. If a target vertex has multiple source vertices, then the\n");
printf("    source values are averaged together. It does not seem to make much difference.\n");
printf("\n");
printf("  --fwhm-src fwhmsrc\n");
printf("  --fwhm-trg fwhmtrg (can also use --fwhm)\n");
printf("\n");
printf("    Smooth the source or target with a gaussian with the given fwhm (mm). This is\n");
printf("    actually an approximation done using iterative nearest neighbor smoothing.\n");
printf("    The number of iterations is computed based on the white surface. This\n");
printf("    method is similar to heat kernel smoothing. This will give the same\n");
printf("    results as --nsmooth-{in,out}, but automatically computes the the \n");
printf("    number of iterations based on the desired fwhm.\n");
printf("\n");
printf("  --nsmooth-in  niterations\n");
printf("  --nsmooth-out niterations  [note: same as --smooth]\n");
printf("\n");
printf("    Number of smoothing iterations. Each iteration consists of averaging each\n");
printf("    vertex with its neighbors. When only smoothing is desired, just set the \n");
printf("    the source and target subjects to the same subject. --smooth-in smooths\n");
printf("    the input surface values prior to any resampling. --smooth-out smooths\n");
printf("    after any resampling. See also --fwhm-src and --fwhm-trg.\n");
printf("\n");
printf("  --frame framenumber\n");
printf("\n");
printf("    When using paint/w output format, this specifies which frame to output. This\n");
printf("    format can store only one frame. The frame number is zero-based (default is 0).\n");
printf("\n");
printf("  --noreshape\n");
printf("\n");
printf("    By default, mri_surf2surf will save the output as multiple\n");
printf("    'slices'; has no effect for paint/w output format. For ico, the output\n");
printf("    will appear to be a 'volume' with Nv/R colums, 1 row, R slices and Nf \n");
printf("    frames, where Nv is the number of vertices on the surface. For icosahedrons, \n");
printf("    R=6. For others, R will be the prime factor of Nv closest to 6. Reshaping \n");
printf("    is for logistical purposes (eg, in the analyze format the size of a dimension \n");
printf("    cannot exceed 2^15). Use this flag to prevent this behavior. This has no \n");
printf("    effect when the output type is paint.\n");
printf("\n");
printf("EXAMPLES:\n");
printf("\n");
printf("1. Resample a subject's thickness of the left cortical hemisphere on to a \n");
printf("   7th order icosahedron and save in analyze4d format:\n");
printf("\n");
printf("   mri_surf2surf --hemi lh --srcsubject bert \n");
printf("      --srcsurfval thickness --src_type curv \n");
printf("      --trgsubject ico --trgicoorder 7 \n");
printf("      --trgsurfval bert-thickness-lh.img --trg_type analyze4d \n");
printf("\n");
printf("2. Resample data on the icosahedron to the right hemisphere of subject bert.\n");
printf("   Save in paint so that it can be viewed as an overlay in tksurfer. The \n");
printf("   source data is stored in bfloat format (ie, icodata_000.bfloat, ...)\n");
printf("\n");
printf("   mri_surf2surf --hemi rh --srcsubject ico \n");
printf("      --srcsurfval icodata-rh --src_type bfloat \n");
printf("      --trgsubject bert \n");
printf("      --trgsurfval ./bert-ico-rh.w --trg_type paint \n");
printf("\n");
printf("3. Convert the surface coordinates of the lh.white of a subject to a\n");
printf("   (talairach) average (ie, a subject created by make_average_subject):\n");
printf("\n");
printf("   mri_surf2surf --s yoursubject --hemi lh --sval-tal-xyz white \n");
printf("      --tval lh.white.yoursubject --tval-xyz --trgsubject youraveragesubject\n");
printf("\n");
printf("   This will create youraveragesubject/surf/lh.white.yoursubject\n");
printf("\n");
printf("\n");
printf("BUG REPORTS: send bugs to analysis-bugs@nmr.mgh.harvard.edu. Make sure \n");
printf("    to include the version and full command-line and enough information to\n");
printf("    be able to recreate the problem. Not that anyone does.\n");
printf("\n");
printf("BUGS:\n");
printf("\n");
printf("  When the output format is paint, the output file must be specified with\n");
printf("  a partial path (eg, ./data-lh.w) or else the output will be written into\n");
printf("  the subject's anatomical directory.\n");
printf("\n");
  exit(1) ;
}

/* --------------------------------------------- */
static void dump_options(FILE *fp)
{
  fprintf(fp,"srcsubject = %s\n",srcsubject);
  fprintf(fp,"srcval     = %s\n",srcvalfile);
  fprintf(fp,"srctype    = %s\n",srctypestring);
  fprintf(fp,"trgsubject = %s\n",trgsubject);
  fprintf(fp,"trgval     = %s\n",trgvalfile);
  fprintf(fp,"trgtype    = %s\n",trgtypestring);
  fprintf(fp,"surfreg    = %s\n",surfregfile);
  fprintf(fp,"srchemi    = %s\n",srchemi);
  fprintf(fp,"trghemi    = %s\n",trghemi);
  fprintf(fp,"frame      = %d\n",framesave);
  fprintf(fp,"fwhm-in    = %g\n",fwhm_Input);
  fprintf(fp,"fwhm-out   = %g\n",fwhm);
  return;
}
/* --------------------------------------------- */
static void print_version(void)
{
  fprintf(stdout, "%s\n", vcid) ;
  exit(1) ;
}
/* --------------------------------------------- */
static void argnerr(char *option, int n)
{
  if(n==1)
    fprintf(stdout,"ERROR: %s flag needs %d argument\n",option,n);
  else
    fprintf(stdout,"ERROR: %s flag needs %d arguments\n",option,n);
  exit(-1);
}
/* --------------------------------------------- */
static void check_options(void)
{
  if(srcsubject == NULL){
    fprintf(stdout,"ERROR: no source subject specified\n");
    exit(1);
  }
  if(srcvalfile == NULL && UseSurfSrc == 0){
    fprintf(stdout,"A source value path must be supplied\n");
    exit(1);
  }

  if(UseSurfSrc == 0){
  if( strcasecmp(srctypestring,"w") != 0 &&
      strcasecmp(srctypestring,"curv") != 0 &&
      strcasecmp(srctypestring,"paint") != 0 ){
    if(srctype == MRI_VOLUME_TYPE_UNKNOWN) {
      srctype = mri_identify(srcvalfile);
      if(srctype == MRI_VOLUME_TYPE_UNKNOWN){
	fprintf(stdout,"ERROR: could not determine type of %s\n",srcvalfile);
	exit(1);
      }
    }
  }
  }

  if(trgsubject == NULL){
    fprintf(stdout,"ERROR: no target subject specified\n");
    exit(1);
  }
  if(trgvalfile == NULL){
    fprintf(stdout,"A target value path must be supplied\n");
    exit(1);
  }

  if(UseSurfTarg == 0){
    if( strcasecmp(trgtypestring,"w") != 0 &&
	strcasecmp(trgtypestring,"curv") != 0 &&
	strcasecmp(trgtypestring,"paint") != 0 ){
      if(trgtype == MRI_VOLUME_TYPE_UNKNOWN) {
	trgtype = mri_identify(trgvalfile);
	if(trgtype == MRI_VOLUME_TYPE_UNKNOWN){
	  fprintf(stdout,"ERROR: could not determine type of %s\n",trgvalfile);
	  exit(1);
	}
      }
    }
  }
  else{
    if(UseSurfSrc != SURF_SRC_XYZ && UseSurfSrc != SURF_SRC_TAL_XYZ){
      printf("ERROR: must use --sval-xyz or --sval-tal-xyz with --tval-xyz\n");
      exit(1);
    }
  }

  if(surfregfile == NULL) surfregfile = "sphere.reg";
  else printf("Registration surface changed to %s\n",surfregfile);

  if(srchemi == NULL){
    fprintf(stdout,"ERROR: no hemifield specified\n");
    exit(1);
  }

  if(fwhm != 0 && nSmoothSteps != 0){
    printf("ERROR: cannot specify --fwhm-out and --nsmooth-out\n");
    exit(1);
  }
  if(fwhm_Input != 0 && nSmoothSteps_Input != 0){
    printf("ERROR: cannot specify --fwhm-in and --nsmooth-in\n");
    exit(1);
  }
  return;
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
int GetNVtxsFromWFile(char *wfile)
{
  FILE *fp;
  int i,ilat, num, nvertices;
  int *vtxnum;
  float *wval;

  fp = fopen(wfile,"r");
  if (fp==NULL) {
    fprintf(stdout,"ERROR: Progname: GetNVtxsFromWFile():\n");
    fprintf(stdout,"Could not open %s\n",wfile);
    fprintf(stdout,"(%s,%d,%s)\n",__FILE__, __LINE__,__DATE__);
    exit(1);
  }
  
  fread2(&ilat,fp);
  fread3(&num,fp);
  vtxnum = (int *)   calloc(sizeof(int),   num);
  wval   = (float *) calloc(sizeof(float), num);

  for (i=0;i<num;i++){
    fread3(&vtxnum[i],fp);
    wval[i] = freadFloat(fp) ;
  }
  fclose(fp);

  nvertices = vtxnum[num-1] + 1;

  free(vtxnum);
  free(wval);

  return(nvertices);
}
//MRI *MRIreadHeader(char *fname, int type);
/*---------------------------------------------------------------*/
int GetNVtxsFromValFile(char *filename, char *typestring)
{
  //int err,nrows, ncols, nslcs, nfrms, endian;
  int nVtxs=0;
  int type;
  MRI *mri;

  printf("GetNVtxs: %s %s\n",filename,typestring);

  if(!strcmp(typestring,"curv")){
    fprintf(stdout,"ERROR: cannot get nvertices from curv format\n");
    exit(1);
  }

  if(!strcmp(typestring,"paint") || !strcmp(typestring,"w")){
    nVtxs = GetNVtxsFromWFile(filename);
    return(nVtxs);
  }

  type = string_to_type(typestring);
  mri = MRIreadHeader(filename, type);
  if(mri == NULL) exit(1);
  
  nVtxs = mri->width*mri->height*mri->depth;

  MRIfree(&mri);

  return(nVtxs);
}
/*---------------------------------------------------------------*/
int GetICOOrderFromValFile(char *filename, char *fmt)
{
  int nIcoVtxs,IcoOrder;

  nIcoVtxs = GetNVtxsFromValFile(filename, fmt);

  IcoOrder = IcoOrderFromNVtxs(nIcoVtxs);
  if(IcoOrder < 0){
    fprintf(stdout,"ERROR: number of vertices = %d, does not mach ico\n",
      nIcoVtxs);
    exit(1);

  }
  
  return(IcoOrder);
}
/*---------------------------------------------------------------*/
int dump_surf(char *fname, MRIS *surf, MRI *mri)
{
  FILE *fp;
  float val;
  int vtxno,nnbrs;
  VERTEX *vtx;

  fp = fopen(fname,"w");
  if(fp == NULL){
    printf("ERROR: could not open %s\n",fname);
    exit(1);
  }
  for(vtxno = 0; vtxno < surf->nvertices; vtxno++){
    val = MRIFseq_vox(mri,vtxno,0,0,0); //first frame
    if(val == 0.0) continue;
    nnbrs = surf->vertices[vtxno].vnum;
    vtx = &surf->vertices[vtxno];
    fprintf(fp,"%5d  %2d  %8.4f %8.4f %8.4f   %g\n",
	    vtxno,nnbrs,vtx->x,vtx->y,vtx->z,val);
  }
  fclose(fp);
  return(0);
}



/*---------------------------------------------------------------
  See also mrisTriangleArea() in mrisurf.c
  ---------------------------------------------------------------*/
double MRISareaTriangle(double x0, double y0, double z0, 
			double x1, double y1, double z1, 
			double x2, double y2, double z2)
{
  double xx, yy, zz, a;
      
  xx = (y1-y0)*(z2-z0) - (z1-z0)*(y2-y0);
  yy = (z1-z0)*(x2-x0) - (x1-x0)*(z2-z0);
  zz = (x1-x0)*(y2-y0) - (y1-y0)*(x2-x0);
      
  a = 0.5 * sqrt( xx*xx + yy*yy + zz*zz );
  return(a);
}
/*------------------------------------------------------------*/
int MRIStriangleAngles(double x0, double y0, double z0, 
		       double x1, double y1, double z1, 
		       double x2, double y2, double z2,
		       double *a0, double *a1, double *a2)
{
  double d0, d1, d2, d0s, d1s, d2s;
      
  /* dN is the distance of the segment opposite vertex N*/
  d0s = (x1-x2)*(x1-x2) + (y1-y2)*(y1-y2) + (z1-z2)*(z1-z2);
  d1s = (x0-x2)*(x0-x2) + (y0-y2)*(y0-y2) + (z0-z2)*(z0-z2);
  d2s = (x0-x1)*(x0-x1) + (y0-y1)*(y0-y1) + (z0-z1)*(z0-z1);
  d0 = sqrt(d0s);
  d1 = sqrt(d1s);
  d2 = sqrt(d2s);

  /* Law of cosines */
  *a0 = acos( -(d0s-d1s-d2s)/(2*d1*d2) );
  *a1 = acos( -(d1s-d0s-d2s)/(2*d0*d2) );
  *a2 = M_PI - (*a0 + *a1);

  return(0);
}
/*------------------------------------------------------------*/
MRI *MRISdiffusionWeights(MRIS *surf)
{
  MRI *w;
  int nnbrsmax, nnbrs, vtxno, vtxnonbr;
  double area, wtmp;

  /* count the maximum number of neighbors */
  nnbrsmax = surf->vertices[0].vnum;
  for(vtxno = 0; vtxno < surf->nvertices; vtxno++)
    if(nnbrsmax < surf->vertices[vtxno].vnum) 
      nnbrsmax = surf->vertices[vtxno].vnum;
  printf("nnbrsmax = %d\n",nnbrsmax);

  MRIScomputeMetricProperties(surf); /* for area */

  w = MRIallocSequence(surf->nvertices, 1, 1, MRI_FLOAT, nnbrsmax);
  for(vtxno = 0; vtxno < surf->nvertices; vtxno++){
    area = MRISsumVertexFaceArea(surf, vtxno);
    nnbrs = surf->vertices[vtxno].vnum;
    //printf("%d %6.4f   ",vtxno,area);
    for(nthnbr = 0; nthnbr < nnbrs; nthnbr++){
      vtxnonbr = surf->vertices[vtxno].v[nthnbr];
      wtmp = MRISdiffusionEdgeWeight(surf, vtxno, vtxnonbr);
      MRIFseq_vox(w,vtxno,0,0,nthnbr) = (float)wtmp/area;
      //printf("%6.4f ",wtmp);
    }    
    //printf("\n");
    //MRISdumpVertexNeighborhood(surf,vtxno);
  }

  return(w);
}
/*----------------------------------------------------------------------*/
MRI *MRISdiffusionSmooth(MRIS *Surf, MRI *Src, double GStd, MRI *Targ)
{
  MRI *w, *SrcTmp;
  double FWHM;
  float wtmp,val,val0,valnbr;
  int vtxno, nthnbr, nbrvtxno, Niters, nthiter;
  double dt=1;

  if(Surf->nvertices != Src->width){
    printf("ERROR: MRISgaussianSmooth: Surf/Src dimension mismatch\n");
    return(NULL);
  }

  if(Targ == NULL){
    Targ = MRIallocSequence(Src->width, Src->height, Src->depth, 
            MRI_FLOAT, Src->nframes);
    if(Targ==NULL){
      printf("ERROR: MRISgaussianSmooth: could not alloc\n");
      return(NULL);
    }
  }
  else{
    if(Src->width   != Targ->width  || 
       Src->height  != Targ->height || 
       Src->depth   != Targ->depth  ||
       Src->nframes != Targ->nframes){
      printf("ERROR: MRISgaussianSmooth: output dimension mismatch\n");
      return(NULL);
    }
    if(Targ->type != MRI_FLOAT){
      printf("ERROR: MRISgaussianSmooth: structure passed is not MRI_FLOAT\n");
      return(NULL);
    }
  }

  /* Make a copy in case it's done in place */
  SrcTmp = MRIcopy(Src,NULL);

  /* Compute the weights */
  w = MRISdiffusionWeights(Surf);

  FWHM = GStd*sqrt(log(256.0));
  Niters = (int)(((FWHM*FWHM)/(16*log(2)))/dt);
  printf("Niters = %d, dt=%g, GStd = %g, FWHM = %g\n",Niters,dt,GStd,FWHM);
  for(nthiter = 0; nthiter < Niters; nthiter ++){
    //printf("Step = %d\n",nthiter); fflush(stdout);

    for(vtxno = 0; vtxno < Surf->nvertices; vtxno++){
      nnbrs = Surf->vertices[vtxno].vnum;

      for(frame = 0; frame < Targ->nframes; frame ++){
	val0 = MRIFseq_vox(SrcTmp,vtxno,0,0,frame);
	val = val0;
	//printf("%2d %5d %7.4f   ",nthiter,vtxno,val0);
	for(nthnbr = 0; nthnbr < nnbrs; nthnbr++){
	  nbrvtxno = Surf->vertices[vtxno].v[nthnbr];
	  valnbr = MRIFseq_vox(SrcTmp,nbrvtxno,0,0,frame) ;
	  wtmp = dt*MRIFseq_vox(w,vtxno,0,0,nthnbr);
	  val += wtmp*(valnbr-val0);
	  //printf("%6.4f ",wtmp);
	}/* end loop over neighbor */
	//printf("   %7.4f\n",val);

	MRIFseq_vox(Targ,vtxno,0,0,frame) = val;
      }/* end loop over frame */
      
    } /* end loop over vertex */
    
    MRIcopy(Targ,SrcTmp);
  }/* end loop over smooth step */
  
  MRIfree(&SrcTmp);
  MRIfree(&w);
  
  return(Targ);
}
/*-------------------------------------------------------------
  MRISareNeighbors() - tests whether two vertices are neighbors.
  -------------------------------------------------------------*/
int MRISareNeighbors(MRIS *surf, int vtxno1, int vtxno2)
{
  int nnbrs, nthnbr, nbrvtxno;
  nnbrs = surf->vertices[vtxno1].vnum;
  for(nthnbr = 0; nthnbr < nnbrs; nthnbr++){
    nbrvtxno = surf->vertices[vtxno1].v[nthnbr];
    if(nbrvtxno == vtxno2) return(1);
  }
  return(0);
}
/*-------------------------------------------------------------
  MRIScommonNeighbors() - returns the vertex numbers of the two 
  vertices that are common neighbors of the the two vertices 
  listed.
  -------------------------------------------------------------*/
int MRIScommonNeighbors(MRIS *surf, int vtxno1, int vtxno2, 
			int *cvtxno1, int *cvtxno2)
{
  int nnbrs, nthnbr, nbrvtxno;
  
  *cvtxno1 = -1;
  nnbrs = surf->vertices[vtxno1].vnum;
  for(nthnbr = 0; nthnbr < nnbrs; nthnbr++) {
    nbrvtxno = surf->vertices[vtxno1].v[nthnbr];
    if(nbrvtxno == vtxno2) continue;
    if(MRISareNeighbors(surf,nbrvtxno,vtxno2)){
      if(*cvtxno1 == -1) *cvtxno1 = nbrvtxno;
      else{
	*cvtxno2 = nbrvtxno;
	return(0);
      }
    }
  }
  return(0);
}
/*-------------------------------------------------------------------------
  MRISdiffusionEdgeWeight() - computes the unnormalized weight of an
  edge needed for diffusion-based smoothing on the surface. The actual
  weight must be divided by the area of all the traingles surrounding
  the center vertex. See Chung 2003. 
  ----------------------------------------------------------------------*/
double MRISdiffusionEdgeWeight(MRIS *surf, int vtxno0, int vtxnonbr)
{
  int cvtxno1=0, cvtxno2=0;
  VERTEX *v0, *vnbr, *cv1, *cv2;
  double a0, a1, btmp, ctmp, w;

  MRIScommonNeighbors(surf, vtxno0, vtxnonbr, &cvtxno1, &cvtxno2);

  v0   = &surf->vertices[vtxno0];
  vnbr = &surf->vertices[vtxnonbr];
  cv1  = &surf->vertices[cvtxno1];
  cv2  = &surf->vertices[cvtxno2];

  MRIStriangleAngles(cv1->x,cv1->y,cv1->z, v0->x,v0->y,v0->z,
		     vnbr->x,vnbr->y,vnbr->z, &a0,&btmp,&ctmp);
  MRIStriangleAngles(cv2->x,cv2->y,cv2->z, v0->x,v0->y,v0->z,
		     vnbr->x,vnbr->y,vnbr->z, &a1,&btmp,&ctmp);
  w = 1/tan(a0) + 1/tan(a1);
  return(w);
}
/*-------------------------------------------------------------------------
  MRISvertexSumFaceArea() - sum the area of the faces that the given
  vertex is part of. Make sure to run MRIScomputeMetricProperties()
  prior to calling this function.
  ----------------------------------------------------------------------*/
double MRISsumVertexFaceArea(MRIS *surf, int vtxno)
{
  int n, nfvtx;
  FACE *face;
  double area;

  area = 0;
  nfvtx = 0;
  for(n = 0; n < surf->nfaces; n++){
    face = &surf->faces[n];
    if(face->v[0] == vtxno || face->v[1] == vtxno || face->v[2] == vtxno){
      area += face->area;
      nfvtx ++;
    }
  }

  if(surf->vertices[vtxno].vnum != nfvtx){
    printf("ERROR: MRISsumVertexFaceArea: number of adjacent faces (%d) "
	   "does not equal number of neighbors (%d)\n",
	   nfvtx,surf->vertices[vtxno].vnum);
    exit(1);
  }

  return(area);
}
/*-------------------------------------------------------------------------
  MRISdumpVertexNeighborhood() 
  ----------------------------------------------------------------------*/
int MRISdumpVertexNeighborhood(MRIS *surf, int vtxno)
{
  int  n, nnbrs, nthnbr, nbrvtxno, nnbrnbrs, nthnbrnbr, nbrnbrvtxno;
  VERTEX *v0, *v;
  FACE *face;

  v0 = &surf->vertices[vtxno];
  nnbrs = surf->vertices[vtxno].vnum;

  printf("  seed vtx %d vc = [%6.3f %6.3f %6.3f], nnbrs = %d\n",vtxno,
	 v0->x,v0->y,v0->z,nnbrs);

  for(nthnbr = 0; nthnbr < nnbrs; nthnbr++) {
    nbrvtxno = surf->vertices[vtxno].v[nthnbr];
    v = &surf->vertices[nbrvtxno];
    printf("   nbr vtx %5d v%d = [%6.3f %6.3f %6.3f]    ",
	   nbrvtxno,nthnbr,v->x,v->y,v->z);

    nnbrnbrs = surf->vertices[nbrvtxno].vnum;
    for(nthnbrnbr = 0; nthnbrnbr < nnbrnbrs; nthnbrnbr++) {
      nbrnbrvtxno = surf->vertices[nbrvtxno].v[nthnbrnbr];
      if(MRISareNeighbors(surf,vtxno,nbrnbrvtxno))
      	printf("%5d ",nbrnbrvtxno);
    }
    printf("\n");
  }

  printf("   faces");  
  for(n = 0; n < surf->nfaces; n++){
    face = &surf->faces[n];
    if(face->v[0] == vtxno || face->v[1] == vtxno || face->v[2] == vtxno){
      printf("  %7.4f",face->area);

    }
  }
  printf("\n");

  return(0);
}

/*--------------------------------------------------------------------------*/
MRI *MRISheatkernel(MRIS *surf, double sigma)
{
  int vtxno, nbrvtxno, nnbrs, nnbrsmax;
  VERTEX *cvtx, *nbrvtx;
  double K, Ksum, two_sigma_sqr, dx, dy, dz, d2;
  MRI *hk;

  two_sigma_sqr = 2*pow(sigma,2);

  // Count the maximum number of neighbors
  nnbrsmax = 0;
  for(vtxno=0; vtxno < surf->nvertices; vtxno++){
    nnbrs = surf->vertices[vtxno].vnum;
    if(nnbrsmax < nnbrs) nnbrsmax = nnbrs;
  }

  printf("max no of neighbors = %d\n",nnbrsmax);
  hk = MRIallocSequence(surf->nvertices, 1, 1, MRI_FLOAT, nnbrsmax+1);

  printf("Filling in heat kernel weights\n");
  for(vtxno=0; vtxno < surf->nvertices; vtxno++){
    cvtx = &(surf->vertices[vtxno]);
    nnbrs = cvtx->vnum;
    Ksum = 0;
    for(nthnbr = 0; nthnbr < nnbrs; nthnbr++){
      nbrvtxno = cvtx->v[nthnbr];
      nbrvtx = &(surf->vertices[nbrvtxno]);
      dx = (cvtx->x - nbrvtx->x);
      dy = (cvtx->y - nbrvtx->y);
      dz = (cvtx->z - nbrvtx->z);
      d2 = dx*dx + dy*dy + dz*dz;
      K = exp(-d2/two_sigma_sqr);
      Ksum += K;
      MRIsetVoxVal(hk,vtxno,0,0,nthnbr,K);
    }
    for(nthnbr = 0; nthnbr < nnbrs; nthnbr++){
      K = MRIgetVoxVal(hk,vtxno,0,0,nthnbr);
      MRIsetVoxVal(hk,vtxno,0,0,nthnbr,K/(Ksum+1.0)); // +1 for self
    }
    MRIsetVoxVal(hk,vtxno,0,0,nnbrs,1/(Ksum+1.0));  // self
  }
  
  printf("Done computing heat kernel weights\n");

  MRIwrite(hk,"hk.mgh");
  return(hk);
}


/*--------------------------------------------------------------------------*/

MRI *MRIShksmooth(MRIS *Surf, MRI *Src, double sigma, int nSmoothSteps, MRI *Targ)
{
  int nnbrs, nthstep, frame, vtx, nbrvtx, nthnbr;
  double val, w;
  MRI *SrcTmp, *hk;
  
  if(Surf->nvertices != Src->width){
    printf("ERROR: MRIShksmooth: Surf/Src dimension mismatch\n");
    return(NULL);
  }
  
  if(Targ == NULL){
    Targ = MRIallocSequence(Src->width, Src->height, Src->depth, 
			    MRI_FLOAT, Src->nframes);
    if(Targ==NULL){
      printf("ERROR: MRIShksmooth: could not alloc\n");
      return(NULL);
    }
  }
  else{
    if(Src->width   != Targ->width  || 
       Src->height  != Targ->height || 
       Src->depth   != Targ->depth  ||
       Src->nframes != Targ->nframes){
      printf("ERROR: MRIShksmooth: output dimension mismatch\n");
      return(NULL);
    }
    if(Targ->type != MRI_FLOAT){
      printf("ERROR: MRIShksmooth: structure passed is not MRI_FLOAT\n");
      return(NULL);
    }
  }

  hk = MRISheatkernel(SrcSurfReg, sigma);
  MRIwrite(hk,"hk.mgh");
  
  SrcTmp = MRIcopy(Src,NULL);
  for(nthstep = 0; nthstep < nSmoothSteps; nthstep ++){
    //printf("Step = %d\n",nthstep); fflush(stdout);
    
    for(vtx = 0; vtx < Surf->nvertices; vtx++){
      nnbrs = Surf->vertices[vtx].vnum;
      
      for(frame = 0; frame < Targ->nframes; frame ++){
	w = MRIgetVoxVal(hk,vtx,0,0,nnbrs);
	val = w*MRIFseq_vox(SrcTmp,vtx,0,0,frame);
	
	for(nthnbr = 0; nthnbr < nnbrs; nthnbr++){
	  nbrvtx = Surf->vertices[vtx].v[nthnbr];
	  w = MRIgetVoxVal(hk,vtx,0,0,nthnbr);
	  val += w*MRIFseq_vox(SrcTmp,nbrvtx,0,0,frame) ;
	}/* end loop over neighbor */
	
	MRIFseq_vox(Targ,vtx,0,0,frame) = val;
      }/* end loop over frame */
      
    } /* end loop over vertex */
    
    MRIcopy(Targ,SrcTmp);
  }/* end loop over smooth step */
  
  MRIfree(&SrcTmp);
  
  return(Targ);
}


int DumpSurface(MRIS *surf, char *outfile)
{
  FILE *fp;
  int nnbrsmax, vtxno, nnbrs, nbrvtxno;
  VERTEX *cvtx;

  printf("Dumping surface to %s\n",outfile);

  // Count the maximum number of neighbors
  nnbrsmax = 0;
  for(vtxno=0; vtxno < surf->nvertices; vtxno++){
    nnbrs = surf->vertices[vtxno].vnum;
    if(nnbrsmax < nnbrs) nnbrsmax = nnbrs;
  }

  fp = fopen(outfile,"w");
  if(fp == NULL){
    printf("ERROR: cannot open %s for writing\n",outfile);
    return(1);
  }

  for(vtxno=0; vtxno < surf->nvertices; vtxno++){
    nnbrs = surf->vertices[vtxno].vnum;
    cvtx = &(surf->vertices[vtxno]);
    fprintf(fp,"%6d   %8.3f %8.3f %8.3f   %2d   ",
	    vtxno+1,cvtx->x,cvtx->y,cvtx->z,nnbrs);
    for(nthnbr = 0; nthnbr < nnbrsmax; nthnbr++){
      if(nthnbr < nnbrs) nbrvtxno = cvtx->v[nthnbr];
      else               nbrvtxno = -1;
      fprintf(fp,"%6d ",nbrvtxno+1);
    }
    fprintf(fp,"\n");
  }
  fclose(fp);

  printf("Done dumping surface\n");
  return(0);
}


