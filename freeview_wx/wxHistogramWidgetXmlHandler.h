/**
 * @file  wxHistogramWidgetXmlHandler.h
 * @brief wxHistogramWidgetXmlHandler class.
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2011/03/19 18:44:27 $
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

#ifndef wxHistogramWidgetXmlHandler_h
#define wxHistogramWidgetXmlHandler_h


#include "wx/xrc/xmlres.h"



class WXDLLIMPEXP_XRC wxHistogramWidgetXmlHandler : public wxXmlResourceHandler
{
  DECLARE_DYNAMIC_CLASS( wxHistogramWidgetXmlHandler )


public:
  wxHistogramWidgetXmlHandler();
  ~wxHistogramWidgetXmlHandler()
  {}

  wxObject *DoCreateResource();

  bool CanHandle( wxXmlNode *node );

};

#endif


