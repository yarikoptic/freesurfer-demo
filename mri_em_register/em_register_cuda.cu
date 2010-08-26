/**
 * @file  em_register_cuda.cu
 * @brief Holds em_register CUDA routines
 *
 * Contains CUDA routines for em_register
 */
/*
 * Original Author: Richard Edgar
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2010/08/26 00:00:55 $
 *    $Revision: 1.3.2.1 $
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

#include <iostream>
#include <iomanip>
using namespace std;


#include <thrust/device_new_allocator.h>
#include <thrust/device_ptr.h>
#include <thrust/extrema.h>
#include <thrust/reduce.h>

#include "cudacheck.h"

#include "mriframegpu.hpp"
#include "affinegpu.hpp"
#include "gcasgpu.hpp"

#include "generators.hpp"
#include "cudatypeutils.hpp"

#include "em_register_cuda.h"

// ==================================================================




static GPU::Classes::MRIframeGPU<unsigned char> src_uchar;
texture<unsigned char, 3, cudaReadModeElementType> dt_mri;  // 3D texture

static GPU::Classes::GCASampleGPU myGCAS;

const unsigned int  kCalcLogPKernelSize = 256;
const unsigned int kOptimiseBlockSize = 256;


const unsigned int nIndices = 9;
enum Indices{ iMinTrans=0, iMaxTrans=1, inTrans=2,
	      iMinScale=3, iMaxScale=4, inScale=5,
	      iMinRot=6, iMaxRot=7, inRot=8 };

__constant__ float dc_TransformParams[nIndices];





// =================================================================
// Device Utility functions


//! Extracts data from the MRI texture
__device__ float MRIlookup( const float3 r ) {
  /*!
    Performs a lookup into the MRI texture
  */

  // Offset by 0.5, since texture values are assumed to be at voxel centres

  return( tex3D( dt_mri, r.x+0.5f, r.y+0.5f, r.z+0.5f ) );
}



//! Computes the log_p value for a single point.
__device__ float ComputeLogP( const float val, const float mean,
			      const float prior, const float covar ) {
  
  float det = covar;

  float v = val - mean;

  float log_p = - logf( sqrtf( det ) ) - 0.5*( v*v / covar ) + logf( prior );

  if( log_p < -3 ) {
    log_p = -3;
  }

  return( log_p );
}



//! Routine to sum all the logps for a given transform in shared memory
__device__ float SumLogPs( const GPU::Classes::AffineTransShared &afTrans,
			   const GPU::Classes::GCASonGPU& gcas ) {
  // The accumulator array for this block
  __shared__ float myLogps[kOptimiseBlockSize];
  myLogps[threadIdx.x] = 0;
  
  // Accumulate log p values in shared memory
  for( unsigned int i=0; i<gcas.nSamples; i+= kOptimiseBlockSize ) {
    if( (i + threadIdx.x) < gcas.nSamples ) {
      float3 rOut = afTrans.transform( gcas.GetLocation( i+threadIdx.x ) );

      float mriVal = MRIlookup( rOut );

      myLogps[threadIdx.x] += ComputeLogP( mriVal,
					   gcas.means[i+threadIdx.x],
					   gcas.priors[i+threadIdx.x],
					   gcas.covars[i+threadIdx.x] );
    }
  }

  __syncthreads();

  // Perform reduction sum
#if 0
  // Slow but always correct version
  for( unsigned int d=blockDim.x / 2; d>0; d>>=1 ) {
    if( threadIdx.x < d ) { 
      myLogps[threadIdx.x] += myLogps[threadIdx.x+d];
    }
    __syncthreads();
  }
#else
  // Version optimised for a warpsize of 32
  for( unsigned int d=blockDim.x / 2; d>32; d>>=1 ) {
    if( threadIdx.x < d ) { 
      myLogps[threadIdx.x] += myLogps[threadIdx.x+d];
    }
    __syncthreads();
  }

  if( threadIdx.x < 32 ) {
    myLogps[threadIdx.x] += myLogps[threadIdx.x+32];
    myLogps[threadIdx.x] += myLogps[threadIdx.x+16];
    myLogps[threadIdx.x] += myLogps[threadIdx.x+8];
    myLogps[threadIdx.x] += myLogps[threadIdx.x+4];
    myLogps[threadIdx.x] += myLogps[threadIdx.x+2];
    myLogps[threadIdx.x] += myLogps[threadIdx.x+1];
  }
#endif

  return( myLogps[0] );
}




// ===================================================================
// Device Kernels


//! Kernel to compute log_p for a single transform array
__global__
void ComputeAllLogP( const GPU::Classes::AffineTransformation afTrans,
		     const GPU::Classes::GCASonGPU gcas,
		     float *logps ) {
  /*!
    Driver kernel to compute the value of log_p for every sample.
    Uses the MRI texture and transform matrix stored in constant memory
  */

  const unsigned int iSample = (blockIdx.x*blockDim.x) + threadIdx.x;

  if( iSample >= gcas.nSamples ) {
    // Nothing to do
    return;
  }

  // Compute location with affine transform
  float3 rOut = afTrans.transform( gcas.GetLocation( iSample ) );

  float mriVal = MRIlookup( rOut );


  logps[iSample] = ComputeLogP( mriVal,
				gcas.means[iSample],
				gcas.priors[iSample],
				gcas.covars[iSample] );
}


//! Kernel to compute all probabilities for a given translation generator
__global__
void TranslationLogps( const GPU::Classes::AffineTransformation base,
				  const TranslationGenerator tGen,
				  const GPU::Classes::GCASonGPU gcas,
				  float *logps ) {

  // Find our translation
  float3 myTrans = tGen( blockIdx.x );
  
  __shared__ float m1[GPU::Classes::AffineTransShared::kMatrixSize];
  __shared__ float m2[GPU::Classes::AffineTransShared::kMatrixSize];

  GPU::Classes::AffineTransShared final( m1 ), translation( m2 );

  // Recast the input transformation (which will be in shared memory)
  // as an AffineTransShared. Unfortunately, this needs a const_cast
  const float* tmp = base.GetPointer();
  const GPU::Classes::AffineTransShared bAff( const_cast<float*>(tmp) );

  // Make identity transform
  translation.SetIdentity();

  // Note that we invert the translation
  translation.SetTranslation( -myTrans );

  // Reverse order of multiplications, as compared to mri_em_register.c
  final.Multiply( bAff, translation );
  
  __syncthreads();

  // -- All threads now have access to the transformation

  
  // Compute the final result

  float myLogP = SumLogPs( final, gcas );

  if( threadIdx.x == 0 ) {
    logps[ blockIdx.x ] = myLogP;
  }
}



//! Kernel to compute all transform probabilities for a given transform generator
__global__
void TransformLogps( const GPU::Classes::AffineTransformation base,
		     const float3 originTranslation,
		     const GPU::Classes::GCASonGPU gcas,
		     float *logps ) {

  
  const long b1d = blockIdx.x + ( blockIdx.y * gridDim.x );

  // Find our transform
  LinearGenerator translate( dc_TransformParams[iMinTrans],
			     dc_TransformParams[iMaxTrans],
			     dc_TransformParams[inTrans] );
  LinearGenerator scale( dc_TransformParams[iMinScale],
			 dc_TransformParams[iMaxScale],
			 dc_TransformParams[inScale] );
  LinearGenerator rotate( dc_TransformParams[iMinRot],
			  dc_TransformParams[iMaxRot],
			  dc_TransformParams[inRot] );

  TransformGenerator tGen( translate, scale, rotate );

  float3 myTrans, myScale, myRot;

  tGen.GetTransform( b1d, myTrans, myScale, myRot );


  // Invert the transforms
  myTrans = -myTrans;
  myScale.x = 1/myScale.x;
  myScale.y = 1/myScale.y;
  myScale.z = 1/myScale.z;
  myRot = -myRot;


  // Compute the transform matrix
  __shared__ float m1[GPU::Classes::AffineTransShared::kMatrixSize];
  __shared__ float m2[GPU::Classes::AffineTransShared::kMatrixSize];
  __shared__ float m3[GPU::Classes::AffineTransShared::kMatrixSize];

  GPU::Classes::AffineTransShared A( m1 ), B( m2 ), C( m3 ) ;

  // Recast the input transformations (which will be in shared memory)
  // as an AffineTransShared. Unfortunately, this needs a const_cast
  const float* tmp = base.GetPointer();
  const GPU::Classes::AffineTransShared bAff( const_cast<float*>(tmp) );
  
  // Invert the order in the original find_optimal_linear_xform routine
  B.SetIdentity();
  B.SetTranslation( originTranslation );

  A.Multiply( bAff, B );
  
  B.SetIdentity();
  B.SetXRotation( myRot.x );
  
  C.Multiply( A, B );

  A.SetIdentity();
  A.SetYRotation( myRot.y );

  B.Multiply( C, A );

  A.SetIdentity();
  A.SetZRotation( myRot.z );
  
  C.Multiply( B, A );

  A.SetIdentity();
  A.SetScaling( myScale );

  B.Multiply( C, A );

  A.SetIdentity();
  A.SetTranslation( -originTranslation );

  C.Multiply( B, A );

  A.SetIdentity();
  A.SetTranslation( myTrans );

  B.Multiply( C, A );

  __syncthreads();

  // -- All threads now have access to the transformation
  
  float myLogp = SumLogPs( B, gcas );

  // Write the final result
  if( threadIdx.x == 0 ) {
    logps[ b1d ] = myLogp;
  }
}





// ===================================================================
// External Functions

float CUDA_ComputeLogSampleProbability( const MATRIX *m_L ) {
/*!
    Re-implementation of local_GCAcomputeLogSampleProbability() from
    file mri_em_register.c.
    Assumes that robust is set to false, making the original routine
    a pass-through to GCAcomputeLogSampleProbability() from file
    gca.c.
    Also assumed that CUDA_em_register_Prepare has already
    set up everything
  */

  thrust::device_ptr<float> d_logpvals;
  MATRIX *inv_m_L = NULL;

  // Get the inverse of the transform matrix
  inv_m_L = MatrixInverse( (MATRIX*)m_L, inv_m_L );

  GPU::Classes::AffineTransformation myTransform( inv_m_L );
  const GPU::Classes::GCASonGPU myGCASonGPU( myGCAS );
  const unsigned int nsamples = myGCASonGPU.nSamples;

  d_logpvals = thrust::device_new<float>( nsamples );

  // ---------------------------------------
  float logps;

  // Run the log_p evaluation kernel
  dim3 grid, threads;
  threads.x = kCalcLogPKernelSize;
  threads.y = threads.z = 1;
  grid.x = static_cast<int>( ceilf ( static_cast<float>(nsamples) / threads.x ) );
  grid.y = grid.z = 1;

  ComputeAllLogP<<<grid,threads>>>( myTransform, myGCASonGPU,
				    thrust::raw_pointer_cast(d_logpvals) );
  CUDA_CHECK_ERROR( "ComputeAllLogP kernel failed!\n" );

  // Do the reduction
  logps = thrust::reduce( d_logpvals, d_logpvals+nsamples );

  MatrixFree( &inv_m_L );
  thrust::device_delete( d_logpvals );

  return( logps );
}





void CUDA_FindOptimalTranslation( const MATRIX *baseTransform,
				  const float minTrans,
				  const float maxTrans,
				  const unsigned int nTrans,
				  float *maxLogP,
				  float *dx,
				  float *dy,
				  float *dz ) {
  /*!
    Routine to find the best translation to match the
    MRI to the classifier array.
    A 'base' transform is supplied, and then translations
    within the given limits are searched
  */

  const unsigned int totalTrans = nTrans * nTrans * nTrans;

  // Device vector to hold logps
  thrust::device_ptr<float> d_logps;

  d_logps = thrust::device_new<float>( totalTrans );

  // Construct the generator which will give the required translations
  TranslationGenerator myGen( minTrans, maxTrans, nTrans );
  
  

  // Extract the 'base' transform, inverting
  MATRIX *invBaseTransform = NULL;
  invBaseTransform = MatrixInverse( baseTransform, invBaseTransform );
  GPU::Classes::AffineTransformation myBaseTransform( invBaseTransform );

  // Get the GCAsample array, which must already be on the GPU
  const GPU::Classes::GCASonGPU myGCASonGPU( myGCAS );

  // Compute all the probabilities
  dim3 grid, threads;

  threads.x = kOptimiseBlockSize;
  threads.y = threads.z = 1;

  grid.x = totalTrans;
  grid.y = grid.z = 1;

  TranslationLogps<<<grid,threads>>>( myBaseTransform,
				      myGen,
				      myGCAS,
				      thrust::raw_pointer_cast( d_logps ) );
  CUDA_CHECK_ERROR( "TranslationLogps failed!" );
 
#if 0
  for( unsigned int i=0; i<totalTrans; i++ ) {
    float3 translation = myGen(i);
    cout << __FUNCTION__  << " "
	 << setw(8) << setprecision(4) << translation.x << " "
	 << setw(8) << setprecision(4) << translation.y << " "
	 << setw(8) << setprecision(4) << translation.z << " "
	 << setw(12) << setprecision(8) << d_logps[i] << endl;
  }
#endif

  // Extract the maximum location
  thrust::device_ptr<float> maxLoc;
  maxLoc = thrust::max_element( d_logps, d_logps+totalTrans );

  // Get the maximum value
  *maxLogP = *maxLoc;

  // Convert the location to the required translation
  const int index = (maxLoc - d_logps);
  const float3 trans = myGen(index);


  *dx = trans.x;
  *dy = trans.y;
  *dz = trans.z;

  thrust::device_delete( d_logps );
}





void CUDA_FindOptimalTransform( const MATRIX *baseTransform,
				const MATRIX *originTranslation,
				const float minTrans,
				const float maxTrans,
				const unsigned int nTrans,
				const float minScale,
				const float maxScale,
				const unsigned nScale,
				const float minRot,
				const float maxRot,
				const unsigned int nRot,
				double *maxLogP,
				double *dx,
				double *dy,
				double *dz,
				double *sx,
				double *sy,
				double *sz,
				double *rx,
				double *ry,
				double *rz ) {
  /*!
    Routine to find the best transformation to match the
    MRI to the classifier array.
    A 'base' transform is supplied, and then transforms
    within the given limits are searched
  */

  const unsigned int totTranslate = nTrans * nTrans * nTrans;
  const unsigned int totOther = nScale * nScale * nScale *
    nRot * nRot * nRot;

  const unsigned int totalTransforms = totTranslate * totOther;

  // Device vector to hold logps
  thrust::device_ptr<float> d_logps;

  d_logps = thrust::device_new<float>( totalTransforms );

  // Construct the generator which will give the required translations
  LinearGenerator translate( minTrans, maxTrans, nTrans );
  LinearGenerator scale( minScale, maxScale, nScale );
  LinearGenerator rotate( minRot, maxRot, nRot );
  TransformGenerator myGen( translate, scale, rotate );

  // Get the transform generation parameters to the device
  float tParams[nIndices];

  tParams[iMinTrans] = minTrans;
  tParams[iMaxTrans] = maxTrans;
  tParams[inTrans] = nTrans;
  tParams[iMinScale] = minScale;
  tParams[iMaxScale] = maxScale;
  tParams[inScale] = nScale;
  tParams[iMinRot] = minRot;
  tParams[iMaxRot] = maxRot;
  tParams[inRot] = nRot;

  CUDA_SAFE_CALL( cudaMemcpyToSymbol( "dc_TransformParams",
				      tParams,
				      nIndices*sizeof(float),
				      0,
				      cudaMemcpyHostToDevice ) );

  // Extract the 'base' transform, inverting
  MATRIX *invBaseTransform = NULL;
  invBaseTransform = MatrixInverse( baseTransform, invBaseTransform );
  GPU::Classes::AffineTransformation myBaseTransform( invBaseTransform );

  // Extract the origin translation
  const float3 oTranslate = make_float3( originTranslation->rptr[1][4],
					 originTranslation->rptr[2][4],
					 originTranslation->rptr[3][4] );
  
  // Get the GCAsample array, which must already be on the GPU
  const GPU::Classes::GCASonGPU myGCASonGPU( myGCAS );

  dim3 grid, threads;

  // Compute all the probabilities
  threads.x = kOptimiseBlockSize;
  threads.y = threads.z = 1;

  grid.x = totTranslate;
  grid.y = totOther;
  grid.z = 1;

  TransformLogps<<<grid,threads>>>( myBaseTransform,
				    oTranslate,
				    myGCAS,
				    thrust::raw_pointer_cast( d_logps ) );
  CUDA_CHECK_ERROR( "TransformLogps failed!" );
  
  // Locate the maximum probability
  thrust::device_ptr<float> maxLoc;
  maxLoc = thrust::max_element( d_logps, d_logps+totalTransforms );

  // Extract the maximum probability
  *maxLogP = *maxLoc;


  // Convert the maximum probability to the transform parameters
  const int index = (maxLoc - d_logps);

  float3 myTrans, myScale, myRot;
  myGen.GetTransform( index, myTrans, myScale, myRot );

  *dx = myTrans.x;
  *dy = myTrans.y;
  *dz = myTrans.z;

  *sx = myScale.x;
  *sy = myScale.y;
  *sz = myScale.z;

  *rx = myRot.x;
  *ry = myRot.y;
  *rz = myRot.z;

  thrust::device_delete( d_logps );

}




// =================================================================


void CUDA_em_register_Prepare( GCA *gca,
			       GCA_SAMPLE *gcas,
			       const MRI *mri,
			       const int nSamples ) {


  // Sanity check
  if( gca->ninputs != 1 ) {
    cerr << __FUNCTION__ << ": Must have ninputs==1" << endl;
    exit( EXIT_FAILURE );
  }

  // ------------------------------
  // Send the MRI
  const unsigned int nFrame = 0;
  
  src_uchar.Allocate( mri );
  src_uchar.AllocateArray();
  src_uchar.Send( mri, nFrame );
  src_uchar.SendArray();

  // Bind to texture
  dt_mri.normalized = false;
  dt_mri.addressMode[0] = cudaAddressModeClamp;
  dt_mri.addressMode[1] = cudaAddressModeClamp;
  dt_mri.addressMode[2] = cudaAddressModeClamp;
  dt_mri.filterMode = cudaFilterModePoint;

  CUDA_SAFE_CALL( cudaBindTextureToArray( dt_mri, src_uchar.GetArray() ) );

  // Send the GCAS

  myGCAS.SendGPU( gca, gcas, mri, nSamples );

}


void CUDA_em_register_Release( void ) {
  CUDA_SAFE_CALL( cudaUnbindTexture( dt_mri ) );
}
