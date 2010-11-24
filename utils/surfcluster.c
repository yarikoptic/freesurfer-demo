/**
 * @file  surfcluster.c
 * @brief routines for growing clusters on the surface
 *
 * routines for growing clusters on the surface
 * based on intensity thresholds and area threshold. Note: this
 * makes use of the undefval in the MRI_SURFACE structure.
 */
/*
 * Original Author: Doug Greve
 * CVS Revision Info:
 *    $Author: greve $
 *    $Date: 2010/11/24 22:10:37 $
 *    $Revision: 1.24.2.1 $
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
#include <string.h>

#include "error.h"
#include "diag.h"
#include "mri.h"
#include "resample.h"
#include "matrix.h"

// This must be included prior to volcluster.c (I think)
#define SURFCLUSTER_SRC
#include "surfcluster.h"
#undef SURFCLUSTER_SRC

#include "volcluster.h"


static int sclustCompare(const void *a, const void *b);

/*---------------------------------------------------------------
  sculstSrcVersion(void) - returns CVS version of this file.
  ---------------------------------------------------------------*/
const char *sculstSrcVersion(void)
{
  return("$Id: surfcluster.c,v 1.24.2.1 2010/11/24 22:10:37 greve Exp $");
}

/* ------------------------------------------------------------
   sclustMapSurfClusters() - grows a clusters on the surface.  The
   cluster is a list of contiguous vertices that that meet the
   threshold criteria. The cluster does not exist as a list at this
   point. Rather, the clusters are mapped using using the undefval
   element of the MRI_SURF structure. If a vertex meets the cluster
   criteria, then undefval is set to the cluster number.
   ------------------------------------------------------------ */
SCS *sclustMapSurfClusters(MRI_SURFACE *Surf, float thmin, float thmax,
                           int thsign, float minarea, int *nClusters,
                           MATRIX *XFM)
{
  SCS *scs, *scs_sorted;
  int vtx, vtx_inrange, vtx_clustno, CurrentClusterNo;
  float vtx_val,ClusterArea;
  int nVtxsInCluster;

  /* initialized all cluster numbers to 0 */
  for (vtx = 0; vtx < Surf->nvertices; vtx++)
    Surf->vertices[vtx].undefval = 0; /* overloads this elem of struct */

  /* Go through each vertex looking for one that meets the threshold
     criteria and has not been previously assigned to a cluster.
     When found, grow it out. */
  CurrentClusterNo = 1;
  for (vtx = 0; vtx < Surf->nvertices; vtx++)
  {

    vtx_val     = Surf->vertices[vtx].val;
    vtx_clustno = Surf->vertices[vtx].undefval;
    vtx_inrange = clustValueInRange(vtx_val,thmin,thmax,thsign);

    if (vtx_clustno == 0 && vtx_inrange)
    {
      sclustGrowSurfCluster(CurrentClusterNo,vtx,Surf,thmin,thmax,thsign);
      if (minarea > 0)
      {
        /* If the cluster does not meet the area criteria, delete it */
        ClusterArea = sclustSurfaceArea(CurrentClusterNo, Surf,
                                        &nVtxsInCluster) ;
        if (ClusterArea < minarea)
        {
          sclustZeroSurfaceClusterNo(CurrentClusterNo, Surf);
          continue;
        }
      }
      CurrentClusterNo++;
    }
  }

  *nClusters = CurrentClusterNo-1;
  if (*nClusters == 0) return(NULL);

  /* Get a summary of the clusters */
  scs = SurfClusterSummary(Surf, XFM, nClusters);

  /* Sort the clusters by descending maxval */
  scs_sorted  = SortSurfClusterSum(scs, *nClusters);

  if (Gdiag_no > 1)
  {
    printf("--- Surface Cluster Summary (unsorted) ---------------\n");
    DumpSurfClusterSum(stdout,scs,*nClusters);
    printf("---------- sorted ---------------\n");
    DumpSurfClusterSum(stdout,scs_sorted,*nClusters);
  }

  /* Remap the cluster numbers to match the sorted */
  sclustReMap(Surf, *nClusters, scs_sorted);

  free(scs);

  return(scs_sorted);
}
/* ------------------------------------------------------------
   sclustGrowSurfCluster() - grows a cluster on the surface from
   the SeedVtx. The cluster is a list of vertices that are
   contiguous with the seed vertex and that meet the threshold
   criteria. The cluster map itself is defined using the
   undefval of the MRI_SURF structure. If a vertex meets the
   cluster criteria, then undefval is set to the ClusterNo.
   The ClustNo cannot be 0.
   ------------------------------------------------------------ */
int sclustGrowSurfCluster(int ClusterNo, int SeedVtx, MRI_SURFACE *Surf,
                          float thmin, float thmax, int thsign)
{
  int nbr, nbr_vtx, nbr_inrange, nbr_clustno;
  float nbr_val;

  if (ClusterNo == 0)
  {
    printf("ERROR: clustGrowSurfCluster(): ClusterNo is 0\n");
    return(1);
  }

  Surf->vertices[SeedVtx].undefval = ClusterNo;

  for (nbr=0; nbr < Surf->vertices[SeedVtx].vnum; nbr++)
  {

    nbr_vtx     = Surf->vertices[SeedVtx].v[nbr];
    nbr_clustno = Surf->vertices[nbr_vtx].undefval;
    if (nbr_clustno != 0) continue;
    nbr_val     = Surf->vertices[nbr_vtx].val;
    if (fabs(nbr_val) < thmin) continue;
    nbr_inrange = clustValueInRange(nbr_val,thmin,thmax,thsign);
    if (!nbr_inrange) continue;
    sclustGrowSurfCluster(ClusterNo, nbr_vtx, Surf, thmin,thmax,thsign);

  }
  return(0);
}
/*----------------------------------------------------------------
  sclustSurfaceArea() - computes the surface area (in mm^2) of a
  cluster. Note:   MRIScomputeMetricProperties() must have been
  run on the surface.
  ----------------------------------------------------------------*/
float sclustSurfaceArea(int ClusterNo, MRI_SURFACE *Surf, int *nvtxs)
{
  int vtx, vtx_clusterno;
  float ClusterArea;

  *nvtxs = 0;
  ClusterArea = 0.0;
  for (vtx = 0; vtx < Surf->nvertices; vtx++)
  {
    vtx_clusterno = Surf->vertices[vtx].undefval;
    if(vtx_clusterno != ClusterNo) continue;
    if(! Surf->group_avg_vtxarea_loaded)
      ClusterArea += Surf->vertices[vtx].area;
    else
      ClusterArea += Surf->vertices[vtx].group_avg_area;
    (*nvtxs) ++;
  }

  if(Surf->group_avg_surface_area > 0 && 
     ! Surf->group_avg_vtxarea_loaded) {
    // In Dec 2008, a bug was found in this section of code.  The
    // above line read only: if(Surf->group_avg_surface_area > 0) This
    // caused the group vertex area ajustment to be applied twice
    // because the section of code immediately above has already
    // applied it if the group vertex area was already loaded (which
    // it always was). This caused the surface area to be too big (by
    // about 20-25% for fsaverage). This was fixed by adding: 
    //   && !Surf->group_avg_vtxarea_loaded

    // This function is called by both mri_glmfit and mri_surfcluster.
    // To indicate this fix, a new global variable was created called
    // FixSurfClusterArea, which is set to 1. The mere presence of
    // this variable implies that this bug was fixed.  The variable
    // exists so that the CSD created by mri_glmfit can record the
    // fact that the cluster area is correct. When mri_surfcluster
    // reads in the CSD, the presense of this flag in the CSD file
    // will indicate that the area is correct. If it is not correct,
    // then mri_surfcluster will exit with error. This assures that an
    // old CSD file will not be used with the new mri_surfcluster.

    // This will not prevent new CSD files from being used with an old
    // version of mri_surfcluster. However, the CSD format was also
    // changed to be incompatible with the old CSD reader.

    // Always do this now (4/9/10)
    ClusterArea *= (Surf->group_avg_surface_area/Surf->total_area);
    //if (getenv("FIX_VERTEX_AREA") != NULL)
    //ClusterArea *= (Surf->group_avg_surface_area/Surf->total_area);
  }

  return(ClusterArea);
}
/*----------------------------------------------------------------
  sclustSurfaceMax() - returns the maximum intensity value of
  inside a given cluster and the vertex at which it occured.
----------------------------------------------------------------*/
float sclustSurfaceMax(int ClusterNo, MRI_SURFACE *Surf, int *vtxmax)
{
  int vtx, vtx_clusterno, first_hit;
  float vtx_val, vtx_val_max = 0;

  first_hit = 1;
  for (vtx = 0; vtx < Surf->nvertices; vtx++)
  {
    vtx_clusterno = Surf->vertices[vtx].undefval;
    if (vtx_clusterno != ClusterNo) continue;

    vtx_val = Surf->vertices[vtx].val;
    if (first_hit)
    {
      vtx_val_max = vtx_val;
      *vtxmax = vtx;
      first_hit = 0;
      continue;
    }

    if (fabs(vtx_val) > fabs(vtx_val_max))
    {
      vtx_val_max = vtx_val;
      *vtxmax = vtx;
    }

  }

  return(vtx_val_max);
}
/*----------------------------------------------------------------
  sclustZeroSurfaceClusterNo() - finds all the vertices with
  cluster number equal to ClusterNo and sets the cluster number
  to zero (cluster number is the undefval member of the surface
  structure). Nothing is done to the surface value. This function
  is good for pruning clusters that do not meet some other
  criteria (eg, area threshold).
  ----------------------------------------------------------------*/
float sclustZeroSurfaceClusterNo(int ClusterNo, MRI_SURFACE *Surf)
{
  int vtx, vtx_clusterno;

  for (vtx = 0; vtx < Surf->nvertices; vtx++)
  {
    vtx_clusterno = Surf->vertices[vtx].undefval;
    if (vtx_clusterno == ClusterNo)
      Surf->vertices[vtx].undefval = 0;
  }

  return(0);
}
/*----------------------------------------------------------------
  sclustZeroSurfaceNonClusters() - zeros the value of all the vertices
  that are not assocated with a cluster. The cluster number is the
  undefval member of the surface structure.
  ----------------------------------------------------------------*/
float sclustZeroSurfaceNonClusters(MRI_SURFACE *Surf)
{
  int vtx, vtx_clusterno;

  for (vtx = 0; vtx < Surf->nvertices; vtx++)
  {
    vtx_clusterno = Surf->vertices[vtx].undefval;
    if (vtx_clusterno == 0)  Surf->vertices[vtx].val = 0.0;
  }

  return(0);
}
/*----------------------------------------------------------------
  sclustSetSurfaceClusterToClusterNo() - sets the value of a vertex to the
  cluster number. The cluster number is the undefval member of the
  surface structure.
  ----------------------------------------------------------------*/
float sclustSetSurfaceValToClusterNo(MRI_SURFACE *Surf)
{
  int vtx, vtx_clusterno;

  for (vtx = 0; vtx < Surf->nvertices; vtx++)
  {
    vtx_clusterno = Surf->vertices[vtx].undefval;
    Surf->vertices[vtx].val = vtx_clusterno;
  }

  return(0);
}
/*----------------------------------------------------------------
  sclustSetSurfaceClusterToCWP() - sets the value of a vertex to
  -log10(cluster-wise pvalue).
  ----------------------------------------------------------------*/
float sclustSetSurfaceValToCWP(MRI_SURFACE *Surf, SCS *scs)
{
  int vtx, vtx_clusterno;
  float val;

  for (vtx = 0; vtx < Surf->nvertices; vtx++)
  {
    vtx_clusterno = Surf->vertices[vtx].undefval;

    if (vtx_clusterno==0) val = 0;
    else {
      val = scs[vtx_clusterno-1].pval_clusterwise;
      if(val == 0.0) val = 50;
      else           val = -log10(val);
      val = val * SIGN(scs[vtx_clusterno-1].maxval);
    }
    Surf->vertices[vtx].val = val;
  }

  return(0);
}
/*----------------------------------------------------------------
  sclustCountClusters() - counts the number of clusters. Really
  just returns the largest cluster number, which will be the
  number of clusters if there are no holes.
  ----------------------------------------------------------------*/
float sclustCountClusters(MRI_SURFACE *Surf)
{
  int vtx, vtx_clusterno, maxclusterno;

  maxclusterno = 0;
  for (vtx = 0; vtx < Surf->nvertices; vtx++)
  {
    vtx_clusterno = Surf->vertices[vtx].undefval;
    if (maxclusterno < vtx_clusterno) maxclusterno = vtx_clusterno;
  }
  return(maxclusterno);
}


/*----------------------------------------------------------------*/
SCS *SurfClusterSummary(MRI_SURFACE *Surf, MATRIX *T, int *nClusters)
{
  int n;
  SURFCLUSTERSUM *scs;
  MATRIX *xyz, *xyzxfm;

  *nClusters = sclustCountClusters(Surf);
  if (*nClusters == 0) return(NULL);

  xyz    = MatrixAlloc(4,1,MATRIX_REAL);
  xyz->rptr[4][1] = 1;
  xyzxfm = MatrixAlloc(4,1,MATRIX_REAL);

  scs = (SCS *) calloc(*nClusters, sizeof(SCS));

  for (n = 0; n < *nClusters ; n++)
  {
    scs[n].clusterno = n+1;
    scs[n].area   = sclustSurfaceArea(n+1, Surf, &scs[n].nmembers);
    scs[n].maxval = sclustSurfaceMax(n+1,  Surf, &scs[n].vtxmaxval);
    scs[n].x = Surf->vertices[scs[n].vtxmaxval].x;
    scs[n].y = Surf->vertices[scs[n].vtxmaxval].y;
    scs[n].z = Surf->vertices[scs[n].vtxmaxval].z;
    if (T != NULL)
    {
      xyz->rptr[1][1] = scs[n].x;
      xyz->rptr[2][1] = scs[n].y;
      xyz->rptr[3][1] = scs[n].z;
      MatrixMultiply(T,xyz,xyzxfm);
      scs[n].xxfm = xyzxfm->rptr[1][1];
      scs[n].yxfm = xyzxfm->rptr[2][1];
      scs[n].zxfm = xyzxfm->rptr[3][1];
    }
  }

  MatrixFree(&xyz);
  MatrixFree(&xyzxfm);

  return(scs);
}
/*----------------------------------------------------------------*/
int DumpSurfClusterSum(FILE *fp, SCS *scs, int nClusters)
{
  int n;

  for (n=0; n < nClusters; n++)
  {
    fprintf(fp,"%4d  %4d  %8.4f  %6d    %6.2f  %4d   %5.1f %5.1f %5.1f   "
            "%5.1f %5.1f %5.1f\n",n,scs[n].clusterno,
            scs[n].maxval, scs[n].vtxmaxval,
            scs[n].area,scs[n].nmembers,
            scs[n].x, scs[n].y, scs[n].z,
            scs[n].xxfm, scs[n].yxfm, scs[n].zxfm);
  }
  return(0);
}
/*----------------------------------------------------------------*/
SCS *SortSurfClusterSum(SCS *scs, int nClusters)
{
  SCS *scs_sorted;
  int n;

  scs_sorted = (SCS *) calloc(nClusters, sizeof(SCS));

  for (n=0; n < nClusters; n++)
    memmove(&scs_sorted[n],&scs[n],sizeof(SCS));

  /* Note: scs_sorted.clusterno does not changed */
  qsort((void *) scs_sorted, nClusters, sizeof(SCS), sclustCompare);

  return(scs_sorted);
}

/*----------------------------------------------------------------
  sclustReMap() - remaps the cluster numbers (ie, undefval) in the
  surface structure based on the sorted surface cluster summary
  (SCS). It is assumed that the scs.clusterno in the sorted SCS
  is the cluster id that corresponds to the original cluster id.
  ----------------------------------------------------------------*/
int sclustReMap(MRI_SURFACE *Surf, int nClusters, SCS *scs_sorted)
{
  int vtx, vtx_clusterno, c;

  if (Gdiag_no > 1)
  {
    printf("sclustReMap:\n");
    for (c=1; c <= nClusters; c++)
      printf("new = %3d old = %3d\n",c,scs_sorted[c-1].clusterno);
  }

  for (vtx = 0; vtx < Surf->nvertices; vtx++)
  {
    vtx_clusterno = Surf->vertices[vtx].undefval;
    for (c=1; c <= nClusters; c++)
    {
      if (vtx_clusterno == scs_sorted[c-1].clusterno)
      {
        Surf->vertices[vtx].undefval = c;
        break;
      }
    }
  }

  return(0);
}

/*----------------------------------------------------------------*/
/*--------------- STATIC FUNCTIONS BELOW HERE --------------------*/
/*----------------------------------------------------------------*/

/*----------------------------------------------------------------
  sclustCompare() - compares two surface cluster summaries (for
  use with qsort().
  ----------------------------------------------------------------*/
static int sclustCompare(const void *a, const void *b)
{
  SCS sc1, sc2;

  sc1 = *((SURFCLUSTERSUM *)a);
  sc2 = *((SURFCLUSTERSUM *)b);

  if (fabs(sc1.maxval) > fabs(sc2.maxval) ) return(-1);
  if (fabs(sc1.maxval) < fabs(sc2.maxval) ) return(+1);

  if (sc1.area > sc2.area) return(-1);
  if (sc1.area < sc2.area) return(+1);

  return(0);
}

/*-------------------------------------------------------------------
  sclustMaxClusterArea() - returns the area of the cluster with the
  maximum area.
  -------------------------------------------------------------------*/
double sclustMaxClusterArea(SURFCLUSTERSUM *scs, int nClusters)
{
  int n;
  double maxarea;

  if (nClusters==0) return(0);

  maxarea = scs[0].area;
  for (n=0; n<nClusters; n++)
    if (maxarea < scs[n].area) maxarea = scs[n].area;
  return(maxarea);
}

/*-------------------------------------------------------------------
  sclustMaxClusterCount() - returns the area of the cluster with the
  maximum number of members (count)
  -------------------------------------------------------------------*/
int sclustMaxClusterCount(SURFCLUSTERSUM *scs, int nClusters)
{
  int n;
  int maxcount;

  if (nClusters==0) return(0);

  maxcount = scs[0].nmembers;
  for (n=0; n<nClusters; n++)
    if (maxcount < scs[n].nmembers) maxcount = scs[n].nmembers;
  return(maxcount);
}

/*---------------------------------------------------------------*/
SCS *sclustPruneByCWPval(SCS *ClusterList, int nclusters, 
			 double cwpvalthresh,int *nPruned, 
			 MRIS *surf)
{
  int n,nth,vtxno,map[10000];
  SCS *scs;

  // Construct a new SCS with pruned clusters
  nth = 0;
  for(n=0; n < nclusters; n++)
    if(ClusterList[n].pval_clusterwise < cwpvalthresh) nth++;
  *nPruned = nth;

  scs = (SCS *) calloc(*nPruned, sizeof(SCS));
  nth = 0;
  for(n=0; n < nclusters; n++){
    if(ClusterList[n].pval_clusterwise <= cwpvalthresh){
      memmove(&scs[nth],&ClusterList[n],sizeof(SCS));
      map[n] = nth;
      nth++;
    }
  }

  for(vtxno = 0; vtxno < surf->nvertices; vtxno++){
    n = surf->vertices[vtxno].undefval; //1-based
    if(n == 0) continue;
    if(ClusterList[n-1].pval_clusterwise > cwpvalthresh){
      // Remove clusters/values from surface 
      surf->vertices[vtxno].undefval = 0;
      surf->vertices[vtxno].val = 0;
    } else {
      // Re-number
      surf->vertices[vtxno].undefval = map[n-1] + 1;
    }
  }

  return(scs);
}

/* ------------------------------------------------------------
   int sclustAnnot(MRIS *surf, int NClusters)
   Convert clusters into annotation
   ------------------------------------------------------------*/
int sclustAnnot(MRIS *surf, int NClusters)
{
  COLOR_TABLE *ct ;
  int vtxno, vtx_clusterno, annot, n;

  ct = CTABalloc(NClusters+1);
  surf->ct = ct;

  for(n=1; n < NClusters; n++) // no cluster 0
    sprintf(surf->ct->entries[n]->name, "%s-%03d","cluster",n);

  for(vtxno = 0; vtxno < surf->nvertices; vtxno++)  {
    vtx_clusterno = surf->vertices[vtxno].undefval;
    if(vtx_clusterno == 0 || vtx_clusterno > NClusters){
      surf->vertices[vtxno].annotation = 0;
      continue;
    }
    CTABannotationAtIndex(surf->ct, vtx_clusterno, &annot);
    surf->vertices[vtxno].annotation = annot;
  }
  return(0);
}

/* --------------------------------------------------------------*/
/*!
  \fn int sclustGrowByDist(MRIS *surf, int seedvtxno, double dthresh, 
        int shape, int vtxno, int init, int *vtxlist)
  \brief Grow a cluster from the seed vertex out to a given distance.
  The undefval of the surf will be zeroed and all vertices in
  within distance are set to 1. vtxlist is filled with the vertex 
  numbers, and the return value is the number of vertices in the
  cluster. If shape is SPHERICAL_COORDS, then distance is computed
  along the arc of a sphere, otherwise 2d flat is assumed. This
  is a recursive function. Recursive calls will have the
  the vtxno >= 0, so set vtxno = -1 when you call this function.
*/
int sclustGrowByDist(MRIS *surf, int seedvtxno, double dthresh, 
		   int shape, int vtxno, int *vtxlist)
{
  static double radius=0, radius2=0;
  static int nhits=0, ncalls=0;
  static VERTEX *v1 = NULL;
  VERTEX *v2;
  double theta=0, costheta=0, d=0;
  int nthnbr,nbrvtxno;

  if(vtxlist == NULL){
    printf("ERROR: vtxlist is NULL\n");
    return(-1);
  }

  // This is just a check to make sure that the caller has done the right thing.
  // Note: this check does not work after the first call.
  if(ncalls == 0 && vtxno >= 0){
    printf("ERROR: set vtxno to a negative value when calling this function!\n");
    return(-1);
  }

  // A negative vtxno indicates a non-recursive call, Init required
  if(vtxno < 0){
    v1 = &(surf->vertices[seedvtxno]);
    ncalls = 0;
    nhits = 0;
    if(shape == SPHERICAL_COORDS){
      radius2 = (v1->x*v1->x + v1->y*v1->y + v1->z*v1->z);
      radius = sqrt(radius2);
    }
    for(vtxno = 0; vtxno < surf->nvertices; vtxno++)
      surf->vertices[vtxno].undefval = 0;
    vtxno = seedvtxno;
  }
  v2 = &(surf->vertices[vtxno]);
  ncalls++;

  if(v2->undefval) return(0);

  if(shape == SPHERICAL_COORDS) {
    costheta = ((v1->x*v2->x) + (v1->y*v2->y) + (v1->z*v2->z))/radius2;
    theta = acos(costheta);
    d = radius*theta;
    //printf("%g %g %g %g\n",costheta,theta,radius,radius2);
  } else {
    d = sqrt((v1->x-v2->x)*(v1->x-v2->x) + (v1->y-v2->y)*(v1->y-v2->y));
  }

  // Check distance against threshold
  if(d > dthresh) return(0);

  // Add to the list
  vtxlist[nhits] = vtxno;
  nhits ++;
  
  //printf("%3d %3d %6d %6d   %g %g %g   %g %g %g   %g\n",
  // ncalls,nhits,seedvtxno,vtxno,
  // v1->x,v1->y,v1->z,v2->x,v2->y,v2->z, d);

  // Go throught the neighbors ...
  v2->undefval = 1;
  for(nthnbr=0; nthnbr < v2->vnum; nthnbr++){
    nbrvtxno = v2->v[nthnbr];
    sclustGrowByDist(surf, seedvtxno, dthresh, shape, nbrvtxno, vtxlist);
  }
  
  return(nhits);
}
