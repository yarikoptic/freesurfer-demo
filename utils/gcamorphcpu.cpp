/**
 * @file  gcamorphcpu.cpp
 * @brief Holds GCA morph data on the CPU
 *
 * 
 */
/*
 * Original Author: Richard Edgar
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2010/10/22 23:15:57 $
 *    $Revision: 1.5.2.1 $
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


#include "gcamorphcpu.hpp"



namespace Freesurfer {


  void GCAmorphCPU::CheckIntegrity( void ) const {
    bool good;

    good = this->rx.MatchDims( this->ry );
    good = good && ( this->rx.MatchDims( this->rz ) );
    
    good = good && ( this->rx.MatchDims( this->origx ) );
    good = good && ( this->rx.MatchDims( this->origy ) );
    good = good && ( this->rx.MatchDims( this->origz ) );

    good = good && ( this->rx.MatchDims( this->dx ) );
    good = good && ( this->rx.MatchDims( this->dy ) );
    good = good && ( this->rx.MatchDims( this->dz ) );

    good = good && ( this->rx.MatchDims( this->odx ) );
    good = good && ( this->rx.MatchDims( this->ody ) );
    good = good && ( this->rx.MatchDims( this->odz ) );

    good = good && ( this->rx.MatchDims( this->origArea ) );
    good = good && ( this->rx.MatchDims( this->origArea1 ) );
    good = good && ( this->rx.MatchDims( this->origArea2 ) );

    good = good && ( this->rx.MatchDims( this->area ) );
    good = good && ( this->rx.MatchDims( this->area1 ) );
    good = good && ( this->rx.MatchDims( this->area2 ) );

    good = good && ( this->rx.MatchDims( this->invalid ) );
    good = good && ( this->rx.MatchDims( this->label ) );
    good = good && ( this->rx.MatchDims( this->status ) );
    good = good && ( this->rx.MatchDims( this->labelDist ) );

    good = good && ( this->rx.MatchDims( this->mean ) );
    good = good && ( this->rx.MatchDims( this->variance ) );

    if( !good ) {
      std::cerr << __FUNCTION__
		<< ": Dimension mismatch"
		<< std::endl;
      exit( EXIT_FAILURE );
    }
  }

  // -----------------------------

  void GCAmorphCPU::AllocateFromTemplate( const GPU::Classes::GCAmorphGPU& src ) {
    src.CheckIntegrity();

    const dim3 dims = src.d_rx.GetDims();

    this->AllocateAll( dims.x, dims.y, dims.z );
  }



  void GCAmorphCPU::AllocateAll( const unsigned int nx,
				 const unsigned int ny,
				 const unsigned int nz ) {
    this->rx.Allocate( nx, ny, nz );
    this->ry.Allocate( nx, ny, nz );
    this->rz.Allocate( nx, ny, nz );

    this->origx.Allocate( nx, ny, nz );
    this->origy.Allocate( nx, ny, nz );
    this->origz.Allocate( nx, ny, nz );

    this->dx.Allocate( nx, ny, nz );
    this->dy.Allocate( nx, ny, nz );
    this->dz.Allocate( nx, ny, nz );

    this->odx.Allocate( nx, ny, nz );
    this->ody.Allocate( nx, ny, nz );
    this->odz.Allocate( nx, ny, nz );

    this->origArea.Allocate( nx, ny, nz );
    this->origArea1.Allocate( nx, ny, nz );
    this->origArea2.Allocate( nx, ny, nz );

    this->area.Allocate( nx, ny, nz );
    this->area1.Allocate( nx, ny, nz );
    this->area2.Allocate( nx, ny, nz );

    this->invalid.Allocate( nx, ny, nz );
    this->label.Allocate( nx, ny, nz );
    this->status.Allocate( nx, ny, nz );
    this->labelDist.Allocate( nx, ny, nz );

    this->mean.Allocate( nx, ny, nz );
    this->variance.Allocate( nx, ny, nz );
  }



  // --------------------------------

  void GCAmorphCPU::GetFromGPU( const GPU::Classes::GCAmorphGPU& src ) {
    /*!
      Retrieves linearly packed data from the GPU.
      Assumes that the current object has already allocated its memory
    */
    
    GCAmorphCPU::tGetTot.Start();

    src.CheckIntegrity();
    this->CheckIntegrity();

    const dim3 srcDims = src.d_rx.GetDims();
    unsigned int myx, myy, myz;
    this->rx.GetDims( myx, myy, myz );
    if( (srcDims.x != myx) ||
	(srcDims.y != myy) ||
	(srcDims.z != myz) ) {
      std::cerr << __FUNCTION__
		<< ": Dimension mismatch!" << std::endl;
      exit( EXIT_FAILURE );
    }

    // Copy scalars
    this->exp_k = src.exp_k;
    this->neg = src.neg;
    this->gca = src.gca;
    
    // Do the PCIe thing
    this->rx.PullGPU( src.d_rx );
    this->ry.PullGPU( src.d_ry );
    this->rz.PullGPU( src.d_rz );

    this->origx.PullGPU( src.d_origx );
    this->origy.PullGPU( src.d_origy );
    this->origz.PullGPU( src.d_origz );

    this->dx.PullGPU( src.d_dx );
    this->dy.PullGPU( src.d_dy );
    this->dz.PullGPU( src.d_dz );
    
    this->odx.PullGPU( src.d_odx );
    this->ody.PullGPU( src.d_ody );
    this->odz.PullGPU( src.d_odz );
    
    this->origArea.PullGPU( src.d_origArea );
    this->origArea1.PullGPU( src.d_origArea1 );
    this->origArea2.PullGPU( src.d_origArea2 );
    
    this->area.PullGPU( src.d_area );
    this->area1.PullGPU( src.d_area1 );
    this->area2.PullGPU( src.d_area2 );

    this->invalid.PullGPU( src.d_invalid );
    this->status.PullGPU( src.d_status );
    this->label.PullGPU( src.d_label );
    this->labelDist.PullGPU( src.d_labelDist );
    
    this->mean.PullGPU( src.d_mean );
    this->variance.PullGPU( src.d_variance );

    CUDA_SAFE_CALL( cudaThreadSynchronize() );

    GCAmorphCPU::tGetTot.Stop();
  }




  void GCAmorphCPU::PutOnGPU( GPU::Classes::GCAmorphGPU& dst ) const {
    /*!
      Puts the linearly packed data back on the GPU.
      Requires preallocated data
    */
    
    dst.CheckIntegrity();
    this->CheckIntegrity();

    const dim3 dstDims = dst.d_rx.GetDims();
    unsigned int myx, myy, myz;
    this->rx.GetDims( myx, myy, myz );
    if( (dstDims.x != myx) ||
	(dstDims.y != myy) ||
	(dstDims.z != myz) ) {
      std::cerr << __FUNCTION__
		<< ": Dimension mismatch!" << std::endl;
      exit( EXIT_FAILURE );
    }

    // Copy data to page-locked memory
    this->rx.PushGPU( dst.d_rx );
    this->ry.PushGPU( dst.d_ry );
    this->rz.PushGPU( dst.d_rz );

    this->origx.PushGPU( dst.d_origx );
    this->origy.PushGPU( dst.d_origy );
    this->origz.PushGPU( dst.d_origz );

    this->dx.PushGPU( dst.d_dx );
    this->dy.PushGPU( dst.d_dy );
    this->dz.PushGPU( dst.d_dz );
    
    this->odx.PushGPU( dst.d_odx );
    this->ody.PushGPU( dst.d_ody );
    this->odz.PushGPU( dst.d_odz );
    
    this->origArea.PushGPU( dst.d_origArea );
    this->origArea1.PushGPU( dst.d_origArea1 );
    this->origArea2.PushGPU( dst.d_origArea2 );
    
    this->area.PushGPU( dst.d_area );
    this->area1.PushGPU( dst.d_area1 );
    this->area2.PushGPU( dst.d_area2 );

    this->invalid.PushGPU( dst.d_invalid );
    this->status.PushGPU( dst.d_status );
    this->label.PushGPU( dst.d_label );
    this->labelDist.PushGPU( dst.d_labelDist );
    
    this->mean.PushGPU( dst.d_mean );
    this->variance.PushGPU( dst.d_variance );

    // Copy scalars
    dst.exp_k = this->exp_k;
    dst.neg = this->neg;
    std::cerr << __FUNCTION__
	      << ": Did not reset gca in dst"
	      << std::endl;

    CUDA_SAFE_CALL( cudaThreadSynchronize() );
  }


  // ----------------------------------------------

  void GCAmorphCPU::ShowTimings( void ) {
#ifdef CUDA_SHOW_TIMINGS
      std::cout << "==================================" << std::endl;
      std::cout << "GCAmorphCPU timers" << std::endl;
      std::cout << "------------------" << std::endl;
#ifndef CUDA_FORCE_SYNC
      std::cout << "WARNING: CUDA_FORCE_SYNC not #defined" << std::endl;
      std::cout << "Timings may not be accurate" << std::endl;
#endif
      std::cout << std::endl;

      std::cout << "Get from GPU:" << std::endl;
      std::cout << "Total       : " << GCAmorphCPU::tGetTot << std::endl;
      std::cout << std::endl;

      std::cout << "==================================" << std::endl;
#endif
  }

  // Define static members
  SciGPU::Utilities::Chronometer GCAmorphCPU::tGetTot;
}





#endif
