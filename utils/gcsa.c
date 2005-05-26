#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "mrisurf.h"
#include "annotation.h"
#include "mrishash.h"
#include "error.h"
#include "diag.h"
#include "utils.h"
#include "gcsa.h"
#include "proto.h"
#include "fio.h"
#include "transform.h"
#include "macros.h"
#include "utils.h"
#include "cma.h"
#include "icosahedron.h"
#include "colortab.h"
#include "tags.h"

#define BIG_AND_NEGATIVE            -10000000.0

static int edge_to_index(VERTEX *v, VERTEX *vn) ;
static int    MRIScomputeVertexPermutation(MRI_SURFACE *mris, int *indices) ;
static int    GCSAupdateNodeMeans(GCSA_NODE *gcsan, 
                                  int label, double *v_inputs, int ninputs) ;
static int    GCSAupdateNodeGibbsPriors(CP_NODE *cpn, int label, 
                                        MRI_SURFACE *mris, int vno) ;
static int    GCSAupdateNodeCovariance(GCSA_NODE *gcsan, int label, 
                                       double *v_inputs, int ninputs) ;
static int    GCSANclassify(GCSA_NODE *gcsa_node, CP_NODE *cpn,
                            double *v_inputs, 
                            int ninputs, double *pprob) ;
static double gcsaNbhdGibbsLogLikelihood(GCSA *gcsa, MRI_SURFACE *mris, 
                                         double *v_inputs, int vno, 
                                         double gibbs_coef,
                                         int label) ;
static double gcsaVertexGibbsLogLikelihood(GCSA *gcsa, MRI_SURFACE *mris, 
                                           double *v_inputs, int vno, 
                                           double gibbs_coef) ;
static int add_gc_to_gcsan(GCSA_NODE *gcsan_src, int nsrc, 
                           GCSA_NODE *gcsan_dst) ;
#if 0
static int add_cp_to_cpn(CP_NODE *cpn_src, int nsrc, CP_NODE *cpn_dst) ;
static GCSA_NODE *findClosestNode(GCSA *gcsa, int vno, int label) ;
#endif

/*static CP *getCP(CP_NODE *cpn, int label) ;*/
static GCS *getGC(GCSA_NODE *gcsan, int label, int *pn) ;
static int load_inputs(VERTEX *v, double *v_inputs, int ninputs) ;
static int fill_cpn_holes(GCSA *gcsa) ;
static int fill_gcsan_holes(GCSA *gcsa) ;

GCSA  *
GCSAalloc(int ninputs, int icno_priors, int icno_classifiers)
{
  char        fname[STRLEN], *cp ;
  int         i, n ;
  GCSA        *gcsa ;
  GCSA_NODE   *gcsan ;
  CP_NODE     *cpn ;
  GCS         *gcs ;
  double      max_len ;

  read_annotation_table() ;  /* for debugging */

  gcsa = (GCSA *)calloc(1, sizeof(GCSA)) ;
  if (!gcsa)
    ErrorExit(ERROR_NOMEMORY, "GCSAalloc(%d, %d, %d): could not allocate gcsa",
              ninputs, icno_priors, icno_classifiers) ;

  gcsa->icno_priors = icno_priors ;
  gcsa->icno_classifiers = icno_classifiers ;
  gcsa->ninputs = ninputs ;
  cp = getenv("FREESURFER_HOME") ;
  if (!cp)
    ErrorExit(ERROR_BADPARM,"%s: FREESURFER_HOME not defined in environment",Progname);
  /* generate a lower-res table for the classifiers */
  sprintf(fname, "%s/lib/bem/ic%d.tri", cp, icno_classifiers) ;
  gcsa->mris_classifiers = ICOread(fname) ; 
  if (!gcsa->mris_classifiers)
    ErrorExit(ERROR_NOMEMORY, "GCSAalloc(%d, %d, %d): could not read ico %s",
              ninputs, icno_classifiers, icno_priors, fname) ;
  MRISprojectOntoSphere(gcsa->mris_classifiers, gcsa->mris_classifiers, 
                        DEFAULT_RADIUS) ;


  gcsa->gc_nodes = (GCSA_NODE *)calloc(gcsa->mris_classifiers->nvertices, 
                                       sizeof(GCSA_NODE)) ;
  if (!gcsa->gc_nodes)
    ErrorExit(ERROR_NOMEMORY, "GCSAalloc(%d, %d): could not allocate nodes",
              ninputs, gcsa->mris_classifiers->nvertices) ;


#define DEFAULT_GCS  2

  for (i = 0 ; i < gcsa->mris_classifiers->nvertices ; i++)
  {
    if (i == Gdiag_no)
      DiagBreak() ;
    gcsan = &gcsa->gc_nodes[i] ;
    gcsan->max_labels = DEFAULT_GCS;
    gcsan->gcs = (GCS *)calloc(DEFAULT_GCS, sizeof(GCS)) ;
    if (!gcsan->gcs)
      ErrorExit(ERROR_NOMEMORY, "GCSAalloc(%d, %d): could not allocate gcs %d",
                ninputs, gcsa->mris_classifiers->nvertices, i) ;
    gcsan->labels = (int *)calloc(DEFAULT_GCS, sizeof(int)) ;
    if (!gcsan->labels)
      ErrorExit(ERROR_NOMEMORY, 
                "GCSAalloc(%d, %d): could not allocate labels %d",
                ninputs, gcsa->mris_classifiers->nvertices, i) ;
    for (n = 0 ; n < DEFAULT_GCS ; n++)
    {
      gcs = &gcsan->gcs[n] ;
      gcs->v_means = VectorAlloc(ninputs, MATRIX_REAL) ;
      gcs->m_cov = MatrixAlloc(ninputs, ninputs, MATRIX_REAL) ;
    }
  }

  /* allocate and fill in prior structs */
  sprintf(fname, "%s/lib/bem/ic%d.tri", cp, icno_priors) ;
  gcsa->mris_priors = ICOread(fname) ; 
  if (!gcsa->mris_priors)
    ErrorExit(ERROR_NOMEMORY, "GCSAalloc(%d, %d, %d): could not read ico %s",
              ninputs, icno_priors, icno_classifiers, fname) ;
  MRISprojectOntoSphere(gcsa->mris_priors, gcsa->mris_priors, DEFAULT_RADIUS) ;
  
  gcsa->cp_nodes = 
    (CP_NODE *)calloc(gcsa->mris_priors->nvertices, sizeof(CP_NODE)) ;
  if (!gcsa->cp_nodes)
    ErrorExit(ERROR_NOMEMORY, "GCSAalloc(%d, %d): could not allocate cp nodes",
              ninputs, gcsa->mris_priors->nvertices) ;
#define DEFAULT_CPS  1

  for (i = 0 ; i < gcsa->mris_priors->nvertices ; i++)
  {
    cpn = &gcsa->cp_nodes[i] ;
    cpn->max_labels = DEFAULT_CPS ;
    cpn->cps = (CP *)calloc(DEFAULT_CPS, sizeof(CP)) ;
    if (!cpn->cps)
      ErrorExit(ERROR_NOMEMORY, "GCSAalloc(%d, %d): could not allocate gcs %d",
                ninputs, gcsa->mris_priors->nvertices, i) ;
    cpn->labels = (int *)calloc(DEFAULT_CPS, sizeof(int)) ;
    if (!cpn->labels)
      ErrorExit(ERROR_NOMEMORY, 
                "GCSAalloc(%d, %d): could not allocate labels %d",
                ninputs, gcsa->mris_priors->nvertices, i) ;
  }


  MRIScomputeVertexSpacingStats(gcsa->mris_classifiers, NULL, NULL, &max_len, 
                                NULL,NULL);
  gcsa->mht_classifiers = 
    MHTfillVertexTableRes(gcsa->mris_classifiers, NULL, CURRENT_VERTICES,
                          2*max_len);

  MRIScomputeVertexSpacingStats(gcsa->mris_priors, NULL, NULL, &max_len, 
                                NULL,NULL);
  gcsa->mht_priors = 
    MHTfillVertexTableRes(gcsa->mris_priors, NULL, CURRENT_VERTICES,2*max_len);
  return(gcsa) ;
}

int
GCSAfree(GCSA **pgcsa)
{
  int       i, n ;
  GCSA      *gcsa ;
  GCSA_NODE *gcsan ;
  GCS       *gcs ;
  CP_NODE   *cpn ;

  gcsa = *pgcsa ; *pgcsa = NULL ;

  MHTfree(&gcsa->mht_classifiers) ; MHTfree(&gcsa->mht_priors) ;

  for (i = 0 ; i < gcsa->mris_priors->nvertices ; i++)
  {
    if (i == Gdiag_no)
      DiagBreak() ;
    cpn = &gcsa->cp_nodes[i] ;
    
    free(cpn->cps) ;
  }
  for (i = 0 ; i < gcsa->mris_classifiers->nvertices ; i++)
  {
    if (i == Gdiag_no)
      DiagBreak() ;
    gcsan = &gcsa->gc_nodes[i] ;
    
    for (n = 0 ; n < gcsan->nlabels ; n++)
    {
      gcs = &gcsan->gcs[n] ;
      VectorFree(&gcs->v_means) ; MatrixFree(&gcs->m_cov) ;
    }
    free(gcsan->gcs) ;
  }

  MRISfree(&gcsa->mris_classifiers) ; MRISfree(&gcsa->mris_priors) ;
  free(gcsa->cp_nodes) ; free(gcsa->gc_nodes) ; free(gcsa) ;
  return(NO_ERROR) ;
}

/*
  v->x,y,z     should have canonical coordinates in them and
  v->origx,y,z should have original coordinates.
  second fundamental form should have been computed on
*/
int
GCSAtrainMeans(GCSA *gcsa, MRI_SURFACE *mris)
{
  int        vno, vno_prior, vno_classifier ;
  VERTEX     *v, *v_prior, *v_classifier ;
  GCSA_NODE  *gcsan ;
  CP_NODE    *cpn ;
  double     v_inputs[100] ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (vno == Gdiag_no)
      DiagBreak() ;
    load_inputs(v, v_inputs, gcsa->ninputs) ;
    if (vno == Gdiag_no && v->annotation == 1336341)
      DiagBreak() ;
    if (v->annotation == 0)  /* not labeled */
      continue ;

    /* find the prior vertex */
    v_prior = GCSAsourceToPriorVertex(gcsa, v) ;
    vno_prior = v_prior - gcsa->mris_priors->vertices ;
    if (vno_prior == Gdiag_no)
      DiagBreak() ;
    cpn = &gcsa->cp_nodes[vno_prior] ;

    /* first update classifier statistics */
    v_classifier = GCSAsourceToClassifierVertex(gcsa, v_prior) ;
    vno_classifier = v_classifier - gcsa->mris_classifiers->vertices ;
    if (vno_classifier == Gdiag_no)
      DiagBreak() ;
    gcsan = &gcsa->gc_nodes[vno_classifier] ;
    GCSAupdateNodeMeans(gcsan, v->annotation, v_inputs, gcsa->ninputs) ;

    /* now update prior statistics */
    GCSAupdateNodeGibbsPriors(cpn, v->annotation, mris, vno) ;
  }
  return(NO_ERROR) ;
}

int
GCSAtrainCovariances(GCSA *gcsa, MRI_SURFACE *mris)
{
  int        vno, vno_classifier, vno_prior ;
  VERTEX     *v, *v_classifier, *v_prior ;
  GCSA_NODE  *gcsan ;
  double     v_inputs[100] ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (vno == Gdiag_no)
      DiagBreak() ;
    load_inputs(v, v_inputs, gcsa->ninputs) ;
    if (vno == Gdiag_no && v->annotation == 1336341)
      DiagBreak() ;
    if (v->annotation == 0)  /* not labeled */
      continue ;

    /* update class covariances */
    v_prior = GCSAsourceToPriorVertex(gcsa, v) ;
    vno_prior = v_prior - gcsa->mris_priors->vertices ;
    if (vno_prior == Gdiag_no)
      DiagBreak() ;
    v_classifier = GCSAsourceToClassifierVertex(gcsa, v_prior) ;
    vno_classifier = v_classifier - gcsa->mris_classifiers->vertices ;
    if (vno_classifier == Gdiag_no)
      DiagBreak() ;
    gcsan = &gcsa->gc_nodes[vno_classifier] ;
    GCSAupdateNodeCovariance(gcsan, v->annotation, v_inputs, gcsa->ninputs);
  }
  return(NO_ERROR) ;
}
VERTEX *
GCSAsourceToClassifierVertex(GCSA *gcsa, VERTEX *v)
{
  VERTEX *vdst ;

  vdst = MHTfindClosestVertex(gcsa->mht_classifiers, gcsa->mris_classifiers,v);
  return(vdst) ;
}
VERTEX *
GCSAsourceToPriorVertex(GCSA *gcsa, VERTEX *v)
{
  VERTEX *vdst ;

  vdst = MHTfindClosestVertex(gcsa->mht_priors, gcsa->mris_priors, v) ;
  return(vdst) ;
}

static int
GCSAupdateNodeMeans(GCSA_NODE *gcsan, int label, double *v_inputs, int ninputs)
{
  int   n, i ;
  GCS   *gcs ;

  if (label == 0)
    DiagBreak() ;

  for (n = 0 ; n < gcsan->nlabels ; n++)
  {
    if (gcsan->labels[n] == label)
      break ;
  }
  if (n >= gcsan->nlabels)  /* have to allocate a new classifier */
  {
    if (n >= gcsan->max_labels)
    {
      int   old_max_labels, i ;
      int   *old_labels ;
      GCS   *old_gcs ;

      if (gcsan->max_labels >= 6)
        DiagBreak() ;
      old_max_labels = gcsan->max_labels ; 
      gcsan->max_labels += 2 ;
      old_labels = gcsan->labels ;
      old_gcs = gcsan->gcs ;

      /* allocate new ones */
      gcsan->gcs = (GCS *)calloc(gcsan->max_labels, sizeof(GCS)) ;
      if (!gcsan->gcs)
        ErrorExit(ERROR_NOMEMORY, 
                  "GCANupdateNodeMeans: couldn't expand gcs to %d",
                  gcsan->max_labels) ;
      gcsan->labels = (int *)calloc(gcsan->max_labels, sizeof(int)) ;
      if (!gcsan->labels)
        ErrorExit(ERROR_NOMEMORY, 
                  "GCANupdateNodeMeans: couldn't expand labels to %d",
                  gcsan->max_labels) ;
      /* copy the old ones over */
      for (i = 0 ; i < old_max_labels ; i++)
      {
        gcsan->gcs[i].v_means = old_gcs[i].v_means ;
        gcsan->gcs[i].m_cov = old_gcs[i].m_cov ;
        gcsan->gcs[i].total_training = old_gcs[i].total_training ;
      }

      /* allocate new ones */
      for (i = old_max_labels ; i < gcsan->max_labels ; i++)
      {
        gcs = &gcsan->gcs[i] ;
        gcs->v_means = VectorAlloc(ninputs, MATRIX_REAL) ;
        gcs->m_cov = MatrixAlloc(ninputs, ninputs, MATRIX_REAL) ;
      }

      memmove(gcsan->labels, old_labels, old_max_labels*sizeof(int)) ;

      /* free the old ones */
      free(old_gcs) ; free(old_labels) ;
    }
    gcsan->nlabels++ ;
  }

  gcs = &gcsan->gcs[n] ;

  gcsan->labels[n] = label ;
  gcs->total_training++ ;
  gcsan->total_training++ ;

  /* these will be updated when training is complete */
  for (i = 0 ; i < ninputs ; i++)
    VECTOR_ELT(gcs->v_means, i+1) += v_inputs[i] ; 
  

  return(NO_ERROR) ;
}

int
GCSAnormalizeMeans(GCSA *gcsa)
{
  int        vno, n, total_gcs, total_cps ;
  GCSA_NODE  *gcsan ;
  GCS        *gcs ;

  for (total_gcs = vno = 0 ; vno < gcsa->mris_classifiers->nvertices ; vno++)
  {
    gcsan = &gcsa->gc_nodes[vno] ;
    total_gcs += gcsan->nlabels ;
    for (n = 0 ; n < gcsan->nlabels ; n++)
    {
      gcs = &gcsan->gcs[n] ;
      MatrixScalarMul(gcs->v_means, 
                      1.0/(float)gcs->total_training,gcs->v_means);

      /* the priors will get updated later */
    }
  }

  for (total_cps = vno = 0 ; vno < gcsa->mris_priors->nvertices ; vno++)
    total_cps += gcsa->cp_nodes[vno].nlabels ;

  printf("%d classifier means computed, %2.1f/vertex, %2.1f priors/vertex\n",
         total_gcs, (float)total_gcs/gcsa->mris_classifiers->nvertices,
         (float)total_cps/gcsa->mris_priors->nvertices) ;
  return(NO_ERROR) ;
}

static int
GCSAupdateNodeCovariance(GCSA_NODE *gcsan, int label, double *v_inputs, 
                         int ninputs)
{
  int   n, i, j ;
  GCS   *gcs ;
  double cov, mean1, mean2 ;

  for (n = 0 ; n < gcsan->nlabels ; n++)
  {
    if (gcsan->labels[n] == label)
      break ;
  }
  if (n >= gcsan->nlabels) 
  {
    dump_gcsan(gcsan, NULL, stderr, 0) ;
    ErrorExit(ERROR_BADPARM, "GCSAupdateNodeCovariance(%d): unknown label\n",
              label) ;
  }
  gcs = &gcsan->gcs[n] ;

  /* these will be updated when training is complete */
  for (i = 0 ; i < ninputs ; i++)
  {
    mean1 = VECTOR_ELT(gcs->v_means, i+1) ;
    for (j = 0 ; j <= i ; j++)
    {
      mean2 = VECTOR_ELT(gcs->v_means, j+1) ;
      cov = (v_inputs[i]-mean1) * (v_inputs[j]-mean2) ;
      *MATRIX_RELT(gcs->m_cov, i+1, j+1) += cov ;
      *MATRIX_RELT(gcs->m_cov, j+1, i+1) += cov ;
    }
  }
  
  return(NO_ERROR) ;
}

int
GCSAnormalizeCovariances(GCSA *gcsa)
{
  int        vno, n, cno, i, j ;
  GCSA_NODE  *gcsan ;
  GCS        *gcs ;
  CP_NODE    *cpn ;
  CP         *cp ;
  MATRIX     *m_inv ;
  double     mean, min_var ;

#define MIN_VAR 0.01

  for (vno = 0 ; vno < gcsa->mris_classifiers->nvertices ; vno++)
  {
    if (vno == Gdiag_no)
      DiagBreak() ;
    gcsan = &gcsa->gc_nodes[vno] ;
    for (n = 0 ; n < gcsan->nlabels ; n++)
    {
      gcs = &gcsan->gcs[n] ;
      if (gcs->total_training >= 2)
        MatrixScalarMul(gcs->m_cov, 1.0/(gcs->total_training-1.0f),gcs->m_cov);

      if (gcs->total_training < 5)    /* not that many samples - regularize */
      {
        for (i = 1 ; i <= gcsa->ninputs ; i++)
        {
          mean = VECTOR_ELT(gcs->v_means, i) ;
          min_var = (.1*mean * .1*mean) ;
          if (gcs->total_training > 1)
            min_var *= (gcs->total_training-1) ;
          if (min_var < MIN_VAR)
            min_var = MIN_VAR ;
          if (*MATRIX_RELT(gcs->m_cov, i, i) < min_var)
            *MATRIX_RELT(gcs->m_cov, i, i) = min_var ;
        }
      }
      else
      {
        for (i = 1 ; i <= gcsa->ninputs ; i++)
        {
          mean = VECTOR_ELT(gcs->v_means, i) ;
          min_var = mean*.1 ;
          if (min_var < MIN_VAR)
            min_var = MIN_VAR ;
          if (FZERO(*MATRIX_RELT(gcs->m_cov, i, i)))
            *MATRIX_RELT(gcs->m_cov, i, i) = min_var ;
        }
      }

      /* check to see if covariance is singular, and if so regularize it */
      m_inv = MatrixInverse(gcs->m_cov, NULL) ;
      cno = MatrixConditionNumber(gcs->m_cov) ;
      if (m_inv == NULL || (cno > 100) || (cno <= 0))
      {
        m_inv = MatrixIdentity(gcsa->ninputs, NULL) ;
        MatrixScalarMul(m_inv, 0.1, m_inv) ;
        MatrixAdd(m_inv, gcs->m_cov, gcs->m_cov) ;
      }
      MatrixFree(&m_inv) ;
      m_inv = MatrixIdentity(gcsa->ninputs, NULL) ;
      if (!m_inv)
      {
        fprintf(stderr, "vno %d, n = %d, m_cov is singular!\n",
                vno, n) ;
      }
      else
        MatrixFree(&m_inv) ;
    }
  }

  /* now normalize priors */
  for (vno = 0 ; vno < gcsa->mris_priors->nvertices ; vno++)
  {
    if (vno == Gdiag_no)
      DiagBreak() ;
    cpn = &gcsa->cp_nodes[vno] ;
    for (n = 0 ; n < cpn->nlabels ; n++)
    {
      cp = &cpn->cps[n] ;
      for (i = 0 ; i < GIBBS_SURFACE_NEIGHBORS ; i++)
      {
        for (j = 0 ; j < cp->nlabels[i] ; j++)
          cp->label_priors[i][j] /= (float)cp->total_nbrs[i] ;
      }
      cp->prior /= (float)cpn->total_training ;
    }
  }

  fill_cpn_holes(gcsa) ;
  fill_gcsan_holes(gcsa) ;

  return(NO_ERROR) ;
}
int
GCSAwrite(GCSA *gcsa, char *fname)
{
  FILE       *fp ;
  int        vno, n, i, j ;
  GCSA_NODE  *gcsan ;
  GCS        *gcs ;
  CP_NODE    *cpn ;
  CP         *cp ;

  fp = fopen(fname, "wb") ;
  if (!fp)
    ErrorReturn(ERROR_NOFILE, 
                (ERROR_NOFILE, "GCSAwrite(%s): could not open file", fname)) ;

  fwriteInt(GCSA_MAGIC, fp) ;
  fwriteInt(gcsa->ninputs, fp) ;
  fwriteInt(gcsa->icno_classifiers, fp) ;
  fwriteInt(gcsa->icno_priors, fp) ;

  for (i = 0 ; i < gcsa->ninputs ; i++)
  {
    fwriteInt(gcsa->inputs[i].type, fp) ;
    fwriteInt(strlen(gcsa->inputs[i].fname)+1, fp) ;
    fwrite(gcsa->inputs[i].fname, sizeof(char), 
           strlen(gcsa->inputs[i].fname)+1, fp) ;
    fwriteInt(gcsa->inputs[i].navgs, fp) ;
    fwriteInt(gcsa->inputs[i].flags, fp) ;
  }

  /* write out class statistics first */
  for (vno = 0 ; vno < gcsa->mris_classifiers->nvertices ; vno++)
  {
    if (vno == Gdiag_no)
      DiagBreak() ;
    gcsan = &gcsa->gc_nodes[vno] ;
    fwriteInt(gcsan->nlabels, fp) ;
    fwriteInt(gcsan->total_training, fp) ;
    
    for (n = 0 ; n < gcsan->nlabels ; n++)
    {
      gcs = &gcsan->gcs[n] ;
      fwriteInt(gcsan->labels[n], fp) ;
      fwriteInt(gcs->total_training, fp) ;
      MatrixAsciiWriteInto(fp, gcs->v_means) ;
      MatrixAsciiWriteInto(fp, gcs->m_cov) ;
    }
  }

  /* now write out prior info */
  for (vno = 0 ; vno < gcsa->mris_priors->nvertices ; vno++)
  {
    if (vno == Gdiag_no)
      DiagBreak() ;
    cpn = &gcsa->cp_nodes[vno] ;
    fwriteInt(cpn->nlabels, fp) ;
    fwriteInt(cpn->total_training, fp) ;
    
    for (n = 0 ; n < cpn->nlabels ; n++)
    {
      cp = &cpn->cps[n] ;
      fwriteInt(cpn->labels[n], fp) ;
      fwriteFloat(cp->prior, fp) ;
      
      for (i = 0 ; i < GIBBS_SURFACE_NEIGHBORS ; i++)
      {
        fwriteInt(cp->total_nbrs[i], fp) ;
        fwriteInt(cp->nlabels[i], fp) ;
        for (j = 0 ; j < cp->nlabels[i] ; j++)
        {
          fwriteInt(cp->labels[i][j], fp) ;
          fwriteFloat(cp->label_priors[i][j], fp) ;
        }
      }
    }
  }

	if (gcsa->ptable_fname)
	{
		COLOR_TABLE *ct ;
		ct = CTABread(gcsa->ptable_fname) ;
		if (ct)
		{
			fwriteInt(TAG_COLORTABLE, fp) ;
			CTABwriteInto(fp, ct) ;
			CTABfree(&ct) ;
		}
	}
  fclose(fp) ;
  return(NO_ERROR) ;
}

GCSA *
GCSAread(char *fname)
{
  FILE       *fp ;
  int        vno, n, ninputs, icno_classifiers, icno_priors, magic, i, j ;
  GCSA_NODE  *gcsan ;
  GCS        *gcs ;
  GCSA       *gcsa ;
  CP_NODE    *cpn ;
  CP         *cp ;
  
  fp = fopen(fname, "rb") ;
  if (!fp)
    ErrorReturn(NULL, 
                (ERROR_NOFILE, "GCSAread(%s): could not open file", fname)) ;

  magic = freadInt(fp) ;
  if (magic != GCSA_MAGIC)
    ErrorReturn(NULL,
                (ERROR_BADFILE, 
                 "GCSAread(%s): file magic #%x != GCSA_MAGIC (%x)", 
                 fname, magic, GCSA_MAGIC)) ;
  ninputs = freadInt(fp) ;
  icno_classifiers = freadInt(fp) ;
  icno_priors = freadInt(fp) ;

  gcsa = GCSAalloc(ninputs,icno_priors, icno_classifiers) ;

  for (i = 0 ; i < ninputs ; i++)
  {
    gcsa->inputs[i].type = freadInt(fp) ;
    j = freadInt(fp) ;
    fread(gcsa->inputs[i].fname, sizeof(char), j, fp) ;
    gcsa->inputs[i].navgs = freadInt(fp) ;
    gcsa->inputs[i].flags = freadInt(fp) ;
  }

  /* first read in class statistics */
  for (vno = 0 ; vno < gcsa->mris_classifiers->nvertices ; vno++)
  {
    if (vno == Gdiag_no)
      DiagBreak() ;
    gcsan = &gcsa->gc_nodes[vno] ;
    for (n = 0 ; n < gcsan->nlabels ; n++)
    {
      MatrixFree(&gcsan->gcs[n].v_means) ;
      MatrixFree(&gcsan->gcs[n].m_cov) ;
    }
    gcsan->max_labels = gcsan->nlabels = freadInt(fp) ;
    free(gcsan->labels) ; free(gcsan->gcs) ;
    gcsan->gcs = (GCS *)calloc(gcsan->nlabels, sizeof(GCS)) ;
    if (!gcsan->gcs)
      ErrorExit(ERROR_NOMEMORY, "GCSAread(%s): could not allocate gcs %d",
                fname, vno) ;
    gcsan->labels = (int *)calloc(gcsan->nlabels, sizeof(int)) ;
    if (!gcsan->labels)
      ErrorExit(ERROR_NOMEMORY, 
                "GCSAread(%s): could not allocate labels %d", fname, vno) ;
    
    gcsan->total_training = freadInt(fp) ;
    
    for (n = 0 ; n < gcsan->nlabels ; n++)
    {
      gcs = &gcsan->gcs[n] ;
      gcsan->labels[n] = freadInt(fp) ;
      gcs->total_training = freadInt(fp) ;
      gcs->v_means = MatrixAsciiReadFrom(fp, NULL) ;
      gcs->m_cov = MatrixAsciiReadFrom(fp, NULL) ;
    }
  }

  /* now read in prior info */
  for (vno = 0 ; vno < gcsa->mris_priors->nvertices ; vno++)
  {
    if (vno == Gdiag_no)
      DiagBreak() ;
    cpn = &gcsa->cp_nodes[vno] ;
    cpn->max_labels = cpn->nlabels = freadInt(fp) ;
    free(cpn->labels) ; free(cpn->cps) ;
    cpn->cps = (CP *)calloc(cpn->nlabels, sizeof(CP)) ;
    if (!cpn->cps)
      ErrorExit(ERROR_NOMEMORY, "GCSAread(%s): could not allocate cps %d",
                fname, vno) ;
    cpn->labels = (int *)calloc(cpn->nlabels, sizeof(int)) ;
    if (!cpn->labels)
      ErrorExit(ERROR_NOMEMORY, 
                "GCSAread(%s): could not allocate labels %d", fname, vno) ;
    
    cpn->total_training = freadInt(fp) ;
    
    for (n = 0 ; n < cpn->nlabels ; n++)
    {
      cp = &cpn->cps[n] ;
      cpn->labels[n] = freadInt(fp) ;
      cp->prior = freadFloat(fp) ;
      for (i = 0 ; i < GIBBS_SURFACE_NEIGHBORS ; i++)
      {
        cp->total_nbrs[i] = freadInt(fp) ;
        cp->nlabels[i] = freadInt(fp) ;
        cp->labels[i] = (int *)calloc(cp->nlabels[i], sizeof(int)) ;
        if (!cp->labels[i])
          ErrorExit(ERROR_NOMEMORY, 
                    "GCSAread: couldn't allocate labels to %d",
                    cp->nlabels[i]+1) ;

        cp->label_priors[i] = (float *)calloc(cp->nlabels[i], sizeof(float));
        if (!cp->label_priors[i])
          ErrorExit(ERROR_NOMEMORY, "GCSAread: couldn't allocate gcs to %d" ,
                    cp->nlabels[i]+1) ;
        for (j = 0 ; j < cp->nlabels[i] ; j++)
        {
          cp->labels[i][j] = freadInt(fp) ;
          cp->label_priors[i][j] = freadFloat(fp) ;
        }
      }
    }
  }

	while (!feof(fp))
	{
		int tag ;

		tag = freadInt(fp) ;
		switch (tag)
		{
		case TAG_COLORTABLE:
			printf("reading color table from GCSA file....\n") ;
			gcsa->ct = CTABreadFrom(fp) ;
			break ;
		default:
			break ;
		}
	}

  fclose(fp) ;
  return(gcsa) ;
}

static int Gvno = -1 ;
int
GCSAlabel(GCSA *gcsa, MRI_SURFACE *mris)
{
  int        vno, vno_classifier, label, vno_prior ;
  VERTEX     *v, *v_classifier, *v_prior ;
  GCSA_NODE  *gcsan ;
  CP_NODE    *cpn ;
  double     v_inputs[100], p ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (vno == Gdiag_no)
      DiagBreak() ;
    load_inputs(v, v_inputs, gcsa->ninputs) ;
    
    v_prior = GCSAsourceToPriorVertex(gcsa, v) ;
    vno_prior = v_prior - gcsa->mris_priors->vertices ;
    if (vno_prior == Gdiag_no)
      DiagBreak() ;
    v_classifier = GCSAsourceToClassifierVertex(gcsa, v_prior) ;
    vno_classifier = v_classifier - gcsa->mris_classifiers->vertices ;
    if (vno_classifier == Gdiag_no)
      DiagBreak() ;
    gcsan = &gcsa->gc_nodes[vno_classifier] ;

    cpn = &gcsa->cp_nodes[vno_prior] ;
    label = GCSANclassify(gcsan, cpn, v_inputs, gcsa->ninputs, &p) ;
    v->annotation = label ;
    if (vno == Gdiag_no)
		{
			int  n ;
			CP     *cp ;
			GCS    *gcs ;

			if (gcsa->ninputs == 2)
				printf("v %d: inputs (%2.2f, %2.2f), label=%s (%d, %d)\n",
							 vno, v->val, v->val2, annotation_to_name(label, NULL), annotation_to_index(label), label) ;
			for (n = 0 ; n < cpn->nlabels ; n++)
			{
				cp = &cpn->cps[n] ;
				gcs = getGC(gcsan, cpn->labels[n], NULL) ;
				printf("label %s (%d) [%d], means:\n", annotation_to_name(cpn->labels[n], NULL), n, cpn->labels[n]) ;
				MatrixPrint(stdout, gcs->v_means) ;
			}
		}
  }
  return(NO_ERROR) ;
}
static int
GCSANclassify(GCSA_NODE *gcsan, CP_NODE *cpn, double *v_inputs, int ninputs, 
              double *pprob)
{
  int    n, best_label, i ;
  double p, ptotal, max_p, det ;
  CP     *cp ;
  GCS    *gcs ;
  static MATRIX *m_cov_inv = NULL ;
  static VECTOR *v_tmp = NULL, *v_x = NULL ;

  if (v_x && ninputs != v_x->rows)
  {
    MatrixFree(&m_cov_inv) ; MatrixFree(&v_tmp) ;
    VectorFree(&v_x) ;
  }

  ptotal = 0.0 ; max_p = -10000 ; best_label = 0 ;
  for (n = 0 ; n < cpn->nlabels ; n++)
  {
    cp = &cpn->cps[n] ;
    gcs = getGC(gcsan, cpn->labels[n], NULL) ;
    if (!gcs)
      {
	ErrorPrintf(ERROR_BADPARM, "GCSANclassify: could not find GCS for node %d!",n) ;
	continue ;
      }

    v_x = VectorCopy(gcs->v_means, v_x) ;
    for (i = 0 ; i < ninputs ; i++)
      VECTOR_ELT(v_x, i+1) -= v_inputs[i] ;
    m_cov_inv = MatrixInverse(gcs->m_cov, m_cov_inv) ;
    if (!m_cov_inv)
    {
      MATRIX *m_tmp ;
      fprintf(stderr, "Singular matrix in GCSAclassify,vno=%d,n=%d:\n",Gvno,n);
      MatrixPrint(stderr, gcs->m_cov) ;
#if 0
      ErrorExit(ERROR_BADPARM, "") ;
#else
      m_tmp = MatrixIdentity(ninputs, NULL) ;
      MatrixScalarMul(m_tmp, 0.1, m_tmp) ;
      MatrixAdd(m_tmp, gcs->m_cov, m_tmp) ;
      m_cov_inv = MatrixInverse(m_tmp, NULL) ;
      if (!m_cov_inv)
      {
        ErrorExit(ERROR_BADPARM, "GCSANclassify: could not regularize matrix");
      }
#endif
    }
    v_tmp = MatrixMultiply(m_cov_inv, v_x, v_tmp) ;
    p = VectorDot(v_x, v_tmp) ;
    det = MatrixDeterminant(gcs->m_cov) ;
    p = cp->prior * exp(-0.5 * p) * 1.0/(sqrt(det)) ; ptotal += p ;
    if (p > max_p)
    {
      max_p = p ; best_label = cpn->labels[n] ;
    }
  }
  if (pprob)
    *pprob = max_p / ptotal ;
  return(best_label) ;
}

int
dump_gcsan(GCSA_NODE *gcsan, CP_NODE *cpn, FILE *fp, int verbose)
{
  int   n, index, i, j ;
  GCS   *gcs ;
  char  *name ;
  CP    *cp ;

  if (!cpn)
    return(NO_ERROR) ;

  fprintf(fp, "GCSAN with %d labels (%d training examples)\n", 
           cpn->nlabels, cpn->total_training) ;
  for (n = 0 ; n < cpn->nlabels ; n++)
  {
    cp = &cpn->cps[n] ;
    name = annotation_to_name(cpn->labels[n],&index) ;
    fprintf(fp, "  %d: label %s (%d, %d)\n", n, name, index,cpn->labels[n]) ;
    gcs = getGC(gcsan, cpn->labels[n], NULL) ;
    if (!gcs)
      ErrorPrintf(ERROR_BADPARM, 
                  "dump_gcsan: could not find GCS for node %d!",n) ;

    fprintf(fp, "\tprior %2.1f\n", cp->prior) ;

    if (gcs && gcs->v_means->rows > 1)
    {
      fprintf(fp, "\tmean:\n") ;       MatrixPrint(fp, gcs->v_means) ;
      fprintf(fp, "\tcovariance:\n") ; MatrixPrint(fp, gcs->m_cov) ;
    }
    else
    {
      fprintf(fp, "\tmean %2.2f +- %2.1f\n",
              VECTOR_ELT(gcs->v_means,1), sqrt(*MATRIX_RELT(gcs->m_cov,1,1))) ;
    }
    if (verbose)
    {
      for (i = 0 ; i < GIBBS_SURFACE_NEIGHBORS ; i++)
      {
        if (cp->nlabels[i] <= 0)
          continue ;
        fprintf(fp, "\tnbrs[%d] = %d\n", i, cp->nlabels[i]) ;
        for (j = 0 ; j < cp->nlabels[i] ; j++)
          fprintf(fp, "\t\tlabel %d, prior %2.1f\n", cp->labels[i][j],
                  cp->label_priors[i][j]) ;
      }
    }
  }
  return(NO_ERROR) ;
}
int
GCSAdump(GCSA *gcsa, int vno, MRI_SURFACE *mris, FILE *fp)
{
  int        vno_classifier, vno_prior ;
  VERTEX     *vclassifier, *v, *vprior ;
  GCSA_NODE  *gcsan ;
  CP_NODE    *cpn ;

  v = &mris->vertices[vno] ; v->tx = v->x ; v->ty = v->y ; v->tz = v->z ;
  v->x = v->cx ; v->y = v->cy ; v->z = v->cz ;
  vprior = GCSAsourceToPriorVertex(gcsa, v) ;
  vclassifier = GCSAsourceToClassifierVertex(gcsa, vprior) ;
  vno_classifier = vclassifier - gcsa->mris_classifiers->vertices ;
  vno_prior = vprior - gcsa->mris_priors->vertices ;
  gcsan = &gcsa->gc_nodes[vno_classifier] ;
  cpn = &gcsa->cp_nodes[vno_prior] ;
  fprintf(fp, "v %d --> vclassifier %d, vprior %d\n", 
          vno, vno_classifier, vno_prior) ;
  dump_gcsan(gcsan, cpn, fp, 0) ;
  v->x = v->tx ; v->y = v->ty ; v->z = v->tz ;
  return(NO_ERROR) ;
}


static int
GCSAupdateNodeGibbsPriors(CP_NODE *cpn,int label, MRI_SURFACE *mris, int vno)
{
  int     n, i, j, m, nbr_label ;
  CP      *cp ;
  VERTEX  *v, *vn ;

  if (label == 0)
    DiagBreak() ;

  for (n = 0 ; n < cpn->nlabels ; n++)
  {
    if (cpn->labels[n] == label)
      break ;
  }
  if (n >= cpn->nlabels)  /* have to allocate a new prior struct */
  {
    if (n >= cpn->max_labels)
    {
      int   old_max_labels, i ;
      int   *old_labels ;
      CP    *old_cps ;

      if (cpn->max_labels >= 6)
        DiagBreak() ;
      old_max_labels = cpn->max_labels ; 
      cpn->max_labels += 2 ;
      old_labels = cpn->labels ;
      old_cps = cpn->cps ;

      /* allocate new ones */
      cpn->cps = (CP *)calloc(cpn->max_labels, sizeof(CP)) ;
      if (!cpn->cps)
        ErrorExit(ERROR_NOMEMORY, 
                  "GCANupdateNodeGibbsPriors: couldn't expand cps to %d",
                  cpn->max_labels) ;
      cpn->labels = (int *)calloc(cpn->max_labels, sizeof(int)) ;
      if (!cpn->labels)
        ErrorExit(ERROR_NOMEMORY, 
                  "GCANupdateNodeGibbsPriors: couldn't expand labels to %d",
                  cpn->max_labels) ;
      /* copy the old ones over */
      for (i = 0 ; i < old_max_labels ; i++)
      {
        cp = &cpn->cps[i] ;
        cp->prior = old_cps[i].prior ;

        for (j = 0 ; j < GIBBS_SURFACE_NEIGHBORS ; j++)
        {
          cp->label_priors[j] = old_cps[i].label_priors[j] ;
          cp->labels[j] = old_cps[i].labels[j] ;
          cp->nlabels[j] = old_cps[i].nlabels[j] ;
          cp->total_nbrs[j] = old_cps[i].total_nbrs[j] ;
        }
      }

      memmove(cpn->labels, old_labels, old_max_labels*sizeof(int)) ;

      /* free the old ones */
      free(old_cps) ; free(old_labels) ;
    }
    cpn->labels[cpn->nlabels] = label ;
    cpn->nlabels++ ;
  }

  cpn->total_training++ ;
  cp = &cpn->cps[n] ;
  v = &mris->vertices[vno] ;
  for (m = 0 ; m < v->vnum ; m++)
  {
    vn = &mris->vertices[v->v[m]] ;
    i = edge_to_index(v, vn) ;
    cp->total_nbrs[i]++ ;
    nbr_label = vn->annotation ;
    
    /* see if this label already exists */
    for (n = 0 ; n < cp->nlabels[i] ; n++)
      if (cp->labels[i][n] == nbr_label)
        break ;
    if (n >= cp->nlabels[i])   /* not there - reallocate stuff */
    {
      int   *old_labels ;
      float *old_label_priors ;

      old_labels = cp->labels[i] ;
      old_label_priors = cp->label_priors[i] ;

      /* allocate new ones */
      cp->label_priors[i] = (float *)calloc(cp->nlabels[i]+1, sizeof(float));
      if (!cp->label_priors[i])
        ErrorExit(ERROR_NOMEMORY, "GCAupdateNodeGibbsPriors: "
                  "couldn't expand cp to %d" ,cp->nlabels[i]+1) ;
      cp->labels[i] = (int *)calloc(cp->nlabels[i]+1, sizeof(int)) ;
      if (!cp->labels[i])
        ErrorExit(ERROR_NOMEMORY, 
                  "GCSAupdateNodeGibbsPriors: couldn't expand labels to %d",
                  cp->nlabels[i]+1) ;

      if (cp->nlabels[i] > 0)  /* copy the old ones over */
      {
        memmove(cp->label_priors[i], old_label_priors, 
                cp->nlabels[i]*sizeof(float)) ;
        memmove(cp->labels[i], old_labels, cp->nlabels[i]*sizeof(int)) ;

        /* free the old ones */
        free(old_label_priors) ; free(old_labels) ;
      }
      cp->labels[i][cp->nlabels[i]++] = nbr_label ;
    }
    cp->label_priors[i][n] += 1.0f ;
  }
  cp->prior += 1.0f ;

  return(NO_ERROR) ;
}
static int 
edge_to_index(VERTEX *v, VERTEX *vn)
{
  float  dx, dy, dz, dot1, dot2, index, x_dist, y_dist, z_dist ;

  /* first find out which of the principal directions the edge connecting v 
     and vn is most closely parallel to */
  dx = vn->origx - v->origx ; dy = vn->origy - v->origy ; 
  dz = vn->origz - v->origz ;

  dot1 = dx*v->e1x + dy*v->e1y + dz*v->e1z ;
  dot2 = dx*v->e2x + dy*v->e2y + dz*v->e2z ;

  x_dist = fabs(1 - dx) ; y_dist = fabs(1 - dy) ; z_dist = fabs(1 - dz) ;
  if (fabs(dot1) > fabs(dot2))   /* 1st principal direction - 0 or 1 */
    index = 0 ;
  else   /* second principal direction - 1 or 2 */
    index = 2 ;

  return(index) ;
  if (x_dist > y_dist && x_dist > z_dist)   /* use sign of x */
    return(dx > 0 ? index : index+1) ;
  else if (y_dist > z_dist)
    return(dy > 0 ? index : index+1) ;
  else
    return(dz > 0 ? index : index+1) ;
}

#define MIN_CHANGED 0

int gcsa_write_iterations = 0 ;
char *gcsa_write_fname = NULL ;

int
GCSAreclassifyUsingGibbsPriors(GCSA *gcsa, MRI_SURFACE *mris)
{
  int       *indices ;
  int        n, vno, i, nchanged, label, best_label, old_label, vno_prior, 
             vno_classifier, niter, examined ;
  double     ll, max_ll ;
  VERTEX     *v, *v_classifier, *v_prior, *vn ;
  GCSA_NODE  *gcsan ;
  CP_NODE    *cpn ;
  double     v_inputs[100] ;

  indices = (int *)calloc(mris->nvertices, sizeof(int)) ;

  niter = 0 ;
  if (gcsa_write_iterations != 0)
  {
    char fname[STRLEN] ;
    sprintf(fname, "%s%03d.annot", gcsa_write_fname, niter) ;
    printf("writing snapshot to %s...\n", fname) ;
    MRISwriteAnnotation(mris, fname) ;
  }

  /* mark all vertices, so they will all be considered the first time through*/
  for (vno = 0 ; vno < mris->nvertices ; vno++)
    mris->vertices[vno].marked = 1 ;
  do
  {
    nchanged = 0 ; examined = 0 ;
    MRIScomputeVertexPermutation(mris, indices) ;
    for (i = 0 ; i < mris->nvertices ; i++)
    {
      vno = indices[i] ;
      v = &mris->vertices[vno] ;
      if (v->marked == 0)
        continue ;
      v->marked = 0 ;
      examined++ ;

      if (vno == Gdiag_no)
        DiagBreak() ;

      load_inputs(v, v_inputs, gcsa->ninputs) ;

      v_prior = GCSAsourceToPriorVertex(gcsa, v) ;
      vno_prior = v_prior - gcsa->mris_priors->vertices ;
      if (vno_prior == Gdiag_no)
        DiagBreak() ;
      cpn = &gcsa->cp_nodes[vno_prior] ;
      if (cpn->nlabels <= 1)
        continue ;

      v_classifier = GCSAsourceToClassifierVertex(gcsa, v_prior) ;
      vno_classifier = v_classifier - gcsa->mris_classifiers->vertices ;
      gcsan = &gcsa->gc_nodes[vno_classifier] ;
      if (vno_classifier == Gdiag_no)
        DiagBreak() ;

      best_label = old_label = v->annotation ; 
			if (vno == Gdiag_no)
				printf("reclassifying vertex %d...\n", vno) ;
      max_ll =gcsaNbhdGibbsLogLikelihood(gcsa,mris,v_inputs,vno,1.0,old_label);
      for (n = 0 ; n < cpn->nlabels ; n++)
      {
        label = cpn->labels[n] ;
        ll = gcsaNbhdGibbsLogLikelihood(gcsa, mris, v_inputs, vno, 1.0,label) ;
				if (vno == Gdiag_no)
					printf("\tlabel %s (%d, %d): ll=%2.3f\n", annotation_to_name(label, NULL), label, annotation_to_index(label), ll) ;
        if (ll > max_ll)
        {
          max_ll = ll ;
          best_label = label ;
					if (vno == Gdiag_no)
						printf("\tlabel %s NEW MAX\n", annotation_to_name(label, NULL)) ;
        }
      }
      if (best_label != old_label)
      {
				if (vno == Gdiag_no)
					printf("v %d: label changed from %s (%d) to %s (%d)\n",
								 vno, annotation_to_name(old_label, NULL), old_label, annotation_to_name(best_label, NULL), best_label) ;
        v->marked = 1 ;
        nchanged++ ; v->annotation = best_label ;
      }
    }
    printf("%03d: %6d changed, %d examined...\n", niter, nchanged, examined) ;
    niter++ ;
    if (gcsa_write_iterations && (niter % gcsa_write_iterations) == 0)
    {
      char fname[STRLEN] ;
      sprintf(fname, "%s%03d.annot", gcsa_write_fname, niter) ;
      printf("writing snapshot to %s...\n", fname) ;
      MRISwriteAnnotation(mris, fname) ;
    }
    for (vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v = &mris->vertices[vno] ; 
      if (v->marked != 1)
        continue ;
      for (n = 0 ; n < v->vnum ; n++)
      {
        vn = &mris->vertices[v->v[n]] ;
        if (vn->marked == 1)
          continue ;
        vn->marked = 2 ;
      }
    }
  } while (nchanged > MIN_CHANGED) ;
  free(indices) ;
  return(NO_ERROR) ;
}
int
MRIScomputeVertexPermutation(MRI_SURFACE *mris, int *indices)
{
  int i, index, tmp ;


  for (i = 0 ; i < mris->nvertices ; i++)
  {
    indices[i] = i ;
  }
  for (i = 0 ; i < mris->nvertices ; i++)
  {
    index = (int)randomNumber(0.0, (double)(mris->nvertices-0.0001)) ;
    tmp = indices[index] ; 
    indices[index] = indices[i] ; indices[i] = tmp ;
  }
  return(NO_ERROR) ;
}

static double
gcsaNbhdGibbsLogLikelihood(GCSA *gcsa, MRI_SURFACE *mris, double *v_inputs, 
                             int vno, double gibbs_coef, int label)
{
  double   total_ll, ll ;
  int      n, old_annotation ;
  VERTEX   *v ;

  v = &mris->vertices[vno] ;
  old_annotation = v->annotation ;
  v->annotation = label ;

  total_ll = gcsaVertexGibbsLogLikelihood(gcsa, mris, v_inputs, vno,
                                          gibbs_coef) ;

  for (n = 0 ; n < v->vnum ; n++)
  {
    ll = gcsaVertexGibbsLogLikelihood(gcsa, mris, v_inputs, v->v[n],
                                      gibbs_coef) ;
    total_ll += ll ;
  }

  v->annotation = old_annotation ;
  return(total_ll) ;
}
static double
gcsaVertexGibbsLogLikelihood(GCSA *gcsa, MRI_SURFACE *mris, double *v_inputs,
                            int vno, double gibbs_coef)
{
  double    ll, nbr_prior, det ;
  int       nbr_label, label, i,j, np, n, nc, vno_prior, vno_classifier ;
  GCSA_NODE *gcsan ;
  CP_NODE   *cpn ;
  CP        *cp ;
  GCS       *gcs ;
  VERTEX    *v, *v_prior, *v_classifier ;
  static MATRIX *m_cov_inv = NULL ;
  static VECTOR *v_tmp = NULL, *v_x = NULL ;

  if (v_x && gcsa->ninputs != v_x->cols)
  {
    MatrixFree(&m_cov_inv) ; MatrixFree(&v_tmp) ;
    VectorFree(&v_x) ;
  }

  v = &mris->vertices[vno] ;

  v_prior = GCSAsourceToPriorVertex(gcsa, v) ;
  vno_prior = v_prior - gcsa->mris_priors->vertices ;
  if (vno_prior == Gdiag_no)
    DiagBreak() ;
  cpn = &gcsa->cp_nodes[vno_prior] ;

  v_classifier = GCSAsourceToClassifierVertex(gcsa, v_prior) ;
  vno_classifier = v_classifier - gcsa->mris_classifiers->vertices ;
  if (vno_classifier == Gdiag_no)
    DiagBreak() ;
  gcsan = &gcsa->gc_nodes[vno_classifier] ;

  label = v->annotation ;

  for (np = 0 ; np < cpn->nlabels ; np++)
  {
    if (cpn->labels[np] == label)
      break ;
  }
  if (np >= cpn->nlabels)  /* never occured here */
    return(BIG_AND_NEGATIVE) ;

  for (nc = 0 ; nc < gcsan->nlabels ; nc++)
  {
    if (gcsan->labels[nc] == label)
      break ;
  }
  if (nc >= gcsan->nlabels)  /* never occured here */
    return(BIG_AND_NEGATIVE) ;

  gcs = &gcsan->gcs[nc] ;
  cp = &cpn->cps[np] ;
  
  /* compute Mahalanobis distance */
  v_x = VectorCopy(gcs->v_means, v_x) ;
  for (i = 0 ; i < gcsa->ninputs ; i++)
    VECTOR_ELT(v_x, i+1) -= v_inputs[i] ;
  m_cov_inv = MatrixInverse(gcs->m_cov, m_cov_inv) ;
  if (!m_cov_inv)
    ErrorExit(ERROR_BADPARM,
              "GCSAvertexLogLikelihood: could not invert matrix");

  det = MatrixDeterminant(gcs->m_cov) ;
  v_tmp = MatrixMultiply(m_cov_inv, v_x, v_tmp) ;
  ll = -0.5 *VectorDot(v_x, v_tmp) - 0.5 * log(det) ;

  nbr_prior = 0.0 ;
  for (n = 0 ; n < v->vnum ; n++)
  {
    nbr_label = mris->vertices[v->v[n]].annotation ;
    i = edge_to_index(v, &mris->vertices[v->v[n]]) ;
    for (j = 0 ; j < cp->nlabels[i] ; j++)
    {
      if (nbr_label == cp->labels[i][j])
        break ;
    }
    if (j < cp->nlabels[i])
    {
      if (!FZERO(cp->label_priors[i][j]))
        nbr_prior += log(cp->label_priors[i][j]) ;
      else
        nbr_prior += BIG_AND_NEGATIVE ;
    }
    else   /* never occurred - make it unlikely */
    {
      nbr_prior += BIG_AND_NEGATIVE ;
    }
  }
  ll += (gibbs_coef * nbr_prior + log(cp->prior)) ;

  return(ll) ;
}
#if 0
static CP *
getCP(CP_NODE *cpn, int label)
{
  int      n ;

  for (n = 0 ; n < cpn->nlabels ; n++)
    if (cpn->labels[n] == label)
      return(&cpn->cps[n]) ;
  return(NULL) ;
}
#endif

static GCS *
getGC(GCSA_NODE *gcsan, int label, int *pn)
{
  int      n ;

  for (n = 0 ; n < gcsan->nlabels ; n++)
    if (gcsan->labels[n] == label)
    {
      if (pn)
        *pn = n ;
      return(&gcsan->gcs[n]) ;
    }
  return(NULL) ;
}

int
GCSAreclassifyLabel(GCSA *gcsa, MRI_SURFACE *mris, LABEL *area)
{
  int        i, n, vno, annotation, best_label, max_ll, ll, nchanged, total ;
  VERTEX     *v, *vn ;
  double     v_inputs[100] ;

  total = 0 ;
  annotation = mris->vertices[area->lv[0].vno].annotation ;
  do
  {
    nchanged = 0 ;
    for (i = 0 ; i < area->n_points ; i++)
    {
      vno = area->lv[i].vno ;
      v = &mris->vertices[vno] ;
      
      if (vno == Gdiag_no)
        DiagBreak() ;

      if (v->ripflag || v->marked)
        continue ;

      load_inputs(v, v_inputs, gcsa->ninputs) ;
      max_ll = 10*BIG_AND_NEGATIVE ;
      best_label = v->annotation ;
      for (n = 0 ; n < v->vnum ; n++)
      {
        vn = &mris->vertices[v->v[n]] ;
        if (vn->annotation == annotation)
          continue ; ;
        ll = gcsaNbhdGibbsLogLikelihood(gcsa, mris, v_inputs, vno, 1.0,
                                        v->annotation) ;
        if (ll > max_ll || best_label == v->annotation)
        {
          max_ll = ll ;
          best_label = vn->annotation ;
        }
      }
      if (v->annotation != best_label)
      {
        v->annotation = best_label ;
        v->marked = 1 ;
        nchanged++ ;
      }
    }
    /*    printf("nchanged = %03d\n", nchanged) ;*/
    total += nchanged ;
  } while (nchanged > 0) ;
  return(total) ;
}

static int
load_inputs(VERTEX *v, double *v_inputs, int ninputs)
{
  v_inputs[0] = v->val ;
  if (ninputs > 1)
    v_inputs[1] = v->val2 ;
  if (ninputs > 2)
    v_inputs[2] = v->imag_val ;
  return(NO_ERROR) ;
}
#if 1
static int
fill_cpn_holes(GCSA *gcsa)
{
  int       min_n, vno, n, i, nholes, nfilled, x, y, z, vno_classifier ;
  double    dist, min_dist ;
  VERTEX    *v, *vn, *v_classifier ;
  CP_NODE   *cpn, *cpn_nbr ;
  CP        *cp, *cp_nbr ;
  GCSA_NODE *gcsan ;
  GCS       *gcs ;

  nholes = 0 ;
  do
  {
    nfilled = 0 ;
    for (vno = 0 ; vno < gcsa->mris_priors->nvertices ; vno++)
    {
      if (vno == Gdiag_no)
        DiagBreak() ;
      cpn = &gcsa->cp_nodes[vno] ;
      if (cpn->nlabels > 0)
        continue ;
      
      /* found an empty cpn - find nearest nbr with data and fill in */
      v = &gcsa->mris_priors->vertices[vno] ;
      x = v->x ; y = v->y ; z = v->z ;
      min_dist = 100000 ;  min_n = -1 ;
      for (n = 0 ; n < v->vnum ; n++)
      {
        cpn_nbr = &gcsa->cp_nodes[v->v[n]] ;
        if (cpn_nbr->nlabels == 0)
          continue ;
        vn = &gcsa->mris_priors->vertices[v->v[n]] ;
        dist = sqrt(SQR(vn->x-x)+SQR(vn->y-y)+SQR(vn->z-z)) ;
        if (dist < min_dist || min_n < 0)
        {
          min_n = n ;
          min_dist = dist ;
        }
      }
      if (min_n < 0)   /* none of the nbr have data either */
        continue ;
      nfilled++ ;
      cpn_nbr = &gcsa->cp_nodes[v->v[min_n]] ;
      free(cpn->labels) ; free(cpn->cps) ;
      *cpn = *cpn_nbr ;
      cpn->cps = (CP *)calloc(cpn->nlabels, sizeof(CP)) ;
      if (!cpn->cps)
        ErrorExit(ERROR_NOMEMORY,"fill_cpn_holes(): could not allocate cps %d",
                  vno) ;
      cpn->labels = (int *)calloc(cpn->nlabels, sizeof(int)) ;
      if (!cpn->labels)
        ErrorExit(ERROR_NOMEMORY, 
                  "fill_cpn_holes(): could not allocate labels %d",
                  vno) ;
      cpn->max_labels = cpn->nlabels ;
      v_classifier = GCSAsourceToClassifierVertex(gcsa, v) ;
      vno_classifier = v_classifier - gcsa->mris_classifiers->vertices ;
      if (vno_classifier == Gdiag_no)
        DiagBreak() ;
      gcsan = &gcsa->gc_nodes[vno_classifier] ;
      for (n = 0 ; n < cpn->nlabels ; n++)
      {
        cp = &cpn->cps[n] ; cp_nbr = &cpn_nbr->cps[n] ;
        cpn->labels[n] = cpn_nbr->labels[n] ;
        *cp = *cp_nbr ;
        for (i = 0 ; i < GIBBS_SURFACE_NEIGHBORHOOD ; i++)
        {
          if (cp->nlabels[i])
          {
            cp->label_priors[i] = 
              (float *)calloc(cp->nlabels[i], sizeof(float));
            if (!cp->label_priors[i])
              ErrorExit(ERROR_NOMEMORY, "fill_cpn_holes: "
                        "couldn't expand cp to %d" ,cp->nlabels[i]) ;
            cp->labels[i] = (int *)calloc(cp->nlabels[i]+1, sizeof(int)) ;
            if (!cp->labels[i])
              ErrorExit(ERROR_NOMEMORY, 
                        "fill_cpn_holes: couldn't expand labels to %d",
                        cp->nlabels[i]) ;

            memmove(cp->label_priors[i], cp_nbr->label_priors[i], 
                    cp->nlabels[i]*sizeof(float)) ;
            memmove(cp->labels[i], cp_nbr->labels[i], 
                    cp->nlabels[i]*sizeof(int)) ;
          }
        }
        gcs = getGC(gcsan, cpn->labels[n], NULL) ;
        if (!gcs) /* couldn't find one for this label - fix */
        {
          GCSA_NODE *gcsan_nbr ;
          int       i, vno_nbr_classifier ;
          VERTEX    *vn, *vn_classifier ;

          vn = &gcsa->mris_priors->vertices[v->v[min_n]] ;
          vn_classifier = GCSAsourceToClassifierVertex(gcsa, vn) ;
          vno_nbr_classifier = vn_classifier-gcsa->mris_classifiers->vertices;
          if (vno_nbr_classifier == Gdiag_no)
            DiagBreak() ;
          
          gcsan_nbr = &gcsa->gc_nodes[vno_nbr_classifier] ;
          gcs = getGC(gcsan_nbr, cpn->labels[n], &i) ;
          if (!gcs)
            ErrorPrintf(ERROR_BADPARM, "fill_cpn_holes: could not find cp!\n");
          else
            add_gc_to_gcsan(gcsan_nbr, i, gcsan) ;
        }
      }
    }
    nholes += nfilled ;
  } while (nfilled > 0) ;
  printf("%d holes filled in class prior array\n", nholes) ;
  return(NO_ERROR) ;
}

static int
fill_gcsan_holes(GCSA *gcsa)
{
  int        min_n, vno, n, nholes, nfilled, x, y, z ;
  double     dist, min_dist ;
  VERTEX     *v, *vn ;
  GCSA_NODE  *gcsan, *gcsan_nbr ;
  GCS        *gcs, *gcs_nbr ;

  nholes = 0 ;
  do
  {
    nfilled = 0 ;
    for (vno = 0 ; vno < gcsa->mris_classifiers->nvertices ; vno++)
    {
      if (vno == Gdiag_no)
        DiagBreak() ;
      gcsan = &gcsa->gc_nodes[vno] ;
      if (gcsan->nlabels > 0)
        continue ;
      
      /* found an empty gcsan - find nearest nbr with data and fill in */
      v = &gcsa->mris_classifiers->vertices[vno] ;
      x = v->x ; y = v->y ; z = v->z ;
      min_dist = 100000 ;  min_n = -1 ;
      for (n = 0 ; n < v->vnum ; n++)
      {
        gcsan_nbr = &gcsa->gc_nodes[v->v[n]] ;
        if (gcsan_nbr->nlabels == 0)
          continue ;
        vn = &gcsa->mris_priors->vertices[v->v[n]] ;
        dist = sqrt(SQR(vn->x-x)+SQR(vn->y-y)+SQR(vn->z-z)) ;
        if (dist < min_dist || min_n < 0)
        {
          min_n = n ;
          min_dist = dist ;
        }
      }
      if (min_n < 0)   /* none of the nbr have data either */
        continue ;
      nfilled++ ;
      gcsan_nbr = &gcsa->gc_nodes[v->v[min_n]] ;
      for (n = 0 ; n < gcsan->max_labels ; n++)
      {
        MatrixFree(&gcsan->gcs[n].m_cov) ;
        MatrixFree(&gcsan->gcs[n].v_means) ;
      }
      free(gcsan->labels) ; free(gcsan->gcs) ;
      *gcsan = *gcsan_nbr ;
      gcsan->gcs = (GCS *)calloc(gcsan->nlabels, sizeof(GCS)) ;
      if (!gcsan->gcs)
        ErrorExit(ERROR_NOMEMORY,
                  "fill_gcsan_holes(): could not allocate gcs %d",
                  vno) ;
      gcsan->labels = (int *)calloc(gcsan->nlabels, sizeof(int)) ;
      if (!gcsan->labels)
        ErrorExit(ERROR_NOMEMORY, 
                  "fill_gcsan_holes(): could not allocate labels %d",
                  vno) ;
      gcsan->max_labels = gcsan->nlabels ;
      for (n = 0 ; n < gcsan->nlabels ; n++)
      {
        gcsan->labels[n] = gcsan_nbr->labels[n] ;
        gcs = &gcsan->gcs[n] ; gcs_nbr = &gcsan_nbr->gcs[n] ;
        *gcs = *gcs_nbr ;
        gcs->m_cov = MatrixCopy(gcs_nbr->m_cov, NULL) ;
        gcs->v_means = MatrixCopy(gcs_nbr->v_means, NULL) ;
      }
    }
    nholes += nfilled ;
  } while (nfilled > 0) ;
  printf("%d holes filled in gcsan array\n", nholes) ;
  return(NO_ERROR) ;
}
#else
static int
fill_cpn_holes(GCSA *gcsa)
{
  int      vno, n, nholes, nfilled, filled, i ;
  VERTEX   *v ;
  CP_NODE  *cpn, *cpn_nbr ;

  nholes = 0 ;
  do
  {
    nfilled = 0 ;
    for (vno = 0 ; vno < gcsa->mris_priors->nvertices ; vno++)
    {
      if (vno == Gdiag_no)
        DiagBreak() ;
      if (Gdiag_no == -111)
        free(gcsa->cp_nodes[Gdiag_no].cps) ;
      cpn = &gcsa->cp_nodes[vno] ;
      if (cpn->nlabels > 0)
        continue ;
      
      if (fabs(cpn->total_training) > 1000)
        DiagBreak() ;
      /* found an empty cpn - find nearest nbr with data and fill in */
      v = &gcsa->mris_priors->vertices[vno] ;
      for (filled = n = 0 ; n < v->vnum ; n++)
      {
        cpn_nbr = &gcsa->cp_nodes[v->v[n]] ;
        if (cpn_nbr->nlabels == 0)
          continue ;
        if (v->v[n] == Gdiag_no)
          DiagBreak() ;
        filled++ ;
        if (cpn_nbr->total_training <= 0)
          DiagBreak() ;
        if (fabs(cpn_nbr->total_training) > 1000)
          DiagBreak() ;
        for (i = 0 ; i < cpn_nbr->nlabels ; i++)
          add_cp_to_cpn(cpn_nbr, i, cpn) ;
        if (fabs(cpn_nbr->total_training) > 1000)
          DiagBreak() ;
      }
      if (fabs(cpn->total_training) > 1000)
        DiagBreak() ;
      if (filled && cpn->total_training == 0)
        DiagBreak() ;
      if (filled > 0)
      {
        cpn->total_training /= (float)filled ;
        nfilled++ ;
      }
    }
    nholes += nfilled ;
  } while (nfilled > 0) ;
  printf("%d holes filled in class prior array\n", nholes) ;
  return(NO_ERROR) ;
}

static int
fill_gcsan_holes(GCSA *gcsa)
{
  int        vno, n, i, nholes, nfilled, filled ;
  VERTEX     *v ;
  GCSA_NODE  *gcsan, *gcsan_nbr ;

  nholes = 0 ;
  do
  {
    nfilled = 0 ;
    for (vno = 0 ; vno < gcsa->mris_classifiers->nvertices ; vno++)
    {
      if (vno == Gdiag_no)
        DiagBreak() ;
      gcsan = &gcsa->gc_nodes[vno] ;
      if (gcsan->nlabels > 0)
        continue ;
      
      /* found an empty gcsan - find nearest nbr with data and fill in */
      v = &gcsa->mris_classifiers->vertices[vno] ;
      for (filled = n = 0 ; n < v->vnum ; n++)
      {
        gcsan_nbr = &gcsa->gc_nodes[v->v[n]] ;
        if (gcsan_nbr->nlabels == 0)
          continue ;
        filled++ ;
        for (i = 0 ; i < gcsan_nbr->nlabels ; i++)
          add_gc_to_gcsan(gcsan_nbr, i, gcsan) ;
      }
      if (filled)
        nfilled++ ;
    }
    nholes += nfilled ;
  } while (nfilled > 0) ;
  printf("%d holes filled in gcsan array\n", nholes) ;
  return(NO_ERROR) ;
}
#endif

static int
add_gc_to_gcsan(GCSA_NODE *gcsan_src, int nsrc, GCSA_NODE *gcsan_dst)
{
  int ndst ;

  for (ndst = 0 ; ndst < gcsan_dst->nlabels ; ndst++)
    if (gcsan_dst->labels[ndst] == gcsan_src->labels[nsrc])
      break ;

  if (ndst >= gcsan_dst->nlabels)   /* reallocate stuff */
  {
    int *old_labels ;
    GCS  *old_gcs ;

    gcsan_dst->nlabels++ ;
    old_gcs = gcsan_dst->gcs ;
    old_labels = gcsan_dst->labels ;
    gcsan_dst->labels = (int *)calloc(gcsan_dst->nlabels, sizeof(int)) ;
    if (!gcsan_dst->labels)
      ErrorExit(ERROR_NOMEMORY, "add_gcs_gcsan: could not resize GCSAN") ;
    gcsan_dst->gcs = (GCS *)calloc(gcsan_dst->nlabels, sizeof(GCS)) ;
    if (!gcsan_dst->gcs)
      ErrorExit(ERROR_NOMEMORY, "add_gcs_gcsan: could not resize GCSAN") ;
    if (old_labels)
    { 
      int n ;

      for (n = gcsan_dst->nlabels ; n < gcsan_dst->max_labels ; n++)
      {
        MatrixFree(&old_gcs[n].m_cov) ;
        MatrixFree(&old_gcs[n].v_means) ;
      }
      memmove(gcsan_dst->labels, old_labels, 
              (gcsan_dst->nlabels-1)*sizeof(int)) ;
      free(old_labels) ;
      
      memmove(gcsan_dst->gcs, old_gcs, (gcsan_dst->nlabels-1)*sizeof(GCS)) ;
      free(old_gcs) ;
    }
    gcsan_dst->labels[ndst] = gcsan_src->labels[nsrc] ;
    memmove(&gcsan_dst->gcs[ndst], &gcsan_src->gcs[nsrc], sizeof(GCS)) ;
    gcsan_dst->max_labels = gcsan_dst->nlabels ;
    gcsan_dst->gcs[ndst].m_cov = MatrixCopy(gcsan_src->gcs[nsrc].m_cov, NULL);
    gcsan_dst->gcs[ndst].v_means = MatrixCopy(gcsan_src->gcs[nsrc].v_means, 
                                              NULL);
    gcsan_dst->max_labels = gcsan_dst->nlabels ;
  }
  else   /* add to existing GCS */
  {
    GCS  *gcs_src, *gcs_dst ;
    MATRIX  *m_src, *m_dst ;

    gcs_src = &gcsan_src->gcs[nsrc] ; gcs_dst = &gcsan_dst->gcs[ndst] ;
    m_src = MatrixScalarMul(gcs_src->v_means, gcs_src->total_training, NULL) ;
    m_dst = MatrixScalarMul(gcs_dst->v_means, gcs_dst->total_training, NULL) ;
    MatrixAdd(m_src, m_dst, gcs_dst->v_means) ;
    MatrixFree(&m_src) ; MatrixFree(&m_dst) ;

    m_src = MatrixScalarMul(gcs_src->m_cov, gcs_src->total_training-1, NULL) ;
    m_dst = MatrixScalarMul(gcs_dst->m_cov, gcs_dst->total_training-1, NULL) ;
    MatrixAdd(m_src, m_dst, gcs_dst->m_cov) ;
    MatrixFree(&m_src) ; MatrixFree(&m_dst) ;

    /* normalize them */
    gcs_dst->total_training += gcs_src->total_training ;
    MatrixScalarMul(gcs_dst->v_means, 1.0 / (float)gcs_dst->total_training,
                    gcs_dst->v_means) ;
    MatrixScalarMul(gcs_dst->m_cov, 1.0 / ((float)gcs_dst->total_training-1),
                    gcs_dst->m_cov) ;
  }

  gcsan_dst->total_training += gcsan_src->total_training ;
  return(NO_ERROR) ;
}
#if 0
static int
add_cp_to_cpn(CP_NODE *cpn_src, int nsrc, CP_NODE *cpn_dst)
{
  int ndst, i, ntraining ;
  CP  *cp_src, *cp_dst ;


  for (ndst = 0 ; ndst < cpn_dst->nlabels ; ndst++)
    if (cpn_dst->labels[ndst] == cpn_src->labels[nsrc])
      break ;

  ntraining = nint(cpn_src->cps[nsrc].prior * cpn_src->total_training) ;
  if (ndst >= cpn_dst->nlabels)   /* reallocate stuff */
  {
    int *old_labels ;
    CP  *old_cps ;

    cpn_dst->nlabels++ ;
    old_cps = cpn_dst->cps ;
    old_labels = cpn_dst->labels ;
    cpn_dst->labels = (int *)calloc(cpn_dst->nlabels, sizeof(int)) ;
    if (!cpn_dst->labels)
      ErrorExit(ERROR_NOMEMORY, "add_cp_cpn: could not resize CPN") ;
    cpn_dst->cps = (CP *)calloc(cpn_dst->nlabels, sizeof(CP)) ;
    if (!cpn_dst->cps)
      ErrorExit(ERROR_NOMEMORY, "add_cp_cpn: could not resize CPN") ;
    if (old_labels)
    {
      memmove(cpn_dst->labels, old_labels, (cpn_dst->nlabels-1)*sizeof(int)) ;
      free(old_labels) ;
      
      memmove(cpn_dst->cps, old_cps, (cpn_dst->nlabels-1)*sizeof(CP)) ;
      free(old_cps) ;
    }
    cpn_dst->labels[ndst] = cpn_src->labels[nsrc] ;
    memmove(&cpn_dst->cps[ndst], &cpn_src->cps[nsrc], sizeof(CP)) ;
    cpn_dst->max_labels = cpn_dst->nlabels ;
    cpn_dst->cps[ndst].prior = 
      cpn_src->total_training * cpn_src->cps[nsrc].prior /
      (cpn_src->total_training + cpn_dst->total_training) ;
    cp_src = &cpn_src->cps[nsrc] ; cp_dst = &cpn_dst->cps[ndst] ;
    for (i = 0 ; i < GIBBS_SURFACE_NEIGHBORHOOD ; i++)
    {
      if (cp_src->nlabels[i] > 0)
      {
        cp_dst->label_priors[i] = 
          (float *)calloc(cp_src->nlabels[i], sizeof(float));
        if (!cp_dst->label_priors[i])
          ErrorExit(ERROR_NOMEMORY, "add_cp_to_cpn: "
                    "couldn't expand cp to %d" ,cp_dst->nlabels[i]) ;
        cp_dst->labels[i] = (int *)calloc(cp_dst->nlabels[i], sizeof(int)) ;
        if (!cp_dst->labels[i])
          ErrorExit(ERROR_NOMEMORY, 
                    "add_cp_to_cpn: couldn't expand labels to %d",
                    cp_dst->nlabels[i]) ;

        memmove(cp_dst->label_priors[i], cp_src->label_priors[i], 
                cp_dst->nlabels[i]*sizeof(float)) ;
        memmove(cp_dst->labels[i], cp_src->labels[i], 
                cp_dst->nlabels[i]*sizeof(int)) ;
      }
    }
  }
  else   /* add to existing CP */
  {
    int jsrc, jdst ;

    cp_src = &cpn_src->cps[nsrc] ; cp_dst = &cpn_dst->cps[ndst] ;
    cp_dst->prior = 
      (cpn_src->total_training * cp_src->prior + 
       cpn_dst->total_training * cp_dst->prior) /
      (cpn_src->total_training + cpn_dst->total_training) ;

    /* update nbrhood priors */
    for (i = 0 ; i < GIBBS_SURFACE_NEIGHBORHOOD ; i++)
    {
      for (jsrc = 0 ; jsrc < cp_src->nlabels[i] ; jsrc++)
      {
        for (jdst = 0 ; jdst < cp_dst->nlabels[i] ; jdst++)
          if (cp_src->labels[i][jsrc] == cp_dst->labels[i][jdst])
            break ;

        if (jdst >= cp_dst->nlabels[i])   /* new one - allocate it */
        {
          int   *old_labels ;
          float *old_label_priors ;

          old_labels = cp_dst->labels[i] ;
          old_label_priors = cp_dst->label_priors[i] ;

          /* allocate new ones */
          cp_dst->label_priors[i] = 
            (float *)calloc(cp_dst->nlabels[i]+1, sizeof(float));
          if (!cp_dst->label_priors[i])
            ErrorExit(ERROR_NOMEMORY, "add_cp_to_cpn: "
                      "couldn't expand cp to %d" ,cp_dst->nlabels[i]+1) ;
          cp_dst->labels[i] = (int *)calloc(cp_dst->nlabels[i]+1, sizeof(int));
          if (!cp_dst->labels[i])
            ErrorExit(ERROR_NOMEMORY, 
                      "add_cp_to_cpn: couldn't expand labels to %d",
                      cp_dst->nlabels[i]+1) ;

          if (cp_dst->nlabels[i] > 0)  /* copy the old ones over */
          {
            memmove(cp_dst->label_priors[i], old_label_priors, 
                    cp_dst->nlabels[i]*sizeof(float)) ;
            memmove(cp_dst->labels[i], old_labels, 
                    cp_dst->nlabels[i]*sizeof(int));

            /* free the old ones */
            free(old_label_priors) ; free(old_labels) ;
          }
          cp_dst->labels[i][cp_dst->nlabels[i]++] = cp_src->labels[i][jsrc] ;
        }
        cp_dst->label_priors[i][jdst] = 
          ((cp_src->label_priors[i][jsrc] * cp_src->total_nbrs[i]) +
           (cp_dst->label_priors[i][jdst] * cp_dst->total_nbrs[i])) /
          (cp_src->total_nbrs[i] + cp_dst->total_nbrs[i]) ;
        cp_dst->total_nbrs[i] += cp_src->total_nbrs[i] ;
      }
    }
  }

  cpn_dst->total_training += ntraining ;
  return(NO_ERROR) ;
}

static GCSA_NODE *
findClosestNode(GCSA *gcsa, int vno, int label)
{
  GCSA_NODE  *gcsan_nbr ;
  int        n, min_n ;
  VERTEX     *v, *vn ;
  float      x, y, z, min_dist, dist ;


  v = &gcsa->mris_classifiers->vertices[vno] ;
  x = v->x ; y = v->y ; z = v->z ;
  min_dist = 100000 ;  min_n = -1 ;
  for (n = 0 ; n < v->vtotal ; n++)
  {
    gcsan_nbr = &gcsa->gc_nodes[v->v[n]] ;
    if (getGC(gcsan_nbr, label, NULL) == NULL)
      continue ;
    vn = &gcsa->mris_priors->vertices[v->v[n]] ;
    dist = sqrt(SQR(vn->x-x)+SQR(vn->y-y)+SQR(vn->z-z)) ;
    if (dist < min_dist || min_n < 0)
    {
      min_n = n ;
      min_dist = dist ;
    }
  }
  if (min_n < 0) /* search whole surface */
  {
    min_dist = 100000 ;
    for (n = 0 ; n < gcsa->mris_classifiers->nvertices ; n++)
    {
      gcsan_nbr = &gcsa->gc_nodes[n] ;
      if (getGC(gcsan_nbr, label, NULL) == NULL)
        continue ;
      vn = &gcsa->mris_classifiers->vertices[n] ;
      dist = sqrt(SQR(vn->x-x)+SQR(vn->y-y)+SQR(vn->z-z)) ;
      if (dist < min_dist || min_n < 0)
      {
        min_n = n ;
        min_dist = dist ;
      }
    }
  }
  else
    gcsan_nbr = &gcsa->gc_nodes[v->v[min_n]] ;
  return(gcsan_nbr) ;
}

#endif
int
GCSAputInputType(GCSA *gcsa, int type, char *fname, int navgs,int flags,int ino)
{
  if (ino < 0 || ino >= gcsa->ninputs)
    ErrorReturn(ERROR_BADPARM, 
                (ERROR_BADPARM, 
                 "GCSAputInputType(%d, %s, %d, %d): index out of range",
                 type, fname ? fname : "NULL", navgs, ino)) ;

  gcsa->inputs[ino].type = type ;
  gcsa->inputs[ino].navgs = navgs ;
  if (fname)
    strcpy(gcsa->inputs[ino].fname, fname) ;
  gcsa->inputs[ino].flags = flags ;
  return(NO_ERROR) ;
}

int
GCSAsetCovariancesToIdentity(GCSA *gcsa)
{
  GCSA_NODE  *gcsan ;
  GCS        *gcs ;
  int        i, n ;

  for (i = 0 ; i < gcsa->mris_classifiers->nvertices ; i++)
  {
    if (i == Gdiag_no)
      DiagBreak() ;
    gcsan = &gcsa->gc_nodes[i] ;
    for (n = 0 ; n < gcsan->nlabels ; n++)
    {
      gcs = &gcsan->gcs[n] ;
      MatrixIdentity(gcsa->ninputs, gcs->m_cov) ;
    }
  }
  return(NO_ERROR) ;
}

