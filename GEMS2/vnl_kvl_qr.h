// This is core/vnl/algo/vnl_qr.h
#ifndef vnl_qr_h_
#define vnl_qr_h_
#ifdef VCL_NEEDS_PRAGMA_INTERFACE
#pragma interface
#endif


//
// KVL: what I need is the "economy size" QR decomposition, i.e. for a mxn matrix A:
//  - if m>n, only the first n columns of Q and the first n rows of R are computed.
//  - if m<=n, it is just the usual case
//
// Of course I can in theory do this by just selecting the relevant columns from the
// current VNL implementation, doing Q().get_n_columns( 0, n ), but that's just not
// memory efficient and fast enough for m >> n
//
// Unfortunately VNL was obviously not implemented with subclasses in mind: the
// relevant data members are private.
//
// Therefore I'm just copying the code here with some minimal adjustments
//



//:
// \file
// \brief Calculate inverse of a matrix using QR
// \author  Andrew W. Fitzgibbon, Oxford RRG
// \date   08 Dec 96
//
// \verbatim
//  Modifications
//   081296 AWF Temporarily abandoned as I realized my problem was symmetric...
//   080697 AWF Recovered, implemented solve().
//   200897 AWF Added determinant().
//   071097 AWF Added Q(), R().
//   Christian Stoecklin, ETH Zurich, added QtB(v)
//   31-mar-2000 fsm: templated
//   28/03/2001 - dac (Manchester) - tidied up documentation
//   13 Jan.2003 - Peter Vanroose - added missing implementation for inverse(),
//                                tinverse(), solve(matrix), extract_q_and_r().
// \endverbatim

#include <vnl/vnl_vector.h>
#include <vnl/vnl_matrix.h>
#include <vcl_iosfwd.h>

//: Extract the Q*R decomposition of matrix M.
//  The decomposition is stored in a compact and time-efficient
// packed form, which is most easily used via the "solve" and
// "determinant" methods.

template <class T>
class vnl_qr
{
 public:
  vnl_qr(vnl_matrix<T> const & M);
 ~vnl_qr();

  //: return the inverse matrix of M
  vnl_matrix<T> inverse () const;
  //: return the transpose of the inverse matrix of M
  vnl_matrix<T> tinverse () const;
  //: return the original matrix M
  vnl_matrix<T> recompose () const;

  //: Solve equation M x = rhs for x using the computed decomposition.
  vnl_matrix<T> solve (const vnl_matrix<T>& rhs) const;
  //: Solve equation M x = rhs for x using the computed decomposition.
  vnl_vector<T> solve (const vnl_vector<T>& rhs) const;

  //: Return the determinant of M.  This is computed from M = Q R as follows:
  // |M| = |Q| |R|.
  // |R| is the product of the diagonal elements.
  // |Q| is (-1)^n as it is a product of Householder reflections.
  // So det = -prod(-r_ii).
  T determinant() const;
  //: Unpack and return unitary part Q.
  vnl_matrix<T> const& Q( bool useEconomySize = false ) const;
  //: Unpack and return R.
  vnl_matrix<T> const& R( bool useEconomySize = false ) const;
  //: Return residual vector d of M x = b -> d = Q'b
  vnl_vector<T> QtB(const vnl_vector<T>& b) const;

  void extract_q_and_r(vnl_matrix<T>* q, vnl_matrix<T>* r) const { *q = Q(); *r = R(); }

 private:
  vnl_matrix<T> qrdc_out_;
  vnl_vector<T> qraux_;
  vnl_vector<long> jpvt_;
  vnl_matrix<T>* Q_;
  vnl_matrix<T>* R_;

  // Disallow assignment.
  vnl_qr(const vnl_qr<T> &) { }
  void operator=(const vnl_qr<T> &) { }
};

//: Compute determinant of matrix "M" using QR.
template <class T>
inline T vnl_qr_determinant(vnl_matrix<T> const& m)
{
  return vnl_qr<T>(m).determinant();
}

template <class T>
vcl_ostream& operator<<(vcl_ostream&, vnl_qr<T> const & qr);


#include "vnl_kvl_qr.txx"

#endif // vnl_qr_h_
