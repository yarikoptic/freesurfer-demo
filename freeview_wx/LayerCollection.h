/**
 * @file  LayerCollection.h
 * @brief Collection of layers of the same type.
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

#ifndef LayerCollection_h
#define LayerCollection_h

#include <string>
#include <vector>
#include "Listener.h"
#include "Broadcaster.h"

class Layer;
class LayerMRI;
class vtkRenderer;
class vtkProp;

class LayerCollection : public Listener, public Broadcaster
{
public:
  LayerCollection( std::string type );
  virtual ~LayerCollection();

  int GetNumberOfLayers();
  Layer* GetLayer( int i );
  int GetLayerIndex( Layer* layer );

  bool AddLayer( Layer* layer, bool initializeCoordinate = false );
  bool RemoveLayer( Layer* layer, bool deleteObject = true );
  bool MoveLayerUp( Layer* layer );
  bool MoveLayerDown( Layer* layer );
  bool MoveToTop( Layer* layer );
  bool CycleLayer( bool bMoveUp = true );

  void Append2DProps( vtkRenderer* renderer, int nImagePlane );
  void Append3DProps( vtkRenderer* renderer, bool* bSliceVisibility = NULL );

  bool Contains( Layer* layer );
  bool IsEmpty();
  
  void ClearAll();

  void SetActiveLayer( Layer* layer );
  Layer* GetActiveLayer();

  Layer* GetFirstVisibleLayer();

  double* GetSlicePosition();
  void GetSlicePosition( double* slicePos );
  bool SetSlicePosition( int nPlane, double dPos, bool bRoundToGrid = true );
  bool OffsetSlicePosition( int nPlane, double dPosDiff, bool bRoundToGrid = true );
  bool SetSlicePosition( int nPlane, int nSliceNumber );
  bool SetSlicePosition( double* slicePos );

  double* GetCurrentRASPosition();
  void GetCurrentRASPosition( double* pos );
  void SetCurrentRASPosition( double* pos );

  double* GetCursorRASPosition();
  void GetCursorRASPosition( double* pos );
  void SetCursorRASPosition( double* pos );

  void GetCurrentRASIndex( int* nIdx );

  double* GetWorldOrigin();
  void SetWorldOrigin( double* dWorldOrigin );

  double* GetWorldSize();
  void SetWorldSize( double* dWorldSize );

  double* GetWorldVoxelSize();
  void SetWorldVoxelSize( double* dVoxelSize );

  void GetWorldCenter( double* pos );

  std::vector<Layer*> GetLayers();

  Layer* HasProp( vtkProp* prop );

  virtual void DoListenToMessage( std::string const iMsg, void* iData, void* sender );

  std::string GetType();

protected:
  std::vector<Layer*> m_layers;

  double      m_dSlicePosition[3];
  double      m_dWorldOrigin[3];
  double      m_dWorldSize[3];
  double      m_dWorldVoxelSize[3];

  double      m_dCurrentRASPosition[3];
  int         m_nCurrentRASIndex[3];

  double      m_dCursorRASPosition[3];

  Layer*      m_layerActive;
  std::string m_strType;
};

#endif


