/**
 * @file  kvlEMSegmenter.h
 * @brief REPLACE_WITH_ONE_LINE_SHORT_DESCRIPTION
 *
 * REPLACE_WITH_LONG_DESCRIPTION_OR_REFERENCE
 */
/*
 * Original Author: Koen Van Leemput
 * Modified by: Juan Eugenio Iglesias (support for multiple channels)
 * CVS Revision Info:
 *    $Author: zkaufman $
 *    $Date: 2016/12/02 17:16:11 $
 *    $Revision: 1.1 $
 *
 * Copyright © 2011 The General Hospital Corporation (Boston, MA) "MGH"
 *
 * Terms and conditions for use, reproduction, distribution and contribution
 * are found in the 'FreeSurfer Software License Agreement' contained
 * in the file 'LICENSE' found in the FreeSurfer distribution, and here:
 *
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferSoftwareLicense
 *
 * Reporting: freesurfer@nmr.mgh.harvard.edu
 *
 */
#ifndef __kvlEMSegmenter_h
#define __kvlEMSegmenter_h

#include "itkImage.h"
#include <vector>
#include "kvlAtlasMesh.h"
#include "kvlImageSmoother.h"


namespace kvl
{



/**
 *
 */
class EMSegmenter: public itk::Object
{
public :

  /** Standard class typedefs */
  typedef EMSegmenter  Self;
  typedef itk::Object  Superclass;
  typedef itk::SmartPointer< Self >  Pointer;
  typedef itk::SmartPointer< const Self >  ConstPointer;

  /** Method for creation through the object factory. */
  itkNewMacro( Self );

  /** Run-time type information (and related methods). */
  itkTypeMacro( EMSegmenter, itk::Object );

  /** Some typedefs */
  typedef itk::Image< unsigned short, 3 >  ImageType;
  typedef itk::Image< float, 3 >  ClassificationImageType;
  typedef itk::Image< float, 3 >  BiasCorrectedImageType;
  typedef itk::Image< float, 3 >  BiasFieldImageType;


  // Set image
//   void SetImages( std::vector<ImageType*>  images );
  void SetImages( std::vector<ImageType::Pointer>  images );
  void SetImage( ImageType::Pointer  image );
  const std::vector< float > GetVariances();


  // Get image
  ImageType::Pointer  GetImage(int ind) const
  {
    return m_Images[ind];
  }

  // Get image
  BiasCorrectedImageType*  GetBiasCorrectedImage(int ind=0) const
  {
    return m_BiasCorrectedImages[ind];
  }

  // Get estimated bias field image
  BiasFieldImageType*  GetBiasField(int ind) const
  {
    return m_BiasFields[ind];
  }


  // Set atlas mesh
  void SetAtlasMesh( const AtlasMesh* atlasMesh,
                     const std::vector< unsigned int >& lookupTable = std::vector< unsigned int >(),
                     const std::vector< unsigned int >& independentParametersLookupTable = std::vector< unsigned int >() );

  // Get atlas mesh
  const AtlasMesh* GetAtlasMesh() const
  {
    return m_AtlasMesh;
  }
  AtlasMesh* GetAtlasMesh()
  {
    return m_AtlasMesh;
  }

  //
  double  Segment();

  //
  void  SetImageAndSegmentItWithCurrentParametersAndWithoutBiasFieldCorrection( ImageType::Pointer image ); 
  void  SetImagesAndSegmentItWithCurrentParametersAndWithoutBiasFieldCorrection( std::vector<ImageType::Pointer> images );



  //
  BiasCorrectedImageType::Pointer  BiasCorrect( ImageType::Pointer image, int upsamplingFactor, int channelIndex=0) const;

  //
  static ImageType::Pointer  ConvertToNativeImageType(  BiasCorrectedImageType::Pointer image );

  //
  unsigned int GetNumberOfClasses() const
  {
    return m_NumberOfClasses;
  }

  //
  const ClassificationImageType* GetPrior( unsigned int classNumber ) const
  {
    if ( m_Priors.size() == 0 )
    {
      this->Initialize();
    }
    return m_Priors[ classNumber ];
  }

  //
  const ClassificationImageType* GetPosterior( unsigned int classNumber ) const
  {
    if ( m_Priors.size() == 0 )
    {
      this->Initialize();
    }
    return m_Posteriors[ classNumber ];
  }

  //
  const std::vector< vnl_vector<float> >&  GetMeans() const
  {
    return m_Means;
  }

  //
  const std::vector< vnl_matrix<float> >&  GetPrecisions() const
  {
    return m_Precisions;
  }

  //
  const std::vector< float >&  GetPriorWeights() const
  {
    return m_PriorWeights;
  }

  //
  int GetIterationNumber() const
  {
    return m_IterationNumber;
  }

  //
  void SetMaximumNumberOfIterations( int maximumNumberOfIterations )
  {
    m_MaximumNumberOfIterations = maximumNumberOfIterations;
  }

  //
  int GetMaximumNumberOfIterations() const
  {
    return m_MaximumNumberOfIterations;
  }

  //
  void  SetStopCriterion( float stopCriterion )
  {
    m_StopCriterion = stopCriterion;
  }

  //
  float  GetStopCriterion() const
  {
    return m_StopCriterion;
  }

  //
  const std::vector< unsigned int >&  GetLookupTable() const
  {
    return m_LookupTable;
  }

  //
  const std::vector< unsigned int >&  GetIndependentParametersLookupTable() const
  {
    return m_IndependentParametersLookupTable;
  }

  //
  void  SetPartialVolumeUpsamplingFactors( const int* partialVolumeUpsamplingFactors )
  {  
    m_NumberOfSubvoxels = 1;
    for ( int i = 0; i < 3; i++ )
    {
      m_PartialVolumeUpsamplingFactors[ i ] = partialVolumeUpsamplingFactors[ i ];
      m_NumberOfSubvoxels *= m_PartialVolumeUpsamplingFactors[ i ];

      if ( m_PartialVolumeUpsamplingFactors[ i ] != 1 )
      {
        this->SetMaximumNumberOfIterations( 100 );
      }
    }
  }

  //
  const int*  GetPartialVolumeUpsamplingFactors() const
  {
    return m_PartialVolumeUpsamplingFactors;
  }

  //
  void  SetBiasFieldOrder( int biasFieldOrder )
  {
    m_BiasFieldOrder = biasFieldOrder;
  //    m_BiasFieldResidueSmoothers = 0;
  }

  //
  int  GetBiasFieldOrder() const
  {
    return m_BiasFieldOrder;
  }


  //
  void  SetReestimatedRelativeWeightOfSharedClasses( bool reestimatedRelativeWeightOfSharedClasses )
  {
    m_ReestimatedRelativeWeightOfSharedClasses = reestimatedRelativeWeightOfSharedClasses;
  }

  bool  GetReestimatedRelativeWeightOfSharedClasses() const
  {
    return m_ReestimatedRelativeWeightOfSharedClasses;
  }

  //
  typedef itk::Image< AtlasAlphasType, 3 >  SuperResolutionProbabilityImageType;
  const SuperResolutionProbabilityImageType*  GetSuperResolutionPriors() const
  {
    return m_SuperResolutionPriors;
  }
  SuperResolutionProbabilityImageType::Pointer  GetSuperResolutionLikelihoods() const;
  SuperResolutionProbabilityImageType::Pointer  GetSuperResolutionPosteriors() const;

  //
  ImageType::Pointer  GetSuperResolutionImage(int ind) const;

  //
  AtlasMesh::Pointer  GetSuperResolutionMesh( const AtlasMesh* mesh ) const;
  AtlasMesh::Pointer  GetNormalResolutionMesh( const AtlasMesh* superResolutionMesh ) const;


protected:
  EMSegmenter();
  virtual ~EMSegmenter();

  //
  void Initialize() const;

  //
  double UpdatePosteriors();

  //
  void UpdateModelParameters( int biasFieldOrderUsed );

  //
  void UpdateMixtureModelParameters();

  //
  void UpdateBiasFieldModelParameters( int biasFieldOrderUsed );

  //
  static itk::Array< float >  GetMixingProbabilities( const itk::Array< float >& pureProbabilitiesOfClass1,
      const itk::Array< float >& pureProbabilitiesOfClass2 );

  //
  float  DoOneEMIterationWithPartialVoluming();

  //
  static AtlasMesh::Pointer  GetTransformedMesh( const AtlasMesh* mesh, const float* translation, const float* scaling );


private:
  EMSegmenter(const Self&); //purposely not implemented
  void operator=(const Self&); //purposely not implemented

  //
  AtlasMesh::Pointer  GetDistributedMesh( const AtlasMesh* atlasMesh, const std::vector< unsigned int >& lookupTable ) const;

  //
  void  SplitSharedClasses();

  // Data members
  std::vector<ImageType::Pointer>  m_Images;
  int m_n_channels;
  std::vector<BiasCorrectedImageType::Pointer>  m_BiasCorrectedImages;
  std::vector<BiasFieldImageType::Pointer>  m_BiasFields;
  AtlasMesh::Pointer  m_AtlasMesh;

  std::vector< unsigned int >  m_LookupTable;
  std::vector< unsigned int >  m_IndependentParametersLookupTable;

  unsigned int  m_NumberOfClasses;
  int  m_BiasFieldOrder;
  bool  m_ReestimatedRelativeWeightOfSharedClasses;

  mutable std::vector< ClassificationImageType::Pointer >  m_Priors;
  mutable std::vector< ClassificationImageType::Pointer >  m_Posteriors;
  std::vector< vnl_vector<float> >  m_Means;
  std::vector< vnl_matrix<float> >  m_Precisions;
  std::vector< float >  m_PriorWeights;

  int m_IterationNumber;
  int m_MaximumNumberOfIterations;
  float  m_StopCriterion;

  // Stuff related to partial volume modeling
  int  m_NumberOfSubvoxels;
  int  m_PartialVolumeUpsamplingFactors[ 3 ];

  mutable SuperResolutionProbabilityImageType::Pointer  m_SuperResolutionPriors;

  // For bias field correction, we have a smoother that fits a polynomial
  // model to a noisy residue image. Initializing the smoother (in particular
  // setting up orthonormal basis functions) takes some time, so we keep it
  // here as a remember that needs to be initialized once
  std::vector<ImageSmoother::Pointer>  m_BiasFieldResidueSmoothers;


  typedef itk::Image< std::vector< AtlasAlphasType* >, 3 >  SubvoxelPriorsImageType;
  mutable SubvoxelPriorsImageType::Pointer  m_SubvoxelPriorsImage;  // Low-res image

};


} // end namespace kvl

#endif

