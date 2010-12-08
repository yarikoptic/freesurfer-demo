/**
 * @file  SurfaceOverlay.cxx
 * @brief Implementation for surface layer properties.
 *
 * In 2D, the MRI is viewed as a single slice, and controls are
 * provided to change the color table and other viewing options. In
 * 3D, the MRI is viewed in three planes in 3D space, with controls to
 * move each plane axially.
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: krish $
 *    $Date: 2010/12/08 23:41:46 $
 *    $Revision: 1.5.2.2 $
 *
 * Copyright (C) 2007-2009,
 * The General Hospital Corporation (Boston, MA).
 * All rights reserved.
 *
 * Distribution, usage and copying of this software is covered under the
 * terms found in the License Agreement file named 'COPYING' found in the
 * FreeSurfer source code root directory, and duplicated here:
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferOpenSourceLicense
 *
 * General inquiries: freesurfer@nmr.mgh.harvard.edu
 * Bug reports: analysis-bugs@nmr.mgh.harvard.edu
 *
 */


#include <assert.h>
#include "SurfaceOverlay.h"
#include "vtkLookupTable.h"
#include "vtkRGBAColorTransferFunction.h"
#include "vtkMath.h"
#include "LayerSurface.h"
#include "SurfaceOverlayProperties.h"
#include "FSSurface.h"
#include <wx/filename.h>


SurfaceOverlay::SurfaceOverlay ( LayerSurface* surf ) :
    Broadcaster( "SurfaceOverlay" ),
    Listener( "SurfaceOverlay" ),
    m_fData( NULL ),
    m_surface( surf ),
    m_bCorrelationData( false ),
    m_mriCorrelation( NULL )
{
  InitializeData();  
  
  m_properties =  new SurfaceOverlayProperties( this );
  m_properties->AddListener( this );
}

SurfaceOverlay::~SurfaceOverlay ()
{
  if ( m_fData )
    delete[] m_fData;
  
  delete m_properties;
  if ( m_mriCorrelation )
  {
    MRIfree( &m_mriCorrelation );
  }
}

void SurfaceOverlay::DoListenToMessage ( std::string const iMessage, void* iData, void* sender )
{
  if ( iMessage == "ColorMapChanged" )
  {
    this->SendBroadcast( "OverlayChanged", this );
  }
}

void SurfaceOverlay::InitializeData()
{
  if ( m_surface )
  {
    MRIS* mris = m_surface->GetSourceSurface()->GetMRIS();
    if ( m_fData )
      delete[] m_fData;
    m_nDataSize = mris->nvertices;
    m_fData = new float[ m_nDataSize ];
    if ( !m_fData )
      return;

    m_dMaxValue = m_dMinValue = mris->vertices[0].val;
    for ( int vno = 0; vno < m_nDataSize; vno++ )
    {
      m_fData[vno] = mris->vertices[vno].val;
      if ( m_dMaxValue < m_fData[vno] )
        m_dMaxValue = m_fData[vno];
      else if ( m_dMinValue > m_fData[vno] )
        m_dMinValue = m_fData[vno];
    }
  }
}

bool SurfaceOverlay::LoadCorrelationData( const char* filename )
{
  MRI* mri = ::MRIreadHeader( filename, -1 ); 
  if ( mri == NULL )
  {
    cerr << "MRIread failed" << endl;
    return false;
  }
  if ( mri->width != m_nDataSize*2 || mri->height != m_nDataSize*2 )
  {
    cerr << "Correlation data does not match with surface" << endl;
    MRIfree( &mri );
    return false;
  }
  MRIfree( &mri );
  mri = ::MRIread( filename );      // long process
  if ( mri == NULL )
  {
    cerr << "MRIread failed" << endl;
    return false;
  }
  m_mriCorrelation = mri;
  m_bCorrelationData = true;
  m_bCorrelationDataReady = false;
  
  return true;
}

void SurfaceOverlay::UpdateCorrelationAtVertex( int nVertex )
{
  int nOffset = m_surface->GetHemisphere() * m_nDataSize;
  m_dMaxValue = m_dMinValue = MRIFseq_vox( m_mriCorrelation, nOffset, nVertex + nOffset, 0, 0 );
  for ( int i = 0; i < m_nDataSize; i++ )
  {
    m_fData[i] = MRIFseq_vox( m_mriCorrelation, i + nOffset, nVertex + nOffset, 0, 0 );
    if ( m_dMaxValue < m_fData[i] )
      m_dMaxValue = m_fData[i];
    else if ( m_dMinValue > m_fData[i] )
      m_dMinValue = m_fData[i];
  }
  m_bCorrelationDataReady = true;
}

const char* SurfaceOverlay::GetName()
{
  return m_strName.c_str();
}

void SurfaceOverlay::SetName( const char* name )
{
  m_strName = name;
}

void SurfaceOverlay::MapOverlay( unsigned char* colordata )
{
  if ( !m_bCorrelationData || m_bCorrelationDataReady )
    m_properties->MapOverlayColor( colordata, m_nDataSize );
}

double SurfaceOverlay::GetDataAtVertex( int nVertex )
{
  return m_fData[nVertex];
}


