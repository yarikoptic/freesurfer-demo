/**
 * @file  Region2DLine.cpp
 * @brief Region2DLine.
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

#include "Region2DLine.h"
#include "RenderView2D.h"
#include <vtkActor2D.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>
#include <vtkPlaneSource.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkCoordinate.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkMath.h>
#include <vtkLine.h>
#include "LayerMRI.h"

Region2DLine::Region2DLine( RenderView2D* view ) :
  Region2D( view )
{
  m_actorLine = vtkSmartPointer<vtkActor2D>::New();
  m_actorLine->GetProperty()->SetOpacity( 0.75 );
  m_actorLine->GetProperty()->SetLineWidth( 3 );
//  m_actorLine->VisibilityOff();
  m_actorText = vtkSmartPointer<vtkTextActor>::New();
  m_actorText->SetTextScaleMode( vtkTextActor::TEXT_SCALE_MODE_NONE ); 
  m_actorText->GetTextProperty()->SetColor( 1, 1, 1 );
  m_actorText->GetTextProperty()->ShadowOn(); 
  m_actorText->GetTextProperty()->SetShadowOffset( 1, 1 );
  m_actorText->GetTextProperty()->SetFontSize( 15 );  
  Highlight( true );
}

Region2DLine::~Region2DLine()
{}


void Region2DLine::SetLine( int x1, int y1, int x2, int y2 )
{
  m_nX1 = x1;
  m_nY1 = y1;
  m_nX2 = x2;
  m_nY2 = y2;  
  
  UpdateWorldCoords();
  Update();
}

void Region2DLine::UpdateWorldCoords()
{
  m_view->MousePositionToRAS( m_nX1, m_nY1, m_dPt1 ); 
  m_view->MousePositionToRAS( m_nX2, m_nY2, m_dPt2 );
}

void Region2DLine::SetPoint1( int x1, int y1 )
{
  m_nX1 = x1;
  m_nY1 = y1;
  
  m_view->MousePositionToRAS( m_nX1, m_nY1, m_dPt1 ); 
  Update();
}

void Region2DLine::SetPoint2( int x2, int y2 )
{
  m_nX2 = x2;
  m_nY2 = y2;
  
  m_view->MousePositionToRAS( m_nX2, m_nY2, m_dPt2 );
  Update();
}

void Region2DLine::Update()
{
  if ( !m_actorLine->GetVisibility() )
    return;
  
  double pt1[3], pt2[3], pt3[3];
 
  m_view->WorldToViewport( m_dPt1[0], m_dPt1[1], m_dPt1[2], pt1[0], pt1[1], pt1[2] );
  m_view->WorldToViewport( m_dPt2[0], m_dPt2[1], m_dPt2[2], pt2[0], pt2[1], pt2[2] );
  for ( int i = 0; i < 3; i++ )
    pt3[i] = (pt1[i] + pt2[i])/ 2.0;
  
  vtkSmartPointer<vtkPoints> pts = vtkSmartPointer<vtkPoints>::New();
  vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
  pts->InsertNextPoint( pt1 );
  pts->InsertNextPoint( pt2 );
  vtkIdType indices[2] = { 0, 1 };
  lines->InsertNextCell( 2, indices );
  
  vtkSmartPointer<vtkPolyData> polydata = vtkSmartPointer<vtkPolyData>::New();
  polydata->SetPoints( pts );
  polydata->SetLines( lines );
  vtkSmartPointer<vtkPolyDataMapper2D> mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
  mapper->SetInput( polydata );
  
  vtkSmartPointer<vtkCoordinate> coords = vtkSmartPointer<vtkCoordinate>::New();
  coords->SetCoordinateSystemToViewport();
  mapper->SetTransformCoordinate( coords );
  m_actorLine->SetMapper( mapper );
  
//  m_actorText->SetInput( GetShortStats().c_str() );
  m_actorText->SetPosition( pt3 );
  UpdateStats();
}

void Region2DLine::UpdateStats()
{
  char ch[1000];
  sprintf( ch, "%.2f mm", sqrt(vtkMath::Distance2BetweenPoints( m_dPt1, m_dPt2 )) );
  m_strShortStats = ch;
  m_actorText->SetInput( ch );
 
  LayerMRI* layer = m_view->GetFirstNonLabelVolume();
  if ( layer )
  {
    double* values = NULL;
    int* indices = NULL;
    int count = 0;
    layer->GetVoxelsOnLine( m_dPt1, m_dPt2, m_view->GetViewPlane(), indices, values, &count );
    char ch[1000];  
    m_strsLongStats.clear();
    for ( int i = 0; i < count; i++ )
    {
      sprintf( ch, "[%d, %d, %d]  %.2f", indices[i*3], indices[i*3+1], indices[i*3+2], values[i] );
      m_strsLongStats.push_back( ch );
    }
    delete[] indices;
    delete[] values;
  }
  
  Region2D::UpdateStats();
}

void Region2DLine::AppendProp( vtkRenderer* renderer )
{
  renderer->AddViewProp( m_actorLine );
  renderer->AddViewProp( m_actorText );
}

void Region2DLine::Show( bool bShow )
{
  m_actorLine->SetVisibility( bShow?1:0 );
  Update();
}

void Region2DLine::GetWorldPoint( int nIndex, double* pt )
{
  if ( nIndex == 0 )
  {
    pt[0] = m_dPt1[0];
    pt[1] = m_dPt1[1];
    pt[2] = m_dPt1[2];
  }
  else
  {
    pt[0] = m_dPt2[0];
    pt[1] = m_dPt2[1];
    pt[2] = m_dPt2[2];
  }
}

void Region2DLine::Offset( int x, int y )
{
  double pt0[3], pt[3];
  m_view->MousePositionToRAS( 0, 0, pt0 );
  m_view->MousePositionToRAS( x, y, pt );
  for ( int i = 0; i < 3; i++ )
  {
    m_dPt1[i] += ( pt[i] - pt0[i] );
    m_dPt2[i] += ( pt[i] - pt0[i] );
  }
  Update();
}

// indexOut returns the index of the vertex point
// returns -1 if the hit point is not close to any vertices
bool Region2DLine::Contains( int x, int y, int* indexOut )
{
  // first calculate the threshold distance in world space
  double pt1[3], pt2[3];
  m_view->MousePositionToRAS( 0, 0, pt1 );
  m_view->MousePositionToRAS( 5, 5, pt2 );
  double dTh2 = vtkMath::Distance2BetweenPoints( pt1, pt2 );
  
  // calculate the hit point in world space
  double pt[3];
  m_view->MousePositionToRAS( x, y, pt );
  int nPlane = m_view->GetViewPlane();
  pt[nPlane] = m_dPt1[nPlane];
  
  if ( vtkMath::Distance2BetweenPoints( pt, m_dPt1 ) < dTh2 )
  {
    if ( indexOut )
      *indexOut = 0;
    return true;
  }
  else if ( vtkMath::Distance2BetweenPoints( pt, m_dPt2 ) < dTh2 )
  {
    if ( indexOut )
      *indexOut = 1;
    return true;
  }
  else if ( indexOut )
    *indexOut = -1;
  
  double closestPt[3], t;
  if ( vtkLine::DistanceToLine( pt, m_dPt1, m_dPt2, t, closestPt ) < dTh2 )
  {
    if ( t >= 0 && t <= 1 )
      return true;
  }
  return false;
}

void Region2DLine::Highlight( bool bHighlight )
{
  m_actorLine->GetProperty()->SetColor( bHighlight?0:1, 1, 0);
}

void Region2DLine::UpdatePoint( int nIndex, int nX, int nY )
{
  if ( nIndex == 0 )
    m_view->MousePositionToRAS( nX, nY, m_dPt1 );
  else if ( nIndex == 1 )
    m_view->MousePositionToRAS( nX, nY, m_dPt2 );
  Update();
}

void Region2DLine::UpdateSlicePosition( int nPlane, double pos )
{
  m_dPt1[nPlane] = pos;
  m_dPt2[nPlane] = pos;
  Region2D::UpdateSlicePosition( nPlane, pos );
}
