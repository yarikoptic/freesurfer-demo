/**
 * @file  VolumeFilter.cpp
 * @brief Base VolumeFilter class.
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: rpwang $
 *    $Date: 2014/01/30 22:08:25 $
 *    $Revision: 1.11 $
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
 *
 */

#include "VolumeFilter.h"
#include <math.h>
#include "LayerMRI.h"
#include <vtkImageData.h>
#include "ProgressCallback.h"
#include <QTimer>

extern "C"
{
#include "utils.h"
}


VolumeFilter::VolumeFilter( LayerMRI* input, LayerMRI* output, QObject* parent ) :
  QObject( parent ),
  m_nKernelSize( 3 )
{
  SetInputOutputVolumes( input, output );
  m_timerProgress = new QTimer(this);
  connect(m_timerProgress, SIGNAL(timeout()), this, SLOT(OnTimeout()));
}

VolumeFilter::~VolumeFilter()
{
}

void VolumeFilter::SetInputOutputVolumes( LayerMRI* input, LayerMRI* output )
{
  m_volumeInput = input;
  m_volumeOutput = output;
  if ( m_volumeInput )
  {
    connect(m_volumeInput, SIGNAL(destroyed()),
            this, SLOT(OnLayerObjectDeleted()), Qt::UniqueConnection);
  }
  if ( m_volumeOutput )
  {
    connect(m_volumeOutput, SIGNAL(destroyed()),
            this, SLOT(OnLayerObjectDeleted()), Qt::UniqueConnection);
  }
}

void VolumeFilter::OnLayerObjectDeleted()
{
  if ( sender() == m_volumeInput )
  {
    m_volumeInput = NULL;
  }
  else if ( sender() == m_volumeOutput)
  {
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
    cerr << "Volume has been removed. Please start over.\n";
    return false;
  }

  if (m_volumeInput == m_volumeOutput)
  {
    m_volumeInput->SaveForUndo(-1);
  }

  if ( Execute() )
  {
    m_volumeOutput->SetModified();
    m_volumeOutput->SendActorUpdated();
    return true;
  }
  else
  {
    return false;
  }
}

#include <QDebug>

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
    break;
  }

  int* dim = image->GetDimensions();
  int zFrames = image->GetNumberOfScalarComponents();
  MRI* mri = NULL;
  try {
    mri = MRIallocSequence( dim[0], dim[1], dim[2], mri_type, zFrames );
  } catch (int ret) {
    return NULL;
  }

  if ( !mri )
  {
    cerr << "Can not allocate memory for MRI.\n";
    return NULL;
  }

  for ( int j = 0; j < mri->height; j++ )
  {
      for ( int k = 0; k < mri->depth; k++ )
      {
        for ( int i = 0; i < mri->width; i++ )
        {
          for ( int nFrame = 0; nFrame < mri->nframes; nFrame++ )
          {
            switch ( mri->type )
            {
            case MRI_UCHAR:
              MRIseq_vox( mri, i, j, k, nFrame ) =
                (unsigned char)image->GetScalarComponentAsDouble(i, j, k, nFrame);
              break;
            case MRI_INT:
              MRIIseq_vox( mri, i, j, k, nFrame ) =
                (int)image->GetScalarComponentAsDouble(i, j, k, nFrame);
              break;
            case MRI_LONG:
              MRILseq_vox( mri, i, j, k, nFrame ) =
                (long)image->GetScalarComponentAsDouble(i, j, k, nFrame);
              break;
            case MRI_FLOAT:
              MRIFseq_vox( mri, i, j, k, nFrame ) =
                (float)image->GetScalarComponentAsDouble(i, j, k, nFrame);
              break;
            case MRI_SHORT:
              MRISseq_vox( mri, i, j, k, nFrame ) =
                (int)image->GetScalarComponentAsDouble(i, j, k, nFrame);
              break;
            default:
              break;
            }
          }
        }
      }
      exec_progress_callback(j, mri->height, 0, 1);
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
  for ( int nZ = 0; nZ < zZ; nZ++ )
  {
    for ( int nY = 0; nY < zY; nY++ )
    {
      for ( int nX = 0; nX < zX; nX++ )
      {
        for ( int nFrame = 0; nFrame < zFrames; nFrame++ )
        {
          switch ( mri->type )
          {
          case MRI_UCHAR:
            image->SetScalarComponentFromDouble(nX, nY, nZ, nFrame,
                                   MRIseq_vox( mri, nX, nY, nZ, nFrame ) );
            break;
          case MRI_INT:
            image->SetScalarComponentFromDouble(nX, nY, nZ, nFrame,
                                   MRIIseq_vox( mri, nX, nY, nZ, nFrame ) );
            break;
          case MRI_LONG:
            image->SetScalarComponentFromDouble(nX, nY, nZ, nFrame,
                                   MRILseq_vox( mri, nX, nY, nZ, nFrame ) );
            break;
          case MRI_FLOAT:
            image->SetScalarComponentFromDouble(nX, nY, nZ, nFrame,
                                   MRIFseq_vox( mri, nX, nY, nZ, nFrame ) );
            break;
          case MRI_SHORT:
            image->SetScalarComponentFromDouble(nX, nY, nZ, nFrame,
                                   MRISseq_vox( mri, nX, nY, nZ, nFrame ) );
            break;
          default:
            break;
          }
        }
      }
    }
    exec_progress_callback(nZ, zZ, 0, 1);
  }
}

void VolumeFilter::TriggerFakeProgress(int interval)
{
  m_nTimerCount = 0;
  m_timerProgress->start(interval);
}

void VolumeFilter::OnTimeout()
{
  m_nTimerCount++;
  if (m_nTimerCount == 100)
    m_nTimerCount = 50;
  emit Progress(m_nTimerCount);
}
