/**
 * @file  gcalinearprior.cpp
 * @brief Class to hold a volume of GCA priors in linear memory
 *
 */
/*
 * Original Authors: Richard Edgar
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2010/10/22 23:14:50 $
 *    $Revision: 1.1.2.1 $
 *
 * Copyright (C) 2002-2010,
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
#include <cstdlib>
#include <limits>
using namespace std;


#include "minmax.hpp"

#include "gcalinearprior.hpp"

namespace Freesurfer {
  // ==========================================
  void GCAlinearPrior::PrintStats( ostream& os ) const {
    os << "Stats for GCApriorNode" << endl;
    os << "  Bytes allocated = " << this->bytes << endl;
    os << "  Exhumation time = " << this->tExhume << endl;
    os << "  Inhumation time = " << this->tInhume << endl;
  }

  // ==========================================
  void GCAlinearPrior::ExtractDims( const GCA* const src ) {
    /*!
      Fills in the dimensions required from the given GCA.
      Does this by looping over all voxels, and finding the
      maximum number of elements allocated for each.
    */

    this->xDim = src->prior_width;
    this->yDim = src->prior_height;
    this->zDim = src->prior_depth;

    MinMax<short> nLabelsVoxel;

    for( int ix=0; ix<this->xDim; ix++ ) {
      for( int iy=0; iy<this->yDim; iy++ ) {
	for( int iz=0; iz<this->zDim; iz++ ) {
	  nLabelsVoxel.Accumulate( src->priors[ix][iy][iz].nlabels );
	}
      }
    }

    this->labelDim = nLabelsVoxel.GetMax();
  }

  // ==========================================
  void GCAlinearPrior::Allocate( void ) {
    /*!
      Allocates the main arrays according to the 'dimension'
      members.
      All vectors are cleared first, and filled with defaults
    */

    this->bytes = 0;

    const size_t nVoxels = this->xDim * this->yDim * this->zDim;

    //! Indicate the number of labels for each voxel
    this->nLabels.clear();
    this->nLabels.resize( nVoxels,
			  numeric_limits<short>::max() );
    this->bytes += this->nLabels.size() * sizeof(short);

    //! Space for maxLabels
    this->maxLabels.clear();
    this->maxLabels.resize( nVoxels, 0 );
    this->bytes += this->maxLabels.size() * sizeof(short);
    
    //! Space for the labels
    this->labels.clear();
    this->labels.resize( nVoxels * this->labelDim,
			 numeric_limits<unsigned short>::max() );
    this->bytes += this->labels.size() * sizeof(unsigned short);

    //! Space for the priors
    this->priors.clear();
    this->priors.resize( nVoxels * this->labelDim,
			 numeric_limits<float>::quiet_NaN() );
    this->bytes += this->priors.size() * sizeof(float);

    //! Space for totTraining
    this->totTraining.clear();
    this->totTraining.resize( nVoxels,
			      numeric_limits<int>::max() );
    this->bytes += this->totTraining.size() * sizeof(int);
  }


  // ==========================================

  void GCAlinearPrior::Exhume( const GCA* const src ) {
    /*!
      This method is responsible for extracting GCA_PRIOR data
      from the source GCA, and packing into the linear arrays
    */

    this->tExhume.Start();

    this->ExtractDims( src );
    this->Allocate();

    for( int ix=0; ix<this->xDim; ix++ ) {
      for( int iy=0; iy<this->yDim; iy++ ) {
	for( int iz=0; iz<this->zDim; iz++ ) {
	  const GCA_PRIOR* const gcap = &(src->priors[ix][iy][iz]);
	  
	  this->voxelLabelCount(ix,iy,iz) = gcap->nlabels;
	  this->maxVoxelLabel(ix,iy,iz) = gcap->max_labels;
	  
	  this->totalTraining(ix,iy,iz) = gcap->total_training;

	  for( int iLabel=0;
	       iLabel<this->voxelLabelCount(ix,iy,iz);
	       iLabel++ ) {
	    this->voxelLabel(ix,iy,iz,iLabel) = gcap->labels[iLabel];
	    this->voxelPrior(ix,iy,iz,iLabel) = gcap->priors[iLabel];
	  }
	  
	}
      }
    }
    
    this->tExhume.Stop();
  }

  // ==========================================
  void GCAlinearPrior::Inhume( GCA* dst ) const {
    /*!
      Stores data about the priors back into the target
      GCA.
      Starts by destroying all of the GCA_PRIOR data
      which is there.
      One might prefer the subsequent reallocation
      to use the routines in gca.c, but that would
      probably involve writing them first.
    */

    this->tInhume.Start();
    
    // Dump the old data
    this->ScorchPriors( dst );

    // Set dimensions
    dst->prior_width = this->xDim;
    dst->prior_height = this->yDim;
    dst->prior_depth = this->zDim;

    // Start allocating
    dst->priors = (GCA_PRIOR***)calloc( this->xDim, sizeof(GCA_PRIOR**) );
    if( !(dst->priors) ) {
      cerr << __FUNCTION__
	   << ": dst->priors allocation failed" << endl;
      exit( EXIT_FAILURE );
    }
    
    // Loop
    for( int ix=0; ix<this->xDim; ix++ ) {
      // Allocate pointer block
      dst->priors[ix] = (GCA_PRIOR**)calloc( this->yDim, sizeof(GCA_PRIOR*) );
      if( !(dst->priors[ix]) ) {
	cerr << __FUNCTION__
	     << ": dst->priors[ix] allocation failed" << endl;
	exit( EXIT_FAILURE );
      }
      
      for( int iy=0; iy<this->yDim; iy++ ) {
	// Allocate pointer block
	dst->priors[ix][iy] = (GCA_PRIOR*)calloc( this->zDim, sizeof(GCA_PRIOR) );
	if( !(dst->priors[ix][iy]) ) {
	  cerr << __FUNCTION__
	       << ": dst->priors[ix][iy] allocation failed" << endl;
	  exit( EXIT_FAILURE );
	}

	for( int iz=0; iz<this->zDim; iz++ ) {

	  GCA_PRIOR* gcap = &(dst->priors[ix][iy][iz]);

	  gcap->nlabels = this->voxelLabelCount(ix,iy,iz);
	  gcap->max_labels = this->maxVoxelLabel(ix,iy,iz);

	  gcap->total_training = this->totalTraining(ix,iy,iz);

	  gcap->labels = (unsigned short*)calloc( this->voxelLabelCount(ix,iy,iz),
						  sizeof(unsigned short) );
	  gcap->priors = (float*)calloc( this->voxelLabelCount(ix,iy,iz),
					 sizeof(float) );
	  for( int iLabel=0;
	       iLabel < this->voxelLabelCount(ix,iy,iz);
	       iLabel++ ) {
	    gcap->labels[iLabel] = this->voxelLabel(ix,iy,iz,iLabel);
	    gcap->priors[iLabel] = this->voxelPrior(ix,iy,iz,iLabel);
	  }

	}
      }
    }

    this->tInhume.Stop();
  }

  // ==========================================

  void GCAlinearPrior::ScorchPriors( GCA* targ ) const {
    /*!
      This method destroys the priors structure of a GCA,
      prior to inhumation of new data
    */
    for( int ix=0; ix<targ->prior_width; ix++ ) {
      for( int iy=0; iy<targ->prior_height; iy++ ) {
	for( int iz=0; iz<targ->prior_depth; iz++ ) {
	  free( targ->priors[ix][iy][iz].labels );
	  free( targ->priors[ix][iy][iz].priors );
	}
	free( targ->priors[ix][iy] );
      }
      free( targ->priors[ix] );
    }
    free( targ->priors );
  }


}
