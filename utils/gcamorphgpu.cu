/**
 * @file  gcamorphgpu.cu
 * @brief Holds GCA morph data on the GPU
 *
 * 
 */
/*
 * Original Author: Richard Edgar
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2010/10/22 22:40:14 $
 *    $Revision: 1.36.2.1 $
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

#include "macros.h"

#include "chronometer.hpp"

#include "volumegpucompare.hpp"


#include "gcamorphgpu.hpp"


//! Texture reference for rx
texture<float,3,cudaReadModeElementType> dt_rx;
//! Texture reference for ry
texture<float,3,cudaReadModeElementType> dt_ry;
//! Texture reference for rz
texture<float,3,cudaReadModeElementType> dt_rz;

//! Texture reference for dx
texture<float,3,cudaReadModeElementType> dt_dx;
//! Texture reference for dy
texture<float,3,cudaReadModeElementType> dt_dy;
//! Texture reference for dz
texture<float,3,cudaReadModeElementType> dt_dz;


// ==============================================================

namespace GPU {
  namespace Classes {

    // --------------------------------------------

    void GCAmorphGPU::CheckIntegrity( void ) const {
      /*!
	Checks that all the allocated member arrays have
	the same dimensions.
	Aborts the program if the check fails
      */

      const dim3 myDims = this->d_rx.GetDims();

      bool good = ( myDims == this->d_ry.GetDims() );
      good = ( good && ( myDims == this->d_rz.GetDims() ) );

      good = ( good && ( myDims == this->d_origx.GetDims() ) );
      good = ( good && ( myDims == this->d_origy.GetDims() ) );
      good = ( good && ( myDims == this->d_origz.GetDims() ) );

      good = ( good && ( myDims == this->d_dx.GetDims() ) );
      good = ( good && ( myDims == this->d_dy.GetDims() ) );
      good = ( good && ( myDims == this->d_dz.GetDims() ) );
      
      good = ( good && ( myDims == this->d_odx.GetDims() ) );
      good = ( good && ( myDims == this->d_ody.GetDims() ) );
      good = ( good && ( myDims == this->d_odz.GetDims() ) );

      good = ( good && ( myDims == this->d_invalid.GetDims() ) );

      good = ( good && ( myDims == this->d_origArea.GetDims() ) );
      good = ( good && ( myDims == this->d_origArea1.GetDims() ) );
      good = ( good && ( myDims == this->d_origArea2.GetDims() ) );

      good = ( good && ( myDims == this->d_area.GetDims() ) );
      good = ( good && ( myDims == this->d_area1.GetDims() ) );
      good = ( good && ( myDims == this->d_area2.GetDims() ) );

      good = ( good && ( myDims == this->d_label.GetDims() ) );
      good = ( good && ( myDims == this->d_status.GetDims() ) );

      good = ( good && ( myDims == this->d_mean.GetDims() ) );
      good = ( good && ( myDims == this->d_variance.GetDims() ) );

      good = ( good && ( myDims == this->d_labelDist.GetDims() ) );

      if( !good ) {
	std::cerr << __FUNCTION__
		  << ": Dimension mismatch"
		  << std::endl;
	exit( EXIT_FAILURE );
      }
    }

    // --------------------------------------------

    void GCAmorphGPU::AllocateAll( const dim3& dims ) {
      /*!
	Allocates GPU memory to hold a volume
	of the given size.
	If possible, it keeps the current allocation.
      */

      // Start by seeing if the current allocation is consistent
      this->CheckIntegrity();

      // See if we can re-use existing allocation
      if( dims == this->d_rx.GetDims() ) {
	return;
      }

      // Release existing memory
      this->ReleaseAll();

      // Allocate anew
      this->d_rx.Allocate( dims );
      this->d_ry.Allocate( dims );
      this->d_rz.Allocate( dims );

      this->d_origx.Allocate( dims );
      this->d_origy.Allocate( dims );
      this->d_origz.Allocate( dims );
 
      this->d_dx.Allocate( dims );
      this->d_dy.Allocate( dims );
      this->d_dz.Allocate( dims );
 
      this->d_odx.Allocate( dims );
      this->d_ody.Allocate( dims );
      this->d_odz.Allocate( dims );

      this->d_area.Allocate( dims );
      this->d_area1.Allocate( dims );
      this->d_area2.Allocate( dims );

      this->d_origArea.Allocate( dims );
      this->d_origArea1.Allocate( dims );
      this->d_origArea2.Allocate( dims );

      this->d_invalid.Allocate( dims );
      this->d_label.Allocate( dims );
      this->d_status.Allocate( dims );
      this->d_labelDist.Allocate( dims );

      this->d_mean.Allocate( dims );
      this->d_variance.Allocate( dims );
    }


    void GCAmorphGPU::ReleaseAll( void ) {
      /*!
	Releases each of the members.
	Recall that the VolumeGPU::Release method
	will also release any CUDA arrays.
      */
      this->d_rx.Release();
      this->d_ry.Release();
      this->d_rz.Release();

      this->d_dx.Release();
      this->d_dy.Release();
      this->d_dz.Release();

      this->d_odx.Release();
      this->d_ody.Release();
      this->d_odz.Release();

      this->d_origx.Release();
      this->d_origy.Release();
      this->d_origz.Release();

      this->d_origArea.Release();
      this->d_origArea1.Release();
      this->d_origArea2.Release();

      this->d_area.Release();
      this->d_area1.Release();
      this->d_area2.Release();

      this->d_invalid.Release();
      this->d_label.Release();
      this->d_status.Release();
      this->d_labelDist.Release();

      this->d_mean.Release();
      this->d_variance.Release();

    }



    void GCAmorphGPU::ClearAll( void ) {
      this->d_rx.Zero();
      this->d_ry.Zero();
      this->d_rz.Zero();

      this->d_dx.Zero();
      this->d_dy.Zero();
      this->d_dz.Zero();

      this->d_odx.Zero();
      this->d_ody.Zero();
      this->d_odz.Zero();

      this->d_origx.Zero();
      this->d_origy.Zero();
      this->d_origz.Zero();

      this->d_origArea.Zero();
      this->d_origArea1.Zero();
      this->d_origArea2.Zero();

      this->d_area.Zero();
      this->d_area1.Zero();
      this->d_area2.Zero();

      this->d_invalid.Zero();
      this->d_label.Zero();
      this->d_status.Zero();
      this->d_labelDist.Zero();

      this->d_mean.Zero();
      this->d_variance.Zero();
    }


    // --------------------------------------------

    void GCAmorphGPU::SendAll( const GCAM* src ) {
      /*!
	Sends all supported data in the given GCAM
	to the GPU.
	This involves a lot of packing data, and hence
	is going to be painfully slow
      */

      GCAmorphGPU::tSendTot.Start();

#if 0
      std::cerr << __FUNCTION__
		<< ": Catching gcamorph usage"
		<< std::endl;
      exit( EXIT_FAILURE );
#endif

      // Check for number of inputs
      if( src->ninputs != 1 ) {
	std::cerr << __FUNCTION__
		  << ": Must have only one input in the GC1D!"
		  << std::endl;
	exit( EXIT_FAILURE );
      }

      // Copy scalars
      this->exp_k = src->exp_k;
      this->neg = src->neg;
      this->gca = src->gca;

      // Extract the dimensions
      const dim3 dims = make_uint3( src->width,
				    src->height,
				    src->depth );

      // Allocate device memory
      this->AllocateAll( dims );

      // Allocate some page-locked host buffers
      GCAmorphGPU::AllocateHost( *this );


      GCAmorphGPU::tSendPack.Start();
      for( unsigned int i=0; i<dims.x; i++ ) {
	for( unsigned int j=0; j<dims.y; j++ ) {
	  for( unsigned int k=0; k<dims.z; k++ ) {

	    // Get the 1d index (same for all arrays)
	    const unsigned int i1d = this->d_rx.Index1D( i, j, k );
	    // Get the current node
	    const GCA_MORPH_NODE& gcamn = src->nodes[i][j][k];
	    
	    // Pack the data
	    GCAmorphGPU::h_rx[i1d] = gcamn.x;
	    GCAmorphGPU::h_ry[i1d] = gcamn.y;
	    GCAmorphGPU::h_rz[i1d] = gcamn.z;

	    GCAmorphGPU::h_origx[i1d] = gcamn.origx;
	    GCAmorphGPU::h_origy[i1d] = gcamn.origy;
	    GCAmorphGPU::h_origz[i1d] = gcamn.origz;

	    GCAmorphGPU::h_dx[i1d] = gcamn.dx;
	    GCAmorphGPU::h_dy[i1d] = gcamn.dy;
	    GCAmorphGPU::h_dz[i1d] = gcamn.dz;

	    GCAmorphGPU::h_odx[i1d] = gcamn.odx;
	    GCAmorphGPU::h_ody[i1d] = gcamn.ody;
	    GCAmorphGPU::h_odz[i1d] = gcamn.odz;

	    GCAmorphGPU::h_origArea[i1d] = gcamn.orig_area;
	    GCAmorphGPU::h_origArea1[i1d] = gcamn.orig_area1;
	    GCAmorphGPU::h_origArea2[i1d] = gcamn.orig_area2;

	    GCAmorphGPU::h_area[i1d] = gcamn.area;
	    GCAmorphGPU::h_area1[i1d] = gcamn.area1;
	    GCAmorphGPU::h_area2[i1d] = gcamn.area2;

	    GCAmorphGPU::h_invalid[i1d] = gcamn.invalid;
	    GCAmorphGPU::h_status[i1d] = gcamn.status;
	    GCAmorphGPU::h_label[i1d] = gcamn.label;
	    GCAmorphGPU::h_labelDist[i1d] = gcamn.label_dist;

	    // Deal with the GC1D
	    if( gcamn.gc != NULL ) {
	      /*
		Store the mean and variance.
		Check at top of the routine has ensured
		that there's only one input.
		This means that the covariance is really
		a variance
	      */
	      GCAmorphGPU::h_mean[i1d] = gcamn.gc->means[0];
	      GCAmorphGPU::h_variance[i1d] = gcamn.gc->covars[0];
	    } else {
	      /*
		Store negative numbers to indicate that
		there is no GC1D here.
		Since a variance must be >=0, this is
		a reliable test
	      */
	      GCAmorphGPU::h_mean[i1d] = -1;
	      GCAmorphGPU::h_variance[i1d] = -1;
	    }


	  }
	}
      }
      GCAmorphGPU::tSendPack.Stop();


      GCAmorphGPU::tSendTransfer.Start();
      // Send the data
      this->d_rx.SendBuffer( GCAmorphGPU::h_rx );
      this->d_ry.SendBuffer( GCAmorphGPU::h_ry );
      this->d_rz.SendBuffer( GCAmorphGPU::h_rz );

      this->d_origx.SendBuffer( GCAmorphGPU::h_origx );
      this->d_origy.SendBuffer( GCAmorphGPU::h_origy );
      this->d_origz.SendBuffer( GCAmorphGPU::h_origz );

      this->d_dx.SendBuffer( GCAmorphGPU::h_dx );
      this->d_dy.SendBuffer( GCAmorphGPU::h_dy );
      this->d_dz.SendBuffer( GCAmorphGPU::h_dz );

      this->d_odx.SendBuffer( GCAmorphGPU::h_odx );
      this->d_ody.SendBuffer( GCAmorphGPU::h_ody );
      this->d_odz.SendBuffer( GCAmorphGPU::h_odz );
      
      this->d_origArea.SendBuffer( GCAmorphGPU::h_origArea );
      this->d_origArea1.SendBuffer( GCAmorphGPU::h_origArea1 );
      this->d_origArea2.SendBuffer( GCAmorphGPU::h_origArea2 );

      this->d_area.SendBuffer( GCAmorphGPU::h_area );
      this->d_area1.SendBuffer( GCAmorphGPU::h_area1 );
      this->d_area2.SendBuffer( GCAmorphGPU::h_area2 );

      this->d_invalid.SendBuffer( GCAmorphGPU::h_invalid );
      this->d_status.SendBuffer( GCAmorphGPU::h_status );
      this->d_label.SendBuffer( GCAmorphGPU::h_label );
      this->d_labelDist.SendBuffer( GCAmorphGPU::h_labelDist );

      this->d_mean.SendBuffer( GCAmorphGPU::h_mean );
      this->d_variance.SendBuffer( GCAmorphGPU::h_variance );

      // Wait for the copies to complete
      CUDA_SAFE_CALL( cudaThreadSynchronize() );
      GCAmorphGPU::tSendTransfer.Stop();


      GCAmorphGPU::tSendTot.Stop();

    }

    // --------------------------------------------

    void GCAmorphGPU::RecvAll( GCAM* dst ) const {
      /*!
	Retrieves all supported data in the given GCAM
	from the GPU.
	This involves a lot of packing data, and hence
	is going to be painfully slow
      */

      GCAmorphGPU::tRecvTot.Start();

      // Check for number of inputs
      if( dst->ninputs != 1 ) {
	std::cerr << __FUNCTION__
		  << ": Must have only one input in the GC1D!"
		  << std::endl;
	exit( EXIT_FAILURE );
      }


      // Copy scalars
      dst->exp_k = this->exp_k;
      dst->neg = this->neg;
      std::cerr << __FUNCTION__
		<< ": Did not reset gca in dst"
		<< std::endl;

      // Extract the dimensions
      const dim3 dims = this->d_rx.GetDims();

      // Allocate page-locked host memory
      GCAmorphGPU::AllocateHost( *this );

      GCAmorphGPU::tRecvTransfer.Start();
      // Fetch the data
      this->d_rx.RecvBuffer( GCAmorphGPU::h_rx );
      this->d_ry.RecvBuffer( GCAmorphGPU::h_ry );
      this->d_rz.RecvBuffer( GCAmorphGPU::h_rz );

      this->d_origx.RecvBuffer( GCAmorphGPU::h_origx );
      this->d_origy.RecvBuffer( GCAmorphGPU::h_origy );
      this->d_origz.RecvBuffer( GCAmorphGPU::h_origz );

      this->d_dx.RecvBuffer( GCAmorphGPU::h_dx );
      this->d_dy.RecvBuffer( GCAmorphGPU::h_dy );
      this->d_dz.RecvBuffer( GCAmorphGPU::h_dz );

      this->d_odx.RecvBuffer( GCAmorphGPU::h_odx );
      this->d_ody.RecvBuffer( GCAmorphGPU::h_ody );
      this->d_odz.RecvBuffer( GCAmorphGPU::h_odz );

      this->d_origArea.RecvBuffer( GCAmorphGPU::h_origArea );
      this->d_origArea1.RecvBuffer( GCAmorphGPU::h_origArea1 );
      this->d_origArea2.RecvBuffer( GCAmorphGPU::h_origArea2 );

      this->d_area.RecvBuffer( GCAmorphGPU::h_area );
      this->d_area1.RecvBuffer( GCAmorphGPU::h_area1 );
      this->d_area2.RecvBuffer( GCAmorphGPU::h_area2 );

      this->d_invalid.RecvBuffer( GCAmorphGPU::h_invalid );
      this->d_status.RecvBuffer( GCAmorphGPU::h_status );
      this->d_label.RecvBuffer( GCAmorphGPU::h_label );
      this->d_labelDist.RecvBuffer( GCAmorphGPU::h_labelDist );

      this->d_mean.RecvBuffer( GCAmorphGPU::h_mean );
      this->d_variance.RecvBuffer( GCAmorphGPU:: h_variance );
      CUDA_SAFE_CALL( cudaThreadSynchronize() );
      GCAmorphGPU::tRecvTransfer.Stop();

      GCAmorphGPU::tRecvPack.Start();
      for( unsigned int i=0; i<dims.x; i++ ) {
	for( unsigned int j=0; j<dims.y; j++ ) {
	  for( unsigned int k=0; k<dims.z; k++ ) {

	    // Get the 1d index (same for all arrays)
	    const unsigned int i1d = this->d_rx.Index1D( i, j, k );
	    // Get the current node
	    GCA_MORPH_NODE* gcamn = &(dst->nodes[i][j][k]);

	    gcamn->x = GCAmorphGPU::h_rx[i1d];
	    gcamn->y = GCAmorphGPU::h_ry[i1d];
	    gcamn->z = GCAmorphGPU::h_rz[i1d];

	    gcamn->origx = GCAmorphGPU::h_origx[i1d];
	    gcamn->origy = GCAmorphGPU::h_origy[i1d];
	    gcamn->origz = GCAmorphGPU::h_origz[i1d];
	    
	    gcamn->dx = GCAmorphGPU::h_dx[i1d];
	    gcamn->dy = GCAmorphGPU::h_dy[i1d];
	    gcamn->dz = GCAmorphGPU::h_dz[i1d];
	    
	    gcamn->odx = GCAmorphGPU::h_odx[i1d];
	    gcamn->ody = GCAmorphGPU::h_ody[i1d];
	    gcamn->odz = GCAmorphGPU::h_odz[i1d];

	    gcamn->orig_area = GCAmorphGPU::h_origArea[i1d];
	    gcamn->orig_area1 = GCAmorphGPU::h_origArea1[i1d];
	    gcamn->orig_area2 = GCAmorphGPU::h_origArea2[i1d];

	    gcamn->area = GCAmorphGPU::h_area[i1d];
	    gcamn->area1 = GCAmorphGPU::h_area1[i1d];
	    gcamn->area2 = GCAmorphGPU::h_area2[i1d];

	    gcamn->invalid = GCAmorphGPU::h_invalid[i1d];
	    gcamn->label = GCAmorphGPU::h_label[i1d];
	    gcamn->label_dist = GCAmorphGPU::h_labelDist[i1d];
	    gcamn->status = GCAmorphGPU::h_status[i1d];

	    // We now have a quandary... how to test for validity
	    if( gcamn->gc != NULL ) {
	      // We know there's only one input from test at the top
	      gcamn->gc->means[0] = GCAmorphGPU::h_mean[i1d];
	      gcamn->gc->covars[0] = GCAmorphGPU::h_variance[i1d];
	    } else {
	      if( GCAmorphGPU::h_variance[i1d] >= 0 ) {
		std::cerr << __FUNCTION__
			  << ": Host has no GC1D but GPU has valid variance"
			  << std::endl;
		exit( EXIT_FAILURE );
	      }
	    }

	  }
	}
      }
      GCAmorphGPU::tRecvPack.Stop();

      GCAmorphGPU::tRecvTot.Stop();

    }




    // --------------------------------------------

    const unsigned int kCMPKernelSize = 16;
    const unsigned int iCMPGlobalsInvalid = 0;
    const unsigned int iCMPGlobalsNeg = 1;

    //! Device function to look up displacement vectors
    __device__ float3 FetchVector( const unsigned int ix,
				   const unsigned int iy,
				   const unsigned int iz ) {

      float3 r;
      r.x = tex3D( dt_rx, ix+0.5f, iy+0.5f, iz+0.5f );
      r.y = tex3D( dt_ry, ix+0.5f, iy+0.5f, iz+0.5f );
      r.z = tex3D( dt_rz, ix+0.5f, iy+0.5f, iz+0.5f );

      return( r );
    }

    //! Kernel to perform work of gcamComputeMetricProperties
    __global__
    void CompMetPropKernel( const VolumeArgGPU<float> origArea,
			    VolumeArgGPU<char> invalid,
			    VolumeArgGPU<float> area,
			    VolumeArgGPU<float> area1,
			    VolumeArgGPU<float> area2,
			    int *globals ) {
      /*!
	This kernel performs the work of gcamComputeMetricProperties.
	For now, it's unoptimised, and may cause a lot of un-necessary
	memory transations
      */
      // Compute co-ordinates
      const unsigned int ix = threadIdx.x + ( blockIdx.x * blockDim.x );
      const unsigned int iy = threadIdx.y + ( blockIdx.y * blockDim.y );

      // Check if in volume
      if( !origArea.InVolume( ix, iy, 0 ) ) {
	return;
      }

      // Loop over each z slice
      for( unsigned int iz=0; iz< origArea.dims.z; iz++ ) {

	int neg = 0;
	int num = 0;

	// Check for invalid node
	if( invalid( ix, iy, iz ) == GCAM_POSITION_INVALID ) {
	  atomicAdd( &(globals[iCMPGlobalsInvalid]), 1 );
	  continue;
	}

	// Fetch the location of the current voxel
	const float3 r = FetchVector( ix, iy, iz );
	
	// Zero the 'area'
	area(ix,iy,iz) = 0;

	// Compute Jacobean determinants on the 'right'
	if( (ix<origArea.dims.x-1) &&
	    (iy<origArea.dims.y-1) &&
	    (iz<origArea.dims.z-1) ) {


	  // Check for validity
	  if( (invalid(ix+1,iy,iz) != GCAM_POSITION_INVALID) &&
	      (invalid(ix,iy+1,iz) != GCAM_POSITION_INVALID) &&
	      (invalid(ix,iy,iz+1) != GCAM_POSITION_INVALID) ) {
	    
	    num++;
	    

	    float3 vi = FetchVector(ix+1,iy  ,iz  ) - r;
	    float3 vj = FetchVector(ix  ,iy+1,iz  ) - r;
	    float3 vk = FetchVector(ix  ,iy  ,iz+1) - r;

	    float tmpArea = stp( vj, vk, vi );
	    if( tmpArea <= 0 ) {
	      neg = 1;
	    }

	    area1(ix,iy,iz) = tmpArea;
	    area(ix,iy,iz) += tmpArea;

	  }
	} else {
	  // Going to 'right' would fall out of the volume
	  area1(ix,iy,iz) = 0;
	}


	// Compute Jacobean determinants on the 'left'
	if( (ix>0) && (iy>0) && (iz>0) ) {
	  
	  // Check for validity
	  if( (invalid(ix-1,iy,iz) != GCAM_POSITION_INVALID) &&
	      (invalid(ix,iy-1,iz) != GCAM_POSITION_INVALID) &&
	      (invalid(ix,iy,iz-1) != GCAM_POSITION_INVALID) ) {
	    num++;

	    // I think this ordering preserves handedness
	    // It's different to that in gcamorph.c
	    float3 vi = r - FetchVector(ix-1,iy  ,iz  );
	    float3 vj = r - FetchVector(ix  ,iy-1,iz  );
	    float3 vk = r - FetchVector(ix  ,iy  ,iz-1);

	    float tmpArea = stp( vj, vk, vi );

	    if( tmpArea <= 0 ) {
	      neg = 1;
	    }

	    area2(ix,iy,iz) = tmpArea;
	    area(ix,iy,iz) += tmpArea;
	  }
	} else {
	  area2(ix,iy,iz) = 0;
	}

	// Check if at least one determinant was computed
	if( num > 0 ) {
	  // area is mean of 'left' and 'right' areas
	  area(ix,iy,iz) /= num;
	} else {
	  invalid(ix,iy,iz) = GCAM_AREA_INVALID;
	  area(ix,iy,iz) = 0;
	}

	// Keep track of sign changes
	if( (invalid(ix,iy,iz)==GCAM_VALID) &&
	    neg &&
	    origArea(ix,iy,iz) > 0 ) {
	  atomicAdd( &(globals[iCMPGlobalsNeg]), 1 );
	}

	// Increment invalid counter
	if( invalid(ix,iy,iz) != GCAM_VALID ) {
	  // We need to test again
	  atomicAdd( &(globals[iCMPGlobalsInvalid]), 1 );
	}
      }
    }
    
    void GCAmorphGPU::ComputeMetricProperties( int& invalid ) {
      /*!
	Routine to duplicate gcamComputeMetricProperties
	from the file gcamorph.c.
	It essentially computes a lot of jacobean determinants
	and sums them up.
	The argument \a invalid is used to return the number of
	invalid locations found, a task performed by the
	global variable \c Ginvalid in gcamorph.c.
      */


      GCAmorphGPU::tCMPtot.Start();

      // Sanity check
      this->CheckIntegrity();

      // Allocate temporary on the device to hold invalid and neg
      int *d_globals;
      CUDA_SAFE_CALL( cudaMalloc( (void**)&d_globals, 2*sizeof(int) ) );
      CUDA_SAFE_CALL( cudaMemset( d_globals, 0, 2*sizeof(int) ) );

      // Get the d_rx, d_ry and d_rz fields bound to textures
      this->d_rx.AllocateArray();
      this->d_ry.AllocateArray();
      this->d_rz.AllocateArray();
      this->d_rx.SendArray();
      this->d_ry.SendArray();
      this->d_rz.SendArray();

      dt_rx.normalized = false;
      dt_rx.addressMode[0] = cudaAddressModeClamp;
      dt_rx.addressMode[1] = cudaAddressModeClamp;
      dt_rx.addressMode[2] = cudaAddressModeClamp;
      dt_rx.filterMode = cudaFilterModePoint;

      dt_ry.normalized = false;
      dt_ry.addressMode[0] = cudaAddressModeClamp;
      dt_ry.addressMode[1] = cudaAddressModeClamp;
      dt_ry.addressMode[2] = cudaAddressModeClamp;
      dt_ry.filterMode = cudaFilterModePoint;

      dt_rz.normalized = false;
      dt_rz.addressMode[0] = cudaAddressModeClamp;
      dt_rz.addressMode[1] = cudaAddressModeClamp;
      dt_rz.addressMode[2] = cudaAddressModeClamp;
      dt_rz.filterMode = cudaFilterModePoint;
      
      CUDA_SAFE_CALL( cudaBindTextureToArray( dt_rx, this->d_rx.GetArray() ) );
      CUDA_SAFE_CALL( cudaBindTextureToArray( dt_ry, this->d_ry.GetArray() ) );
      CUDA_SAFE_CALL( cudaBindTextureToArray( dt_rz, this->d_rz.GetArray() ) );
      

      // Run the kernel
      dim3 grid, threads;

      threads.x = threads.y = kCMPKernelSize;
      threads.z = 1;

      grid = this->d_rx.CoverBlocks( kCMPKernelSize );
      grid.z = 1;

      GCAmorphGPU::tCMPcompute.Start();
      CompMetPropKernel<<<grid,threads>>>
	( this->d_origArea, this->d_invalid,
	  this->d_area, this->d_area1, this->d_area2,
	  d_globals );
      CUDA_CHECK_ERROR( "CompMetPropKernel failed!\n" );
      GCAmorphGPU::tCMPcompute.Stop();

      // Retrieve global statistics
      int globals[2];
      CUDA_SAFE_CALL( cudaMemcpy( &globals, d_globals,
				  2*sizeof(int),
				  cudaMemcpyDeviceToHost ) );
      invalid = globals[iCMPGlobalsInvalid];
      this->neg = globals[iCMPGlobalsNeg];

      // Release device temporary
      CUDA_SAFE_CALL( cudaFree( d_globals ) );

      // Unbind the textures
      CUDA_SAFE_CALL( cudaUnbindTexture( dt_rx ) );
      CUDA_SAFE_CALL( cudaUnbindTexture( dt_ry ) );
      CUDA_SAFE_CALL( cudaUnbindTexture( dt_rz ) );



      GCAmorphGPU::tCMPtot.Stop();
    }


    // --------------------------------------------



    void GCAmorphGPU::ClearGradient( void ) {
      this->d_dx.Zero();
      this->d_dy.Zero();
      this->d_dz.Zero();
    }

    void GCAmorphGPU::ClearMomentum( void ) {
      this->d_odx.Zero();
      this->d_ody.Zero();
      this->d_odz.Zero();
    }

    
    // --------------------------------------------

    const unsigned int kApplyGradientKernelSize = 16;

    __device__ void FetchDerivs( const unsigned int ix,
				 const unsigned int iy,
				 const unsigned int iz,
				 float& dx, float& dy, float& dz ) {

      const float xLoc = ix+0.5f;
      const float yLoc = iy+0.5f;
      const float zLoc = iz+0.5f;

      dx = tex3D( dt_dx, xLoc, yLoc, zLoc );
      dy = tex3D( dt_dy, xLoc, yLoc, zLoc );
      dz = tex3D( dt_dz, xLoc, yLoc, zLoc );
    }

    __global__
    void ApplyGradientKernel( const VolumeArgGPU<char> invalid,
			      VolumeArgGPU<float> odx,
			      VolumeArgGPU<float> ody,
			      VolumeArgGPU<float> odz,
			      VolumeArgGPU<float> rx,
			      VolumeArgGPU<float> ry,
			      VolumeArgGPU<float> rz,
			      const float dt, const float momentum ) {
      const unsigned int bx = ( blockIdx.x * blockDim.x );
      const unsigned int by = ( blockIdx.y * blockDim.y );
      const unsigned int ix = threadIdx.x + bx;
      const unsigned int iy = threadIdx.y + by;

      for( unsigned int iz = 0; iz< invalid.dims.z; iz++ ) {
	if( invalid.InVolume(ix,iy,iz) ) {

	  if( invalid(ix,iy,iz) == GCAM_POSITION_INVALID ) {
	    continue;
	  }

	  // Fetch the dx, dy and dz values from textures
	  float gcamdx, gcamdy, gcamdz;
	  FetchDerivs( ix, iy, iz, gcamdx, gcamdy, gcamdz );

	  float ldx, ldy, ldz;

	  ldx = gcamdx*dt + odx(ix,iy,iz)*momentum;
	  ldy = gcamdy*dt + ody(ix,iy,iz)*momentum;
	  ldz = gcamdz*dt + odz(ix,iy,iz)*momentum;

	  // Update odx, ody, odz
	  odx(ix,iy,iz) = ldx;
	  ody(ix,iy,iz) = ldy;
	  odz(ix,iy,iz) = ldz;

	  // Update x, y z
	  rx(ix,iy,iz) += ldx;
	  ry(ix,iy,iz) += ldy;
	  rz(ix,iy,iz) += ldz;
	}
      }
    }

    void GCAmorphGPU::ApplyGradient( GCA_MORPH_PARMS *parms ) {
      
      // Start with a sanity check
      this->CheckIntegrity();

      // Put dx, dy and dz into textures
      this->d_dx.AllocateArray();
      this->d_dy.AllocateArray();
      this->d_dz.AllocateArray();
      this->d_dx.SendArray();
      this->d_dy.SendArray();
      this->d_dz.SendArray();

      dt_dx.normalized = false;
      dt_dx.addressMode[0] = cudaAddressModeClamp;
      dt_dx.addressMode[1] = cudaAddressModeClamp;
      dt_dx.addressMode[2] = cudaAddressModeClamp;
      dt_dx.filterMode = cudaFilterModePoint;

      dt_dy.normalized = false;
      dt_dy.addressMode[0] = cudaAddressModeClamp;
      dt_dy.addressMode[1] = cudaAddressModeClamp;
      dt_dy.addressMode[2] = cudaAddressModeClamp;
      dt_dy.filterMode = cudaFilterModePoint;

      dt_dz.normalized = false;
      dt_dz.addressMode[0] = cudaAddressModeClamp;
      dt_dz.addressMode[1] = cudaAddressModeClamp;
      dt_dz.addressMode[2] = cudaAddressModeClamp;
      dt_dz.filterMode = cudaFilterModePoint;
      
      CUDA_SAFE_CALL( cudaBindTextureToArray( dt_dx, this->d_dx.GetArray() ) );
      CUDA_SAFE_CALL( cudaBindTextureToArray( dt_dy, this->d_dy.GetArray() ) );
      CUDA_SAFE_CALL( cudaBindTextureToArray( dt_dz, this->d_dz.GetArray() ) );
      
      

      // Run the computation
      dim3 grid, threads;
      
      threads.x = threads.y = kApplyGradientKernelSize;
      threads.z = 1;

      grid = this->d_invalid.CoverBlocks( kApplyGradientKernelSize );
      grid.z = 1;

      ApplyGradientKernel<<<grid,threads>>>
	( this->d_invalid,
	  this->d_odx, this->d_ody, this->d_odz,
	  this->d_rx, this->d_ry, this->d_rz,
	  parms->dt, parms->momentum );
      CUDA_CHECK_ERROR( "ApplyGradientKernel failed!\n" );


      // Unbind the textures
      CUDA_SAFE_CALL( cudaUnbindTexture( dt_dx ) );
      CUDA_SAFE_CALL( cudaUnbindTexture( dt_dy ) );
      CUDA_SAFE_CALL( cudaUnbindTexture( dt_dz ) );


      // Something we can't do yet....
      if (!DZERO(parms->l_area_intensity)) {
	std::cerr << __FUNCTION__ 
		  << ": gcamCreateNodeLookupTable not implemented!"
		  << std::endl;
	exit( EXIT_FAILURE );
      }

    }

    // --------------------------------------------
    
    const unsigned int kUndoGradientKernelSize = 16;
    
    __global__
    void UndoGradientKernel( const VolumeArgGPU<char> invalid,
			     VolumeArgGPU<float> odx,
			     VolumeArgGPU<float> ody,
			     VolumeArgGPU<float> odz,
			     VolumeArgGPU<float> rx,
			     VolumeArgGPU<float> ry,
			     VolumeArgGPU<float> rz ) {
      const unsigned int bx = ( blockIdx.x * blockDim.x );
      const unsigned int by = ( blockIdx.y * blockDim.y );
      const unsigned int ix = threadIdx.x + bx;
      const unsigned int iy = threadIdx.y + by;
      
      for( unsigned int iz = 0; iz< invalid.dims.z; iz++ ) {
	if( invalid.InVolume(ix,iy,iz) ) {
	  
	  if( invalid(ix,iy,iz) == GCAM_POSITION_INVALID ) {
	    continue;
	  }
	  
	  float ldx = odx(ix,iy,iz);
	  float ldy = ody(ix,iy,iz);
	  float ldz = odz(ix,iy,iz);
	  
	  // Update odx, ody, odz
	  odx(ix,iy,iz) = 0;
	  ody(ix,iy,iz) = 0;
	  odz(ix,iy,iz) = 0;
	  
	  // Update x, y z
	  rx(ix,iy,iz) -= ldx;
	  ry(ix,iy,iz) -= ldy;
	    rz(ix,iy,iz) -= ldz;
	}
      }
    }


    void GCAmorphGPU::UndoGradient( void ) {

      this->CheckIntegrity();
      
      // Run the computation
      dim3 grid, threads;
      
      threads.x = threads.y = kUndoGradientKernelSize;
      threads.z = 1;

      grid = this->d_invalid.CoverBlocks( kUndoGradientKernelSize );
      grid.z = 1;

      UndoGradientKernel<<<grid,threads>>>
	( this->d_invalid,
	  this->d_odx, this->d_ody, this->d_odz,
	  this->d_rx, this->d_ry, this->d_rz );
      CUDA_CHECK_ERROR( "UndoGradientKernel failed!\n" );
    }

    // --------------------------------------------

    const unsigned int kAddStatusKernelSize = 16;

    __global__
    void AddStatusKernel( VolumeArgGPU<int> status, const int addState ) {
      const unsigned int bx = ( blockIdx.x * blockDim.x );
      const unsigned int by = ( blockIdx.y * blockDim.y );
      const unsigned int ix = threadIdx.x + bx;
      const unsigned int iy = threadIdx.y + by;
      
      for( unsigned int iz = 0; iz< status.dims.z; iz++ ) {
	if( status.InVolume(ix,iy,iz) ) {
	  status(ix,iy,iz) |= addState;
	}
      }
    }

    void GCAmorphGPU::AddStatus( const int addState ) {

      this->CheckIntegrity();

      // Run the computation
      dim3 grid, threads;
      
      threads.x = threads.y = kAddStatusKernelSize;
      threads.z = 1;

      grid = this->d_status.CoverBlocks( kAddStatusKernelSize );
      grid.z = 1;
      
      AddStatusKernel<<<grid,threads>>>( this->d_status, addState );
      CUDA_CHECK_ERROR( "AddStatusKernel failed!" );
    }


    // --------------------------------------------

    const unsigned int kRemoveStatusKernelSize = 16;

    __global__
    void RemoveStatusKernel( VolumeArgGPU<int> status, const int subtractState ) {
      const unsigned int bx = ( blockIdx.x * blockDim.x );
      const unsigned int by = ( blockIdx.y * blockDim.y );
      const unsigned int ix = threadIdx.x + bx;
      const unsigned int iy = threadIdx.y + by;
      
      const int invState = ~subtractState;

      for( unsigned int iz = 0; iz< status.dims.z; iz++ ) {
	if( status.InVolume(ix,iy,iz) ) {
	  status(ix,iy,iz) &= invState;
	}
      }
    }

    void GCAmorphGPU::RemoveStatus( const int subtractState ) {

      this->CheckIntegrity();

      // Run the computation
      dim3 grid, threads;
      
      threads.x = threads.y = kRemoveStatusKernelSize;
      threads.z = 1;

      grid = this->d_status.CoverBlocks( kRemoveStatusKernelSize );
      grid.z = 1;
      
      RemoveStatusKernel<<<grid,threads>>>( this->d_status, subtractState );
      CUDA_CHECK_ERROR( "AddStatusKernel failed!" );
    }
    
    // --------------------------------------------

    void GCAmorphGPU::ResetLabelNodeStatus( void ) {
      this->RemoveStatus( GCAM_LABEL_NODE );
      this->RemoveStatus( GCAM_IGNORE_LIKELIHOOD );
    }
    

    // ----------------------------------------------------
    void GCAmorphGPU::ShowTimings( void ) {
#ifdef CUDA_SHOW_TIMINGS
      std::cout << "==================================" << std::endl;
      std::cout << "GCAmorphGPU timers" << std::endl;
      std::cout << "------------------" << std::endl;
#ifndef CUDA_FORCE_SYNC
      std::cout << "WARNING: CUDA_FORCE_SYNC not #defined" << std::endl;
      std::cout << "Timings may not be accurate" << std::endl;
#endif
      std::cout << std::endl;

      std::cout << "Send:" << std::endl;
      std::cout << "      Pack : " << GCAmorphGPU::tSendPack << std::endl;
      std::cout << "  Transfer : " << GCAmorphGPU::tSendTransfer << std::endl;
      std::cout << "Total      : " << GCAmorphGPU::tSendTot << std::endl;
      std::cout << std::endl;

      std::cout << "Recv:" << std::endl;
      std::cout << "      Pack : " << GCAmorphGPU::tRecvPack << std::endl;
      std::cout << "  Transfer : " << GCAmorphGPU::tRecvTransfer << std::endl;
      std::cout << "Total      : " << GCAmorphGPU::tRecvTot << std::endl;
      std::cout << std::endl;

      std::cout << "Host Memory:" << std::endl;
      std::cout << "     Alloc : " << GCAmorphGPU::tHostAlloc << std::endl;
      std::cout << "   Release : " << GCAmorphGPU::tHostRelease << std::endl;
      std::cout << " Randomise : " << GCAmorphGPU::tHostRandomise << std::endl;
      std::cout << std::endl;

      std::cout << "Compute Metric Properties:" << std::endl;
      std::cout << "   Compute : " << GCAmorphGPU::tCMPcompute << std::endl;
      std::cout << "Total      : " << GCAmorphGPU::tCMPtot << std::endl;
      std::cout << std::endl;

      std::cout << "==================================" << std::endl;
#endif
    }
    

    
    // Define static members
    SciGPU::Utilities::Chronometer GCAmorphGPU::tSendTot;
    SciGPU::Utilities::Chronometer GCAmorphGPU::tSendPack;
    SciGPU::Utilities::Chronometer GCAmorphGPU::tSendTransfer;
    SciGPU::Utilities::Chronometer GCAmorphGPU::tRecvTot;
    SciGPU::Utilities::Chronometer GCAmorphGPU::tRecvPack;
    SciGPU::Utilities::Chronometer GCAmorphGPU::tRecvTransfer;
    SciGPU::Utilities::Chronometer GCAmorphGPU::tHostAlloc;
    SciGPU::Utilities::Chronometer GCAmorphGPU::tHostRelease;
    SciGPU::Utilities::Chronometer GCAmorphGPU::tHostRandomise;
    SciGPU::Utilities::Chronometer GCAmorphGPU::tCMPtot;
    SciGPU::Utilities::Chronometer GCAmorphGPU::tCMPcompute;


    dim3 GCAmorphGPU::hostDims = make_uint3(0,0,0);
    float *GCAmorphGPU::h_rx, *GCAmorphGPU::h_ry, *GCAmorphGPU::h_rz;
    float *GCAmorphGPU::h_origx, *GCAmorphGPU::h_origy, *GCAmorphGPU::h_origz;
    float *GCAmorphGPU::h_dx, *GCAmorphGPU::h_dy, *GCAmorphGPU::h_dz;
    float *GCAmorphGPU::h_odx, *GCAmorphGPU::h_ody, *GCAmorphGPU::h_odz;
    float *GCAmorphGPU::h_origArea, *GCAmorphGPU::h_origArea1, *GCAmorphGPU::h_origArea2;
    float *GCAmorphGPU::h_area, *GCAmorphGPU::h_area1, *GCAmorphGPU::h_area2;
    char *GCAmorphGPU::h_invalid;
    int *GCAmorphGPU::h_label, *GCAmorphGPU::h_status;
    float *GCAmorphGPU::h_labelDist;
    float *GCAmorphGPU::h_mean;
    float *GCAmorphGPU::h_variance;



    void GCAmorphGPU::AllocateHost( const GCAmorphGPU& gcam ) {
      
      // Check integrity
      gcam.CheckIntegrity();

      // Check if current allocation OK
      const dim3 gcamDims = gcam.d_rx.GetDims();
      const size_t reqSize = gcamDims.x * gcamDims.y * gcamDims.z;
      size_t currSize = GCAmorphGPU::hostDims.x * GCAmorphGPU::hostDims.y * GCAmorphGPU::hostDims.z;

      if( reqSize <= currSize ) {
	return;
      }

      std::cerr << __FUNCTION__
		<< ": Warning - not thread safe!" << std::endl;

      // Get rid of the old allocation
      GCAmorphGPU::ReleaseHost();

      GCAmorphGPU::tHostAlloc.Start();
      // Set dimensions
      GCAmorphGPU::hostDims = gcam.d_rx.GetDims();

      // Do the allocations
      GCAmorphGPU::h_rx = gcam.d_rx.AllocateHostBuffer();
      GCAmorphGPU::h_ry = gcam.d_ry.AllocateHostBuffer();
      GCAmorphGPU::h_rz = gcam.d_rz.AllocateHostBuffer();
     
      GCAmorphGPU::h_origx = gcam.d_origx.AllocateHostBuffer();
      GCAmorphGPU::h_origy = gcam.d_origy.AllocateHostBuffer();
      GCAmorphGPU::h_origz = gcam.d_origz.AllocateHostBuffer();

      GCAmorphGPU::h_dx = gcam.d_dx.AllocateHostBuffer();
      GCAmorphGPU::h_dy = gcam.d_dy.AllocateHostBuffer();
      GCAmorphGPU::h_dz = gcam.d_dz.AllocateHostBuffer();

      GCAmorphGPU::h_odx = gcam.d_odx.AllocateHostBuffer();
      GCAmorphGPU::h_ody = gcam.d_ody.AllocateHostBuffer();
      GCAmorphGPU::h_odz = gcam.d_odz.AllocateHostBuffer();

      GCAmorphGPU::h_origArea = gcam.d_origArea.AllocateHostBuffer();
      GCAmorphGPU::h_origArea1 = gcam.d_origArea1.AllocateHostBuffer();
      GCAmorphGPU::h_origArea2 = gcam.d_origArea2.AllocateHostBuffer();

      GCAmorphGPU::h_area = gcam.d_area.AllocateHostBuffer();
      GCAmorphGPU::h_area1 = gcam.d_area1.AllocateHostBuffer();
      GCAmorphGPU::h_area2 = gcam.d_area2.AllocateHostBuffer();

      GCAmorphGPU::h_invalid = gcam.d_invalid.AllocateHostBuffer();
      GCAmorphGPU::h_status = gcam.d_status.AllocateHostBuffer();
      GCAmorphGPU::h_label = gcam.d_label.AllocateHostBuffer();
      GCAmorphGPU::h_labelDist = gcam.d_labelDist.AllocateHostBuffer();

      GCAmorphGPU::h_mean = gcam.d_mean.AllocateHostBuffer();
      GCAmorphGPU::h_variance = gcam.d_variance.AllocateHostBuffer();

      GCAmorphGPU::tHostAlloc.Stop();

    }


    
    void GCAmorphGPU::ReleaseHost( void ) {

      // Sanity check
      if( GCAmorphGPU::hostDims == make_uint3(0,0,0) ) {
	return;
      }

      std::cerr << __FUNCTION__
		<< ": Warning - not thread safe!" << std::endl;

      GCAmorphGPU::tHostRelease.Start();

      GCAmorphGPU::hostDims = make_uint3(0,0,0);

      // Release page-locked host memory
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_rx ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_ry ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_rz ) );

      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_origx ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_origy ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_origz ) );

      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_dx ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_dy ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_dz ) );

      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_odx ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_ody ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_odz ) );

      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_origArea ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_origArea1 ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_origArea2 ) );

      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_area ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_area1 ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_area2 ) );

      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_invalid ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_status ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_label ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_labelDist ) );

      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_mean ) );
      CUDA_SAFE_CALL( cudaFreeHost( GCAmorphGPU::h_variance ) );

      GCAmorphGPU::tHostRelease.Stop();
    }


    

    template<typename T>
    void RandomArray( T* arr, const size_t nVals ) {
      
      for( unsigned int i=0; i<nVals; i++ ) {
	char randVal = std::rand() % std::numeric_limits<char>::max();
	arr[i] = static_cast<T>(randVal);
      }

    }


    void GCAmorphGPU::RandomiseHost( void ) {

      // Sanity check
      if( GCAmorphGPU::hostDims == make_uint3(0,0,0) ) {
	return;
      }

      std::cerr << __FUNCTION__
		<< ": Warning - not thread safe!" << std::endl;

      GCAmorphGPU::tHostRandomise.Start();

      size_t currSize = GCAmorphGPU::hostDims.x *
	GCAmorphGPU::hostDims.y * GCAmorphGPU::hostDims.z;

      RandomArray( GCAmorphGPU::h_rx, currSize );
      RandomArray( GCAmorphGPU::h_ry, currSize );
      RandomArray( GCAmorphGPU::h_rz, currSize );

      
      RandomArray( GCAmorphGPU::h_origx, currSize );
      RandomArray( GCAmorphGPU::h_origy, currSize );
      RandomArray( GCAmorphGPU::h_origz, currSize );

      RandomArray( GCAmorphGPU::h_dx, currSize );
      RandomArray( GCAmorphGPU::h_dy, currSize );
      RandomArray( GCAmorphGPU::h_dz, currSize );

      RandomArray( GCAmorphGPU::h_odx, currSize );
      RandomArray( GCAmorphGPU::h_ody, currSize );
      RandomArray( GCAmorphGPU::h_odz, currSize );

      RandomArray( GCAmorphGPU::h_origArea, currSize );
      RandomArray( GCAmorphGPU::h_origArea1, currSize );
      RandomArray( GCAmorphGPU::h_origArea2, currSize );

      RandomArray( GCAmorphGPU::h_area, currSize );
      RandomArray( GCAmorphGPU::h_area1, currSize );
      RandomArray( GCAmorphGPU::h_area2, currSize );

      RandomArray( GCAmorphGPU::h_invalid, currSize );
      RandomArray( GCAmorphGPU::h_status, currSize );
      RandomArray( GCAmorphGPU::h_label, currSize );
      RandomArray( GCAmorphGPU::h_labelDist, currSize );

      RandomArray( GCAmorphGPU::h_mean, currSize );
      RandomArray( GCAmorphGPU::h_variance, currSize );

      
      GCAmorphGPU::tHostRandomise.Stop();
    }

  }
}


void gcamClearGradientGPU( GCA_MORPH* gcam ) {
  GPU::Classes::GCAmorphGPU gcamGPU;
  
  gcamGPU.SendAll( gcam );
  gcamGPU.ClearGradient();
  gcamGPU.RecvAll( gcam );

}

void gcamClearMomentumGPU( GCA_MORPH* gcam ) {
  GPU::Classes::GCAmorphGPU gcamGPU;
  
  gcamGPU.SendAll( gcam );
  gcamGPU.ClearMomentum();
  gcamGPU.RecvAll( gcam );

}


void gcamComputeMetricPropertiesGPU( GCA_MORPH* gcam,
				     int *invalid ) {
  /*!
    This is a wrapper around the CUDA implementation
    of gcamComputeMetricProperties
  */

  GPU::Classes::GCAmorphGPU gcamGPU;
  
  gcamGPU.SendAll( gcam );
  gcamGPU.ComputeMetricProperties( *invalid );
  gcamGPU.RecvAll( gcam );

}


void gcamApplyGradientGPU( GCA_MORPH *gcam, GCA_MORPH_PARMS *parms ) {

  GPU::Classes::GCAmorphGPU gcamGPU;
  
  gcamGPU.SendAll( gcam );
  gcamGPU.ApplyGradient( parms );
  gcamGPU.RecvAll( gcam );
}


void gcamUndoGradientGPU( GCA_MORPH *gcam ) {

  GPU::Classes::GCAmorphGPU gcamGPU;
  
  gcamGPU.SendAll( gcam );
  gcamGPU.UndoGradient();
  gcamGPU.RecvAll( gcam );
}


void gcamAddStatusGPU( GCA_MORPH *gcam, const int statusFlags ) {

  GPU::Classes::GCAmorphGPU gcamGPU;
  
  gcamGPU.SendAll( gcam );
  gcamGPU.AddStatus( statusFlags );
  gcamGPU.RecvAll( gcam );
}


void gcamRemoveStatusGPU( GCA_MORPH *gcam, const int statusFlags ) {

  GPU::Classes::GCAmorphGPU gcamGPU;
  
  gcamGPU.SendAll( gcam );
  gcamGPU.RemoveStatus( statusFlags );
  gcamGPU.RecvAll( gcam );
}

#endif
