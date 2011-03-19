/**
 * @file  Region2DPolyline.h
 * @brief Region2DPolyline data object.
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

#ifndef Region2DPolyline_h
#define Region2DPolyline_h

#include "Region2D.h"
#include <wx/wx.h>
#include <string>
#include <vector>
#include "vtkSmartPointer.h"

class vtkActor2D;
class RenderView2D;
class vtkRenderer;
class vtkTextActor;

class Region2DPolyline : public Region2D
{
public:
  Region2DPolyline( RenderView2D* view, bool bSpline = false );
  virtual ~Region2DPolyline();

  void AddPoint( int x, int y );
  
  void Offset( int x, int y );
  
  bool Contains( int x, int y, int* nIndexOut = NULL );
  void UpdatePoint( int nIndex, int nX, int nY );
  
  void AppendProp( vtkRenderer* renderer );
  
  void Show( bool bshow = true );
  void Highlight( bool bHighlight = true );
  
  void Update();
  void UpdateStats();
  
  void UpdateSlicePosition( int nPlane, double pos );
  
  void GetWorldPoint( int nIndex, double* pt );
  
  void RemoveLastPoint();

protected:
  void UpdateWorldCoords();  
  
  vtkSmartPointer<vtkActor2D>   m_actorPolyline;
  vtkSmartPointer<vtkActor2D>   m_actorPoints;
  vtkSmartPointer<vtkTextActor> m_actorText;
  
  struct ScreenPoint {
    int pos[2];
  };
  
  struct WorldPoint {
    double pos[3];
  };
  
  std::vector<ScreenPoint>   m_screenPts;       // 2D points
  std::vector<WorldPoint>    m_worldPts;
  
  bool    m_bSpline;
};

#endif


