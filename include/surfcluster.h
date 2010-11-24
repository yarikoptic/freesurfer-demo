/**
 * @file  surfcluster.h
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
 *    $Revision: 1.13.2.1 $
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

#ifndef _SURFCLUSTER_H
#define _SURFCLUSTER_H

#include "mri.h"
#include "mrisurf.h"
#include "label.h"

#undef SIGN
#define SIGN(x) (((x)>0)? 1.0 : -1.0 )


#ifdef SURFCLUSTER_SRC
int FixSurfClusterArea = 1;
#else
extern int FixSurfClusterArea;
#endif

/* Surface Cluster Summary */
typedef struct
{
  int   clusterno;
  int   nmembers;
  float area;
  float maxval;
  int   vtxmaxval;
  float x,y,z;
  float xxfm,yxfm,zxfm;
  double pval_clusterwise; // from cluster simulation
  double pval_clusterwise_low; // from cluster simulation
  double pval_clusterwise_hi; // from cluster simulation
}
SURFCLUSTERSUM, SCS;

SCS *sclustMapSurfClusters(MRI_SURFACE *Surf, float thmin, float thmax,
                           int thsign, float minarea, int *nClusters,
                           MATRIX *XFM);
int sclustGrowSurfCluster(int ClustNo, int SeedVtx, MRI_SURFACE *Surf,
                          float thmin, float thmax, int thsign);
float sclustSurfaceArea(int ClusterNo, MRI_SURFACE *Surf, int *nvtxs) ;
float sclustSurfaceMax(int ClusterNo, MRI_SURFACE *Surf, int *vtxmax) ;
float sclustZeroSurfaceClusterNo(int ClusterNo, MRI_SURFACE *Surf);
float sclustZeroSurfaceNonClusters(MRI_SURFACE *Surf);
float sclustSetSurfaceValToClusterNo(MRI_SURFACE *Surf);
float sclustSetSurfaceValToCWP(MRI_SURFACE *Surf, SCS *scs);
float sclustCountClusters(MRI_SURFACE *Surf);
SCS *SurfClusterSummary(MRI_SURFACE *Surf, MATRIX *T, int *nClusters);
int DumpSurfClusterSum(FILE *fp, SCS *scs, int nClusters);
SCS *SortSurfClusterSum(SCS *scs, int nClusters);
int sclustReMap(MRI_SURFACE *Surf, int nClusters, SCS *scs_sorted);
double sclustMaxClusterArea(SURFCLUSTERSUM *scs, int nClusters);
int sclustMaxClusterCount(SURFCLUSTERSUM *scs, int nClusters);
SCS *sclustPruneByCWPval(SCS *ClusterList, int nclusters, 
			 double cwpvalthresh,int *nPruned, 
			 MRIS *surf);
int sclustAnnot(MRIS *surf, int NClusters);
int sclustGrowByDist(MRIS *surf, int seedvtxno, double dthresh, 
		     int shape, int vtxno, int *vtxlist);
const char *sculstSrcVersion(void);

#endif
