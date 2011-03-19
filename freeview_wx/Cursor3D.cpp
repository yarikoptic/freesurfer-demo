/**
 * @file  Cursor3D.cpp
 * @brief Cursor for 3D view.
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2011/03/19 18:44:23 $
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

#include <wx/xrc/xmlres.h>
#include "Cursor3D.h"
#include "vtkRenderer.h"
#include "vtkActor2D.h"
#include "vtkProperty.h"
#include "vtkPolyDataMapper.h"
#include <vtkTubeFilter.h>
#include "vtkPolyData.h"
#include "MainWindow.h"
#include "RenderView3D.h"
#include "vtkPolyData.h"
#include "vtkPoints.h"
#include "vtkCellArray.h"
#include "vtkFloatArray.h"
#include <vtkProperty.h>
#include "MyUtils.h"
#include "LayerMRI.h"
#include "LayerPropertiesMRI.h"


Cursor3D::Cursor3D( RenderView3D* view ) :
    m_view( view )
{
  m_actorCursor = vtkSmartPointer<vtkActor>::New();
  m_actorCursor->GetProperty()->SetColor( 1, 0, 0 );
  m_actorCursor->PickableOff();

  RebuildActor();
}

Cursor3D::~Cursor3D()
{}

void Cursor3D::RebuildActor()
{
// vtkRenderer* renderer = m_view->GetRenderer();

  double dLen = 1.5;
  int n = 0;
  vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
  vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
  points->InsertNextPoint( dLen, 0, 0 );
  points->InsertNextPoint( -dLen, 0, 0 );
  lines->InsertNextCell( 2 );
  lines->InsertCellPoint( n++ );
  lines->InsertCellPoint( n++ );
  points->InsertNextPoint( 0, dLen, 0 );
  points->InsertNextPoint( 0, -dLen, 0 );
  lines->InsertNextCell( 2 );
  lines->InsertCellPoint( n++ );
  lines->InsertCellPoint( n++ );
  points->InsertNextPoint( 0, 0, dLen);
  points->InsertNextPoint( 0, 0, -dLen );
  lines->InsertNextCell( 2 );
  lines->InsertCellPoint( n++ );
  lines->InsertCellPoint( n++ );

  vtkSmartPointer<vtkPolyData> polydata = vtkSmartPointer<vtkPolyData>::New();
  polydata->SetPoints( points );
  polydata->SetLines( lines );

  vtkSmartPointer<vtkTubeFilter> tube = vtkSmartPointer<vtkTubeFilter>::New();
  tube->SetInput( polydata );
  tube->SetNumberOfSides( 12 );
  tube->SetRadius( 0.1 );
  tube->CappingOn();

  vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  mapper->SetInputConnection( tube->GetOutputPort() );

  m_actorCursor->SetMapper( mapper );
}


void Cursor3D::SetPosition( double* pos )
{
  for ( int i = 0; i < 3; i++ )
  {
    m_dPosition[i] = pos[i];
  }

  m_actorCursor->SetPosition( pos );
}

void Cursor3D::Update()
{}

void Cursor3D::AppendActor( vtkRenderer* renderer )
{
  renderer->AddViewProp( m_actorCursor );
}

void Cursor3D::SetColor( double r, double g, double b )
{
  m_actorCursor->GetProperty()->SetColor( r, g, b );
}

void Cursor3D::SetColor( const wxColour& color )
{
  m_actorCursor->GetProperty()->SetColor( color.Red()/255.0, color.Green()/255.0, color.Blue()/255.0 );
}

void Cursor3D::GetColor( double* rgb )
{
  m_actorCursor->GetProperty()->GetColor( rgb );
}

wxColour Cursor3D::GetColor()
{
  double* rgb = m_actorCursor->GetProperty()->GetColor();
  return wxColour( (int)(rgb[0]*255), (int)(rgb[1]*255), (int)(rgb[2]*255) );
}

double* Cursor3D::GetPosition()
{
  return m_dPosition;
}

void Cursor3D::GetPosition( double* pos )
{
  for ( int i = 0; i < 3; i++ )
    pos[i] = m_dPosition[i];
}


void Cursor3D::Show( bool bShow )
{
  m_actorCursor->SetVisibility( bShow?1:0 );
}

bool Cursor3D::IsShown()
{
  return m_actorCursor->GetVisibility() > 0;
}
