// Copyright (c) 2013, Sandia Corporation.
 // Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 // the U.S. Government retains certain rights in this software.
 // 
 // Redistribution and use in source and binary forms, with or without
 // modification, are permitted provided that the following conditions are
 // met:
 // 
 //     * Redistributions of source code must retain the above copyright
 //       notice, this list of conditions and the following disclaimer.
 // 
 //     * Redistributions in binary form must reproduce the above
 //       copyright notice, this list of conditions and the following
 //       disclaimer in the documentation and/or other materials provided
 //       with the distribution.
 // 
 //     * Neither the name of Sandia Corporation nor the names of its
 //       contributors may be used to endorse or promote products derived
 //       from this software without specific prior written permission.
 // 
 // THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 // "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 // LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 // A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 // OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 // SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 // LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 // DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 // THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 // (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 // OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef STK_NGP_NGP_H_
#define STK_NGP_NGP_H_

#include <Kokkos_Core.hpp>

#include <stk_ngp/NgpSpaces.hpp>
#include <stk_ngp/NgpMesh.hpp>
#include <stk_ngp/NgpField.hpp>
#include <stk_ngp/NgpAtomics.hpp>
#include <stk_ngp/NgpForEachEntity.hpp>
#include <stk_ngp/NgpReductions.hpp>

namespace ngp {

#ifdef KOKKOS_ENABLE_CUDA
using Mesh = StaticMesh;
template <typename T> using Field = ngp::StaticField<T>;
template <typename T> using ConstField = ngp::ConstStaticField<T>;
#else
using Mesh = ngp::StkMeshAdapter;
template <typename T> using Field = ngp::StkFieldAdapter<T>;
template <typename T> using ConstField = ngp::ConstStkFieldAdapter<T>;
#endif

}

#endif /* STK_NGP_NGP_H_ */
