/**
 * @file  LivewireTool.h
 * @brief LivewireTool data object.
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

#ifndef LivewireTool_h
#define LivewireTool_h

#include <vtkSmartPointer.h>

#include <string>
#include <vector>

class vtkImageClip;
class vtkImageData;
class vtkDijkstraImageGeodesicPath;
class vtkPoints;
class vtkImageChangeInformation;

class LivewireTool
{
public:
  LivewireTool( );
  virtual ~LivewireTool();

  void UpdateImageDataInfo( vtkImageData* image, int nPlane, int nSlice );

  void GetLivewirePoints( double* pt1_in, double* pt2_in, vtkPoints* pts_out );

  void GetLivewirePoints( vtkImageData* image, int nPlane, int nSlice,
                          double* pt1_in, double* pt2_in, vtkPoints* pts_out );

protected:
  int  m_nPlane;
  int  m_nSlice;
  vtkSmartPointer<vtkDijkstraImageGeodesicPath>  m_path;
  vtkImageData*         m_imageData;
  vtkImageData*         m_imageSlice;
};

#endif


