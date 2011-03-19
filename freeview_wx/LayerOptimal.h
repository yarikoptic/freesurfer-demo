/**
 * @file  LayerOptimal.h
 * @brief Layer class for computed optimal volume.
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

#ifndef LayerOptimal_h
#define LayerOptimal_h

#include "LayerMRI.h"
#include "vtkSmartPointer.h"
#include <string>
#include <vector>

class FSVectorVolume;
class wxWindow;
class wxCommandEvent;
class LayerPropertiesDTI;

class LayerOptimal : public LayerMRI
{
public:
  LayerOptimal( LayerMRI* ref );
  virtual ~LayerOptimal();

  bool Create( LayerMRI* layer_label, std::vector<LayerMRI*> layers );

  // bool Rotate( std::vector<RotationElement>& rotations, wxWindow* wnd, wxCommandEvent& event );
  bool UpdateVolume();

protected:
  void DoListenToMessage( std::string const iMessage, void* iData, void* sender );

  LayerMRI* m_layerLabel;
  std::vector<LayerMRI*> m_layers;
};

#endif


