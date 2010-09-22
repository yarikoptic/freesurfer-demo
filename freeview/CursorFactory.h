/**
 * @file  Cursor2D.h
 * @brief Cursor creator/loader for 2D view.
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2010/09/22 17:13:35 $
 *    $Revision: 1.10.2.2 $
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

#ifndef CursorFactory_h
#define CursorFactory_h

#include <wx/cursor.h>

class CursorFactory
{
public:
  CursorFactory();

  static void Initialize();

  static wxCursor CursorPan;
  static wxCursor CursorZoom;

  static wxCursor CursorPencil;
  static wxCursor CursorFill;
  static wxCursor CursorPolyline;
  static wxCursor CursorColorPicker;
  static wxCursor CursorMeasureLine;
  static wxCursor CursorMeasureRectangle;
  static wxCursor CursorMeasurePolyline;
  static wxCursor CursorGrab;
  static wxCursor CursorContour;
  static wxCursor CursorAdd;
  static wxCursor CursorRemove;
};

#endif


