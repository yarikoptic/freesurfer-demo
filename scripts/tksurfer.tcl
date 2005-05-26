#! /usr/bin/tixwish

# $Id: tksurfer.tcl,v 1.66.2.3 2005/05/26 22:27:19 kteich Exp $

package require BLT;

# This function finds a file from a list of directories.
proc FindFile { ifnFile ilDirs } {
    foreach sPath $ilDirs {
	set sFullFileName [ file join $sPath $ifnFile ]
	if { [file readable $sFullFileName] } {
	    puts "Reading $sFullFileName"
	    return $sFullFileName
	}
    }
    puts "Couldn't find $ifnFile: Not in $ilDirs"
    return ""
}

# Try to get some default script locations from environment variables.
set sDefaultScriptsDir ""
catch { set sDefaultScriptsDir "$env(FREESURFER_HOME)/lib/tcl" }
set sTksurferScriptsDir ""
catch { set sTksurferScriptsDir "$env(TKSURFER_SCRIPTS_DIR)" }
set sFsgdfDir ""
catch { set sFsgdfDir "$env(FSGDF_DIR)" }

# Find the right file names and source them.
set fnCommon \
    [FindFile tkm_common.tcl \
	 [list $sTksurferScriptsDir "." "../scripts" $sDefaultScriptsDir]]
if { [string compare $fnCommon ""] == 0 } { exit }
source $fnCommon

set fnWrappers \
    [FindFile tkm_wrappers.tcl \
	 [list $sTksurferScriptsDir "." "../scripts" $sDefaultScriptsDir]]
if { [string compare $fnWrappers ""] == 0 } { exit }
source $fnWrappers

set fnFsgdf \
    [FindFile fsgdfPlot.tcl \
	 [list $sFsgdfDir "." "../scripts" $sDefaultScriptsDir]]
if { [string compare $fnFsgdf ""] == 0 } { exit }
source $fnFsgdf



# ================================================================== CONSTANTS

set ksWindowName "TkSurfer Tools"
set ksImageDir   "$env(FREESURFER_HOME)/lib/images/"

# ===================================================== DEFAULT FILE LOCATIONS

# subject/surf : surface, vertices
# subject/surf + pwd : overlay, time course, curvature, path
# FREESURFER_HOME : clut
# subject/label + pwd : label, annotation
# subject/fmri + pwd : field sign, field mask
# subject/rgb + pwd : rgb

array set gaFileNameDefDirs [list \
    kFileName_Surface   "$home/$subject/surf" \
    kFileName_Script    "$home/$subject/scripts" \
    kFileName_Label     "$home/$subject/label" \
    kFileName_BEM       "$home/$subject/bem" \
    kFileName_FMRI      "$home/$subject/fmri" \
    kFileName_RGB       "$home/$subject/rgb" \
    kFileName_TIFF      "$home/$subject/rgb" \
    kFileName_Home      "$home/$subject" \
    kFileName_PWD       "$env(PWD)" \
    kFileName_CSURF     "$env(FREESURFER_HOME)" \
]

# determine the list of shortcut dirs for the file dlog boxes
set glShortcutDirs {}
if { [info exists env(SUBJECTS_DIR)] } {
    lappend glShortcutDirs $env(SUBJECTS_DIR)
    lappend glShortcutDirs $env(SUBJECTS_DIR)/$subject
}
if { [info exists env(FREESURFER_DATA)] } {
    lappend glShortcutDirs $env(FREESURFER_DATA)
}
if { [info exists env(FREESURFER_HOME)] } {
    lappend glShortcutDirs $env(FREESURFER_HOME)
}
if { [info exists env(PWD)] } {
    lappend glShortcutDirs $env(PWD)
}
if { [info exists env(FSDEV_TEST_DATA)] } {
    lappend glShortcutDirs $env(FSDEV_TEST_DATA)
}

# =========================================================== LINKED VARIABLES

### current transforms
set xrot 0
set yrot 0
set zrot 0
set xtrans 0
set ytrans 0
set scalepercent 100 

### lights: can't be written back (floats in gl struct)
set light0 0.4
set light1 0.0
set light2 0.6
set light3 0.2
set offset 0.25

### events
set userok 0
set blinkflag FALSE
# set initdelay $blinkdelay

### misc defaults
set surfcolor 1
#set colscale 0
#set fthresh 0.1
set shrinksteps 5
set smoothsteps 5
set smoothtype val
set redrawlockflag 0

set glLabelNames {}

set gCopyFieldTarget 0

# default transform values
set kanDefaultTransform(rotate) 90
set kanDefaultTransform(translate) 10
set kanDefaultTransform(scale) 110

# set some default histogram data
set gaHistogramData(zoomed) 0

# used in overlay config dialog
set gbOverlayApplyToAll 0

# ====================================================== LINKED VAR MANAGEMENT

# holds local values of all the linked variables
set gaLinkedVars(light0) 0
set gaLinkedVars(light1) 0
set gaLinkedVars(light2) 0 
set gaLinkedVars(light3) 0
set gaLinkedVars(offset) 0
set gaLinkedVars(colscale) 0
set gaLinkedVars(truncphaseflag) 0
set gaLinkedVars(invphaseflag) 0
set gaLinkedVars(revphaseflag) 0
set gaLinkedVars(complexvalflag) 0
set gaLinkedVars(ignorezeroesinhistogramflag) 1
set gaLinkedVars(currentvaluefield) 0
set gaLinkedVars(falpha) 1.0
set gaLinkedVars(fthresh) 0
set gaLinkedVars(fmid) 0
set gaLinkedVars(foffset) 0
set gaLinkedVars(fthreshmax) 0
set gaLinkedVars(fslope) 1.0
set gaLinkedVars(fnumconditions) 0
set gaLinkedVars(fnumtimepoints) 0
set gaLinkedVars(ftimepoint) 0
set gaLinkedVars(fcondition) 0
set gaLinkedVars(fmin) 0
set gaLinkedVars(fmax) 0
set gaLinkedVars(cslope) 0
set gaLinkedVars(cmid) 0
set gaLinkedVars(cmin) 0
set gaLinkedVars(cmax) 0
set gaLinkedVars(forcegraycurvatureflag) 0
set gaLinkedVars(angle_cycles) 0
set gaLinkedVars(angle_offset) 0
set gaLinkedVars(sulcflag) 0
set gaLinkedVars(surfcolor) 0
set gaLinkedVars(vertexset) 0
set gaLinkedVars(overlayflag) 0
set gaLinkedVars(funcmin) 0
set gaLinkedVars(funcmax) 0
set gaLinkedVars(scalebarflag) 0
set gaLinkedVars(colscalebarflag) 0
set gaLinkedVars(verticesflag) 0
set gaLinkedVars(cmid) 0
set gaLinkedVars(dipavg) 0
set gaLinkedVars(curvflag) 1
set gaLinkedVars(mouseoverflag) 0
set gaLinkedVars(redrawlockflag) 0
set gaLinkedVars(drawlabelflag) 1
set gaLinkedVars(labelstyle) 0
set gaLinkedVars(timeresolution) 0
set gaLinkedVars(numprestimpoints) 0
set gaLinkedVars(colortablename) ""

    
# groups of variables that get sent to c code together
array set gaLinkedVarGroups {
    scene { light0 light1 light2 light3 offset }
    overlay { falpha colscale truncphaseflag invphaseflag revphaseflag 
	complexvalflag foffset fthresh fmid foffset fslope fmin fmax 
	fnumtimepoints fnumconditions ftimepoint fcondition 
	ignorezeroesinhistogramflag labels_before_overlay_flag}
    curvature { cslope cmid cmin cmax forcegraycurvatureflag }
    phase { angle_offset angle_cycles }
    inflate { sulcflag }
    view { curvflag flagsurfcolor vertexset overlayflag scalebarflag 
	colscalebarflag verticesflag currentvaluefield drawcursorflag }
    cvavg { cmid dipavg }
    mouseover { mouseoverflag }
    all { light0 light1 light2 light3 offset colscale truncphaseflag invphaseflag revphaseflag complexvalflag ignorezeroesinhistogramflag currentvaluefield falpha  fthresh fmid foffset fthreshmax fslope  fnumconditions fnumtimepoints ftimepoint fcondition fmin fmax cslope cmid cmin cmax forcegraycurvatureflag angle_cycles angle_offset sulcflag surfcolor vertexset overlayflag funcmin funcmax scalebarflag colscalebarflag verticesflag cmid dipavg curvflag mouseoverflag redrawlockflag drawlabelflag labelstyle timeresolution numprestimpoints colortablename }
    redrawlock { redrawlockflag }
    graph { timeresolution numprestimpoints }
    label { colortablename drawlabelflag labelstyle labels_before_overlay_flag}
}

proc SendLinkedVarGroup { iGroup } {
    global gaLinkedVarGroups gaLinkedVars
    set lVars $gaLinkedVarGroups($iGroup)
    foreach var $lVars {
	catch {
	    upvar #0 $var varToUpdate;
	    set varToUpdate $gaLinkedVars($var)
	}
    }
}

proc UpdateLinkedVarGroup { iGroup } {
    global gaLinkedVarGroups gaLinkedVars
    set lVars $gaLinkedVarGroups($iGroup)
    foreach var $lVars {
  catch {
    upvar #0 $var varToUpdate
    set gaLinkedVars($var) $varToUpdate
      }
    }
}

proc PrintLinkedVarGroup { iGroup } {
    global gaLinkedVarGroups gaLinkedVars
    set lVars $gaLinkedVarGroups($iGroup)
    foreach var $lVars {
  puts "$var=$gaLinkedVars($var)"
    }
}

proc PrintVar { iVar nIndex op } {
    puts "$iVar\($nIndex\) $op"
}

proc UpdateLabel { inSet inLabelIndex isLabel } {
    global glLabel gsaLabelContents
    if { $inSet == 0 } {
	set labelSet cursor
    } else {
	set labelSet mouseover
    }
    set gsaLabelContents([lindex $glLabel $inLabelIndex],value,$labelSet) $isLabel
}

proc UpdateUndoItemLabel { isLabel } {

    .w.fwMenuBar.mbwEdit.mw entryconfigure 1 -label $isLabel
}

proc UpdateValueLabelName { inValueIndex isName } {
    
    global gaScalarValueID gsaLabelContents gaSwapFieldInfo
    if { [info exists gaScalarValueID($inValueIndex,label)] == 0 } {
        puts "UpdateValueLabelName: $inValueIndex invalid"
        return
    }

    set label $gaScalarValueID($inValueIndex,label)

    # set the label contents name.
    set gsaLabelContents($label,name) $isName

    # set the swap field info name.
    set gaSwapFieldInfo($label,label) $isName

    # view->information menu
    .w.fwMenuBar.mbwView.mw.cmw2 entryconfigure [expr 12 + $inValueIndex] \
      -label $isName
    # view->overlay menu
    .w.fwMenuBar.mbwView.mw.cmw8 entryconfigure [expr 1 + $inValueIndex] \
      -label $isName
}

proc SwapValueLabelNames { inValueIndexA inValueIndexB } {

    global gaScalarValueID gsaLabelContents gaSwapFieldInfo
    if { [info exists gaScalarValueID($inValueIndexA,label)] == 0 } {
        puts "UpdateValueLabelName: $inValueIndex invalid"
        return
    }
    if { [info exists gaScalarValueID($inValueIndexB,label)] == 0 } {
        puts "UpdateValueLabelName: $inValueIndex invalid"
        return
    }

    set labelA $gaScalarValueID($inValueIndexA,label)
    set labelB $gaScalarValueID($inValueIndexB,label)

    # get old values
    set sLabelContentsA $gsaLabelContents($labelA,name)
    set sLabelContentsB $gsaLabelContents($labelB,name)

    # set the label contents name.
    set gsaLabelContents($labelA,name) $sLabelContentsB
    set gsaLabelContents($labelB,name) $sLabelContentsA

    # set the swap field info name.
    set gaSwapFieldInfo($labelA,label) $sLabelContentsB
    set gaSwapFieldInfo($labelB,label) $sLabelContentsA
}

proc RequestOverlayInfoUpdate {} {
    # tell the c code to send us the current overlay info.
    sclv_send_current_field_info
}

# ==================================================================== GLOBALS

set gNextTransform(rotate,x) 0
set gNextTransform(rotate,y) 0
set gNextTransform(rotate,z) 0
set gNextTransform(rotate,deg) 0
set gNextTransform(translate,x) 0
set gNextTransform(translate,y) 0
set gNextTransform(translate,z) 0
set gNextTransform(translate,dist) 0
set gNextTransform(scale) 0
set gNextTransform(scale,amt) 100

# labels
set glLabel { \
  kLabel_VertexIndex \
  kLabel_Distance \
  kLabel_Coords_RAS \
  kLabel_Coords_MniTal \
  kLabel_Coords_Tal \
  kLabel_Coords_Index \
  kLabel_Coords_Normal \
  kLabel_Coords_Sphere_XYZ \
  kLabel_Coords_Sphere_RT \
  kLabel_Curvature \
  kLabel_Fieldsign \
  kLabel_Val \
  kLabel_Val2 \
  kLabel_ValBak \
  kLabel_Val2Bak \
  kLabel_ValStat \
  kLabel_ImagVal \
  kLabel_Mean \
  kLabel_MeanImag \
  kLabel_StdError \
  kLabel_Amplitude \
  kLabel_Angle \
  kLabel_Degree \
  kLabel_Label \
  kLabel_Annotation \
  kLabel_MRIValue \
  kLabel_Parcellation_Name }
foreach label $glLabel {
    set gfwaLabel($label,cursor) ""
    set gfwaLabel($label,mouseover) ""
}

set gsaLabelContents(kLabel_VertexIndex,name)       "Vertex Index"
set gsaLabelContents(kLabel_Distance,name)          "Distance"
set gsaLabelContents(kLabel_Coords_RAS,name)        "Vertex RAS"
set gsaLabelContents(kLabel_Coords_MniTal,name)     "Vertex MNI Talairach"
set gsaLabelContents(kLabel_Coords_Tal,name)        "Vertex Talairach"
set gsaLabelContents(kLabel_Coords_Index,name)      "MRI Index"
set gsaLabelContents(kLabel_Coords_Normal,name)     "Vertex Normal"
set gsaLabelContents(kLabel_Coords_Sphere_XYZ,name) "Spherical X, Y, Z"
set gsaLabelContents(kLabel_Coords_Sphere_RT,name)  "Spherical Rho, Theta"
set gsaLabelContents(kLabel_Curvature,name)         "Curvature"
set gsaLabelContents(kLabel_Fieldsign,name)         "Field Sign"
set gsaLabelContents(kLabel_Val,name)               "Overlay Layer 1 (empty)"
set gsaLabelContents(kLabel_Val2,name)              "Overlay Layer 2 (empty)"
set gsaLabelContents(kLabel_ValBak,name)            "Overlay Layer 3 (empty)"
set gsaLabelContents(kLabel_Val2Bak,name)           "Overlay Layer 4 (empty)"
set gsaLabelContents(kLabel_ValStat,name)           "Overlay Layer 5 (empty)"
set gsaLabelContents(kLabel_ImagVal,name)           "Overlay Layer 6 (empty)"
set gsaLabelContents(kLabel_Mean,name)              "Overlay Layer 7 (empty)"
set gsaLabelContents(kLabel_MeanImag,name)          "Overlay Layer 8 (empty)"
set gsaLabelContents(kLabel_StdError,name)          "Overlay Layer 9 (empty)"
set gsaLabelContents(kLabel_Amplitude,name)         "Amplitude"
set gsaLabelContents(kLabel_Angle,name)             "Angle"
set gsaLabelContents(kLabel_Degree,name)            "Degree"
set gsaLabelContents(kLabel_Label,name)             "Label"
set gsaLabelContents(kLabel_Annotation,name)        "Annotation"
set gsaLabelContents(kLabel_MRIValue,name)          "MRI Value"
set gsaLabelContents(kLabel_Parcellation_Name,name) "Parcellation: "

foreach label $glLabel {
    set gsaLabelContents($label,value,cursor)    "none"
    set gsaLabelContents($label,value,mouseover) "none"
}

set glSwapField { \
  kField_Curv \
  kField_CurvBak \
  kField_Val \
  kField_Val2 \
  kField_ValBak \
  kField_Val2Bak \
  kField_Stat \
  kField_ImagVal }
set gaSwapFieldInfo(kField_Curv,label)     "curv"
set gaSwapFieldInfo(kField_CurvBak,label)  "curvbak"
set gaSwapFieldInfo(kField_Val,label)      "val"
set gaSwapFieldInfo(kField_Val2,label)     "val2"
set gaSwapFieldInfo(kField_ValBak,label)   "valbak"
set gaSwapFieldInfo(kField_Val2Bak,label)  "val2bak"
set gaSwapFieldInfo(kField_Stat,label)     "stat"
set gaSwapFieldInfo(kField_ImagVal,label)  "imag_val"
set n 0
foreach field $glSwapField {
    set gaSwapFieldInfo($field,index) $n
    incr n
}

# scalar values -> labels
set gaScalarValueID(kLabel_Val,index) 0
set gaScalarValueID(0,label) kLabel_Val
set gaScalarValueID(kLabel_Val2,index) 1
set gaScalarValueID(1,label) kLabel_Val2
set gaScalarValueID(kLabel_ValBak,index) 2
set gaScalarValueID(2,label) kLabel_ValBak
set gaScalarValueID(kLabel_Val2Bak,index) 3
set gaScalarValueID(3,label) kLabel_Val2Bak
set gaScalarValueID(kLabel_ValStat,index) 4
set gaScalarValueID(4,label) kLabel_ValStat
set gaScalarValueID(kLabel_ImagVal,index) 5
set gaScalarValueID(5,label) kLabel_ImagVal
set gaScalarValueID(kLabel_Mean,index) 6
set gaScalarValueID(6,label) kLabel_Mean
set gaScalarValueID(kLabel_MeanImag,index) 7
set gaScalarValueID(7,label) kLabel_MeanImag
set gaScalarValueID(kLabel_StdError,index) 8
set gaScalarValueID(8,label) kLabel_StdError

# tool bar frames
set gfwaToolBar(main)  ""
set gfwaToolBar(label)  ""

# ========================================================= BUILDING INTERFACE


proc CreateColorPicker { iwTop iCallbackFunction {izSquare 16} } {
    global gColorPickerCB

    # Picker config.
    set kzSquareX $izSquare
    set kzSquareY $izSquare
    set klSquaresX 25
    set klSquaresY 9
    
    # 216 colors
    set kcReds 4
    set kcGreens 4
    set kcBlues 8

    set gColorPickerCB $iCallbackFunction

    frame $iwTop

    set cwPicker       $iwTop.cwPicker

    # Create the canvas with the right width and height
    canvas $cwPicker \
	-width [expr $kzSquareX * $klSquaresX] \
	-height [expr $kzSquareY * $klSquaresY]

    # Find the color increments for each next square.
    set redInc   [expr 256 / $kcReds]
    set greenInc [expr 256 / $kcGreens]
    set blueInc  [expr 256 / $kcBlues]

    # Start off with color 0,0,0.
    set r 0
    set g 0
    set b 0

    # For each square...
    for { set nY 0 } { $nY < $klSquaresY } { incr nY } {
	for { set nX 0 } { $nX < $klSquaresX } { incr nX } {
	    
	    # Create a square in the current color, converting our
	    # numerical values to a hex color value. Also give it the
	    # common tag 'color' and a tag made out of its numerical
	    # color components.
	    $cwPicker create rectangle \
		[expr $nX * $kzSquareX] [expr $nY * $kzSquareY] \
		[expr ($nX+1) * $kzSquareX] [expr ($nY+1) * $kzSquareY] \
		-fill [format "#%.2x%.2x%.2x" $r $g $b] \
		-tags "color $r-$g-$b"

	    # Increment our numerical color values in the order of r,
	    # g, and then b. With the increments we have, we'll
	    # actually get to 256, but we really want a top value of
	    # 255, so check for the 256 and bump it down to
	    # 255. (Dividing with 255 when finding the increments
	    # doesn't give us the full 0-255 range.
	    incr r $redInc
	    if { $r == 256 } { set r 255 }
	    if { $r > 255 } {
		set r 0
		incr g $greenInc
		if { $g == 256 } { set g 255 }
		if { $g > 255 } {
		    set g 0
		    incr b $blueInc
		    if { $b == 256 } { set b 255 }
		    if { $b > 255 } {
			set b 0
		    }
		}
	    }
	}
    }

    # When we click on a color square, call the function.
    $cwPicker bind color <Button-1> { HandlePickedColor %W }

    pack $cwPicker -fill both
}

proc HandlePickedColor { icwPicker } {
    global gColorPickerCB

    # Get the numerical tag from the current element, the one the
    # mouse is in. This is our r-g-b tag. Extract the numerical elements.
    set color [lindex [$icwPicker gettags current] 1]
    scan $color "%d-%d-%d" r g b

    # Detroy the color picker dlog.
    destroy .wwColorPicker

    # Call the callback function.
    $gColorPickerCB $r $g $b
}

proc CreateColorPickerWindow { iCallbuckFunction } {

    # Make a new window, put a color picker inside it with our given
    # callback function, and pack it.
    toplevel .wwColorPicker 
    wm title .wwColorPicker "Choose a color..."
    tkm_MakeNormalLabel .wwColorPicker.lwInstructions "Chose a color..."
    CreateColorPicker .wwColorPicker.cpwColor $iCallbuckFunction
    pack .wwColorPicker.lwInstructions .wwColorPicker.cpwColor \
	-side top
}

proc DoFileDlog { which } {
    global tDlogSpecs
    tkm_DoFileDlog $tDlogSpecs($which)
}

proc DoSwapSurfaceFieldsDlog {} {

    global gDialog
    global glSwapField gaSwapFieldInfo

    set wwDialog .wwSwapSurfaceFieldsDlog

    # try to create the dlog...
    if { [Dialog_Create $wwDialog "Swap Surface Fields" {-borderwidth 10}] } {

  set fwMain             $wwDialog.fwMain
  set fwSwapField1       $fwMain.fwSwapField1
  set fwSwapField2       $fwMain.fwSwapField2
  set fwButtons          $wwDialog.fwButtons

  frame $fwMain

  tixOptionMenu $fwSwapField1 -label "Swap" \
    -variable swapField1 \
    -options {
      label.anchor e
      label.width 5
      menubutton.width 8
  }
  
  tixOptionMenu $fwSwapField2 -label "with" \
    -variable swapField2 \
    -options {
      label.anchor e
      label.width 5
      menubutton.width 8
  }

  foreach field $glSwapField {
      $fwSwapField1 add command $gaSwapFieldInfo($field,index) \
        -label $gaSwapFieldInfo($field,label)
      $fwSwapField2 add command $gaSwapFieldInfo($field,index) \
        -label $gaSwapFieldInfo($field,label)
  }
  
  # buttons.
  tkm_MakeApplyCloseButtons $fwButtons $wwDialog \
    { swap_vertex_fields $swapField1 $swapField2 } {}

  pack $fwSwapField1 $fwSwapField2 \
    -side left

  pack $fwMain $fwButtons \
    -side top       \
    -expand yes     \
    -fill x         \
    -padx 5         \
    -pady 5
    }
}

proc DoConfigLightingDlog {} {

    global gDialog
    global gaLinkedVars

    set wwDialog .wwConfigLightingDlog

    UpdateLinkedVarGroup scene

    if { [Dialog_Create $wwDialog "Configure Lighting" {-borderwidth 10}] } {

  set fwMain             $wwDialog.fwMain
  set fwLights           $wwDialog.fwLights
  set fwBrightness       $wwDialog.fwBrightness
  set fwButtons          $wwDialog.fwButtons

  frame $fwMain

  # sliders for lights 0 - 3
  tkm_MakeSliders $fwLights { \
    { {"Light 0" ""} gaLinkedVars(light0) 0 1 200 {} 1 0.1} \
    { {"Light 1" ""} gaLinkedVars(light1) 0 1 200 {} 1 0.1} \
    { {"Light 2" ""} gaLinkedVars(light2) 0 1 200 {} 1 0.1} \
    { {"Light 3" ""} gaLinkedVars(light3) 0 1 200 {} 1 0.1} }

  # slider for brightness offset
  tkm_MakeSliders $fwBrightness { \
    { {"Brightness" ""} gaLinkedVars(offset) 0 1 100 {} 1 0.1} }

  # buttons.
  tkm_MakeApplyCloseButtons $fwButtons $wwDialog \
    { SendLinkedVarGroup scene; \
    do_lighting_model $gaLinkedVars(light0) $gaLinkedVars(light1) $gaLinkedVars(light2) $gaLinkedVars(light3) $gaLinkedVars(offset); \
    UpdateAndRedraw } {}

  pack $fwMain $fwLights $fwBrightness $fwButtons \
    -side top       \
    -expand yes     \
    -fill x         \
    -padx 5         \
    -pady 5
    }
}

proc FillOverlayLayerMenu { iowOverlay {iSelectField none} } {
    global gaScalarValueID
    global gsaLabelContents
    global gaLinkedVars
    global glLabel
    
    $iowOverlay config -disablecallback 1

    set lEntries [$iowOverlay entries]
    foreach entry $lEntries { 
	$iowOverlay delete $entry
    }
    
    set nValueIndex 0
    while { [info exists gaScalarValueID($nValueIndex,label)] } {
	$iowOverlay add command $nValueIndex \
	    -label $gsaLabelContents($gaScalarValueID($nValueIndex,label),name)
	incr nValueIndex
    }
    
    $iowOverlay config -disablecallback 0

    switch $iSelectField {
	none {}
	current { $iowOverlay config -value $gaLinkedVars(currentvaluefield) }
	first-empty { 
	    # get the indicies of the first and last overlay labels
	    # and then get the field names.
	    set nFirst [lsearch $glLabel kLabel_Val]
	    set nLast [lsearch $glLabel kLabel_StdError]
	    set lFields [lrange $glLabel $nFirst $nLast]
	    set nIndex 0
	    # look through our label contents and search for the
	    # string "empty" in them. if we find one, use that as the
	    # first empty selection.
	    foreach field $lFields {
		set name $gsaLabelContents($field,name)
		set bMatch [string match *empty* $name]
		if { $bMatch } {
		    $iowOverlay config -value $nIndex
		    break
		}
		incr nIndex
	    }
	    # if we didn't match anything, set the index to 0.
	    if { ! $bMatch } {
		$iowOverlay config -value 0
	    }
	}
    }
}

proc SetMin { iWidget inThresh } {
    global gaLinkedVars

    # set the linked value to the abs of the value on which they
    # clicked. draw a new line on this value.
    set gaLinkedVars(fthresh) [expr abs($inThresh)]
    $iWidget marker create line \
	-coords [list $gaLinkedVars(fthresh) -Inf $gaLinkedVars(fthresh) Inf] \
	-name thresh -outline red
    set negMin [expr -$gaLinkedVars(fthresh)]
    $iWidget marker create line \
	-coords [list $negMin -Inf $negMin Inf] \
	-name negthresh -outline red
    
}

proc SetMid { iWidget inThresh ibUpdateSlope } {
    global gaLinkedVars

    # set the linked value to the abs of the value on which they
    # clicked. draw a new line on this value.
    set gaLinkedVars(fmid) [expr abs($inThresh)]
    $iWidget marker create line \
	-coords [list $gaLinkedVars(fmid) -Inf $gaLinkedVars(fmid) Inf] \
	-name mid -outline blue
    set negMid [expr -$gaLinkedVars(fmid)]
    $iWidget marker create line \
	-coords [list $negMid -Inf $negMid Inf] \
	-name negmid -outline blue
    # calculate the slope if necessary.
    if { $ibUpdateSlope } {
	SetSlope $iWidget [expr 1.0 / ($gaLinkedVars(fthreshmax) - $gaLinkedVars(fmid))] 0
    }
    
}

proc SetMax { iWidget inThresh ibUpdateSlope } {
    global gaLinkedVars
 
    # set the linked value to the abs of the value on which they
    # clicked. draw a new line on this value.
    set gaLinkedVars(fthreshmax) [expr abs($inThresh)]
    $iWidget marker create line \
	-coords [list $gaLinkedVars(fthreshmax) -Inf $gaLinkedVars(fthreshmax) Inf] \
	-name max -outline green
    set negMax [expr -$gaLinkedVars(fthreshmax)]
    $iWidget marker create line \
	-coords [list $negMax -Inf $negMax Inf] \
	-name negmax -outline green
    # calculate the slope if necessary.
    if { $ibUpdateSlope } {
	SetSlope $iWidget [expr 1.0 / ($gaLinkedVars(fthreshmax) - $gaLinkedVars(fmid))] 0
    }
}

proc SetSlope { iWidget inSlope ibUpdateMax } {
    global gaLinkedVars

    # set the linked value to the value on which they clicked. draw a
    # new line on this value.
    set gaLinkedVars(fslope) $inSlope
    # calculate the max if necessary.
    if { $ibUpdateMax } {
	SetMax $iWidget [expr (1.0 / $gaLinkedVars(fslope)) + $gaLinkedVars(fmid)] 0
    }
}

proc UpdateHistogramData { iMin iMax iIncrement iNum ilData } {
    global gaHistogramData

    set gaHistogramData(min) $iMin
    set gaHistogramData(max) $iMax
    set gaHistogramData(increment) $iIncrement
    set gaHistogramData(numBars) $iNum
    set gaHistogramData(data) $ilData
}

proc DoConfigOverlayDisplayDlog {} {
    
    global gDialog
    global gaLinkedVars
    global gCopyFieldTarget
    global gbwHisto
    global gsHistoValue
    global gFDRRate
    
    set wwDialog .wwConfigOverlayDisplayDlog
    
    UpdateLinkedVarGroup overlay
    
    if { [Dialog_Create $wwDialog "Configure Overlay Display" {-borderwidth 10}] } {
	
	set fwMain             $wwDialog.fwMain
	set fwDisplay          $wwDialog.fwDisplay
	set fwPlane            $wwDialog.fwPlane
	set fwColorScale       $wwDialog.fwColorScale
	set fwFlags            $wwDialog.fwFlags
	set fwHisto            $wwDialog.fwHisto
	set fwButtons          $wwDialog.fwButtons
	
	frame $fwMain

	# Overlay display options
	set lwDisplay $fwDisplay.lwDisplay
	set fwOpacity $fwDisplay.fwOpacity

	frame $fwDisplay -relief ridge -border 2

	label $lwDisplay -text "Overlay Display" -font [tkm_GetLabelFont]

	tkm_MakeSliders $fwOpacity [list \
	  [list {"Opacity"} gaLinkedVars(falpha) \
		0 1 100 {} 1 0.1 horizontal ] ]

	grid $lwDisplay -column 0 -row 0
	grid $fwOpacity -column 0 -row 1 -sticky news

	set nMaxCondition [expr $gaLinkedVars(fnumconditions) - 1]
	if { $nMaxCondition < 0 } {
	    set nMaxCondition 0
	}
	set nMaxTimePoint [expr $gaLinkedVars(fnumtimepoints) - 1]
	if { $nMaxTimePoint < 0 } {
	    set nMaxTimePoint 0
	}

	# The plane of data we're viewing.
	set lwPlane            $fwPlane.lwPlane
	set fwTimePoint        $fwPlane.fwTimePoint
	set fwCondition        $fwPlane.fwCondition

	frame $fwPlane -relief ridge -border 2

	label $lwPlane -text "Location" -font [tkm_GetLabelFont]

	tkm_MakeEntryWithIncDecButtons \
	    $fwTimePoint "Time Point (0-$nMaxTimePoint)" \
	    gaLinkedVars(ftimepoint) \
	    {} 1 "0 $nMaxTimePoint"
	
	tkm_MakeEntryWithIncDecButtons \
	    $fwCondition "Condition (0-$nMaxCondition)" \
	    gaLinkedVars(fcondition) \
	    {} 1 "0 $nMaxCondition"

	grid $lwPlane     -column 0 -row 0 -columnspan 2
	grid $fwTimePoint -column 0 -row 1 -sticky w
	grid $fwCondition -column 1 -row 1 -sticky w

	# color scale
	if { 0 } {
	tkm_MakeRadioButtons $fwColorScale y "Color Scale" \
	    gaLinkedVars(colscale) { 
		{ text "Color Wheel (Complex)" 0 {} }
		{ text "RYGB Wheel (Complex)" 8 {} }
		{ text "Two Condition Green Red (Complex)" 4 {} }
		{ text "Green to Red (Signed)" 7 {} }
		{ text "Heat Scale (Stat, Positive)" 1 {} }
		{ text "Blue to Red (Signed)" 6 {} }
		{ text "Not Here (Signed)" 9 {} } }
	}


	set lwColorScale  $fwColorScale.lwColorScale
	set lwSingle      $fwColorScale.lwSingle
	set lwComplex     $fwColorScale.lwComplex
	set cbwColorWheel $fwColorScale.cbwColorWheel
	set cbwRYGBWheel  $fwColorScale.cbwRYGBWheel
	set cbwTwoCond    $fwColorScale.cbwTwoCond
	set cbwGreenRed   $fwColorScale.cbwGreenRed
	set cbwHeat       $fwColorScale.cbwHeat
	set cbwBlueRed    $fwColorScale.cbwBlueRed

	frame $fwColorScale -relief ridge -border 2

	label $lwColorScale -text "Color Scale" -font [tkm_GetLabelFont]
	label $lwSingle -text "Single" -font [tkm_GetLabelFont]
	label $lwComplex -text "Complex" -font [tkm_GetLabelFont]

	radiobutton $cbwColorWheel -font [tkm_GetNormalFont] \
	    -variable gaLinkedVars(colscale) -value 0 -text "Color Wheel"
	radiobutton $cbwRYGBWheel -font [tkm_GetNormalFont] \
	    -variable gaLinkedVars(colscale) -value 8 -text "RYGB Wheel"
	radiobutton $cbwTwoCond -font [tkm_GetNormalFont] \
	    -variable gaLinkedVars(colscale) -value 4 -text "Two Cond G/R"

	radiobutton $cbwGreenRed -font [tkm_GetNormalFont] \
	    -variable gaLinkedVars(colscale) -value 7 -text "Green Red"
	radiobutton $cbwHeat -font [tkm_GetNormalFont] \
	    -variable gaLinkedVars(colscale) -value 1 -text "Heat"
	radiobutton $cbwBlueRed -font [tkm_GetNormalFont] \
	    -variable gaLinkedVars(colscale) -value 6 -text "Blue Red"

	grid $lwColorScale  -column 0 -row 0 -columnspan 4
	grid $lwSingle      -column 0 -row 1 -sticky e
	grid $cbwGreenRed   -column 1 -row 1 -sticky w
	grid $cbwHeat       -column 2 -row 1 -sticky w
	grid $cbwBlueRed    -column 3 -row 1 -sticky w
	grid $lwComplex     -column 0 -row 2 -sticky e
	grid $cbwColorWheel -column 1 -row 2 -sticky w
	grid $cbwRYGBWheel  -column 2 -row 2 -sticky w
	grid $cbwTwoCond    -column 3 -row 2 -sticky w
	
	# checkboxes for truncate, inverse, reverse phase, complex values
	if { 0 } {
	tkm_MakeCheckboxes $fwFlags y {
	    { text "Truncate" gaLinkedVars(truncphaseflag) {} }
	    { text "Inverse" gaLinkedVars(invphaseflag) {} }
	    { text "Reverse" gaLinkedVars(revphaseflag) {} }
	    { text "Complex" gaLinkedVars(complexvalflag) {} } }
	} 

	set lwOptions        $fwFlags.lwOptions
        set cbwTruncate      $fwFlags.cbwTruncate
        set cbwInverse       $fwFlags.cbwInverse
        set cbwReverse       $fwFlags.cbwReverse
        set cbwComplex       $fwFlags.cbwComplex
        set cbwIgnoreZeroes  $fwFlags.cbwIgnoreZeroes

        frame $fwFlags -relief ridge -border 2

	label $lwOptions -text "Display Options" -font [tkm_GetLabelFont]

	checkbutton $cbwTruncate \
	    -variable gaLinkedVars(truncphaseflag) \
	    -text "Truncate" \
	    -font [tkm_GetNormalFont] \
	    -command {SendLinkedVarGroup overlay}
	checkbutton $cbwInverse \
	    -variable gaLinkedVars(invphaseflag) \
	    -text "Inverse" \
	    -font [tkm_GetNormalFont] \
	    -command {SendLinkedVarGroup overlay}
	checkbutton $cbwReverse \
	    -variable gaLinkedVars(revphaseflag) \
	    -text "Reverse" \
	    -font [tkm_GetNormalFont]
	checkbutton $cbwComplex \
	    -variable gaLinkedVars(complexvalflag) \
	    -text "Complex" \
	    -font [tkm_GetNormalFont]
	checkbutton $cbwIgnoreZeroes \
	    -variable gaLinkedVars(ignorezeroesinhistogramflag) \
	    -text "Ignore Zeroes in Histogram" \
	    -font [tkm_GetNormalFont]

	grid $lwOptions   -column 0 -row 0 -columnspan 4
	grid $cbwTruncate -column 0 -row 1 -stick w
	grid $cbwInverse  -column 1 -row 1 -stick w
	grid $cbwReverse  -column 2 -row 1 -stick w
	grid $cbwComplex  -column 3 -row 1 -stick w
	grid $cbwIgnoreZeroes  -column 0 -row 2 -columnspan 4

	# create the histogram frame and subunits
	frame $fwHisto -relief ridge -border 2

	set lwHisto   $fwHisto.lwHisto
	set gbwHisto  $fwHisto.bwHisto
	set fwThresh  $fwHisto.fwThresh
	set ewMin     $fwThresh.ewMin
	set ewMid     $fwThresh.ewMid
	set ewMax     $fwThresh.ewMax
	set ewSlope   $fwThresh.ewSlope
	set fwValueOffset $fwHisto.fwValueOffset
	set ewValue   $fwValueOffset.ewValue
	set ewOffset  $fwValueOffset.ewOffset
	set fwCopy    $fwHisto.fwCopy
	set bwCopy    $fwCopy.bwCopy
	set owTarget  $fwCopy.owTarget
	set cbwAll    $fwCopy.cbwAll
	set fwFDR     $fwHisto.fwFDR
	set bwFDR     $fwFDR.bwFDR
	set cbFDRMarked $fwFDR.cbMarked
	set ewFDRRate $fwFDR.ewFDRRate

	label $lwHisto -text "Threshold" -font [tkm_GetLabelFont]

	# create a barchart object. configure it to hide the legend
	# and rotate the labels on the x axis.
	blt::barchart $gbwHisto -width 200 -height 200
	$gbwHisto legend config -hide yes
	$gbwHisto axis config x -rotate 90.0 -stepsize 5

	# bind the button pressing events to set the thresholds.
	bind $gbwHisto <ButtonPress-1> \
	    { SetMin %W [%W axis invtransform x %x] }
	bind $gbwHisto <ButtonPress-2> \
	    { SetMid %W [%W axis invtransform x %x] 1 }
	bind $gbwHisto <ButtonPress-3> \
	    { SetMax %W [%W axis invtransform x %x] 1 }

	# bind the bututon pressing events that do the zooming.
	bind $gbwHisto <Control-ButtonPress-1>   { Histo_RegionStart %W %x %y }
	bind $gbwHisto <Control-B1-Motion>      { Histo_RegionMotion %W %x %y }
	bind $gbwHisto <Control-ButtonRelease-1> { Histo_RegionEnd %W %x %y }
	bind $gbwHisto <Control-ButtonRelease-3> { Histo_Unzoom %W }

	# when the mouse moves over the histogram, find the closest
	# bar and print its data to the active label. the y is the
	# height of the bar.
	bind $gbwHisto <Motion> {
	    if { [$gbwHisto element closest %x %y aFound -halo 1] } { 
		set gsHistoValue "Value $aFound(x) Count [expr round($aFound(y))]"
	    } 
	}
		
	# make the entries for the threshold values.
	frame $fwThresh

	tkm_MakeEntry $ewMin "Min" gaLinkedVars(fthresh) 6 \
	    {SetMin $gbwHisto $gaLinkedVars(fthresh)}
	tkm_MakeEntry $ewMid "Mid" gaLinkedVars(fmid) 6 \
	    {SetMid $gbwHisto $gaLinkedVars(fmid) 1}
	tkm_MakeEntry $ewMax "Max" gaLinkedVars(fthreshmax) 6 \
	    {SetMax $gbwHisto $gaLinkedVars(fthreshmax) 1}
	tkm_MakeEntry $ewSlope "Slope" gaLinkedVars(fslope) 6 \
	    {SetSlope $gbwHisto $gaLinkedVars(fslope) 1}
	
	# color the entries to match the lines in the histogram.
	$ewMin.lwLabel config -fg red 
	$ewMid.lwLabel config -fg blue
	$ewMax.lwLabel config -fg green

	frame $fwValueOffset

	# this is the field that will show the current value of the
	# graph.
	tkm_MakeActiveLabel $ewValue "Current value: " gsHistoValue 20
	set gsHistoValue "Move mouse over graph bar"

	# The offset field.
	tkm_MakeEntry $ewOffset "Offset" gaLinkedVars(foffset) 6 {}

	pack $ewValue -side left -expand yes -fill x
	pack $ewOffset -side left

	# set initial values for the histogram.
	SetMin $gbwHisto $gaLinkedVars(fthresh)
	SetMid $gbwHisto $gaLinkedVars(fmid) 0
	SetSlope $gbwHisto $gaLinkedVars(fslope) 1
	
	pack $ewMin $ewMid $ewMax $ewSlope \
	    -side left

	# make the button and menu that the user can use to copy the
	# threshold settings to another layer.
	frame $fwCopy
	tkm_MakeButtons $bwCopy \
	    [list \
	     [list text "Copy Settings to Layer" \
	      {sclv_copy_view_settings_from_current_field $gCopyFieldTarget}]]
	tixOptionMenu $owTarget \
	    -variable $gCopyFieldTarget
	FillOverlayLayerMenu $owTarget current
	set gCopyFieldTarget 0

	# if checked, will copy settings to all other layers
	checkbutton $cbwAll \
	    -variable gbOverlayApplyToAll \
	    -text "Apply changes to all layers" \
	    -font [tkm_GetNormalFont]

	# button and field for setting the threshold using FDR.
	frame $fwFDR
	tkm_MakeButtons $bwFDR \
	    [list \
		 [list text "Set Threshold Using FDR" \
		      {sclv_set_current_threshold_using_fdr $gFDRRate $gbFDRMarked}]]

	tkm_MakeEntry $ewFDRRate "Rate" gFDRRate 4 {}

	checkbutton $cbFDRMarked \
	    -variable gbFDRMarked \
	    -text "Only marked" \
	    -font [tkm_GetNormalFont]
	
	pack $bwFDR $ewFDRRate $cbFDRMarked \
	    -side left \
	    -expand yes \
	    -fill x
	
	grid $bwCopy   -column 0 -row 0 
	grid $owTarget -column 1 -row 0 -sticky news
	grid $cbwAll   -column 0 -row 1 -columnspan 2
	grid columnconfigure $fwCopy 0 -weight 0
	grid columnconfigure $fwCopy 1 -weight 1

	pack $lwHisto -side top
	pack $gbwHisto -fill both -expand yes
	pack $fwThresh -side top
	pack $fwValueOffset -side top -expand yes -fill x
	pack $fwCopy -side top  -expand yes -fill x
	pack $fwFDR  -side top  -expand yes -fill x

	# buttons.
	tkm_MakeDialogButtons $fwButtons $wwDialog [list \
		[list Apply { SendLinkedVarGroup overlay; 
		    SetOverlayTimepointAndCondition;
		    if { $gbOverlayApplyToAll } {
			sclv_copy_all_view_settings_from_current_field 
		    };
		    RequestOverlayInfoUpdate; }] \
		[list Close {}] \
		[list Help {ShowOverlayHelpWindow}] \
	]
	
	pack $fwMain $fwDisplay $fwPlane $fwColorScale \
	    $fwFlags $fwHisto $fwFDR $fwButtons \
	    -side top       \
	    -expand yes     \
	    -fill x         \
	    -padx 5         \
	    -pady 5

	# now update it so that we have the current info and stuff.
	UpdateOverlayDlogInfo
    }
}

proc ShowOverlayHelpWindow {} {
    global gDialog

    set wwDialog .wwConfigOverlayHelpDlog
    if { [Dialog_Create $wwDialog "Overlay Help" {-borderwidth 10}] } {

	set fwMain    $wwDialog.fwMain
	set twHelp    $fwMain.twHelp
	set fwButtons $wwDialog.fwButtons

	frame $fwMain

	tixScrolledText $twHelp -scrollbar y
	[$twHelp subwidget text] config -wrap word -relief ridge -bd 1
	[$twHelp subwidget text] insert end "At the top of the window, there are two entry widgets with up/down arrows next to them. If there are multiple time points or conditions in the overlay data, you can change the time point and condition of the overlay volume with these controls. You can see the total range of each variable in the parentheses next to the label.

Next is the color scale. You can change the color scheme in which the overlay layers are drawn. Some do not work with simple scalar overlays and need multiple values loaded, but don't be afraid to experiment.

Next are four boolean options for configuring the data display. Truncate will only show positive values if checked. If Inverse is checked, negative values will be shown in the positive value colors and vice versa. Reverse will switch the sign of the data; with Reverse and Truncate checked, you can show only the negative data. Complex is for special configurations involving multiple overlay layers.

The histogram can be used to set the color scale threshold values graphically. If your data has a wide range of values, you may first want to zoom in on the most populated area of the histogram. Hold down the control key and drag with mouse button one (usually left) to select the range on which you want to zoom in. Control-mouse-three (usually right) to unzoom.

You will see three vertical lines: a red one representing the minimum threshold, a blue one representing the midpoint, and a red one representing the maximum threshold. (Note that the maximum threshold does not represent a cut-off point above which no values will be drawn, but the point at which all values above will be drawn in the maximum 'hot' color.) You can set the the thresholds by clicking in the histogram area; set the min threshold with mouse button one (usually left), the midpoint with button two (middle), and the maximum point with button three (usually right). The colors of the bars in the histogram represent the color in which those values will be drawn on the surface. Note that since the threshold is symetrical, a click on a negative value is treated as a click on the absolute value.

There are numerical representations of these values beneath the histogram. Note that the slope is automatically calculated when the mid point or maximum point are set. You can enter any of these four values manually. When you enter a new midpoint, maximum, or slope, press return to update the other values.

The Copy Settings to Layer button can be used to copy the current threshold settings (all four values) to another layer. Select the layer to which you want to copy these settings in the pull-down menu. You can also automatically set the settings in all layers to the settings in the current layer whenever you hit the apply button by checking the \"Apply changes to all layers\" checkbox.

You always need to click the Apply button to apply any of these changes to the actually display. You can also press the space bar as a shortcut (but only when the cursor is not active in any of the numerical fields)."

	pack $twHelp \
	    -fill both \
	    -expand yes

	tkm_MakeDialogButtons $fwButtons $wwDialog [list \
		[list Close {}] \
	]

	pack $fwMain $fwButtons \
	    -side top \
	    -expand yes \
	    -fill x \
	    -pady 5
    }
}

proc UpdateOverlayDlogInfo {} {
    global gaLinkedVars
    global gaHistogramData
    global gbwHisto
    global knMinWidthPerTick

    # change the time point and condition range labels
    catch {
	set nMaxCondition [expr $gaLinkedVars(fnumconditions) - 1]
	if { $nMaxCondition < 0 } {
	    set nMaxCondition 0
	}
	set nMaxTimePoint [expr $gaLinkedVars(fnumtimepoints) - 1]
	if { $nMaxTimePoint < 0 } {
	    set nMaxTimePoint 0
	}
	.wwConfigOverlayDisplayDlog.fwTimePoint.control config \
		-label "Time Point (0-$nMaxTimePoint)"
	.wwConfigOverlayDisplayDlog.fwCondition.control config \
		-label "Condition (0-$nMaxCondition)"
    }
    
    # set the histogram data
    set err [catch {
	set names [$gbwHisto element names]
	foreach name $names {
	    $gbwHisto element delete $name
	}
	set nValue 0
	for { set x $gaHistogramData(min) } \
	    { $x < $gaHistogramData(max) } \
	    { set x [expr $x + $gaHistogramData(increment)] } {
		set lColors [sclv_get_normalized_color_for_value $x]
		set color [format "#%.2x%.2x%.2x" \
			       [expr round([lindex $lColors 0] * 255.0)] \
			       [expr round([lindex $lColors 1] * 255.0)] \
			       [expr round([lindex $lColors 2] * 255.0)]]
		$gbwHisto element create "Value $x" \
		    -xdata $x -ydata [lindex $gaHistogramData(data) $nValue] \
		    -foreground $color -borderwidth 0 \
		    -barwidth $gaHistogramData(increment)
		incr nValue
	    }
	if {$gaHistogramData(zoomed)} {
	    $gbwHisto axis config x \
	      -min $gaHistogramData(zoomedmin) -max $gaHistogramData(zoomedmax)
	} else {
	    $gbwHisto axis config x \
		-min $gaHistogramData(min) -max $gaHistogramData(max)
	}	

	# set the lines in the histogram
	SetMin $gbwHisto $gaLinkedVars(fthresh)
	SetMid $gbwHisto $gaLinkedVars(fmid) 0
	SetSlope $gbwHisto $gaLinkedVars(fslope) 1

	# calculate a good width for the ticks.
	set width [$gbwHisto cget -width]
	set widthPerTick [expr $width / $gaHistogramData(numBars)]
	if { $widthPerTick < $knMinWidthPerTick } {
	    set numMarks [expr $width / $knMinWidthPerTick]
	    set widthPerTick [expr $gaHistogramData(numBars) / $numMarks]
	    $gbwHisto axis configure x -stepsize $widthPerTick
	} else {
	    set widthPerTick $gaHistogramData(increment)
	    $gwGraph axis configure x -stepsize $widthPerTick
	}
    } result]
#    if {$err != 0} {puts "ERROR updating histo: $result"}

    # rebuild the copy target menu
    catch {
      FillOverlayLayerMenu .wwConfigOverlayDisplayDlog.fwHisto.fwCopy.owTarget current
    }
}

proc Histo_Zoom { iwHisto inX1 inX2 } {
    global gaHistogramData
    if { $inX1 < $inX2 } {
	$iwHisto axis configure x -min $inX1 -max $inX2
    } elseif { $inX1 > $inX2 } {
	$iwHisto axis configure x -min $inX2 -max $inX1
    }
    set gaHistogramData(zoomed) 1
}
proc Histo_Unzoom { iwHisto } {
    global gaHistogramData
    $iwHisto axis configure x y -min {} -max {}
    set gaHistogramData(zoomed) 0
}
proc Histo_RegionStart { iwHisto inX inY } {
    global gnRegionStart
    $iwHisto marker create line -coords { } -name zoomBox \
      -dashes dash -xor yes
    set gnRegionStart(x) [$iwHisto axis invtransform x $inX]
}
proc Histo_RegionMotion { iwHisto inX inY } {
    global gnRegionStart
    set nX [$iwHisto axis invtransform x $inX]
    $iwHisto marker configure zoomBox -coords \
	[list \
	 $gnRegionStart(x) -Inf \
	 $gnRegionStart(x) Inf \
	 $nX Inf \
	 $nX -Inf \
	 $gnRegionStart(x) -Inf]
}
proc Histo_RegionEnd { iwHisto inX inY } {
    global gnRegionStart
    global gaHistogramData
    $iwHisto marker delete zoomBox
    set nX [$iwHisto axis invtransform x $inX]
    Histo_Zoom $iwHisto $gnRegionStart(x) $nX
    set gaHistogramData(zoomedmin) $gnRegionStart(x)
    set gaHistogramData(zoomedmax) $nX
}

proc SetOverlayTimepointAndCondition {} {
    global gaLinkedVars
    global gbwHisto
    SendLinkedVarGroup overlay
    sclv_set_current_timepoint $gaLinkedVars(ftimepoint) \
	$gaLinkedVars(fcondition);
    UpdateAndRedraw 
}

proc SetOverlayField {} {
    global gaLinkedVars
    global gbwHisto
    sclv_set_current_field $gaLinkedVars(currentvaluefield)
    catch { Histo_Unzoom $gbwHisto }
    SendLinkedVarGroup view
    UpdateAndRedraw 
}


proc DoConfigCurvatureDisplayDlog {} {

    global gDialog
    global gaLinkedVars
    UpdateLinkedVarGroup curvature

    set wwDialog .wwConfigCurvatureDisplayDlog

    if { [Dialog_Create $wwDialog "Configure Curvature Display" {-borderwidth 10}] } {

  set fwMain             $wwDialog.fwMain
  set lfwThreshold       $wwDialog.lfwThreshold
  set fwGrayscale        $wwDialog.fwGrayscale
  set fwButtons          $wwDialog.fwButtons

  frame $fwMain

  # fields for slope and midpoint
  tixLabelFrame $lfwThreshold \
    -label "Threshold" \
    -labelside acrosstop \
    -options { label.padX 5 }

  set fwThresholdSub     [$lfwThreshold subwidget frame]
  set fwThresholdSliders $fwThresholdSub.fwThresholdSliders
  set fwThresholdSlope   $fwThresholdSub.fwThresholdSlope

  tkm_MakeSliders $fwThresholdSliders [list \
    [list {"Threshold midpoint"} gaLinkedVars(cmid) \
    $gaLinkedVars(cmin) $gaLinkedVars(cmax) 100 {} 1 0.05 ]]
  tkm_MakeEntry $fwThresholdSlope "Threshold slope" \
    gaLinkedVars(cslope) 6

  pack $fwThresholdSliders $fwThresholdSlope \
    -side top \
    -anchor w

  tkm_MakeCheckboxes $fwGrayscale x {
      { text "Binary gray" gaLinkedVars(forcegraycurvatureflag) {} 
	  "Always draw curvature in binary gray" }
  }

  # buttons.
  tkm_MakeApplyCloseButtons $fwButtons $wwDialog \
    { SendLinkedVarGroup curvature; UpdateAndRedraw } {}

  pack $fwMain $lfwThreshold $fwGrayscale $fwButtons \
    -side top       \
    -expand yes     \
    -fill x         \
    -padx 5         \
    -pady 5
    }
}

proc DoConfigPhaseEncodedDataDisplayDlog {} {

    global gDialog
    global gaLinkedVars
    UpdateLinkedVarGroup phase

    set wwDialog .wwConfigPhaseEncodedDataDisplayDlog

    if { [Dialog_Create $wwDialog "Configure Phase Encoded Data Display" {-borderwidth 10}] } {

  set fwMain             $wwDialog.fwMain
  set fwCycles           $wwDialog.fwCycles
  set fwOffset           $wwDialog.fwOffset
  set fwButtons          $wwDialog.fwButtons

  frame $fwMain
  
  # fields for angle cycles and angle offset
  tkm_MakeEntry $fwCycles "Angle Cycles: " gaLinkedVars(angle_cycles) 6 
  tkm_MakeEntry $fwOffset "Angle Offset: " gaLinkedVars(angle_offset) 6 

  # buttons.
  tkm_MakeApplyCloseButtons $fwButtons $wwDialog \
    { SendLinkedVarGroup phase; UpdateAndRedraw } {}

  pack $fwMain $fwCycles $fwOffset $fwButtons \
    -side top       \
    -expand yes     \
    -fill x         \
    -padx 5         \
    -pady 5
    }
}

proc DoLoadOverlayDlog {} {

    global gDialog gaLinkedVars
    global gaScalarValueID gsaLabelContents
    global glShortcutDirs
    global sFileName

    set wwDialog .wwLoadOverlayDlog

    set knWidth 400
    
    # try to create the dlog...
    if { [Dialog_Create $wwDialog "Load Overlay" {-borderwidth 10}] } {
	
	set fwFile             $wwDialog.fwFile
	set fwFileNote         $wwDialog.fwFileNote
	set fwRegistration     $wwDialog.fwRegistration
	set fwRegistrationNote $wwDialog.fwRegistrationNote
	set fwField            $wwDialog.fwField
	set fwFieldNote        $wwDialog.fwFieldNote
	set fwButtons          $wwDialog.fwButtons
	
	set sFileName [GetDefaultLocation LoadOverlay]
	tkm_MakeFileSelector $fwFile "Load Overlay:" sFileName \
	    [list GetDefaultLocation LoadOverlay] \
	    $glShortcutDirs
	
	[$fwFile.ew subwidget entry] icursor end

	tkm_MakeSmallLabel $fwFileNote "Values file (.w), binary volume file (.bfloat/.bshort/.hdr), COR-.info file, or other" 400
	
	set sRegistrationFileName [GetDefaultLocation LoadOverlayRegistration]
	tkm_MakeFileSelector $fwRegistration "Use Registration:" \
	    sRegistrationFileName \
	    [list GetDefaultLocation LoadOverlayRegistration] \
	    $glShortcutDirs
	
	[$fwRegistration.ew subwidget entry] icursor end

	tkm_MakeSmallLabel $fwRegistrationNote "Optional register.dat file. Leave blank for .v files or to use register.dat in same directory as volume" 400
	
	tixOptionMenu $fwField -label "Into Field:" \
	    -variable nFieldIndex \
	    -options {
		label.anchor e
		label.width 5
		menubutton.width 8
	    }
	
	tkm_MakeSmallLabel $fwFieldNote "The layer into which to load the values" 400
	FillOverlayLayerMenu $fwField first-empty
	
	# buttons.
        tkm_MakeCancelOKButtons $fwButtons $wwDialog { 
	    SetDefaultLocation LoadOverlay $sFileName;
	    SetDefaultLocation LoadOverlayRegistration $sRegistrationFileName;
	    DoLoadFunctionalFile $nFieldIndex $sFileName $sRegistrationFileName
	}
	
	pack $fwFile $fwFileNote $fwRegistration $fwRegistrationNote \
	    $fwField $fwFieldNote $fwButtons \
	    -side top       \
	    -expand yes     \
	    -fill x         \
	    -padx 5         \
	    -pady 5
	
	# after the next idle, the window will be mapped. set the min
	# width to our width and the min height to the mapped height.
	after idle [format {
	    update idletasks
	    wm minsize %s %d [winfo reqheight %s]
	    wm geometry %s =%dx[winfo reqheight %s]
	} $wwDialog $knWidth $wwDialog $wwDialog $knWidth $wwDialog] 
    }
}

proc DoLoadTimeCourseDlog {} {

    global gDialog gaLinkedVars
    global glShortcutDirs
    global sFileName

    set wwDialog .wwLoadTimeCourseDlog

    set knWidth 400
    
    # try to create the dlog...
    if { [Dialog_Create $wwDialog "Load Time Course" {-borderwidth 10}] } {
	
	set fwFile             $wwDialog.fwFile
	set fwFileNote         $wwDialog.fwFileNote
	set fwRegistration     $wwDialog.fwRegistration
	set fwRegistrationNote $wwDialog.fwRegistrationNote
	set fwButtons          $wwDialog.fwButtons
	
	set sFileName [GetDefaultLocation LoadTimeCourse]
	tkm_MakeFileSelector $fwFile "Load TimeCourse:" sFileName \
	    [list GetDefaultLocation LoadTimeCourse] \
	    $glShortcutDirs
	
	[$fwFile.ew subwidget entry] icursor end

	tkm_MakeSmallLabel $fwFileNote "One of the binary volume files (.bfloat/.bshort/.hdr)" 400
	
	set sRegistrationFileName [GetDefaultLocation LoadTimeCourseRegistration]
	tkm_MakeFileSelector $fwRegistration "Use Registration:" \
	    sRegistrationFileName \
	    [list GetDefaultLocation LoadOverlayRegistration] \
	    $glShortcutDirs
	
	[$fwRegistration.ew subwidget entry] icursor end

	tkm_MakeSmallLabel $fwRegistrationNote "Optional register.dat file. Leave blank for .v files or to use register.dat in same directory as volume" 400
	
	# buttons.
        tkm_MakeCancelOKButtons $fwButtons $wwDialog {
	    SetDefaultLocation LoadTimeCourse $sFileName;
	    SetDefaultLocation LoadTimeCourseRegistration \
		$sRegistrationFileName
	    DoLoadFunctionalFile -1 $sFileName $sRegistrationFileName
	}
	
	pack $fwFile $fwFileNote $fwRegistration $fwRegistrationNote \
	    $fwButtons \
	    -side top       \
	    -expand yes     \
	    -fill x         \
	    -padx 5         \
	    -pady 5
	
	# after the next idle, the window will be mapped. set the min
	# width to our width and the min height to the mapped height.
	after idle [format {
	    update idletasks
	    wm minsize %s %d [winfo reqheight %s]
	    wm geometry %s =%dx[winfo reqheight %s]
	} $wwDialog $knWidth $wwDialog $wwDialog $knWidth $wwDialog] 
    }
}

proc DoLoadFunctionalFile { inField isFileName isRegistrationFileName } {

    global val

    set sExtension [file extension $isFileName]
    if { $inField == -1 } {
	func_load_timecourse $isFileName $isRegistrationFileName
    } else {
	if { $sExtension == ".bfloat" || $sExtension == "bshort" } {
	    sclv_read_bfile_values $inField $isFileName $isRegistrationFileName
	} else {
	    set val $isFileName
	    sclv_read_binary_values $inField
	}
	sclv_copy_view_settings_from_field $inField 0
	OverlayLayerChanged
    }
    UpdateAndRedraw
}

proc DoSaveValuesAsDlog {} {

    global gDialog
    global gaScalarValueID gsaLabelContents
    global glShortcutDirs
    global sFileName

    set knWidth 400
    set wwDialog .wwSaveValuesAs

    # try to create the dlog...
    if { [Dialog_Create $wwDialog "Save Values As" {-borderwidth 10}] } {
	
	set fwFile             $wwDialog.fwFile
	set fwFileNote         $wwDialog.fwFileNote
	set fwField            $wwDialog.fwField
	set fwFieldNote        $wwDialog.fwFieldNote
	set fwButtons          $wwDialog.fwButtons
	
	set sFileName [GetDefaultLocation SaveValuesAs]
	tkm_MakeFileSelector $fwFile "Save Values:" sFileName \
	    [list GetDefaultLocation SaveValuesAs] \
	    $glShortcutDirs
	
	[$fwFile.ew subwidget entry] icursor end

	tkm_MakeSmallLabel $fwFileNote "The file name of the values file to create" 400
	
	tixOptionMenu $fwField -label "From Field:" \
	    -variable nFieldIndex \
	    -options {
		label.anchor e
		label.width 5
		menubutton.width 8
	    }
	
	tkm_MakeSmallLabel $fwFieldNote "The layer to save" 400
	
	FillOverlayLayerMenu $fwField current
	
	# buttons.
        tkm_MakeCancelOKButtons $fwButtons $wwDialog \
	    {set val [ExpandFileName $sFileName kFileName_Surface]; 
		SetDefaultLocation SaveValuesAs $val;
		sclv_write_binary_values $nFieldIndex}
	
	pack $fwFile $fwFileNote $fwField $fwFieldNote $fwButtons \
	    -side top       \
	    -expand yes     \
	    -fill x         \
	    -padx 5         \
	    -pady 5

	# after the next idle, the window will be mapped. set the min
	# width to our width and the min height to the mapped height.
	after idle [format {
	    update idletasks
	    wm minsize %s %d [winfo reqheight %s]
	    wm geometry %s =%dx[winfo reqheight %s]
	} $wwDialog $knWidth $wwDialog $wwDialog $knWidth $wwDialog] 
    }
}

proc GDF_LoadDlog {} {
    global gDialog gaLinkedVars
    global gaScalarValueID gsaLabelContents
    global sFileName
    global glShortcutDirs

    set wwDialog .wwLoadGDFDlog

    set knWidth 400
    
    # try to create the dlog...
    if { [Dialog_Create $wwDialog "Load GDF" {-borderwidth 10}] } {
	
	set fwFile             $wwDialog.fwFile
	set fwFileNote         $wwDialog.fwFileNote
	set fwField            $wwDialog.fwField
	set fwFieldNote        $wwDialog.fwFieldNote
	set fwButtons          $wwDialog.fwButtons
	
	set sFileName [GetDefaultLocation LoadGDF]
	tkm_MakeFileSelector $fwFile "Load Group Descriptor File:" sFileName \
	    [list GetDefaultLocation LoadGDF] \
	    $glShortcutDirs

	[$fwFile.ew subwidget entry] icursor end
	
	tkm_MakeSmallLabel $fwFileNote "The GDF file to load" 400
	
	tixOptionMenu $fwField -label "Associate with Overlay:" \
	    -variable nFieldIndex \
	    -options {
		label.anchor e
		label.width 5
		menubutton.width 8
	    }
	
	tkm_MakeSmallLabel $fwFieldNote "The layer with which to associate the GDF" 400
	FillOverlayLayerMenu $fwField current
	
	# buttons.
        tkm_MakeCancelOKButtons $fwButtons $wwDialog \
	    {SetDefaultLocation LoadGDF $sFileName;
		GDF_Load $sFileName $nFieldIndex }
	
	pack $fwFile $fwFileNote $fwField $fwFieldNote $fwButtons \
	    -side top       \
	    -expand yes     \
	    -fill x         \
	    -padx 5         \
	    -pady 5
	
	# after the next idle, the window will be mapped. set the min
	# width to our width and the min height to the mapped height.
	after idle [format {
	    update idletasks
	    wm minsize %s %d [winfo reqheight %s]
	    wm geometry %s =%dx[winfo reqheight %s]
	} $wwDialog $knWidth $wwDialog $wwDialog $knWidth $wwDialog] 
    }
}

proc DoLabelToOverlayDlog {} {

    global gDialog

    set wwDialog .wwLabelToOverlayDlog

    if { [Dialog_Create $wwDialog "Copy Label Statistics to Overlay" {-borderwidth 10}] } {

	set fwMain             $wwDialog.fwMain
	set fwTarget           $wwDialog.fwTarget
	set fwButtons          $wwDialog.fwButtons
	
	frame $fwMain
	
	# target scalar field
	tixOptionMenu $fwTarget -label "Target Field:" \
	    -variable nFieldIndex \
	    -options {
		label.anchor e
		label.width 5
		menubutton.width 8
	    }
	
	FillOverlayLayerMenu $fwTarget first-empty
	
	# buttons.
	tkm_MakeCancelOKButtons $fwButtons $wwDialog \
	    { label_to_stat $nFieldIndex; UpdateAndRedraw } {}
	
	pack $fwMain $fwTarget $fwButtons \
	    -side top       \
	    -expand yes     \
	    -fill x         \
	    -padx 5         \
	    -pady 5
    }
}

proc DoSmoothOverlayDlog {} {

    global gDialog

    set wwDialog .wwSmoothOverlayDlog

    if { [Dialog_Create $wwDialog "Smooth Overlay" {-borderwidth 10}] } {

  set fwMain             $wwDialog.fwMain
  set fwSteps            $wwDialog.fwSteps
  set fwTarget           $wwDialog.fwTarget
  set fwButtons          $wwDialog.fwButtons

  frame $fwMain

  # field for number of steps
  tkm_MakeEntry $fwSteps "Number of Steps: " nSteps 6 

  # target scalar field
  tixOptionMenu $fwTarget -label "Target Field:" \
    -variable nFieldIndex \
    -options {
      label.anchor e
      label.width 5
      menubutton.width 8
  }
  
  FillOverlayLayerMenu $fwTarget current

  # buttons.
  tkm_MakeCancelOKButtons $fwButtons $wwDialog \
    { DoSmoothOverlay $nSteps $nFieldIndex; UpdateAndRedraw } {}

  pack $fwMain $fwSteps $fwTarget $fwButtons \
    -side top       \
    -expand yes     \
    -fill x         \
    -padx 5         \
    -pady 5
    }
}

proc DoSmoothCurvatureDlog {} {

    global gDialog

    set wwDialog .wwSmoothCurvDlog

    if { [Dialog_Create $wwDialog "Smooth Curvature" {-borderwidth 10}] } {

  set fwMain             $wwDialog.fwMain
  set fwSteps            $wwDialog.fwSteps
  set fwButtons          $wwDialog.fwButtons

  frame $fwMain

  # field for number of steps
  tkm_MakeEntry $fwSteps "Number of Steps: " nSteps 6 

  # buttons.
  tkm_MakeCancelOKButtons $fwButtons $wwDialog \
    { DoSmoothCurvature $nSteps; UpdateAndRedraw } {}

  pack $fwMain $fwSteps $fwButtons \
    -side top       \
    -expand yes     \
    -fill x         \
    -padx 5         \
    -pady 5
    }
}

proc DoInflateDlog {} {

    global gDialog
    global gaLinkedVars
    UpdateLinkedVarGroup inflate

    set wwDialog .wwInflateDlog

    if { [Dialog_Create $wwDialog "Inflate" {-borderwidth 10}] } {

  set fwMain             $wwDialog.fwMain
  set fwSteps            $wwDialog.fwSteps
  set fwSulc             $wwDialog.fwSulc
  set fwButtons          $wwDialog.fwButtons

  frame $fwMain

  # field for steps
  tkm_MakeEntry $fwSteps "Number of Steps: " nSteps 6 

  # cb for sulc flag
  tkm_MakeCheckboxes $fwSulc y { \
    {text "Sulc Sum" gaLinkedVars(sulcflag) {} } }

  # buttons.
  tkm_MakeCancelOKButtons $fwButtons $wwDialog \
    { SendLinkedVarGroup inflate; \
    DoInflate $nSteps; UpdateAndRedraw } {}

  pack $fwMain $fwSteps $fwSulc $fwButtons \
    -side top       \
    -expand yes     \
    -fill x         \
    -padx 5         \
    -pady 5
    }
}

proc DoDecimationDlog {} {

    global gDialog
    global glShortcutDirs

    set wwDialog .wwDecimationDlog

    if { [Dialog_Create $wwDialog "Write Decimation" {-borderwidth 10}] } {

  set fwMain             $wwDialog.fwMain
  set fwFileName         $wwDialog.fwFileName
  set fwSpacing          $wwDialog.fwSpacing
  set fwButtons          $wwDialog.fwButtons

  frame $fwMain

  # make file name selector
  set sFileName [GetDefaultLocation WriteDecimation]
  tkm_MakeFileSelector $fwFileName \
      "Write Decimation File:" sFileName \
      [list GetDefaultLocation WriteDecimation] \
      $glShortcutDirs

  [$fwFileName.ew subwidget entry] icursor end

  # field for spacing
  tkm_MakeEntry $fwSpacing "Spacing: " fSpacing 6 

  # buttons.
  set okCmd { 
      SetDefaultLocation WriteDecimation $sFileName;
      DoDecimation $sFileName $fSpacing;
      UpdateAndRedraw }
  tkm_MakeCancelOKButtons $fwButtons $wwDialog \
      "$okCmd"
  
  pack $fwMain $fwFileName $fwSpacing $fwButtons \
    -side top       \
    -expand yes     \
    -fill x         \
    -padx 5         \
    -pady 5
    }
}

proc DoSendToSubjectDlog {} {

    global gDialog

    set wwDialog .wwSendToSubjectDlog

    if { [Dialog_Create $wwDialog "Send To Subject" {-borderwidth 10}] } {

  set fwMain             $wwDialog.fwMain
  set fwSubject          $wwDialog.fwSubject
  set fwButtons          $wwDialog.fwButtons
  
  frame $fwMain
  
  # field for subject name
  tkm_MakeEntry $fwSubject "Subject: " sSubject 20
  
  # buttons.
  tkm_MakeApplyCloseButtons $fwButtons $wwDialog \
    { send_to_subject $sSubject } {}
  
  pack $fwMain $fwSubject $fwButtons \
    -side top       \
    -expand yes     \
    -fill x         \
    -padx 5         \
    -pady 5
    }
}

proc DoSelectVertexDlog {} {

    global gDialog

    set wwDialog .wwSelectVertexDlog

    if { [Dialog_Create $wwDialog "Select Vertex" {-borderwidth 10}] } {

	set fwMain             $wwDialog.fwMain
	set fwVno              $wwDialog.fwVno
	set fwButtons          $wwDialog.fwButtons
	
	frame $fwMain
	
	# field for vno
	tkm_MakeEntry $fwVno "Vertex number: " vno 6 
	
	# buttons.
	tkm_MakeDialogButtons $fwButtons $wwDialog [list \
		[list Apply { select_vertex_by_vno $vno }] \
		[list Close {}] \
        ]
	
	pack $fwMain $fwVno $fwButtons \
	    -side top       \
	    -expand yes     \
	    -fill x         \
	    -padx 5         \
	    -pady 5
    }
}

proc DoCustomFillDlog {} {

    global gDialog
    global gFillParms

    set wwDialog .wwCustomFillDlog

    if { [Dialog_Create $wwDialog "Custom Fill" {-borderwidth 10}] } {
	
	set fwMain             $wwDialog.fwMain
	set lfwFill            $wwDialog.lwFill
	set rbwSeed            $wwDialog.cbwSeed
	set lfwAction          $wwDialog.lwAction
	set fwButtons          $wwDialog.fwButtons
	
	frame $fwMain
	
	# fill text
	tixLabelFrame $lfwFill \
	    -label "Fill Conditions:" \
	    -labelside acrosstop \
	    -options { label.padX 5 }
	
	set cbwFlags           [$lfwFill subwidget frame].cbwFlags

	# cbs for flags
	tkm_MakeCheckboxes $cbwFlags y { \
	    {text "Up to and including boundaries" \
		 gFillParms(noBoundary) {} } \
	    {text "Up to other labels" \
		 gFillParms(noLabel) {} } \
	    {text "Up to and including different curvature" \
		 gFillParms(noCmid) {} } \
	    {text "Up to functional values below threshold" \
		 gFillParms(noFThresh) {} } \
					 }
	
	pack $cbwFlags

	# rbs for seed
	set gFillParms(multiseed) 0
	tkm_MakeRadioButtons $rbwSeed y "Fill From" gFillParms(multiseed) {
	    {text "Last clicked vertex" 0 {} "" }
	    {text "All marked vertices" 1 {} "" }
	}
	
	# action text
	tixLabelFrame $lfwAction \
	    -label "Action:" \
	    -labelside acrosstop \
	    -options { label.padX 5 }

	set fwAction            [$lfwAction subwidget frame]

	# rbs for flags
	set gFillParms(action) 0
	set gFillParms(argument) 0
	label $fwAction.lwNew \
	    -font [tkm_GetNormalFont] \
	    -text "Create new label"
	radiobutton $fwAction.rbwNew \
	    -font [tkm_GetNormalFont] -relief flat\
	    -variable gFillParms(action) -value 0
	label $fwAction.lwAdd \
	    -font [tkm_GetNormalFont] \
	    -text "Add to existing label"
	radiobutton $fwAction.rbwAdd \
	    -font [tkm_GetNormalFont] -relief flat\
	    -variable gFillParms(action) -value 1
	label $fwAction.lwRemove \
	    -font [tkm_GetNormalFont] \
	    -text "Remove from label"
	radiobutton $fwAction.rbwRemove \
	    -font [tkm_GetNormalFont] -relief flat\
	    -variable gFillParms(action) -value 2
	tixOptionMenu $fwAction.owTarget -variable gFillParms(argument)
	LblLst_FillListMenu $fwAction.owTarget 1
	grid $fwAction.rbwNew         -column 0 -row 0 -sticky w
	grid $fwAction.lwNew          -column 1 -row 0 -sticky w
	grid $fwAction.rbwAdd         -column 0 -row 1 -sticky w
	grid $fwAction.lwAdd          -column 1 -row 1 -sticky w
	grid $fwAction.rbwRemove      -column 0 -row 2 -sticky w
	grid $fwAction.lwRemove       -column 1 -row 2 -sticky w
	grid $fwAction.owTarget       -column 1 -row 3 -sticky w
	grid columnconfigure $fwAction 0 -weight 0
	grid columnconfigure $fwAction 1 -weight 1
	
	# buttons.
	tkm_MakeDialogButtons $fwButtons $wwDialog [list \
		[list Apply { fill_flood_from_cursor \
				  $gFillParms(noBoundary) \
				  $gFillParms(noLabel) \
				  $gFillParms(noCmid) \
				  $gFillParms(noFThresh) \
				  $gFillParms(multiseed) \
				  $gFillParms(action) \
				  $gFillParms(argument);
				  clear_all_vertex_marks; UpdateAndRedraw } \
		     "Fill"] \
	 [list Close {}] \
						       ]
	
	pack $fwMain $lfwFill $rbwSeed $lfwAction $fwButtons \
	    -side top       \
	    -expand yes     \
	    -fill x         \
	    -padx 5         \
	    -pady 5
    }
}

proc UpdateCustomFillDlog {} {
    # rebuild the target menu. this will just error out of the window
    # isn't open.
    catch {
	LblLst_FillListMenu .wwCustomFillDlog.fwAction.owTarget 1
    }
}


proc DoLoadLabelValueFileDlog {} {

    global gDialog gaLinkedVars
    global gaScalarValueID gsaLabelContents
    global glShortcutDirs
    global sFileName

    set wwDialog .wwImportLabelValuesDlog

    set knWidth 400
    
    # try to create the dlog...
    if { [Dialog_Create $wwDialog "Load Label Value" {-borderwidth 10}] } {
	
	set fwFile             $wwDialog.fwFile
	set fwFileNote         $wwDialog.fwFileNote
	set fwField            $wwDialog.fwField
	set fwFieldNote        $wwDialog.fwFieldNote
	set fwButtons          $wwDialog.fwButtons
	
	set sFileName [GetDefaultLocation LoadOverlay]
	tkm_MakeFileSelector $fwFile "Load Label Value File:" sFileName \
	    [list GetDefaultLocation LoadOverlay] \
	    $glShortcutDirs
	
	[$fwFile.ew subwidget entry] icursor end

	tkm_MakeSmallLabel $fwFileNote "The label value file" 400
	
	tixOptionMenu $fwField -label "Into Field:" \
	    -variable nFieldIndex \
	    -options {
		label.anchor e
		label.width 5
		menubutton.width 8
	    }
	
	tkm_MakeSmallLabel $fwFieldNote "The layer into which to load the values" 400
	FillOverlayLayerMenu $fwField first-empty
	
	# buttons.
        tkm_MakeCancelOKButtons $fwButtons $wwDialog { 
	    SetDefaultLocation LoadOverlay $sFileName;
	    sclv_load_label_value_file $sFileName $nFieldIndex
	}
	
	pack $fwFile $fwFileNote $fwField $fwFieldNote $fwButtons \
	    -side top       \
	    -expand yes     \
	    -fill x         \
	    -padx 5         \
	    -pady 5
	
	# after the next idle, the window will be mapped. set the min
	# width to our width and the min height to the mapped height.
	after idle [format {
	    update idletasks
	    wm minsize %s %d [winfo reqheight %s]
	    wm geometry %s =%dx[winfo reqheight %s]
	} $wwDialog $knWidth $wwDialog $wwDialog $knWidth $wwDialog] 
    }
}

proc CreateWindow { iwwTop } {
    global ksWindowName
    frame $iwwTop
    wm title . $ksWindowName
    wm withdraw .
}

proc CreateMenuBar { ifwMenuBar } {

    global gaLinkedVars
    global gbShowToolBar gbShowLabel
    UpdateLinkedVarGroup view

    set mbwFile   $ifwMenuBar.mbwFile
    set mbwEdit   $ifwMenuBar.mbwEdit
    set mbwView   $ifwMenuBar.mbwView
    set mbwTools  $ifwMenuBar.mbwTools
    
    frame $ifwMenuBar -border 2 -relief raised
  
    # file menu button
    tkm_MakeMenu $mbwFile "File" {
	{command
	    "Load Surface..."
	    {DoFileDlog LoadSurface} }
	{ cascade "Load Surface Configuration..." {
	    { command
		"Main Vertices"
		{DoFileDlog LoadMainSurface} }
	    { command
		"Inflated Vertices"
		{DoFileDlog LoadInflatedSurface} }
	    { command
		"White Vertices"
		{DoFileDlog LoadWhiteSurface} }
	    { command
		"Pial Vertices"
		{DoFileDlog LoadPialSurface} }
	    { command
		"Original Vertices"
		{DoFileDlog LoadOriginalSurface} } } }
	{command
	    "Save Surface"
	    {} }
	{command
	    "Save Surface As..."
	    {DoFileDlog SaveSurfaceAs} }
	{ separator }
	{command "Load Overlay..."
	    {DoLoadOverlayDlog}}
	{command "Save Overlay As..."
	    {DoSaveValuesAsDlog}
	    mg_OverlayLoaded }
	{command
	    "Load Time Course..."
	    {DoLoadTimeCourseDlog} }
	{command
	    "Load Label Value File..."
	    {DoLoadLabelValueFileDlog} }
	{ separator }
	{command
	    "Load Group Descriptor File..."
	    {GDF_LoadDlog} }
	{ separator }
	{cascade "Curvature" {
	    {command "Load Curvature..."
		{DoFileDlog LoadCurvature}}
	    {command "Save Curvature"
		{CheckFileAndDoCmd $curv write_binary_curv}
		mg_CurvatureLoaded }
	    {command "Save Curvature As..."
		{DoFileDlog SaveCurvatureAs}
		mg_CurvatureLoaded }
	}}
	{cascade "Patch" {
	    {command "Load Patch..."
		{DoFileDlog LoadPatch}}
	    {command "Save Patch"
		{CheckFileAndDoCmd $patch write_binary_patch}
		mg_PatchLoaded }
	    {command "Save Patch As..."
		{DoFileDlog SavePatchAs}
		mg_PatchLoaded }
	}}
	{cascade "Label" {
	    { command "Load Color Table..."
		{ DoFileDlog LoadColorTable } }
	    {command "Load Label..."
		{DoFileDlog LoadLabel}}
	    {command "Save Selected Label..."
		{DoFileDlog SaveLabelAs}
		mg_LabelLoaded }
	    {command "Import Annotation..."
		{DoFileDlog ImportAnnotation} }
	    {command "Export Annotation..."
		{DoFileDlog ExportAnnotation}
		mg_LabelLoaded }
	    {command "Delete All Labels"
		{labl_remove_all; UpdateAndRedraw}
		mg_LabelLoaded }
	}}
	{cascade "Field Sign" {
	    {command "Load Field Sign..."
		{DoFileDlog LoadFieldSign}}
	    {command "Save Field Sign"
		{CheckFileAndDoCmd $fs write_fieldsign}
		mg_FieldSignLoaded }
	    {command "Save Field Sign As..."
		{DoFileDlog SaveFieldSignAs}
		mg_FieldSignLoaded }
	}}
	{cascade "Field Mask" {
	    {command "Load Field Mask..."
		{DoFileDlog LoadFieldMask}}
	    {command "Save Field Mask"
		{CheckFileAndDoCmd $fm write_fsmask}
		mg_FieldMaskLoaded }
	    {command "Save Field Mask As..."
		{DoFileDlog SaveFieldMaskAs}
		mg_FieldMaskLoaded }
	}}   
	{ separator }
	{command
	    "Quit"
	    {exit} 
	} 
    }

    # edit menu 
    tkm_MakeMenu $mbwEdit "Edit" {
	{ command
	    "Nothing to Undo"
	    undo_last_action }
	{ separator }
	{ command
	    "Unmark All Vertices"
	    { clear_all_vertex_marks; UpdateAndRedraw } }
	{ command
	    "Deselect Label"
	    { labl_select -1; UpdateAndRedraw }
	    mg_LabelLoaded } 
    }
    
    # view menu
    tkm_MakeMenu $mbwView "View" {
	{ cascade
	    "Tool Bars" {
		{ check
		    "Main"
		    "ShowToolBar main $gbShowToolBar(main)"
		    gbShowToolBar(main) } }}
	{ cascade
	    "Information" {
		{ check
		    "Vertex Index"
		    "ShowLabel kLabel_VertexIndex $gbShowLabel(kLabel_VertexIndex)"
		    gbShowLabel(kLabel_VertexIndex) }
		{ check
		    "Distance"
		    "ShowLabel kLabel_Distance $gbShowLabel(kLabel_Distance)"
		    gbShowLabel(kLabel_Distance) }
		{ check
		    "Vertex RAS"
		    "ShowLabel kLabel_Coords_RAS $gbShowLabel(kLabel_Coords_RAS)"
		    gbShowLabel(kLabel_Coords_RAS) }
		{ check
		    "Vertex MNI Talairach"
		    "ShowLabel kLabel_Coords_MniTal $gbShowLabel(kLabel_Coords_MniTal)"
		    gbShowLabel(kLabel_Coords_MniTal) }
		{ check
		    "Vertex Talairach"
		    "ShowLabel kLabel_Coords_Tal $gbShowLabel(kLabel_Coords_Tal)"
		    gbShowLabel(kLabel_Coords_Tal) }
		{ check
		    "MRI Index"
		    "ShowLabel kLabel_Coords_Index $gbShowLabel(kLabel_Coords_Index)"
		    gbShowLabel(kLabel_Coords_Index) }
		{ check
		    "Vertex Normal"
		    "ShowLabel kLabel_Coords_Normal $gbShowLabel(kLabel_Coords_Normal)"
		    gbShowLabel(kLabel_Coords_Normal) }
		{ check
		    "Spherical X, Y, Z"
		    "ShowLabel kLabel_Coords_Sphere_XYZ $gbShowLabel(kLabel_Coords_Sphere_XYZ)"
		    gbShowLabel(kLabel_Coords_Sphere_XYZ) }
		{ check
		    "Spherical Rho, Theta"
		    "ShowLabel kLabel_Coords_Sphere_RT $gbShowLabel(kLabel_Coords_Sphere_RT)"
		    gbShowLabel(kLabel_Coords_Sphere_RT) }
		{ check
		    "Curvature"
		    "ShowLabel kLabel_Curvature $gbShowLabel(kLabel_Curvature)"
		    gbShowLabel(kLabel_Curvature)
		    mg_CurvatureLoaded }
		{ check
		    "Field Sign"
		    "ShowLabel kLabel_Fieldsign $gbShowLabel(kLabel_Fieldsign)"
		    gbShowLabel(kLabel_Fieldsign)
		    mg_FieldSignLoaded}
		{ check
		    "Overlay Layer 1"
		    "ShowLabel kLabel_Val $gbShowLabel(kLabel_Val)"
		    gbShowLabel(kLabel_Val)
		    mg_OverlayLoaded }
		{ check
		    "Overlay Layer 2"
		    "ShowLabel kLabel_Val2 $gbShowLabel(kLabel_Val2)"
		    gbShowLabel(kLabel_Val2)
		    mg_OverlayLoaded }
		{ check
		    "Overlay Layer 3"
		    "ShowLabel kLabel_ValBak $gbShowLabel(kLabel_ValBak)"
		    gbShowLabel(kLabel_ValBak)
		    mg_OverlayLoaded }
		{ check
		    "Overlay Layer 4"
		    "ShowLabel kLabel_Val2Bak $gbShowLabel(kLabel_Val2Bak)"
		    gbShowLabel(kLabel_Val2Bak)
		    mg_OverlayLoaded }
		{ check
		    "Overlay Layer 5"
		    "ShowLabel kLabel_ValStat $gbShowLabel(kLabel_ValStat)"
		    gbShowLabel(kLabel_ValStat)
		    mg_OverlayLoaded }
		{ check
		    "Overlay Layer 6"
		    "ShowLabel kLabel_ImagVal $gbShowLabel(kLabel_ImagVal)"
		    gbShowLabel(kLabel_ImagVal)
		    mg_OverlayLoaded }
		{ check
		    "Overlay Layer 7"
		    "ShowLabel kLabel_Mean $gbShowLabel(kLabel_Mean)"
		    gbShowLabel(kLabel_Mean)
		    mg_OverlayLoaded }
		{ check
		    "Overlay Layer 8"
		    "ShowLabel kLabel_MeanImag $gbShowLabel(kLabel_MeanImag)"
		    gbShowLabel(kLabel_MeanImag)
		    mg_OverlayLoaded }
		{ check
		    "Overlay Layer 9"
		    "ShowLabel kLabel_StdError $gbShowLabel(kLabel_StdError)"
		    gbShowLabel(kLabel_StdError)
		    mg_OverlayLoaded }
		{ check
		    "Amplitude"
		    "ShowLabel kLabel_Amplitude $gbShowLabel(kLabel_Amplitude)"
		    gbShowLabel(kLabel_Amplitude) }
		{ check
		    "Angle"
		    "ShowLabel kLabel_Angle $gbShowLabel(kLabel_Angle)"
		    gbShowLabel(kLabel_Angle) }
		{ check
		    "Degree"
		    "ShowLabel kLabel_Degree $gbShowLabel(kLabel_Degree)"
		    gbShowLabel(kLabel_Degree) }
		{ check
		    "Label"
		    "ShowLabel kLabel_Label $gbShowLabel(kLabel_Label)"
		    gbShowLabel(kLabel_Label)
		    mg_LabelLoaded }
		{ check
		    "Annotation"
		    "ShowLabel kLabel_Annotation $gbShowLabel(kLabel_Annotation)"
		    gbShowLabel(kLabel_Annotation) }
		{ check
		    "MRI Value"
		    "ShowLabel kLabel_MRIValue $gbShowLabel(kLabel_MRIValue)"
		    gbShowLabel(kLabel_MRIValue) }
	    }}
	{ cascade "Windows" {
	    { command
		"Labels"
		{ LblLst_ShowWindow }
			mg_LabelLoaded }
	    { command
		"Time Course Graph"
		{ Graph_ShowWindow }
		mg_TimeCourseLoaded }
	    { command
		"Group Plot"
		{GDF_ShowCurrentWindow; GDF_SendCurrentPoints}
		mg_GDFLoaded }
	}}
	{ separator }
	{ cascade "Configure..." {
	    { command "Lighting..."
		{DoConfigLightingDlog} }
	    { command "Overlay..."
			    {DoConfigOverlayDisplayDlog}
		mg_OverlayLoaded  }
	    { command "Time Course..."
		{Graph_DoConfig}
		mg_TimeCourseLoaded }
	    { command "Curvature Display..."
		{DoConfigCurvatureDisplayDlog}
		mg_CurvatureLoaded }
	    { command "Phase Encoded Data Display..."
		{DoConfigPhaseEncodedDataDisplayDlog} }
	}}
	{ separator }
	{ cascade "Surface Configuration" {
	    { radio "Main"
		{ set_current_vertex_set $gaLinkedVars(vertexset)
			UpdateLinkedVarGroup view
		    UpdateAndRedraw }
		gaLinkedVars(vertexset)
		0 }
	    { radio "Inflated"
		{ set_current_vertex_set $gaLinkedVars(vertexset)
		    UpdateLinkedVarGroup view
		    UpdateAndRedraw }
		gaLinkedVars(vertexset)
		1
		mg_InflatedVSetLoaded }
	    { radio "White"
		{ set_current_vertex_set $gaLinkedVars(vertexset)
		    UpdateLinkedVarGroup view
		    UpdateAndRedraw }
		gaLinkedVars(vertexset)
		2
		mg_WhiteVSetLoaded }
	    { radio "Pial"
		{ set_current_vertex_set $gaLinkedVars(vertexset)
		    UpdateLinkedVarGroup view
		    UpdateAndRedraw }
		gaLinkedVars(vertexset)
		3
		mg_PialVSetLoaded }
	    { radio "Original"
		{ set_current_vertex_set $gaLinkedVars(vertexset)
		    UpdateLinkedVarGroup view
		    UpdateAndRedraw }
		gaLinkedVars(vertexset)
		4
		mg_OriginalVSetLoaded }
	}}
	{cascade "Overlay Layer" {
	    { radio "Overlay Layer 1"
		{ SetOverlayField }
		gaLinkedVars(currentvaluefield)
		0
		mg_OverlayLoaded }
	    { radio "Overlay Layer 2"
		{ SetOverlayField }
		gaLinkedVars(currentvaluefield)
		1
		mg_OverlayLoaded }
	    { radio "Overlay Layer 3"
		{ SetOverlayField }
		gaLinkedVars(currentvaluefield)
		2
		mg_OverlayLoaded }
	    { radio "Overlay Layer 4"
		{ SetOverlayField }
		gaLinkedVars(currentvaluefield)
		3
		mg_OverlayLoaded }
	    { radio "Overlay Layer 5"
		{ SetOverlayField }
		gaLinkedVars(currentvaluefield)
		4
		mg_OverlayLoaded }
	    { radio "Overlay Layer 6"
		{ SetOverlayField }
		gaLinkedVars(currentvaluefield)
		5
		mg_OverlayLoaded }
	    { radio "Overlay Layer 7"
		{ SetOverlayField }
		gaLinkedVars(currentvaluefield)
		6
		mg_OverlayLoaded }
	    { radio "Overlay Layer 8"
		{ SetOverlayField }
		gaLinkedVars(currentvaluefield)
		7
		mg_OverlayLoaded }
	    { radio "Overlay Layer 9"
		{ SetOverlayield }
		gaLinkedVars(currentvaluefield)
		8
		mg_OverlayLoaded }
	}}
	{cascade "Label Style" {
	    { radio "Filled"
		{ SendLinkedVarGroup label
		    UpdateAndRedraw }
		gaLinkedVars(labelstyle)
		0
		mg_LabelLoaded }
	    { radio "Outline"
		{ SendLinkedVarGroup label
		    UpdateAndRedraw }
		gaLinkedVars(labelstyle)
		1
		mg_LabelLoaded }
	}}
	{ separator }
	{ check 
	    "Auto-redraw"
	    { SendLinkedVarGroup redrawlock
		UpdateAndRedraw }
	    gaLinkedVars(redrawlockflag) }
	{ check 
	    "Curvature"
	    { SendLinkedVarGroup view
		UpdateAndRedraw }
	    gaLinkedVars(curvflag) 
	    mg_CurvatureLoaded }
	{ check 
	    "Overlay"
	    { SendLinkedVarGroup view
		UpdateAndRedraw }
	    gaLinkedVars(overlayflag) 
	    mg_OverlayLoaded }
	{ check 
	    "Labels"
	    { SendLinkedVarGroup label
		UpdateAndRedraw }
	    gaLinkedVars(drawlabelflag) 
	    mg_LabelLoaded }
	{ check
	    "Labels under Overlay"
	    { SendLinkedVarGroup label
		UpdateAndRedraw }
	    gaLinkedVars(labels_before_overlay_flag)
	    mg_LabelLoaded }
	{ check 
	    "Scale Bar"
	    { SendLinkedVarGroup view
		UpdateAndRedraw }
	    gaLinkedVars(scalebarflag) }
	{ check 
	    "Color Scale Bar"
	    { SendLinkedVarGroup view
		UpdateAndRedraw }
	    gaLinkedVars(colscalebarflag) }
	{ check 
	    "Wireframe Overlay"
	    { SendLinkedVarGroup view
		UpdateAndRedraw }
	    gaLinkedVars(verticesflag) }
	{ check 
	    "Cursor"
	    { SendLinkedVarGroup view
		UpdateAndRedraw }
	    gaLinkedVars(drawcursorflag) }
    }

    
    # tools me
    tkm_MakeMenu $mbwTools "Tools" {
	{ command 
	    "Save Point"
	    DoSavePoint }
	{ command 
	    "Goto Saved Point"
	    DoGotoPoint }
	{ command 
	    "Send to Subject..."
	    { DoSendToSubjectDlog } }
	{ command 
	    "Select Vertex..."
	    { DoSelectVertexDlog } }
	{ separator }
	{ command 
	    "Run Script..."
	    { DoFileDlog RunScript } }
	{ separator }
	{ cascade "Labels" {
	    { command "New Label from Marked Vertices"
		{ labl_new_from_marked_vertices; UpdateAndRedraw } }
	    { command "Mark Seleted Label"
		{ labl_mark_vertices $gnSelectedLabel
		    UpdateAndRedraw }
		mg_LabelLoaded }
	    { command "Delete Selected Label"
		{ labl_remove $gnSelectedLabel
		    UpdateAndRedraw }
		mg_LabelLoaded }
	    { command "Delete All Labels"
		{ labl_remove_all
		    UpdateAndRedraw }
		mg_LabelLoaded }
	    { command "Copy Label Statistics to Overlay..."
		{ DoLabelToOverlayDlog }
		mg_LabelLoaded } 
	    { command "Erode Selected Label"
		{ labl_erode $gnSelectedLabel; UpdateAndRedraw }
		mg_LabelLoaded } 
	    { command "Dilate Selected Label"
		{ labl_dilate $gnSelectedLabel; UpdateAndRedraw }
		mg_LabelLoaded } 
	}}
	{ cascade "Cut" {
	    { command "Cut Line"
		{ cut_line 0
		    UpdateAndRedraw } }
	    { command "Cut Closed Line"
		{ cut_line 1
		    UpdateAndRedraw } }
	    { command "Cut Plane"
		{ cut_plane
		    UpdateAndRedraw } }
	    { command "Clear Cuts"
		{ restore_ripflags 2
		    UpdateAndRedraw } }
	}}
	{ cascade "Time Course" {
	    { command "Graph Marked Vertices Avg"
		{ func_select_marked_vertices
		    func_graph_timecourse_selection}
		mg_TimeCourseLoaded }
	    { command "Graph Label Avg"
		{ func_select_label
		    func_graph_timecourse_selection }
		mg_TimeCourseLoaded }
	    { command "Write Summary of Marked Vertices..."
		{ DoFileDlog WriteMarkedVerticesTCSummary }
		mg_TimeCourseLoaded }
	    { command "Write Summary of Label..."
		{ DoFileDlog WriteLabelTCSummary }
		mg_TimeCourseLoaded }
	    { command "Save Graph to Postscript File"
		{ DoFileDlog SaveGraphToPS }
		mg_TimeCourseLoaded } 
	}}
	{ cascade "Fill" {
	    { command "Make Fill Boundary"
		{ fbnd_new_line_from_marked_vertices
		    clear_all_vertex_marks
		    UpdateAndRedraw } }
	    { command "Delete Selected Boundary"
		{ fbnd_remove_selected_boundary } }
	    { command "Custom Fill..."
		{ DoCustomFillDlog } }
	    { command "Fill Uncut Area"
		{ floodfill_marked_patch 0
		    UpdateAndRedraw } }
	    { command "Fill Stats"
		{ floodfill_marked_patch 1
		    UpdateAndRedraw }
		mg_OverlayLoaded }
	    { command "Fill Curvature"
		{ floodfill_marked_patch 2
		    UpdateAndRedraw }
		mg_CurvatureLoaded } 
	}}
	{ cascade "Surface" {
	    { command "Smooth Curvature..."
		{ DoSmoothvCurvatureDlog }
		mg_CurvatureLoaded }
	    { command "Clear Curvature"
		{ clear_curvature }
		mg_CurvatureLoaded }
	    { command "Smooth Overlay..."
		{ DoSmoothOverlayDlog }
		mg_OverlayLoaded }
	    { command "Inflate..."
		{ DoInflateDlog } }
	    { command "Swap Surface Fields..."
		{ DoSwapSurfaceFieldsDlog } }
	    { command "Write Decimation..."
		{ DoDecimationDlog } }
	    {command "Write Dipoles..."
		{DoFileDlog SaveDipolesAs}}
	    { command "Average Background Midpoint"
		{ UpdateLinkedVarGroup cvavg
		    set gaLinkedVars(cmid) $gaLinkedVars(dipavg)
		    SendLinkedVarGroup cvavg
		    UpdateAndRedraw } } 
	    {command "Mask Values to Label..."
		{DoFileDlog MaskLabel}}
	}}
	{ cascade "Group" {
	    { command "Graph Marked Vertices Avg"
		{ GDF_PlotAvgMarkedVerts }
		mg_GDFLoaded }
	    { command "Save Plotted Data to Table"
		{ DoFileDlog SaveGDFPlotToTable }
		mg_GDFLoaded }
	    { command "Save Plot to Postscript File"
		{ DoFileDlog SaveGDFPlotToPS }
		mg_GDFLoaded } } }
	{ separator }
	{ command "Save RGB As..."
	    { DoFileDlog SaveRGBAs } }
	{ command "Save TIFF As..."
	    { DoFileDlog SaveTIFFAs } }
	{ command "Make Frame"
	    { save_rgb_cmp_frame } }
    }

    pack $mbwFile $mbwEdit $mbwView $mbwTools \
      -side left
}

proc CreateNavigationArea { ifwTop } {

    global gNextTransform

    ResetTransform

    frame $ifwTop
    set fwNoteBook $ifwTop.fwNoteBook
    set fwButtons  $ifwTop.fwButtons

    tixNoteBook $fwNoteBook

    $fwNoteBook add nav1 -label "Historical"
    CreateOldNavigationArea [$fwNoteBook subwidget nav1].fw
    pack [$fwNoteBook subwidget nav1].fw

    $fwNoteBook add nav2 -label "Sensible"
    CreateSimpleNavigationArea [$fwNoteBook subwidget nav2].fw
    pack [$fwNoteBook subwidget nav2].fw

    frame $fwButtons -relief raised -border 2
    tkm_MakeButtons $fwButtons.restore { \
      { text "Restore View" { RestoreView } "" } }
    tkm_MakeButtons $fwButtons.redraw { \
      { text "Redraw View" { UpdateAndRedraw } "" } }

    bind $fwButtons.redraw.bw0 <B2-ButtonRelease> [list UpdateLockButton $fwButtons.redraw.bw0 gaLinkedVars(redrawlockflag)]

    pack $fwButtons.restore $fwButtons.redraw \
      -side left \
      -expand yes \
      -fill x

    pack $fwNoteBook $fwButtons \
      -side top \
      -expand yes \
      -fill x
}

proc CreateSmallNavigationArea { ifwTop } {

    global gaNavigationPane

    ResetTransform

    frame $ifwTop
    set fwNavigation $ifwTop.fwNavigation
    set fwCompressed $ifwTop.fwCompressed
    set fwOld        $ifwTop.fwOld
    set fwButtons    $ifwTop.fwButtons

    CreateSimpleNavigationArea     $fwNavigation
    CreateOldNavigationArea        $fwOld
    CreateCompressedNavigationArea $fwCompressed

    pack $fwCompressed \
      -side top
}

proc CreateSimpleNavigationArea { ifwTop } {

    global gNextTransform

    set knSliderWidth 150
    set knSliderHeight 100

    frame $ifwTop
    
    tkm_MakeBigLabel $ifwTop.rot_label "Rotate"
    tkm_MakeButtons $ifwTop.rot_n {{image icon_arrow_rot_x_pos \
      { rotate_brain_x -$gNextTransform(rotate,degrees); \
      UpdateAndRedraw } ""}}
    tkm_MakeButtons $ifwTop.rot_ne {{image icon_arrow_rot_z_neg \
      { rotate_brain_z $gNextTransform(rotate,degrees); \
      UpdateAndRedraw } ""}}
    tkm_MakeButtons $ifwTop.rot_e {{image icon_arrow_rot_y_neg \
      { rotate_brain_y $gNextTransform(rotate,degrees); \
      UpdateAndRedraw } ""}}
    tkm_MakeButtons $ifwTop.rot_s {{image icon_arrow_rot_x_neg \
      { rotate_brain_x $gNextTransform(rotate,degrees); \
      UpdateAndRedraw } ""}}
    tkm_MakeButtons $ifwTop.rot_w {{image icon_arrow_rot_y_pos \
      { rotate_brain_y -$gNextTransform(rotate,degrees); \
      UpdateAndRedraw } ""}}
    tkm_MakeButtons $ifwTop.rot_nw {{image icon_arrow_rot_z_pos \
      { rotate_brain_z -$gNextTransform(rotate,degrees); \
      UpdateAndRedraw } ""}}
    tkm_MakeSliders $ifwTop.rot_deg [list \
      [list {"Deg"} gNextTransform(rotate,degrees) \
      0 180.0 [expr $knSliderWidth / 2] {} 1 1 horizontal]]
    set nBaseCol 0
    grid $ifwTop.rot_label -column [expr 0 + $nBaseCol] -row 0 -columnspan 3
    grid $ifwTop.rot_n -column [expr 1 + $nBaseCol] -row 1
    grid $ifwTop.rot_ne -column [expr 2 + $nBaseCol] -row 1
    grid $ifwTop.rot_e -column [expr 2 + $nBaseCol] -row 2
    grid $ifwTop.rot_s -column [expr 1 + $nBaseCol] -row 3
    grid $ifwTop.rot_w -column [expr 0 + $nBaseCol] -row 2
    grid $ifwTop.rot_nw -column [expr 0 + $nBaseCol] -row 1
    grid $ifwTop.rot_deg -column [expr 0 + $nBaseCol] -row 4 -columnspan 3


    grid [frame $ifwTop.space1 -width 10] -column 3 -row 0

    tkm_MakeBigLabel $ifwTop.trans_label "Translate"
    tkm_MakeButtons $ifwTop.trans_n {{image icon_arrow_up \
      { translate_brain_y $gNextTransform(translate,dist); \
      UpdateAndRedraw } ""}}
    tkm_MakeButtons $ifwTop.trans_e {{image icon_arrow_right \
      { translate_brain_x $gNextTransform(translate,dist); \
      UpdateAndRedraw } ""}}
    tkm_MakeButtons $ifwTop.trans_s {{image icon_arrow_down \
      { translate_brain_y -$gNextTransform(translate,dist); \
      UpdateAndRedraw } ""}}
    tkm_MakeButtons $ifwTop.trans_w {{image icon_arrow_left \
      { translate_brain_x -$gNextTransform(translate,dist); \
      UpdateAndRedraw } ""}}
    tkm_MakeSliders $ifwTop.trans_mm [list \
      [list {"mm"} gNextTransform(translate,dist) \
      0 100.0 [expr $knSliderWidth / 2] {} 1 1 horizontal]]
    set nBaseCol 4
    grid $ifwTop.trans_label -column [expr 0 + $nBaseCol] -row 0 -columnspan 3
    grid $ifwTop.trans_n -column [expr 1 + $nBaseCol] -row 1
    grid $ifwTop.trans_e -column [expr 2 + $nBaseCol] -row 2
    grid $ifwTop.trans_s -column [expr 1 + $nBaseCol] -row 3
    grid $ifwTop.trans_w -column [expr 0 + $nBaseCol] -row 2
    grid $ifwTop.trans_mm -column [expr 0 + $nBaseCol] -row 4 -columnspan 3

    grid [frame $ifwTop.space2 -width 10] -column 7 -row 0

    tkm_MakeBigLabel $ifwTop.scl_label "Scale"
    tkm_MakeButtons $ifwTop.scl_s {{image icon_zoom_out \
      { scale_brain [expr 1.0 / [expr $gNextTransform(scale,amt)/100.0]]; \
      UpdateAndRedraw } ""}}
    tkm_MakeButtons $ifwTop.scl_b {{image icon_zoom_in \
      { scale_brain [expr $gNextTransform(scale,amt)/100.0]; \
      UpdateAndRedraw } ""}}
    tkm_MakeSliders $ifwTop.scl_per [list \
      [list {"%"} gNextTransform(scale,amt) \
      0 200.0 [expr $knSliderWidth / 2] {} 1 1 horizontal]]
    set nBaseCol 8
    grid $ifwTop.scl_label -column [expr 0 + $nBaseCol] -row 0 -columnspan 3
    grid $ifwTop.scl_s -column [expr 0 + $nBaseCol] -row 2
    grid $ifwTop.scl_b -column [expr 2 + $nBaseCol] -row 2
    grid $ifwTop.scl_per -column [expr 0 + $nBaseCol] -row 4 -columnspan 3
}

proc CreateCompressedNavigationArea { ifwTop } {

    global gNextTransform

    set knSliderWidth 150
    set knSliderHeight 100

    frame $ifwTop
    
    set fwRotation   $ifwTop.fwRotation
    set fwRotButtons $fwRotation.fwRotButtons
    set fwRotSlider  $fwRotation.fwRotSlider

    frame $fwRotation -relief raised -border 2

    tkm_MakeButtons $fwRotButtons { \
      {image icon_arrow_rot_x_pos \
      { rotate_brain_x -$gNextTransform(rotate,degrees); \
      UpdateAndRedraw } ""} \
      \
      {image icon_arrow_rot_z_neg \
      { rotate_brain_z $gNextTransform(rotate,degrees); \
      UpdateAndRedraw } ""} \
      \
      {image icon_arrow_rot_y_neg \
      { rotate_brain_y $gNextTransform(rotate,degrees); \
      UpdateAndRedraw } ""} \
      \
      {image icon_arrow_rot_x_neg \
      { rotate_brain_x $gNextTransform(rotate,degrees); \
      UpdateAndRedraw } ""} \
      \
      {image icon_arrow_rot_y_pos \
      { rotate_brain_y -$gNextTransform(rotate,degrees); \
      UpdateAndRedraw } ""} \
      \
      {image icon_arrow_rot_z_pos \
      { rotate_brain_z -$gNextTransform(rotate,degrees); \
      UpdateAndRedraw } ""} }

    tkm_MakeSliders $fwRotSlider [list \
      [list {"Deg"} gNextTransform(rotate,degrees) \
      0 180.0 [expr $knSliderWidth / 2] {} 1 1 horizontal]]

    pack $fwRotButtons $fwRotSlider -side top


    set fwTranslation   $ifwTop.fwTranslation
    set fwTransButtons  $fwTranslation.fwTransButtons
    set fwTransSlider   $fwTranslation.fwTransSlider

    frame $fwTranslation -relief raised -border 2

    tkm_MakeButtons $fwTransButtons { \
      {image icon_arrow_left \
      { translate_brain_x -$gNextTransform(translate,dist); \
      UpdateAndRedraw } ""} \
      \
      {image icon_arrow_down \
      { translate_brain_y -$gNextTransform(translate,dist); \
      UpdateAndRedraw } ""} \
      \
      {image icon_arrow_up \
      { translate_brain_y $gNextTransform(translate,dist); \
      UpdateAndRedraw } ""} \
      \
      {image icon_arrow_right \
      { translate_brain_x $gNextTransform(translate,dist); \
      UpdateAndRedraw } ""} }

    tkm_MakeSliders $fwTransSlider [list \
      [list {"mm"} gNextTransform(translate,dist) \
      0 100.0 [expr $knSliderWidth / 2] {} 1 1 horizontal]]

    pack $fwTransButtons $fwTransSlider -side top


    set fwScale       $ifwTop.fwScale
    set fwSclButtons  $fwScale.fwSclButtons
    set fwSclSlider   $fwScale.fwSclSlider

    frame $fwScale -relief raised -border 2

    tkm_MakeButtons $fwSclButtons { \
      {image icon_zoom_out \
      { scale_brain [expr 1.0 / [expr $gNextTransform(scale,amt)/100.0]]; \
      UpdateAndRedraw } ""} \
      \
      {image icon_zoom_in \
      { scale_brain [expr $gNextTransform(scale,amt)/100.0]; \
      UpdateAndRedraw } ""} }

    tkm_MakeSliders $fwSclSlider [list \
      [list {"%"} gNextTransform(scale,amt) \
      0 200.0 [expr $knSliderWidth / 2] {} 1 1 horizontal]]

    pack $fwSclButtons $fwSclSlider -side top

    pack $fwRotation $fwTranslation $fwScale -side left
}

proc CreateOldNavigationArea { ifwTop } {

    set knSliderWidth 150
    set knSliderHeight 100

    frame $ifwTop
    
    frame [set fwTransform   $ifwTop.fwTransform]
    frame [set fwRotate      $fwTransform.fwRotate]
    frame [set fwTop         $fwRotate.fwTop]
    frame [set fwBottom      $fwRotate.fwBottom]
    frame [set fwBottomRight $fwRotate.fwBottomRight]
    
    tkm_MakeBigLabel $fwTop.label "Rotate (deg)"
    tkm_MakeSliders $fwTop.y [list \
      [list {"Y"} gNextTransform(rotate,y) \
      180.0 -180.0 $knSliderWidth {} 0 1 horizontal]]
    tkm_MakeSliders $fwBottom.x [list \
    [list {"X"} gNextTransform(rotate,x) \
    180.0 -180.0 $knSliderHeight {} 0 1 vertical]]
    tkm_MakeEntry $fwBottomRight.x "X" gNextTransform(rotate,x) 5
    tkm_MakeEntry $fwBottomRight.y "Y" gNextTransform(rotate,y) 5
    tkm_MakeEntry $fwBottomRight.z "Z" gNextTransform(rotate,z) 5

    pack $fwTop.label $fwTop.y \
      -side top
    pack $fwBottom.x
    pack $fwBottomRight.x $fwBottomRight.y $fwBottomRight.z \
      -side top

    pack $fwTop \
      -side top
    pack $fwBottom \
      -side left
    pack $fwBottomRight \
      -side left \
      -expand yes \
      -fill x
  
    pack $fwRotate  \
      -side left \
      -padx 5 \
      -expand yes \
      -fill x

    frame [set fwTranslate   $fwTransform.fwTranslate]
    frame [set fwTop         $fwTranslate.fwTop]
    frame [set fwBottom      $fwTranslate.fwBottom]
    frame [set fwBottomRight $fwTranslate.fwBottomRight]
    
    tkm_MakeBigLabel $fwTop.label "Translate (mm)"
    tkm_MakeSliders $fwTop.x [list \
      [list {"X"} gNextTransform(translate,x) \
      -100.0 100.0 $knSliderWidth {} 0 1 horizontal]]
    tkm_MakeSliders $fwBottom.y [list \
    [list {"Y"} gNextTransform(translate,y) \
    100.0 -100.0 $knSliderHeight {} 0 1 vertical]]
    tkm_MakeEntry $fwBottomRight.x "X" gNextTransform(translate,x) 5
    tkm_MakeEntry $fwBottomRight.y "Y" gNextTransform(translate,y) 5

    pack $fwTop.label $fwTop.x \
      -side top
    pack $fwBottom.y
    pack $fwBottomRight.x $fwBottomRight.y \
      -side top

    pack $fwTop \
      -side top
    pack $fwBottom \
      -side left
    pack $fwBottomRight \
      -side left \
      -expand yes \
      -fill x
  
    pack $fwTranslate \
      -side left \
      -padx 5 \
      -expand yes \
      -fill x

    frame [set fwScale       $fwTransform.fwScale]
    frame [set fwTop         $fwScale.fwTop]
    frame [set fwLeft        $fwScale.fwLeft]
    frame [set fwRight       $fwScale.fwRight]
    
    tkm_MakeBigLabel $fwTop.label "Scale (%)"

    tkm_MakeSliders $fwLeft.scale [list \
    [list {""} gNextTransform(scale) \
    0.0 200.0 $knSliderHeight {} 0 1 vertical]]
    tkm_MakeEntry $fwRight.scale "" gNextTransform(scale) 5

    pack $fwTop.label \
      -side top
    pack $fwLeft.scale $fwRight.scale \
      -side left

    pack $fwTop \
      -side top 
    pack $fwLeft \
      -side left \
      -expand yes \
      -fill y 
    pack $fwRight \
      -side left \
      -expand yes \
      -fill x
  
    pack $fwScale \
      -side left \
      -expand yes \
      -fill y \
      -padx 5

    frame [set fwButton $ifwTop.fwButton]
    tkm_MakeButtons $fwButton.transform {{ text "Transform" \
      { DoTransform; ResetTransform; UpdateAndRedraw} }}
    pack $fwButton.transform

    pack $fwTransform $fwButton \
      -side top \
      -expand yes \
      -fill x \
      -pady 5
    pack $fwTransform
}

proc CreateCursorFrame { ifwTop } {

    set fwLabel             $ifwTop.fwMainLabel
    set fwLinkCheckbox      $ifwTop.fwLinkCheckbox
    set fwLabels            $ifwTop.fwLabels

    frame $ifwTop

    # the label that goes at the top of the frame
    tkm_MakeBigLabel $fwLabel "Cursor"

    # make the labels
    CreateLabelFrame $fwLabels cursor

    # pack the subframes in a column. 
    pack $fwLabel \
      -side top \
      -anchor w

    pack $fwLabels \
      -side top \
      -anchor s

}

proc CreateMouseoverFrame { ifwTop } {

    global gaLinkedVars
    UpdateLinkedVarGroup mouseover

    set fwTop               $ifwTop.fwTop
    set fwLabel             $fwTop.fwMainLabel
    set fwOn                $fwTop.fwOn
    set fwLabels            $ifwTop.fwLabels

    frame $ifwTop
    frame $fwTop

    # the label that goes at the top of the frame
    tkm_MakeBigLabel $fwLabel "Mouse"

    # make the labels
    CreateLabelFrame $fwLabels mouseover

    # pack the subframes in a column. 
    pack $fwLabel  \
      -side left \
      -anchor w

    pack $fwTop \
      -expand y -fill x \
      -side top \
      -anchor w

    pack $fwLabels \
      -side bottom \
      -anchor s
}

proc CreateLabelFrame { ifwTop iSet } {

    global glLabel gfwaLabel gsaLabelContents gaScalarValueID 

    frame $ifwTop

    # create the frame names
    foreach label $glLabel {
	set gfwaLabel($label,$iSet) $ifwTop.fw$label
    }
    
    # create two active labels in each label frame
    foreach label $glLabel {
	frame $gfwaLabel($label,$iSet)
	set fwLabel $gfwaLabel($label,$iSet).fwLabel
	set fwValue $gfwaLabel($label,$iSet).fwValue
	
	# if it's a value label, make it a normal entry (editable). else 
	# make it an active label (uneditable).
	if { $label == "kLabel_Val" ||         \
		 $label == "kLabel_Val2" ||     \
		 $label == "kLabel_ValBak" ||   \
		 $label == "kLabel_Val2Bak" ||  \
		 $label == "kLabel_ValStat" ||  \
		 $label == "kLabel_ImagVal" ||  \
		 $label == "kLabel_Mean" ||  \
		 $label == "kLabel_MeanImag" ||  \
		 $label == "kLabel_StdError" } { 
	    tkm_MakeEntry $fwLabel "" gsaLabelContents($label,name) 14 "UpdateOverlayDlogInfo; UpdateValueLabelName $gaScalarValueID($label,index) \[set gsaLabelContents($label,name)\]"
	} else {
	    tkm_MakeActiveLabel $fwLabel "" gsaLabelContents($label,name) 14
	}
	
	# active leabel for the contents (uneditable).
	if { $label == "kLabel_VertexIndex" && $iSet == "cursor" } {
	    tkm_MakeEntry $fwValue "" gsaLabelContents($label,value,$iSet) 18 \
		"select_vertex_by_vno \[set gsaLabelContents($label,value,$iSet)\]; redraw"
	} else {
	    tkm_MakeActiveLabel $fwValue "" gsaLabelContents($label,value,$iSet) 18
	}
	
	pack $fwLabel $fwValue \
	    -side left \
	    -anchor w
    }

}

proc ShowLabel { isLabel ibShow } {

    global gbShowLabel
    PackLabel $isLabel cursor $ibShow 
    PackLabel $isLabel mouseover $ibShow
    set gbShowLabel($isLabel) $ibShow
}

proc ShowValueLabel { inValueIndex ibShow } {
    
    global gaScalarValueID gsaLabelContents
    if { [info exists gaScalarValueID($inValueIndex,label)] == 0 } {
  puts "ShowValueLabel: $inValueIndex invalid"
  return
    }

    ShowLabel $gaScalarValueID($inValueIndex,label) $ibShow
}

proc PackLabel { isLabel iSet ibShow } {

    global glLabel gfwaLabel

    # find the label index in our list.
    set nLabel [lsearch -exact $glLabel $isLabel]
    if { $nLabel == -1 } {
  puts "Couldn't find $isLabel\n"
  return;
    }

    # are we showing or hiding?
    if { $ibShow == 1 } {

  # go back and try to pack it after the previous labels
  set lTemp [lrange $glLabel 0 [expr $nLabel - 1]]
  set lLabelsBelow ""
  foreach element $lTemp {
      set lLabelsBelow [linsert $lLabelsBelow 0 $element]
  }
  foreach label $lLabelsBelow {
      if {[catch { pack $gfwaLabel($isLabel,$iSet) \
        -after $gfwaLabel($label,$iSet)    \
        -side top                \
        -anchor w } sResult] == 0} {
    return;
      }
  }
  
  # if that fails, go forward and try to pack it before the later labels
  set lLabelsAbove [lrange $glLabel [expr $nLabel + 1] [llength $glLabel]]
  foreach label $lLabelsAbove {
      if {[catch { pack $gfwaLabel($isLabel,$iSet)  \
        -before $gfwaLabel($label,$iSet)    \
        -side top                  \
        -anchor w } sResult] == 0} {
    return;
      }
  }

  # must be the first one. just pack it.
  catch { pack $gfwaLabel($isLabel,$iSet)  \
    -side top                \
    -anchor w } sResult

    } else {
  
  # else just forget it
  pack forget $gfwaLabel($isLabel,$iSet)
    } 
}

proc CreateToolBar { ifwToolBar } {

    global gfwaToolBar

    frame $ifwToolBar

    # main toolbar
    set gfwaToolBar(main)   $ifwToolBar.fwMainBar
    set fwCut              $gfwaToolBar(main).fwCut
    set fwPoint            $gfwaToolBar(main).fwPoint
    set fwView             $gfwaToolBar(main).fwView
    set fwFill             $gfwaToolBar(main).fwFill
    set fwLabel            $gfwaToolBar(main).fwLabel
    
    frame $gfwaToolBar(main) -border 2 -relief raised
    
    tkm_MakeButtons $fwCut { 
	{ image icon_cut_line { cut_line 0; UpdateAndRedraw } 
	    "Cut Line" } 
	{ image icon_cut_closed_line { cut_line 1; UpdateAndRedraw } 
	    "Cut Closed Line" } 
	{ image icon_cut_plane { cut_plane; UpdateAndRedraw } 
	    "Cut Plane" } 
	{ image icon_cut_area { floodfill_marked_patch 0; 
	    UpdateAndRedraw } "Fill Uncut Area" } 
	{ image icon_cut_clear { restore_ripflags 2; UpdateAndRedraw } 
	    "Clear Cuts" }
    }
    
    tkm_MakeButtons $fwPoint { \
      { image icon_cursor_save { DoSavePoint } "Save Point" } \
      { image icon_cursor_goto { DoGotoPoint } "Goto Saved Point" } }
    
    tkm_MakeButtons $fwView { \
      { image icon_home { RestoreView } "Restore View" } \
      { image icon_redraw { UpdateAndRedraw } "Redraw View" } }
    
    tkm_MakeButtons $fwFill { \
      { image icon_draw_line { fbnd_new_line_from_marked_vertices; \
        clear_all_vertex_marks; UpdateAndRedraw } "Make Fill Boundary" } \
      { image icon_draw_line_closed { close_marked_vertices; \
      fbnd_new_line_from_marked_vertices; \
      clear_all_vertex_marks; UpdateAndRedraw } "Make Closed Fill Boundary" } \
      { image icon_fill_label { DoCustomFillDlog } "Custom Fill" } \
      { image icon_erase_line { fbnd_remove_selected_boundary; } \
      "Remove Selected Boundary" } }
    
    bind $fwView.bw1 <B2-ButtonRelease> [list UpdateLockButton $fwView.bw1 gaLinkedVars(redrawlockflag)]
    
    set fwLabel              $gfwaToolBar(main).fwLabel
    set fwColor              $gfwaToolBar(main).fwColor

    tkm_MakeButtons $fwLabel {
      { image icon_marked_to_label 
	  { labl_new_from_marked_vertices; UpdateAndRedraw } 
	  "New Label from Marked" }
      { image icon_label_to_marked 
	  { labl_mark_vertices $gnSelectedLabel; UpdateAndRedraw } 
	  "Mark Label" } 
      { image icon_erase_label 
	  { labl_remove $gnSelectedLabel; UpdateAndRedraw } 
	  "Erase Label" } }

    tkm_MakeButtons $fwColor {
	{ image icon_color_label
	      { CreateColorPickerWindow LblLst_SetCurrentLabelColor } 
	    "Change label color" } }

    pack $fwCut $fwPoint $fwView $fwFill $fwLabel $fwColor \
      -side left \
      -anchor w \
      -padx 5
}

proc ShowToolBar { isWhich ibShow } {

    global gfwaToolBar gbShowToolBar

    if { $ibShow == 1 } {   
	
	if { [catch { pack $gfwaToolBar($isWhich) \
			  -side top \
			  -fill x \
			  -expand yes \
			  -after $gfwaToolBar(main) } sResult] == 1 } {
	    
	    pack $gfwaToolBar($isWhich) \
		-side top \
		-fill x \
		-expand yes
	}
	
    } else {
	
	pack forget $gfwaToolBar($isWhich)
    }
    
    set gbShowToolBar($isWhich) $ibShow
}

proc MoveToolWindow { inX inY } {
    wm geometry . +$inX+$inY
    wm deiconify .
}

proc SelectVertex { ivno } {
    global gState
    set gState(lSelectedVnos) [list $ivno]
    GDF_SendCurrentPoints
}

proc OverlayLayerChanged {} {
    catch { UpdateOverlayDlogInfo }
    GDF_HideAllWindows
    GDF_ShowCurrentWindow
    GDF_SendCurrentPoints
}

# ====================================================================== GRAPH

# constants
set ksGraphWindowName "Time Course"
set knLineWidth(active) 4
set knLineWidth(inactive) 2
set knMinWidthPerTick 80

# gGraphSetting($dataSet,{visible,condition,label})
# gConditionData($condition,{points,errors})

set glAllColors {Red Green Blue Purple Brown Pink Gray LightBlue Yellow Orange}
set gnMaxColors [llength $glAllColors]
set nCondition 0
foreach dataSet $glAllColors {
    if { 0 == $nCondition } {
  set gGraphSetting($dataSet,visible)  0
    } else {
  set gGraphSetting($dataSet,visible)  1
    }
    set gGraphSetting($dataSet,condition) $nCondition
    set gGraphSetting($dataSet,label) "Condition $nCondition"
    incr nCondition
}
set glGraphColors [lrange $glAllColors 0 end] ;# the ones in use
set gbErrorBars                     0
set gbTimeCourseOffset              0
set gbShowTimeCourseOffsetOptions   0
set gnMaxNumErrorBars               0
set gnNumDataSets                0
set gnNumTimeCourseConditions    0
set gsTimeCourseLocation ""
set gsTimeCourseDataName ""
set gbAutoRangeGraph 1
set gbPreStimOffset 0
set gnFixedAxesSize(x1) 0
set gnFixedAxesSize(x2) 0
set gnFixedAxesSize(y1) 0
set gnFixedAxesSize(y2) 0

set gbFunctionalWindowOpen 0

proc CreateGraphWindow { iwwTop } {
    global gwwGraphWindow ksGraphWindowName

    set gwwGraphWindow $iwwTop
    toplevel $iwwTop
    wm title $iwwTop $ksGraphWindowName
    wm geometry $iwwTop 600x400

    # if they hit the close box for this window, it will just hide it
    # instead of destroying the window.
    wm protocol $iwwTop WM_DELETE_WINDOW { Graph_HideWindow }
}

proc CreateGraphFrame { ifwGraph } {

    global gwGraph
    global knLineWidth

    frame $ifwGraph

    set gwGraph      $ifwGraph.gwGraph
    set fwNotes      $ifwGraph.fwNotes

    blt::graph $gwGraph -title "Time Course" \
      -plotbackground white
    
    tkm_MakeNormalLabel $fwNotes "Click-1 to set time point. Click-2 and drag to zoom in. Click-3 to unzoom."

    pack $gwGraph        \
      -side top    \
      -fill both   \
      -expand true

    pack $fwNotes -side top

    pack $ifwGraph       \
      -side top    \
      -padx 3      \
      -pady 3      \
      -expand true \
      -fill both


    $gwGraph legend bind all <Enter> { 
  $gwGraph element configure [$gwGraph legend get current] \
    -linewidth $knLineWidth(active)
  $gwGraph legend activate [$gwGraph legend get current]
    }
    
    
    $gwGraph legend bind all <Leave> { 
  $gwGraph element configure [$gwGraph legend get current] \
    -linewidth $knLineWidth(inactive)
  $gwGraph legend deactivate [$gwGraph legend get current]
    }
    
    bind $gwGraph <ButtonPress-2> { Graph_RegionStart %W %x %y }
    bind $gwGraph <B2-Motion> { Graph_RegionMotion %W %x %y }
    bind $gwGraph <ButtonRelease-2> { Graph_RegionEnd %W %x %y }
    bind $gwGraph <ButtonRelease-3> { Graph_Unzoom %W }
}

proc Graph_ShowWindow {} {
    global gwwGraphWindow
    wm deiconify $gwwGraphWindow
}

proc Graph_HideWindow {} {
    global gwwGraphWindow
    wm withdraw $gwwGraphWindow
}

proc Graph_BeginData {} {
    global gConditionData gnNumDataSets
    for { set nCond 0 } { $nCond < $gnNumDataSets } { incr nCond } {
  set gConditionData($nCond,points) {}
  set gConditionData($nCond,errors) {}
    }
}

proc Graph_EndData {} {
    UpdateLinkedVarGroup graph
    Graph_Draw
}

proc Graph_ClearGraph {} {
    global gwGraph
    set lElements [$gwGraph element names *]
    foreach element $lElements {
  $gwGraph element delete $element
    }
}

proc Graph_SetPointsData { inCondition ilPoints } {
    global gConditionData gnNumDataSets
    # save the data. update the number of data sets. 
    set gConditionData($inCondition,points) $ilPoints
  set gnNumDataSets [expr $inCondition + 1]
}

proc Graph_SetErrorData { inCondition ilErrors } {
    global gConditionData
    # save the data
    set gConditionData($inCondition,errors) $ilErrors
}

proc Graph_Draw {} {

    global knMinWidthPerTick
    global gwGraph gnFixedAxesSize
    global gConditionData gGraphSetting glGraphColors
    global gbErrorBars gbAutoRangeGraph
    global gnMaxNumErrorBars gnNumDataSets
    global gaLinkedVars gbPreStimOffset

    Graph_ClearGraph

    # if no data, return
    if {$gnNumDataSets == 0} { return; }
    
    # if there is only one condition, show it (0 is hidden by default)
    if { $gnNumDataSets == 1 } {
  set gGraphSetting(Red,visible) 1
    }
    
    foreach dataSet $glGraphColors {
  
  # skip if not visible.
  if { $gGraphSetting($dataSet,visible) == 0 } { continue; }
  
  # get the condition index for this color
  set nCondition $gGraphSetting($dataSet,condition)
  
  # get the data. continue if we couldn't get it. get its length.
  if { [catch { set lGraphData $gConditionData($nCondition,points) } \
    sResult]} { continue; }
  set nLength [llength $lGraphData]
  if { $nLength <= 0 } { continue; }
  
  # try to size the spacing of the ticks on the x axis appropriatly.
  # get the x range of this data and find out how many points we have.
  # then get the width of the graph and divide it by the number
  # of points. if < knMinWidthPerTick pixels, set the step size to
  # the width divided by knMinWidthPerTick. otherwise set it to half
  # time res (half because the minor tick goes in between each
  # major tick).
  set nNumPoints [expr $nLength / 2]
  set nWidth [$gwGraph cget -width]
  set nWidthPerTick [expr $nWidth / $nNumPoints]
  if { $nWidthPerTick < $knMinWidthPerTick } {
      set nNumMarks [expr $nWidth / $knMinWidthPerTick]
      set nWidthPerTick [expr $nNumPoints / $nNumMarks]
      $gwGraph axis configure x -stepsize $nWidthPerTick
  } else {
      set nWidthPerTick [expr $gaLinkedVars(timeresolution) / 2]
      if { $nWidthPerTick < 1 } {
    set nWidthPerTick 1
      }
      $gwGraph axis configure x -stepsize $nWidthPerTick
  }

  # if we're subtracting the prestim avg..
  if { $gbPreStimOffset && $gaLinkedVars(numprestimpoints) > 0 } {

      # get the sum of all the points before the stim.
      set fPreStimSum 0
      set nNumPreStimPoints $gaLinkedVars(numprestimpoints)
      for { set nTP 0 } { $nTP < $nNumPreStimPoints } { incr nTP } {
    set nIndex [expr [expr $nTP * 2] + 1];
    set fPreStimSum [expr double($fPreStimSum) + double([lindex $lGraphData $nIndex])];
      }

      # find the avg.
      set fPreStimAvg [expr double($fPreStimSum) / double($nNumPreStimPoints)]
      # subtract from all points.
      for { set nTP 0 } { $nTP < $nNumPoints } { incr nTP } {
    set nIndex [expr [expr $nTP * 2] + 1];
    set fOldValue [lindex $lGraphData $nIndex]
    set fNewValue [expr double($fOldValue) - double($fPreStimAvg)]
    set lGraphData [lreplace $lGraphData $nIndex $nIndex $fNewValue]
      }
  }


  # graph the data
  $gwGraph element create line$dataSet \
    -data $lGraphData \
    -color $dataSet \
    -label $gGraphSetting($dataSet,label) \
    -pixels 2 \
    -linewidth 2
  
  # if we're drawing error bars...
  if { 1 == $gbErrorBars } {
      
      # get the data for this condition.
      if { [catch { set lErrors $gConditionData($nCondition,errors) } \
        sResult] } { continue; }
      
      # get the num of errors. save the highest number
      set nLength [llength $lErrors]
      if { $nLength > $gnMaxNumErrorBars } { 
    set gnMaxNumErrorBars $nLength
      }

      # for each value...
      for {set nErrorIndex 0} \
        {$nErrorIndex < $nLength} \
        {incr nErrorIndex} {
    
    # get error amt
    set nError [lindex $lErrors $nErrorIndex]
    
    # if 0, continue
    if { $nError == 0 } { continue; }
    
    # get the index of the data in the graph data list
    set nGraphIndex [expr $nErrorIndex * 2]
    
    # get the x/y coords at this point on the graph
    set nX [lindex $lGraphData $nGraphIndex]
    set nY [lindex $lGraphData [expr $nGraphIndex + 1]];
    
    # draw a graph line from the top to the bottom
    $gwGraph element create error$dataSet$nErrorIndex \
      -data [list $nX [expr $nY - $nError] \
      $nX [expr $nY + $nError] ] \
      -color $dataSet \
      -symbol splus \
      -label "" \
      -pixels 5
      }
  }
    }
    
    # draw some axes
    $gwGraph marker create line \
      -coords [list 0 -Inf 0 Inf] \
      -name xAxis
    $gwGraph marker create line \
      -coords [list -Inf 0 Inf 0] \
      -name yAxis
    
    # if autorange is off, set the min and max of the axes to the saved
    # amounts.
    if { $gbAutoRangeGraph == 0 } {
	$gwGraph axis configure y -min $gnFixedAxesSize(y1) \
	    -max $gnFixedAxesSize(y2)
    }
}

proc Graph_SaveToPS { isFileName } {
    global gwGraph
    catch {$gwGraph postscript output $isFileName}
}

proc Graph_UpdateSize { } {
    global gbAutoRangeGraph gwGraph gnFixedAxesSize
    if { $gbAutoRangeGraph == 0 } {
  set gnFixedAxesSize(y1) [lindex [$gwGraph axis limits y] 0]
  set gnFixedAxesSize(y2) [lindex [$gwGraph axis limits y] 1]
    } else {
  $gwGraph axis configure x y -min {} -max {}
    }
}

# zoom callbacks
proc Graph_Zoom { iwGraph inX1 inY1 inX2 inY2 } {
    
    if { $inX1 < $inX2 } {
	$iwGraph axis configure x -min $inX1 -max $inX2
    } elseif { $inX1 > $inX2 } {
	$iwGraph axis configure x -min $inX2 -max $inX1
    }
    if { $inY1 < $inY2 } {
	$iwGraph axis configure y -min $inY1 -max $inY2
    } elseif { $inY1 > $inY2 } {
	$iwGraph axis configure y -min $inY2 -max $inY1
    }
}
proc Graph_Unzoom { iwGraph } {
    $iwGraph axis configure x y -min {} -max {}
}
proc Graph_RegionStart { iwGraph inX inY } {
    global gnRegionStart
    $iwGraph marker create line -coords { } -name zoomBox \
      -dashes dash -xor yes
    set gnRegionStart(x) [$iwGraph axis invtransform x $inX]
    set gnRegionStart(y) [$iwGraph axis invtransform y $inY]
}
proc Graph_RegionMotion { iwGraph inX inY } {
    global gnRegionStart
    set nX [$iwGraph axis invtransform x $inX]
    set nY [$iwGraph axis invtransform y $inY]
    $iwGraph marker configure zoomBox -coords [list \
      $gnRegionStart(x) $gnRegionStart(y) \
      $gnRegionStart(x) $nY $nX $nY \
      $nX $gnRegionStart(y) \
      $gnRegionStart(x) $gnRegionStart(y)]
}
proc Graph_RegionEnd { iwGraph inX inY } {
    global gnRegionStart
    $iwGraph marker delete zoomBox
    set nX [$iwGraph axis invtransform x $inX]
    set nY [$iwGraph axis invtransform y $inY]
    Graph_Zoom $iwGraph $gnRegionStart(x) $gnRegionStart(y) $nX $nY
}

proc Graph_SetTestData { inNumConditions inNumTimePoints } {

    global gaLinkedVars

    Graph_ShowWindow
    Graph_BeginData
    
    set min [expr 0 - [expr $gaLinkedVars(numprestimpoints) * $gaLinkedVars(timeresolution)]]
    set max [expr [expr $inNumTimePoints * $gaLinkedVars(timeresolution)] + $min]
    for { set cn 0 } { $cn < $inNumConditions } { incr cn } {
  set lData {}
  for { set tp 0 } { $tp < $inNumTimePoints } { incr tp } {
      set second [expr [expr $tp * $gaLinkedVars(timeresolution)] - [expr $gaLinkedVars(numprestimpoints) * $gaLinkedVars(timeresolution)]];
      lappend lData $second [expr [expr $tp + 2] + $cn]
  }
  Graph_SetPointsData $cn $lData
    }
    
    Graph_EndData
}

proc Graph_SetNumConditions { inNumConditions } {

    global gnNumTimeCourseConditions 
    global glAllColors gnMaxColors glGraphColors gGraphSetting

    set gnNumTimeCourseConditions $inNumConditions

    # set the graph colors to the first num_conditions possible colors
    set nLastColorToUse [expr $gnNumTimeCourseConditions - 1]
    if { $nLastColorToUse > [expr $gnMaxColors - 1] } {
  set nLastColorToUse [expr $gnMaxColors - 1]
    }
    set glGraphColors [lrange $glAllColors 0 $nLastColorToUse]
    
    # make sure none of our graph setting conditions are invalid
    foreach dataSet $glGraphColors {
  if { $gGraphSetting($dataSet,condition) >= $gnNumTimeCourseConditions } {
      set gGraphSetting($dataSet,condition) 0
      set gGraphSetting($dataSet,visible)   0
  }
    }

}

proc Graph_DoConfigDlog {} {
    
    global gDialog gbShowTimeCourseOffsetOptions
    global gbErrorBars gbTimeCourseOffset
    global gGraphSetting gnNumTimeCourseConditions glGraphColors 
    global gbPreStimOffset
    global gaLinkedVars
    
    set wwDialog .wwTimeCourseConfigDlog
    
    if { [Dialog_Create $wwDialog "Configure Graph" {-borderwidth 10}] } {
  
  set nMaxCondition [expr $gnNumTimeCourseConditions - 1]
  
  set fwConditions          $wwDialog.fwConditions
  set lfwDisplay            $wwDialog.lfwDisplay
  set fwButtons             $wwDialog.fwButtons
  
  frame $fwConditions
  
  tkm_MakeBigLabel $fwConditions.fwNameLabel "Label"
  tkm_MakeBigLabel $fwConditions.fwVisibleLabel "Visible"
  tkm_MakeBigLabel $fwConditions.fwColorLabel "Color"
  tkm_MakeBigLabel $fwConditions.fwConditionLabel "Condition Shown"
  grid $fwConditions.fwNameLabel -column 0 -row 0 -padx 5
  grid configure $fwConditions.fwNameLabel -sticky w
  grid $fwConditions.fwColorLabel -column 1 -row 0 -padx 5
  grid configure $fwConditions.fwColorLabel -sticky w
  grid $fwConditions.fwVisibleLabel -column 2 -row 0 -padx 5
  grid configure $fwConditions.fwVisibleLabel -sticky w
  grid $fwConditions.fwConditionLabel -column 3 -row 0 -padx 5
  grid configure $fwConditions.fwConditionLabel -sticky w
  
  set nRow 1
  foreach dataSet $glGraphColors {
      
      set fw $fwConditions
      
      # entry for the name
      tkm_MakeEntry $fw.fwEntry$dataSet "" gGraphSetting($dataSet,label) 20
      
      # make a list of the args, then make a list of that list. 
      tkm_MakeNormalLabel $fw.fwLabel$dataSet "$dataSet"
      tkm_MakeCheckboxes $fw.cbVisible$dataSet y \
        [list [list text "" gGraphSetting($dataSet,visible) "Graph_Draw"]]
      
      # this goes on the right, an entry for the condition this
      # color is displaying.
      tkm_MakeEntryWithIncDecButtons \
        $fw.fwControl$dataSet \
        "Condition (0-$nMaxCondition)" \
        gGraphSetting($dataSet,condition) \
        "Graph_Draw" \
        1
      
      grid $fw.fwEntry$dataSet -column 0 -row $nRow -padx 5
      grid $fw.fwLabel$dataSet -column 1 -row $nRow -padx 5
      grid configure $fw.fwLabel$dataSet -sticky w -padx 5
      grid $fw.cbVisible$dataSet -column 2 -row $nRow -padx 5
      grid $fw.fwControl$dataSet -column 3 -row $nRow -padx 5

      incr nRow
  }

  tixLabelFrame $lfwDisplay \
    -label "Display" \
    -labelside acrosstop \
    -options { label.padX 5 }

  set fwDisplaySub          [$lfwDisplay subwidget frame]
  set fwOptions             $fwDisplaySub.fwOptions
  set fwPreStimPoints       $fwDisplaySub.fwPreStimPoints
  set fwTimeRes             $fwDisplaySub.fwTimeRes

  set lOffsetOptions {}
  if { $gbShowTimeCourseOffsetOptions == 1 } {
      set lOffsetOptions [list text "Show percent change" \
        gbTimeCourseOffset \
        {set gbTimeCourseOffset $gbTimeCourseOffset} ]
  } 

  tkm_MakeCheckboxes $fwOptions v [list \
    { text "Show error bars" gbErrorBars \
    {set gbErrorBars $gbErrorBars} } \
    { text "Automatically size graph" gbAutoRangeGraph \
    {set gbAutoRangeGraph $gbAutoRangeGraph } } \
    $lOffsetOptions \
    { text "Subtract pre-stim average" gbPreStimOffset \
    {set gbPreStimOffset $gbPreStimOffset} } ]

  tkm_MakeActiveLabel $fwPreStimPoints \
    "Number of pre-stim points: " gaLinkedVars(numprestimpoints) 5
  tkm_MakeActiveLabel $fwTimeRes \
    "Time resolution: " gaLinkedVars(timeresolution) 5


  tkm_MakeApplyCloseButtons $fwButtons $wwDialog {Graph_UpdateSize;Graph_Draw;}


  pack $fwConditions \
    $lfwDisplay $fwOptions \
    $fwPreStimPoints $fwTimeRes $fwButtons \
    -side top \
    -anchor w \
    -expand yes \
    -fill x
    }
}

proc Graph_ShowOffsetOptions { ibShow } {
    global gbShowTimeCourseOffsetOptions
    puts "Graph_ShowOffsetOptions $ibShow"
    set gbShowTimeCourseOffsetOptions $ibShow
}

# =============================================================== LABEL WINDOW

set ksLabelListWindowName "Labels"
# the number of labels we know about
set gnNumLabels 0
# the strucutre names we know about
set glStructures {}
# structure list widget
set glwStructures ""
# currently selected label
set gnSelectedLabel 0
# currently selected structure
set gnSelectedStructure 0

proc LblLst_CreateWindow { iwwTop } {

    global gwwLabelListWindow ksLabelListWindowName

    # creates the window and sets its title and initial size
    set gwwLabelListWindow $iwwTop
    toplevel $iwwTop
    wm title $iwwTop $ksLabelListWindowName
    wm geometry $iwwTop 450x500
    wm minsize $iwwTop 450 500

    # if they hit the close box for this window, it will just hide it
    # instead of destroying the window.
    wm protocol $iwwTop WM_DELETE_WINDOW { LblLst_HideWindow }
}

proc LblLst_CreateLabelList { ifwList } {

    global glwLabel gnNumLabels
    global gaLabelInfo
    global glStructures glwStructures
    global gaLinkedVars

    set fwTop             $ifwList
    set slwLabel          $fwTop.slwLabel
    set lwProperties      $fwTop.lwProperties
    set ewName            $fwTop.ewName
    set cbwVisible        $fwTop.cbwVisible
    set lwColor           $fwTop.lwColor
    set cpwColor          $fwTop.cpwColor
    set lwStructure       $fwTop.lwStructure 
    set bwCopy            $fwTop.bwCopy
    set alwStructure      $fwTop.alwStructure
    set slwStructure      $fwTop.slwStructure

    frame $fwTop -relief raised -border 2
    
    # this is the list box of labels
    tixScrolledListBox $slwLabel \
      -options { listbox.selectmode single } \
	-browsecmd LblLst_SelectHilitedLabel
    set glwLabel [$slwLabel subwidget listbox]
    

    # the info area for the selected label
    tkm_MakeBigLabel $lwProperties "Properties"

    tkm_MakeEntry $ewName "Name" gaLabelInfo(name) 20 {LblLst_SendCurrentInfo}

    tkm_MakeCheckboxes $cbwVisible x [list \
	  [list text "Visible" gaLabelInfo(visible) \
	       {LblLst_SendCurrentInfo; UpdateAndRedraw}] ]

    tkm_MakeNormalLabel $lwColor "Choose a color:"

    CreateColorPicker $cpwColor LblLst_SetCurrentLabelColor 8

    tkm_MakeNormalLabel $lwStructure "Or a structure:"

    # an active label and scrolling list for the structure.
    tkm_MakeActiveLabel $alwStructure "" gaLabelInfo(structureName) 10

    # a button for copying the structure name into the name field
    tkm_MakeButtons $bwCopy [list \
	 [list text "Set Name" \
	      {set gaLabelInfo(name) $gaLabelInfo(structureName);
	      LblLst_SendCurrentInfo} ] ]

    tixScrolledListBox $slwStructure \
	-options { listbox.selectmode single } \
	-browsecmd { LblLst_HandleStructureListClick }
    set glwStructures [$slwStructure subwidget listbox]

    grid $slwLabel       -column 0 -row 0 -sticky news -rowspan 8
    grid $lwProperties   -column 1 -row 0 -sticky w    -columnspan 2
    grid $ewName         -column 1 -row 1 -sticky we   -columnspan 2
    grid $cbwVisible     -column 1 -row 2 -sticky w    -columnspan 2
    grid $lwColor        -column 1 -row 3 -sticky ew   -columnspan 2
    grid $cpwColor       -column 1 -row 4 -sticky news -columnspan 2
    grid $lwStructure    -column 1 -row 5 -sticky w    -columnspan 2
    grid $alwStructure   -column 1 -row 6 -sticky we
    grid $bwCopy         -column 2 -row 6 -sticky ewns
    grid $slwStructure   -column 1 -row 7 -sticky news -columnspan 2

    # Space everything out properly so that the label list and the
    # structure list take up the most space.
    grid columnconfigure $fwTop 0 -weight 1
    grid columnconfigure $fwTop 1 -weight 1
    grid rowconfigure    $fwTop 7 -weight 1

    set gnNumLabels 0
}


proc LblLst_ShowWindow {} {
    global gwwLabelListWindow
    wm deiconify $gwwLabelListWindow
}

proc LblLst_HideWindow {} {
    global gwwLabelListWindow
    wm withdraw $gwwLabelListWindow
}

proc LblLst_UpdateInfo { inIndex isName inStructure ibVisible iRed iGreen iBlue } {

    global gaLabelInfo
    global glStructures
    global gnSelectedLabel
    global glwLabel
    global glwStructures
    global glLabelNames

    # delete the list entry in the list box and reinsert it with the
    # new name
    $glwLabel delete $inIndex
    $glwLabel insert $inIndex $isName

    # select these items in their respective list boxes.
    $glwLabel selection clear 0 end
    $glwLabel selection set $inIndex
    $glwLabel see $inIndex

    # update the info area too.
    set gaLabelInfo(name) $isName
    set gaLabelInfo(visible) $ibVisible
    set gaLabelInfo(red) $iRed
    set gaLabelInfo(green) $iGreen
    set gaLabelInfo(blue) $iBlue

    # if the structure is -1, it's a free label with a structure index
    # of -1, else look up the structure index.
    set gaLabelInfo(structureIndex) $inStructure
    if { -1 == $inStructure } {
	set gaLabelInfo(structureName) "Free"
    } else {
	set gaLabelInfo(structureName) [lindex $glStructures $inStructure]
    }

    # set the name in the list of names
    set glLabelNames [lreplace $glLabelNames $inIndex $inIndex $isName]
}

proc LblLst_SetCurrentLabelColor { iRed iGreen iBlue } {
    global gnSelectedLabel
    global gaLabelInfo

    # change the color.
    set gaLabelInfo(red) $iRed
    set gaLabelInfo(green) $iGreen
    set gaLabelInfo(blue) $iBlue

    # this automatically makes the label a free label. set its index
    # appropriately and give it a nice name.
    set gaLabelInfo(structureIndex) -1 
    set gaLabelInfo(structureName) "Free"

    # tell the c code what we've done.
    LblLst_SendCurrentInfo

    UpdateAndRedraw
}

proc LblLst_SelectHilitedLabel {} {

    global glwLabel


    # find the hilighted label in the list box and select it
    set nSelection [$glwLabel curselection]
    if {$nSelection != ""} {
	LblLst_SelectLabel $nSelection
    }
}

proc LblLst_SelectLabel { inIndex } {

    global gnSelectedLabel

    # select this label. this should in turn send us an update 
    # of its information.
    set gnSelectedLabel $inIndex
    labl_select $inIndex
}

proc LblLst_SendCurrentInfo {} {

    global gnSelectedLabel
    global glwLabel
    global gaLabelInfo
    global glwStructures
    
    # If the structure is not -1, get the selected structure name from
    # the list.
    if { -1 != $gaLabelInfo(structureIndex) } {
	set gaLabelInfo(structureName) [$glwStructures curselection]
    } 

    # send the contents of the label info.
    labl_set_info $gnSelectedLabel $gaLabelInfo(name) \
	$gaLabelInfo(structureIndex) $gaLabelInfo(visible) \
	$gaLabelInfo(red) $gaLabelInfo(green) $gaLabelInfo(blue)
    
    # delete the list entry in the list box and reinsert it with the
    # new name
    $glwLabel delete $gnSelectedLabel
    $glwLabel insert $gnSelectedLabel $gaLabelInfo(name)
}

proc LblLst_AddLabel { isName } {

    global glwLabel
    global glLabelNames
    global gnNumLabels

    # add a label entry to the end of the list box and list of names
    $glwLabel insert end $isName
    lappend glLabelNames $isName
    incr gnNumLabels
}

proc LblLst_RemoveLabel { inIndex } {

    global glwLabel
    global glLabelNames

    # delete the list entry in the list box and the list of names
    $glwLabel delete $inIndex
    set glLabelNames [lreplace $glLabelNames $inIndex $inIndex]
}

proc LblLst_SetHilitedStructure {} {
    global glwLabel gnSelectedLabel
    global glwStructures gnSelectedStructure
    global gaLabelInfo glStructures

    # if we have a selection...
    if { [$glwStructures curselection] != {} } {
	
	# find the hilighted label in the list box and select it
	set gnSelectedStructure [$glwStructures curselection]

	# update the structure
	set gaLabelInfo(structureIndex) $gnSelectedStructure
	set gaLabelInfo(structureName) [lindex $glStructures $gnSelectedStructure]
    }

    # now we have to reselect the label in the lable list since
    # clicking the strucutre list just unselected it. bah, stupid
    # tk. problem is, the structure list will actually call this
    # function twice, once on mouse down and once on mouse up, so
    # after we select the entry in the label list, the structure list
    # will lose its selection, and on the mouse up, when we get the
    # curselection up there, it will return empty. so we need to
    # enclose the stuff up there around that if statement.
    # UNFORTUNATELY, this still doesn't work, the label selection
    # isn't set again.
    $glwLabel selection set $gnSelectedLabel
}

proc LblLst_HandleStructureListClick {} {
    global glwLabel gnSelectedLabel
    global gnSelectedStructure
    global glwStructures

    if { [$glwStructures curselection] != $gnSelectedStructure &&
	 [$glwStructures curselection] != {} } {
	LblLst_SetHilitedStructure
	LblLst_SendCurrentInfo
	UpdateAndRedraw
    }

    $glwLabel selection set $gnSelectedLabel
}

proc LblLst_SetStructures { ilStructures } {

    global glStructures glwStructures

    # set our list of structures.
    set glStructures $ilStructures

    # if we have the widget, delete all the entries and reinsert them.
    if { $glwStructures != "" } {
	$glwStructures delete 0 end
	foreach structure $ilStructures {
	    $glwStructures insert end $structure
	}
    }
}

proc LblLst_FillListMenu { iowList {ibSelectCurrentLabel 0} } {
    global glLabelNames
    global gnSelectedLabel

    $iowList config -disablecallback 1

    set lEntries [$iowList entries]
    foreach entry $lEntries { 
	$iowList delete $entry
    }
    
    set nValueIndex 0
    foreach label $glLabelNames {
	$iowList add command $nValueIndex -label $label
	incr nValueIndex
    }
    
    $iowList config -disablecallback 0

    if { $ibSelectCurrentLabel && $nValueIndex > 0 } {
	$iowList config -value $gnSelectedLabel
    }
}

# ================================================================= FSGDF PLOT

proc GDF_Load { ifnGDF {iOverlay 0} } {
    global gbGDFLoaded gGDFID

    set ID [FsgdfPlot_Read $ifnGDF]
    if { $ID < 0 } { return }

    set gGDFID(overlay,$iOverlay) $ID
    FsgdfPlot_ShowWindow $gGDFID(overlay,$iOverlay)

    tkm_SetMenuItemGroupStatus mg_GDFLoaded 1
    set gbGDFLoaded 1
}

proc GDF_HideAllWindows {} {
    global gbGDFLoaded gGDFID gaLinkedVars
    if { ![info exists gbGDFLoaded] || !$gbGDFLoaded } { return }
    for { set n 0 } { $n < 9 } { incr n } {
	if { [info exists gGDFID(overlay,$n)] } {
	    FsgdfPlot_HideWindow $gGDFID(overlay,$n)
	}
    }
}

proc GDF_ShowCurrentWindow {} {
    global gbGDFLoaded gGDFID gaLinkedVars
    if { ![info exists gbGDFLoaded] || !$gbGDFLoaded } { return }
    if { [info exists gGDFID(overlay,$gaLinkedVars(currentvaluefield))] } {
	FsgdfPlot_ShowWindow $gGDFID(overlay,$gaLinkedVars(currentvaluefield))
    }
}

proc GDF_PlotAvgMarkedVerts {} {
    global gState
    set gState(lSelectedVnos) [get_marked_vnos]
    GDF_SendCurrentPoints
}

proc GDF_SendCurrentPoints {} {
    global gbGDFLoaded gGDFID gaLinkedVars gState
    if { ![info exists gbGDFLoaded] || !$gbGDFLoaded } { return }

    # Make sure we have points to send.
    if { ![info exists gState(lSelectedVnos)] || 
	 [llength $gState(lSelectedVnos)] == 0 } { return }

    set nCurOverlay $gaLinkedVars(currentvaluefield)
    set lMarkedVnos $gState(lSelectedVnos)

    FsgdfPlot_BeginPointList $gGDFID(overlay,$nCurOverlay) 
    foreach vno $lMarkedVnos {
	FsgdfPlot_AddPoint $gGDFID(overlay,$nCurOverlay) $vno 0 0
    }
    FsgdfPlot_EndPointList $gGDFID(overlay,$nCurOverlay) 

    if { [llength $lMarkedVnos] == 1 } {
	set gGDFState(info) "Vertex number [lindex $lMarkedVnos 0]"
    } else {
	set gGDFState(info) "Average of [llength $lMarkedVnos] Marked Vertices"
    }
    FsgdfPlot_SetInfo $gGDFID(overlay,$nCurOverlay) $gGDFState(info)
}


# ======================================================== COMPATIBILITY LAYER

proc setfile { iVarName isFileName } {
    upvar $iVarName localvar
    set localvar [ExpandFileName $isFileName]
}

proc save_rgb_named { isName } {

    global rgb named_rgbdir rel_rgbname

    # cp arg->global for uplevel
    set rel_rgbname $name

    # echo cfunc; update abbrev
    uplevel {setfile rgb $named_rgbdir/$rel_rgbname}

    # cfunc
    save_rgb_named_orig $rel_rgbname
}

# RKT - this can be called by an outside script to make tksurfer look
# at the rgb variable and update the variable in the RGB filename
# widget. i.e. if the user sets teh rgb env var to a file name and
# readenv.tcl is called, call this function to make tksurfer change
# its RGB filename to the value read in from the env.
proc update_rgb_filename {} {
    global rgb
    setfile rgb $rgb
}

# ======================================================================= MISC 

proc prompt {} {
 puts "% "
}

proc LoadSurface { isFileName } {
    global insurf hemi ext
    set insurf [ExpandFileName $isFileName kFileName_Surface]
    read_binary_surf
    UpdateAndRedraw
    set hemi [file rootname [file tail $insurf]]
    set ext [string trimleft [file tail $insurf] $hemi.]
}

proc GetDefaultLocation { iType } {
    global gsaDefaultLocation 
    global gsSubjectDirectory gsSegmentationColorTable env
    if { [info exists gsaDefaultLocation($iType)] == 0 } {
	switch $iType {
	    LoadSurface - SaveSurfaceAs - LoadMainSurface -
	    LoadInflatedSurface - LoadWhiteSurface - LoadPialSurface -
	    LoadOriginalSurface - LoadCurvature - SaveCurvatureAs -
	    LoadPatch - SavePatchAs -
	    SaveValuesAs {
		set gsaDefaultLocation($iType) \
		    [ExpandFileName "" kFileName_Surface]
	    }
	    LoadTimeCourse_Volume - LoadTimeCourse_Register -
	    LoadFieldSign - SaveFieldSignAs -
	    LoadFieldMask - SaveFieldMaskAs - 
	    LoadOverlay - LoadTimeCourse - 
	    LoadOverlayRegistration - LoadTimeCourseRegistration {
		set gsaDefaultLocation($iType) \
		    [ExpandFileName "" kFileName_FMRI]
	    }
	    LoadColorTable {
		set gsaDefaultLocation($iType) \
		    [ExpandFileName "" kFileName_CSURF]
	    }
	    LoadLabel - SaveLabelAs - ImportAnnotation - ExportAnnotation {
		set gsaDefaultLocation($iType) \
		    [ExpandFileName "" kFileName_Label]
	    }
	    SaveDipolesAs {
		set gsaDefaultLocation($iType) \
		    [ExpandFileName "" kFileName_BEM]
	    }
	    RunScript {
		set gsaDefaultLocation($iType) \
		    [ExpandFileName "" kFileName_Script]
	    }
	    WriteDecimation - SaveGraphToPS - 
	    WriteMarkedVerticesTCSummary - WriteLabelTCSummary -
	    SaveGDFPlotToPS - SaveGDFPlotToTable -
	    LoadGDF {
		set gsaDefaultLocation($iType) \
		    [ExpandFileName "" kFileName_Home]
	    }
	    SaveRGBAs {
		set gsaDefaultLocation($iType) \
		    [ExpandFileName "" kFileName_RGB]
	    }
	    SaveTIFFAs {
		set gsaDefaultLocation($iType) \
		    [ExpandFileName "" kFileName_TIFF]
	    }
	    default { 
		set gsaDefaultLocation($iType) [ExpandFileName "" $iType]
	    } 
	}
    } 
    return $gsaDefaultLocation($iType)
}

proc SetDefaultLocation { iType isValue } {
    global gsaDefaultLocation
    if { [string range $isValue 0 0] == "/" } {
	set gsaDefaultLocation($iType) $isValue
    }
}

proc ExpandFileName { isFileName {iFileType ""} } {

    global session home subject
    global gaFileNameDefDirs

    # look at the first char
    set sFirstChar [string range $isFileName 0 0]
    switch $sFirstChar {
	"/" {
	    set sExpandedFileName $isFileName
	}
	"~" {
      if { [string range $isFileName 1 1] == "/" } {
	  set sTail [string range $isFileName 2 end]
	  set sSubject $subject
	  set sSubDir [file dirname $sTail] ;# path portion of file name
	  set sFileName [file tail $sTail]  ;# only file name 
	  if { $sSubDir == "." } {
	      set sExpandedFileName \
		  $home/$sSubject/$sFileName 
	  } else {
        set sExpandedFileName \
	    $home/$sSubject/$sSubDir/$sFileName
	  }
      } else {
	  set sTail [string range $isFileName 1 end]
	  set sSubject [file dirname $sTail]
	  if { $sSubject == "." } {
	      set sSubject [file tail $sTail]
	      set sExpandedFileName \
		  $home/$sSubject
	  } else {
	      set sFileName [file tail $sTail]
	      set sExpandedFileName \
		  $home/$sSubject/$sFileName
	  }
      }
	}
	"*" {
	    set sExpandedFileName \
		$session/[string range $isFileName 2 end]
	} 
	"#" {
	    if { [info exists env(FREESURFER_HOME)] } {
		set sExpandedFileName \
		    $env(FREESURFER_HOME)/lib/tcl/[string range $isFileName 2 end]
	    } else {
		set sExpandedFileName $isFileName
	    }
	}
	default {
	    # see if we have a file name type and can find a default
	    # subdirectory.
	    set sSubDir ""
	    if { $iFileType != "" } {
		if { [info exists gaFileNameDefDirs($iFileType)] } {
		    set sSubDir $gaFileNameDefDirs($iFileType)
		}
	    }
	    # if the first char is a slash, just append the filename.
	    set sFirstChar [string range $sSubDir 0 0]
	    if { $sFirstChar == "/" } {
		set sExpandedFileName $sSubDir/$isFileName
	    } else {
		puts "shouldn't get here: $sSubDir"
		
		if { $sSubDir == "" } {
		    puts "No expansion found!!!!"
		    set sExpandedFileName $isFileName
		} elseif { $sSubDir == "." } {
		    set sExpandedFileName \
			$home/$subject/$isFileName 
		} else {
		    set sExpandedFileName \
			$home/$subject/$sSubDir/$isFileName
		}
	    }
	}
    }
    
    return $sExpandedFileName
}

proc UpdateLockButton { ibwButton ibFlag } {

    upvar #0 $ibFlag bFlag
    if { $bFlag == 0 } {
  set bFlag 1
  $ibwButton config -relief sunken
    } else {
  set bFlag 0
  $ibwButton config -relief raised
    }

    SendLinkedVarGroup redrawlock
}

proc UpdateAndRedraw {} {
    # using after so that dlogs obscuring the graphics window can 
    # close before the redraw.
    after 500 { redraw; }
}

proc RestoreView {} {
    global flag2d
    ResetTransform
    make_lateral_view
    if {$flag2d} {
  restore_zero_position
  rotate_brain_x -90
    }
    UpdateAndRedraw
}

proc DoTransform { } {
    global gNextTransform
    rotate_brain_x $gNextTransform(rotate,x)
    rotate_brain_y $gNextTransform(rotate,y)
    rotate_brain_z $gNextTransform(rotate,z)
    translate_brain_x $gNextTransform(translate,x)
    translate_brain_y $gNextTransform(translate,y)
    scale_brain [expr $gNextTransform(scale)/100.0]
}

proc ResetTransform { } {
    global gNextTransform kanDefaultTransform
    set gNextTransform(rotate,degrees) $kanDefaultTransform(rotate)
    set gNextTransform(translate,dist) $kanDefaultTransform(translate)
    set gNextTransform(scale,amt) $kanDefaultTransform(scale)
}

proc DoSmoothOverlay { inSteps inField } {

    sclv_smooth $inSteps $inField
    RequestOverlayInfoUpdate
}

proc DoSmoothCurvature { inSteps } {

    smooth_curv $inSteps
}

proc DoInflate { inSteps } {
    shrink $inSteps
}

proc DoDecimation { isFileName ifSpacing } { 
    global dec
    
    if { $ifSpacing > 0.999} {
	subsample_dist $ifSpacing
    } else {
	subsample_orient $ifSpacing
    }
    
    # write decimation
    set dec [ExpandFileName $isFileName kFileName_BEM]
    write_binary_decimation
}

proc DoSavePoint {} {
    find_orig_vertex_coordinates
}

proc DoGotoPoint {} {
    select_orig_vertex_coordinates
}


proc CreateImages {} {

    global ksImageDir

    foreach image_name { icon_cursor_goto icon_cursor_save
	icon_arrow_up icon_arrow_down icon_arrow_left icon_arrow_right
	icon_arrow_rot_z_pos icon_arrow_rot_z_neg
	icon_zoom_in icon_zoom_out
	icon_arrow_rot_y_neg icon_arrow_rot_y_pos
	icon_arrow_rot_x_neg icon_arrow_rot_x_pos
	icon_cut_area icon_cut_closed_line icon_cut_line
	icon_marked_to_label icon_label_to_marked icon_erase_label
	icon_color_label
	icon_cut_plane icon_cut_clear
	icon_draw_line icon_draw_line_closed icon_fill_label icon_erase_line
	icon_surface_main icon_surface_original icon_surface_pial
	icon_home icon_redraw } {

  if { [catch {image create photo  $image_name -file \
    [ file join $ksImageDir $image_name.gif ]} sResult] != 0 } {
      dputs "Error loading $image_name:"
      dputs $sResult
  }
    }
}


proc SetKeyBindings {} {

    # redraw
    bind . <Alt-r> { UpdateAndRedraw }

    # movement 
    bind . <Alt-Up>    { rotate_brain_x 18.0; UpdateAndRedraw }
    bind . <Alt-Down>  { rotate_brain_x -18.0; UpdateAndRedraw }
    bind . <Alt-Right> { rotate_brain_y -18.0; UpdateAndRedraw }
    bind . <Alt-Left>  { rotate_brain_y 18.0; UpdateAndRedraw }
    bind . <Alt-Shift-Left>  { rotate_brain_y 180.0; UpdateAndRedraw }
    bind . <Alt-Shift-Right>  { rotate_brain_y -180.0; UpdateAndRedraw }
    bind . <Alt-KP_Next>  { rotate_brain_z 10.0; UpdateAndRedraw }
    bind . <Alt-KP_Prior> { rotate_brain_z -10.0; UpdateAndRedraw }
    bind . <Alt-KP_Right>   { translate_brain_x 10.0; UpdateAndRedraw }
    bind . <Alt-KP_Left>    { translate_brain_x -10.0; UpdateAndRedraw }
    bind . <Alt-KP_Up>      { translate_brain_y 10.0; UpdateAndRedraw }
    bind . <Alt-KP_Down>    { translate_brain_y -10.0; UpdateAndRedraw }
    bind . <Alt-braceleft>    { scale_brain 0.75; UpdateAndRedraw }
    bind . <Alt-braceright>   { scale_brain 1.25; UpdateAndRedraw }
    bind . <Alt-bracketleft>  { scale_brain 0.95; UpdateAndRedraw }
    bind . <Alt-bracketright> { scale_brain 1.05; UpdateAndRedraw }
}

set tDlogSpecs(LoadSurface) \
    [list \
	 -title "Load Surface" \
	 -prompt1 "Load Surface:" \
	 -default1 [list GetDefaultLocation LoadSurface] \
	 -entry1 [list GetDefaultLocation LoadSurface] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the surface" \
	 -okCmd {
	     LoadSurface %s1; 
	     SetDefaultLocation LoadSurface %s1} ]
set tDlogSpecs(SaveSurfaceAs) \
    [list \
	 -title "Save Surface As" \
	 -prompt1 "Save Surface:" \
	 -default1 [list GetDefaultLocation SaveSurfaceAs] \
	 -entry1 [list GetDefaultLocation SaveSurfaceAs] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the surface to save" \
	 -okCmd {
	     set outsurf [ExpandFileName %s1 kFileName_Surface];
	     CheckFileAndDoCmd $outsurf write_binary_surface;
	     SetDefaultLocation SaveSurfaceAs %s1} ]
set tDlogSpecs(LoadMainSurface) \
    [list \
	 -title "Load Main Vertices" \
	 -prompt1 "Load Main Vertices:" \
	 -default1 [list GetDefaultLocation LoadMainSurface] \
	 -entry1 [list GetDefaultLocation LoadMainSurface] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the surface to load into main vertices" \
	 -okCmd {
	     set filename [ExpandFileName %s1 kFileName_Surface];
	     read_surface_vertex_set 0 $filename;
	     SetDefaultLocation LoadMainSurface %s1;
	     set gaLinkedVars(vertexset) 0;
	     set_current_vertex_set $gaLinkedVars(vertexset);
	     UpdateLinkedVarGroup view;
	     UpdateAndRedraw} ]
set tDlogSpecs(LoadInflatedSurface) \
    [list \
	 -title "Load Inflated Vertices" \
	 -prompt1 "Load Inflated Vertices:" \
	 -default1 [list GetDefaultLocation LoadInflatedSurface] \
	 -entry1 [list GetDefaultLocation LoadInflatedSurface] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the surface to load into inflated vertices" \
	 -okCmd {
	     set filename [ExpandFileName %s1 kFileName_Surface];
	     read_surface_vertex_set 1 $filename;
	     SetDefaultLocation LoadInflatedSurface %s1;
	     set gaLinkedVars(vertexset) 1;
	     set_current_vertex_set $gaLinkedVars(vertexset);
	     UpdateLinkedVarGroup view;
	     UpdateAndRedraw} ]
set tDlogSpecs(LoadWhiteSurface) \
    [list \
	 -title "Load White Vertices" \
	 -prompt1 "Load White Vertices:" \
	 -default1 [list GetDefaultLocation LoadWhiteSurface] \
	 -entry1 [list GetDefaultLocation LoadWhiteSurface] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the surface to load into white vertices" \
	 -okCmd {
	     set filename [ExpandFileName %s1 kFileName_Surface];
	     read_surface_vertex_set 2 $filename;
	     SetDefaultLocation LoadWhiteSurface %s1;
	     set gaLinkedVars(vertexset) 2;
	     set_current_vertex_set $gaLinkedVars(vertexset);
	     UpdateLinkedVarGroup view;
	     UpdateAndRedraw} ]
set tDlogSpecs(LoadPialSurface) \
    [list \
	 -title "Load Pial Vertices" \
	 -prompt1 "Load Pial Vertices:" \
	 -default1 [list GetDefaultLocation LoadPialSurface] \
	 -entry1 [list GetDefaultLocation LoadPialSurface] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the surface to load into pial vertices" \
	 -okCmd {
	     set filename [ExpandFileName %s1 kFileName_Surface];
	     read_surface_vertex_set 3 $filename;
	     SetDefaultLocation LoadPialSurface %s1;
	     set gaLinkedVars(vertexset) 3;
	     set_current_vertex_set $gaLinkedVars(vertexset);
	     UpdateLinkedVarGroup view;
	     UpdateAndRedraw} ]
set tDlogSpecs(LoadOriginalSurface) \
    [list \
	 -title "Load Original Vertices" \
	 -prompt1 "Load Original Vertices:" \
	 -default1 [list GetDefaultLocation LoadOriginalSurface] \
	 -entry1 [list GetDefaultLocation LoadOriginalSurface] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the surface to load into original vertices" \
	 -okCmd {
	     set filename [ExpandFileName %s1 kFileName_Surface];
	     read_surface_vertex_set 4 $filename;
	     SetDefaultLocation LoadOriginalSurface %s1;
	     set gaLinkedVars(vertexset) 4;
	     set_current_vertex_set $gaLinkedVars(vertexset);
	     UpdateLinkedVarGroup view;
	     UpdateAndRedraw} ]

set tDlogSpecs(LoadTimeCourse) \
    [list \
	 -title "Load Time Course" \
	 -prompt1 "Load Volume:" \
	 -type1 dir \
	 -presets1 $glShortcutDirs \
	 -note1 "The directory containing the binary volume to load" \
	 -default1 [list GetDefaultLocation LoadTimeCourse_Volume] \
	 -entry1 [list GetDefaultLocation LoadTimeCourse_Volume] \
	 -presets1 $glShortcutDirs \
	 -prompt2 "Stem:" \
	 -type2 text \
	 -note2 "The stem of the binary volume" \
	 -prompt3 "Registration File:" \
	 -presets3 $glShortcutDirs \
	 -note3 "The file name of the registration file to load. Leave blank to use register.dat in the same directory." \
	 -default3 [list GetDefaultLocation LoadTimeCourse_Register] \
	 -entry3 [list GetDefaultLocation LoadTimeCourse_Register] \
	 -presets3 $glShortcutDirs \
	 -okCmd {
	     set fnVolume [ExpandFileName %s1 kFileName_FMRI];
	     set fnRegister [ExpandFileName %s3 kFileName_FMRI];
	     func_load_timecourse $fnVolume %s2 $fnRegister;
	     SetDefaultLocation LoadTimeCourse_Volume %s1;
	     SetDefaultLocation LoadTimeCourse_Register %s3}]

set tDlogSpecs(LoadCurvature) \
    [list \
	 -title "Load Curvature" \
	 -prompt1 "Load Curvature:" \
	 -default1 [list GetDefaultLocation LoadCurvature] \
	 -entry1 [list GetDefaultLocation LoadCurvature] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the curvature data" \
	 -okCmd {
	     set curv [ExpandFileName %s1 kFileName_Surface];
	     read_binary_curv;
	     SetDefaultLocation LoadCurvature %s1;
	     UpdateAndRedraw; } ]
set tDlogSpecs(SaveCurvatureAs) \
    [list \
	 -title "Save Curvature As" \
	 -prompt1 "Save Curvature:" \
	 -default1 [list GetDefaultLocation SaveCurvatureAs] \
	 -entry1 [list GetDefaultLocation SaveCurvatureAs] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the curvature data to save" \
	 -okCmd {set curv [ExpandFileName %s1 kFileName_Surface];
	     SetDefaultLocation SaveCurvatureAs %s1;
	     CheckFileAndDoCmd $curv write_binary_curv} ]

set tDlogSpecs(LoadPatch) \
    [list \
	 -title "Load Patch" \
	 -prompt1 "Load Patch:" \
	 -default1 [list GetDefaultLocation LoadPatch] \
	 -entry1 [list GetDefaultLocation LoadPatch] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the patch data" \
	 -okCmd {
	     set patch [ExpandFileName %s1 kFileName_Surface];
	     SetDefaultLocation LoadPatch %s1;
	     read_binary_patch; 
	     RestoreView; 
	     UpdateAndRedraw; } ]
set tDlogSpecs(SavePatchAs) \
    [list \
	 -title "Save Patch As" \
	 -prompt1 "Save Patch:" \
	 -default1 [list GetDefaultLocation SavePatchAs] \
	 -entry1 [list GetDefaultLocation SavePatchAs] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the patch data to save" \
	 -okCmd {
	     set patch [ExpandFileName %s1 kFileName_Surface];
	     SetDefaultLocation SavePatchAs %s1
	     CheckFileAndDoCmd $patch write_binary_patch} ]

set tDlogSpecs(LoadColorTable) \
    [list \
	 -title "Load Color Table" \
	 -prompt1 "Load Color Table:" \
	 -default1 [list GetDefaultLocation LoadColorTable] \
	 -entry1 [list GetDefaultLocation LoadColorTable] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the color table" \
	 -okCmd {
	     labl_load_color_table [ExpandFileName %s1 kFileName_CSURF];
	     SetDefaultLocation LoadColorTable %s1;
	     UpdateLinkedVarGroup label} ]
set tDlogSpecs(LoadLabel) \
    [list \
	 -title "Load Label" \
	 -prompt1 "Load Label:" \
	 -default1 [list GetDefaultLocation LoadLabel] \
	 -entry1 [list GetDefaultLocation LoadLabel] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the label data" \
	 -okCmd {
	     labl_load [ExpandFileName %s1 kFileName_Label];
	     SetDefaultLocation LoadLabel %s1;
	     UpdateAndRedraw;  }]
set tDlogSpecs(SaveLabelAs) \
    [list \
	 -title "Save Selected Label" \
	 -prompt1 "Save Selected Label:" \
	 -default1 [list GetDefaultLocation SaveLabelAs] \
	 -entry1 [list GetDefaultLocation SaveLabelAs] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the label data to save" \
	 -okCmd {
	     labl_save $gnSelectedLabel [ExpandFileName %s1 kFileName_Label];
	     SetDefaultLocation SaveLabelAs %s1;
	     UpdateAndRedraw;  }]
set tDlogSpecs(ImportAnnotation) \
    [list \
	 -title "Import Annotaion" \
	 -prompt1 "Import Annotation:" \
	 -default1 [list GetDefaultLocation ImportAnnotation] \
	 -entry1 [list GetDefaultLocation ImportAnnotation] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the annotaion" \
	 -okCmd {
	     labl_import_annotation [ExpandFileName %s1 kFileName_Label];
	     SetDefaultLocation ImportAnnotation %s1;
	     UpdateAndRedraw;  }]
set tDlogSpecs(ExportAnnotation) \
    [list \
	 -title "Export Annotaion" \
	 -prompt1 "Export Annotation:" \
	 -default1 [list GetDefaultLocation ExportAnnotation] \
	 -entry1 [list GetDefaultLocation ExportAnnotation] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the annotaion to save" \
	 -okCmd {
	     labl_export_annotation [ExpandFileName %s1 kFileName_Label];
	     SetDefaultLocation ExpandFileName %s1;
	     UpdateAndRedraw; }]

set tDlogSpecs(SaveDipolesAs) [list \
  -title "Save Dipoles As" \
  -prompt1 "Save Dipoles As:" \
  -default1 [list GetDefaultLocation SaveDipolesAs] \
  -entry1 [list GetDefaultLocation SaveDipolesAs] \
  -presets1 $glShortcutDirs \
  -note1 "The file name of the dipoles data to save" \
  -okCmd {
      set dip [ExpandFileName %s1 kFileName_BEM];
      SetDefaultLocation SaveDipolesAs %s1;
      CheckFileAndDoCmd $dip write_binary_dipoles;} ]

set tDlogSpecs(LoadFieldSign) \
    [list \
	 -title "Load Field Sign" \
	 -prompt1 "Load Field Sign:" \
	 -default1 [list GetDefaultLocation LoadFieldSign] \
	 -entry1 [list GetDefaultLocation LoadFieldSign] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the field sign data" \
	 -okCmd {
	     set fs [ExpandFileName %s1 kFileName_FMRI]; 
	     SetDefaultLocation LoadFieldSign %s1;
	     read_fieldsign;
	     RestoreView;  }]
set tDlogSpecs(SaveFieldSignAs) \
    [list \
	 -title "Save Field Sign As" \
	 -prompt1 "Save Field Sign:" \
	 -default1 [list GetDefaultLocation SaveFieldSignAs] \
	 -entry1 [list GetDefaultLocation SaveFieldSignAs] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the field sign data to save" \
	 -okCmd {
	     SetDefaultLocation SaveFieldSignAs %s1;
	     set fs [ExpandFileName %s1 kFileName_FMRI]; 
	     CheckFileAndDoCmd $fs write_fieldsign  }]

set tDlogSpecs(LoadFieldMask) \
    [list \
	 -title "Load Field Mask" \
	 -prompt1 "Load Field Mask:" \
	 -default1 [list GetDefaultLocation LoadFieldMask] \
	 -entry1 [list GetDefaultLocation LoadFieldMask] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the field mask data" \
	 -okCmd {
	     SetDefaultLocation LoadFieldMask %s1;
	     set fm [ExpandFileName %s1 kFileName_FMRI];
	     read_fsmask; 
	     RestoreView;} ]
set tDlogSpecs(SaveFieldMaskAs) \
    [list \
	 -title "Save Field Mask As" \
	 -prompt1 "Save Field Mask:" \
	 -default1 [list GetDefaultLocation SaveFieldMaskAs] \
	 -entry1 [list GetDefaultLocation SaveFieldMaskAs] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the field mask data to save" \
	 -okCmd {
	     SetDefaultLocation SaveFieldMaskAs %s1;
	     set fm [ExpandFileName %s1 kFileName_FMRI];
	     CheckFileAndDoCmd $fm write_fsmask }]

set tDlogSpecs(RunScript) \
    [list \
	 -title "Run Script" \
	 -prompt1 "Run Script:" \
	 -default1 [list GetDefaultLocation RunScript] \
	 -entry1 [list GetDefaultLocation RunScript] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the TCL script to run" \
	 -okCmd {
	     SetDefaultLocation RunScript %s1
	     source [ExpandFileName %s1 kFileName_Script] }]

set tDlogSpecs(SaveGraphToPS) \
    [list \
	 -title "Save Time Course" \
	 -prompt1 "Save Time Course As:" \
	 -note1 "The file name of the PostScript file to create" \
	 -default1 [list GetDefaultLocation SaveGraphToPS] \
	 -entry1 [list GetDefaultLocation SaveGraphToPS] \
	 -presets1 $glShortcutDirs \
	 -okCmd {
	     SetDefaultLocation SaveGraphToPS %s1;
	     Graph_SaveToPS [ExpandFileName %s1 kFileName_Home]} ]

set tDlogSpecs(SaveRGBAs) \
    [list \
	 -title "Save RGB" \
	 -prompt1 "Save RGB As:" \
	 -default1 [list GetDefaultLocation SaveRGBAs] \
	 -entry1 [list GetDefaultLocation SaveRGBAs] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the RGB file to save" \
	 -okCmd {
	     SetDefaultLocation SaveRGBAs %s1;
	     set rgb [ExpandFileName %s1 kFileName_RGB]; save_rgb} ]

set tDlogSpecs(SaveTIFFAs) \
    [list \
	 -title "Save TIFF" \
	 -prompt1 "Save TIFF As:" \
	 -default1 [list GetDefaultLocation SaveTIFFAs] \
	 -entry1 [list GetDefaultLocation SaveTIFFAs] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the TIFF file to save" \
	 -okCmd {
	     SetDefaultLocation SaveTIFFAs %s1;
	     save_tiff [ExpandFileName %s1 kFileName_TIFF]} ]

set tDlogSpecs(WriteMarkedVerticesTCSummary) \
    [list \
	 -title "Save Marked Vertices Summary" \
	 -prompt1 "Save Summary As:" \
	 -default1 [list GetDefaultLocation WriteMarkedVerticesTCSummary] \
	 -entry1 [list GetDefaultLocation WriteMarkedVerticesTCSummary] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the summary text file to create" \
	 -okCmd { 
	     SetDefaultLocation WriteMarkedVerticesTCSummary %s1;
	     func_select_marked_vertices;
	     func_print_timecourse_selection \
		 [ExpandFileName %s1 kFileName_Home] } ]
set tDlogSpecs(WriteLabelTCSummary) \
    [list \
	 -title "Save Label Summary" \
	 -prompt1 "Save Summary As:" \
	 -default1 [list GetDefaultLocation WriteLabelTCSummary] \
	 -entry1 [list GetDefaultLocation WriteLabelTCSummary] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the summary text file to create" \
	 -okCmd { 
	     SetDefaultLocation WriteLabelTCSummary %s1;
	     clear_all_vertex_marks;
	     labl_mark_vertices $gnSelectedLabel;
	     func_select_marked_vertices;
	     func_print_timecourse_selection \
		 [ExpandFileName %s1 kFileName_Home] } ]

set tDlogSpecs(SaveGDFPlotToPS) \
    [list \
	 -title "Save Group Plot" \
	 -prompt1 "Save Plot As:" \
	 -note1 "The file name of the PostScript file to create" \
	 -default1 [list GetDefaultLocation SaveGDFPlotToPS] \
	 -entry1 [list GetDefaultLocation SaveGDFPlotToPS] \
	 -presets1 $glShortcutDirs \
	 -okCmd {
	     SetDefaultLocation SaveGDFPlotToPS %s1;
	     FsgdfPlot_SaveToPostscript \
		 $gGDFID(overlay,$gaLinkedVars(currentvaluefield)) \
		 [ExpandFileName %s1 kFileName_Home]  }]
set tDlogSpecs(SaveGDFPlotToTable) \
    [list \
	 -title "Save Group Data" \
	 -prompt1 "Save Plot As:" \
	 -note1 "The file name of the table to create" \
	 -default1 [list GetDefaultLocation SaveGDFPlotToTable] \
	 -entry1 [list GetDefaultLocation SaveGDFPlotToTable] \
	 -presets1 $glShortcutDirs \
	 -okCmd {
	     SetDefaultLocation SaveGDFPlotToTable %s1;
	     FsgdfPlot_SaveToTable \
		 $gGDFID(overlay,$gaLinkedVars(currentvaluefield)) \
		 [ExpandFileName %s1 kFileName_Home]  }]
set tDlogSpecs(MaskLabel) \
    [list \
	 -title "Mask Values to Label" \
	 -prompt1 "Label:" \
	 -default1 [list GetDefaultLocation LoadLabel] \
	 -entry1 [list GetDefaultLocation LoadLabel] \
	 -presets1 $glShortcutDirs \
	 -note1 "The file name of the label data" \
	 -okCmd {
	     mask_label [ExpandFileName %s1 kFileName_Label];
	     SetDefaultLocation LoadLabel %s1;
	     UpdateAndRedraw;  }]




proc CheckFileAndDoCmd { iFile iFunction } {

    global gDialog
    
    if { [file exists $iFile] == 0 } {
  $iFunction
    } else {

  set wwDialog .wwOKReplaceDlog
  if { [Dialog_Create $wwDialog "File Exists" {-borderwidth 10}] } {
      
      set fwWarning          $wwDialog.fwWarning
      set fwMessage          $wwDialog.fwMessage
      set fwButtons          $wwDialog.fwButtons
      
      tkm_MakeBigLabel $fwWarning "$iFile exists."
      tkm_MakeNormalLabel $fwMessage "Okay to overwrite?"
      
      # buttons.
      tkm_MakeCancelOKButtons $fwButtons $wwDialog [list $iFunction]
      
      pack $fwWarning $fwMessage $fwButtons \
        -side top       \
        -expand yes     \
        -fill x         \
        -padx 5         \
        -pady 5
  }
    }
}

# ======================================================================= MAIN

CreateImages

set wwTop        .w
set fwMenuBar    $wwTop.fwMenuBar
set fwToolBar    $wwTop.fwToolBar
set fwNavigation $wwTop.fwNavigation
set fwCursor     $wwTop.fwCursor
set fwMouseover  $wwTop.fwMouseover

CreateWindow         $wwTop
CreateMenuBar        $fwMenuBar
CreateToolBar        $fwToolBar
#CreateNavigationArea $fwNavigation
CreateSmallNavigationArea $fwNavigation
CreateCursorFrame    $fwCursor
CreateMouseoverFrame $fwMouseover

pack $fwMenuBar $fwToolBar $fwNavigation \
  -side top \
  -expand true \
  -fill x

pack $fwCursor $fwMouseover \
  -side left \
  -padx 5 \
  -expand true \
  -fill x \
  -fill y \
  -anchor s

pack $wwTop

ShowToolBar main 1

# set up graph window 
CreateGraphWindow .wwGraph
CreateGraphFrame .wwGraph.gwGraph
Graph_UpdateSize
Graph_HideWindow

# set up label list window
LblLst_CreateWindow .wwLabelList
LblLst_CreateLabelList .wwLabelList.lwLabel
pack .wwLabelList.lwLabel \
  -fill both \
  -expand yes
LblLst_HideWindow

# wacky bindings
SetKeyBindings

# enable default labels
ShowLabel kLabel_VertexIndex 1
ShowLabel kLabel_Coords_RAS 1
ShowLabel kLabel_Coords_Tal 1

# make sure window is shoowing
MoveToolWindow 0 0

# try loading a default color table. catch it so it doesn't complain
# if we fail.
catch {
    labl_load_color_table $env(FREESURFER_HOME)/surface_labels.txt
}


# Init the fsgdf code.
set gbGDFLoaded 0
tkm_SetMenuItemGroupStatus mg_GDFLoaded 0
FsgdfPlot_Init

# we did it!
dputs "Successfully parsed tksurfer.tcl"

# now try parsing the prefs files. first look in
# $FREESURFER_HOME/lib/tcl/tksurfer_init.tcl, then
# $SUBJECTS_DIR/scripts/tksurfer_init.tcl, then
# $subject/scripts/tksurfer_init.tcl, then
# ~/scripts/tksurfer_init.tcl.
foreach fnUserScript [list $env(FREESURFER_HOME)/lib/tcl/tksurfer_init.tcl $env(SUBJECTS_DIR)/scripts/tksurfer_init.tcl $home/$subject/scripts/tksurfer_init.tcl ~/tksurfer_init.tcl] {
    if { [file exists $fnUserScript] } {
	catch { 
	    dputs "Reading $fnUserScript"
	    source $fnUserScript
	    dputs "Successfully parsed $fnUserScript"
	}
    }
}

