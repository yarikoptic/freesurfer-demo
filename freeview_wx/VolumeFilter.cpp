/**
 * @file  VolumeFilter.cpp
 * @brief Base VolumeFilter class. 
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2011/03/19 18:44:26 $
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

#include "VolumeFilter.h"
#include <math.h>
#include "LayerMRI.h"
#include <vtkImageData.h>

VolumeFilter::VolumeFilter( LayerMRI* input, LayerMRI* output ) : 
    Listener( "VolumeFilter" ), 
    Broadcaster( "VolumeFilter" ),
    m_nKernelSize( 3 )
{
  SetInputOutputVolumes( input, output );
}

VolumeFilter::~VolumeFilter()
{
}

void VolumeFilter::SetInputOutputVolumes( LayerMRI* input, LayerMRI* output )
{
  m_volumeInput = input;
  m_volumeOutput = output;
  if ( m_volumeInput )
    m_volumeInput->AddListener( this );
  if ( m_volumeOutput )
    m_volumeOutput->AddListener( this );
}

void VolumeFilter::DoListenToMessage ( std::string const iMessage, void* iData, void* sender )
{
  if ( iMessage == "LayerObjectDeleted" )
  {
    if ( sender == m_volumeInput || iData == m_volumeInput )
      m_volumeInput = NULL;
    else if ( sender == m_volumeOutput || iData == m_volumeOutput )
      m_volumeOutput = NULL;
  }
}

bool VolumeFilter::ReadyToUpdate()
{
  return ( m_volumeInput && m_volumeOutput );
}

bool VolumeFilter::Update()
{
  if ( !ReadyToUpdate() )
  {
    cerr << "Volume has been removed. Please start over." << endl;
    return false;
  }
  
  if ( Execute() )
  {
    m_volumeOutput->SetModified();
    m_volumeOutput->SendBroadcast( "LayerActorUpdated", m_volumeOutput );
    return true;
  }
  else
    return false;
}

MRI* VolumeFilter::CreateMRIFromVolume( LayerMRI* layer )
{
  vtkImageData* image = layer->GetImageData();
  int mri_type = MRI_FLOAT;
  switch ( image->GetScalarType() )
  {
    case VTK_CHAR:
    case VTK_SIGNED_CHAR:
    case VTK_UNSIGNED_CHAR:
      mri_type = MRI_UCHAR;
      break;
    case VTK_INT:
    case VTK_UNSIGNED_INT:
      mri_type = MRI_INT;
      break;
    case VTK_LONG:
    case VTK_UNSIGNED_LONG:
      mri_type = MRI_LONG;
      break;
    case VTK_SHORT:
    case VTK_UNSIGNED_SHORT:
      mri_type = MRI_SHORT;
      break;
    default:
      break ;
  }
  
  int* dim = image->GetDimensions();
  int zFrames = image->GetNumberOfScalarComponents();
  MRI* mri = MRIallocSequence( dim[0], dim[1], dim[2], mri_type, zFrames );
  if ( !mri )
  {
    cerr << "Can not allocate memory for MRI" << endl;
    return NULL;
  }
  
  int zX = mri->width;
  int zY = mri->height;
  int zZ = mri->depth;
  int nScalarSize = image->GetScalarSize();
  for ( int nZ = 0; nZ < zZ; nZ++ )
  {
    for ( int nY = 0; nY < zY; nY++ )
    {
      for ( int nX = 0; nX < zX; nX++ )
      {
        for ( int nFrame = 0; nFrame < zFrames; nFrame++ )
        {
          void* buf = (char*)&MRIseq_vox( mri, nX, nY, nZ, nFrame);
          void* ptr = (char*)image->GetScalarPointer( nX, nY, nZ )
              + nFrame * nScalarSize;
          memcpy( buf, ptr, nScalarSize );
        }
      }
    }
  }
  
  return mri;
}
  
// map mri data to image data, assuming same data type
void VolumeFilter::MapMRIToVolume( MRI* mri, LayerMRI* layer )
{
  vtkImageData* image = layer->GetImageData();
  int zX = mri->width;
  int zY = mri->height;
  int zZ = mri->depth;
  int zFrames = mri->nframes;
  int nScalarSize = image->GetScalarSize();
  for ( int nZ = 0; nZ < zZ; nZ++ )
  {
    for ( int nY = 0; nY < zY; nY++ )
    {
      for ( int nX = 0; nX < zX; nX++ )
      {
        for ( int nFrame = 0; nFrame < zFrames; nFrame++ )
        {
          void* buf = (char*)&MRIseq_vox( mri, nX, nY, nZ, nFrame);
          void* ptr = (char*)image->GetScalarPointer( nX, nY, nZ )
              + nFrame * nScalarSize;
          memcpy( ptr, buf, nScalarSize );
        }
      }
    }
  }
}


