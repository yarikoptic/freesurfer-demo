/**
 * @file  CostFunctions.h
 * @brief A class that makes available many different cost functions for images
 *   and to combine multiple volumes by mean or median
 *   MRIiterator iterates through MRI (readonly)
 */

/*
 * Original Author: Martin Reuter
 * CVS Revision Info:
 *    $Author: mreuter $
 *    $Date: 2010/07/22 18:00:14 $
 *    $Revision: 1.8.2.1 $
 *
 * Copyright (C) 2008-2009
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

// written by Martin Reuter
// March 20th ,2009
//

#ifndef CostFunctions_H
#define CostFunctions_H

#ifdef __cplusplus
extern "C"
{
#endif
#include "mri.h"
#include "matrix.h"
#ifdef __cplusplus
}
#endif

//#include <utility>
//#include <string>
#include <vector>
#include <iostream>
#include <vnl/vnl_matrix_fixed.h>

class CostFunctions
{
public:
  static float mean(MRI *i);
  static float var(MRI *i);
  static float sdev(MRI *i)
  {
    return sqrt(var(i));
  };
  static float median(MRI *i);
  static float mad(MRI *i, float d = 1.4826);
  static double moment(MRI * i, int x, int y, int z);
  static std::vector < double > centroid (MRI * i);
  static vnl_matrix_fixed < double,3,3 > orientation (MRI * i);

  static float leastSquares(MRI * i1, MRI * i2 = NULL);
  static double tukeyBiweight(MRI *i1, MRI * i2 = NULL, double sat = 4.685);
  static float normalizedCorrelation(MRI * i1, MRI * i2);
  static float mutualInformation(MRI * i1, MRI * i2 = NULL);
  static float normalizedMutualInformation(MRI * i1, MRI * i2 = NULL);
  static float woods(MRI * i1, MRI * i2 = NULL);
  static float correlationRatio(MRI * i1, MRI * i2 = NULL);


protected:
  inline static double rhoTukeyBiweight (double d, double sat = 4.685);
  inline static double rhoSquare (double d)
  {
    return d*d;
  };
};

inline double CostFunctions::rhoTukeyBiweight(double d, double sat)
{
  if (d > sat || d < -sat) return sat*sat/2.0;
  else
  {
    double a = d/sat;
    double b = 1.0 - a * a;
    return (sat*sat/2.0)*(1.0-b*b*b);
  }
}

class MRIiterator
{
public:
  MRIiterator(MRI * i);

  void begin();
  bool isEnd();
  MRIiterator& operator++(int);
  float operator*();


protected:

  float fromUCHAR(void);
  float fromSHORT(void);
  float fromINT(void);
  float fromLONG(void);
  float fromFLOAT(void);

  MRIiterator& opincchunk(int);
  MRIiterator& opincnochunk(int);

  MRI * img;
  unsigned char * pos;
  unsigned char * end;
  float (MRIiterator::*getVal)(void);
  MRIiterator& (MRIiterator::*opinc)(int);
  int x,y,z;
  int bytes_per_voxel;
};

inline MRIiterator::MRIiterator(MRI * i):img(i)
{
  // set current pos to begin
  // and initialize end pointer
  begin();

  switch (img->type)
  {
  case MRI_UCHAR:
    getVal = &MRIiterator::fromUCHAR;
    bytes_per_voxel = sizeof(unsigned char);
    break;
  case MRI_SHORT:
    getVal = &MRIiterator::fromSHORT;
    bytes_per_voxel = sizeof(short);
    break;
  case MRI_INT:
    getVal = &MRIiterator::fromINT;
    bytes_per_voxel = sizeof(int);
    break;
  case MRI_LONG:
    getVal = &MRIiterator::fromLONG;
    bytes_per_voxel = sizeof(long);
    break;
  case MRI_FLOAT:
    getVal = &MRIiterator::fromFLOAT;
    bytes_per_voxel = sizeof(float);
    break;
  default:
    std::cerr << "MRIiterator: Type not supported: " << img->type << std::endl;
    exit(1);
  }


}

inline void MRIiterator::begin()
// set pos to first element
{
  if (img->ischunked)
  {
    pos = (unsigned char*) img->chunk;
    end = (unsigned char*) img->chunk + img->bytes_total;
    opinc = &MRIiterator::opincchunk;
  }
  else
  {
    x = 0;
    y=0;
    z=0;
    pos = (unsigned char*) img->slices[0][0];
    end = NULL;
    opinc = &MRIiterator::opincnochunk;
  }
}

inline bool MRIiterator::isEnd()
{
  //    if(pos > end && end != 0)
// {
//    std::cerr << "MRIiterator::isEnd outside data???" << std::endl;
//    exit(1);
// }
  return (pos == end);
}

inline MRIiterator& MRIiterator::operator++(int i)
{
  return (this->*opinc)(i);
}

inline MRIiterator& MRIiterator::opincchunk(int)
{
//   if (pos < end)
  pos += img->bytes_per_vox;
  return *this;
}

inline MRIiterator& MRIiterator::opincnochunk(int)
{
  x++;
  if (x == img->width)
  {
    x = 0;
    y++;
    if (y == img->height)
    {
      y=0;
      z++;
      if (z== img->depth)
      {
        z=0;
        pos = NULL;
        return *this;
      }
    }
    pos = (unsigned char*) img->slices[z][y];
  }
  else pos += bytes_per_voxel;
  return *this;
}

inline float MRIiterator::fromUCHAR()
{
  return ((float) *(unsigned char *)pos);
}

inline float MRIiterator::fromSHORT()
{
  return ((float) *(short *)pos);
}

inline float MRIiterator::fromINT()
{
  return ((float) *(int *)pos);
}

inline float MRIiterator::fromLONG()
{
  return ((float) *(long *)pos);
}

inline float MRIiterator::fromFLOAT()
{
  return ((float) *(float *)pos);
}

inline float MRIiterator::operator*()
{
//   if (pos < end && pos >= img->chunk)
  return (this->*getVal)();
}

// example:
// MRIiterator it(mri);
// for (it.begin(); !it.isEnd(); it++)
// {
//    std::cout << *it << std::endl;
////    *it = 0;
// }


#endif
