/**
 * @file  LayerWayPoints.cpp
 * @brief Layer data object for MRI surface.
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

#include "LayerWayPoints.h"
#include "LayerMRI.h"
#include "FSWayPoints.h"
#include "LayerPropertiesWayPoints.h"
#include "FSVolume.h"
#include "vtkRGBAColorTransferFunction.h"
#include <vtkRenderer.h>
#include <vtkActor.h>
#include <vtkMath.h>
#include <vtkSphereSource.h>
#include <vtkSplineFilter.h>
#include <vtkPoints.h>
#include <vtkPolyDataMapper.h>
#include <vtkAppendPolyData.h>
#include <vtkCellArray.h>
#include <vtkFloatArray.h>
#include <vtkTubeFilter.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkPlane.h>
#include <vtkCutter.h>
#include <vtkStripper.h>
#include <vtkTriangleFilter.h>
#include <vtkSmartPointer.h>
#include <vtkPointData.h>
#include <stdlib.h>

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#define NUM_OF_SIDES  10  // must be even number!

LayerWayPoints::LayerWayPoints( LayerMRI* ref, int nType ) : LayerEditable()
{
  m_strTypeNames.push_back( "WayPoints" );
  m_actorBalls = vtkActor::New();
  m_actorSpline = vtkActor::New();
  for ( int i = 0; i < 3; i++ )
  {
    m_actorSlice[i] = vtkActor::New();
    m_actorSlice[i]->GetProperty()->SetInterpolationToFlat();
    m_actorSlice[i]->GetProperty()->SetAmbient( 1 );
    m_actorSlice[i]->GetProperty()->SetDiffuse( 0 );
  }
  m_layerRef = ref;
  m_waypointsSource = new FSWayPoints();

  mProperties = new LayerPropertiesWayPoints();
  mProperties->AddListener( this );
  GetProperties()->SetType( nType );

  m_mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  
  if ( nType == LayerPropertiesWayPoints::ControlPoints )
  {
    GetProperties()->SetShowSpline( false );
    GetProperties()->SetRadius ( 0.5 );
    GetProperties()->SetSnapToVoxelCenter( true );
    GetProperties()->SetColor( 0, 1, 0 );
  }
}

LayerWayPoints::~LayerWayPoints()
{
  m_actorBalls->Delete();
  m_actorSpline->Delete();
  for ( int i = 0; i < 3; i++ )
  {
    m_actorSlice[i]->Delete();
  }
}

bool LayerWayPoints::LoadFromFile( std::string filename )
{
  if ( GetProperties()->GetType() == LayerPropertiesWayPoints::ControlPoints )
  {
    if ( !m_waypointsSource->ReadAsControlPoints( filename.c_str() ) )
      return false;
  }
  else
  {
    if ( !m_waypointsSource->ReadAsLabel( filename.c_str() ) )
      return false;
  }

  m_points.clear();
  m_waypointsSource->LabelToWayPoints( m_points, m_layerRef->GetSourceVolume() );
  SetFileName( filename.c_str() );
  RebuildActors();

  return true;
}

bool LayerWayPoints::Save()
{
  if ( m_sFilename.size() == 0 || m_layerRef == NULL )
    return false;

  m_waypointsSource->UpdateLabel( m_points, m_layerRef->GetSourceVolume() );

  bool bSaved = false;
  if ( GetProperties()->GetType() == LayerPropertiesWayPoints::ControlPoints )
    bSaved = m_waypointsSource->WriteAsControlPoints( m_sFilename.c_str() );
  else
    bSaved = m_waypointsSource->WriteAsLabel( m_sFilename.c_str() );
    
  if ( !bSaved )
    m_bModified = true;

  return bSaved;
}

void LayerWayPoints::UpdateLabelData()
{
  m_waypointsSource->UpdateLabel( m_points, m_layerRef->GetSourceVolume() );
}

bool LayerWayPoints::HasUndo()
{
  return m_bufferUndo.size() > 0;
}

bool LayerWayPoints::HasRedo()
{
  return m_bufferRedo.size() > 0;
}

void LayerWayPoints::Undo()
{
  if ( m_bufferUndo.size() > 0 )
  {
    m_bufferRedo.push_back( m_points );
    m_points = m_bufferUndo[m_bufferUndo.size()-1];
    m_bufferUndo.pop_back();
    RebuildActors();
  }
}

void LayerWayPoints::Redo()
{
  if ( m_bufferRedo.size() > 0 )
  {
    m_bufferUndo.push_back( m_points );
    m_points = m_bufferRedo[m_bufferRedo.size()-1];
    m_bufferRedo.pop_back();
    RebuildActors();
  }
}

void LayerWayPoints::SaveForUndo()
{
  m_bufferUndo.push_back( m_points );
  m_bufferRedo.clear();
}

void LayerWayPoints::Append2DProps( vtkRenderer* renderer, int nPlane )
{
  if ( nPlane < 3 && nPlane >= 0 )
    renderer->AddViewProp( m_actorSlice[nPlane] );
}

void LayerWayPoints::Append3DProps( vtkRenderer* renderer, bool* bSliceVisibility )
{
  renderer->AddViewProp( m_actorSpline );
  renderer->AddViewProp( m_actorBalls );
}

void LayerWayPoints::SetVisible( bool bVisible )
{
  if ( GetProperties()->GetShowSpline() )
    m_actorSpline->SetVisibility( bVisible ? 1 : 0 );
  else
    m_actorSpline->SetVisibility( 0 );
  m_actorBalls->SetVisibility( bVisible ? 1 : 0 );
  for ( int i = 0; i < 3; i++ )
    m_actorSlice[i]->SetVisibility( bVisible ? 1 : 0 );

  this->SendBroadcast( "LayerActorUpdated", this );
}

bool LayerWayPoints::IsVisible()
{
  return m_actorSlice[0]->GetVisibility() > 0;
}

bool LayerWayPoints::HasProp( vtkProp* prop )
{
  return ( prop == m_actorSpline || prop == m_actorBalls );
}

void LayerWayPoints::OnSlicePositionChanged( int nPlane )
{
  RebuildActors( false );   // no need to rebuild 3D points
}

void LayerWayPoints::RebuildActors( bool bRebuild3D )
{
  // 2D
  if ( !m_layerRef )
    return;

  MRI* mri = m_layerRef->GetSourceVolume()->GetMRITarget();
  double voxel_size[3] = { mri->xsize, mri->ysize, mri->zsize };
// double* origin = m_layerRef->GetWorldOrigin();
  double scale = min( voxel_size[0], min( voxel_size[1], voxel_size[2] ) );
  double radius = GetProperties()->GetRadius();
  for ( int i = 0; i < 3; i++ )
  {
    vtkPolyDataMapper* mapper = vtkPolyDataMapper::New();
    vtkAppendPolyData* append = vtkAppendPolyData::New();
    int n = 0;
    for ( size_t j = 0; j < m_points.size(); j++ )
    {
      if ( fabs( m_dSlicePosition[i] - m_points[j].pt[i] ) < ( voxel_size[i] / 2 ) )
      {
        vtkSphereSource* sphere = vtkSphereSource::New();
        double point[3] = { m_points[j].pt[0], m_points[j].pt[1], m_points[j].pt[2] };
        point[i] = m_dSlicePosition[i];
        sphere->SetCenter( point );
        sphere->SetRadius( radius * scale );
        sphere->SetThetaResolution( 12 );
        sphere->SetPhiResolution( 24 );

        vtkSmartPointer<vtkPlane> plane = vtkSmartPointer<vtkPlane>::New();
        plane->SetOrigin( point );
        plane->SetNormal( i==0?1:0, i==1?1:0, i==2?1:0 );

        vtkSmartPointer<vtkCutter> cutter =
          vtkSmartPointer<vtkCutter>::New();
        cutter->SetInputConnection( sphere->GetOutputPort() );
        cutter->SetCutFunction( plane );

        vtkSmartPointer<vtkStripper> stripper = vtkSmartPointer<vtkStripper>::New();
        stripper->SetInputConnection( cutter->GetOutputPort() );
        stripper->Update();

        vtkSmartPointer<vtkPolyData> cutpoly = vtkSmartPointer<vtkPolyData>::New();
        cutpoly->SetPoints( stripper->GetOutput()->GetPoints() );
        cutpoly->SetPolys( stripper->GetOutput()->GetLines() );

        vtkSmartPointer<vtkTriangleFilter> triangleFilter = vtkSmartPointer<vtkTriangleFilter>::New();
        triangleFilter->SetInput( cutpoly );

        // append->AddInput( triangleFilter->GetOutput() );
        append->AddInput( sphere->GetOutput() );
        sphere->Delete();
        n++;
      }
    }
    if ( n > 0 )
      mapper->SetInputConnection( append->GetOutputPort() );
    else
      mapper->SetInput( vtkSmartPointer<vtkPolyData>::New() );
    m_actorSlice[i]->SetMapper( mapper );
    append->Delete();
    mapper->Delete();
  }

  if ( !bRebuild3D )
    return;
  
  // 3D
  vtkAppendPolyData* append = vtkAppendPolyData::New();
  vtkPoints* pts = vtkPoints::New();
  vtkCellArray* lines = vtkCellArray::New();
  lines->InsertNextCell( m_points.size() );
  for ( size_t i = 0; i < m_points.size(); i++ )
  {
    vtkSphereSource* sphere = vtkSphereSource::New();
    sphere->SetCenter( m_points[i].pt );
    sphere->SetRadius( radius * scale );
    sphere->SetThetaResolution( 10 );
    sphere->SetPhiResolution( 20 );
    append->AddInput( sphere->GetOutput() );
    sphere->Delete();
    pts->InsertNextPoint( m_points[i].pt );
    lines->InsertCellPoint( i );
  }
  vtkPolyDataMapper* mapper = vtkPolyDataMapper::New();
  if ( m_points.size() > 0 )
    mapper->SetInput( append->GetOutput() );
  else
    mapper->SetInput( vtkSmartPointer<vtkPolyData>::New() );
  append->Delete();
  m_actorBalls->SetMapper( mapper );
  mapper->Delete();

  if ( GetProperties()->GetShowSpline() )
  {
    vtkPolyData* polydata = vtkPolyData::New();
    polydata->SetPoints( pts );
    polydata->SetLines( lines );
    vtkSplineFilter* spline = vtkSplineFilter::New();
    if ( GetProperties()->GetScalarType() == LayerPropertiesWayPoints::ScalarSet )
    {
      spline->SetSubdivideToSpecified();
      spline->SetNumberOfSubdivisions( GetProperties()->GetActiveScalarSet().nNum - 1 );
    }
    if ( m_points.size() > 1 )
    {
      spline->SetInput( polydata );
      vtkTubeFilter* tube = vtkTubeFilter::New();
      tube->SetNumberOfSides( NUM_OF_SIDES );
      tube->SetInputConnection( spline->GetOutputPort() );
      tube->SetRadius( GetProperties()->GetSplineRadius() * scale );
      tube->CappingOn();
      m_mapper->SetInputConnection( tube->GetOutputPort() );
      tube->Update();
      m_actorSpline->SetMapper( m_mapper );
      UpdateScalars();
      tube->Delete();
    }
    polydata->Delete();
    spline->Delete();
  }
  
  pts->Delete();
  lines->Delete();

  UpdateColorMap();
  UpdateOpacity();

  SendBroadcast( "LayerActorUpdated", this );
}

void LayerWayPoints::UpdateScalars()
{
  if ( 1 )
  {
    LayerMRI* layer = GetProperties()->GetScalarLayer();
    vtkPolyData* polydata = m_mapper->GetInput();
    vtkPoints* pts = polydata->GetPoints();
    int nPts = pts->GetNumberOfPoints();
    vtkFloatArray* scalars = vtkFloatArray::New();
    scalars->SetNumberOfValues( nPts );
    double pt[3] = { 0, 0, 0 };
    double val = 0;
    for ( int i = 0; i < nPts; i++ )
    {
      if ( (i%NUM_OF_SIDES) == 0 )
      {
        double* p1 = pts->GetPoint( i );
        double* p2 = pts->GetPoint( i + NUM_OF_SIDES/2 );
        pt[0] = ( p1[0] + p2[0] ) / 2;
        pt[1] = ( p1[1] + p2[1] ) / 2;
        pt[2] = ( p1[2] + p2[2] ) / 2;
        if ( layer && GetProperties()->GetScalarType() == LayerPropertiesWayPoints::ScalarLayer )
          val = layer->GetVoxelValue( pt );
        else if ( GetProperties()->GetScalarType() == LayerPropertiesWayPoints::ScalarSet )
          val = GetProperties()->GetActiveScalarSet().dValue[i/NUM_OF_SIDES];
      }
      scalars->SetValue( i, val );
    }
    polydata->GetPointData()->SetScalars( scalars );
    polydata->Modified();
    scalars->Delete();
  }
}

// return index of the point if found, otherwise returns -1
int LayerWayPoints::FindPoint( double* ras, double tolerance )
{
  double dt = tolerance;
  if ( dt < 0 )
  {
    double* voxel_size = m_layerRef->GetWorldVoxelSize();
    dt = GetProperties()->GetRadius() * min( voxel_size[0], min( voxel_size[1], voxel_size[2] ) );
    dt = dt * dt;
  }
  for ( size_t i = 0; i < m_points.size(); i++ )
  {
    if ( vtkMath::Distance2BetweenPoints( m_points[i].pt, ras ) < dt )
      return i;
  }
  return -1;
}

// returns index of the point
int LayerWayPoints::AddPoint( double* ras_in, double value )
{
// cout << ras[0] << " " << ras[1] << " " << ras[2] << endl;
  double ras[3];
  if ( GetProperties()->GetSnapToVoxelCenter() )
  {
    m_layerRef->SnapToVoxelCenter( ras_in, ras );
  }
  else
  {
    ras[0] = ras_in[0];
    ras[1] = ras_in[1];
    ras[2] = ras_in[2];
  }
  
  if ( m_points.size() < 2 )
  {
    WayPoint p;
    p.pt[0] = ras[0];
    p.pt[1] = ras[1];
    p.pt[2] = ras[2];
    p.value = value;
    m_points.push_back( p );

    RebuildActors();

    SetModified();

    return m_points.size() - 1;
  }
  else
  {
    // first find the closest point
    double dist = 1e20;
    int n = 0;
    for ( size_t i = 0; i < m_points.size(); i++ )
    {
      double temp = vtkMath::Distance2BetweenPoints( ras, m_points[i].pt );
      if ( temp < dist )
      {
        n = i;
        dist = temp;
      }
    }

    // then determine where to insert the point
    int n1, n2;
    if ( n == 0 )
    {
      n1 = 0;
      n2 = 1;
    }
    else
    {
      n1 = n-1;
      n2 = n;
    }

    double d1 = vtkMath::Distance2BetweenPoints( ras, m_points[n1].pt );
    double d2 = vtkMath::Distance2BetweenPoints( ras, m_points[n2].pt );
    double d3 = vtkMath::Distance2BetweenPoints( m_points[n1].pt, m_points[n2].pt );

    if ( d3 >= d1 && d3 >= d2 )
      n = n2;
    else if ( d1 < d2 )
    {
      n = n1;
    }
    else
      n = n2 + 1;

    WayPoint p;
    p.pt[0] = ras[0];
    p.pt[1] = ras[1];
    p.pt[2] = ras[2];
    p.value = value;

    if ( n >= (int)m_points.size() )
      m_points.push_back( p );
    else
      m_points.insert( m_points.begin() + n, p );

    SetModified();

    RebuildActors();

    return n;
  }
}

bool LayerWayPoints::RemovePoint( double* ras, double tolerance )
{
  return RemovePoint( FindPoint( ras ) );
}

bool LayerWayPoints::RemovePoint( int nIndex )
{
  if ( nIndex < 0 || nIndex >= (int)m_points.size() )
    return false;

  m_points.erase( m_points.begin() + nIndex );

  SetModified();

  RebuildActors();

  return true;
}

void LayerWayPoints::UpdatePoint( double* ras, int nIndex, bool rebuildActor )
{
  if ( GetProperties()->GetSnapToVoxelCenter() )
  {
    m_layerRef->SnapToVoxelCenter( ras, m_points[nIndex].pt );
  }
  else
  {
    m_points[nIndex].pt[0] = ras[0];
    m_points[nIndex].pt[1] = ras[1];
    m_points[nIndex].pt[2] = ras[2];
  }

  SetModified();

  if ( rebuildActor )
    RebuildActors();
}


void LayerWayPoints::DoListenToMessage( std::string const iMessage, void* iData, void* sender )
{
  if ( iMessage == "ColorMapChanged" )
  {
    this->UpdateColorMap();
    this->SendBroadcast( "LayerActorUpdated", this );
  }
  else if ( iMessage == "OpacityChanged" )
  {
    this->UpdateOpacity();
    this->SendBroadcast( "LayerActorUpdated", this );
  }
  else if ( iMessage == "RadiusChanged" )
  {
    this->RebuildActors();
    this->SendBroadcast( "LayerActorUpdated", this );
  }
  else if ( iMessage == "ScalarLayerChanged" )
  {
    this->RebuildActors();
    this->SendBroadcast( "LayerActorUpdated", this );
  }
  else if ( iMessage == "ScalarSetChanged" )
  {
    this->RebuildActors();
    this->SendBroadcast( "LayerActorUpdated", this );
  }
  else if ( iMessage == "SplineVisibilityChanged" )
  {
    this->SetVisible( IsVisible() );
    this->RebuildActors();
    this->SendBroadcast( "LayerActorUpdated", this );
  }
  else if ( iMessage == "SnapToVoxelCenterChanged" )
  {
    for ( size_t i = 0; i < m_points.size(); i++ )
      UpdatePoint( m_points[i].pt, i, false );
    this->RebuildActors();
    this->SendBroadcast( "LayerActorUpdated", this );
  }
  
  LayerEditable::DoListenToMessage( iMessage, iData, sender );
}

void LayerWayPoints::UpdateOpacity()
{
  for ( int i = 0; i < 3; i++ )
  {
    m_actorSlice[i]->GetProperty()->SetOpacity( GetProperties()->GetOpacity() );
  }
}

void LayerWayPoints::UpdateColorMap()
{
  for ( int i = 0; i < 3; i++ )
    m_actorSlice[i]->GetProperty()->SetColor( GetProperties()->GetColor() );

  m_actorBalls->GetProperty()->SetColor( GetProperties()->GetColor() );

  switch ( GetProperties()->GetColorMap() )
  {
  case LayerPropertiesWayPoints::SolidColor:
    m_mapper->ScalarVisibilityOff();
    m_actorSpline->GetProperty()->SetColor( GetProperties()->GetSplineColor() );
    break;
  case LayerPropertiesWayPoints::HeatScale:
    m_mapper->ScalarVisibilityOn();
    m_mapper->SetLookupTable( GetProperties()->GetHeatScaleLUT() );
    break;
  }
}

bool LayerWayPoints::Rotate( std::vector<RotationElement>& rotations, wxWindow* wnd, wxCommandEvent& event )
{
  m_points.clear();
  m_waypointsSource->LabelToWayPoints( m_points, m_layerRef->GetSourceVolume() );
  RebuildActors();

  return true;
}
