/**
 * @file  gcamorphgpu.hpp
 * @brief Holds GCA morph data on the GPU
 *
 * 
 */
/*
 * Original Author: Richard Edgar
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2010/10/22 22:40:14 $
 *    $Revision: 1.25.2.1 $
 *
 * Copyright (C) 2002-2008,
 * The General Hospital Corporation (Boston, MA). 
 * All rights reserved.
 *
 * Distribution, usage and copying of this software is covered under the
 * terms found in the License Agreement file named 'COPYING' found in the
 * FreeSurfer source code root directory, and duplicated here:
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferOpenSourceLicense
 *
 * General inquiries: freesurfer@nmr.mgh.harvard.edu
 *
 */

#ifdef GCAMORPH_ON_GPU

#ifndef GCA_MORPH_GPU_H
#define GCA_MORPH_GPU_H

#include <cuda_runtime.h>

#include "gcamorph.h"

#include "chronometer.hpp"

#include "volumegpu.hpp"


// ==================================================================

namespace GPU {

  namespace Classes {

    //! Class to hold a GCA morph on the GPU
    class GCAmorphGPU {
      /*!
	Class to hold a GCA morph on the GPU.
	This version of the class only supports
	one 'input' in the GC1D structure
      */
    public:
      //! Matches x in GCAmorph
      VolumeGPU<float> d_rx;
      //! Matches y in GCAmorph
      VolumeGPU<float> d_ry;
      //! Matches z in GCAmorph
      VolumeGPU<float> d_rz;

      //! Matches origx in GCAmorph
      VolumeGPU<float> d_origx;
      //! Matches origy in GCAmorph
      VolumeGPU<float> d_origy;
      //! Matches origz in GCAmorph
      VolumeGPU<float> d_origz;

      //! Matches dx in GCAmorph
      VolumeGPU<float> d_dx;
      //! Matches dy in GCAmorph
      VolumeGPU<float> d_dy;
      //! Matches dz in GCAmorph
      VolumeGPU<float> d_dz;

      //! Matches odx in GCAmorph
      VolumeGPU<float> d_odx;
      //! Matches ody in GCAmorph
      VolumeGPU<float> d_ody;
      //! Matches odz in GCAmorph
      VolumeGPU<float> d_odz;

      //! Matches orig_area field in GCAmorph
      VolumeGPU<float> d_origArea;
      //! Matches orig_area1 field in GCAmorph
      VolumeGPU<float> d_origArea1;
      //! Matches orig_area2 field in GCAmorph
      VolumeGPU<float> d_origArea2;

      //! Matches area field in GCAmorph
      VolumeGPU<float> d_area;
      //! Matches area1 field in GCAmorph
      VolumeGPU<float> d_area1;
      //! Matches area2 field in GCAmorph
      VolumeGPU<float> d_area2;

      //! Matches invalid flag in GCAmorph
      VolumeGPU<char> d_invalid;
      //! Matches label field in GCAMorph
      VolumeGPU<int> d_label;
      //! Matches status field in GCAMorph
      VolumeGPU<int> d_status;
      //! Matches the 'label_dist' field of the GCAmorph
      VolumeGPU<float> d_labelDist;

      //! Matches the 'means' field of the GC1D
      VolumeGPU<float> d_mean;
      //! Matches the 'covars' field of the GC1D (a variance with only one mean). A negative value indicates that no value is stored for this or corresponding d_mean
      VolumeGPU<float> d_variance;

      //! Matches exp_k in GCAmorph
      double exp_k;

      //! Matches neg in GCAmorph
      int neg;

      //! Matches GCA in GCAmorph (NASTY!!!)
      GCA* gca;

      // -----------------------------------------------
      // Static host arrays for transfers
      static dim3 hostDims;
      static float *h_rx, *h_ry, *h_rz;
      static float *h_origx, *h_origy, *h_origz;
      static float *h_dx, *h_dy, *h_dz;
      static float *h_odx, *h_ody, *h_odz;
      static float *h_origArea, *h_origArea1, *h_origArea2;
      static float *h_area, *h_area1, *h_area2;
      static char *h_invalid;
      static int *h_label, *h_status;
      static float *h_labelDist;
      static float *h_mean;
      static float *h_variance;

      static void AllocateHost( const GCAmorphGPU& gcam );
      static void ReleaseHost( void );
      static void RandomiseHost( void );
      

      // -----------------------------------------
      // Constructors & Destructor

      //! Default constructor
      GCAmorphGPU( void ) : d_rx(), d_ry(), d_rz(),
			    d_origx(), d_origy(), d_origz(),
			    d_dx(), d_dy(), d_dz(),
			    d_odx(), d_ody(), d_odz(),
			    d_origArea(), d_origArea1(), d_origArea2(),
			    d_area(), d_area1(), d_area2(),
			    d_invalid(),
			    d_label(),
			    d_status(),
			    d_labelDist(),
			    d_mean(),
			    d_variance(),
			    exp_k(0),
			    neg(0),
			    gca(NULL) {};

      //! Destructor
      ~GCAmorphGPU( void ) {};

      // -------------------------------------------

      //! Checks integrity of members
      void CheckIntegrity( void ) const;

      // -------------------------------------------
      // Memory management
      
      //! Allocates GPU memory for volume of given size
      void AllocateAll( const dim3& dims );

      //! Releases all the GPU memory
      void ReleaseAll( void );

      //! Zeros out all the memory
      void ClearAll( void );

      // -------------------------------------------
      // Transfer routines

      //! Sends all data to the GPU
      void SendAll( const GCAM* src );

      //! Receives all data from the GPU
      void RecvAll( GCAM* dst ) const;

      // -------------------------------------------
      // Computations

      //! Computes the properties of the metric
      void ComputeMetricProperties( int& invalid );

      //! Zeros out the dx, dy and dz fields
      void ClearGradient( void );

      //! Zeros out the odx, ody and odz fields
      void ClearMomentum( void );

      //! Applies a gradient (gcamApplyGradient)
      void ApplyGradient( GCA_MORPH_PARMS *parms );

      //! Undoes a gradien application (gcamUndoGradient)
      void UndoGradient( void );

      //! Adds a status flag to all nodes
      void AddStatus( const int addState );

      //! Removes a status flag from all nodes
      void RemoveStatus( const int subtractState );

      //! Removes two labels
      void ResetLabelNodeStatus( void );

      // -------------------------------------------
      static void ShowTimings( void );

    private:


      static SciGPU::Utilities::Chronometer tSendTot;
      static SciGPU::Utilities::Chronometer tSendPack, tSendTransfer;
      static SciGPU::Utilities::Chronometer tRecvTot;
      static SciGPU::Utilities::Chronometer tRecvPack, tRecvTransfer;
      static SciGPU::Utilities::Chronometer tHostAlloc, tHostRelease;
      static SciGPU::Utilities::Chronometer tHostRandomise;

      static SciGPU::Utilities::Chronometer tCMPtot;
      static SciGPU::Utilities::Chronometer tCMPcompute;
    };

  }
}

#endif


#endif
