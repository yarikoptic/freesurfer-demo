/**
 * @file  vtkSimpleLabelEdgeFilter.h
 * @brief A simple label edge filter ONLY for 2D label image (2D vtkImageData).
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
// .NAME vtkSimpleLabelEdgeFilter - Simple example of an image-image filter.
// .SECTION Description
// This is an example of a simple image-image filter. It copies it's input
// to it's output (point by point). It shows how templates can be used
// to support various data types.
// .SECTION See also
// vtkSimpleImageToImageFilter

#include "vtkSimpleLabelEdgeFilter.h"

#include "vtkImageData.h"
#include "vtkObjectFactory.h"

vtkCxxRevisionMacro(vtkSimpleLabelEdgeFilter, "$Revision: 1.1.4.1 $");
vtkStandardNewMacro(vtkSimpleLabelEdgeFilter);

// The switch statement in Execute will call this method with
// the appropriate input type (IT). Note that this example assumes
// that the output data type is the same as the input data type.
// This is not always the case.
template <class IT>
    void vtkSimpleLabelEdgeFilterExecute(vtkImageData* input,
                                            vtkImageData* output,
                                            IT* inPtr, IT* outPtr)
{
  int dim[3];
  input->GetDimensions(dim);
  if (input->GetScalarType() != output->GetScalarType())
  {
    vtkGenericWarningMacro(<< "Execute: input ScalarType, " << input->GetScalarType()
        << ", must match out ScalarType " << output->GetScalarType());
    return;
  }
  if ( dim[2] > 1 )
  {
    vtkGenericWarningMacro(<< "Execute: input must be 2D image");
    return;
  }

  memcpy( outPtr, inPtr, sizeof( IT )*dim[0]*dim[1] );
  for ( int i = 1; i < dim[0]-1; i++ )
  {
    for ( int j = 1; j < dim[1]-1; j++ )
    {
      IT pixelvalue = inPtr[j*dim[0]+i];
      if ( pixelvalue > 0 &&
           inPtr[(j+1)*dim[0]+i] == pixelvalue &&
           inPtr[(j-1)*dim[0]+i] == pixelvalue &&
           inPtr[j*dim[0]+i+1] == pixelvalue &&
           inPtr[j*dim[0]+i-1] == pixelvalue )
      {
        outPtr[j*dim[0]+i] = 0;
      }
    }
  }
}

void vtkSimpleLabelEdgeFilter::SimpleExecute(vtkImageData* input,
    vtkImageData* output)
{

  void* inPtr = input->GetScalarPointer();
  void* outPtr = output->GetScalarPointer();

  switch(output->GetScalarType())
  {
    // This is simply a #define for a big case list. It handles all
    // data types VTK supports.
    vtkTemplateMacro(
        vtkSimpleLabelEdgeFilterExecute(input, output,
                                           static_cast<VTK_TT *>(inPtr), 
                                           static_cast<VTK_TT *>(outPtr)));
    default:
      vtkGenericWarningMacro("Execute: Unknown input ScalarType");
      return;
  }
}
