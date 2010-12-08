/*
 * Original Author: Dan Ginsburg (@ Children's Hospital Boston)
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2010/12/08 17:26:31 $
 *    $Revision: 1.1.2.2 $
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

/// \brief Brief description
/// Reduce the number of vertices and faces in a surface. 
///
/// \b NAME
///
/// mris_decimate 
///
/// \b SYNPOSIS
///
/// mris_decimate [options] <input surface> <output surface>
///
/// \b DESCRIPTION
///
///  This tool reduces the number of triangles in a surface using the
///  the GNU Triangulated Surface Library documented at:
///
///           http://gts.sourceforge.net/reference/book1.html
///
///  Please see the GTS documentation for details on the decimation algorithm.
///  mris_decimate will read in an existing surface and write out a new one
///  with less triangles.  The decimation level and other options can be provided
///  on the command-line.
///

// $Id: main.cpp,v 1.1.2.2 2010/12/08 17:26:31 nicks Exp $


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sstream>
#include <iostream>

#include "mris_decimate.h"

extern "C"
{
#include "macros.h"
#include "utils.h"
#include "fio.h"
#include "version.h"
#include "cmdargs.h"
#include "error.h"
#include "diag.h"
}



///
//  Function Prototypes
//
static int  get_option(int argc, char **argv);
static void print_usage(void) ;
static void usage_exit(void);
static void print_help(void) ;
static void print_version(void) ;
static void dump_options(FILE *fp);
int main(int argc, char *argv[]) ;

///
//  Global Variables
//
static char vcid[] = "$Id: main.cpp,v 1.1.2.2 2010/12/08 17:26:31 nicks Exp $";
char *Progname = NULL;
char *cmdline;
int debug=0;
int checkoptsonly=0;
struct utsname uts;
DECIMATION_OPTIONS gDecimationOptions;

///////////////////////////////////////////////////////////////////////////////////
//  
//  Public Functions
//
//

///////////////////////////////////////////////////////////////////////////////////
//  
//  Private Functions
//
//

///
/// Callback to print out status messages during decimation
///
void DecimateProgressCallback(float percent, const char *msg, void *userData)
{
    std::cout << std::string(msg) << std::endl;
}


///
/// \fn Main entrypoint for mris_decimate
/// \return 0 on succesful run, 1 on error
///
int main(int argc, char *argv[]) 
{
    // Initialize Decimation options
    memset(&gDecimationOptions, 0, sizeof(DECIMATION_OPTIONS));
    gDecimationOptions.decimationLevel = 0.5; // Default decimation level if not specified

    char **av;
    char *in_fname, out_fpath[STRLEN] ;
    int ac, nargs;
    MRI_SURFACE *mris ;

    nargs = handle_version_option (argc, argv, vcid, "$Name:  $");
    if (nargs && argc - nargs == 1) 
        exit (0);
    Progname = argv[0] ;
    argc -= nargs;
    cmdline = argv2cmdline(argc,argv);
    uname(&uts);

    if (argc < 3)
        usage_exit();

    ErrorInit(NULL, NULL, NULL) ;
    DiagInit(NULL, NULL, NULL) ;
    
    ac = argc ;
    av = argv ;
    for ( ; argc > 1 && ISOPTION(*argv[1]) ; argc--, argv++) 
    {
        nargs = get_option(argc, argv) ;
        argc -= nargs ;
        argv += nargs ;
    }

    if (argc < 3)
        usage_exit();

    in_fname = argv[1] ;
    FileNameAbsolute(argv[2], out_fpath);

    dump_options(stdout);

    mris = MRISread(in_fname) ;
    if (!mris)
    {
        ErrorExit(ERROR_NOFILE, "%s: could not read surface file %s",
                  Progname, in_fname) ;
    }

	std::cout << "Original Surface Number of vertices: " << mris->nvertices << std::endl;
	std::cout << "Original Surface Number of faces: " << mris->nfaces << std::endl;

    // Decimate the surface
    decimateSurface(&mris, gDecimationOptions, DecimateProgressCallback);

    // Write out the results
    MRISwrite(mris, out_fpath);
    MRISfree(&mris);

    return 0;
}

///
/// \fn int get_option(int argc, char **argv)
/// \brief Parses a command-line argument
/// \param argc - number of command line arguments
/// \param argv - pointer to a character pointer
///
static int get_option(int argc, char *argv[]) 
{
    int  nargs = 0 ;
    char *option ;

    option = argv[1] + 1 ;            /* past '-' */
    if (!stricmp(option, "-help"))
    {
        print_help() ;
    }
    else if (!stricmp(option, "-version"))
    {
        print_version() ;
    } 
    else switch (toupper(*option)) 
    {
    case 'D':
        gDecimationOptions.decimationLevel = atof(argv[2]) ;
        printf("using decimation = %2.2f\n", gDecimationOptions.decimationLevel) ;
        nargs = 1 ;
        break ;
    case 'M':
        gDecimationOptions.setMinimumAngle = true;
        gDecimationOptions.minimumAngle = atof(argv[2]);
        printf("using minimumAngle = %f\n", gDecimationOptions.minimumAngle) ;
        nargs = 1;
        break;
    default:
        fprintf(stderr, "unknown option %s\n", argv[1]) ;
        exit(1) ;
        break ;
    }

  return(nargs) ;
}


///
/// \fn static void usage_exit(void)
/// \brief Prints usage and exits
///
static void usage_exit(void) 
{
    print_usage() ;
    exit(1) ;
}

///
/// \fn static void print_usage(void)
/// \brief Prints usage and returns (does not exit)
///
static void print_usage(void) 
{
    printf("USAGE: %s [options] <input surface> <output surface>\n",Progname) ;
    printf("\n");
    printf("This program reduces the number of triangles in a surface and\n");
    printf("outputs the new surface to a file using the GNU Triangulated Surface (GTS)\n");
    printf("Library.\n");
    printf("\nValid options are:\n\n");
    printf("   -d decimationLevel\n");
    printf("         target decimation level of new surface (value between 0<-->1.0, default: 0.5).\n");
    printf("         The resulting surface will have approximately triangles = <decimationLevel> * origTriangles\n");
    printf("   -m minimumAngle\n");
    printf("         The minimum angle in degrees allowed between faces during decimation (default: 1.0).\n");
    printf("\n");
    printf("\n");
    printf("   --help      print out information on how to use this program\n");
    printf("   --version   print out version and exit\n");
    printf("\n");
    printf("\n");
}

///
/// \fn static void print_help(void)
/// \brief Prints help and exits
///
static void print_help(void) 
{
    print_usage() ;
    exit(1) ;
}

///
/// \fn static void print_version(void)
/// \brief Prints version and exits
///
static void print_version(void) 
{
    printf("%s\n", vcid) ;
    exit(1) ;
}


///
/// \fn static void dump_options(FILE *fp)
/// \brief Prints command-line options to the given file pointer
/// \param FILE *fp - file pointer
///
static void dump_options(FILE *fp) 
{    
    fprintf(fp,"\n");
    fprintf(fp,"%s\n",vcid);
    fprintf(fp,"cmdline %s\n",cmdline);
    fprintf(fp,"sysname  %s\n",uts.sysname);
    fprintf(fp,"hostname %s\n",uts.nodename);
    fprintf(fp,"machine  %s\n",uts.machine);
    fprintf(fp,"user     %s\n",VERuser());

    fprintf(fp,"\ndecimationLevel            %f\n", gDecimationOptions.decimationLevel);
    if (gDecimationOptions.setMinimumAngle)
        fprintf(fp,"minimumAngle               %f\n", gDecimationOptions.minimumAngle) ;
    else
        fprintf(fp,"minimumAngle               %f\n", 1.0f) ;
    
    return;
}
