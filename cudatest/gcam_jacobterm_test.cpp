#include <cstdlib>
#include <cstdio>

#include <iostream>
#include <iomanip>
#include <string>
using namespace std;

#include <boost/program_options.hpp>
namespace bpo = boost::program_options;

#include "gcamorph.h"
#include "gcamorphtestutils.hpp"

#include "chronometer.hpp"


#ifdef FS_CUDA
#include "devicemanagement.h"
#endif



// ==========================================================

const string inFileDefault = "gcamJacobTerminput";
#ifdef FS_CUDA
const string outFileDefault = "gcamJacobTermoutputGPU";
#else
const string outFileDefault = "gcamJacobTermoutputCPU";
#endif

const float lambdaDefault = 1;

string inFilename;
string outFilename;
float lambda;

const char* Progname = "gcam_jacobterm_test";




// ==========================================================

void ReadCommandLine( int ac, char* av[] ) {

  try {
    bpo::options_description desc("Allowed options");
    desc.add_options()
      ("help", "Produce help message" )
      ("input", bpo::value<string>(&inFilename)->default_value(inFileDefault), "Input filename (.nc will be appended)" )
      ("output", bpo::value<string>(&outFilename)->default_value(outFileDefault), "Output filename (.nc will be appended)" )
      ("lambda", bpo::value<float>(&lambda)->default_value(lambdaDefault), "Value of l_jacob" )
      ;

    
    bpo::variables_map vm;
    bpo::store( bpo::parse_command_line( ac, av, desc ), vm );
    bpo::notify( vm );
    
    if( vm.count( "help" ) ) {
      cout << desc << endl;
      exit( EXIT_SUCCESS );
    }
  }
  catch( exception& e ) {
    cerr << "Error: " << e.what() << endl;
    exit( EXIT_FAILURE );
  }
  catch( ... ) {
    cerr << "Unknown exception" << endl;
    exit( EXIT_FAILURE );
  }
}



// ==========================================================

int main( int argc, char *argv[] ) {

  SciGPU::Utilities::Chronometer tTotal;
  GCAMorphUtils myUtils;

  cout << "GCAM Jacobian Term Tester" << endl;
  cout << "=========================" << endl << endl;


#ifdef FS_CUDA
#ifndef GCAMORPH_ON_GPU
  cerr << "GCAMORPH_ON_GPU is not defined." << endl;
  cerr << "Test meaningless" << endl;
  exit( EXIT_FAILURE );
#endif
#endif

#ifdef FS_CUDA
  AcquireCUDADevice();
#else
  cout << "CPU Version" << endl;
#endif
  cout << endl;

  ReadCommandLine( argc, argv );

  // ============================================

  // Read the input file
  GCAM* gcam = NULL;

  myUtils.Read( &gcam, inFilename );

  // Stop subsequent calls complaining
  gcam->ninputs = 1;

  // ============================================
  // Perform the calculation
  tTotal.Start();
  gcamJacobianTerm( gcam, NULL, lambda, 0 );
  tTotal.Stop();

  cout << "Computation took " << tTotal << endl;

  // =============================================
  // Write the output
  myUtils.Write( gcam, outFilename );

#ifdef FS_CUDA
  PrintGPUtimers();
#endif

  // ====================================
  // Release
  GCAMfree( &gcam );
  
  exit( EXIT_SUCCESS );
}
