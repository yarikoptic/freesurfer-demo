/**
 * @file  SurfaceRegion.cpp
 * @brief Surface region from a surface selection in 3D view.
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: krish $
 *    $Date: 2010/12/08 23:41:46 $
 *    $Revision: 1.7.2.4 $
 *
 * Copyright (C) 2008-2009,
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

#include "SurfaceRegion.h"
#include "vtkRenderer.h"
#include "vtkActor2D.h"
#include "vtkProperty.h"
#include "vtkPolyDataMapper.h"
#include "vtkPolyData.h"
#include "vtkPolyDataWriter.h"
#include "vtkXMLPolyDataWriter.h"
#include "MainWindow.h"
#include "RenderView3D.h"
#include "vtkPolyData.h"
#include "vtkPoints.h"
#include "vtkCellArray.h"
#include "vtkPoints.h"
#include "vtkSelectPolyData.h"
#include "vtkProperty.h"
#include "vtkClipPolyData.h"
#include "vtkBox.h"
#include "vtkMath.h"
#include "MyUtils.h"
#include "LayerMRI.h"
#include "LayerPropertiesMRI.h"
#include "vtkCleanPolyData.h"
#include "vtkAppendPolyData.h"
#include "SurfaceRegionGroups.h"
#include <wx/ffile.h>

SurfaceRegion::SurfaceRegion( LayerMRI* owner ) : 
    Broadcaster( "SurfaceRegion" ),
    Listener( "SurfaceRegion" )
{
  m_actorMesh = vtkSmartPointer<vtkActor>::New();
  m_actorMesh->GetProperty()->SetColor( 0, 1, 0 );
  m_actorMesh->GetProperty()->SetRepresentationToWireframe();
  m_actorMesh->GetProperty()->SetLineWidth( 2 );

  m_actorOutline = vtkSmartPointer<vtkActor>::New();
  m_actorOutline->GetProperty()->SetColor( 0, 1, 0 );
  m_actorOutline->GetProperty()->SetLineWidth( 3 );
  
  m_points = vtkSmartPointer<vtkPoints>::New();
  m_selector = vtkSmartPointer<vtkSelectPolyData>::New();
  m_selector->SetSelectionModeToSmallestRegion();
  
  // use a clipper to pre-clip the big surface for faster selecting
  m_clipbox = vtkSmartPointer<vtkBox>::New();
  m_clipperPre = vtkSmartPointer<vtkClipPolyData>::New();
  m_clipperPre->SetClipFunction( m_clipbox );
//  m_clipper->GenerateClippedOutputOn();
  m_clipperPre->InsideOutOn();
  m_selector->SetInput( m_clipperPre->GetOutput() );
  m_cleanerPost = vtkSmartPointer<vtkCleanPolyData>::New();
  m_cleanerPost->SetInputConnection( m_selector->GetOutputPort() );
  m_mri = owner;
  m_nGroup = 1;
  m_color = wxColour( "blue" );
}

SurfaceRegion::~SurfaceRegion()
{}

vtkActor* SurfaceRegion::GetMeshActor()
{
  return m_actorMesh;
}

void SurfaceRegion::ResetOutline()
{
  if ( !m_polydataHolder.GetPointer() )
    m_polydataHolder = vtkSmartPointer<vtkPolyData>::New();
  m_polydataHolder->DeepCopy( m_actorMesh->GetMapper()->GetInput() );
  m_actorOutline->VisibilityOn();
  m_points->Reset();
}

void SurfaceRegion::RebuildOutline( bool bClose )
{
  vtkSmartPointer<vtkPolyData> polydata = vtkSmartPointer<vtkPolyData>::New();
  vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
  if ( bClose && m_points->GetNumberOfPoints() > 0 )
    m_points->InsertNextPoint( m_points->GetPoint( 0 ) );
  lines->InsertNextCell( m_points->GetNumberOfPoints() );
  for ( int i = 0; i < m_points->GetNumberOfPoints(); i++ )
    lines->InsertCellPoint( i );
  polydata->SetPoints( m_points );
  polydata->SetLines( lines );
  vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  mapper->SetInput( polydata );
  m_actorOutline->SetMapper( mapper );
  m_actorOutline->SetVisibility( bClose ? 0 : 1 );
}

void SurfaceRegion::SetInput( vtkPolyData* polydata )
{
  m_clipperPre->SetInput( polydata );
  m_clipbox->SetBounds( polydata->GetBounds() );
}

void SurfaceRegion::AddPoint( double* pt )
{
  m_points->InsertNextPoint( pt );
  RebuildOutline( false );
}

bool SurfaceRegion::Close()
{
  RebuildOutline( true );
  if ( m_points->GetNumberOfPoints() > 3 )
  {
    double bounds[6], cpt[3], len = 0;
    m_points->GetBounds( bounds );
    for ( int i = 0; i < 3; i++ )
    {
      cpt[i] = (bounds[i*2+1] + bounds[i*2]) / 2.0;
      len += (bounds[i*2+1] - bounds[i*2]);
    }
    for ( int i = 0; i < 3; i++ )
    {
      bounds[i*2] = cpt[i] - len/2.0;
      bounds[i*2+1] = cpt[i] + len/2.0;
    }
    m_points->Modified();
    m_clipbox->SetBounds( bounds );
    m_selector->SetLoop( m_points );
    m_cleanerPost->Update();
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->ScalarVisibilityOff();
    m_actorMesh->SetMapper( mapper );
    if ( m_polydataHolder.GetPointer() )
    {
      vtkSmartPointer<vtkAppendPolyData> append = vtkSmartPointer<vtkAppendPolyData>::New();
      append->AddInput( m_polydataHolder );
      append->AddInput( m_cleanerPost->GetOutput() );
      vtkSmartPointer<vtkCleanPolyData> cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
      cleaner->SetInputConnection( append->GetOutputPort() );
      cleaner->PointMergingOn();
      cleaner->ToleranceIsAbsoluteOn();
      cleaner->SetAbsoluteTolerance( 0.0001 );
      cleaner->Update();
      
      // merge duplicate cells
      vtkPolyData* polydata = cleaner->GetOutput();
      std::vector<vtkIdType> idList;
      vtkCellArray* polys = polydata->GetPolys();
      vtkSmartPointer<vtkCellArray> new_polys = vtkSmartPointer<vtkCellArray>::New();
      polys->InitTraversal();
      vtkIdType npts;
      vtkIdType* pts;
      while ( polys->GetNextCell( npts, pts ) )
      {
        bool bFound = false;
        for ( size_t i = 0; i < idList.size() && !bFound; i+=3 )
        {
          if ( pts[0] == idList[i] && pts[1] == idList[i+1] && pts[2] == idList[i+2] )
            bFound = true;
        }
        if ( !bFound )
        {
          new_polys->InsertNextCell( npts, pts );
          idList.push_back( pts[0] );
          idList.push_back( pts[1] );
          idList.push_back( pts[2] );
        }
      }
      polydata->SetPolys( new_polys );
          
      mapper->SetInput( polydata );
      return true;
    }
    else
    {
      mapper->SetInput( m_cleanerPost->GetOutput() );
      vtkPolyData* polydata = m_selector->GetOutput();
      return ( polydata && polydata->GetPoints() && polydata->GetPoints()->GetNumberOfPoints() > 0 );
    }
  }
  else
    return false;
}

void SurfaceRegion::Update()
{}

void SurfaceRegion::AppendProps( vtkRenderer* renderer )
{
  renderer->AddViewProp( m_actorMesh );
  renderer->AddViewProp( m_actorOutline );
}

void SurfaceRegion::SetColor( const wxColour& color )
{
  m_color = color;
  m_actorMesh->GetProperty()->SetColor( color.Red()/255.0, color.Green()/255.0, color.Blue()/255.0 );
  SendBroadcast( "SurfaceRegionColorChanged", this, this );
}

wxColour SurfaceRegion::GetColor()
{
  return m_color;
}

void SurfaceRegion::Show( bool bShow )
{
  m_actorMesh->SetVisibility( bShow?1:0 );
}

bool SurfaceRegion::HasPoint( double* pos )
{
  double delta[3] = { 0, 0, 0 };
  return vtkMath::PointIsWithinBounds( pos, m_actorMesh->GetBounds(), delta );
}
  
void SurfaceRegion::Highlight( bool bHighlight )
{
  if ( bHighlight )
    m_actorMesh->GetProperty()->SetColor( 0, 1, 0 );
  else
    m_actorMesh->GetProperty()->SetColor( m_color.Red()/255.0, m_color.Green()/255.0, m_color.Blue()/255.0 );
}

bool SurfaceRegion::Write( wxString& fn )
{
  FILE* fp = fopen( fn.c_str(), "w" );
  if ( !fp )
    return false;
  
  bool ret = WriteHeader( fp, m_mri ) && WriteBody( fp );
  fclose( fp );
  
  return ret;
}

bool SurfaceRegion::WriteHeader( FILE* fp, LayerMRI* mri_ref, int nNum )
{
  wxString strg = _("VOLUME_PATH ");
  strg << mri_ref->GetFileName() << _("\n")
      << _("VOLUME_THRESHOLD ") << mri_ref->GetProperties()->GetContourMinThreshold() << _(" ") 
      << mri_ref->GetProperties()->GetContourMaxThreshold() << _("\n") 
      << _( "SURFACE_REGIONS " ) << nNum << _("\n");
  wxFFile file( fp );
  file.SeekEnd();
  bool ret = file.Write( strg );
  file.Flush();
  file.Detach();
  return ret;
}

bool SurfaceRegion::WriteBody( FILE* fp )
{
  // clean the polydata before writing
  vtkSmartPointer<vtkCleanPolyData> cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
  cleaner->SetInput( m_actorMesh->GetMapper()->GetInput() );
  cleaner->Update();
  vtkPolyData* polydata = cleaner->GetOutput();
  vtkPoints* points = polydata->GetPoints();
  vtkCellArray* polys = polydata->GetPolys();
  wxString strg = _("SURFACE_REGION\n");
  strg << _( "ID " ) << m_nId << _("\n")
      << _( "GROUP_ID " ) << m_nGroup << _("\n")
      << _( "POINTS " ) << points->GetNumberOfPoints() << _("\n");
  double pt[3];
  for ( vtkIdType i = 0; i < points->GetNumberOfPoints(); i++ )
  {
    points->GetPoint( i, pt );
    m_mri->TargetToRAS( pt, pt );
    strg << pt[0] << " " << pt[1] << " " << pt[2] << _("\n");
  }
  strg << _( "POLYGONS " ) << polys->GetNumberOfCells() << _("\n");
  vtkIdType nPts;
  vtkIdType* pts = NULL;
  polys->InitTraversal();
  while ( polys->GetNextCell( nPts, pts ) )
  {
    strg << nPts << " ";
    for ( vtkIdType j = 0; j < nPts; j++ )
      strg << pts[j] << " ";
    strg << _("\n");
  }
  wxFFile file( fp );
  file.SeekEnd();
  bool ret = file.Write( strg );
  file.Flush();
  file.Detach();
  
  return ret;
}

bool SurfaceRegion::Load( FILE* fp )
{
  char tmp_strg[1000];
  wxString id_strg = _("SURFACE_REGION");
  while ( fscanf( fp, "%s\n", tmp_strg ) != EOF && id_strg != tmp_strg );
  if ( id_strg != tmp_strg )
    return false;
  
  int nId, nGroup = 1, nPts = 0;
  float x, y, z;
  char ch[100] = {0};
  if ( fscanf( fp, "ID %d", &nId ) == EOF || fscanf( fp, "%s", ch ) == EOF ) 
    return false;
  
  wxString strg = ch;
  if ( strg == "GROUP_ID" )
    fscanf( fp, "%d\nPOINTS %d", &nGroup, &nPts );
  else
    fscanf( fp, "%d", &nPts );
  if ( nPts <= 0 )
    return false;
  if ( nGroup <= 0 )
    nGroup = 1;
  
  vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
  double pt[3];
  for ( int i = 0; i < nPts; i++ )
  {
    fscanf( fp, "%f %f %f", &x, &y, &z );
    pt[0] = x;
    pt[1] = y;
    pt[2] = z;
    m_mri->RASToTarget( pt, pt );
    points->InsertNextPoint( pt );
  }
  
  int nPolys = 0;
  if ( fscanf( fp, "\nPOLYGONS %d", &nPolys ) == EOF || nPolys == 0 )
    return false;
  
  vtkSmartPointer<vtkCellArray> polys = vtkSmartPointer<vtkCellArray>::New();
  vtkIdType n[3];
  int nInts[3], nIds = 0;
  for ( int i = 0; i < nPolys; i++ )
  {
    fscanf( fp, "%d %d %d %d", &nIds, nInts, nInts+1, nInts+2 );
    n[0] = nInts[0];
    n[1] = nInts[1];
    n[2] = nInts[2];
    polys->InsertNextCell( nIds, n );
  }
  
  // clean the polydata after loading
  vtkSmartPointer<vtkPolyData> polydata = vtkSmartPointer<vtkPolyData>::New();
  polydata->SetPoints( points );
  polydata->SetPolys( polys );
  vtkSmartPointer<vtkCleanPolyData> cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
  cleaner->SetInput( polydata );
  vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  mapper->SetInputConnection( cleaner->GetOutputPort() );
  mapper->ScalarVisibilityOff();
  m_actorMesh->SetMapper( mapper );
  
  SetGroup( nGroup );
  
  return true;
}

bool SurfaceRegion::DeleteCell( RenderView3D* view, int pos_x, int pos_y )
{
  int nIdPicked = view->PickCell( m_actorMesh, pos_x, pos_y );
  if ( nIdPicked >= 0 )
  {
    vtkPolyData* polydata = vtkPolyDataMapper::SafeDownCast( m_actorMesh->GetMapper() )->GetInput();
    vtkSmartPointer<vtkCellArray> new_polys = vtkSmartPointer<vtkCellArray>::New();
    vtkCellArray* polys = polydata->GetPolys();
    polys->InitTraversal();
    vtkIdType npts;
    vtkIdType* pts;
    int nId = 0;
    while ( polys->GetNextCell( npts, pts ) )
    {
      if ( nId != nIdPicked )
      {
        new_polys->InsertNextCell( npts, pts );
      }
      nId++;
    }
    polydata->SetPolys( new_polys );
    return true;
  }
  else
    return false;
}

void SurfaceRegion::SetGroup( int nGroup )
{
  m_nGroup = nGroup;
  SetColor( m_mri->GetSurfaceRegionGroups()->GetGroupColor( nGroup ) );
}

