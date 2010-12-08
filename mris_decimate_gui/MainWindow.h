/*
 * Original Author: Dan Ginsburg (@ Children's Hospital Boston)
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2010/12/08 17:26:31 $
 *    $Revision: 1.3.2.2 $
 *
 * Copyright (C) 2010,
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
#ifndef __MainWindow__
#define __MainWindow__

#include <wx/string.h>
#include <vector>

extern "C"
{
#include "mrisurf.h"
}

/**
@file
Subclass of MainWindowBase, which is generated by wxFormBuilder.
*/

#include "MainWindowBase.h"

class RenderPanel;
class DecimatePanel;
class wxPanel;

/** Implementing MainWindowBase */
class MainWindow : public MainWindowBase
{
protected:
    // Handlers for MainWindowBase events.
    void OnFileOpen( wxCommandEvent& event );
    void OnFileExit( wxCommandEvent& event );
    void OnFileSave( wxCommandEvent& event );
    void OnFileSaveAs( wxCommandEvent& event );
    void OnAbout( wxCommandEvent& event );
    void OnClose( wxCloseEvent& event );

    void CommandLoadSurface( const wxArrayString& sa );
    void CommandScreenCapture( const wxArrayString& sa );
    void CommandDecimationLevel( const wxArrayString& sa );
    void CommandCurvature( const wxArrayString& sa );
    void CommandFileSave( const wxArrayString& sa );
    void CommandCameraRotate( const wxArrayString& sa );

    void LoadSurface(const wxString& filePath);


public:
    /// Constructor
    MainWindow( wxWindow* parent );

    /// Add a script command to run on startup
    void AddScript( const wxArrayString& script );

    /// Run a script established from the command-line (or elsewhere)
    void RunScript();

    /// Handle updates to the camera
    void HandleCameraUpdate();
        
    /// Original MRIS surface
    MRI_SURFACE *m_origSurface;

    /// Decimate Panel
    DecimatePanel *m_decimatePanel;

    /// Render Panel
    RenderPanel *m_renderPanel;

    /// Render Panel Holder
    wxPanel *m_renderPanelHolder;

    /// Current File Path
    wxString m_currentFilePath;

    /// Last load directory
    wxString m_lastLoadDir;

    /// Last save directory
    wxString m_lastSaveDir;

    /// Script commands
    std::vector<wxArrayString> m_scripts;

};

#endif // __MainWindow__
