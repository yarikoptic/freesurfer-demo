/**
 * @file  VolumeFilterMedian.cpp
 * @brief Base VolumeFilterMedian class. 
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

#include "VolumeFilterMedian.h"
#include <math.h>
#include "LayerMRI.h"
#include <vtkImageData.h>
#include <vtkImageMedian3D.h>

VolumeFilterMedian::VolumeFilterMedian( LayerMRI* input, LayerMRI* output ) : 
    VolumeFilter( input, output )
{
}

VolumeFilterMedian::~VolumeFilterMedian()
{
}

bool VolumeFilterMedian::Execute()
{
  vtkSmartPointer<vtkImageMedian3D> filter = vtkSmartPointer<vtkImageMedian3D>::New();
  filter->SetKernelSize( m_nKernelSize, m_nKernelSize, m_nKernelSize );
  filter->SetInput( m_volumeInput->GetImageData() );  
  filter->Update();
  m_volumeOutput->GetImageData()->DeepCopy( filter->GetOutput() );
  return true;
}

