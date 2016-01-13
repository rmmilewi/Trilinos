/*
// @HEADER
// ***********************************************************************
//
//          Tpetra: Templated Linear Algebra Services Package
//                 Copyright (2008) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
//
// ************************************************************************
// @HEADER
*/

#include "Tpetra_MultiVector.hpp"
#include "Teuchos_UnitTestHarness.hpp"
#include "TpetraCore_ETIHelperMacros.h"
#include <cstdlib> // atexit

namespace { // (anonymous)

  // atexit() callback that finalizes the given Kokkos execution
  // space, if it is currently initialized.
  template<class ExecSpace>
  void finalizeExecSpace () {
    if (ExecSpace::is_initialized ()) {
      ExecSpace::finalize ();
    }
  }

  // Take care of Kokkos execution space initialization automatically.
  // This ensures that each execution space gets initialized and
  // finalized at most once, in that order, over all the tests in this
  // file.  Kokkos requires this, just like MPI does for MPI_Init and
  // MPI_Finalize.
  template<class ExecSpace>
  struct InitExecSpace {
    InitExecSpace () {
      if (! ExecSpace::is_initialized ()) {
        ExecSpace::initialize ();
      }
      // How should we respond if atexit() fails to register our hook?
      // That means that the Kokkos execution space won't get
      // finalized at the end of the program.  Kokkos already prints
      // out a message in that case.  Thus, we don't have to do
      // anything here.  It's not Kokkos' fault if atexit() ran out of
      // space for all the hooks.
      if (! registeredExitHook_) {
        (void) atexit (finalizeExecSpace<ExecSpace>);
        registeredExitHook_ = true;
      }
    }

    bool isInitialized () {
      return ExecSpace::is_initialized ();
    }

    static bool registeredExitHook_;
  };

  // mfh 13 Jan 2016: The test uses the Tpetra ETI macros for
  // instantiation, so it should use Tpetra's macros, not Kokkos'
  // macros, for declaring and defining these helper classes.
  // Otherwise, build warnings (e.g., "unused variable") may result.

#ifdef HAVE_TPETRA_INST_CUDA
#  ifndef KOKKOS_HAVE_CUDA
#    error "HAVE_TPETRA_INST_CUDA is defined, but KOKKOS_HAVE_CUDA is not.  In order for Kokkos::Cuda to be enabled in Tpetra, it must be enabled in Kokkos first."
#  endif // KOKKOS_HAVE_CUDA

  template<>
  struct InitExecSpace<Kokkos::Cuda> {
    typedef Kokkos::Cuda ExecSpace;

    InitExecSpace () {
      // Make sure that HostSpace's execution space is initialized
      // first.  Otherwise, Cuda::initialize() throws an exception.
      InitExecSpace<Kokkos::HostSpace::execution_space> init2;

      if (! ExecSpace::is_initialized ()) {
        ExecSpace::initialize ();
      }
      // How should we respond if atexit() fails to register our hook?
      // That means that the Kokkos execution space won't get
      // finalized at the end of the program.  Kokkos already prints
      // out a message in that case.  Thus, we don't have to do
      // anything here.  It's not Kokkos' fault if atexit() ran out of
      // space for all the hooks.
      if (! registeredExitHook_) {
        (void) atexit (finalizeExecSpace<ExecSpace>);
        registeredExitHook_ = true;
      }
    }

    bool isInitialized () {
      return ExecSpace::is_initialized ();
    }

    static bool registeredExitHook_;
  };
#endif // HAVE_TPETRA_INST_CUDA

#ifdef HAVE_TPETRA_INST_SERIAL
#  ifndef KOKKOS_HAVE_SERIAL
#    error "HAVE_TPETRA_INST_SERIAL is defined, but KOKKOS_HAVE_SERIAL is not.  In order for Kokkos::Serial to be enabled in Tpetra, it must be enabled in Kokkos first."
#  endif // KOKKOS_HAVE_SERIAL

  template<> bool InitExecSpace<Kokkos::Serial>::registeredExitHook_ = false;
#endif // HAVE_TPETRA_INST_SERIAL

#ifdef HAVE_TPETRA_INST_OPENMP
#  ifndef KOKKOS_HAVE_OPENMP
#    error "HAVE_TPETRA_INST_OPENMP is defined, but KOKKOS_HAVE_OPENMP is not.  In order for Kokkos::OpenMP to be enabled in Tpetra, it must be enabled in Kokkos first."
#  endif // KOKKOS_HAVE_OPENMP

  template<> bool InitExecSpace<Kokkos::OpenMP>::registeredExitHook_ = false;
#endif // HAVE_TPETRA_INST_OPENMP

#ifdef HAVE_TPETRA_INST_PTHREAD
#  ifndef KOKKOS_HAVE_PTHREAD
#    error "HAVE_TPETRA_INST_PTHREAD is defined, but KOKKOS_HAVE_PTHREAD is not.  In order for Kokkos::Threads (Pthreads-based execution space) to be enabled in Tpetra, it must be enabled in Kokkos first."
#  endif // KOKKOS_HAVE_PTHREAD

  template<> bool InitExecSpace<Kokkos::Threads>::registeredExitHook_ = false;
#endif // HAVE_TPETRA_INST_PTHREAD

#ifdef HAVE_TPETRA_INST_CUDA
#  ifndef KOKKOS_HAVE_CUDA
#    error "HAVE_TPETRA_INST_CUDA is defined, but KOKKOS_HAVE_CUDA is not.  In order for Kokkos::Cuda to be enabled in Tpetra, it must be enabled in Kokkos first."
#  endif // KOKKOS_HAVE_CUDA

  // We don't need "template<>" here, because we already partially
  // specialized InitExecSpace for Kokkos::Cuda above.
  bool InitExecSpace<Kokkos::Cuda>::registeredExitHook_ = false;
#endif // HAVE_TPETRA_INST_CUDA

  //
  // UNIT TESTS
  //

  // Test that taking a subview of a Kokkos::DualView with zero rows
  // and nonzero columns produces a Kokkos::DualView with the correct
  // number of columns.
  //
  // This test doesn't need MPI.  Even if the default communicator
  // contains multiple processes, all processes do the same thing, so
  // we don't need to check all processes via all-reduce.  (Remember
  // that with the Teuchos unit test framework, only Process 0 prints
  // to the output stream 'out', and therefore only Process 0 can
  // trigger failure.)
  TEUCHOS_UNIT_TEST_TEMPLATE_4_DECL(Kokkos_DualView, DegenerateSubview, S, LO, GO, NODE)
  {
    using Kokkos::ALL;
    using Kokkos::subview;
    using std::endl;
    typedef Tpetra::MultiVector<S, LO, GO, NODE> MV;
    typedef typename MV::dual_view_type dual_view_type;
    typedef typename dual_view_type::execution_space execution_space;
    typedef typename dual_view_type::size_type size_type;

    out << "Make sure that taking a subview of a Kokkos::DualView "
      "with zero rows and nonzero columns produces a Kokkos::DualView "
      "with the correct number of columns." << endl;
    Teuchos::OSTab tab0 (out);

    // Initialize the execution space, if it hasn't already been
    // initialized.  I'm not so worried about giving the execution
    // space the best parameters for performance; the point is to test
    // taking subviews of a Kokkos::DualView.
    InitExecSpace<execution_space> init;
    // This avoids warnings for 'init' being unused.
    TEST_ASSERT( init.isInitialized () );
    if (! init.isInitialized ()) {
      return; // avoid crashes if initialization failed
    }
    TEST_ASSERT( execution_space::is_initialized () );
    if (! execution_space::is_initialized ()) {
      return; // avoid crashes if initialization failed
    }
    out << "Successfully initialized execution space, if necessary" << endl;

    size_type newNumRows = 0;
    size_type newNumCols = 0;
    std::pair<size_t, size_t> rowRng (0, 0);
    std::pair<size_t, size_t> colRng (0, 0);
    dual_view_type X_sub;

    out << "Make sure that Tpetra::MultiVector::dual_view_type has rank 2"
        << endl;
    TEST_EQUALITY_CONST( (int) dual_view_type::rank, 2 );

    size_type numRows = 0;
    size_type numCols = 10;
    out << "Create a " << numRows << " x " << numCols << " DualView" << endl;
    dual_view_type X ("X", numRows, numCols);

    TEST_EQUALITY_CONST( X.dimension_0 (), numRows );
    TEST_EQUALITY_CONST( X.dimension_1 (), numCols );
    TEST_EQUALITY_CONST( X.d_view.dimension_0 (), numRows );
    TEST_EQUALITY_CONST( X.d_view.dimension_1 (), numCols );
    TEST_EQUALITY_CONST( X.h_view.dimension_0 (), numRows );
    TEST_EQUALITY_CONST( X.h_view.dimension_1 (), numCols );
    out << endl;

    newNumRows = numRows;
    newNumCols = 5;
    colRng = std::pair<size_t, size_t> (0, newNumCols);
    out << "Create a " << newNumRows << " x " << newNumCols
        << " subview using (ALL, pair(" << colRng.first
        << "," << colRng.second << "))" << endl;
    X_sub = subview (X, ALL (), colRng);

    out << "X_sub claims to be " << X_sub.dimension_0 () << " x "
        << X_sub.dimension_1 () << endl;

    TEST_EQUALITY_CONST( X_sub.dimension_0 (), newNumRows );
    TEST_EQUALITY_CONST( X_sub.dimension_1 (), newNumCols );
    TEST_EQUALITY_CONST( X_sub.d_view.dimension_0 (), newNumRows );
    TEST_EQUALITY_CONST( X_sub.d_view.dimension_1 (), newNumCols );
    TEST_EQUALITY_CONST( X_sub.h_view.dimension_0 (), newNumRows );
    TEST_EQUALITY_CONST( X_sub.h_view.dimension_1 (), newNumCols );
    out << endl;

    newNumRows = numRows;
    newNumCols = 1;
    colRng = std::pair<size_t, size_t> (0, newNumCols);
    out << "Create a " << newNumRows << " x " << newNumCols
        << " subview using (ALL, pair(" << colRng.first
        << "," << colRng.second << "))" << endl;
    X_sub = subview (X, ALL (), colRng);

    out << "X_sub claims to be " << X_sub.dimension_0 () << " x "
        << X_sub.dimension_1 () << endl;

    TEST_EQUALITY_CONST( X_sub.dimension_0 (), newNumRows );
    TEST_EQUALITY_CONST( X_sub.dimension_1 (), newNumCols );
    TEST_EQUALITY_CONST( X_sub.d_view.dimension_0 (), newNumRows );
    TEST_EQUALITY_CONST( X_sub.d_view.dimension_1 (), newNumCols );
    TEST_EQUALITY_CONST( X_sub.h_view.dimension_0 (), newNumRows );
    TEST_EQUALITY_CONST( X_sub.h_view.dimension_1 (), newNumCols );
    out << endl;

    newNumRows = 0;
    newNumCols = numCols;
    rowRng = std::pair<size_t, size_t> (0, 0);
    out << "Create a " << newNumRows << " x " << newNumCols
        << " subview using (pair(" << rowRng.first << ","
        << rowRng.second << "), ALL)" << endl;
    X_sub = subview (X, rowRng, ALL ());

    out << "X_sub claims to be " << X_sub.dimension_0 () << " x "
        << X_sub.dimension_1 () << endl;

    TEST_EQUALITY_CONST( X_sub.dimension_0 (), newNumRows );
    TEST_EQUALITY_CONST( X_sub.dimension_1 (), newNumCols );
    TEST_EQUALITY_CONST( X_sub.d_view.dimension_0 (), newNumRows );
    TEST_EQUALITY_CONST( X_sub.d_view.dimension_1 (), newNumCols );
    TEST_EQUALITY_CONST( X_sub.h_view.dimension_0 (), newNumRows );
    TEST_EQUALITY_CONST( X_sub.h_view.dimension_1 (), newNumCols );
    out << endl;

    newNumRows = 0;
    newNumCols = 5;
    rowRng = std::pair<size_t, size_t> (0, newNumRows);
    colRng = std::pair<size_t, size_t> (0, newNumCols);
    out << "Create a " << newNumRows << " x " << newNumCols
        << " subview using (pair(" << rowRng.first << ","
        << rowRng.second << "), pair(" << colRng.first << ","
        << colRng.second << "))" << endl;
    X_sub = subview (X, rowRng, colRng);

    out << "X_sub claims to be " << X_sub.dimension_0 () << " x "
        << X_sub.dimension_1 () << endl;

    TEST_EQUALITY_CONST( X_sub.dimension_0 (), newNumRows );
    TEST_EQUALITY_CONST( X_sub.dimension_1 (), newNumCols );
    TEST_EQUALITY_CONST( X_sub.d_view.dimension_0 (), newNumRows );
    TEST_EQUALITY_CONST( X_sub.d_view.dimension_1 (), newNumCols );
    TEST_EQUALITY_CONST( X_sub.h_view.dimension_0 (), newNumRows );
    TEST_EQUALITY_CONST( X_sub.h_view.dimension_1 (), newNumCols );
    out << endl;
  }

//
// INSTANTIATIONS
//

#define UNIT_TEST_GROUP( SCALAR, LO, GO, NODE ) \
  TEUCHOS_UNIT_TEST_TEMPLATE_4_INSTANT( Kokkos_DualView, DegenerateSubview, SCALAR, LO, GO, NODE)

  TPETRA_ETI_MANGLING_TYPEDEFS()

  TPETRA_INSTANTIATE_TESTMV( UNIT_TEST_GROUP )

} // namespace (anonymous)

