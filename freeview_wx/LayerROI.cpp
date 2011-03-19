/**
 * @file  LayerROI.cpp
 * @brief Layer data object for MRI volume.
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2011/03/19 18:44:25 $
 *    $Revision: 1.1.4.1 $
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

#include <wx/wx.h>
#include "LayerROI.h"
#include "vtkFSVolumeSource.h"
#include "vtkRenderer.h"
#include "vtkImageReslice.h"
#include "vtkImageActor.h"
#include "vtkPolyDataMapper.h"
#include "vtkSmartPointer.h"
#include "vtkMatrix4x4.h"
#include "vtkImageMapToColors.h"
#include "vtkImageFlip.h"
#include "vtkTransform.h"
#include "vtkPlaneSource.h"
#include "vtkTransformPolyDataFilter.h"
#include "vtkTexture.h"
#include "vtkImageActor.h"
#include "vtkActor.h"
#include "vtkRGBAColorTransferFunction.h"
#include "vtkLookupTable.h"
#include "vtkProperty.h"
#include "vtkFreesurferLookupTable.h"
#include "vtkImageChangeInformation.h"
#include "LayerPropertiesROI.h"
#include "MyUtils.h"
#include "LayerMRI.h"
#include "FSLabel.h"
#include <stdlib.h>

LayerROI::LayerROI( LayerMRI* layerMRI ) : LayerVolumeBase()
{
  m_strTypeNames.push_back( "ROI" );

  m_label = new FSLabel();
  for ( int i = 0; i < 3; i++ )
  {
    m_dSlicePosition[i] = 0;
    m_sliceActor2D[i] = vtkImageActor::New();
    m_sliceActor3D[i] = vtkImageActor::New();
    /* m_sliceActor2D[i]->GetProperty()->SetAmbient( 1 );
     m_sliceActor2D[i]->GetProperty()->SetDiffuse( 0 );
     m_sliceActor3D[i]->GetProperty()->SetAmbient( 1 );
     m_sliceActor3D[i]->GetProperty()->SetDiffuse( 0 );*/
    m_sliceActor2D[i]->InterpolateOff();
    m_sliceActor3D[i]->InterpolateOff();
  }
  mProperties = new LayerPropertiesROI();
  mProperties->AddListener( this );

  m_layerSource = layerMRI;
  m_imageDataRef = layerMRI->GetImageData();
  if ( m_layerSource )
  {
    SetWorldOrigin( m_layerSource->GetWorldOrigin() );
    SetWorldVoxelSize( m_layerSource->GetWorldVoxelSize() );
    SetWorldSize( m_layerSource->GetWorldSize() );

    m_imageData = vtkSmartPointer<vtkImageData>::New();
    // m_imageData->DeepCopy( m_layerSource->GetRASVolume() );

    m_imageData->SetNumberOfScalarComponents( 1 );
    m_imageData->SetScalarTypeToUnsignedChar();
    m_imageData->SetOrigin( GetWorldOrigin() );
    m_imageData->SetSpacing( GetWorldVoxelSize() );
    m_imageData->SetDimensions( ( int )( m_dWorldSize[0] / m_dWorldVoxelSize[0] + 0.5 ),
                                ( int )( m_dWorldSize[1] / m_dWorldVoxelSize[1] + 0.5 ),
                                ( int )( m_dWorldSize[2] / m_dWorldVoxelSize[2] + 0.5 ) );
    m_imageData->AllocateScalars();
    void* ptr = m_imageData->GetScalarPointer();
    int* nDim = m_imageData->GetDimensions();
    // cout << nDim[0] << ", " << nDim[1] << ", " << nDim[2] << endl;
    memset( ptr, 0, m_imageData->GetScalarSize() * nDim[0] * nDim[1] * nDim[2] );
    InitializeActors();
  }
}

LayerROI::~LayerROI()
{
  for ( int i = 0; i < 3; i++ )
  {
    m_sliceActor2D[i]->Delete();
    m_sliceActor3D[i]->Delete();
  }

  if ( m_label )
    delete m_label;
}

bool LayerROI::LoadROIFromFile( std::string filename )
{
  if ( !m_label->LabelRead( filename.c_str() ) )
    return false;

  m_label->UpdateRASImage( m_imageData, m_layerSource->GetSourceVolume() );

  m_sFilename = filename;

  return true;
}

void LayerROI::InitializeActors()
{
  if ( m_layerSource == NULL )
    return;

  for ( int i = 0; i < 3; i++ )
  {
    // The reslice object just takes a slice out of the volume.
    //
    mReslice[i] = vtkSmartPointer<vtkImageReslice>::New();
    mReslice[i]->SetInput( m_imageData );
//  mReslice[i]->SetOutputSpacing( sizeX, sizeY, sizeZ );
    mReslice[i]->BorderOff();

    // This sets us to extract slices.
    mReslice[i]->SetOutputDimensionality( 2 );

    // This will change depending what orienation we're in.
    mReslice[i]->SetResliceAxesDirectionCosines( 1, 0, 0,
        0, 1, 0,
        0, 0, 1 );

    // This will change to select a different slice.
    mReslice[i]->SetResliceAxesOrigin( 0, 0, 0 );
    //
    // Image to colors using color table.
    //
    mColorMap[i] = vtkSmartPointer<vtkImageMapToColors>::New();
    mColorMap[i]->SetLookupTable( GetProperties()->GetLookupTable() );
    mColorMap[i]->SetInputConnection( mReslice[i]->GetOutputPort() );
    mColorMap[i]->SetOutputFormatToRGBA();
    mColorMap[i]->PassAlphaToOutputOn();

    //
    // Prop in scene with plane mesh and texture.
    //
    m_sliceActor2D[i]->SetInput( mColorMap[i]->GetOutput() );
    m_sliceActor3D[i]->SetInput( mColorMap[i]->GetOutput() );

    // Set ourselves up.
    this->OnSlicePositionChanged( i );
  }

  this->UpdateOpacity();
  this->UpdateColorMap();
}


void LayerROI::UpdateOpacity()
{
  for ( int i = 0; i < 3; i++ )
  {
    m_sliceActor2D[i]->SetOpacity( GetProperties()->GetOpacity() );
    m_sliceActor3D[i]->SetOpacity( GetProperties()->GetOpacity() );
  }
}

void LayerROI::UpdateColorMap ()
{
  assert( GetProperties() );

  for ( int i = 0; i < 3; i++ )
    mColorMap[i]->SetLookupTable( GetProperties()->GetLookupTable() );
  // m_sliceActor2D[i]->GetProperty()->SetColor(1, 0, 0);
}

void LayerROI::Append2DProps( vtkRenderer* renderer, int nPlane )
{
  wxASSERT ( nPlane >= 0 && nPlane <= 2 );

  renderer->AddViewProp( m_sliceActor2D[nPlane] );
}

void LayerROI::Append3DProps( vtkRenderer* renderer, bool* bSliceVisibility )
{
  for ( int i = 0; i < 3; i++ )
  {
    if ( bSliceVisibility == NULL || bSliceVisibility[i] )
      renderer->AddViewProp( m_sliceActor3D[i] );
  }
}

void LayerROI::OnSlicePositionChanged( int nPlane )
{
  if ( !m_layerSource )
    return;

  assert( GetProperties() );

  vtkSmartPointer<vtkMatrix4x4> matrix =
    vtkSmartPointer<vtkMatrix4x4>::New();
  matrix->Identity();
  switch ( nPlane )
  {
  case 0:
    m_sliceActor2D[0]->PokeMatrix( matrix );
    m_sliceActor2D[0]->SetPosition( m_dSlicePosition[0], 0, 0 );
    m_sliceActor2D[0]->RotateX( 90 );
    m_sliceActor2D[0]->RotateY( -90 );
    m_sliceActor3D[0]->PokeMatrix( matrix );
    m_sliceActor3D[0]->SetPosition( m_dSlicePosition[0], 0, 0 );
    m_sliceActor3D[0]->RotateX( 90 );
    m_sliceActor3D[0]->RotateY( -90 );

    // Putting negatives in the reslice axes cosines will flip the
    // image on that axis.
    mReslice[0]->SetResliceAxesDirectionCosines( 0, -1, 0,
        0, 0, 1,
        1, 0, 0 );
    mReslice[0]->SetResliceAxesOrigin( m_dSlicePosition[0], 0, 0  );
    mReslice[0]->Modified();
    break;
  case 1:
    m_sliceActor2D[1]->PokeMatrix( matrix );
    m_sliceActor2D[1]->SetPosition( 0, m_dSlicePosition[1], 0 );
    m_sliceActor2D[1]->RotateX( 90 );
    // m_sliceActor2D[1]->RotateY( 180 );
    m_sliceActor3D[1]->PokeMatrix( matrix );
    m_sliceActor3D[1]->SetPosition( 0, m_dSlicePosition[1], 0 );
    m_sliceActor3D[1]->RotateX( 90 );
    // m_sliceActor3D[1]->RotateY( 180 );

    // Putting negatives in the reslice axes cosines will flip the
    // image on that axis.
    mReslice[1]->SetResliceAxesDirectionCosines( 1, 0, 0,
        0, 0, 1,
        0, 1, 0 );
    mReslice[1]->SetResliceAxesOrigin( 0, m_dSlicePosition[1], 0 );
    mReslice[1]->Modified();
    break;
  case 2:
    m_sliceActor2D[2]->SetPosition( 0, 0, m_dSlicePosition[2] );
    // m_sliceActor2D[2]->RotateY( 180 );
    m_sliceActor3D[2]->SetPosition( 0, 0, m_dSlicePosition[2] );
    // m_sliceActor3D[2]->RotateY( 180 );

    mReslice[2]->SetResliceAxesDirectionCosines( 1, 0, 0,
        0, 1, 0,
        0, 0, 1 );
    mReslice[2]->SetResliceAxesOrigin( 0, 0, m_dSlicePosition[2]  );
    mReslice[2]->Modified();
    break;
  }
}

void LayerROI::DoListenToMessage( std::string const iMessage, void* iData, void* sender )
{
  if ( iMessage == "ColorMapChanged" )
  {
    this->UpdateColorMap();
    this->SendBroadcast( "LayerActorUpdated", this, this );
  }
  else if ( iMessage == "OpacityChanged" )
  {
    this->UpdateOpacity();
    this->SendBroadcast( "LayerActorUpdated", this, this );
  }
  LayerVolumeBase::DoListenToMessage( iMessage, iData, sender );
}

void LayerROI::SetVisible( bool bVisible )
{
  for ( int i = 0; i < 3; i++ )
  {
    m_sliceActor2D[i]->SetVisibility( bVisible ? 1 : 0 );
    m_sliceActor3D[i]->SetVisibility( bVisible ? 1 : 0 );
  }
  this->SendBroadcast( "LayerActorUpdated", this, this );
}

bool LayerROI::IsVisible()
{
  return m_sliceActor2D[0]->GetVisibility() > 0;
}

void LayerROI::SetModified()
{
  mReslice[0]->Modified();
  mReslice[1]->Modified();
  mReslice[2]->Modified();

  LayerVolumeBase::SetModified();
}

bool LayerROI::SaveROI( wxWindow* wnd, wxCommandEvent& event )
{
  if ( m_sFilename.size() == 0 || m_imageData.GetPointer() == NULL )
    return false;

  m_label->UpdateLabelFromImage( m_imageData, m_layerSource->GetSourceVolume(), wnd, event );

  bool bSaved = m_label->LabelWrite( m_sFilename.c_str() );
  if ( !bSaved )
    m_bModified = true;

  return bSaved;
}

bool LayerROI::HasProp( vtkProp* prop )
{
  for ( int i = 0; i < 3; i++ )
  {
    if ( m_sliceActor2D[i] == prop || m_sliceActor3D[i] == prop )
      return true;
  }
  return false;
}

bool LayerROI::DoRotate( std::vector<RotationElement>& rotations, wxWindow* wnd, wxCommandEvent& event )
{
  m_label->UpdateRASImage( m_imageData, m_layerSource->GetSourceVolume() );

  return true;
}

void LayerROI::DoRestore()
{
  m_label->UpdateRASImage( m_imageData, m_layerSource->GetSourceVolume() );
}

void LayerROI::UpdateLabelData( wxWindow* wnd, wxCommandEvent& event )
{
  if ( IsModified() )
    m_label->UpdateLabelFromImage( m_imageData, m_layerSource->GetSourceVolume(), wnd, event );
}
