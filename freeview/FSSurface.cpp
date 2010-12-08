/**
 * @file  FSSurface.h
 * @brief Base surface class that takes care of I/O and data conversion.
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: krish $
 *    $Date: 2010/12/08 23:41:44 $
 *    $Revision: 1.40.2.1 $
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

#include <wx/wx.h>
#include <wx/filename.h>
#include "FSSurface.h"
#include <stdexcept>
#include "vtkShortArray.h"
#include "vtkLongArray.h"
#include "vtkUnsignedCharArray.h"
#include "vtkFloatArray.h"
#include "vtkPointData.h"
#include "vtkCellData.h"
#include "vtkCellArray.h"
#include "vtkIntArray.h"
#include "vtkSmartPointer.h"
#include "vtkImageReslice.h"
#include "vtkMatrix4x4.h"
#include "vtkTransform.h"
#include "vtkImageChangeInformation.h"
#include "vtkPolyData.h"
#include "vtkTubeFilter.h"
#include "vtkMath.h"
#include "vtkLine.h"
#include "vtkPlane.h"
#include "vtkCutter.h"
#include "FSVolume.h"
#include "MyUtils.h"

using namespace std;

FSSurface::FSSurface( FSVolume* ref ) :
    m_MRIS( NULL ),
    m_MRISTarget( NULL ),
    m_bBoundsCacheDirty( true ),
    m_bCurvatureLoaded( false ),
    m_nActiveSurface( SurfaceMain ),
    m_volumeRef( ref ),
    m_nActiveVector( -1 )
{
  m_polydata = vtkSmartPointer<vtkPolyData>::New();
  m_polydataVector = vtkSmartPointer<vtkPolyData>::New();
  m_polydataVertices = vtkSmartPointer<vtkPolyData>::New();
  m_polydataWireframes = vtkSmartPointer<vtkPolyData>::New();
  m_polydataTarget = vtkSmartPointer<vtkPolyData>::New();

  for ( int i = 0; i < 3; i++ )
    m_polydataVector2D[i] = vtkSmartPointer<vtkPolyData>::New();
  
  for ( int i = 0; i < NUM_OF_VSETS; i++ )
  {
    m_fVertexSets[i] = NULL;
    m_fNormalSets[i] = NULL;
    m_bSurfaceLoaded[i] = false;
    m_HashTable[i] = NULL;
  }
}

FSSurface::~FSSurface()
{
  if ( m_MRIS )
    ::MRISfree( &m_MRIS );

  if ( m_MRISTarget )
    ::MRISfree( &m_MRISTarget );

  for ( int i = 0; i < NUM_OF_VSETS; i++ )
  {
    if ( m_fNormalSets[i] )
      delete[] m_fNormalSets[i];

    if ( m_fVertexSets[i] )
      delete[] m_fVertexSets[i];

    if ( m_HashTable[i] )
      MHTfree( &m_HashTable[i] );
  }

  for ( size_t i = 0; i <  m_vertexVectors.size(); i++ )
  {
    delete[] m_vertexVectors[i].data;
  }
  m_vertexVectors.clear();
}

bool FSSurface::MRISRead( const char* filename, wxWindow* wnd, wxCommandEvent& event, 
                          const char* vector_filename,
                          const char* patch_filename,
                          const char* target_filename )
{
  if ( m_MRIS )
    ::MRISfree( &m_MRIS );

  event.SetInt( event.GetInt() + 1 );
  wxPostEvent( wnd, event );

  char* fn = strdup( filename );
  m_MRIS = ::MRISread( fn );
  free( fn );

  if ( m_MRIS == NULL )
  {
    cerr << "MRISread failed" << endl;
    return false;
  }
  
  if ( patch_filename )
  {
    if ( ::MRISreadPatch( m_MRIS, patch_filename ) != 0 )
    {
      cerr << "Can not load patch file " << patch_filename << endl;
    }
  }
  // Get some info from the MRIS. This can either come from the volume
  // geometry data embedded in the surface; this is done for newer
  // surfaces. Or it can come from the source information in the
  // transform. We use it to get the RAS center offset for the
  // surface->RAS transform.
  
  
  m_SurfaceToRASMatrix[0] = 1;
  m_SurfaceToRASMatrix[1] = 0;
  m_SurfaceToRASMatrix[2] = 0;
  m_SurfaceToRASMatrix[3] = 0;
  m_SurfaceToRASMatrix[4] = 0;
  m_SurfaceToRASMatrix[5] = 1;
  m_SurfaceToRASMatrix[6] = 0;
  m_SurfaceToRASMatrix[7] = 0;
  m_SurfaceToRASMatrix[8] = 0;
  m_SurfaceToRASMatrix[9] = 0;
  m_SurfaceToRASMatrix[10] = 1;
  m_SurfaceToRASMatrix[11] = 0;
  m_SurfaceToRASMatrix[12] = 0;
  m_SurfaceToRASMatrix[13] = 0;
  m_SurfaceToRASMatrix[14] = 0;
  m_SurfaceToRASMatrix[15] = 1;
  
  /*
  if ( m_MRIS->vg.valid )
  {
    m_SurfaceToRASMatrix[3] = m_MRIS->vg.c_r;
    m_SurfaceToRASMatrix[7] = m_MRIS->vg.c_a;
    m_SurfaceToRASMatrix[11] = m_MRIS->vg.c_s;
  }
  else if ( m_MRIS->lta )
  {
    m_SurfaceToRASMatrix[3] = -m_MRIS->lta->xforms[0].src.c_r;
    m_SurfaceToRASMatrix[7] = -m_MRIS->lta->xforms[0].src.c_a;
    m_SurfaceToRASMatrix[11] = -m_MRIS->lta->xforms[0].src.c_s;
  }
  */
  m_bHasVolumeGeometry = false;
  if ( m_MRIS->vg.valid )
  {
    MRI* tmp = MRIallocHeader(m_MRIS->vg.width, m_MRIS->vg.height, m_MRIS->vg.depth, MRI_UCHAR);
    useVolGeomToMRI(&m_MRIS->vg, tmp);
    MATRIX* vox2rasScanner = MRIxfmCRS2XYZ(tmp, 0);
    MATRIX* vo2rasTkReg = MRIxfmCRS2XYZtkreg(tmp);
    MATRIX* vox2rasTkReg_inv = MatrixInverse( vo2rasTkReg, NULL );
    MATRIX* M = MatrixMultiply( vox2rasScanner, vox2rasTkReg_inv, NULL );
    for ( int i = 0; i < 16; i++ )
    {
      m_SurfaceToRASMatrix[i] = 
        (double) *MATRIX_RELT( M, (i/4)+1, (i%4)+1 );
    }
    MRIfree( &tmp );
    MatrixFree( &vox2rasScanner );
    MatrixFree( &vo2rasTkReg );
    MatrixFree( &vox2rasTkReg_inv );
    MatrixFree( &M );
    m_bHasVolumeGeometry = true;
  }  

  // Make our transform object and set the matrix.
  m_SurfaceToRASTransform = vtkSmartPointer<vtkTransform>::New();
  m_SurfaceToRASTransform->SetMatrix( m_SurfaceToRASMatrix );

  // Make the hash table. This makes it with v->x,y,z.
  if ( m_HashTable[0] )
    MHTfree( &m_HashTable[0] );
  m_HashTable[0] = MHTfillVertexTableRes( m_MRIS, NULL, CURRENT_VERTICES, 2.0 );

  UpdatePolyData();

  // Save main vertices and normals
  SaveVertices( m_MRIS, SurfaceMain );
  SaveNormals ( m_MRIS, SurfaceMain );
  m_bSurfaceLoaded[SurfaceMain] = true;

  if ( !patch_filename )
  {
    LoadSurface ( "white",    SurfaceWhite );
    LoadSurface ( "pial",     SurfacePial );
    LoadSurface ( "orig",     SurfaceOriginal );
    LoadSurface ( "inflated", SurfaceInflated );
  }
  
  RestoreVertices( m_MRIS, SurfaceMain );
  RestoreNormals( m_MRIS, SurfaceMain );

  if ( target_filename != NULL )
    LoadTargetSurface( target_filename, wnd, event );
  
  if ( vector_filename != NULL )
    LoadVectors ( vector_filename );

  LoadCurvature();
// cout << "MRISread finished" << endl;

  return true;
}

void FSSurface::LoadTargetSurface( const char* filename, wxWindow* wnd, wxCommandEvent& event )
{
  event.SetInt( event.GetInt() + 1 );
  wxPostEvent( wnd, event );

  if ( m_MRISTarget )
    ::MRISfree( &m_MRISTarget );
  
  char* fn = strdup( filename );
  m_MRISTarget = ::MRISread( fn );
  free( fn );

  if ( m_MRISTarget == NULL )
  {
    cerr << "MRISread failed. Can not load target surface." << endl;
    return;
  }
  
  UpdatePolyData( m_MRISTarget, m_polydataTarget );
}

bool FSSurface::MRISWrite( const char* filename, wxWindow* wnd, wxCommandEvent& event )
{
  if ( m_MRIS == NULL )
  {
    cerr << "No MRIS to write." << endl;
    return false;
  }  
   
  return ( ::MRISwrite( m_MRIS, filename ) == 0 );
}

bool FSSurface::MRISReadVectors( const char* filename, wxWindow* wnd, wxCommandEvent& event )
{
  event.SetInt( event.GetInt() + 1 );
  wxPostEvent( wnd, event );

  return LoadVectors( filename );
}

bool FSSurface::LoadSurface( const char* filename, int nSet )
{
  if ( ::MRISreadVertexPositions( m_MRIS, (char*)filename ) != 0 )
  {
    cerr << "could not load surface from " << filename << endl;
    m_bSurfaceLoaded[nSet] = false;
    return false;
  }
  else
  {
    if ( m_HashTable[nSet] )
      MHTfree( &m_HashTable[nSet] );
    m_HashTable[nSet] = MHTfillVertexTableRes( m_MRIS, NULL, CURRENT_VERTICES, 2.0 );
    ComputeNormals();
    SaveVertices( m_MRIS, nSet );
    SaveNormals ( m_MRIS, nSet );
    m_bSurfaceLoaded[nSet] = true;
    return true;
  }
}

bool FSSurface::LoadCurvature( const char* filename )
{
  if ( ::MRISreadCurvatureFile( m_MRIS, (char*)(filename ? filename : "curv" ) ) != 0 )
  {
    cerr << "could not read curvature from " << (filename ? filename : "curv") << endl;
    m_bCurvatureLoaded = false;
    return false;
  }
  else
  {
    int cVertices = m_MRIS->nvertices;

    vtkSmartPointer<vtkFloatArray> curvs = vtkSmartPointer<vtkFloatArray>::New();
    curvs->Allocate( cVertices );
    curvs->SetNumberOfComponents( 1 );
    curvs->SetName( "Curvature" );

   for ( int vno = 0; vno < cVertices; vno++ )
    {
      curvs->InsertNextValue( m_MRIS->vertices[vno].curv );
    }
    m_polydata->GetPointData()->SetScalars( curvs );
    m_polydataWireframes->GetPointData()->SetScalars( curvs );
    m_bCurvatureLoaded = true;

    return true;
  }
}

bool FSSurface::LoadOverlay( const char* filename )
{
  if ( ::MRISreadValues( m_MRIS, (char*)( filename ) ) != 0 )
  {
    cerr << "could not read overlay data from " << filename << endl;
    return false;
  }
  else
  {
    return true;
  }
}

/*
bool FSSurface::LoadOverlay( const char* filename )
{
  // user read curvature routine because read values does not handle filename properly
  int nCount = m_MRIS->nvertices;
  for ( int i = 0; i < nCount; i++ )
    m_MRIS->vertices[i].val = m_MRIS->vertices[i].curv;
  float fMin = m_MRIS->min_curv;
  float fMax = m_MRIS->max_curv;
  
  if ( ::MRISreadCurvatureFile( m_MRIS, (char*)( filename ) ) != 0 )
  {
    cerr << "could not read overlay data from " << filename << endl;
    return false;
  }
  else
  {
    // move curv to val and restore real curv
    float ftemp;
    for ( int i = 0; i < nCount; i++ )
    {
      ftemp = m_MRIS->vertices[i].curv;
      m_MRIS->vertices[i].curv = m_MRIS->vertices[i].val;
      m_MRIS->vertices[i].val = ftemp;
    }
    m_MRIS->min_curv = fMin;
    m_MRIS->max_curv = fMax;
    
    return true;
  }
}
*/

bool FSSurface::LoadVectors( const char* filename )
{
  char* ch = strdup( filename );
  MRI* mri = ::MRIread( ch );  
  free( ch );
    
  VertexVectorItem vector;
  std::string fn = filename;
  vector.name = fn.substr( fn.find_last_of("/\\")+1);
  if ( mri )
  {
    if ( mri->type != MRI_FLOAT )
    {
      cerr << "Vector volume must be in type of MRI_FLOAT." << endl;
    }
    else if ( SaveVertices( mri, vector.data ) )
    {
      m_vertexVectors.push_back( vector );
      m_nActiveVector = m_vertexVectors.size() - 1;
      UpdateVectors();
      ::MRIfree( &mri );
      cout << "vector data loaded for surface from " << filename << endl;
      return true;
    }
  }
  else if ( ::MRISreadVertexPositions( m_MRIS, (char*)filename ) == 0 )
  {
    // compute vectors
    if ( ComputeVectors( m_MRIS, vector.data ) )
    {
      m_vertexVectors.push_back( vector );
      m_nActiveVector = m_vertexVectors.size() - 1;
      
      // restore original vertices in m_MRIS because MRISreadVertexPositions changed it!
      RestoreVertices( m_MRIS, m_nActiveSurface );
      
      UpdateVectors();

      cout << "vector data loaded for surface from " << filename << endl;
      return true;
    }
  }

  if ( mri )
    ::MRIfree( &mri );
  return false;
}

bool FSSurface::ComputeVectors( MRIS* mris, VertexItem*& buffer )
{
  int nvertices = mris->nvertices;
  VERTEX *v;
  for ( int vno = 0; vno < nvertices; vno++ )
  {
    v = &mris->vertices[vno];
    v->x -= m_fVertexSets[m_nActiveSurface][vno].x;
    v->y -= m_fVertexSets[m_nActiveSurface][vno].y;
    v->z -= m_fVertexSets[m_nActiveSurface][vno].z;
  }
  return SaveVertices( mris, buffer );
}

void FSSurface::SaveVertices( MRIS* mris, int nSet )
{
  if ( !mris || nSet >= NUM_OF_VSETS )
    return;

  SaveVertices( mris, m_fVertexSets[nSet] );
}

bool FSSurface::SaveVertices( MRIS* mris, VertexItem*& buffer )
{
  int nvertices = mris->nvertices;
  VERTEX *v;

  if ( buffer == NULL )
  {
    buffer = new VertexItem[nvertices];
    if ( !buffer )
    {
      cerr << "Can not allocate memory for vertex sets." << endl;
      return false;
    }
  }
  for ( int vno = 0; vno < nvertices; vno++ )
  {
    v = &mris->vertices[vno];
    buffer[vno].x = v->x;
    buffer[vno].y = v->y;
    buffer[vno].z = v->z;
  }
  return true;
}


bool FSSurface::SaveVertices( MRI* mri, VertexItem*& buffer )
{
  int nvertices = mri->width;

  if ( buffer == NULL )
  {
    buffer = new VertexItem[nvertices];
    if ( !buffer )
    {
      cerr << "Can not allocate memory for vertex sets." << endl;
      return false;
    }
  }
  for ( int vno = 0; vno < nvertices; vno++ )
  {
    buffer[vno].x = MRIFseq_vox( mri, vno, 0, 0, 0 );
    buffer[vno].y = MRIFseq_vox( mri, vno, 0, 0, 1 );
    buffer[vno].z = MRIFseq_vox( mri, vno, 0, 0, 2 );
  }
  return true;
}

void FSSurface::RestoreVertices( MRIS* mris, int nSet )
{
  if ( !mris || nSet >= NUM_OF_VSETS || m_fVertexSets[nSet] == NULL)
    return;

  int nvertices = mris->nvertices;
  VERTEX *v;

  for ( int vno = 0; vno < nvertices; vno++ )
  {
    v = &mris->vertices[vno];
    v->x = m_fVertexSets[nSet][vno].x;
    v->y = m_fVertexSets[nSet][vno].y;
    v->z = m_fVertexSets[nSet][vno].z;
  }
}

void FSSurface::SaveNormals( MRIS* mris, int nSet )
{
  if ( !mris || nSet >= NUM_OF_VSETS )
    return;

  int nvertices = mris->nvertices;
  VERTEX *v;

  if ( m_fNormalSets[nSet] == NULL )
  {
    m_fNormalSets[nSet] = new VertexItem[nvertices];
    if ( !m_fNormalSets[nSet] )
    {
      cerr << "Can not allocate memory for normal sets." << endl;
      return;
    }
  }
  for ( int vno = 0; vno < nvertices; vno++ )
  {
    v = &mris->vertices[vno];
    m_fNormalSets[nSet][vno].x = v->nx;
    m_fNormalSets[nSet][vno].y = v->ny;
    m_fNormalSets[nSet][vno].z = v->nz;
  }
}

void FSSurface::RestoreNormals( MRIS* mris, int nSet )
{
  if ( !mris || nSet >= NUM_OF_VSETS || m_fNormalSets[nSet] == NULL)
    return;

  int nvertices = mris->nvertices;
  VERTEX *v;

  for ( int vno = 0; vno < nvertices; vno++ )
  {
    v = &mris->vertices[vno];
    v->nx = m_fNormalSets[nSet][vno].x;
    v->ny = m_fNormalSets[nSet][vno].y;
    v->nz = m_fNormalSets[nSet][vno].z;
  }
}

void FSSurface::GetNormalAtVertex( int nVertex, double* vec_out )
{
  vec_out[0] = m_fNormalSets[m_nActiveSurface][nVertex].x;
  vec_out[1] = m_fNormalSets[m_nActiveSurface][nVertex].y;
  vec_out[2] = m_fNormalSets[m_nActiveSurface][nVertex].z;
}

void FSSurface::UpdatePolyData()
{
  UpdatePolyData( m_MRIS, m_polydata, m_polydataVertices, m_polydataWireframes );
}

void FSSurface::UpdatePolyData( MRIS* mris, 
                                vtkPolyData* polydata, 
                                vtkPolyData* polydata_verts, 
                                vtkPolyData* polydata_wireframe )
{
  // Allocate all our arrays.
  int cVertices = mris->nvertices;
  int cFaces = mris->nfaces;

  vtkSmartPointer<vtkPoints> newPoints =
      vtkSmartPointer<vtkPoints>::New();
  newPoints->Allocate( cVertices );

  vtkSmartPointer<vtkCellArray> newPolys =
      vtkSmartPointer<vtkCellArray>::New();
  newPolys->Allocate( newPolys->EstimateSize(cFaces,VERTICES_PER_FACE) );

  vtkSmartPointer<vtkFloatArray> newNormals = 
      vtkSmartPointer<vtkFloatArray>::New();
  newNormals->Allocate( cVertices );
  newNormals->SetNumberOfComponents( 3 );
  newNormals->SetName( "Normals" );;
  
  vtkSmartPointer<vtkCellArray> verts;
  if ( polydata_verts )
  {
    verts = vtkSmartPointer<vtkCellArray>::New();
    verts->Allocate( cVertices );
  }

  // Go through the surface and copy the vertex and normal for each
  // vertex. We need to transform them from surface RAS into standard
  // RAS.
  float point[3], normal[3], surfaceRAS[3];
  for ( int vno = 0; vno < cVertices; vno++ )
  {
    surfaceRAS[0] = mris->vertices[vno].x;
    surfaceRAS[1] = mris->vertices[vno].y;
    surfaceRAS[2] = mris->vertices[vno].z;
    this->ConvertSurfaceToRAS( surfaceRAS, point );
    if ( m_volumeRef )
      m_volumeRef->RASToTarget( point, point );
    newPoints->InsertNextPoint( point );

    normal[0] = mris->vertices[vno].nx;
    normal[1] = mris->vertices[vno].ny;
    normal[2] = mris->vertices[vno].nz;
    newNormals->InsertNextTuple( normal );
    
    if ( polydata_verts )
    {  
      vtkIdType n = vno;
      verts->InsertNextCell( 1, &n );
    }
  }

  // Go through and add the face indices. 
  vtkSmartPointer<vtkCellArray> lines;
  if ( polydata_wireframe )
  {
    lines = vtkSmartPointer<vtkCellArray>::New();
    lines->Allocate( cFaces * 6 );
  }
  vtkIdType face[VERTICES_PER_FACE];
  for ( int fno = 0; fno < cFaces; fno++ )
  {
    if ( mris->faces[fno].ripflag == 0 )
    {
      face[0] = mris->faces[fno].v[0];
      face[1] = mris->faces[fno].v[1];
      face[2] = mris->faces[fno].v[2];
      newPolys->InsertNextCell( 3, face );
      if ( polydata_wireframe )
      {
        lines->InsertNextCell( 2, face );
        lines->InsertNextCell( 2, face+1 );
        vtkIdType t[2] = { face[0], face[2] };
        lines->InsertNextCell( 2, t );
      }
    }  
  }

  polydata->SetPoints( newPoints );
  polydata->GetPointData()->SetNormals( newNormals );
  newPolys->Squeeze(); // since we've estimated size; reclaim some space
  polydata->SetPolys( newPolys ); 
  if ( polydata_verts )
  {
    polydata_verts->SetPoints( newPoints );
    polydata_verts->SetVerts( verts );
  }
  if ( polydata_wireframe )
  {
    polydata_wireframe->SetPoints( newPoints );
    polydata_wireframe->SetLines( lines ); 
  }
}

void FSSurface::UpdateVerticesAndNormals()
{
  // Allocate all our arrays.
  int cVertices = m_MRIS->nvertices;

  vtkSmartPointer<vtkPoints> newPoints =
    vtkSmartPointer<vtkPoints>::New();
  newPoints->Allocate( cVertices );

  vtkSmartPointer<vtkFloatArray> newNormals =
    vtkSmartPointer<vtkFloatArray>::New();
  newNormals->Allocate( cVertices );
  newNormals->SetNumberOfComponents( 3 );
  newNormals->SetName( "Normals" );

  // Go through the surface and copy the vertex and normal for each
  // vertex. We have to transform them from surface RAS into normal
  // RAS.
  float point[3], normal[3], surfaceRAS[3];
  for ( int vno = 0; vno < cVertices; vno++ )
  {
    surfaceRAS[0] = m_MRIS->vertices[vno].x;
    surfaceRAS[1] = m_MRIS->vertices[vno].y;
    surfaceRAS[2] = m_MRIS->vertices[vno].z;
    this->ConvertSurfaceToRAS( surfaceRAS, point );
    if ( m_volumeRef )
      m_volumeRef->RASToTarget( point, point );
    newPoints->InsertNextPoint( point );
    
    normal[0] = m_MRIS->vertices[vno].nx;
    normal[1] = m_MRIS->vertices[vno].ny;
    normal[2] = m_MRIS->vertices[vno].nz;
    newNormals->InsertNextTuple( normal );
  }

  m_polydata->SetPoints( newPoints );
  m_polydata->GetPointData()->SetNormals( newNormals );
  m_polydataVertices->SetPoints( newPoints );
  m_polydataWireframes->SetPoints( newPoints );
  m_polydata->Update();

  // if vector data exist
  UpdateVectors();
}

void FSSurface::UpdateVectors()
{
  if ( HasVectorSet() && m_nActiveVector >= 0 )
  {
    VertexItem* vectors = m_vertexVectors[m_nActiveVector].data;
    int cVertices = m_MRIS->nvertices;
    vtkPoints* oldPoints = m_polydata->GetPoints();
    float point[3], surfaceRAS[3];
    vtkIdType n = 0;
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
    vtkSmartPointer<vtkCellArray> verts = vtkSmartPointer<vtkCellArray>::New();
    for ( int vno = 0; vno < cVertices; vno++ )
    {
      if ( vectors[vno].x != 0 || vectors[vno].y != 0 || vectors[vno].z != 0 )
      {
        surfaceRAS[0] = m_fVertexSets[m_nActiveSurface][vno].x + vectors[vno].x;
        surfaceRAS[1] = m_fVertexSets[m_nActiveSurface][vno].y + vectors[vno].y;
        surfaceRAS[2] = m_fVertexSets[m_nActiveSurface][vno].z + vectors[vno].z;
        this->ConvertSurfaceToRAS( surfaceRAS, point );
        if ( m_volumeRef )
          m_volumeRef->RASToTarget( point, point );
  
        points->InsertNextPoint( oldPoints->GetPoint( vno ) );
        points->InsertNextPoint( point );
  
        verts->InsertNextCell( 1, &n );
  
        lines->InsertNextCell( 2 );
        lines->InsertCellPoint( n++ );
        lines->InsertCellPoint( n++ );
      }
    }
    m_polydataVector->SetPoints( points );
    m_polydataVector->SetLines( lines );
    m_polydataVector->SetVerts( verts );
  }
}

void FSSurface::UpdateVector2D( int nPlane, double slice_pos, vtkPolyData* contour_polydata )
{
  if ( HasVectorSet() && m_nActiveVector >= 0 )
  {
    VertexItem* vectors = m_vertexVectors[m_nActiveVector].data;
    int cVertices = m_MRIS->nvertices;
    int cFaces = m_MRIS->nfaces;
    
    // first figure out what vertices crossing the plane
    unsigned char* mask = new unsigned char[cVertices];
    memset( mask, 0, cVertices ); 
    vtkPoints* oldPoints = m_polydata->GetPoints();
    double pt_a[3], pt_b[3];
    for ( int fno = 0; fno < cFaces; fno++ )
    {
      if ( m_MRIS->faces[fno].ripflag == 0 )
      {
        int* np = m_MRIS->faces[fno].v;
        vtkIdType lines[3][2] = { {np[0], np[1]}, {np[1], np[2]}, {np[2], np[0]} };
        for ( int i = 0; i < 3; i++ )
        {
          oldPoints->GetPoint( lines[i][0], pt_a );
          oldPoints->GetPoint( lines[i][1], pt_b );
          if ( (pt_a[nPlane] >= slice_pos && pt_b[nPlane] <= slice_pos) ||
                (pt_a[nPlane] <= slice_pos && pt_b[nPlane] >= slice_pos) )
          {
            mask[lines[i][0]] = 1;
            mask[lines[i][1]] = 1;
          }
        }
      }  
    }
    
    // build vector actor
    vtkIdType n = 0;
    double point[3], surfaceRAS[3];
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
    vtkSmartPointer<vtkCellArray> verts = vtkSmartPointer<vtkCellArray>::New();
    double old_pt[3];
    vtkPoints* contour_pts = NULL;
    vtkCellArray* contour_lines = NULL;
    if ( contour_polydata )
    {
      contour_pts = contour_polydata->GetPoints();
      contour_lines = contour_polydata->GetLines();
    }
    
    vtkPolyData* target_polydata = NULL;
    vtkSmartPointer<vtkCutter> cutter = vtkSmartPointer<vtkCutter>::New();
    if ( m_polydataTarget->GetPoints() && m_polydataTarget->GetPoints()->GetNumberOfPoints() > 0 )
    {        
      vtkSmartPointer<vtkPlane> slicer = vtkSmartPointer<vtkPlane>::New();
      double pos[3] = { 0, 0, 0 };
      pos[nPlane] = slice_pos;
      slicer->SetOrigin( pos );
      slicer->SetNormal( (nPlane==0), (nPlane==1), (nPlane==2) );
      cutter->SetInput( m_polydataTarget );
      cutter->SetCutFunction( slicer );
      cutter->Update();
      target_polydata = cutter->GetOutput();
    }

    for ( int vno = 0; vno < cVertices; vno++ )
    {
      if ( mask[vno] )
      {
        oldPoints->GetPoint( vno, old_pt );
        surfaceRAS[0] = m_fVertexSets[m_nActiveSurface][vno].x + vectors[vno].x;
        surfaceRAS[1] = m_fVertexSets[m_nActiveSurface][vno].y + vectors[vno].y;
        surfaceRAS[2] = m_fVertexSets[m_nActiveSurface][vno].z + vectors[vno].z;
        this->ConvertSurfaceToRAS( surfaceRAS, point );
        if ( m_volumeRef )
          m_volumeRef->RASToTarget( point, point );
  
        if ( contour_pts )
        { 
          double new_pt[3];
          ProjectVectorPoint2D( old_pt, contour_pts, contour_lines, new_pt );
          
          for ( int i = 0; i < 3; i++ )
          {
            point[i] += (new_pt[i] - old_pt[i] );
          }
          points->InsertNextPoint( new_pt );
          
          if ( target_polydata && target_polydata->GetPoints() )
          {
            ProjectVectorPoint2D( point, target_polydata->GetPoints(), target_polydata->GetLines(), point );
          }          
          points->InsertNextPoint( point );
        }
        else
        {
          points->InsertNextPoint( old_pt );
          points->InsertNextPoint( point );
        }
  
        verts->InsertNextCell( 1, &n );
        lines->InsertNextCell( 2 );
        lines->InsertCellPoint( n++ );
        lines->InsertCellPoint( n++ );
      }
    }
    m_polydataVector2D[nPlane]->SetPoints( points );
    m_polydataVector2D[nPlane]->SetLines( lines );
    m_polydataVector2D[nPlane]->SetVerts( verts );
    delete[] mask;
  }  
}

bool FSSurface::ProjectVectorPoint2D( double* pt_in, 
                                      vtkPoints* contour_pts, 
                                      vtkCellArray* contour_lines, 
                                      double* pt_out )
{
  // first find the closest point on the contour
  double pt0[3];
  double dist2 = 1e10;
  int n0 = 0;
  double* old_pt = pt_in;
  
//  cout << contour_pts << " " << contour_lines << endl; fflush(0); 
//  cout << contour_pts->GetNumberOfPoints() << " " << 
//      contour_lines->GetNumberOfCells() << endl;
  for ( int i = 0; i < contour_pts->GetNumberOfPoints(); i++ )
  {
    contour_pts->GetPoint( i, pt0 );
    double d = vtkMath::Distance2BetweenPoints( old_pt, pt0 );
    if ( d < dist2 )
    {
      dist2 = d;
      n0 = i;
    }
  }

  // find the closest projection on the contour
  double cpt1[3], cpt2[3], t1, t2, p0[3], p1[3], p2[3];
  int n1 = -1, n2 = -1;
  contour_lines->InitTraversal();
  vtkIdType npts = 0, *cellpts;
  while ( contour_lines->GetNextCell( npts, cellpts ) )
  {
    if ( cellpts[0] == n0 )
    {
      if ( n1 < 0 )
        n1 = cellpts[1];
      else
        n2 = cellpts[1];
    }
    else if ( cellpts[1] == n0 )
    {
      if ( n1 < 0 )
        n1 = cellpts[0];
      else
        n2 = cellpts[0];
    }
    if ( n1 >= 0 && n2 >= 0 )
      break;
  }
  
  if ( n1 >= 0 && n2 >= 0 )
  {        
    contour_pts->GetPoint( n0, p0 );
    contour_pts->GetPoint( n1, p1 );
    contour_pts->GetPoint( n2, p2 );
    double d1 = vtkLine::DistanceToLine( old_pt, p0, p1, t1, cpt1 );
    double d2 = vtkLine::DistanceToLine( old_pt, p0, p2, t2, cpt2 );
    if ( d1 < d2 )
    {
      pt_out[0] = cpt1[0];
      pt_out[1] = cpt1[1];
      pt_out[2] = cpt1[2];
    }
    else
    {
      pt_out[0] = cpt2[0];
      pt_out[1] = cpt2[1];
      pt_out[2] = cpt2[2]; 
    }
    return true;
  }
  else
    return false;
}

void FSSurface::GetVectorAtVertex( int nVertex, double* vec_out, int nVector )
{
  int nv = nVector;
  if ( nv < 0 )
    nv = m_nActiveVector;
  
  VertexItem* vectors = m_vertexVectors[nv].data;
  vec_out[0] = vectors[nVertex].x;
  vec_out[1] = vectors[nVertex].y;
  vec_out[2] = vectors[nVertex].z;
}

bool FSSurface::SetActiveSurface( int nIndex )
{
  if ( nIndex == m_nActiveSurface )
    return false;

  m_nActiveSurface = nIndex;

  RestoreVertices( m_MRIS, nIndex );

  if ( m_fNormalSets[nIndex] == NULL )
  {
    ComputeNormals();
    SaveNormals( m_MRIS, nIndex );
  }
  else
  {
    RestoreNormals( m_MRIS, nIndex );
  }

  UpdateVerticesAndNormals();

  return true;
}

bool FSSurface::SetActiveVector( int nIndex )
{
  if ( nIndex == m_nActiveVector )
    return false;

  m_nActiveVector = nIndex;

  UpdateVectors();
  return true;
}

void FSSurface::NormalFace(int fac, int n, float *norm)
{
  int n0,n1;
  FACE *f;
  MRIS* mris = m_MRIS;
  float v0[3],v1[3];

  n0 = (n==0)                   ? VERTICES_PER_FACE-1 : n-1;
  n1 = (n==VERTICES_PER_FACE-1) ? 0                   : n+1;
  f = &mris->faces[fac];
  v0[0] = mris->vertices[f->v[n]].x - mris->vertices[f->v[n0]].x;
  v0[1] = mris->vertices[f->v[n]].y - mris->vertices[f->v[n0]].y;
  v0[2] = mris->vertices[f->v[n]].z - mris->vertices[f->v[n0]].z;
  v1[0] = mris->vertices[f->v[n1]].x - mris->vertices[f->v[n]].x;
  v1[1] = mris->vertices[f->v[n1]].y - mris->vertices[f->v[n]].y;
  v1[2] = mris->vertices[f->v[n1]].z - mris->vertices[f->v[n]].z;
  Normalize(v0);
  Normalize(v1);
  norm[0] = -v1[1]*v0[2] + v0[1]*v1[2];
  norm[1] = v1[0]*v0[2] - v0[0]*v1[2];
  norm[2] = -v1[0]*v0[1] + v0[0]*v1[1];
}

float FSSurface::TriangleArea(int fac, int n)
{
  int n0,n1;
  FACE *f;
  MRIS* mris = m_MRIS;
  float v0[3],v1[3],d1,d2,d3;

  n0 = (n==0)                   ? VERTICES_PER_FACE-1 : n-1;
  n1 = (n==VERTICES_PER_FACE-1) ? 0                   : n+1;
  f = &mris->faces[fac];
  v0[0] = mris->vertices[f->v[n]].x - mris->vertices[f->v[n0]].x;
  v0[1] = mris->vertices[f->v[n]].y - mris->vertices[f->v[n0]].y;
  v0[2] = mris->vertices[f->v[n]].z - mris->vertices[f->v[n0]].z;
  v1[0] = mris->vertices[f->v[n1]].x - mris->vertices[f->v[n]].x;
  v1[1] = mris->vertices[f->v[n1]].y - mris->vertices[f->v[n]].y;
  v1[2] = mris->vertices[f->v[n1]].z - mris->vertices[f->v[n]].z;
  d1 = -v1[1]*v0[2] + v0[1]*v1[2];
  d2 = v1[0]*v0[2] - v0[0]*v1[2];
  d3 = -v1[0]*v0[1] + v0[0]*v1[1];
  return sqrt(d1*d1+d2*d2+d3*d3)/2;
}

void FSSurface::Normalize( float v[3] )
{
  float d;

  d = sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
  if (d>0)
  {
    v[0] /= d;
    v[1] /= d;
    v[2] /= d;
  }
}

void FSSurface::ComputeNormals()
{
  if ( !m_MRIS )
    return;

  MRIS* mris = m_MRIS;
  int k,n;
  VERTEX *v;
  FACE *f;
  float norm[3],snorm[3];

  for (k=0;k<mris->nfaces;k++)
    if (mris->faces[k].ripflag)
    {
      f = &mris->faces[k];
      for (n=0;n<VERTICES_PER_FACE;n++)
        mris->vertices[f->v[n]].border = TRUE;
    }
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
    {
      v = &mris->vertices[k];
      snorm[0]=snorm[1]=snorm[2]=0;
      v->area = 0;
      for (n=0;n<v->num;n++)
        if (!mris->faces[v->f[n]].ripflag)
        {
          NormalFace(v->f[n],v->n[n],norm);
          snorm[0] += norm[0];
          snorm[1] += norm[1];
          snorm[2] += norm[2];
          v->area += TriangleArea(v->f[n],v->n[n]);
          /* Note: overest. area by 2! */
        }
      Normalize( snorm );

      if (v->origarea<0)
        v->origarea = v->area;

      v->nx = snorm[0];
      v->ny = snorm[1];
      v->nz = snorm[2];
    }
}

void FSSurface::ConvertSurfaceToRAS ( float iX, float iY, float iZ,
                                      float& oX, float& oY, float& oZ ) const
{
  float surface[3];
  float ras[3];

  surface[0] = iX;
  surface[1] = iY;
  surface[2] = iZ;

  this->ConvertSurfaceToRAS( surface, ras );

  oX = ras[0];
  oY = ras[1];
  oZ = ras[2];
}

void FSSurface::ConvertSurfaceToRAS ( double iX, double iY, double iZ,
                                      double& oX, double& oY, double& oZ ) const
{
  double surface[3];
  double ras[3];

  surface[0] = iX;
  surface[1] = iY;
  surface[2] = iZ;

  this->ConvertSurfaceToRAS( surface, ras );

  oX = ras[0];
  oY = ras[1];
  oZ = ras[2];
}

void FSSurface::ConvertRASToSurface ( float iX, float iY, float iZ,
                                      float& oX, float& oY, float& oZ ) const
{
  float ras[3];
  float surface[3];

  ras[0] = iX;
  ras[1] = iY;
  ras[2] = iZ;

  this->ConvertRASToSurface( ras, surface );

  oX = surface[0];
  oY = surface[1];
  oZ = surface[2];
}

void FSSurface::ConvertRASToSurface ( double iX, double iY, double iZ,
                                      double& oX, double& oY, double& oZ ) const
{
  double ras[3];
  double surface[3];

  ras[0] = iX;
  ras[1] = iY;
  ras[2] = iZ;

  this->ConvertRASToSurface( ras, surface );

  oX = surface[0];
  oY = surface[1];
  oZ = surface[2];
}

void FSSurface::ConvertSurfaceToRAS ( float const iSurf[3], float oRAS[3] ) const
{
  m_SurfaceToRASTransform->TransformPoint( iSurf, oRAS );
}

void FSSurface::ConvertSurfaceToRAS ( double const iSurf[3], double oRAS[3] ) const
{
  m_SurfaceToRASTransform->TransformPoint( iSurf, oRAS );
}

void FSSurface::ConvertRASToSurface ( float const iRAS[3], float oSurf[3] ) const
{
  m_SurfaceToRASTransform->GetInverse()->TransformPoint( iRAS, oSurf );
}

void FSSurface::ConvertRASToSurface ( double const iRAS[3], double oSurf[3] ) const
{
  m_SurfaceToRASTransform->GetInverse()->TransformPoint( iRAS, oSurf );
}

void FSSurface::GetBounds ( float oRASBounds[6] )
{
  if ( NULL == m_MRIS )
  {
    oRASBounds[0] = oRASBounds[1] = oRASBounds[2] =
                                      oRASBounds[3] = oRASBounds[4] = oRASBounds[5] = 0;
    return;
  }

  if ( m_bBoundsCacheDirty )
  {
    m_RASBounds[0] = m_RASBounds[2] = m_RASBounds[4] = 999999;
    m_RASBounds[1] = m_RASBounds[3] = m_RASBounds[5] = -999999;

    // Find the bounds.
    for ( int vno = 0; vno < m_MRIS->nvertices; vno++ )
    {

      // Translate to actual RAS coords.
      float rasX, rasY, rasZ;
      this->ConvertSurfaceToRAS( m_fVertexSets[SurfaceMain][vno].x,
                                 m_fVertexSets[SurfaceMain][vno].y,
                                 m_fVertexSets[SurfaceMain][vno].z,
                                 rasX, rasY, rasZ );

      if ( rasX < m_RASBounds[0] ) m_RASBounds[0] = rasX;
      if ( rasX > m_RASBounds[1] ) m_RASBounds[1] = rasX;
      if ( rasY < m_RASBounds[2] ) m_RASBounds[2] = rasY;
      if ( rasY > m_RASBounds[3] ) m_RASBounds[3] = rasY;
      if ( rasZ < m_RASBounds[4] ) m_RASBounds[4] = rasZ;
      if ( rasZ > m_RASBounds[5] ) m_RASBounds[5] = rasZ;

    }

    m_bBoundsCacheDirty = false;
  }

  oRASBounds[0] = m_RASBounds[0];
  oRASBounds[1] = m_RASBounds[1];
  oRASBounds[2] = m_RASBounds[2];
  oRASBounds[3] = m_RASBounds[3];
  oRASBounds[4] = m_RASBounds[4];
  oRASBounds[5] = m_RASBounds[5];
}


int FSSurface::GetNumberOfVertices () const
{
  if ( m_MRIS )
    return m_MRIS->nvertices;
  else
    return 0;
}


int FSSurface::FindVertexAtRAS ( float const iRAS[3], float* oDistance )
{
  float surf[3];
  this->ConvertRASToSurface( iRAS, surf );

  return this->FindVertexAtSurfaceRAS( surf, oDistance );
}

int FSSurface::FindVertexAtRAS ( double const iRAS[3], double* oDistance )
{
  double surf[3];
  this->ConvertRASToSurface( iRAS, surf );

  return this->FindVertexAtSurfaceRAS( surf, oDistance );
}

int FSSurface::FindVertexAtSurfaceRAS ( float const iSurfaceRAS[3],
                                        float* oDistance )
{
  VERTEX v;
  v.x = iSurfaceRAS[0];
  v.y = iSurfaceRAS[1];
  v.z = iSurfaceRAS[2];
  float distance;
  int nClosestVertex =
    MHTfindClosestVertexNo( m_HashTable[m_nActiveSurface], m_MRIS, &v, &distance );

  if ( -1 == nClosestVertex )
  {
    // cerr << "No vertices found." << endl;
    return -1;
  }

  if ( NULL != oDistance )
  {
    *oDistance = distance;
  }

  return nClosestVertex;
}

int FSSurface::FindVertexAtSurfaceRAS ( double const iSurfaceRAS[3],
                                        double* oDistance )
{
  VERTEX v;
  v.x = static_cast<float>(iSurfaceRAS[0]);
  v.y = static_cast<float>(iSurfaceRAS[1]);
  v.z = static_cast<float>(iSurfaceRAS[2]);
  float distance;
  int nClosestVertex = MHTfindClosestVertexNo( m_HashTable[m_nActiveSurface], m_MRIS, &v, &distance );
  if ( -1 == nClosestVertex )
  {
    // cerr << "No vertices found.";
  }

  if ( NULL != oDistance )
  {
    *oDistance = static_cast<double>(distance);
  }

  return nClosestVertex;
}

bool FSSurface::GetRASAtVertex ( int inVertex, float ioRAS[3] )
{
  float surfaceRAS[3];
  if ( this->GetSurfaceRASAtVertex( inVertex, surfaceRAS ) )
  {
    this->ConvertSurfaceToRAS( surfaceRAS, ioRAS );
    return true;
  }
  else
    return false;
}

bool FSSurface::GetRASAtVertex ( int inVertex, double ioRAS[3] )
{
  double surfaceRAS[3];
  if ( this->GetSurfaceRASAtVertex( inVertex, surfaceRAS ) )
  {
    this->ConvertSurfaceToRAS( surfaceRAS, ioRAS );
    return true;
  }
  else
    return false;
}

bool FSSurface::GetSurfaceRASAtVertex ( int inVertex, float ioRAS[3] )
{
  if ( m_MRIS == NULL )
//  throw runtime_error( "GetRASAtVertex: m_MRIS was NULL" );
    return false;

  if ( inVertex < 0 || inVertex >= m_MRIS->nvertices )
//  throw runtime_error( "GetRASAtVertex: inVertex was invalid" );
    return false;

  if ( m_nActiveSurface >= 0 && m_fVertexSets[m_nActiveSurface] != NULL )
  {
    ioRAS[0] = m_fVertexSets[m_nActiveSurface][inVertex].x;
    ioRAS[1] = m_fVertexSets[m_nActiveSurface][inVertex].y;
    ioRAS[2] = m_fVertexSets[m_nActiveSurface][inVertex].z;
  }
  else
  {
    ioRAS[0] = m_MRIS->vertices[inVertex].x;
    ioRAS[1] = m_MRIS->vertices[inVertex].y;
    ioRAS[2] = m_MRIS->vertices[inVertex].z;
  }

  return true;
}

bool FSSurface::GetSurfaceRASAtVertex ( int inVertex, double ioRAS[3] )
{
  if ( m_MRIS == NULL )
//  throw runtime_error( "GetRASAtVertex: m_MRIS was NULL" );
    return false;

  if ( inVertex < 0 || inVertex >= m_MRIS->nvertices )
//  throw runtime_error( "GetRASAtVertex: inVertex was invalid" );
    return false;

  if ( m_nActiveSurface >= 0 && m_fVertexSets[m_nActiveSurface] != NULL )
  {
    ioRAS[0] = m_fVertexSets[m_nActiveSurface][inVertex].x;
    ioRAS[1] = m_fVertexSets[m_nActiveSurface][inVertex].y;
    ioRAS[2] = m_fVertexSets[m_nActiveSurface][inVertex].z;
  }
  else
  {
    ioRAS[0] = static_cast<double>(m_MRIS->vertices[inVertex].x);
    ioRAS[1] = static_cast<double>(m_MRIS->vertices[inVertex].y);
    ioRAS[2] = static_cast<double>(m_MRIS->vertices[inVertex].z);
  }

  return true;
}

const char* FSSurface::GetVectorSetName( int nSet )
{
  if ( nSet >= 0 )
    return m_vertexVectors[nSet].name.c_str();
  else
    return NULL;
}

double FSSurface::GetCurvatureValue( int nVertex )
{
  return m_MRIS->vertices[nVertex].curv;
}

void FSSurface::Reposition( FSVolume *volume, int target_vno, double target_val, int nsize, double sigma ) 
{
  MRISsaveVertexPositions( m_MRIS, INFLATED_VERTICES ); 
  float fval = (float)target_val;
  MRISrepositionSurface( m_MRIS, volume->GetMRI(), &target_vno, &fval, 1, nsize, sigma );
  SaveVertices( m_MRIS, m_nActiveSurface );
  ComputeNormals();
  SaveNormals( m_MRIS, m_nActiveSurface );
  UpdateVerticesAndNormals();
}

void FSSurface::Reposition( FSVolume *volume, int target_vno, double* coord, int nsize, double sigma ) 
{
  MRISsaveVertexPositions( m_MRIS, INFLATED_VERTICES ); 
  MRISrepositionSurfaceToCoordinate( m_MRIS, volume->GetMRI(), target_vno, coord[0], coord[1], coord[2], nsize, sigma );
  SaveVertices( m_MRIS, m_nActiveSurface );
  ComputeNormals();
  SaveNormals( m_MRIS, m_nActiveSurface );
  UpdateVerticesAndNormals();
}

void FSSurface::UndoReposition()
{
  MRISrestoreVertexPositions( m_MRIS, INFLATED_VERTICES );
  SaveVertices( m_MRIS, m_nActiveSurface );
  ComputeNormals();
  SaveNormals( m_MRIS, m_nActiveSurface );
  UpdateVerticesAndNormals();
}
