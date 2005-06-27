#! /usr/bin/tixwish

# $Id: tkm_functional.tcl,v 1.24.2.3 2005/06/27 19:51:08 kteich Exp $

package require BLT;

# constants
set ksWindowName "Time Course"
set knLineWidth(active) 4
set knLineWidth(inactive) 2
set knMinWidthPerTick 40

# FunV_tDisplayFlag
set FunV_tDisplayFlag_Ol_TruncateNegative 0
set FunV_tDisplayFlag_Ol_TruncatePositive 1
set FunV_tDisplayFlag_Ol_ReversePhase     2
set FunV_tDisplayFlag_Ol_OffsetValues     3
set FunV_tDisplayFlag_Ol_IgnoreThreshold  4
set FunV_tDisplayFlag_Ol_Grayscale        5
set FunV_tDisplayFlag_Ol_Opaque           6
set FunV_tDisplayFlag_TC_GraphWindowOpen  7
set FunV_tDisplayFlag_TC_OffsetValues     8
set FunV_tDisplayFlag_TC_PreStimOffset    9

# FunD_tSampleType
set FunD_tSampleType(nearest)   0
set FunD_tSampleType(trilinear) 1

# our global vars
set gnTimePoint      0
set gnTimeSecond     0
set gnCondition      0
set gbTruncateNegative 0
set gbTruncatePositive 0
set gbReverse        0
set gbIgnoreThreshold      0
set gbGrayscale      0
set gbOpaque         0
set gbOverlayOffset  0
set gbShowOverlayOffsetOptions   0
set gfThreshold(min)   0
set gfThreshold(mid)   0
set gfThreshold(slope) 0
set gfOverlayRange(max) 0
set gfOverlayRange(min) 0
set gnOverlayNumTimePoints 0
set gnOverlayNumConditions 0
set gsOverlayDataName ""
set gfOverlayAlpha 1.0
set gOverlaySampleType $FunD_tSampleType(nearest)
set gFDRRate 0.05
set gbFDMask 0

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
set glGraphColors [lrange $glAllColors 0 end]
set gbErrorBars                     0
set gbTimeCourseOffset              0
set gbShowTimeCourseOffsetOptions   0
set gnPreStimPoints                 0
set gnTimeResolution                0
set gnMaxNumErrorBars               0
set gnNumDataSets                0
set gnNumTimeCourseConditions    0
set gsTimeCourseLocation ""
set gsTimeCourseDataName ""
set gbAutoRangeGraph 1
set gbPreStimOffset 0
set gnFixedGraphCoords(x1) 0
set gnFixedGraphCoords(x2) 0
set gnFixedGraphCoords(y1) 0
set gnFixedGraphCoords(y2) 0

set gbFunctionalWindowOpen 0

# ============================================================== DRAWING GRAPH

proc TimeCourse_BeginDrawingGraph {} {
}

proc TimeCourse_ClearData {} {
    global gwGraph
    set lElements [$gwGraph element names *]
    foreach element $lElements {
  $gwGraph element delete $element
    }
}

proc TimeCourse_UpdateGraphData { inDataSet ilPoints } {
    global glGraphData gnNumDataSets

    # save our graph data
    set glGraphData($inDataSet) $ilPoints
    if {[expr $inDataSet + 1] > $gnNumDataSets} {
	set gnNumDataSets [expr $inDataSet + 1]
    }
}

proc TimeCourse_UpdateErrorData { inDataSet ilErrors } {
    global glErrorData
    set glErrorData($inDataSet) $ilErrors
}

proc TimeCourse_EndDrawingGraph {} {
    TimeCourse_DrawGraph
}

proc TimeCourse_DrawGraph {} {

    global knMinWidthPerTick
    global gwGraph gnFixedGraphCoords
    global glErrorData glGraphData gGraphSetting glGraphColors
    global gbErrorBars gbAutoRangeGraph
    global gnMaxNumErrorBars gnNumDataSets
    global gnTimeResolution gbPreStimOffset gnPreStimPoints

    TimeCourse_ClearData

    # if no data, return
    if {$gnNumDataSets == 0} {
	return;
    }
    
    
    foreach dataSet $glGraphColors {
	
	# if not visible, draw some dummy data in this color. make
	# sure to use the already established min and max as the dummy
	# data so we don't mess up our range. 
	if { $gGraphSetting($dataSet,visible) == 0 } {
	    continue;
	}
	
	# get the condition index for this color
	set nCondition $gGraphSetting($dataSet,condition)
	
	# make sure we have this data
	if { [catch { set lGraphData $glGraphData($nCondition) } sResult]} {
	    # no data
	    continue;
	}
	set nLength [llength $lGraphData]
	if { $nLength <= 0 } {
	    continue;
	}
	
	# try to size the spacing of the ticks on the x axis appropriatly.
	# get the x range of this data and find out how many points we have.
	# then get the width of the graph and divide it by the number
	# of points. if < 40 pixels, set the step size to the width divided
	# by 40. otherwise set it to half time res (half because the minor
	# tick goes in between each major tick).
	set nNumPoints [expr $nLength / 2]
	set nWidth [$gwGraph cget -width]
	set nWidthPerTick [expr $nWidth / $nNumPoints]
	if { $nWidthPerTick < $knMinWidthPerTick } {
	    set nNumMarks [expr $nWidth / $knMinWidthPerTick]
	    set nWidthPerTick [expr $nNumPoints / $nNumMarks]
	    $gwGraph axis configure x -stepsize $nWidthPerTick
	} else {
	    set nWidthPerTick [expr $gnTimeResolution / 2]
	    if { $nWidthPerTick < 1 } {
		set nWidthPerTick 1
	    }
	    $gwGraph axis configure x -stepsize $nWidthPerTick
	}
	
	
	# if we're subtracting the prestim avg..
	if { $gbPreStimOffset && $gnPreStimPoints > 0 } {
	    
	    # get the sum of all the points before the stim.
	    set fPreStimSum 0
	    for { set nTP 0 } { $nTP < $gnPreStimPoints } { incr nTP } {
		set nIndex [expr [expr $nTP * 2] + 1];
		set fPreStimSum [expr double($fPreStimSum) + double([lindex $lGraphData $nIndex])];
	    }
	    
	    # find the avg.
	    set fPreStimAvg [expr double($fPreStimSum) / double($gnPreStimPoints)]
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

	    # get the errors for this condition
	    if { [catch { set lErrors $glErrorData($nCondition) } sResult] } {
		# no error data
		continue;
	    }
	    
	    
	    # for each error...
	    set nLength [llength $lErrors]
	    if { $nLength > $gnMaxNumErrorBars } { 
		set gnMaxNumErrorBars $nLength
	    }
	    for {set nErrorIndex 0} {$nErrorIndex < $nLength} {incr nErrorIndex} {
		
		# get error amt
		set nError [lindex $lErrors $nErrorIndex]
		
		# if 0, continue
		if { $nError == 0 } {
		    continue
		}
		
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
    
    
    TimeCourse_DrawCurrentTimePoint
    
    if { $gbAutoRangeGraph == 0 } {
	$gwGraph axis configure y -min $gnFixedGraphCoords(y1) \
	    -max $gnFixedGraphCoords(y2)
    }
}

proc TimeCourse_DrawCurrentTimePoint {} {
    
    global gwGraph gnTimeSecond
    
    # delete the old one
    catch {$gwGraph marker delete currentTimePoint}

    # draw a dashed line at current time point
    $gwGraph marker create line \
      -coords [list $gnTimeSecond -Inf $gnTimeSecond Inf] \
      -name currentTimePoint \
      -dashes {5 5}
}

proc TimeCourse_SaveGraphToPS { isFileName } {
    global gwGraph
    catch {$gwGraph postscript output $isFileName}
}

proc Graph_UpdateSize { } {
    global gbAutoRangeGraph gwGraph gnFixedGraphCoords
    if { $gbAutoRangeGraph == 0 } {
  set gnFixedGraphCoords(y1) [lindex [$gwGraph axis limits y] 0]
  set gnFixedGraphCoords(y2) [lindex [$gwGraph axis limits y] 1]
    } else {
  $gwGraph axis configure x y -min {} -max {}
    }
}

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


proc Graph_HandleClick { iwGraph inX inY } {

    global gnTimeResolution gnPreStimPoints

    # get the x coord of the graph and send it to the c code.
    set nSecond [$iwGraph axis invtransform x $inX]
    set nTimePoint [expr [expr $nSecond / $gnTimeResolution] + $gnPreStimPoints]

    puts "sec = $nSecond timeres = $gnTimeResolution prestim $gnPreStimPoints tp = $nTimePoint"

    Overlay_SetTimePoint $nTimePoint

}

# ============================================================== UPDATING VARS

proc ShowFunctionalWindow {} {

    global gwwTop gbFunctionalWindowOpen
    wm deiconify $gwwTop
    set gbFunctionalWindowOpen 1
}

proc HideFunctionalWindow {} {

    global gwwTop gbFunctionalWindowOpen
    wm withdraw $gwwTop
    set gbFunctionalWindowOpen 0
}

proc Overlay_DoConfigDlog {} { 

    global gDialog
    global FunV_tDisplayFlag_Ol_TruncateNegative 
    global FunV_tDisplayFlag_Ol_TruncatePositive 
    global FunV_tDisplayFlag_Ol_ReversePhase
    global FunV_tDisplayFlag_Ol_Grayscale
    global gnOverlayNumTimePoints gnOverlayNumConditions
    global gnTimePoint gnCondition gbOverlayOffset gbShowOverlayOffsetOptions
    global gbTruncateNegative gbReverse gbIgnoreThreshold 
    global gbGrayscale gbOpaque gbTruncatePositive
    global gfThreshold gfOverlayRange
    global gfOverlayAlpha
    
    set wwDialog .wwOverlayConfigDlog
    
    if { [Dialog_Create $wwDialog "Configure Functional Overlay" \
	      {-borderwidth 10}] } {
	
	set lfwLocation       $wwDialog.lfwLocation
	set lfwDisplay        $wwDialog.lfwDisplay
	set lfwThreshold      $wwDialog.lfwThreshold
	set lfwAlpha          $wwDialog.lfwAlpha
	set fwButtons         $wwDialog.fwButtons
	
	Overlay_SaveConfiguration;
	
	tixLabelFrame $lfwLocation \
	    -label "Location" \
	    -labelside acrosstop \
	    -options { label.padX 5 }
	
	set fwLocationSub    [$lfwLocation subwidget frame]
	set fwTimePoint      $fwLocationSub.fwTimePoint
	set fwCondition      $fwLocationSub.fwCondition
	
	set nMaxCondition [expr $gnOverlayNumConditions - 1]
	set nMaxTimePoint [expr $gnOverlayNumTimePoints - 1]
	
	tkm_MakeEntryWithIncDecButtons \
	    $fwTimePoint "Time Point (0-$nMaxTimePoint)" \
	    gnTimePoint \
	    {} 1
	
	tkm_MakeEntryWithIncDecButtons \
	    $fwCondition "Condition (0-$nMaxCondition)" \
	    gnCondition \
	    {} 1
	
	
	tixLabelFrame $lfwDisplay \
	    -label "Display" \
	    -labelside acrosstop \
	    -options { label.padX 5 }
	
	set fwDisplaySub    [$lfwDisplay subwidget frame]
	set fwOptions       $fwDisplaySub.fwOptions
	set fwSampleType    $fwDisplaySub.fwSampleType
	
	set lOffsetOptions {}
	if { $gbShowOverlayOffsetOptions == 1 } {
	    set lOffsetOptions \
		[list text "Show percent change" \
		     gbOverlayOffset "set gbOverlayOffset \$gbOverlayOffset" ]
	}
	
	tkm_MakeCheckboxes $fwOptions v \
	    [list \
		 { text "Truncate negative values" gbTruncateNegative
		     "set gbTruncateNegative \$gbTruncateNegative" } \
		 { text "Truncate positive values"
		     gbTruncatePositive
		     "set gbTruncatePositive \$gbTruncatePositive" } \
		 { text "Reverse values" gbReverse
		     "set gbReverse \$gbReverse" } \
		 { text "Grayscale" gbGrayscale
		     "set gbGrayscale \$gbGrayscale" } \
		 { text "Opaque" gbOpaque
		     "set gbOpaque \$gbOpaque" } \
		 $lOffsetOptions ]
	tkm_MakeRadioButtons $fwSampleType x "Sample Type" gOverlaySampleType \
	    [list \
		 { text "Nearest Neighbor" 0 "" } \
		 { text "Trilinear" 1 "" } \
		]

	tixLabelFrame $lfwThreshold \
	    -label "Threshold" \
	    -labelside acrosstop \
	    -options { label.padX 5 }
	
	set fwThresholdSub     [$lfwThreshold subwidget frame]
	set fwIgnoreThresh     $fwThresholdSub.fwIgnoreThresh
	set fwThresholdSliders $fwThresholdSub.fwThresholdSliders
	set fwThresholdSlope   $fwThresholdSub.fwThresholdSlope
	
	set fwFDR      $fwThresholdSub.fwFDR
	set bwFDR      $fwFDR.bwFDR
	set ewFDRRate  $fwFDR.ewFDRRate
	set cbFDRMask  $fwFDR.cbFDRMask

	tkm_MakeCheckboxes $fwIgnoreThresh y {
	    { text "Ignore Threshold" gbIgnoreThreshold
		"set gbIgnoreThreshold \$gbIgnoreThreshold" } }
		
	tkm_MakeSliders $fwThresholdSliders \
	    [list \
		 [list {"Threshold minimum"} gfThreshold(min) \
		      -10000 10000 100 {} 1 0.25] \
		 [list {"Threshold midpoint"} gfThreshold(mid) \
		      -10000 10000 100 {} 1 0.25]]
	
	tkm_MakeEntry $fwThresholdSlope "Threshold slope" gfThreshold(slope) 6

	frame $fwFDR
	tkm_MakeButtons $bwFDR \
	    [list \
		 [list text "Set Threshold Using FDR" \
		      {Overlay_SetThresholdUsingFDR $gFDRRate $gbFDRMask}]]

	tkm_MakeEntry $ewFDRRate "Rate" gFDRRate 4 {}

	tkm_MakeCheckboxes $cbFDRMask y {
	    { text "Mask to brain" gbFDRMask
		"set gbFDRMask \$gbFDRMask" } }
		
	pack $bwFDR $ewFDRRate $cbFDRMask \
	    -side left \
	    -expand yes \
	    -fill x

	tixLabelFrame $lfwAlpha \
	    -label "Overlay Alpha" \
	    -labelside acrosstop \
	    -options { label.padX 5 }
	
	set fwAlphaSub     [$lfwAlpha subwidget frame]
	set fwAlpha        $fwAlphaSub.fwAlpha
	
	tkm_MakeSliders $fwAlpha \
	    [list \
		 [list {"Alpha"} gfOverlayAlpha \
		      0 1 100 {} 0 0.1]]

	tkm_MakeCancelApplyOKButtons $fwButtons $wwDialog \
	    { Overlay_SetConfiguration; } \
	    { Overlay_RestoreConfiguration; }
	
	
	pack $lfwLocation $fwTimePoint $fwCondition \
	    $lfwDisplay $fwOptions $fwSampleType \
	    $lfwThreshold $lfwAlpha $fwIgnoreThresh \
	    $fwThresholdSliders $fwThresholdSlope $fwFDR $fwAlpha \
	    $fwButtons \
	    -side top \
	    -anchor w \
	    -expand yes \
	    -fill x
    }
}

proc TimeCourse_DoConfigDlog {} {
    
    global gDialog gbShowTimeCourseOffsetOptions
    global gbErrorBars gnPreStimPoints gnTimeResolution gbTimeCourseOffset
    global gGraphSetting gnNumTimeCourseConditions glGraphColors 
    global gbPreStimOffset
    
    set wwDialog .wwTimeCourseConfigDlog
    
    if { [Dialog_Create $wwDialog "Configure Time Course" {-borderwidth 10}] } {
  
  set nMaxCondition [expr $gnNumTimeCourseConditions - 1]
  
  set fwConditions          $wwDialog.fwConditions
  set lfwDisplay            $wwDialog.lfwDisplay
  set fwButtons             $wwDialog.fwButtons
  
  TimeCourse_SaveConfiguration;
  
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
        [list [list text "" gGraphSetting($dataSet,visible) \
        "TimeCourse_SetGraphSetting $dataSet visible \
        \$gGraphSetting($dataSet,visible)"]]
      
      # this goes on the right, an entry for the condition this
      # color is displaying.
      tkm_MakeEntryWithIncDecButtons \
        $fw.fwControl$dataSet \
        "Condition (0-$nMaxCondition)" \
        gGraphSetting($dataSet,condition) \
        "TimeCourse_SetGraphSetting $dataSet condition" \
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
    {set gbAutoRangeGraph $gbAutoRangeGraph; Graph_UpdateSize} } \
    $lOffsetOptions \
    { text "Subtract pre-stim average" gbPreStimOffset \
    {set gbPreStimOffset $gbPreStimOffset} } ]

  tkm_MakeEntryWithIncDecButtons \
    $fwPreStimPoints "Number of pre-stim points" gnPreStimPoints \
    {} 1

  tkm_MakeActiveLabel \
    $fwTimeRes "Time resolution" gnTimeResolution


  tkm_MakeCancelApplyOKButtons $fwButtons $wwDialog \
    { TimeCourse_SetConfiguration; TimeCourse_DrawGraph } \
    { TimeCourse_RestoreConfiguration; TimeCourse_DrawGraph }


  pack $fwConditions \
    $lfwDisplay $fwOptions \
    $fwPreStimPoints $fwTimeRes $fwButtons \
    -side top \
    -anchor w \
    -expand yes \
    -fill x
    }
}

proc TimeCourse_UpdateGraphLabel { isDataSet isLabel } {

    global glAllColors gGraphSetting

    set nDataSet [lsearch -exact $glAllColors $isDataSet]
    if { $nDataSet == -1 } {
	dputs "TimeCourse_UpdateGraphLabel: Couldn't find $isDataSet\n"
	return;
    }

    set gGraphSetting($isDataSet,label) $isLabel
}

# ================================================== MANAGING INTERNAL STUFF

proc TimeCourse_SetGraphSetting { iColor iType iValue } {
    global gGraphSetting gnNumTimeCourseConditions
    
    if { $iType == "condition" } {
  if { $iValue >= $gnNumTimeCourseConditions } {
      ErrorDlog "Invalid condition for $iColor. Must be between 0 and $gnNumTimeCourseConditions."
      return;
  }
  if { $iValue < 0 } {
      ErrorDlog "Invalid condition for $iColor. Must be between 0 and $gnNumTimeCourseConditions."
      return;
  }
    }

    set gGraphSetting($iColor,$iType) $iValue
}

proc Overlay_SaveConfiguration {} {

    global gnTimePoint gnCondition gbTruncateNegative gbReverse
    global gbIgnoreThreshold gfThreshold gbGrayscale gbOpaque
    global gbOverlayOffset gbTruncatePositive gfOverlayAlpha
    global gOverlaySampleType
    global gnSavedTimePoint gnSavedCondition gbSavedTruncateNegative 
    global gbSavedGrayscale gbSavedIgnoreThreshold gbSavedTruncatePositive
    global gbSavedReverse gfSavedThreshold gbSavedOverlayOffset gbSavedOpaque
    global gfSavedOverlayAlpha gSavedOverlaySampleType

    set gnSavedTimePoint $gnTimePoint
    set gnSavedCondition $gnCondition
    set gbSavedTruncateNegative $gbTruncateNegative
    set gbSavedTruncatePositive $gbTruncatePositive
    set gbSavedReverse $gbReverse
    set gbSavedIgnoreThreshold $gbIgnoreThreshold
    set gbSavedGrayscale $gbGrayscale
    set gbSavedOpaque $gbOpaque
    set gbSavedOverlayOffset $gbOverlayOffset
    foreach entry {min mid slope} {
	set gfSavedThreshold($entry) $gfThreshold($entry)
    }
    set gfSavedOverlayAlpha $gfOverlayAlpha
    set gSavedOverlaySampleType $gOverlaySampleType

}

proc Overlay_RestoreConfiguration {} {

    global gnTimePoint gnCondition gbTruncateNegative gbReverse
    global gbIgnoreThreshold gfThreshold gbGrayscale gbOpaque
    global gbOverlayOffset gbTruncatePositive gfOverlayAlpha
    global gOverlaySampleType
    global gnSavedTimePoint gnSavedCondition gbSavedTruncateNegative 
    global gbSavedGrayscale gbSavedIgnoreThreshold gbSavedTruncatePositive
    global gbSavedReverse gfSavedThreshold gbSavedOverlayOffset gbSavedOpaque
    global gfSavedOverlayAlpha gSavedOverlaySampleType
    
    set gnTimePoint $gnSavedTimePoint
    set gnCondition $gnSavedCondition
    set gbTruncateNegative $gbSavedTruncateNegative
    set gbTruncatePositive $gbSavedTruncatePositive
    set gbReverse $gbSavedReverse
    set gbIgnoreThreshold $gbSavedIgnoreThreshold
    set gbGrayscale $gbSavedGrayscale
    set gbOpaque $gbSavedOpaque
    set gbOverlayOffset $gbSavedOverlayOffset
    foreach entry {min mid slope} {
	set gfThreshold($entry) $gfSavedThreshold($entry)
    }
    set gfOverlayAlpha $gfSavedOverlayAlpha
    set gOverlaySampleType $gSavedOverlaySampleType

    Overlay_SetConfiguration
}

proc Overlay_SetConfiguration {} {

    global gnTimePoint gnCondition gbTruncateNegative gbReverse gfThreshold
    global gbOverlayOffset gbTruncatePositive gbIgnoreThreshold gbGrayscale
    global gbOpaque gfOverlayAlpha gOverlaySampleType
    global FunV_tDisplayFlag_Ol_TruncateNegative
    global FunV_tDisplayFlag_Ol_TruncatePositive
    global FunV_tDisplayFlag_Ol_ReversePhase
    global FunV_tDisplayFlag_Ol_OffsetValues
    global FunV_tDisplayFlag_Ol_IgnoreThreshold
    global FunV_tDisplayFlag_Ol_Grayscale
    global FunV_tDisplayFlag_Ol_Opaque

    Overlay_SetTimePoint $gnTimePoint
    Overlay_SetCondition $gnCondition
    Overlay_SetDisplayFlag $FunV_tDisplayFlag_Ol_TruncateNegative $gbTruncateNegative
    Overlay_SetDisplayFlag $FunV_tDisplayFlag_Ol_ReversePhase $gbReverse
    Overlay_SetDisplayFlag $FunV_tDisplayFlag_Ol_IgnoreThreshold $gbIgnoreThreshold
    Overlay_SetDisplayFlag $FunV_tDisplayFlag_Ol_Grayscale $gbGrayscale
    Overlay_SetDisplayFlag $FunV_tDisplayFlag_Ol_Opaque $gbOpaque
    Overlay_SetDisplayFlag $FunV_tDisplayFlag_Ol_TruncatePositive $gbTruncatePositive
    Overlay_SetDisplayFlag $FunV_tDisplayFlag_Ol_OffsetValues $gbOverlayOffset
    Overlay_SetThreshold $gfThreshold(min) $gfThreshold(mid) $gfThreshold(slope)
    SetFuncOverlayAlpha $gfOverlayAlpha
    Overlay_SetVolumeSampleType $gOverlaySampleType

}

proc TimeCourse_SaveConfiguration {} {

    global gbErrorBars gnPreStimPoints gnTimeResolution gGraphSetting
    global gbTimeCourseOffset gbPreStimOffset
    global gbSavedErrorBars gnSavedPreStimPoints gnSavedTimeResolution
    global gSavedGraphSetting glGraphColors gbSavedTimeCourseOffset
    global gbSavedPreStimOffset 

    set gbSavedErrorBars $gbErrorBars
    set gnSavedPreStimPoints $gnPreStimPoints
    set gnSavedTimeResolution $gnTimeResolution
    set gbSavedTimeCourseOffset $gbTimeCourseOffset
#    set gbSavedPreStimOffset $gbPreStimOffset
    foreach color $glGraphColors {
  foreach entry {visible condition} {
      set gSavedGraphSetting($color,$entry) $gGraphSetting($color,$entry)
  }
    }
}

proc TimeCourse_RestoreConfiguration {} {

    global gbErrorBars gnPreStimPoints gnTimeResolution gGraphSetting
    global gbTimeCourseOffset gbPreStimOffset
    global gbSavedErrorBars gnSavedPreStimPoints gnSavedTimeResolution
    global gSavedGraphSetting glGraphColors gbSavedTimeCourseOffset
    global gbSavedPreStimOffset

    set gbErrorBars $gbSavedErrorBars
    set gnPreStimPoints $gnSavedPreStimPoints
    set gnTimeResolution $gnSavedTimeResolution
    set gbTimeCourseOffset $gbSavedTimeCourseOffset
#    set gbPreStimOffset $gbSavedPreStimOffset
    foreach color $glGraphColors {
  foreach entry {visible condition} {
      set gGraphSetting($color,$entry) $gSavedGraphSetting($color,$entry)
  }
    }

    TimeCourse_SetConfiguration;
}

proc TimeCourse_SetConfiguration {} {

    global gbErrorBars gnPreStimPoints gnTimeResolution gbTimeCourseOffset
    global gbPreStimOffset
    global FunV_tDisplayFlag_TC_OffsetValues FunV_tDisplayFlag_TC_PreStimOffset

    TimeCourse_SetErrorBarsFlag $gbErrorBars
    TimeCourse_SetNumPreStimPoints $gnPreStimPoints
    TimeCourse_SetTimeResolution $gnTimeResolution
    TimeCourse_SetDisplayFlag $FunV_tDisplayFlag_TC_OffsetValues $gbTimeCourseOffset
#    TimeCourse_SetDisplayFlag $FunV_tDisplayFlag_TC_PreStimOffset $gbPreStimOffset
}
# =======================================================================


# ============================================================= C CALLBACKS

proc Overlay_UpdateNumTimePoints { inNumPts } { 
    global gnOverlayNumTimePoints
    set gnOverlayNumTimePoints $inNumPts
}

proc Overlay_UpdateNumConditions { inNumConditions } { 
    global gnOverlayNumConditions
    set gnOverlayNumConditions $inNumConditions
}

proc Overlay_UpdateDisplayFlag { iFlag ibNewValue } { 
    
    global FunV_tDisplayFlag_Ol_ReversePhase 
    global FunV_tDisplayFlag_Ol_TruncateNegative
    global FunV_tDisplayFlag_Ol_TruncatePositive
    global FunV_tDisplayFlag_Ol_IgnoreThreshold
    global FunV_tDisplayFlag_Ol_Grayscale
    global FunV_tDisplayFlag_Ol_Opaque
    global gbTruncateNegative gbReverse gbTruncatePositive gbIgnoreThreshold
    global gbGrayscale gbOpaque

    if { $FunV_tDisplayFlag_Ol_ReversePhase == $iFlag } {
  set gbReverse $ibNewValue
    }
    if { $FunV_tDisplayFlag_Ol_IgnoreThreshold == $iFlag } {
  set gbIgnoreThreshold $ibNewValue
    }
    if { $FunV_tDisplayFlag_Ol_Grayscale == $iFlag } {
  set gbGrayscale $ibNewValue
    }
    if { $FunV_tDisplayFlag_Ol_Opaque == $iFlag } {
  set gbOpaque $ibNewValue
    }
    if { $FunV_tDisplayFlag_Ol_TruncateNegative == $iFlag } {
  set gbTruncateNegative $ibNewValue
    }
    if { $FunV_tDisplayFlag_Ol_TruncatePositive == $iFlag } {
  set gbTruncatePositive $ibNewValue
    }
}

proc Overlay_UpdateDataName { isName } { 
    global gsOverlayDataName
    set gsOverlayDataName $isName
} 

proc Overlay_UpdateTimePoint { inTimePoint inTimeSecond } {

    global gnTimePoint gnTimeSecond gbFunctionalWindowOpen
    set gnTimePoint  $inTimePoint
    set gnTimeSecond $inTimeSecond

    if { $gbFunctionalWindowOpen == 1 } {
  TimeCourse_DrawCurrentTimePoint
    }
}

proc Overlay_UpdateCondition { inCondition } {
    global gnCondition 
    set gnCondition $inCondition
}

proc Overlay_UpdateThreshold { ifMin ifMid ifSlope } {
    global gfThreshold
    set gfThreshold(min) $ifMin
    set gfThreshold(mid) $ifMid
    set gfThreshold(slope) $ifSlope
}

proc Overlay_UpdateRange { ifMin ifMax } {
    global gfOverlayRange
    set gfOverlayRange(min) $ifMin
    set gfOverlayRange(max) $ifMax
}

proc Overlay_ShowOffsetOptions { ibShow } {

    global gbShowOverlayOffsetOptions
    set gbShowOverlayOffsetOptions $ibShow
}

proc TimeCourse_UpdateNumConditions { inNumConditions } { 
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

    # If there is only 1 condition, make it condition 0
    # visible. Otherwise make condition 0 not visible.
    set dataSet [lindex $glGraphColors 0]
    if { $inNumConditions == 1 } {
	set gGraphSetting($dataSet,visible) 1
    } else {
	set gGraphSetting($dataSet,visible) 0
    }
}

proc TimeCourse_UpdateNumPreStimPoints { inNumPts } { 
    global gnPreStimPoints
    set gnPreStimPoints $inNumPts
 }

proc TimeCourse_UpdateTimeResolution { inTimeRes } { 
    global gnTimeResolution
    set gnTimeResolution $inTimeRes
 }

proc TimeCourse_UpdateDisplayFlag { iFlag ibNewValue } { 

    global FunV_tDisplayFlag_TC_GraphWindowOpen
    
    if { $FunV_tDisplayFlag_TC_GraphWindowOpen == $iFlag } {
  if { $ibNewValue == 0 } {
      HideFunctionalWindow
  } else {
      ShowFunctionalWindow
  }
    }
}

proc TimeCourse_UpdateDataName { isName } { 
    global gwwTop gsTimeCourseDataName 
    set gsTimeCourseDataName $isName
    TimeCourse_SetGraphTitle
}

proc TimeCourse_UpdateLocationName { isName } { 
    global gsTimeCourseLocation
    set gsTimeCourseLocation $isName
    TimeCourse_SetGraphTitle
}

proc TimeCourse_SetGraphTitle {} {
    global gwGraph gsTimeCourseDataName gsTimeCourseLocation
    $gwGraph configure -title "$gsTimeCourseDataName ($gsTimeCourseLocation)"
}

proc TimeCourse_SetErrorBarsFlag { ibNewValue } {
    global gbErrorBars
    set gbErrorBars $ibNewValue
}

proc TimeCourse_ShowOffsetOptions { ibShow } {

    global gbShowTimeCourseOffsetOptions
    set gbShowTimeCourseOffsetOptions $ibShow
}

# ========================================================= BUILDING INTERFACE

proc CreateWindow { iwwTop } {
    global ksWindowName
    toplevel $iwwTop
    wm title $iwwTop $ksWindowName
    wm geometry $iwwTop 600x400
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

    bind $gwGraph <ButtonPress-1> { Graph_HandleClick %W %x %y }
    bind $gwGraph <ButtonPress-2> { Graph_RegionStart %W %x %y }
    bind $gwGraph <B2-Motion> { Graph_RegionMotion %W %x %y }
    bind $gwGraph <ButtonRelease-2> { Graph_RegionEnd %W %x %y }
    bind $gwGraph <ButtonRelease-3> { Graph_Unzoom %W }
}

# = ====================================================================== MAIN

# build the window
set gwwTop       .wwFunctional
set fwGraph      $gwwTop.fwGraph

CreateWindow          $gwwTop
CreateGraphFrame      $fwGraph

set gfwGraph $fwGraph

HideFunctionalWindow

proc TestData {} {

    set kNumConditions 2
    set kNumTimePoints 10

    ShowFunctionalWindow

    TimeCourse_BeginDrawingGraph

    for { set cn 0 } { $cn < $kNumConditions } { incr cn } {
  set lData {}
  for { set tp 0 } { $tp < $kNumTimePoints } { incr tp } {
      lappend lData $tp [expr $tp / [expr $cn + 1]]
  }
  TimeCourse_UpdateGraphData $cn $lData
    }
    TimeCourse_UpdateNumConditions $kNumConditions

    TimeCourse_EndDrawingGraph
}

# enable these to test the script from the command line
#TimeCourse_DoConfigDlog
#Overlay_DoConfigDlog
#set gbErrorBars 1
#TestData

Graph_UpdateSize

dputs "Successfully parsed tkm_functional.tcl"

