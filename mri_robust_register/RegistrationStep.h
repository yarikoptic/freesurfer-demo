/**
 * @file RegistrationStep.h
 * @brief A class to compute a robust symmetric registration (single step)
 *
 */

/*
 * Original Author: Martin Reuter
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2010/07/22 18:41:41 $
 *    $Revision: 1.2.2.2 $
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
// Jul. 16 ,2010
//

#ifndef RegistrationStep_H
#define RegistrationStep_H

#ifdef __cplusplus
extern "C"
{
#endif
#include "mri.h"
#ifdef __cplusplus
}
#endif

#include <utility>
#include <vector>
#include <cassert>
#include <iostream>
#include <string>
#include <vnl/vnl_vector.h>
#include <vnl/vnl_matrix.h>
#include <vnl/vnl_matrix_fixed.h>
#include "MyMRI.h"
#include "MyMatrix.h"
#include "Regression.h"
#include "Quaternion.h"

//class Registration;

template <class T>
class RegistrationStep
{
public:
 
					 
  RegistrationStep(const Registration & R):sat(R.sat),iscale(R.iscale),
									 transonly(R.transonly),rigid(R.rigid),robust(R.robust),rtype(R.rtype),subsamplesize(R.subsamplesize),
									 debug(R.debug),verbose(R.verbose),floatsvd(false),iscalefinal(R.iscalefinal),mri_weights(NULL),mri_indexing(NULL){};

  ~RegistrationStep()
	{ 
	  if (mri_indexing) MRIfree(&mri_indexing);
		if (mri_weights)  MRIfree(&mri_weights);
	};

  std::pair < vnl_matrix_fixed <double,4,4 >, double > computeRegistrationStep(MRI * mriS, MRI* mriT);

	double getwcheck()      {return wcheck;};
	double getwchecksqrt()  {return wchecksqrt;};
	double getzeroweights() {return zeroweights;};
	MRI * getWeights()  {return mri_weights;}; //??? who takes care of freeing?
	std::pair < vnl_matrix_fixed <double,4,4 >, double > getMd() {return Md;};

  void setFloatSVD(bool fsvd) { floatsvd = fsvd;}; // only makes sense for T=double;
	
// 
//   //   returns weights:
//   std::pair < vnl_vector < double >, MRI* > computeRegistrationStepW(MRI * mriS, MRI* mriT);
//   //   returns param vector:
//   vnl_vector <double > computeRegistrationStepP(MRI * mriS, MRI* mriT);
//   //   returns 4x4 matrix and iscale:
//   std::pair < vnl_matrix_fixed <double,4,4 >, double> computeRegistrationStep(MRI * mriS = NULL, MRI* mriT = NULL);

  // only public because of resampling testing in Registration.cpp
	// should be made protected at some point.
  void constructAb(MRI *mriS, MRI *mriT, vnl_matrix < T > &A, vnl_vector < T > &b);

  static std::pair < vnl_matrix_fixed <double,4,4 >, double > convertP2Md(const vnl_vector < T >& p,int rtype);


private:
// in:

  double sat;
  bool iscale;
  bool transonly;
  bool rigid;
  bool robust;
  int rtype;
  int subsamplesize;
  int debug;
	int verbose;
	bool floatsvd; // should be removed
	double iscalefinal; // from the last step, used in constructAB
	
// out:

  std::pair < vnl_matrix_fixed <double,4,4 >, double > Md;
	MRI* mri_weights;
	
	double wcheck; // set from computeRegistrationStepW
	double wchecksqrt; // set from computeRegistrationStepW
  double zeroweights;// set from computeRegistrationStepW

//internal
  MRI * mri_indexing;
	
};

//template <class T>
//std::pair < vnl_vector < double >, MRI* > RegistrationStep<T>::computeRegistrationStepW(MRI * mriS, MRI* mriT)
template <class T>
std::pair < vnl_matrix_fixed <double,4,4 >, double >  RegistrationStep<T>::computeRegistrationStep(MRI * mriS, MRI* mriT)
// computes Registration single step
// the mri's have to be in same space
// returns parameter vector and float MRI with the weights (if robust, else weights ==NULL)
//
// if rigid (only trans and rot) else use affine transform
// if robust  use M-estimator instead of ordinary least squares
// if iscale add parameter for intensity scaling
// rtype only for rigid (2: affine restriction to rigid, 1: use rigid from robust-paper)
{

	vnl_matrix < T > A;
	vnl_vector < T > b;

  if (rigid && rtype==2)
  {
    if (verbose > 1) std::cout << "rigid and rtype 2 !" << std::endl;
		assert(rtype !=2);
		
//     // compute non rigid A
//     rigid = false;
//     Ab = constructAb(mriS,mriT);
//     rigid = true;
//     // now restrict A  (= A R(lastp) )
//     MATRIX* R;
//     if (lastp) R = constructR(lastp);
//     else
//     {
//       int l = 6;
//       if (!rigid) l = 12;
//       if (iscale) l++;
//       MATRIX* tm = MatrixAlloc(l,1,MATRIX_REAL);
//       MatrixClear(tm);
//       R = constructR(tm);
//       MatrixFree(&tm);
//       //MatrixPrintFmt(stdout,"% 2.8f",R);exit(1);
// 
//     }
//     MATRIX* Rt = MatrixTranspose(R,NULL);
//     MATRIX* nA = MatrixMultiply(Ab.first,Rt,NULL);
//     //MatrixPrintFmt(stdout,"% 2.8f",nA);exit(1);
//     MatrixFree(&Ab.first);
//     Ab.first = nA;
//     MatrixFree(&R);
//     MatrixFree(&Rt);
  }
  else
  {
    //std::cout << "Rtype  " << rtype << std::endl;

    constructAb(mriS,mriT,A,b);
  }

  if (verbose > 1) std::cout << "   - checking A and b for nan ..." << std::flush;
	if (!A.is_finite() || !b.is_finite() )
	{
	   std::cerr << " A or b constain NAN or infinity values!!" << std::endl;
		 exit(1);
	}
		 
  if (verbose > 1) std::cout << "  DONE" << std::endl;
	
//  std::pair < vnl_vector <double>, MRI* > pw; pw.second = NULL;
		vnl_vector < T > p;
  Regression< T > R(A,b);
	R.setVerbose(verbose);
	R.setFloatSvd(floatsvd);
  if (robust)
  {
		vnl_vector < T > w;
    if (verbose > 1) std::cout << "   - compute robust estimate ( sat "<<sat<<" )..." << std::flush;
    if (sat < 0) p = R.getRobustEstW(w);
    else p = R.getRobustEstW(w,sat);

		A.clear();
		b.clear();
		
    if (verbose > 1) std::cout << "  DONE" << std::endl;
//    pw.first  = p;
//    pw.second = NULL;

//    std::cout << " pw-final  : "<< std::endl;
//    std::cout.precision(16);
//		for (unsigned int iii=0;iii<p.size(); iii++)
//		  std::cout << p[iii] << std::endl;;
//    std::cout.precision(8);

    // transform weights vector back to 3d (mri real)
	  if ( mri_weights && (mri_weights->width != mriS->width || mri_weights->height != mriS->height || mri_weights->depth != mriS->depth ))  
		  MRIfree(&mri_weights); 
	
		if ( ! mri_weights) 
		{
      mri_weights = MRIalloc(mriS->width, mriS->height, mriS->depth, MRI_FLOAT);
      MRIcopyHeader(mriS, mri_weights) ;
      mri_weights->type = MRI_FLOAT;
      MRIsetResolution(mri_weights, mriS->xsize, mriS->ysize, mriS->zsize);
		}
//    pw.second = MRIalloc(mriS->width, mriS->height, mriS->depth, MRI_FLOAT);
//    MRIcopyHeader(mriS, pw.second) ;
//    pw.second->type = MRI_FLOAT;
//    MRIsetResolution(pw.second, mriS->xsize, mriS->ysize, mriS->zsize);
    int x,y,z;
    unsigned int count = 0;
		long int val;
		wcheck = 0.0;
		wchecksqrt = 0.0;
		// sigma = max(widht,height,depth) / 6;
		double sigma   = mriS->width;
		if (mriS->height > sigma) sigma = mriS->height;
		if (mriS->depth  > sigma) sigma = mriS->depth;
		sigma = sigma / 6.0;
		double sigma22 = 2.0 * sigma * sigma;
		double factor = 1.0 / sqrt(M_PI * sigma22);
		double dsum = 0.0;
		//MRI * gmri = MRIalloc(mriS->width,mriS->height,mriS->depth, MRI_FLOAT);
    for (z = 0 ; z < mriS->depth  ; z++)
      for (x = 0 ; x < mriS->width  ; x++)
        for (y = 0 ; y < mriS->height ; y++)
        {
          val = MRILvox(mri_indexing,x,y,z);
// 						double xx = x-0.5*mriS->width;
// 						double yy = y-0.5*mriS->height;
// 						double zz = z-0.5*mriS->depth;
// 						double distance2 = xx*xx+yy*yy+zz*zz;
// 						double gauss    = factor * exp(- distance2 / sigma22 );
// 						MRIFvox(gmri,x,y,z) = gauss;
         // if (val == -10) MRIFvox(pw.second, x, y, z) = -0.5; // init value (border)
          //else if (val == -1) MRIFvox(pw.second, x, y, z) = -1; // zero element (skipped)
          if (val == -10) MRIFvox(mri_weights, x, y, z) = -0.5; // init value (border)
          else if (val == -1) MRIFvox(mri_weights, x, y, z) = -1; // zero element (skipped)
          else
          {
            //std::cout << val << "  xyz: " << x << " " << y << " " << z << " " << std::flush;
            assert(val < (int)w.size());
            //MRIFvox(pw.second, x, y, z) = w[val] * w[val];  // pwm.second is the square root of weights
            MRIFvox(mri_weights, x, y, z) = w[val] * w[val];  // pwm.second is the square root of weights
            //std::cout << "d"<<*MATRIX_RELT(pwm.second,val , 1)<< " " << MRIFvox(pw.second, x, y, z)<< std::endl;
						// compute distance to center:
						double xx = x-0.5*mriS->width;
						double yy = y-0.5*mriS->height;
						double zz = z-0.5*mriS->depth;
						double distance2 = xx*xx+yy*yy+zz*zz;
						double gauss    = factor * exp(- distance2 / sigma22 );
						dsum   += gauss;
						wcheck += gauss * (1.0 - w[val] * w[val]); 
						wchecksqrt += gauss * (1.0 - w[val]);  //!!!!! historical, better not use the square root (use wcheck)
            count++;
          }
        }
    //cout << std::endl;
//		MRIwrite(gmri,"mri_gauss.mgz");
//		MRIwrite(mri_indexing, "mri_indexing.mgz");
//		MRIwrite(pw.second, "mri_weights.mgz");
//		MRIfree(&gmri);
    //std::cout << " count-1: " << count-1 << " rows: " << pwm.second->rows << std::endl;
    assert(count == w.size());
		
		wcheck = wcheck / dsum;
		wchecksqrt = wchecksqrt / dsum;
		if (verbose > 1)
   		std::cout << "   - Weight Check: " << wcheck << "  wsqrt: " << wchecksqrt<< std::endl;
//		if (wcheck > 0.5) 
//		{
//		   std::cerr << " Too many voxels in the center are removed! Try to set a larger SAT value! " << std::endl;
//		   exit(1);
//		}

    //if (pwm.second != NULL) MatrixFree(&pwm.second);
  }
  else
  {
    if (verbose > 1) std::cout << "   - compute least squares estimate ..." << std::flush;
    //pw.first = R.getLSEst();
    p = R.getLSEst();
		A.clear();
		b.clear();
    if (verbose > 1) std::cout << "  DONE" << std::endl;
    //pw.second = NULL; // no weights in this case
		if (mri_weights) MRIfree(&mri_weights);
  }

//  zeroweights = R.getLastZeroWeightPercent();
  zeroweights = R.getLastWeightPercent(); // does not need pointers A and B to be valid

//  R.plotPartialSat(name);

  Md = convertP2Md(p,rtype);

  return Md;
}

// template <class T>
// vnl_vector <double > RegistrationStep<T>::computeRegistrationStepP(MRI * mriS, MRI* mriT)
// // computes Registration single step
// // retruns parameter Vector only
// {
//   std::pair < vnl_vector <double >, MRI*> pw = computeRegistrationStepW(mriS,mriT);
//   if (pw.second)
//   {
//     //cout << "mrisweight widht " << pw.second->width << std::endl;
// //     if (debug > 0)
// //     {
// //       string n = name+string("-mriS-weights.mgz");
// //       MRIwrite(pw.second,n.c_str());
// //     }
//     MRIfree(&pw.second);
//   }
//   return pw.first;
// }
// 
// 
// template <class T>
// std::pair <vnl_matrix_fixed <double,4,4 >,double> RegistrationStep<T>::computeRegistrationStep(MRI * mriS, MRI* mriT)
// // computes Registration single step
// // retruns 4x4 matrix and iscale value
// {
// //cout << "  Registration::computeRegistrationStep " << std::endl;
//   vnl_vector <double > p = computeRegistrationStepP(mriS,mriT);
// //   std::cout << " prows: " << p->rows << std::endl;
//   std::pair <vnl_matrix_fixed <double,4,4 >, double> pd = convertP2Md(p,rtype);
//   return pd;
// }
// 
// 

template <class T>
void RegistrationStep<T>::constructAb(MRI *mriS, MRI *mriT,vnl_matrix < T >& A,vnl_vector< T >&b)
// similar to robust paper
// (with symmetry and iscale)
{

  if (verbose > 1) std::cout << "   - constructAb: " << std::endl;

  assert(mriT != NULL);
  assert(mriS != NULL);
  assert(mriS->width == mriT->width);
  assert(mriS->height== mriT->height);
  assert(mriS->depth == mriT->depth);
  assert(mriS->type  == mriT->type);
  //assert(mriS->width == mask->width);
  //assert(mriS->height== mask->height);
  //assert(mriS->depth == mask->depth);
  //assert(mask->type == MRI_INT);
  //MRIclear(mask);

  int z,y,x;
  long int ss = mriS->width * mriS->height * mriS->depth;
  if (mri_indexing) MRIfree(&mri_indexing);
  if (ss > std::numeric_limits<int>::max())
  {
     if (verbose > 1) std::cout << "     -- using LONG for indexing ... " << std::flush;
     mri_indexing = MRIalloc(mriS->width, mriS->height, mriS->depth,MRI_LONG);
     if (mri_indexing == NULL) 
        ErrorExit(ERROR_NO_MEMORY,"Registration::constructAB could not allocate memory for mri_indexing") ;
     if (verbose > 1) std::cout << " done!" << std::endl;
  }
  else 
  {
     double mu = ((double)ss) * sizeof(int) / (1024.0 * 1024.0);
     if (verbose > 1) std::cout << "     -- allocating " << mu << "Mb mem for indexing ... " << std::flush;
     mri_indexing = MRIalloc(mriS->width, mriS->height, mriS->depth,MRI_INT);
     if (mri_indexing == NULL) 
        ErrorExit(ERROR_NO_MEMORY,"Registration::constructAB could not allocate memory for mri_indexing") ;
     if (verbose > 1) std::cout << " done!" << std::endl;
  }

  for (z = 0 ; z < mriS->depth ; z++)
    for (x = 0 ; x < mriS->width ; x++)
      for (y = 0 ; y < mriS->height ; y++)
        MRILvox(mri_indexing, x, y, z) = -10;


  bool dosubsample = false;
  if (subsamplesize > 0)
    dosubsample = (mriS->width > subsamplesize && mriS->height > subsamplesize && mriS->depth > subsamplesize);

  // we will need the derivatives
  if (verbose > 1) std::cout << "     -- compute derivatives ... " << std::flush;
#if 0
  MRI *Sfx=NULL,*Sfy=NULL,*Sfz=NULL,*Sbl=NULL;
  MRI *Tfx=NULL,*Tfy=NULL,*Tfz=NULL,*Tbl=NULL;
  MyMRI::getPartials(mriS,Sfx,Sfy,Sfz,Sbl);
  MyMRI::getPartials(mriT,Tfx,Tfy,Tfz,Tbl);
  MRI * SmT  = MRIsubtract(Sbl,Tbl,NULL); //S-T = f2-f1 =  delta f from paper
  MRI * fx1  = MRIadd(Sfx,Tfx,Tfx); // store at Tfx location, renamed to fx1
  MRIfree(&Sfx);
  MRIscalarMul(fx1,fx1,0.5);
  MRI * fy1  = MRIadd(Sfy,Tfy,Tfy); // store at Tfy location, renamed to fy1
  MRIfree(&Sfy);
  MRIscalarMul(fy1,fy1,0.5);
  MRI * fz1  = MRIadd(Sfz,Tfz,Tfz); // store at Tfz location, renamed to fz1
  MRIfree(&Sfz);
  MRIscalarMul(fz1,fz1,0.5);
  MRI * ft1  = MRIadd(Sbl,Tbl,Tbl); // store at Tbl location, renamed to ft1
  MRIfree(&Sbl);
  MRIscalarMul(ft1,ft1,0.5);
#else
  MRI *SpTh = MRIalloc(mriS->width,mriS->height,mriS->depth,MRI_FLOAT);
	SpTh = MRIadd(mriS,mriT,SpTh);
	SpTh = MRIscalarMul(SpTh,SpTh,0.5);
	MRI *fx1 = NULL, *fy1 = NULL, *fz1 = NULL, *ft1 = NULL;
	MyMRI::getPartials(SpTh,fx1,fy1,fz1,ft1);
	MRI * SmT = MRIalloc(mriS->width,mriS->height,mriS->depth,MRI_FLOAT);
	SmT = MRIsubtract(mriS,mriT,SmT);
	SmT = MyMRI::getBlur(SmT,SmT);
#endif
  if (verbose > 1) std::cout << " done!" << std::endl;
  //MRIwrite(fx1,"fx.mgz");
  //MRIwrite(fy1,"fy.mgz");
  //MRIwrite(fz1,"fz.mgz");
  //MRIwrite(ft1,"ft.mgz");

  // subsample if wanted
  MRI * fx,* fy,* fz,* ft;
  if (dosubsample)
  {
    if (verbose > 1) std::cout << "     -- subsample ... "<< std::flush;

    fx = MyMRI::subSample(fx1);
    MRIfree(&fx1);
    fy = MyMRI::subSample(fy1);
    MRIfree(&fy1);
    fz = MyMRI::subSample(fz1);
    MRIfree(&fz1);
    ft = MyMRI::subSample(ft1);
    MRIfree(&ft1);
		
		MRI * SmTt = SmT;
		SmT = MyMRI::subSample(SmTt);
    MRIfree(&SmTt);
		
    if (verbose > 1) std::cout << " done! " << std::endl;
  }
  else //just rename
  {
    fx = fx1;
    fy = fy1;
    fz = fz1;
    ft = ft1;
  }

//cout << " size fx : " << fx->width << " , " << fx->height << " , " << fx->depth << std::endl;
//cout << " size src: " << mriS->width << " , " << mriS->height << " , " << mriS->depth << std::endl;

  // compute 'counti': the number of rows needed (zero elements need to be removed)
  int n = fx->width * fx->height * fx->depth;
  if (verbose > 1) std::cout << "     -- size " << fx->width << " x " << fx->height << " x " << fx->depth << " = " << n << std::flush;
  long int counti = 0;
  double eps = 0.00001;
	int fxd = fx->depth ;
	int fxw = fx->width ;
	int fxh = fx->height ;
	int fxstart = 0;
  for (z = fxstart ; z < fxd ; z++)
    for (x = fxstart ; x < fxw ; x++)
      for (y = fxstart ; y < fxh ; y++)
      {
        if (isnan(MRIFvox(fx, x, y, z)) ||isnan(MRIFvox(fy, x, y, z)) || isnan(MRIFvox(fz, x, y, z)) || isnan(MRIFvox(ft, x, y, z)) )
        {
          if (verbose > 0) std::cout << " found a nan value!!!" << std::endl;
          continue;
        }
        if (fabs(MRIFvox(fx, x, y, z)) < eps  && fabs(MRIFvox(fy, x, y, z)) < eps &&  fabs(MRIFvox(fz, x, y, z)) < eps )
        {
          //if (verbose > 0) std::cout << " found a zero element !!!" << std::endl;
          continue;
        }
        counti++; // start with 1
      }	
  if (verbose > 1) std::cout << "  need only: " << counti << std::endl;
	if (counti == 0)
	{
	   cerr << std::endl;
	   cerr << " ERROR: All entries are zero! Images do not overlap." << std::endl;
		 cerr << "    Try calling with --noinit (if the original images are well aligned)" << std::endl;
		 cerr << "    Or use --transform <init.lta> with an approximate alignment" <<endl;
		 cerr << "    obtained from tkregister or another registration program." << std::endl << std::endl;
		 exit(1);
	}
   

  // allocate the space for A and B
  int pnum = 12;
  if (transonly)  pnum = 3;
  else if (rigid) pnum = 6;
  if (iscale) pnum++;

  double amu = ((double)counti*(pnum+1)) * sizeof(T) / (1024.0 * 1024.0); // +1 =  rowpointer vector
	double bmu = (double)counti * sizeof(T) / (1024.0 * 1024.0);
  if (verbose > 1) std::cout << "     -- allocating " << amu + bmu<< "Mb mem for A and b ... " << std::flush;
	bool OK = A.set_size(counti,pnum);
	OK = OK && b.set_size(counti);
	if ( !OK )
	{
 	  std::cout << std::endl;
     ErrorExit(ERROR_NO_MEMORY,"Registration::constructAB could not allocate memory for A and b") ;
	
	}
//   MATRIX* A = MatrixAlloc(counti,pnum,MATRIX_REAL);
//   VECTOR* b = MatrixAlloc(counti,1,MATRIX_REAL);
//   if (A == NULL || b == NULL) 
// 	{
// 	  std::cout << std::endl;
//     ErrorExit(ERROR_NO_MEMORY,"Registration::constructAB could not allocate memory for A and b") ;
// 	}
  if (verbose > 1) std::cout << " done! " << std::endl;
	double maxmu = 5* amu + 7 * bmu;
	if (floatsvd) maxmu = amu + 3*bmu + 2*(amu+bmu);
  if (verbose > 1 ) std::cout << "         (MAX usage in SVD will be > " << maxmu << "Mb mem + 6 MRI) " << std::endl;
  if (maxmu > 3800)
	{
	  std::cout << "     -- WARNING: mem usage large: " << maxmu <<"Mb mem + 6 MRI" << std::endl;
		string fsvd;
		if (!floatsvd) fsvd = "--floatsvd and/or ";
	  std::cout << "          Maybe use "<<fsvd<< "--subsample <int> " << std::endl;
	}

  // Loop and construct A and b
  int xp1,yp1,zp1;
	long int count = 0;
  for (z = fxstart ; z < fxd ; z++)
    for (x = fxstart ; x < fxw ; x++)
      for (y = fxstart ; y < fxh ; y++)
      {
        if (isnan(MRIFvox(fx, x, y, z)) ||isnan(MRIFvox(fy, x, y, z)) || isnan(MRIFvox(fz, x, y, z)) || isnan(MRIFvox(ft, x, y, z)) )
        {
          //if (verbose > 0) std::cout << " found a nan value!!!" << std::endl;
          continue;
        }

        if (dosubsample)
        {
          //xp1 = 2*x+2;
          //yp1 = 2*y+2;
          //zp1 = 2*z+2;
          xp1 = 2*x;
          yp1 = 2*y;
          zp1 = 2*z;
        }
        else // if not subsampled, shift only due to 5 tab derivative above
        {
          //xp1 = x+2;
          //yp1 = y+2;
          //zp1 = z+2; 
          xp1 = x;
          yp1 = y;
          zp1 = z; 
        }
        assert(xp1 < mriS->width);
        assert(yp1 < mriS->height);
        assert(zp1 < mriS->depth);


        if (fabs(MRIFvox(fx, x, y, z)) < eps  && fabs(MRIFvox(fy, x, y, z)) < eps &&  fabs(MRIFvox(fz, x, y, z)) < eps )
        {
          //if (verbose > 0) std::cout << " found a zero element !!!" << std::endl;
          MRILvox(mri_indexing, xp1, yp1, zp1) = -1;
          continue;
        }

       // count++; // start with 1

        if (xp1 >= mriS->width || yp1 >= mriS->height || zp1 >= mriS->depth)
        {

          cerr << " outside !!! " << xp1 << " " << yp1 << " " << zp1 << std::endl;
          assert(1==2);
        }

        MRILvox(mri_indexing, xp1, yp1, zp1) = count;

        //cout << "x: " << x << " y: " << y << " z: " << z << " std::coutn: "<< count << std::endl;
        //cout << " " << count << " mrifx: " << MRIFvox(mri_fx, x, y, z) << " mrifx int: " << (int)MRIvox(mri_fx,x,y,z) <<endl;
				int dof = 0;
        if (transonly)
        {
//           *MATRIX_RELT(A, count, 1) = MRIFvox(fx, x, y, z);
//           *MATRIX_RELT(A, count, 2) = MRIFvox(fy, x, y, z);
//           *MATRIX_RELT(A, count, 3) = MRIFvox(fz, x, y, z);
          A[count][0] = MRIFvox(fx, x, y, z);
          A[count][1] = MRIFvox(fy, x, y, z);
          A[count][2] = MRIFvox(fz, x, y, z);
				  dof = 3;
        }
        else if (rigid)
        {
//           *MATRIX_RELT(A, count, 1) = MRIFvox(fx, x, y, z);
//           *MATRIX_RELT(A, count, 2) = MRIFvox(fy, x, y, z);
//           *MATRIX_RELT(A, count, 3) = MRIFvox(fz, x, y, z);
//           *MATRIX_RELT(A, count, 4) = MRIFvox(fz, x, y, z)*yp1 - MRIFvox(fy, x, y, z)*zp1;
//           *MATRIX_RELT(A, count, 5) = MRIFvox(fx, x, y, z)*zp1 - MRIFvox(fz, x, y, z)*xp1;
//           *MATRIX_RELT(A, count, 6) = MRIFvox(fy, x, y, z)*xp1 - MRIFvox(fx, x, y, z)*yp1;
          A[count][0] =  MRIFvox(fx, x, y, z);
          A[count][1] =  MRIFvox(fy, x, y, z);
          A[count][2] =  MRIFvox(fz, x, y, z);
          A[count][3] =  (MRIFvox(fz, x, y, z)*yp1 - MRIFvox(fy, x, y, z)*zp1);
          A[count][4] =  (MRIFvox(fx, x, y, z)*zp1 - MRIFvox(fz, x, y, z)*xp1);
          A[count][5] =  (MRIFvox(fy, x, y, z)*xp1 - MRIFvox(fx, x, y, z)*yp1);
					dof = 6;
					
        }
        else // affine
        {
//           *MATRIX_RELT(A, count, 1)  = MRIFvox(fx, x, y, z)*xp1;
//           *MATRIX_RELT(A, count, 2)  = MRIFvox(fx, x, y, z)*yp1;
//           *MATRIX_RELT(A, count, 3)  = MRIFvox(fx, x, y, z)*zp1;
//           *MATRIX_RELT(A, count, 4)  = MRIFvox(fx, x, y, z);
//           *MATRIX_RELT(A, count, 5)  = MRIFvox(fy, x, y, z)*xp1;
//           *MATRIX_RELT(A, count, 6)  = MRIFvox(fy, x, y, z)*yp1;
//           *MATRIX_RELT(A, count, 7)  = MRIFvox(fy, x, y, z)*zp1;
//           *MATRIX_RELT(A, count, 8)  = MRIFvox(fy, x, y, z);
//           *MATRIX_RELT(A, count, 9)  = MRIFvox(fz, x, y, z)*xp1;
//           *MATRIX_RELT(A, count, 10) = MRIFvox(fz, x, y, z)*yp1;
//           *MATRIX_RELT(A, count, 11) = MRIFvox(fz, x, y, z)*zp1;
//           *MATRIX_RELT(A, count, 12) = MRIFvox(fz, x, y, z);
          A[count][0]  = MRIFvox(fx, x, y, z)*xp1;
          A[count][1]  = MRIFvox(fx, x, y, z)*yp1;
          A[count][2]  = MRIFvox(fx, x, y, z)*zp1;
          A[count][3]  = MRIFvox(fx, x, y, z);
          A[count][4]  = MRIFvox(fy, x, y, z)*xp1;
          A[count][5]  = MRIFvox(fy, x, y, z)*yp1;
          A[count][6]  = MRIFvox(fy, x, y, z)*zp1;
          A[count][7]  = MRIFvox(fy, x, y, z);
          A[count][8]  = MRIFvox(fz, x, y, z)*xp1;
          A[count][9]  = MRIFvox(fz, x, y, z)*yp1;
          A[count][10] = MRIFvox(fz, x, y, z)*zp1;
          A[count][11] = MRIFvox(fz, x, y, z);
					dof = 12;
        }

////          if (iscale) *MATRIX_RELT(A, count, 7) = MRIFvox(Sbl, x, y, z);
 //         if (iscale) *MATRIX_RELT(A, count, dof+1) = MRIFvox(Tbl, x, y, z);
 
 // !! ISCALECHANGE
        // if (iscale) *MATRIX_RELT(A, count, dof+1) =  (0.5 / iscalefinal) * ( MRIFvox(Tbl, x, y, z) + MRIFvox(Sbl,x,y,z));

        // if (iscale) A[count][dof] =  (0.5 / iscalefinal) * ( MRIFvox(Tbl, x, y, z) + MRIFvox(Sbl,x,y,z));
         if (iscale) A[count][dof] =  MRIFvox(ft, x, y, z) / iscalefinal;
//         if (iscale) A[count][dof]  = MRIFvox(Sbl,x,y,z);
				 
//         *MATRIX_RELT(b, count, 1) = - MRIFvox(ft, x, y, z); // ft = T-S => -ft = S-T
//         b[count] = - MRIFvox(ft, x, y, z); // ft was = T-S => -ft = S-T
         b[count] =  MRIFvox(SmT, x, y, z); // S-T

        count++; // start with 0 above

      }
	assert(counti == count);
      
  // free remaining MRI    
  MRIfree(&fx);
  MRIfree(&fy);
  MRIfree(&fz);
  MRIfree(&ft);
	MRIfree(&SmT);
  //if (Sbl) MRIfree(&Sbl);
  //if (Tbl) MRIfree(&Tbl);

  // Setup return std::pair
 //  std::pair <MATRIX*, VECTOR* > Ab(A,b);
	
// 	if (counter == 1) exit(1);
// 	counter++;
// 	MatrixWriteTxt((name+"A.txt").c_str(),A);
// 	MatrixWriteTxt((name+"b.txt").c_str(),b);
//	if (counter == 1) exit(1);
	
//   // adjust sizes
//   std::pair <MATRIX*, VECTOR* > Ab(NULL,NULL);
//   double abmu2 = ((double)count*(pnum+1)) * sizeof(float) / (1024.0 * 1024.0);
//   if (verbose > 1) std::cout << "     -- allocating another " << abmu2 << "Mb mem for A and b ... " << std::flush;
//   Ab.first  = MatrixAlloc(count,pnum,MATRIX_REAL);
//   Ab.second = MatrixAlloc(count,1,MATRIX_REAL);
//   if (Ab.first == NULL || Ab.second == NULL) 
// 	{
// 	  std::cout << std::endl;
//     ErrorExit(ERROR_NO_MEMORY,"Registration::constructAB could not allocate memory for Ab.first Ab.second") ;
// 	}
//   if (verbose > 1) std::cout << " done! " << std::endl;	
//   for (int rr = 1; rr<= count; rr++)
//   {
//     *MATRIX_RELT(Ab.second, rr, 1) = *MATRIX_RELT(b, rr, 1);
//     for (int cc = 1; cc <= pnum; cc++)
//     {
//       *MATRIX_RELT(Ab.first, rr, cc) = *MATRIX_RELT(A, rr, cc);
//       assert (!isnan(*MATRIX_RELT(Ab.first, rr, cc)));
//     }
//     assert (!isnan(*MATRIX_RELT(Ab.second, rr, 1)));
//   }
//   MatrixFree(&A);
//   MatrixFree(&b);

  return;
}


template <class T>
pair < vnl_matrix_fixed <double,4,4 >, double > RegistrationStep<T>::convertP2Md(const vnl_vector < T >& p, int rtype)
// rtype : use restriction (if 2) or rigid from robust paper
// returns registration as 4x4 matrix M, and iscale
{
//   std::cout << " RegistrationStep<T>::convertP2Md(MATRIX* p) (p->rows: " << p->rows << " )" << std::flush;
  std::pair < vnl_matrix_fixed <double,4,4 >, double> ret; ret.second = 0.0;

  vnl_vector < T > pt;
	
  if (p.size() == 4 ||p.size() == 7 || p.size() == 10|| p.size() == 13) // iscale
  {
    //std::cout << " has intensity " << std::endl;
    // cut off intensity scale
		
		// ISCALECHANGE:
   // //ret.second = 1.0-*MATRIX_RELT(p, p->rows, 1);		
//    ret.second = 1.0/(1.0+*MATRIX_RELT(p, p->rows, 1));
    ret.second =  (double) p[p.size()-1];
		
    pt.set_size(p.size()-1);
    for (unsigned int rr = 0; rr< pt.size(); rr++)
      pt[rr] = p[rr];
  }
  else pt = p;

  if (pt.size() == 12)
	{
	  ret.first.set_identity();
    int count = 0;
    for (int rr = 0;rr<3;rr++)
    for (int cc = 0;cc<4;cc++)
    {
      ret.first[rr][cc] +=  pt[count];
      count++;
    }
	  //ret.first = MyMatrix::aff2mat(pt,NULL);
	} 
  else if (pt.size() == 6)
  {
    //ret.first = p2mat(pt,NULL);
		
		// split translation and rotation:
		vnl_vector_fixed <double,3 > t;
		vnl_vector_fixed <double,3 > r;
    for (int rr = 0;rr<3;rr++)
    {
      t[rr] = (double)pt[rr];
      r[rr] = (double)pt[rr+3];
    }
    // converts rot vector (3x1) and translation vector (3x1)
    // into an affine matrix (homogeneous coord) 4x4
    // if global rtype ==1 r1,r2,r3 are as in robust paper (axis, and length is angle)
    // if global rtype ==2 then r1,r2,r3 are angles around x,y,z axis (order 1zrot,2yrot,3xrot)
		vnl_matrix < double > rmat;
		Quaternion q;
    if (rtype == 2)
    {
      // first convert rotation to quaternion (clockwise)
      q.importZYXAngles(-r[2], -r[1], -r[0]);
    }
    else if (rtype ==1)
    {
      // first convert rotation to quaternion;
      q.importRotVec(r[0],r[1],r[2]);
    }
    else assert (1==2);
    // then to rotation matrix
    rmat = MyMatrix::getVNLMatrix(q.getRotMatrix3d(),3);
		
    int rr, cc;
    for (rr=0;rr<3;rr++)
    {
      for (cc=0;cc<3;cc++) // copy rot-matrix
        ret.first[rr][cc] = rmat[rr][cc];

      // copy translation into 4th column
      ret.first[rr][3] = t[rr];
      // set 4th row to zero
      ret.first[3][rr] = 0.0;
    }
    //except 4,4
    ret.first[3][3] = 1.0;
  }
  else if (pt.size() ==3)
  {
	  ret.first.set_identity();
		ret.first[0][3] = pt[0];
		ret.first[1][3] = pt[1];
		ret.first[2][3] = pt[2];
  }
  else
  {
    cerr << " parameter neither 3,6 nor 12 : " << pt.size() <<" ??" << std::endl;
    assert(1==2);
  }

//   std::cout << " -- DONE " << std::endl;
  return ret;
}

#endif
