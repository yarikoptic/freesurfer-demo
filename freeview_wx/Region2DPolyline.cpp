/**
 * @file  Region2DPolyline.cpp
 * @brief Region2DPolyline.
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

#include "Region2DPolyline.h"
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
#include <vtkSplineFilter.h>
#include <vtkMath.h>
#include <vtkLine.h>
#include <vtkParametricSpline.h>
#include "LayerMRI.h"

Region2DPolyline::Region2DPolyline( RenderView2D* view, bool bSpline ) :
  Region2D( view )
{
  m_bSpline = bSpline;
  m_actorPolyline = vtkSmartPointer<vtkActor2D>::New();
  m_actorPolyline->GetProperty()->SetOpacity( 0.75 );
  m_actorPolyline->GetProperty()->SetLineWidth( 3 );
  m_actorPoints = vtkSmartPointer<vtkActor2D>::New();
  m_actorPoints->GetProperty()->SetOpacity( 0.75 );
  m_actorPoints->GetProperty()->SetPointSize( 5 );

  m_actorText = vtkSmartPointer<vtkTextActor>::New();
  m_actorText->SetTextScaleMode( vtkTextActor::TEXT_SCALE_MODE_NONE ); 
  m_actorText->GetTextProperty()->SetColor( 1, 1, 1 );
  m_actorText->GetTextProperty()->ShadowOn(); 
  m_actorText->GetTextProperty()->SetShadowOffset( 1, 1 );
  m_actorText->GetTextProperty()->SetFontSize( 15 );  
  Highlight( true );
}

Region2DPolyline::~Region2DPolyline()
{}


void Region2DPolyline::AddPoint( int x, int y )
{
  ScreenPoint pt;
  pt.pos[0] = x;
  pt.pos[1] = y;
  m_screenPts.push_back( pt );
  
  UpdateWorldCoords();
  Update();
}

void Region2DPolyline::RemoveLastPoint()
{
  m_screenPts.pop_back();
  m_worldPts.pop_back();
  Update();
}

void Region2DPolyline::UpdateWorldCoords()
{
  WorldPoint wp;
  for ( size_t i = m_worldPts.size(); i < m_screenPts.size(); i++ )
  {
    m_view->MousePositionToRAS( m_screenPts[i].pos[0], m_screenPts[i].pos[1], wp.pos ); 
    if ( i > 0 && vtkMath::Distance2BetweenPoints( wp.pos, m_worldPts[i-1].pos ) < 1e-8 )
    {
      wp.pos[0] += 0.00001;
      wp.pos[1] += 0.00001;
      wp.pos[2] += 0.00001;
    }
    m_worldPts.push_back( wp );
  }
}

void Region2DPolyline::Update()
{
  if ( !m_actorPolyline->GetVisibility() || m_worldPts.size() < 2 )
    return;
  
  double pt[3];
  vtkSmartPointer<vtkPoints> pts = vtkSmartPointer<vtkPoints>::New();
  vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
  lines->InsertNextCell( m_worldPts.size() );
  for ( size_t i = 0; i < m_worldPts.size(); i++ )
  {
    m_view->WorldToViewport( m_worldPts[i].pos[0], m_worldPts[i].pos[1], m_worldPts[i].pos[2], pt[0], pt[1], pt[2] );
    pts->InsertNextPoint( pt );
    lines->InsertCellPoint( i );
  }
  
  vtkSmartPointer<vtkPolyData> polydata = vtkSmartPointer<vtkPolyData>::New();
  polydata->SetPoints( pts );
  polydata->SetLines( lines );
  vtkSmartPointer<vtkPolyDataMapper2D> mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
  if ( !m_bSpline )
  {
    mapper->SetInput( polydata );
  }
  else
  {
    vtkSmartPointer<vtkSplineFilter> spline = vtkSmartPointer<vtkSplineFilter>::New();
    spline->SetInput( polydata );
    LayerMRI* layer = m_view->GetFirstNonLabelVolume();
    if ( layer )
    {
      spline->SetSubdivideToLength(); 
      spline->SetLength( 3*layer->GetMinimumVoxelSize() );
    }
    spline->Update();
    mapper->SetInputConnection( spline->GetOutputPort() );
  }
    
  vtkSmartPointer<vtkCoordinate> coords = vtkSmartPointer<vtkCoordinate>::New();
  coords->SetCoordinateSystemToViewport();
  mapper->SetTransformCoordinate( coords );
  m_actorPolyline->SetMapper( mapper );
  
  vtkSmartPointer<vtkPolyData> polydata2 = vtkSmartPointer<vtkPolyData>::New();
  polydata2->SetPoints( pts );
  vtkSmartPointer<vtkCellArray> verts = vtkSmartPointer<vtkCellArray>::New();
  verts->Allocate( pts->GetNumberOfPoints() );
  for ( int i = 0; i < pts->GetNumberOfPoints(); i++ )
  {
    vtkIdType n = i;
    verts->InsertNextCell( 1, &n );
  }
  polydata2->SetVerts( verts );
  mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
  mapper->SetInput( polydata2 );
  mapper->SetTransformCoordinate( coords );
  m_actorPoints->SetMapper( mapper );
  
//  m_actorText->SetInput( GetShortStats().c_str() );
//  double mid_pt[3];  
  m_actorText->SetPosition( pts->GetPoint( 0 ) );
  UpdateStats();
}

void Region2DPolyline::UpdateStats()
{
  if ( m_worldPts.size() < 2 )
    return;
  
  double dist = 0;
  char ch[1000];
  LayerMRI* layer = m_view->GetFirstNonLabelVolume();
  m_strsLongStats.clear();
  if ( m_bSpline )
  {
    vtkSmartPointer<vtkPoints> pts = vtkSmartPointer<vtkPoints>::New();
    for ( size_t i = 0; i < m_worldPts.size(); i++ )
      pts->InsertNextPoint( m_worldPts[i].pos );
    vtkSmartPointer<vtkParametricSpline> spline = vtkSmartPointer<vtkParametricSpline>::New();
    spline->SetPoints( pts );
    
    int nSteps = 100;
    double* values = NULL;
    int* indices = NULL;
    for ( int i = 0; i < nSteps; i++ )
    {
      double pt1[3], pt2[3], u[3], du[9];
      double uvalue = ((double)i)/nSteps;
      u[0] = u[1] = u[2] = uvalue;
      spline->Evaluate( u, pt1, du );
      uvalue = (i+1.0)/nSteps;
      u[0] = u[1] = u[2] = uvalue;
      spline->Evaluate( u, pt2, du );
      dist += sqrt( vtkMath::Distance2BetweenPoints( pt1, pt2 ) );
      int count = 0;
      if ( layer )
      {
        layer->GetVoxelsOnLine( pt1, pt2, m_view->GetViewPlane(), indices, values, &count );
        for ( int j = 0; j < (i==nSteps-1?count:count-1); j++ )
        {
          sprintf( ch, "[%d, %d, %d]  %.2f", indices[j*3], indices[j*3+1], indices[j*3+2], values[j] );
          m_strsLongStats.push_back( ch );
        }
        delete[] values;
        delete[] indices;
      }
    }
  }
  else
  {
    double* values = NULL;
    int* indices = NULL;
    for ( size_t i = 1; i < m_worldPts.size(); i++ )
    {
      dist += sqrt( vtkMath::Distance2BetweenPoints( m_worldPts[i-1].pos, m_worldPts[i].pos ) );
      int count = 0;
      if ( layer )
      {
        layer->GetVoxelsOnLine( m_worldPts[i-1].pos, m_worldPts[i].pos, m_view->GetViewPlane(), indices, values, &count );
        for ( int j = 0; j < (i==m_worldPts.size()-1?count:count-1); j++ )
        {
          sprintf( ch, "[%d, %d, %d]  %.2f", indices[j*3], indices[j*3+1], indices[j*3+2], values[j] );
          m_strsLongStats.push_back( ch );
        }
        delete[] values;
        delete[] indices;
      }
    }
  }
    
  sprintf( ch, "%.2f mm", dist );
  m_strShortStats = ch;
  m_actorText->SetInput( ch );
  
  Region2D::UpdateStats();
}

void Region2DPolyline::AppendProp( vtkRenderer* renderer )
{
  renderer->AddViewProp( m_actorPolyline );
  renderer->AddViewProp( m_actorPoints );
  renderer->AddViewProp( m_actorText );
}

void Region2DPolyline::Show( bool bShow )
{
  m_actorPolyline->SetVisibility( bShow?1:0 );
  m_actorPoints->SetVisibility( bShow?1:0 );
  m_actorText->SetVisibility( bShow?1:0 );
  Update();
}

void Region2DPolyline::GetWorldPoint( int nIndex, double* pt )
{
  if ( nIndex < (int)m_worldPts.size() )
  {
    pt[0] = m_worldPts[nIndex].pos[0];
    pt[1] = m_worldPts[nIndex].pos[1];
    pt[2] = m_worldPts[nIndex].pos[2];
  }
}

void Region2DPolyline::Offset( int x, int y )
{
  double pt0[3], pt[3];
  m_view->MousePositionToRAS( 0, 0, pt0 );
  m_view->MousePositionToRAS( x, y, pt );
  for ( size_t n = 0; n < m_worldPts.size(); n++ )
  {
    for ( int i = 0; i < 3; i++ )
    {
      m_worldPts[n].pos[i] += ( pt[i] - pt0[i] );
    }
  }
  Update();
}

// indexOut returns the index of the vertex point
// returns -1 if the hit point is not close to any vertices
bool Region2DPolyline::Contains( int x, int y, int* indexOut )
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
  
  for ( size_t i = 0; i < m_worldPts.size(); i++ )
  {
    pt[nPlane] = m_worldPts[i].pos[nPlane];
    if ( vtkMath::Distance2BetweenPoints( pt, m_worldPts[i].pos ) < dTh2 )
    {
      if ( indexOut )
        *indexOut = i;
      return true;
    }
  }
  if ( indexOut )
    *indexOut = -1;
  
  double closestPt[3], t;
  for ( size_t i = 0; i < m_worldPts.size()-1; i++ )
  {
    if ( vtkLine::DistanceToLine( pt, m_worldPts[i].pos, m_worldPts[i+1].pos, t, closestPt ) < dTh2 )
    {
      if ( t >= 0 && t <= 1 )
        return true;
    }
  }
  return false;
}

void Region2DPolyline::Highlight( bool bHighlight )
{
  m_actorPolyline->GetProperty()->SetColor( bHighlight?0:1, 1, 0);
  m_actorPoints->GetProperty()->SetColor( bHighlight?0:1, 1, 0);
}

void Region2DPolyline::UpdatePoint( int nIndex, int nX, int nY )
{
  if ( nIndex < 0 )
    nIndex = m_worldPts.size() - 1;
  
  if ( nIndex < (int)m_worldPts.size() )
  {
    m_view->MousePositionToRAS( nX, nY, m_worldPts[nIndex].pos );
    Update();
  }
}

void Region2DPolyline::UpdateSlicePosition( int nPlane, double pos )
{
  for ( size_t i = 0; i < m_worldPts.size(); i++ )
    m_worldPts[i].pos[nPlane] = pos;
  
  Region2D::UpdateSlicePosition( nPlane, pos );
}
