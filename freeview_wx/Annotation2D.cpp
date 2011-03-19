/**
 * @file  Annotation2D.cpp
 * @brief Coordinate annotation for 2D views.
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
#include "Annotation2D.h"
#include "vtkRenderer.h"
#include "vtkPoints.h"
#include "vtkPolyData.h"
#include "vtkPolyDataMapper2D.h"
#include "vtkCellArray.h"
#include "vtkMath.h"
#include "vtkTextActor.h"
#include "vtkTextProperty.h"
#include "vtkActor2D.h"
#include "LayerCollection.h"
#include "MainWindow.h"
#include "vtkPropCollection.h"
#include "LayerMRI.h"
#include "LayerPropertiesMRI.h"
#include "MyUtils.h"

#define NUMBER_OF_COORD_ANNOTATIONS 6


Annotation2D::Annotation2D()
{
  m_actorsAll = vtkSmartPointer<vtkPropCollection>::New();
  for ( int i = 0; i < NUMBER_OF_COORD_ANNOTATIONS; i++ )
  {
    m_actorCoordinates[i] = vtkSmartPointer<vtkTextActor>::New();
    m_actorCoordinates[i]->GetPositionCoordinate()->
      SetCoordinateSystemToNormalizedViewport();
    m_actorCoordinates[i]->GetTextProperty()->ShadowOff();
    m_actorCoordinates[i]->GetTextProperty()->SetFontSize(11);
    m_actorCoordinates[i]->GetTextProperty()->ItalicOff();
    // m_actorCoordinates[i]->GetTextProperty()->SetFontFamilyToTimes();
    m_actorsAll->AddItem( m_actorCoordinates[i] );
  }
  m_actorCoordinates[0]->SetPosition( 0.01, 0.5 );
  m_actorCoordinates[0]->GetTextProperty()->SetJustificationToLeft();
  m_actorCoordinates[0]->GetTextProperty()->
    SetVerticalJustificationToCentered();
  m_actorCoordinates[1]->SetPosition( 0.5, 0.99 );
  m_actorCoordinates[1]->GetTextProperty()->SetJustificationToCentered();
  m_actorCoordinates[1]->GetTextProperty()->SetVerticalJustificationToTop();
  m_actorCoordinates[2]->SetPosition( 0.99, 0.5 );
  m_actorCoordinates[2]->GetTextProperty()->SetJustificationToRight();
  m_actorCoordinates[2]->GetTextProperty()->
    SetVerticalJustificationToCentered();
  m_actorCoordinates[3]->SetPosition( 0.5, 0.01 );
  m_actorCoordinates[3]->GetTextProperty()->SetJustificationToCentered();
  m_actorCoordinates[3]->GetTextProperty()->SetVerticalJustificationToBottom();

  // indicate slice location
  m_actorCoordinates[4]->SetPosition( 0.99, 0.01 );
  m_actorCoordinates[4]->GetTextProperty()->SetJustificationToRight();
  m_actorCoordinates[4]->GetTextProperty()->SetVerticalJustificationToBottom();

  // indicate slice number
  m_actorCoordinates[5]->SetPosition( 0.99, 0.99 );
  m_actorCoordinates[5]->GetTextProperty()->SetJustificationToRight();
  m_actorCoordinates[5]->GetTextProperty()->SetVerticalJustificationToTop();

  // scale actors
  m_actorScaleLine = vtkSmartPointer<vtkActor2D>::New();
  m_actorScaleLine->GetPositionCoordinate()->
    SetCoordinateSystemToNormalizedViewport();
  m_actorScaleLine->SetPosition( 0.05, 0.05 );
  m_actorsAll->AddItem( m_actorScaleLine );

  m_actorScaleTitle = vtkSmartPointer<vtkTextActor>::New();
  m_actorScaleTitle->GetPositionCoordinate()->
    SetCoordinateSystemToNormalizedViewport();
  m_actorScaleTitle->GetTextProperty()->ShadowOff();
  m_actorScaleTitle->GetTextProperty()->SetFontSize(11);
  m_actorScaleTitle->GetTextProperty()->ItalicOff();
  m_actorScaleTitle->GetTextProperty()->SetJustificationToCentered();
  m_actorScaleTitle->GetTextProperty()->SetVerticalJustificationToTop();
  m_actorsAll->AddItem( m_actorScaleTitle );
}

Annotation2D::~Annotation2D()
{}

void Annotation2D::Update( vtkRenderer* renderer, int nPlane )
{
  double slicePos[3] = { 0, 0, 0 };
  LayerCollection* lc = 
    MainWindow::GetMainWindowPointer()->GetLayerCollection( "MRI" );
  lc->GetSlicePosition( slicePos );
  bool bHasLayer = ( lc->GetNumberOfLayers() > 0 );

  if ( !bHasLayer )
  {
    return;
  }

  LayerMRI* mri = ( LayerMRI* )lc->GetActiveLayer();
  int nSliceNumber[3];
  double ras[3];
  if ( mri )
  {
   mri->RemapPositionToRealRAS( slicePos, ras );
   mri->RASToOriginalIndex( ras, nSliceNumber );
  }
  
  double centPos[3];
  double* worigin = mri->GetWorldOrigin();
  double* wsize = mri->GetWorldSize();
  double* wvoxel = mri->GetWorldVoxelSize();
  for ( int i = 0; i < 3; i++ )
  {
    centPos[i] = worigin[i] + wsize[i]/2;
    if ( !mri )
      nSliceNumber[i] = (int)( ( slicePos[i] - worigin[i] ) / wvoxel[i] );
  }

  double pos[4] = { 0, 1, 1, 0 }, tpos = 0;

  switch ( nPlane )
  {
  case 0:
    renderer->NormalizedViewportToView( pos[0], pos[1], tpos );
    renderer->ViewToWorld( pos[0], pos[1], tpos );
    pos[0] = pos[1];
    pos[1] = tpos;
    renderer->NormalizedViewportToView( pos[2], pos[3], tpos );
    renderer->ViewToWorld( pos[2], pos[3], tpos );
    pos[2] = pos[3];
    pos[3] = tpos;

    tpos = slicePos[0];
    mri->RemapPositionToRealRAS( tpos, pos[0], pos[1], 
				 slicePos[0], pos[0], pos[1] );
    if ( pos[0] >= 0 )
      m_actorCoordinates[0]->SetInput
	( wxString::Format
	  ( _("A %.2f"), pos[0]).char_str() );
    else
      m_actorCoordinates[0]->SetInput
	( wxString::Format
	  ( _("P %.2f"), -pos[0]).char_str() );

    if ( pos[1] >= 0 )
      m_actorCoordinates[1]->SetInput
	( wxString::Format
	  ( _("S %.2f"), pos[1]).char_str() );
    else
      m_actorCoordinates[1]->SetInput
	( wxString::Format
	  ( _("I  %.2f"), -pos[1]).char_str() );

    mri->RemapPositionToRealRAS( tpos, pos[2], pos[3], 
				 slicePos[0], pos[2], pos[3] );
    if ( pos[2] >= 0 )
      m_actorCoordinates[2]->SetInput
	( wxString::Format
	  ( _("A %.2f"), pos[2]).char_str() );
    else
      m_actorCoordinates[2]->SetInput
	( wxString::Format
	  ( _("P %.2f"), -pos[2]).char_str() );

    if ( pos[3] >= 0 )
      m_actorCoordinates[3]->SetInput
	( wxString::Format
	  ( _("S %.2f"), pos[3]).char_str() );
    else
      m_actorCoordinates[3]->SetInput
	( wxString::Format
	  ( _("I  %.2f"), -pos[3]).char_str() );

    mri->RemapPositionToRealRAS( tpos, centPos[1], centPos[2], 
				 slicePos[0], centPos[1], centPos[2] );
    if ( slicePos[0] >= 0 )
      m_actorCoordinates[4]->SetInput
	( wxString::Format
	  ( _("R %.2f"), slicePos[0]).char_str() );
    else
      m_actorCoordinates[4]->SetInput
	( wxString::Format
	  ( _("L %.2f"), -slicePos[0]).char_str() );

    break;
  case 1:
    renderer->NormalizedViewportToView( pos[0], pos[1], tpos );
    renderer->ViewToWorld( pos[0], pos[1], tpos );
    pos[1] = tpos;
    renderer->NormalizedViewportToView( pos[2], pos[3], tpos );
    renderer->ViewToWorld( pos[2], pos[3], tpos );
    pos[3] = tpos;

    tpos = slicePos[1];
    mri->RemapPositionToRealRAS( pos[0], tpos, pos[1], pos[0], 
				 slicePos[1], pos[1] );
    if ( pos[0] >= 0 )
      m_actorCoordinates[0]->SetInput
	( wxString::Format
	  ( _("R %.2f"), pos[0]).char_str() );
    else
      m_actorCoordinates[0]->SetInput
	( wxString::Format
	  ( _("L %.2f"), -pos[0]).char_str() );

    if ( pos[1] >= 0 )
      m_actorCoordinates[1]->SetInput
	( wxString::Format
	  ( _("S %.2f"), pos[1]).char_str() );
    else
      m_actorCoordinates[1]->SetInput
	( wxString::Format
	  ( _("I  %.2f"), -pos[1]).char_str() );

    mri->RemapPositionToRealRAS( pos[2], tpos, pos[3], pos[2], 
				 slicePos[1], pos[3] );

    if ( pos[2] >= 0 )
      m_actorCoordinates[2]->SetInput
	( wxString::Format
	  ( _("R %.2f"), pos[2]).char_str() );
    else
      m_actorCoordinates[2]->SetInput
	( wxString::Format
	  ( _("L %.2f"), -pos[2]).char_str() );

    if ( pos[3] >= 0 )
      m_actorCoordinates[3]->SetInput
	( wxString::Format
	  ( _("S %.2f"), pos[3]).char_str() );
    else
      m_actorCoordinates[3]->SetInput
	( wxString::Format
	  ( _("I  %.2f"), -pos[3]).char_str() );

    mri->RemapPositionToRealRAS( centPos[0], tpos, centPos[2], centPos[0], 
				 slicePos[1], centPos[2] );
    if ( slicePos[1] >= 0 )
      m_actorCoordinates[4]->SetInput
	( wxString::Format
	  ( _("A %.2f"), slicePos[1]).char_str() );
    else
      m_actorCoordinates[4]->SetInput
	( wxString::Format
	  ( _("P %.2f"), -slicePos[1]).char_str() );

    break;
  case 2:
    renderer->NormalizedViewportToView( pos[0], pos[1], tpos );
    renderer->ViewToWorld( pos[0], pos[1], tpos );
    tpos = 0;
    renderer->NormalizedViewportToView( pos[2], pos[3], tpos );
    renderer->ViewToWorld( pos[2], pos[3], tpos );

    tpos = slicePos[2];
    mri->RemapPositionToRealRAS( pos[0], pos[1], tpos, 
				 pos[0], pos[1], slicePos[2] );

    if ( pos[0] >= 0 )
      m_actorCoordinates[0]->SetInput
	( wxString::Format
	  ( _("R %.2f"), pos[0]).char_str() );
    else
      m_actorCoordinates[0]->SetInput
	( wxString::Format
	  ( _("L %.2f"), -pos[0]).char_str() );

    if ( pos[1] >= 0 )
      m_actorCoordinates[1]->SetInput
	( wxString::Format
	  ( _("A %.2f"), pos[1]).char_str() );
    else
      m_actorCoordinates[1]->SetInput
	( wxString::Format
	  ( _("P %.2f"), -pos[1]).char_str() );

    mri->RemapPositionToRealRAS( pos[2], pos[3], tpos, 
				 pos[2], pos[3], slicePos[2] );

    if ( pos[2] >= 0 )
      m_actorCoordinates[2]->SetInput
	( wxString::Format
	  ( _("R %.2f"), pos[2]).char_str() );
    else
      m_actorCoordinates[2]->SetInput
	( wxString::Format
	  ( _("L %.2f"), -pos[2]).char_str() );

    if ( pos[3] >= 0 )
      m_actorCoordinates[3]->SetInput
	( wxString::Format
	  ( _("A %.2f"), pos[3]).char_str() );
    else
      m_actorCoordinates[3]->SetInput
	( wxString::Format
	  ( _("P %.2f"), -pos[3]).char_str() );

    mri->RemapPositionToRealRAS( centPos[0], centPos[1], tpos, 
				 centPos[0], centPos[1], slicePos[2] );
    if ( slicePos[2] >= 0 )
      m_actorCoordinates[4]->SetInput
	( wxString::Format
	  ( _("S %.2f"), slicePos[2]).char_str() );
    else
      m_actorCoordinates[4]->SetInput
	( wxString::Format
	  ( _("I  %.2f"), -slicePos[2]).char_str() );

    break;
  }

  // update slice number
  int nOrigPlane = nPlane;
  if ( mri )
  {
    const char* ostr = mri->GetOrientationString();
    char ch[3][3] = {"RL", "AP", "IS"};
    for (int i = 0; i < 3; i++)
    {
      if (ostr[i] == ch[nPlane][0] || ostr[i] == ch[nPlane][1])
      {
        nOrigPlane = i;
        break;
      }
    }
  }
  m_actorCoordinates[5]->SetInput
    ( mri ? wxString::Format
      ( _("%d"), nSliceNumber[nOrigPlane] ).char_str() : "" );

  // update scale line
  double* xy_pos = m_actorScaleLine->GetPosition();
  double w_pos[3], w_pos2[3];
  int nNumOfTicks = 5;
  MyUtils::NormalizedViewportToWorld( renderer, 
				      xy_pos[0], 
				      xy_pos[1], 
				      w_pos[0], 
				      w_pos[1], 
				      w_pos[2] );
  MyUtils::NormalizedViewportToWorld( renderer, 
				      xy_pos[0] + 0.5, 
				      xy_pos[1], 
				      w_pos2[0], 
				      w_pos2[1], 
				      w_pos2[2] );
  w_pos[ nPlane ] = w_pos2[ nPlane ] = 0;
  double d = 0.5 / 
    sqrt( vtkMath::Distance2BetweenPoints( w_pos, w_pos2 ) ) * 10;
  wxString title = _("1 cm");
  if ( d >= 0.5 - xy_pos[0] )
  {
    d /= 2;
    title = _("5 mm");
    if ( d >= 0.5 - xy_pos[0] )
    {
      d *= 0.4;
      title = _("2 mm");
      nNumOfTicks = 2;
      if ( d >= 0.5 - xy_pos[0] )
      {
        d /= 2;
        title = _("1 mm");
        nNumOfTicks = 1;
        if ( d >= 0.5 - xy_pos[0] )
        {
          d /= 2;
          title = _("500 um");
          nNumOfTicks = 5;
          if ( d >= 0.5 - xy_pos[0] )
          {
            d *= 0.4;
            title = _("200 um");
            nNumOfTicks = 2;
            if ( d >= 0.5 - xy_pos[0] )
            {
              d /= 2;
              title = _("100 um");
              nNumOfTicks = 1;
            }
          }
        }
      }
    }
  }

  UpdateScaleActors( d, nNumOfTicks,  title.char_str() );
}

void Annotation2D::UpdateScaleActors( double length, 
				      int nNumOfTicks, 
				      const char* title )
{
  // scale line
  double* pos = m_actorScaleLine->GetPosition();
  double tick_len = 0.007;
  vtkPoints* Pts = vtkPoints::New();
  Pts->InsertNextPoint( 0, 0, 0 );
  Pts->InsertNextPoint( length, 0, 0 );
  vtkCellArray* Lines = vtkCellArray::New();
  Lines->InsertNextCell( 2 );
  Lines->InsertCellPoint( 0 );
  Lines->InsertCellPoint( 1 );
  Lines->InsertNextCell( 2 );
  Lines->InsertCellPoint( 1 );
  Lines->InsertCellPoint( 0 );

  int n = 2;
  double step = length / nNumOfTicks;
  for ( int i = 0; i <= nNumOfTicks; i++ )
  {
    Pts->InsertNextPoint( step*i, 0, 0 );
    Pts->InsertNextPoint( step*i, -tick_len, 0 );
    Lines->InsertNextCell( 2 );
    Lines->InsertCellPoint( n++ );
    Lines->InsertCellPoint( n++ );
  }

  vtkPolyData* poly = vtkPolyData::New();
  poly->SetPoints(Pts);
  poly->SetLines(Lines);
  Pts->Delete();
  Lines->Delete();

  vtkCoordinate* normCoords = vtkCoordinate::New();
  normCoords->SetCoordinateSystemToNormalizedViewport();

  vtkPolyDataMapper2D* pMapper = vtkPolyDataMapper2D::New();
  pMapper->SetInput( poly );
  pMapper->SetTransformCoordinate(normCoords);
  poly->Delete();
  normCoords->Delete();

  m_actorScaleLine->SetMapper(pMapper);
  pMapper->Delete();

  // title
  m_actorScaleTitle->SetPosition( pos[0] + length / 2, pos[1] - 0.01 );
  m_actorScaleTitle->SetInput( title );
}

void Annotation2D::AppendAnnotations( vtkRenderer* renderer )
{
  for ( int i = 0; i < NUMBER_OF_COORD_ANNOTATIONS; i++ )
    renderer->AddViewProp( m_actorCoordinates[i] );

  renderer->AddViewProp( m_actorScaleLine );
  renderer->AddViewProp( m_actorScaleTitle );
}

void Annotation2D::ShowScaleLine( bool bShow )
{
  m_actorScaleLine->SetVisibility( bShow?1:0 );
  m_actorScaleTitle->SetVisibility( bShow?1:0 );
}

bool Annotation2D::GetShowScaleLine()
{
  return m_actorScaleLine->GetVisibility() > 0;
}

void Annotation2D::Show( bool bShow )
{
  vtkProp* prop = NULL;
  m_actorsAll->InitTraversal();
  while ( ( prop = m_actorsAll->GetNextProp() ) )
  {
    prop->SetVisibility( bShow?1:0 );
  }
}

bool Annotation2D::IsVisible()
{
  m_actorsAll->InitTraversal();
  vtkProp* prop = m_actorsAll->GetNextProp();
  if ( prop )
    return (prop->GetVisibility() > 0);
  else
    return false;
}

