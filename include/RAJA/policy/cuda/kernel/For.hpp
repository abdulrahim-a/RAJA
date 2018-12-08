/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   Header file for CUDA statement executors.
 *
 ******************************************************************************
 */

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) 2016-18, Lawrence Livermore National Security, LLC.
//
// Produced at the Lawrence Livermore National Laboratory
//
// LLNL-CODE-689114
//
// All rights reserved.
//
// This file is part of RAJA.
//
// For details about use and distribution, please read RAJA/LICENSE.
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//


#ifndef RAJA_policy_cuda_kernel_For_HPP
#define RAJA_policy_cuda_kernel_For_HPP

#include "RAJA/config.hpp"

#include "RAJA/policy/cuda/kernel/internal.hpp"


namespace RAJA
{

namespace internal
{


/*
 * Executor for thread work sharing loop inside CudaKernel.
 * Mapping directly from threadIdx.xyz to indices
 * Assigns the loop index to offset ArgumentId
 */
template <typename Data,
          camp::idx_t ArgumentId,
          int ThreadDim,
          typename... EnclosedStmts>
struct CudaStatementExecutor<
    Data,
    statement::For<ArgumentId, RAJA::cuda_thread_xyz_direct<ThreadDim>, EnclosedStmts...>> {

  using stmt_list_t = StatementList<EnclosedStmts...>;

  using enclosed_stmts_t =
      CudaStatementListExecutor<Data, stmt_list_t>;


  static
  inline
  RAJA_DEVICE
  void exec(Data &data)
  {
    auto len = segment_length<ArgumentId>(data);
    auto i = get_cuda_dim<ThreadDim>(threadIdx);

    // assign thread id directly to offset
    data.template assign_offset<ArgumentId>(i);

    // execute enclosed statements if in bounds
    if(i < len){
      enclosed_stmts_t::exec(data);
    }
  }


  static
  inline
  LaunchDims calculateDimensions(Data const &data)
  {
    int len = segment_length<ArgumentId>(data);

    // request one thread per element in the segment
    LaunchDims dims;
    set_cuda_dim<ThreadDim>(dims.threads, len);

    // since we are direct-mapping, we REQUIRE len
    set_cuda_dim<ThreadDim>(dims.min_threads, len);

    // combine with enclosed statements
    LaunchDims enclosed_dims = enclosed_stmts_t::calculateDimensions(data);
    return dims.max(enclosed_dims);
  }
};

/*
 * Executor for thread work sharing loop inside CudaKernel.
 * Mapping directly from threadIdx.xyz to indices
 * Assigns the loop index to offset ArgumentId
 * Assigns the loop index to param ParamId
 */
template <typename Data,
          camp::idx_t ArgumentId,
          typename ParamId,
          int ThreadDim,
          typename... EnclosedStmts>
struct CudaStatementExecutor<
    Data,
    statement::ForICount<ArgumentId, ParamId, RAJA::cuda_thread_xyz_direct<ThreadDim>, EnclosedStmts...>>
    : public CudaStatementExecutor<
        Data,
        statement::For<ArgumentId, RAJA::cuda_thread_xyz_direct<ThreadDim>, EnclosedStmts...>> {

  using Base = CudaStatementExecutor<
        Data,
        statement::For<ArgumentId, RAJA::cuda_thread_xyz_direct<ThreadDim>, EnclosedStmts...>>;

  using typename Base::enclosed_stmts_t;

  static
  inline
  RAJA_DEVICE
  void exec(Data &data)
  {
    auto len = segment_length<ArgumentId>(data);
    auto i = get_cuda_dim<ThreadDim>(threadIdx);

    // assign thread id directly to offset
    data.template assign_offset<ArgumentId>(i);
    data.template assign_param<ParamId>(i);

    // execute enclosed statements if in bounds
    if(i < len){
      enclosed_stmts_t::exec(data);
    }
  }
};



/*
 * Executor for thread work sharing loop inside CudaKernel.
 * Provides a block-stride loop (stride of blockDim.xyz) for
 * each thread in xyz.
 * Assigns the loop index to offset ArgumentId
 */
template <typename Data,
          camp::idx_t ArgumentId,
          int ThreadDim,
          int MinThreads,
          typename... EnclosedStmts>
struct CudaStatementExecutor<
    Data,
    statement::For<ArgumentId, RAJA::cuda_thread_xyz_loop<ThreadDim, MinThreads>, EnclosedStmts...>> {

  using stmt_list_t = StatementList<EnclosedStmts...>;

  using enclosed_stmts_t =
      CudaStatementListExecutor<Data, stmt_list_t>;


  static
  inline RAJA_DEVICE void exec(Data &data)
  {
    // block stride loop
    int len = segment_length<ArgumentId>(data);
    auto i0 = get_cuda_dim<ThreadDim>(threadIdx);
    auto i_stride = get_cuda_dim<ThreadDim>(blockDim);
    for(auto i = i0;i < len;i += i_stride){

      // Assign the x thread to the argument
      data.template assign_offset<ArgumentId>(i);

      // execute enclosed statements
      enclosed_stmts_t::exec(data);
    }
  }


  static
  inline
  LaunchDims calculateDimensions(Data const &data)
  {
    int len = segment_length<ArgumentId>(data);

    // request one thread per element in the segment
    LaunchDims dims;
    set_cuda_dim<ThreadDim>(dims.threads, len);

    // but, since we are looping, we only need 1 thread, or whatever
    // the user specified for MinThreads
    set_cuda_dim<ThreadDim>(dims.min_threads, MinThreads);

    // combine with enclosed statements
    LaunchDims enclosed_dims = enclosed_stmts_t::calculateDimensions(data);
    return dims.max(enclosed_dims);
  }
};

/*
 * Executor for thread work sharing loop inside CudaKernel.
 * Provides a block-stride loop (stride of blockDim.xyz) for
 * each thread in xyz.
 * Assigns the loop index to offset ArgumentId
 * Assigns the loop index to param ParamId
 */
template <typename Data,
          camp::idx_t ArgumentId,
          typename ParamId,
          int ThreadDim,
          int MinThreads,
          typename... EnclosedStmts>
struct CudaStatementExecutor<
    Data,
    statement::ForICount<ArgumentId, ParamId, RAJA::cuda_thread_xyz_loop<ThreadDim, MinThreads>, EnclosedStmts...>>
    : public CudaStatementExecutor<
        Data,
        statement::For<ArgumentId, RAJA::cuda_thread_xyz_loop<ThreadDim, MinThreads>, EnclosedStmts...>> {

  using Base = CudaStatementExecutor<
        Data,
        statement::For<ArgumentId, RAJA::cuda_thread_xyz_loop<ThreadDim, MinThreads>, EnclosedStmts...>>;

  using typename Base::enclosed_stmts_t;

  static
  inline RAJA_DEVICE void exec(Data &data)
  {
    // block stride loop
    int len = segment_length<ArgumentId>(data);
    auto i0 = get_cuda_dim<ThreadDim>(threadIdx);
    auto i_stride = get_cuda_dim<ThreadDim>(blockDim);
    for(auto i = i0;i < len;i += i_stride){

      // Assign the x thread to the argument
      data.template assign_offset<ArgumentId>(i);
      data.template assign_param<ParamId>(i);

      // execute enclosed statements
      enclosed_stmts_t::exec(data);
    }
  }
};



/*
 * Executor for block work sharing inside CudaKernel.
 * Provides a grid-stride loop (stride of gridDim.xyz) for
 * each block in xyz.
 * Assigns the loop index to offset ArgumentId
 */
template <typename Data,
          camp::idx_t ArgumentId,
          int BlockDim,
          typename... EnclosedStmts>
struct CudaStatementExecutor<
    Data,
    statement::For<ArgumentId, RAJA::cuda_block_xyz_loop<BlockDim>, EnclosedStmts...>> {

  using stmt_list_t = StatementList<EnclosedStmts...>;

  using enclosed_stmts_t =
      CudaStatementListExecutor<Data, stmt_list_t>;


  static
  inline RAJA_DEVICE void exec(Data &data)
  {
    // grid stride loop
    int len = segment_length<ArgumentId>(data);
    auto i0 = get_cuda_dim<BlockDim>(blockIdx);
    auto i_stride = get_cuda_dim<BlockDim>(gridDim);
    for(auto i = i0;i < len;i += i_stride){

      // Assign the x thread to the argument
      data.template assign_offset<ArgumentId>(i);

      // execute enclosed statements
      enclosed_stmts_t::exec(data);
    }
  }


  static
  inline
  LaunchDims calculateDimensions(Data const &data)
  {
    int len = segment_length<ArgumentId>(data);

    // request one block per element in the segment
    LaunchDims dims;
    set_cuda_dim<BlockDim>(dims.blocks, len);

    // combine with enclosed statements
    LaunchDims enclosed_dims = enclosed_stmts_t::calculateDimensions(data);
    return dims.max(enclosed_dims);
  }
};

/*
 * Executor for block work sharing inside CudaKernel.
 * Provides a grid-stride loop (stride of gridDim.xyz) for
 * each block in xyz.
 * Assigns the loop index to offset ArgumentId
 * Assigns the loop index to param ParamId
 */
template <typename Data,
          camp::idx_t ArgumentId,
          typename ParamId,
          int BlockDim,
          typename... EnclosedStmts>
struct CudaStatementExecutor<
    Data,
    statement::ForICount<ArgumentId, ParamId, RAJA::cuda_block_xyz_loop<BlockDim>, EnclosedStmts...>>
    : public CudaStatementExecutor<
        Data,
        statement::For<ArgumentId, RAJA::cuda_block_xyz_loop<BlockDim>, EnclosedStmts...>> {

  using Base = CudaStatementExecutor<
      Data,
      statement::For<ArgumentId, RAJA::cuda_block_xyz_loop<BlockDim>, EnclosedStmts...>>;

  using typename Base::enclosed_stmts_t;

  static
  inline RAJA_DEVICE void exec(Data &data)
  {
    // grid stride loop
    int len = segment_length<ArgumentId>(data);
    auto i0 = get_cuda_dim<BlockDim>(blockIdx);
    auto i_stride = get_cuda_dim<BlockDim>(gridDim);
    for(auto i = i0;i < len;i += i_stride){

      // Assign the x thread to the argument
      data.template assign_offset<ArgumentId>(i);
      data.template assign_param<ParamId>(i);

      // execute enclosed statements
      enclosed_stmts_t::exec(data);
    }
  }
};



/*
 * Executor for sequential loops inside of a CudaKernel.
 *
 * This is specialized since it need to execute the loop immediately.
 * Assigns the loop index to offset ArgumentId
 */
template <typename Data,
          camp::idx_t ArgumentId,
          typename... EnclosedStmts>
struct CudaStatementExecutor<
    Data,
    statement::For<ArgumentId, seq_exec, EnclosedStmts...> > {

  using stmt_list_t = StatementList<EnclosedStmts...>;

  using enclosed_stmts_t =
      CudaStatementListExecutor<Data, stmt_list_t>;

  static
  inline
  RAJA_DEVICE
  void exec(Data &data)
  {

    using idx_type = camp::decay<decltype(camp::get<ArgumentId>(data.offset_tuple))>;

    idx_type len = segment_length<ArgumentId>(data);

    for(idx_type i = 0;i < len;++ i){
      // Assign i to the argument
      data.template assign_offset<ArgumentId>(i);

      // execute enclosed statements
      enclosed_stmts_t::exec(data);
    }
  }


  static
  inline
  LaunchDims calculateDimensions(Data const &data)
  {
    return enclosed_stmts_t::calculateDimensions(data);
  }
};

/*
 * Executor for sequential loops inside of a CudaKernel.
 *
 * This is specialized since it need to execute the loop immediately.
 * Assigns the loop index to offset ArgumentId
 * Assigns the loop index to param ParamId
 */
template <typename Data,
          camp::idx_t ArgumentId,
          typename ParamId,
          typename... EnclosedStmts>
struct CudaStatementExecutor<
    Data,
    statement::ForICount<ArgumentId, ParamId, seq_exec, EnclosedStmts...> >
    : public CudaStatementExecutor<
        Data,
        statement::For<ArgumentId, seq_exec, EnclosedStmts...> > {

  using Base = CudaStatementExecutor<
      Data,
      statement::For<ArgumentId, seq_exec, EnclosedStmts...> >;

  using typename Base::enclosed_stmts_t;

  static
  inline
  RAJA_DEVICE
  void exec(Data &data)
  {
    using idx_type = camp::decay<decltype(camp::get<ArgumentId>(data.offset_tuple))>;

    idx_type len = segment_length<ArgumentId>(data);

    for(idx_type i = 0;i < len;++ i){
      // Assign i to the argument
      data.template assign_offset<ArgumentId>(i);
      data.template assign_param<ParamId>(i);

      // execute enclosed statements
      enclosed_stmts_t::exec(data);
    }
  }
};





}  // namespace internal
}  // end namespace RAJA


#endif /* RAJA_pattern_kernel_HPP */
