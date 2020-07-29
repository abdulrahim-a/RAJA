/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   RAJA header file containing user interface for RAJA::Teams
 *
 ******************************************************************************
 */

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) 2016-20, Lawrence Livermore National Security, LLC
// and RAJA project contributors. See the RAJA/COPYRIGHT file for details.
//
// SPDX-License-Identifier: (BSD-3-Clause)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

#ifndef RAJA_pattern_teams_core_HPP
#define RAJA_pattern_teams_core_HPP

#include "RAJA/config.hpp"
#include "RAJA/internal/get_platform.hpp"
#include "RAJA/policy/cuda/policy.hpp"
#include "RAJA/policy/loop/policy.hpp"
#include "RAJA/policy/openmp/policy.hpp"
#include "RAJA/util/macros.hpp"
#include "RAJA/util/plugins.hpp"
#include "RAJA/util/types.hpp"
#include "RAJA/util/StaticLayout.hpp"
#include "camp/camp.hpp"
#include "camp/concepts.hpp"
#include "camp/tuple.hpp"


#if defined(__CUDA_ARCH__)
#define TEAM_SHARED __shared__
#else
#define TEAM_SHARED
#endif

namespace RAJA
{

template<typename DataType, camp::idx_t... Sizes>
struct TeamSharedArray
{
  using layout_t = RAJA::StaticLayout<camp::make_idx_seq_t<sizeof...(Sizes)>,Sizes...>;
#if defined(__CUDA_ARCH__)
  DataType *array;
#else
  DataType array[layout_t::size()];
#endif

  TeamSharedArray()
  {
#if defined(__CUDA_ARCH__)
    const camp::idx_t NumElem = layout_t::size();
    TEAM_SHARED DataType m_array[NumElem];
    array = &m_array[0];
#endif
  }


  template<typename ...Indices>
  RAJA_HOST_DEVICE
  DataType &operator()(Indices ...indices)
  {
    return array[layout_t::s_oper(indices...)];
  }
};



// GPU or CPU threads available
enum ExecPlace {
HOST
#if defined(RAJA_ENABLE_CUDA)
,DEVICE
#endif
,NUM_PLACES };


// Support for Host, Host_threads, and Device
template <typename HOST_POLICY
#if defined(RAJA_ENABLE_CUDA)
          ,typename DEVICE_POLICY
#endif
          >
struct LoopPolicy {
  using host_policy_t = HOST_POLICY;
#if defined(RAJA_ENABLE_CUDA)
  using device_policy_t = DEVICE_POLICY;
#endif
};

template <typename HOST_POLICY
#if defined(RAJA_ENABLE_CUDA)
          ,typename DEVICE_POLICY
#endif
>
struct LaunchPolicy {
  using host_policy_t = HOST_POLICY;
#if defined(RAJA_ENABLE_CUDA)
  using device_policy_t = DEVICE_POLICY;
#endif
};


struct Teams {
  int value[3];

  RAJA_INLINE
  RAJA_HOST_DEVICE
  constexpr Teams() : value{1, 1, 1} {}

  RAJA_INLINE
  RAJA_HOST_DEVICE
  constexpr Teams(int i) : value{i, 1, 1} {}

  RAJA_INLINE
  RAJA_HOST_DEVICE
  constexpr Teams(int i, int j) : value{i, j, 1} {}

  RAJA_INLINE
  RAJA_HOST_DEVICE
  constexpr Teams(int i, int j, int k) : value{i, j, k} {}
};

struct Threads {
  int value[3];

  RAJA_INLINE
  RAJA_HOST_DEVICE
  constexpr Threads() : value{1, 1, 1} {}


  RAJA_INLINE
  RAJA_HOST_DEVICE
  constexpr Threads(int i) : value{i, 1, 1} {}

  RAJA_INLINE
  RAJA_HOST_DEVICE
  constexpr Threads(int i, int j) : value{i, j, 1} {}

  RAJA_INLINE
  RAJA_HOST_DEVICE
  constexpr Threads(int i, int j, int k) : value{i, j, k} {}
};

struct Lanes {
  int value;

  RAJA_INLINE
  RAJA_HOST_DEVICE
  constexpr Lanes() : value(0) {}

  RAJA_INLINE
  RAJA_HOST_DEVICE
  constexpr Lanes(int i) : value(i) {}
};

struct Resources {
public:
  Teams teams;
  Threads threads;
  Lanes lanes;

  RAJA_INLINE
  Resources() = default;

  Resources(Teams in_teams, Threads in_threads)
      : teams(in_teams), threads(in_threads){};

  /*
  template <typename... ARGS>
  RAJA_INLINE
  explicit Resources(ARGS const &... args)
  {
    camp::sink(apply(args)...);
  }
  */
private:
  RAJA_HOST_DEVICE
  RAJA_INLINE
  Teams apply(Teams const &a) { return (teams = a); }

  RAJA_HOST_DEVICE
  RAJA_INLINE
  Threads apply(Threads const &a) { return (threads = a); }

  RAJA_HOST_DEVICE
  RAJA_INLINE
  Lanes apply(Lanes const &a) { return (lanes = a); }
};

template<typename DataType, size_t N, size_t Nx, size_t Ny, size_t Nz>
struct PrivateMemoryImpl
{

  const int X{Nx};
  const int Y{Ny};
  const int Z{Nz};

#if defined(__CUDA_ARCH__)
 mutable DataType Array[N];
#else
  mutable DataType Array[N*Nx*Ny*Nz];
#endif

  RAJA_HOST_DEVICE
  DataType &operator()(int i, int tx, int ty=0, int tz=0) const
  {
#if defined(__CUDA_ARCH__)
    return Array[i];
#else
    const int offset = N*tx + N*Nx*ty + N*Nx*Ny*tz;
    return Array[i + offset];
#endif
  }

};

template<size_t Nx=16, size_t Ny=16, size_t Nz=4>
struct ThreadExclusive
{
 template<typename DataType, size_t N>
  using ExclusiveMem = PrivateMemoryImpl<DataType, N, Nx, Ny,Nz>;
};

class LaunchContext : public Resources
{
public:
  ExecPlace exec_place;

  LaunchContext(Resources const &base, ExecPlace place)
      : Resources(base), exec_place(place)
  {
  }


  RAJA_HOST_DEVICE
  void teamSync()
  {
#if defined(__CUDA_ARCH__)
    __syncthreads();
#endif
  }
};


template <typename LAUNCH_POLICY>
struct LaunchExecute;

template <typename POLICY_LIST, typename BODY>
void launch(ExecPlace place, Resources const &team_resources, BODY const &body)
{
  switch (place) {
    case HOST: {
        using launch_t = LaunchExecute<typename POLICY_LIST::host_policy_t>;
         launch_t::exec(LaunchContext(team_resources, HOST), body);
        break;
      }
#ifdef RAJA_ENABLE_CUDA
    case DEVICE: {
        using launch_t = LaunchExecute<typename POLICY_LIST::device_policy_t>;
        launch_t::exec(LaunchContext(team_resources, DEVICE), body);
        break;
      }
#endif
    default:
      throw "unknown launch place!";
  }
}

template <typename POLICY, typename SEGMENT>
struct LoopExecute;


template <typename POLICY_LIST,
          typename CONTEXT,
          typename SEGMENT,
          typename BODY>
RAJA_HOST_DEVICE RAJA_INLINE void loop(CONTEXT const &ctx,
                                       SEGMENT const &segment,
                                       BODY const &body)
{
#ifdef __CUDA_ARCH__
  LoopExecute<typename POLICY_LIST::device_policy_t, SEGMENT>::exec(ctx,
                                                                    segment,
                                                                    body);
#else  
  LoopExecute<typename POLICY_LIST::host_policy_t, SEGMENT>::exec(ctx,
                                                                  segment,
                                                                  body);
#endif
}

template <typename POLICY_LIST,
          typename CONTEXT,
          typename SEGMENT,
          typename BODY>
RAJA_HOST_DEVICE RAJA_INLINE void loop(CONTEXT const &ctx,
                                       SEGMENT const &segment0,
                                       SEGMENT const &segment1,
                                       BODY const &body)
{
#ifdef __CUDA_ARCH__
  LoopExecute<typename POLICY_LIST::device_policy_t, SEGMENT>::exec(ctx,
                                                                    segment0,
                                                                    segment1,
                                                                    body);
#else
  LoopExecute<typename POLICY_LIST::host_policy_t, SEGMENT>::exec(ctx,
                                                                  segment0,
                                                                  segment1,
                                                                  body);
#endif
}

template <typename POLICY_LIST,
          typename CONTEXT,
          typename SEGMENT,
          typename BODY>
RAJA_HOST_DEVICE RAJA_INLINE void loop(CONTEXT const &ctx,
                                       SEGMENT const &segment0,
                                       SEGMENT const &segment1,
                                       SEGMENT const &segment2,
                                       BODY const &body)
{

#ifdef __CUDA_ARCH__
  LoopExecute<typename POLICY_LIST::device_policy_t, SEGMENT>::exec(
      ctx, segment0, segment1, segment2, body);
#else
  LoopExecute<typename POLICY_LIST::host_policy_t, SEGMENT>::exec(
      ctx, segment0, segment1, segment2, body);                                                                  
#endif
}


}  // namespace RAJA
#endif
