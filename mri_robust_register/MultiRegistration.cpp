/**
 * @file MultiRegistration.cpp
 * @brief A class to handle registration of multiple files
 *
 * MultiRegistration is a class to compute a robust registration
 *  of several images. It makes use routines from Registration 
 *
 * written by Martin Reuter
 *  Aug. 12th ,2009
 *
 */

/*
 * Original Author: Martin Reuter
 * CVS Revision Info:
 *    $Author: mreuter $
 *    $Date: 2010/08/31 22:19:45 $
 *    $Revision: 1.11.2.5 $
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



#include "MultiRegistration.h"
#include "Registration.h"
#include "Regression.h"
#include "RobustGaussian.h"
#include "CostFunctions.h"
#include "MyMatrix.h"
#include "MyMRI.h"

#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vnl/vnl_inverse.h>
#include <vnl/vnl_matrix.h>
#include <vnl/vnl_matrix_fixed.h>
#include <vnl/algo/vnl_svd.h>

// all other software are all in "C"
#ifdef __cplusplus
extern "C"
{
#endif
#include "error.h"
#include "macros.h"
#include "mri.h"
#include "matrix.h"
#ifdef __cplusplus
}
#endif

using namespace std;

void MultiRegistration::clear()
{
  if (mri_mean) MRIfree(&mri_mean);

  for (unsigned int i = 0;i<mri_mov.size();i++)
  {
			if (i<ltas.size() && ltas[i]) LTAfree(&ltas[i]);
			if (i<mri_warps.size() && mri_warps[i]) MRIfree(&mri_warps[i]);
      if (i<mri_weights.size() && mri_weights[i]) MRIfree(&mri_weights[i]);
      if (i<mri_mov.size() && mri_mov[i]) MRIfree(&mri_mov[i]);

  }

}

/*!
  \fn int loadMovables(const std::vector < std::string > mov)
  \brief Loads the movable volumes as specified on command line
  \param P  Paramters for the initialization
*/
int MultiRegistration::loadMovables(const std::vector < std::string > pmov)
{

  assert (mri_mov.size () == 0);
  int n = (int) pmov.size();
  assert(n>0);
  mri_mov.resize(n);
  mov = pmov; // copy of input filenames
  vector < double > msize (pmov.size());
  for (unsigned int i = 0;i<mov.size(); i++)
  {
    cout << "reading source '"<<mov[i]<<"'..." << endl;

    mri_mov[i] = MRIread(mov[i].c_str()) ;
    if (!mri_mov[i])
    {
      ErrorExit(ERROR_NOFILE, "MultiRegistration::loadMovables: could not open input volume %s.\n",
                mov[i].c_str()) ;
    }
    msize[i] = mri_mov[i]->xsize;
    if (mri_mov[i]->ysize < msize[i]) msize[i] = mri_mov[i]->ysize ;
    if (mri_mov[i]->zsize < msize[i]) msize[i] = mri_mov[i]->zsize ;	
  }
  float EPS = 0.00001;
  for (unsigned int i = 1;i<mov.size();i++)
  {
    if (fabs(mri_mov[i]->xsize-mri_mov[0]->xsize) > EPS || fabs(mri_mov[i]->ysize-mri_mov[0]->ysize) > EPS || fabs(mri_mov[i]->zsize-mri_mov[0]->zsize) > EPS)
    {
      cerr << "ERROR: MultiRegistration::loadMovables: images have different voxel sizes.\n";
      cerr << "  Currently not supported, maybe first make conform?\n";
      cerr << "  Debug info: size(" << i << ") = " << mri_mov[i]->xsize << ", " << mri_mov[i]->ysize << ", " << mri_mov[i]->zsize << "   size(0) = " << mri_mov[0]->xsize << ", " << mri_mov[0]->ysize << ", " << mri_mov[0]->zsize << endl;
      ErrorExit(ERROR_BADFILE, "MultiRegistration::loadMovables: voxel size is different %s.\n",
                mov[i].c_str()) ;	 
    }
  }

  mri_warps.resize(n,NULL);
  intensities.resize(n,1.0);
  ltas.resize(n,NULL);
  if (robust) mri_weights.resize(n,NULL);

  return mov.size();
}

int MultiRegistration::loadLTAs(const std::vector < std::string > nltas)
{
	 assert(nltas.size() == mov.size());
	 assert(ltas.size() == mov.size());
	 iltas = nltas; // copy of input filenames
	 for (uint i = 0;i<nltas.size();i++)
	 {
        TRANSFORM * trans = TransformRead(nltas[i].c_str());
        LTA* lta =  (LTA *)trans->xform ;
        if (!lta)
          ErrorExit(ERROR_BADFILE, "MultiRegistration::loadLTAs could not read transform file %s", nltas[i].c_str()) ;
        if ( ! lta->xforms[0].src.valid)
          ErrorExit(ERROR_BADFILE, "MultiRegistration::loadLTAs no source geometry, use lta with valid geometry ( %s )",nltas[i].c_str());
        if ( ! lta->xforms[0].dst.valid)
          ErrorExit(ERROR_BADFILE, "MultiRegistration::loadLTAs no target geometry, use lta with valid geometry ( %s )",nltas[i].c_str());

        lta = LTAchangeType(lta,LINEAR_VOX_TO_VOX);

        ltas[i] = lta;
   }
   return (int)ltas.size();
}



/*!
  \fn void initRegistration(Registration & R, const Parameters & P)
  \brief Initializes a Registration with Parameters (rigid, iscale, transonly, robust, sat and doubleprec)
  \param R  Registration to be initialized
*/
void MultiRegistration::initRegistration(Registration & R)
{
  // assert(n < (int) P.mov.size());

  R.setRigid(rigid);
  R.setIscale(iscale);
  R.setTransonly(transonly);
  R.setRobust(robust);
  R.setSaturation(sat);
  R.setDoublePrec(doubleprec);
  //R.setDebug(debug);
  
	
	if (subsamplesize > 0) R.setSubsamplesize(subsamplesize);

//   int pos = P.mov[n].rfind(".");
//   if (pos > 0) R.setName(P.mov[n].substr(0,pos));
//   else  R.setName(P.mov[n]);
//
//  // if (P.subsamplesize > 0) R.setSubsamplesize(P.subsamplesize);
//
//
//   R.setSource(P.mri_mov[n],P.fixvoxel,P.keeptype);
//   R.setTarget(P.mri_mean,P.fixvoxel,P.keeptype);
}

bool MultiRegistration::averageSet(int itdebug)
// maps mov to template (using ltas)
// adjust intensities (if iscale)
// creates template acording to average:1 mean, 2 median..
{
  unsigned int nin = mri_mov.size();
	assert(nin > 1);
  assert (mri_warps.size() == nin);
	assert (ltas.size() == nin);
	assert (intensities.size() == nin);
	
    cout << "warping movs and creating initial template..." << endl;
		if (iscale) cout << " allow intensity scaling" << endl;
    for (unsigned int i = 0;i<nin;i++)
    {  
      if (mri_warps[i]) MRIfree(&mri_warps[i]); 
      //mri_warps[i] = MRIclone(mri_mov[0],mri_warps[i]);
      //mri_warps[i] = LTAtransform(mri_mov[i],mri_warps[i], ltas[i]);
			// use geometry from ltas
			// (if initXforms was called, this is the center of mass of all tps)
      mri_warps[i] = LTAtransform(mri_mov[i],NULL, ltas[i]);
		  MRIcopyPulseParameters(mri_mov[i],mri_warps[i]);
			if (iscale)
			{
			   mri_warps[i] = MyMRI::MRIvalscale(mri_warps[i],mri_warps[i], intensities[i]);
			}
      if (debug)
      {
        ostringstream oss;
        oss << outdir << "tp" << i+1 << "_to_template-it"<<itdebug<<".mgz";
        MRIwrite(mri_warps[i], oss.str().c_str()) ;
      }
    }
    mri_mean = averageSet(mri_warps, mri_mean,average,sat);
    return true;
}

MRI* MultiRegistration::averageConformSet(int itdebug)
// maps mov to template (using ltas)
// adjust intensities (if iscale)
// creates template acording to average:1 mean, 2 median..
{
  unsigned int nin = mri_mov.size();
	assert(nin > 1);
	assert (ltas.size() == nin);
	assert (intensities.size() == nin);
	
	std::vector < MRI* > mri_cwarps(nin,NULL);

    cout << "warping movs and creating conform template..." << endl;
		if (iscale) cout << " allow intensity scaling" << endl;
    for (unsigned int i = 0;i<nin;i++)
    {  

			// use geometry from ltas
			// (if initXforms was called, this is the center of mass of all tps)
      mri_cwarps[i] = LTAtransform(mri_mov[i],NULL, ltas[i]);
		  MRIcopyPulseParameters(mri_mov[i],mri_warps[i]);
			//????
            cerr << " MultiRegistration::averageConformSet not implemented, yet !!!" << endl;
			assert (1==2);
			
			if (iscale)
			{
			   mri_cwarps[i] = MyMRI::MRIvalscale(mri_cwarps[i],mri_cwarps[i], intensities[i]);
			}
      if (debug)
      {
        ostringstream oss;
        oss << outdir << "tp" << i+1 << "_to_template_conform-it"<<itdebug<<".mgz";
        MRIwrite(mri_cwarps[i], oss.str().c_str()) ;
      }
    }
    MRI* mri_cmean = averageSet(mri_cwarps, NULL,average,sat);
    return mri_cmean;
}

bool MultiRegistration::computeTemplate(int itmax, double eps , int iterate, double epsit)
// itmax : iterations for template creation
// iterate: iterations for registration
// epsit: 
// uses ltas (if set), mri_mov and parameters
// sets mri_mean, ltas, mri_warps (mri_weights and intensities if available)
{
  int nin = (int) mri_mov.size();
	assert(nin > 1);
  
	if (debug) cout << endl << endl << "MultiRegistration::computeTemplate(avitmax: "<< itmax << ", aveps " << eps << ", regit: " << iterate << ", regeps: " <<epsit<<" )" << endl;
	
	
  cout << "Computing first template" << endl;  
  bool havexforms = (ltas[0] != NULL);
  if (!havexforms) // create simple initial transforms (not recommended)
    mri_mean = initialAverageSet(mri_mov,NULL,average,sat);
  else // we have initial transforms
  {  

    averageSet(0);
//     cout << "warping movs and creating initial template..." << endl;
// 		if (iscale) cout << " allow intensity scaling" << endl;
//     for (int i = 0;i<nin;i++)
//     {  
//       if (mri_warps[i]) MRIfree(&mri_warps[i]); // should not happen
//       mri_warps[i] = MRIclone(mri_mov[0],mri_warps[i]);
//       mri_warps[i] = LTAtransform(mri_mov[i],mri_warps[i], ltas[i]);
// 			if (iscale)
// 			{
// 			   mri_warps[i] = MyMRI::MRIvalscale(mri_warps[i],mri_warps[i], intensities[i]);
// 			}
//       if (debug)
//       {
//         ostringstream oss;
//         oss << outdir << "tp" << i+1 << "_to_template-it0.mgz";
//         MRIwrite(mri_warps[i], oss.str().c_str()) ;
//       }
//     }
//     mri_mean = averageSet(mri_warps, mri_mean,average,sat);
  }
  
  if (itmax==0) // no iterations necessary just return with mean (and ltas and warps);
  {
    //strncpy(P.mri_mean->fname, P.mean.c_str(),STRLEN);
    return true;
  }

  
  strncpy(mri_mean->fname,(outdir+"template-it0.mgz").c_str(),STRLEN);
  if (debug)
  {
    cout << "debug: saving template-it0.mgz" << endl;
    MRIwrite(mri_mean,(outdir+"template-it0.mgz").c_str());
  }

   cout << "template fname: " << mri_mean->fname << endl;

//  int itmax  = 10;
//  double eps = 0.025;

  int itcount = 0;
  double maxchange = 100;

  //vector < MATRIX * > transforms(mri_mov.size(),NULL);
  vector < vnl_matrix_fixed < double, 4, 4> > transforms(mri_mov.size());
  if (havexforms) //init transforms
  {
     for (int i=0;i<nin;i++)
     {
        LTAchangeType(ltas[i],LINEAR_VOX_TO_VOX); //vox2vox for registration
        //transforms[i] = MatrixCopy(ltas[i]->xforms[0].m_L, NULL);
        transforms[i] = MyMatrix::convertMATRIX2VNL(ltas[i]->xforms[0].m_L);
     }
	
  }
  
  vector < Registration > Rv(mri_mov.size());
  for (int i = 0;i<nin;i++) Rv[i].setSource(mri_mov[i],fixvoxel,keeptype);

  LTA * lastlta = NULL;
  while (itcount < itmax && maxchange > eps)
  {
    itcount++;
		
    // if we have prior ltas, allow termination
		// by initializing maxchange =0 so that it can be 
		// computed below
		if ( ltas[0] ) maxchange = 0;

    cout << endl << "=======================================================" << endl;
    cout << endl << "Working on global iteration : " << itcount << endl;
    cout << endl << "=======================================================" << endl;
    //cout << "========================================" << endl;
    //printmemusage();
    //cout << "========================================" << endl << endl;
		
    // register all inputs to mean
    vector < double > dists(nin,1000); // should be larger than maxchange!
    for (int i = 0;i<nin;i++)
    {
		  cout << endl << "Working on TP " << i+1 << endl;
      Rv[i].clear();
      initRegistration(Rv[i]); //set parameter
      Rv[i].setTarget(mri_mean,
                      fixvoxel,
                      keeptype); // gaussian pyramid will be constructed for
                                  // each Rv[i], could be optimized
      ostringstream oss;
      oss << outdir << "tp" << i+1 << "_to_template-it" << itcount;
      Rv[i].setName(oss.str());

      // compute Alignment
      //std::pair <MATRIX*, double> Md;
      std::pair <vnl_matrix_fixed < double, 4, 4>, double> Md;
      //int iterate = P.iterate;
      //double epsit= P.epsit;
      int maxres = 0;
			// on higher iterations use subsamplesize as passed on commandline
      int subsamp = subsamplesize;
			// simplify first steps (only if we do not have good transforms):
			if (!havexforms)
			{
        switch (itcount)
        {
        case 1:
          maxres = 2;
          break;
        case 2:
          maxres = 2;
          break;
        case 3:
          maxres = 1;
          break;
        case 4:
          maxres = 1;
          break;
        case 5:
          subsamp = 180;
          break;
        }
		  }

//      if (havexforms) // default case, as we use the new mean space init!
//      {
//        //// tried high res iteration only:
//				// (does not make sense, we need 2 iter. on high res anyway)
//        //Md= Rv[i].computeIterativeRegistration(1,epsit,NULL,NULL,transforms[i],P.intensities[i]);
//        //dists[i] = sqrt(Rv[i].AffineTransDistSq(Md.first, transforms[i]));
//        //if (itcount ==1) subsamp = 180;	// so use subsample for first step on high res
//        maxres = 0; //go up to hig-res allways (skip first steps as above)
//      }

      Rv[i].setSubsamplesize(subsamp);
			if (satit) Rv[i].findSaturation();
      Rv[i].computeMultiresRegistration(maxres,
                                               iterate,
                                               epsit,
                                               NULL,
                                               NULL,
                                               transforms[i],
                                               intensities[i]);
		  Md.first  = Rv[i].getFinalVox2Vox();
			Md.second = Rv[i].getFinalIscale();
      //if (transforms[i]) MatrixFree(&transforms[i]);
      transforms[i] = Md.first;
      intensities[i] = Md.second;

      // convert Matrix to LTA ras to ras
      if (lastlta)  LTAfree(&lastlta);
      if (ltas[i]) lastlta = ltas[i];
      ltas[i] = MyMatrix::VOXmatrix2LTA(Md.first,mri_mov[i],mri_mean);
      //P.ltas[i] = LTAalloc(1,P.mri_mov[i]);
      //P.ltas[i]->xforms[0].m_L =
      //  MRIvoxelXformToRasXform (P.mri_mov[i],
      //                           P.mri_mean,
      //                           Md.first,
      //                           P.ltas[i]->xforms[0].m_L) ;
      //P.ltas[i]->type = LINEAR_RAS_TO_RAS ;
      //getVolGeom(P.mri_mov[i], &P.ltas[i]->xforms[0].src);
      //getVolGeom(P.mri_mean, &P.ltas[i]->xforms[0].dst);

      // compute maxchange
      if (lastlta)
      {
        LTAchangeType(lastlta,LINEAR_RAS_TO_RAS); //measure dist in RAS coords
        dists[i] = sqrt(MyMatrix::AffineTransDistSq(lastlta->xforms[0].m_L,
                                                ltas[i]->xforms[0].m_L));
        LTAfree(&lastlta);
        if (dists[i] > maxchange) maxchange = dists[i];
        cout << endl << "tp " << i+1 << " distance: " << dists[i] << endl;
      }


      // create warps: warp mov to mean
      cout << "warping mov to template..." << endl;
      if (mri_warps[i]) MRIfree(&mri_warps[i]);
      mri_warps[i] = MRIclone(mri_mean,mri_warps[i]);
      mri_warps[i] = LTAtransform(mri_mov[i],mri_warps[i], ltas[i]);
		  MRIcopyPulseParameters(mri_mov[i],mri_warps[i]);

      // here do scaling of intensity values
      if (Rv[i].isIscale() && Md.second > 0)
      {
        cout << "Adjusting Intensity of WARP by " << Md.second << endl;
        mri_warps[i] = MyMRI::MRIvalscale(mri_warps[i],
                                         mri_warps[i], Md.second);
      }
			
			// copy weights (as RV will be cleared)
      //   (info: they are in original half way space)
	     cout << "backup weights ..." << endl;
       mri_weights[i] = MRIcopy(Rv[i].getWeights(),mri_weights[i]);
			 
      //cout << " LS difference after: " <<
      //CF.leastSquares(mri_aligned,P.mri_dst) << endl;
      //cout << " NC difference after: " <<
      //CF.normalizedCorrelation(mri_aligned,P.mri_dst) << endl;


      if (debug)
      {
        cout << "debug: writing transforms, warps, weights ..." << endl;
        LTAwriteEx(ltas[i], (oss.str()+".lta").c_str()) ;

        MRIwrite(mri_warps[i], (oss.str()+".mgz").c_str()) ;

        if (Rv[i].isIscale() && Md.second >0)
        {
          string fn = oss.str() + "-intensity.txt";
          ofstream f(fn.c_str(),ios::out);
          f << Md.second;
          f.close();
        }

        // if we have weights:  
        if (mri_weights[i] != NULL)
        {
          //std::pair <MATRIX*, MATRIX*> map2weights = Rv[i].getHalfWayMaps();
          //MATRIX * hinv = MatrixInverse(map2weights.second,NULL);
          std::pair <vnl_matrix_fixed < double , 4, 4> , vnl_matrix_fixed < double, 4, 4 > > map2weights = Rv[i].getHalfWayMaps();
          vnl_matrix_fixed < double , 4, 4>  hinv = vnl_inverse(map2weights.second);

          cout << endl;
          //MatrixPrint(stdout,map2weights.first) ;
					cout << map2weights.first << endl;
          cout << endl;
          //MatrixPrint(stdout,map2weights.second) ;
					cout << map2weights.second << endl;
          cout << endl;
          //MatrixPrint(stdout,hinv) ;
					cout << hinv << endl;
          cout << endl;

          MRI * wtarg = MRIalloc(mri_weights[i]->width,
                                 mri_weights[i]->height,
                                 mri_weights[i]->depth,
                                 MRI_FLOAT);
          MRIcopyHeader(mri_weights[i],wtarg);
          MATRIX * v2r  = MRIgetVoxelToRasXform(mri_mean);
          MRIsetVoxelToRasXform(wtarg,v2r);
          wtarg->type = MRI_FLOAT;
          wtarg->i_to_r__ = MatrixCopy(mri_mean->i_to_r__, wtarg->i_to_r__);
          wtarg->r_to_i__ = MatrixCopy(mri_mean->r_to_i__, wtarg->r_to_i__);

          wtarg = MyMRI::MRIlinearTransform(mri_weights[i],wtarg, hinv);
          MRIwrite(wtarg, (oss.str()+"-weights.mgz").c_str()) ;
          MRIwrite(mri_weights[i], (oss.str()+"-www.mgz").c_str());
          MRIfree(&wtarg);
          //MatrixFree(&hinv);
          MatrixFree(&v2r);
        }
      } // if debug end
    
		  // clear to reduce memory usage:
      Rv[i].clear();
      Rv[i].freeGPT();
			
      cout << endl << "Finished TP : " << i+1 << endl;
      cout << endl;
      cout << "=====================================================" << endl;
      //printmemusage();
      //cout << "========================================" << endl << endl;

    } // for loop end (all timepoints)

    if (dists[0] <= maxchange) // it was computed, so print it:
    {
      cout << endl << "Global Iteration " << itcount <<" Distances: " << endl;
      for (unsigned int i=0;i<dists.size();i++) cout << dists[i] <<" ";
      cout << endl << "Maxchange: " << maxchange << endl << endl;
    }

    // create new mean
    cout << "Computing new template " <<itcount << endl;
    mri_mean = averageSet(mri_warps, mri_mean,average,sat);
    if (debug)
    {
      ostringstream oss;
      oss << outdir << "template-it" << itcount << ".mgz";
      cout << "debug: writing template to " << oss.str() << endl;
      MRIwrite(mri_mean,oss.str().c_str());
      //strncpy(mri_mean->fname, oss.str().c_str(),STRLEN);
    }

  } // end while

  //strncpy(P.mri_mean->fname, P.mean.c_str(),STRLEN);
  
	cout << " DONE : computeTemplate " << endl;
  return true;
}


bool MultiRegistration::halfWayTemplate(int maxres, int iterate, double epsit, bool vox2vox)
// can be used only with two input images
{
  int nin = (int) mri_mov.size();
  assert (nin == 2);

  // register 1 with 2

  Registration R;
  initRegistration(R); //set parameter
  R.setSourceAndTarget(mri_mov[0],mri_mov[1],keeptype);

  ostringstream oss;
  oss << outdir << "halfway_template.mgz";
  R.setName(oss.str().c_str());

  // use initial transform if given:
  //MATRIX *minit = NULL;
  vnl_matrix_fixed < double , 4 , 4 > minit;
	assert(ltas.size() ==2);
  if (ltas[0])
  {
    LTAchangeType(ltas[0],LINEAR_VOX_TO_VOX); //vox2vox for registration
    LTAchangeType(ltas[1],LINEAR_VOX_TO_VOX); //vox2vox for registration
    assert (ltas[0]->type == LINEAR_VOX_TO_VOX);
    assert (ltas[1]->type == LINEAR_VOX_TO_VOX);
    minit = vnl_inverse(MyMatrix::convertMATRIX2VNL(ltas[1]->xforms[0].m_L)) * MyMatrix::convertMATRIX2VNL(ltas[0]->xforms[0].m_L) ;
    R.setDebug(1);
  }	
		
  // compute Alignment
  //std::pair <MATRIX*, double> Md;
  std::pair < vnl_matrix_fixed < double, 4, 4> , double> Md;
	// adjust subsamplesize, if passed:
	if (subsamplesize > 0 ) R.setSubsamplesize(subsamplesize);
	if (satit) R.findSaturation();
  R.computeMultiresRegistration(maxres,iterate,epsit,NULL,NULL,minit);
	Md.first  = R.getFinalVox2Vox();
	Md.second = R.getFinalIscale();
	//if (minit) MatrixFree(&minit);
  intensities[0] = Md.second;
  intensities[1] = 1;

  cout << "creating half way data ..." << endl;
	assert(MyMRI::isIsotropic(mri_mov[0]) && MyMRI::isIsotropic(mri_mov[1]));
	
  std::pair < vnl_matrix_fixed < double, 4, 4>, vnl_matrix_fixed < double, 4, 4> > maps2weights = R.getHalfWayMaps();
  LTA * m2hwlta = LTAalloc(1,mri_mov[0]);
  LTA * d2hwlta = LTAalloc(1,mri_mov[1]);
  if (!vox2vox) // do ras to ras
  {
    // cout << "converting VOX to RAS and saving RAS2RAS..." << endl ;
    // (use geometry of destination space for half-way) WIll NOT WORK FOR nonistorpic due to internal resampling
		m2hwlta->xforms[0].m_L = MyMatrix::convertVNL2MATRIX(maps2weights.first);
    m2hwlta->xforms[0].m_L = MRIvoxelXformToRasXform (mri_mov[0],
                                                      mri_mov[1],
                                                      m2hwlta->xforms[0].m_L,
                                                      m2hwlta->xforms[0].m_L) ;
    m2hwlta->type = LINEAR_RAS_TO_RAS ;
		
		d2hwlta->xforms[0].m_L = MyMatrix::convertVNL2MATRIX(maps2weights.second);
    d2hwlta->xforms[0].m_L = MRIvoxelXformToRasXform (mri_mov[1],
                                                      mri_mov[1],
                                                      d2hwlta->xforms[0].m_L,
                                                      d2hwlta->xforms[0].m_L) ;
    d2hwlta->type = LINEAR_RAS_TO_RAS ;
  }
  else // vox to vox
  {
    // cout << "saving VOX2VOX..." << endl ;
    m2hwlta->xforms[0].m_L = MyMatrix::convertVNL2MATRIX(maps2weights.first,
                                        m2hwlta->xforms[0].m_L) ;
    m2hwlta->type = LINEAR_VOX_TO_VOX ;
    d2hwlta->xforms[0].m_L = MyMatrix::convertVNL2MATRIX(maps2weights.second,
                                        m2hwlta->xforms[0].m_L) ;
    d2hwlta->type = LINEAR_VOX_TO_VOX ;
  }
  // add src and dst info (use src as target geometry in both cases)
  getVolGeom(mri_mov[0], &m2hwlta->xforms[0].src);
  getVolGeom(mri_mov[0], &m2hwlta->xforms[0].dst);
  getVolGeom(mri_mov[1], &d2hwlta->xforms[0].src);
  getVolGeom(mri_mov[0], &d2hwlta->xforms[0].dst);

  ltas.resize(2,NULL);
  if (ltas[0]) LTAfree(&ltas[0]);
  if (ltas[1]) LTAfree(&ltas[1]);
  ltas[0] = m2hwlta;
  ltas[1] = d2hwlta;

  cout << " creating half-way template ..." << endl;
  // take dst info from lta:
  if (mri_warps[0]) MRIfree(&mri_warps[0]);
  if (mri_warps[1]) MRIfree(&mri_warps[1]);
  mri_warps[0] = LTAtransform(mri_mov[0],NULL, m2hwlta);
	MRIcopyPulseParameters(mri_mov[0],mri_warps[0]);
  mri_warps[1] = LTAtransform(mri_mov[1],NULL, d2hwlta);
	MRIcopyPulseParameters(mri_mov[1],mri_warps[1]);
  // here do scaling of intensity values
  if (R.isIscale() && Md.second > 0)
  {
    cout << "Adjusting Intensity of MOV by " << Md.second << endl;
    mri_warps[0] = MyMRI::MRIvalscale(mri_warps[0], mri_warps[0], Md.second);
  }
  mri_mean = averageSet(mri_warps, mri_mean,average,sat);


  mri_weights[0] = R.getWeights(); // in original half way space
  mri_weights[1] = mri_weights[0];


  // copy weights for both if we have them (as the R->weights will be destroyed):
  if (mri_weights[0])
    for (unsigned int i = 0;i<mri_weights.size();i++)
      mri_weights[i] = MRIcopy(mri_weights[0],NULL);
			
  //MatrixFree(&Md.first);
	return true;
}


bool MultiRegistration::initialXforms(int tpi, bool fixtp, int maxres, int iterate, double epsit)
// will set ltas (as RAS to RAS )
// tpi 1....n
// uses outdir
{
  if (tpi <= 0) return false; // no init?
  cout << endl << "MultiRegistration::initializing Xforms (init " << tpi << " , maxres "<< maxres << " , iterate " << iterate << " , epsit " << epsit<< " ) : " << endl;
  assert(ltas.size() == mri_mov.size());
  assert(mri_mov.size() > 1);
  
  int nin = (int) mri_mov.size();
	
  tpi--; // tpi 0....n-1
  assert (tpi>=0 && tpi <nin);
	
  // create index set for lookup (tpi should switch places with 0)
  vector < int > index(nin);
  for (int i = 0;i<nin;i++) index[i] = i;
  index[0] = tpi;
  index[tpi] = 0;


  // Register everything to tpi TP
//  vector < Registration > Rv(nin);
  vector < std::pair <vnl_matrix_fixed < double, 4, 4> , double> > Md(nin);
	vector < double >  centroid;

  //Md[0].first = MatrixIdentity(4,NULL);
  Md[0].first.set_identity();
  Md[0].second= 1.0;
  for (int i = 1;i<nin;i++) 
  {
	  int j = index[i]; // use new index
    cout << endl << "  computing initial registration of TP "<< j+1 <<" ( "<<mov[j]<<" )" << endl;
		cout << "   wrt to TP "<<tpi+1<<" ( "<<mov[tpi]<<" )" << endl;

    ostringstream oss;
    oss << outdir << "tp" << j+1 << "_to_tp" << tpi;
    
    Registration R;
    initRegistration(R); //set parameter
    R.setSourceAndTarget(mri_mov[j],mri_mov[tpi],keeptype);
    R.setName(oss.str());
		
    // compute Alignment (maxres,iterate,epsit) are passed above
	  if (satit) R.findSaturation();
    R.computeMultiresRegistration(maxres,iterate,epsit);
		Md[i].first = R.getFinalVox2Vox();
		Md[i].second = R.getFinalIscale();
		
		// get centroid of tpi:
		if (i==1) centroid = R.getCentroidT();

//    ostringstream oss2;
//    oss2 << "tp" << j+1;    
//    MatrixWrite(Md[i].first,(oss2.str()+".fsmat").c_str(),oss2.str().c_str());
//    Md[i].first = MatrixRead((oss2.str()+".fsmat").c_str());
//    Md[i].second = 1.0;

//     if (P.debug)
//     {
//       LTA * nlta = VOXmatrix2LTA(Md[i].first,P.mri_mov[j],P.mri_mov[tpi]);
//       LTAwriteEx(nlta, (oss.str()+".lta").c_str()) ;
// 
//       MRI* warped = MRIclone(P.mri_mov[tpi],NULL);
//       warped = LTAtransform(P.mri_mov[j],warped, nlta);
// 
//       if (R.isIscale() && Md[i].second >0)
//       {
//         string fn = oss.str() + "-intensity.txt";
//         ofstream f(fn.c_str(),ios::out);
//         f << Md[i].second;
//         f.close();
// 	
//         warped = R.MRIvalscale(warped,warped, Md[i].second);
//       }
//       MRIwrite(warped, (oss.str()+".mgz").c_str()) ;
//       // todo: maybe output weights (not necessary so far) 
//       LTAfree(&nlta);
//       MRIfree(&warped);     
//     }
  } // end for loop (initial registration to inittp)
	
	assert(ltas.size() == mri_mov.size());
	if (fixtp) // we are basically done, as we maped to this (itp) TP
	{
			
    for (int i = 0;i<nin;i++) 
    {
  	  int j = index[i]; // use new index
      
      assert(ltas[j] == NULL);
      ltas[j] = MyMatrix::VOXmatrix2LTA(Md[i].first,mri_mov[j],mri_mov[tpi]); // use geometry of tpi
    	intensities[j] = Md[i].second;
			
      if (debug)
      {
        ostringstream oss;
        oss << outdir << "tp" << j+1 << "_to_mcoord";
        LTAwriteEx(ltas[j], (oss.str()+".lta").c_str()) ;
        MRI * warped = LTAtransform(mri_mov[j],NULL, ltas[j]);
        if (iscale)
        {
          string fn = oss.str() + "-intensity.txt";
          ofstream f(fn.c_str(),ios::out);
          f << Md[i].second;
          f.close();
	
          warped = MyMRI::MRIvalscale(warped,warped, Md[i].second);
        }
        MRIwrite(warped, (oss.str()+".mgz").c_str()) ;
        MRIfree(&warped);     
      }
    
      // cleanup
      //MatrixFree(&Md[i].first);
	  }
		
		return true;
	} // end fixtp
	
	
	// find geometric mean intensity scale
	if (iscale)
	{
	  double mint = 1.0;
	  for (int i = 0; i<nin;i++)
	  {
	     int j = index[i];
       if (i==0) intensities[j] = 1.0;
       else
  	   {
  		    assert (Md[i].second > 0);
    		  intensities[j] = Md[i].second;
  				mint *=  intensities[j];
    	 }
    }
	  // geometric mean
	  mint = pow(mint,1.0/nin);
	
	  // set intenstiy factors so that geo-mean is 1:
	  for (int i = 0;i<nin;i++)
	    intensities[i] *= (1.0 / mint);
	}
	
  // find mean center
//   vector <MATRIX* > mras(nin);
//   mras[0] = MatrixIdentity(4,NULL);
//   MATRIX* rot = MatrixIdentity(3,NULL);
//   MATRIX* roti = NULL;
//   MATRIX* trans = VectorAlloc(3,MATRIX_REAL);
//   MATRIX* meant = MatrixZero(3,1,NULL);
//   MATRIX* meanr = MatrixIdentity(3,NULL);
  vector < vnl_matrix_fixed < double, 4, 4> > mras(nin);
  mras[0].set_identity();
  vnl_matrix_fixed < double, 3, 3> rot; rot.set_identity();
  vnl_matrix_fixed < double, 3, 3> roti;
  vnl_vector_fixed < double, 3 > trans;
  vnl_vector_fixed < double, 3 > meant(0.0);
  vnl_matrix_fixed < double, 3, 3> meanr; meanr.set_identity();
  for (int i = 1;i<nin;i++) 
  {
	  int j = index[i];
    cout << "  computing coord of TP "<< j+1 <<" ( "<<mov[j]<<" )" << endl;
		cout << "   wrt to TP "<<tpi+1<<" ( "<<mov[tpi]<<" )" << endl;
    mras[i] = MyMRI::MRIvoxelXformToRasXform (mri_mov[j],mri_mov[tpi],Md[i].first);
		//cout << " mras[" << i << "]: " << endl << mras[i] << endl;

    // split into rotation translation
    MyMatrix::getRTfromM(mras[i],rot,trans);
		//cout << " trans: " << endl << trans << endl;
    //cout << " rot  : " << endl << rot << endl;

    // reverse order (first translate, then rotate)
    // rot stays, trans changes: Rx+t = R (x + R^{-1} t)
    roti  = rot.transpose(); // inverse
    //cout << " roti: " << endl << roti << endl;
    
    trans = roti * trans;
		//cout << " transi: - " << endl << trans << endl;
    
//     if (P.debug) // output transonly
//     {
//       ostringstream oss;
//       oss << outdir << "tp" << j+1 << "_to_tp"<<tpi<<"-transonly";
//       MATRIX *Mtemp = getMfromRT(NULL,trans,NULL);
//       LTA * nlta = RASmatrix2LTA(Mtemp,P.mri_mov[j],P.mri_mov[tpi]);
//       MRI* warped = LTAtransform(P.mri_mov[j],NULL, nlta);
//       MRIwrite(warped, (oss.str()+".mgz").c_str()) ;
//       LTAfree(&nlta);
//       MRIfree(&warped);
//       MatrixFree(&Mtemp);         
//     }
    meant -= trans;
    meanr += roti;
  }
  

  //average
  meant = (1.0/nin) * meant;
	//cout << "meant: "<< endl << meant << endl;
  meanr = (1.0/nin) *meanr;
	//cout << "meanr: " << endl << meanr << endl;
  
  // project meanr back to SO(3) (using polar decomposition)
	assert(rigid);
	
// 	{
//      VECTOR * vz = VectorAlloc(3,MATRIX_REAL);
// 		 MATRIX * meanro = MyMatrix::convertVNL2MATRIX(meanr,NULL);
//      MATRIX * mv = MatrixSVD(meanro,vz,NULL); // setting meanr = U
//      MatrixPrintFmt(stdout,"% 2.8f",meanro); cout << endl;
//      //MatrixPrintFmt(stdout,"% 2.8f",vz); cout << endl;
//      MatrixPrintFmt(stdout,"% 2.8f",mv); cout << endl;
//      mv = MatrixTranspose(mv,mv);
//      //MatrixPrintFmt(stdout,"% 2.8f",mv); cout << endl;
//      meanro = MatrixMultiply(meanro,mv,meanro);
//      cout << " meanr.proj: " << endl;MatrixPrintFmt(stdout,"% 2.8f",meanro); cout << endl;
//      MatrixFree(&mv);
//      MatrixFree(&vz);
//      // Mm is the matrix from tpi ras to mean ras:
//      //MATRIX * Mm = MyMatrix::getMfromRT(meanro,meant,NULL);
//   	 //cout << " mm : " << endl; MatrixPrintFmt(stdout, "% 2.8f",Mm); cout << endl;
// 		 cout << " -----------------------------------------------" << endl;
//   }

  vnl_svd < double > svd_decomp(meanr);
  if ( svd_decomp.valid() )
  {
	    vnl_matrix < double > mu = svd_decomp.U();
			//cout << " mu   : " << mu << endl;
      vnl_matrix < double > mv = svd_decomp.V();
			//cout << " mv   : " << mv << endl;
			mv.inplace_transpose();
			meanr = mu * mv;
			//cout << " meanr: " << meanr << endl;
			//cout << " meant: " << meant << endl;
			//cout << " mm   : " << MyMatrix::getMfromRT(meanr,meant) << endl;
  }
  else
  {
     cerr << "MultiRegistration::initialXforms ERROR: SVD not possible?" << endl;
		 exit(1);
  }
  vnl_matrix_fixed < double, 4, 4 > Mm(MyMatrix::getMfromRT(meanr,meant));

  // construct target geometry for the mean space by centering
	// at the mean of all tp centroids mapped to the mean space
	// (the mean space may be outside any of the input RAS geometries)
	// copy initial geometry from TPI (keep directions/rotation)
	MRI * template_geo = MRIclone(mri_mov[tpi],NULL);
	// map average centroid from TPI vox space to mean RAS space:
// 	MATRIX * tpi_v2r   = MRIgetVoxelToRasXform(mri_mov[tpi]);
// 	VECTOR * ncenter   = VectorAlloc(4, MATRIX_REAL) ;
// 	for (uint ii = 0; ii<3;ii++)
// 	   VECTOR_ELT(ncenter,ii+1) = centroid[ii];
//   VECTOR_ELT(ncenter,4) = 1.0;
// 	// map to RAS:
// 	ncenter = MatrixMultiply(tpi_v2r, ncenter, ncenter);
// 	// map to mean space
// 	ncenter = MatrixMultiply(Mm, ncenter, ncenter);
//   // set new center in geometry
// 	template_geo->c_r = VECTOR_ELT(ncenter,1);
// 	template_geo->c_a = VECTOR_ELT(ncenter,2);
// 	template_geo->c_s = VECTOR_ELT(ncenter,3);
//   template_geo->ras_good_flag = 1;
//   MRIreInitCache(template_geo);
	
	MATRIX * mv2r_temp = MRIgetVoxelToRasXform(mri_mov[tpi]);
	vnl_matrix_fixed < double, 4, 4> tpi_v2r(MyMatrix::convertMATRIX2VNL(mv2r_temp));
	MatrixFree(&mv2r_temp);
	vnl_vector_fixed < double, 4 > ncenter;
	for (uint ii = 0; ii<3;ii++)
	   ncenter[ii] = centroid[ii];
  ncenter[3] = 1.0;
	// map to RAS:
	ncenter = tpi_v2r * ncenter;
	// map to mean space
	ncenter = Mm * ncenter;
  // set new center in geometry
	template_geo->c_r = ncenter[0];
	template_geo->c_a = ncenter[1];
	template_geo->c_s = ncenter[2];
  template_geo->ras_good_flag = 1;
  MRIreInitCache(template_geo);
	
	
  // construct maps from each image to the mean space
  //MATRIX * M = NULL;
  vnl_matrix_fixed < double, 4, 4>  M;
  //cout << " Mm: " << endl; MatrixPrintFmt(stdout,"% 2.8f",Mm); cout << endl;
  for (int i = 0;i<nin;i++) 
  {
	  int j = index[i];
    cout << "  computing mean coord of TP "<< j+1 <<" ( "<<mov[j]<<" ) " << endl;
    // concat transforms: meant meanr mras
    // from right to left: mras[j] (aligns to T1)
    //                     then do the mean rot and move to mean location

    //MatrixPrintFmt(stdout,"% 2.8f",mras[i]); cout << endl;
    //M = MatrixMultiply(Mm,mras[i],M);
    M = Mm * mras[i];
    //MatrixPrintFmt(stdout,"% 2.8f",M); cout << endl;
    
    // make lta from M (M is RAS to RAS)
    assert(ltas.size() == mri_mov.size());
    assert(ltas[j] == NULL);
    ltas[j] = MyMatrix::RASmatrix2LTA(M,mri_mov[j],template_geo); // use geometry of template_geo
			
    if (debug)
    {
      ostringstream oss;
      oss << outdir << "tp" << j+1 << "_to_mcoord";
      LTAwriteEx(ltas[j], (oss.str()+".lta").c_str()) ;
      MRI * warped = LTAtransform(mri_mov[j],NULL, ltas[j]);
      if (iscale)
      {
        string fn = oss.str() + "-intensity.txt";
        ofstream f(fn.c_str(),ios::out);
        f << Md[i].second;
        f.close();
	
        warped = MyMRI::MRIvalscale(warped,warped, Md[i].second);
      }
      MRIwrite(warped, (oss.str()+".mgz").c_str()) ;
      MRIfree(&warped);     
    }
    
    // cleanup
    //MatrixFree(&mras[i]);
    //MatrixFree(&Md[i].first);
  }
  //cleanup
	MRIfree(&template_geo);
  //MatrixFree(&M);
  //MatrixFree(&Mm);
  return true;
}

bool MultiRegistration::writeMean(const std::string& mean)
{
	    if (! mri_mean ) 
			{
			   cout << " ERROR: No average exists! Skipping output." << endl;
				 return false;
			}
  strncpy(mri_mean->fname, mean.c_str(),STRLEN);
  return (MRIwrite(mri_mean,mean.c_str()) == 0);
}

bool MultiRegistration::writeConformMean(const std::string& mean)
{
  if (! mri_mean ) 
	{
		cout << " ERROR: No average exists! Skipping output." << endl;
		return false;
	}
			
  // create conform mean
	MRI * mri_cmean = averageConformSet(0);
			
  strncpy(mri_cmean->fname, mean.c_str(),STRLEN);
	int ok = MRIwrite(mri_cmean,mean.c_str());
  return (ok == 0);
}

bool MultiRegistration::writeLTAs(const std::vector < std::string > & nltas, bool vox2vox,const string & mean)
{
   assert (nltas.size() == ltas.size());
	 int error = 0;
   for (unsigned int i = 0;i<nltas.size();i++)
	 {
	    if (! ltas[i] ) 
			{
			   cout << " ERROR: No ltas exist! Skipping output." << endl;
				 return false;
			}

      if (vox2vox)
      {
        error += (LTAchangeType(ltas[i], LINEAR_VOX_TO_VOX)==NULL);
      }
      else assert(ltas[i]->type == LINEAR_RAS_TO_RAS);
      strncpy(ltas[i]->xforms[0].dst.fname, mean.c_str(),STRLEN);
      strncpy(ltas[i]->xforms[0].src.fname, mov[i].c_str(),STRLEN);
      LTAwriteEx(ltas[i], nltas[i].c_str()) ;	 
	 }
	 return (error == 0);
}

bool MultiRegistration::writeWarps(const std::vector <  std::string >& nwarps)
{
   assert (nwarps.size() == mri_warps.size());
	 int error = 0;
   for (unsigned int i = 0;i<nwarps.size();i++)
	 {
	    if (! mri_warps[i] ) 
			{
			   cout << " ERROR: No warps exist! Skipping output." << endl;
				 return false;
			}
		  error += MRIwrite(mri_warps[i],nwarps[i].c_str()) ;
	 }
	 return (error == 0);
}

bool MultiRegistration::writeIntensities(const std::vector < std::string >& nintens)
{
   assert (nintens.size() == intensities.size());
	 int error = 0;
   for (unsigned int i = 0;i<nintens.size();i++)
	 {
	     assert (intensities[i] > 0);
          ofstream f(nintens[i].c_str(),ios::out);
					if (f.good())
					{
            f << intensities[i];
            f.close();
					} else error++;
	 }
	 return (error == 0);
}

bool MultiRegistration::writeWeights(const std::vector < std::string >& nweights, bool oneminusweights)
{
   assert (nweights.size() == mri_weights.size());
	 int error = 0;
   for (unsigned int i = 0;i<nweights.size();i++)
	 {
	    if (! mri_weights[i])
			{
			   cout << " No weights constructed, skipping output of weights" << endl;
				 return false;
			}
			MRI * wtmp = mri_weights[i];
			if (oneminusweights) wtmp = MRIlinearScale(wtmp,NULL,-1,1,0);
		  error += MRIwrite(wtmp, nweights[i].c_str()) ;
			if (oneminusweights) MRIfree(&wtmp);
	 }
	 return (error == 0);
}


/*!
  \fn MRI* averageSet(const vector < MRI * >& set, MRI* mean, int method, double sat)
  \brief Averages the movable volumes depending on method
  \param set vector of movable volumes
  \param mean  output of mean volume
  \param method  0 = mean, 1 = median, 2 = tukey biweight (testing)
  \param sat     saturation for tukey biweight
*/
MRI* MultiRegistration::averageSet(const vector < MRI * >& set,
                       MRI* mean, int method, double sat)
{
  assert(set.size() > 1);

//    for(uint i = 0;i<set.size();i++)
//    {
//       cout << " TP " << i+1 << endl;
//       cout << " mean   : " << CostFunctions::mean(set[i])   << endl;
//       cout << " sdev   : " << CostFunctions::sdev(set[i])   << endl;
//       cout << " median : " << CostFunctions::median(set[i]) << endl;
//       cout << " mad    : " << CostFunctions::mad(set[i])    << endl;
//     }

  if (method == 0)
  {
    // mean
    cout << "    using mean" << endl;
    for (unsigned int i = 0;i<set.size();i++)
    {
      mean = MRIaverage(set[i],i,mean);
    }
  }
  else if (method ==1)
  {
    cout << " using median " << endl;
    // robust
    int x,y,z,i;
    assert(set.size() > 0);
    float dd[set.size()];
    if (!mean) mean = MRIclone(set[0],NULL);
    //MRI * midx = MRIalloc(set[0]->width,set[0]->height,set[0]->depth, MRI_FLOAT);
    //MRIcopyHeader(mean,midx);
    //midx->type = MRI_FLOAT;
    pair < float, float > mm;
    for (z = 0 ; z < set[0]->depth ; z++)
      for (y = 0 ; y < set[0]->height ; y++)
        for (x = 0 ; x < set[0]->width ; x++)
        {
          for (i=0; i<(int) set.size();i++)
            dd[i] = MRIgetVoxVal(set[i],x,y,z,0);
          mm = RobustGaussian<float>::medianI(dd,(int)set.size());
          MRIsetVoxVal(mean,x,y,z,0,mm.first);
	  //MRIsetVoxVal(midx,x,y,z,0,mm.second);
        }
    //MRIwrite(midx,"midx.mgz");
    //MRIwrite(mean,"m.mgz");
    //assert(1==2);
  }
  else if (method ==2)
  {
    cout << "    using tukey biweight" << endl;
    // robust tukey biweight
    int x,y,z,i;
//     MATRIX* b = MatrixAlloc(set.size(),1,MATRIX_REAL);
//     pair < MATRIX* , MATRIX* >  muw;
    vnl_vector < float > b(set.size());
    assert(set.size() > 0);
    if (!mean) mean = MRIclone(set[0],NULL);
    for (z = 0 ; z < set[0]->depth ; z++)
      for (y = 0 ; y < set[0]->height ; y++)
        for (x = 0 ; x < set[0]->width ; x++)
        {
          // cout << " x: " << x << " y: " << y << " z: " <<z << "  size: " << set.size() <<endl;
          for (i=0; i<(int) set.size();i++)
          {
            //cout << "i: " << i << endl;
            //b->rptr[i+1][1] = MRIgetVoxVal(set[i],x,y,z,0);
            b[i] = MRIgetVoxVal(set[i],x,y,z,0);
          }
          //cout << endl << "intgensities at this voxel:" << endl; ;
          //MatrixPrintFmt(stdout,"% 2.8f",b);

          Regression<float> R(b);
					vnl_vector < float > p = R.getRobustEst(sat);
          //muw = R.getRobustEstW(sat);
          //    cout << " tukey mean: " << muw.first->rptr[1][1] << endl;
          //MRIsetVoxVal(mean,x,y,z,0,muw.first->rptr[1][1]);
          MRIsetVoxVal(mean,x,y,z,0,p[0]);
          //MatrixFree(&muw.first);
          //MatrixFree(&muw.second);
        }
  }
  else if (method ==3) // needs more development (sigma..)
  {
    cout << "    using tukey biweight (alltoghether)" << endl;
    // robust tukey biweight
    int x,y,z,i;
    int n = set[0]->depth * set[0]->height *  set[0]->width ;
//     MATRIX* b = MatrixAlloc(set.size()*n,1,MATRIX_REAL);
//     MATRIX* A = MatrixAlloc(set.size()*n,n,MATRIX_REAL);
//     A = MatrixZero(A->rows,A->cols,A);
//     pair < MATRIX* , MATRIX* >  muw;
    vnl_matrix < float > A(set.size()*n,n);
		vnl_vector < float > b(set.size()*n);
		A.fill(0.0);
		
    assert(set.size() > 0);
    if (!mean) mean = MRIclone(set[0],NULL);
    for (i=0; i<(int) set.size();i++)
    {
      assert(set[i]->width == set[0]->width);
      assert(set[i]->height == set[0]->height);
      assert(set[i]->depth == set[0]->depth);
      int pcount = 0;
      for (z = 0 ; z < set[0]->depth ; z++)
        for (y = 0 ; y < set[0]->height ; y++)
          for (x = 0 ; x < set[0]->width ; x++)
          {
            // cout << " x: " << x << " y: " << y << " z: " <<z << "  size: " << set.size() <<endl;
            //cout << "i: " << i << endl;
//             b->rptr[pcount*set.size()+i+1][1] =
//               MRIgetVoxVal(set[i],x,y,z,0);
//             A->rptr[pcount*set.size()+i+1][pcount+1] = 1;
               b[pcount*set.size()+i] = MRIgetVoxVal(set[i],x,y,z,0);
							 A[pcount*set.size()+i][pcount] = 1.0;
            pcount++;
          }
    }
    //cout << endl << "intgensities at this voxel:" << endl; ;
    //MatrixPrintFmt(stdout,"% 2.8f",b);

    Regression<float> R(A,b);
    //muw = R.getRobustEstW(sat);
		vnl_vector < float > p = R.getRobustEst(sat);
    //    cout << " tukey mean: " << muw.first->rptr[1][1] << endl;
    int pcount = 0;
    for (z = 0 ; z < set[0]->depth ; z++)
      for (y = 0 ; y < set[0]->height ; y++)
        for (x = 0 ; x < set[0]->width ; x++)
        {
          //pcount++;
          //MRIsetVoxVal(mean,x,y,z,0,muw.first->rptr[pcount][1]);
          MRIsetVoxVal(mean,x,y,z,0,p[pcount]);
          pcount++;
        }
    //MatrixFree(&muw.first);
    //MatrixFree(&muw.second);

  }
  else
  {

    cerr <<  " averageSet  method " << method << " unknown" << endl;
    exit(1);
  }
//       cout << " Average Vol "  << endl;
//       cout << " mean   : " << CostFunctions::mean(mean) << endl;
//       cout << " sdev   : " << CostFunctions::sdev(mean) << endl;
//       cout << " median : " << CostFunctions::median(mean) << endl;
//       cout << " mad    : " << CostFunctions::mad(mean)<< endl;

  return mean;
}

/*!
  \fn MRI* initialAverageSet(const vector < MRI * >& set, MRI* mean, int method, double sat)
  \brief Averages the movable volumes depending on method
  \param set vector of movable volumes
  \param mean  output of mean volume
  \param method  0 = mean, 1 = median, 2 = tukey biweight (testing)
  \param sat     saturation for tukey biweight
*/
MRI* MultiRegistration::initialAverageSet(const vector < MRI * >& set,
                              MRI* mean, int method, double sat)
{
// the initial average can be any place as long as it is
// not identical to one of the
// initial images (to avoid introducing a bias).
// it can be the average of all images but that is extremely blurry, and if
// we have only few images, far apart, maybe the algorithm will not converge
// or converge very slowly (has not happened yet).
// therefore we find an initial coarse alignment by using moments

  assert(set.size() > 1);
  int n = (int) set.size();
  vector < double >  centroid(3,0);
  vector < vector < double > >centroids(n);
  for (int i = 0;i<n;i++)
  {
    centroids[i] = CostFunctions::centroid(set[i]);
    centroid[0] += centroids[i][0];
    centroid[1] += centroids[i][1];
    centroid[2] += centroids[i][2];
  }
  centroid[0] /= n;
  centroid[1] /= n;
  centroid[2] /= n;

  // map set to mean of centroids
  MATRIX* Mtrans = MatrixIdentity(4,NULL);
  vector < MRI* > newset(n);
  for (int i = 0;i<n;i++)
  {
    Mtrans->rptr[1][4] = centroid[0]-centroids[i][0];
    Mtrans->rptr[2][4] = centroid[1]-centroids[i][1];
    Mtrans->rptr[3][4] = centroid[2]-centroids[i][2];
    newset[i] = MRIlinearTransform(set[i],NULL,Mtrans);
  }

  return averageSet(newset,mean,method,sat);
}
