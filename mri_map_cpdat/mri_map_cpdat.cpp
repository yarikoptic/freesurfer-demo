/**
 * @file  mri_map_cpdat.c
 * @brief map a control.dat file (manual edits) to another space
 *
 */
/*
 * Original Author: Martin Reuter
 * CVS Revision Info:
 *    $Author: mreuter $
 *    $Date: 2010/11/11 21:14:51 $
 *    $Revision: 1.2.2.2 $
 *
 * Copyright (C) 2010-2011,
 * The General Hospital Corporation (Boston, MA). 
 * All rights reserved.
 *
 * Distribution, usage and copying of this software is covered under the
 * terms found in the License Agreement file named 'COPYING' found in the
 * FreeSurfer source code root directory, and duplicated here:
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferOpenSourceLicense
 *
 * General inquiries: freesurfer@nmr.mgh.harvard.edu
 *
 */


//#include <stdio.h>
//#include <stdlib.h>
//#include <math.h>
//#include <ctype.h>
#include <string>
#include <iostream>

#ifdef __cplusplus
extern "C"
{
#endif
#include "error.h"
#include "macros.h"
//#include "mri.h"
//#include "matrix.h"
//#include "timer.h"
//#include "diag.h"
//#include "mrimorph.h"
#include "version.h"
#include "transform.h"
#include "ctrpoints.h"

#ifdef __cplusplus
}
#endif

using namespace std;

struct Parameters
{
  string cpin;
  string cpout;
  string lta;
};
static struct Parameters P =
  { "","",""};

static int get_option(int argc, char *argv[], Parameters & P) ;
static void  usage_exit(int code) ;

static char vcid[] = "$Id: mri_map_cpdat.cpp,v 1.2.2.2 2010/11/11 21:14:51 mreuter Exp $";
char *Progname = NULL;

int main(int argc, char *argv[])
{

  int    nargs;


  // Default initialization
  nargs = handle_version_option(argc, argv,vcid,"$Name:  $");
  if (nargs && argc - nargs == 1) exit (0);
  argc -= nargs;
  Progname = argv[0] ;
	argc--;
  argv++;
  ErrorInit(NULL, NULL, NULL) ;
  //DiagInit(NULL, NULL, NULL) ;
	
	
  for ( ; argc > 1 && ISOPTION(*argv[0]) ; argc--, argv++)
	{
    nargs = get_option(argc, argv,P) ;
    argc -= nargs ;
    argv += nargs ;
  }

  if (P.cpin == "" || P.cpout== "" || P.lta == "")
    usage_exit(0) ;

  // Timer
  //struct timeb start ;
  //int    msec,minutes,seconds;
  //TimerStart(&start) ;

  int count = 0;
  int useRealRAS = 0;
	
	// read ctrl points
  MPoint *pArray = MRIreadControlPoints(P.cpin.c_str(), &count, &useRealRAS);
  // read lta
	cout << "Reading LTA" << endl;
	LTA* lta = LTAread(P.lta.c_str());
	// map ctrl points
	cout << "Mapping control points..." << endl;
	MPoint *mappedArray = MRImapControlPoints(pArray,count,useRealRAS,NULL,lta);
	// write ctrl points
	cout << "Writing control points..." << endl;
	MRIwriteControlPoints(mappedArray, count, useRealRAS, P.cpout.c_str());

  //cleanup
	free(pArray);
	free(mappedArray);

  return(0) ;

}

static int
get_option(int argc, char *argv[], Parameters & P)
{
  int  nargs = 0 ;
  char *option ;

  option = argv[0] + 1 ;            /* past '-' */
  if (option[0] == '-') option = option +1;  // remove second '-'
  StrUpper(option) ;
	
//  if (!stricmp(option, "-help")||!stricmp(option, "-usage")) {
//    usage_exit(0);
//  } else 
	
	if (!strcmp(option, "IN"))
	{
    P.cpin = string(argv[1]);
    nargs = 1;
    cout << "--in:  Using "<< P.cpin << " as input file." << endl;
  }
	else if (!strcmp(option, "OUT"))
	{
    P.cpout = string(argv[1]);
    nargs = 1;
    cout << "--out: Using "<< P.cpout << " as output file." << endl;
  }
	else if (!strcmp(option, "LTA"))
	{
    P.lta = string(argv[1]);
    nargs = 1;
    cout << "--lta: Using "<< P.lta << " as transform." << endl;
  }
	else
	{
    cerr << endl << endl << "ERROR: Option: " << argv[0] << " unknown !! " << endl << endl;
    exit(1);
  }

  return(nargs) ;
}

static void
usage_exit(int code)
{
//  outputHelp(Progname);

//#ifdef GREGT
  printf("\n%s <Required Arguments>\n\n", Progname) ;
	printf("  Maps a control.dat file to a different space using an LTA\n\n");
  printf("  -in  <file>      input  control point txt file\n");
  printf("  -out <file>      output control point txt file\n");
  printf("  -lta <file>      lta transform file to be applied\n");
  printf("  \n");
//#endif
  exit(code);
}
