/**
 * @file  LayerPLabel.cpp
 * @brief Layer class for p-label volumes.
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
#include "LayerPLabel.h"
#include "LayerPropertiesDTI.h"
#include "MyUtils.h"
#include "FSVolume.h"
#include "vtkFloatArray.h"
#include "vtkPointData.h"
#include "vtkImageMapToColors.h"
#include "vtkLookupTable.h"
#include "vtkMath.h"
#include "vtkAbstractTransform.h"
#include "MainWindow.h"
#include "LUTDataHolder.h"
#include <vtkImageActor.h>
#include <vtkImageReslice.h>
#include <wx/filename.h>

LayerPLabel::LayerPLabel( LayerMRI* ref ) : LayerMRI( ref ),
    m_volumeTemp( NULL)
{
  m_strTypeNames.push_back( "PLabel" );

  SetEditable( false );
}

LayerPLabel::~LayerPLabel()
{
  if ( m_volumeTemp )
    delete m_volumeTemp;
}

bool LayerPLabel::LoadVolumeFiles( wxWindow* wnd, wxCommandEvent& event )
{
  if ( m_sFilenames.size() == 0 )
    return false;
  
  m_sFilename = m_sFilenames[0].c_str();
  if ( !LayerMRI::LoadVolumeFromFile( wnd, event ) )
    return false;
  
  if ( m_volumeTemp )
    delete m_volumeTemp;
  m_volumeTemp = NULL;
  
  LUTDataHolder* luts = MainWindow::GetMainWindowPointer()->GetLUTData();
  COLOR_TABLE* ct = luts->GetColorTable( m_sLUT );
  if ( !ct )
  {
    cerr << "Can not find look up table." << endl;
  }
  
  event.SetInt( 0 );

  m_imageData = vtkSmartPointer<vtkImageData>::New();
  m_imageData->SetScalarTypeToUnsignedChar();
  m_imageData->SetNumberOfScalarComponents(4);
  m_imageIndex = vtkSmartPointer<vtkImageData>::New();
  m_imageIndex->SetScalarTypeToFloat();
  m_imageIndex->SetNumberOfScalarComponents(2);
  for ( size_t i = 0; i < m_sFilenames.size(); i++ )
  {
    wxString fn = wxFileName( m_sFilenames[i] ).GetName();
    if ( !m_sFilenamePrefix.IsEmpty() )
    {
      fn = fn.Mid( m_sFilenamePrefix.Len() );
      SetName( m_sFilenamePrefix.char_str() );
    }
    
    int r, g, b;
    if ( CTABrgbAtIndexi( ct, CTABentryNameToIndex( fn.char_str(), ct ), &r, &g, &b ) != 0 )
    {
      cerr << "Can not find index for color name " << fn.c_str() << endl;
      return false;
    }
    
    int color[4] = { r, g, b, 255 };
    m_volumeTemp = new FSVolume( m_volumeRef );
    if ( !m_volumeTemp->MRIRead( m_sFilenames[i].c_str(),
                                   m_sRegFilename.size() > 0 ? m_sRegFilename.c_str() : NULL,
                                   wnd,
                                   event ) )
    {
      cerr << "Can not load volume file " << m_sFilenames[i].c_str() << endl;
      return false;
    }
    
    vtkImageData* imageData = m_volumeTemp->GetImageOutput();
    if ( i == 0 ) // initialize m_imageData
    {
      m_imageData->SetDimensions( imageData->GetDimensions() );
      m_imageData->SetOrigin( imageData->GetOrigin() );
      m_imageData->SetSpacing( imageData->GetSpacing() );
      m_imageData->SetExtent( imageData->GetExtent() );
      m_imageData->AllocateScalars();
      m_imageIndex->SetDimensions( imageData->GetDimensions() );
      m_imageIndex->SetOrigin( imageData->GetOrigin() );
      m_imageIndex->SetSpacing( imageData->GetSpacing() );
      m_imageIndex->SetExtent( imageData->GetExtent() );
      m_imageIndex->AllocateScalars();
    }
    
    int* dim = m_imageData->GetDimensions();
    for ( int ni = 0; ni < dim[0]; ni++ )
    {
      for ( int nj = 0; nj < dim[1]; nj++ )
      {
        for ( int nk = 0; nk < dim[2]; nk++ )
        {
          float pvalue = imageData->GetScalarComponentAsFloat( ni, nj, nk, 0 );
          if ( i == 0)
          {
            m_imageIndex->SetScalarComponentFromFloat(ni, nj, nk, 0, 0);
            m_imageIndex->SetScalarComponentFromFloat(ni, nj, nk, 1, pvalue);
          }
          else
          {
            float old_pvalue = m_imageIndex->GetScalarComponentAsFloat( ni, nj, nk, 0 );
            if ( old_pvalue < pvalue)
            {
              m_imageIndex->SetScalarComponentFromFloat(ni, nj, nk, 0, i);  
              m_imageIndex->SetScalarComponentFromFloat(ni, nj, nk, 1, pvalue);
            }
          }
          for ( int m = 0; m < 4; m++ )
          { 
            float fvalue = 0;
            if ( i != 0 )
              fvalue = m_imageData->GetScalarComponentAsFloat( ni, nj, nk, m );
            
            fvalue += color[m]*pvalue/255;
            if ( fvalue > 255 )
              fvalue = 255;
            m_imageData->SetScalarComponentFromFloat( ni, nj, nk, m, fvalue );
          }
        }
      }
    }
    
    delete m_volumeTemp;
    m_volumeTemp = NULL;
  }
  
  InitializeActors();
  for ( int i = 0; i < 3; i++ )
  {
    m_sliceActor2D[i]->SetInput( mReslice[i]->GetOutput() );
    m_sliceActor3D[i]->SetInput( mReslice[i]->GetOutput() );
  }
  
  event.SetInt( 90 );

  event.SetInt( 100 );
  wxPostEvent( wnd, event );

  return true;
}

void LayerPLabel::UpdateColorMap()
{
  // over-ride parent class, do nothing
}

double LayerPLabel::GetVoxelValue( double* pos )
{
  if ( m_imageIndex.GetPointer() == NULL )
    return 0;

  vtkAbstractTransform* tr = mReslice[0]->GetResliceTransform();
  double pos_new[3];
  tr->TransformPoint( pos, pos_new );
  
  double* orig = m_imageIndex->GetOrigin();
  double* vsize = m_imageIndex->GetSpacing();
  int* ext = m_imageIndex->GetExtent();
  
  int n[3];
  for ( int i = 0; i < 3; i++ )
  {
    n[i] = (int)( ( pos_new[i] - orig[i] ) / vsize[i] + 0.5 );
  }

  if ( n[0] < ext[0] || n[0] > ext[1] ||
       n[1] < ext[2] || n[1] > ext[3] ||
       n[2] < ext[4] || n[2] > ext[5] )
    return 0;
  else
    return m_imageIndex->GetScalarComponentAsDouble( n[0], n[1], n[2], 1 );
}

std::string LayerPLabel::GetLabelName( double* pos )
{
  if ( m_imageIndex.GetPointer() == NULL )
    return 0;

  vtkAbstractTransform* tr = mReslice[0]->GetResliceTransform();
  double pos_new[3];
  tr->TransformPoint( pos, pos_new );
  
  double* orig = m_imageIndex->GetOrigin();
  double* vsize = m_imageIndex->GetSpacing();
  int* ext = m_imageIndex->GetExtent();
  
  int n[3];
  for ( int i = 0; i < 3; i++ )
  {
    n[i] = (int)( ( pos_new[i] - orig[i] ) / vsize[i] + 0.5 );
  }

  int nIndex = 0;
  if ( n[0] < ext[0] || n[0] > ext[1] ||
       n[1] < ext[2] || n[1] > ext[3] ||
       n[2] < ext[4] || n[2] > ext[5] )
  {
    return "";
  }
  else
  {
    nIndex = (int)m_imageIndex->GetScalarComponentAsDouble( n[0], n[1], n[2], 0 ); 
    wxString strg = m_sFilenames[nIndex].Mid( m_sFilenamePrefix.Len() );
    int n = strg.Find('.', true);
    if (n >= 0 )
      strg = strg.Left(n);
    return strg.c_str();
  }
}

bool LayerPLabel::DoRotate( std::vector<RotationElement>& rotations, wxWindow* wnd, wxCommandEvent& event )
{
  /* 
  m_bResampleToRAS = false;
  m_volumeSource->SetResampleToRAS( m_bResampleToRAS );
  m_vectorSource->SetResampleToRAS( m_bResampleToRAS );

  bool ret = LayerMRI::Rotate( rotations, wnd, event ) && m_vectorSource->Rotate( rotations, wnd, event );

  InitializeDTIColorMap( wnd, event );
  return ret;*/
  return true;
}

