.. ##
.. ## Copyright (c) 2016-20, Lawrence Livermore National Security, LLC
.. ## and other RAJA project contributors. See the RAJA/COPYRIGHT file
.. ## for details.
.. ##
.. ## SPDX-License-Identifier: (BSD-3-Clause)
.. ##

.. _resource-label:

=========
Resources
=========

This section describes the basic concepts of Resource types and their functionality in 
``RAJA::forall``. Resources are used as an interface to various backend constructs and their 
respective hardware. Currently there exists Resource types for ``Cuda``, ``Hip``, ``Omp`` (target) 
and ``Host``. Resource objects allow the user to execute ``RAJA::forall`` calls asynchronously on a
respective thread/stream. The underlying concept of each individual Resource is still under 
development and it should be considered that functionality / behaviour may change.

.. note:: * Currently feature complete asynchronous behaviour and streamed/threaded support is
            only available when using ``Cuda`` or ``Hip`` resources. 
          * The ``RAJA::resources`` namespace aliases the ``camp::resources`` namespace.

Each resource has a set of underlying functionality that is synonymous across all resource types.  

 ===================== ===============================================
 Methods               Brief description
 ===================== ===============================================
 get_platform          Returns the underlying camp platform
                       the resource is associated with.
 get_event             Return an Event object for the resource from
                       the last resource call.
 allocate              Allocate data per the resource's given
                       backend.
 deallocate            Deallocate data per the resource's given
                       backend.
 memcpy                Perform a memory copy from a src location
                       to a destination location from the
                       resource's backend.
 memset                Set memory value per the resourse's
                       given backend.
 wait_for              Enqueue a wait on the resource's stream/thread
                       for a user passed event to occur.
 ===================== ===============================================

.. note:: * ``deallocate``, ``memcpy`` and ``memset`` will only work with pointers that correspond
            to memory locations that have been allocated on the resource's respective device.
  
Each resource type also defines specific backend information/functionality. For example, each
Cuda resource contains a ``cudaStream_t`` value with an associated get method. See the 
individual functionality for each resource in ``raja/tpl/camp/include/resource/``.

.. note:: * Stream IDs are assigned to resources in a round robin fashion. The number of
            independent streams for a given backend is limited to the maximum number of concurrent
            streams that backend supports. 

------------
Type-Erasure
------------

Resources can be declared in two formats: An erased resource, and a concrete resource. The 
underlying runtime functionality is the same for both formats. An erased resource allows a user 
the ability to change the resource backend at runtime. 

Concrete Cuda resource::

    RAJA::resources::Cuda my_cuda_res;

Erased resource::

    if (use_gpu)
      RAJA::resources::Resource my_res{RAJA::resources::Cuda()};
    else
      RAJA::resources::Resource my_res{RAJA::resources::Host()};


Memory allocation on resources::

    int* a1 = my_cuda_res.allocate<int>(ARRAY_SIZE);
    int* a2 = my_res.allocate<int>(ARRAY_SIZE);

If ``use_gpu`` is ``true``, the underlying type of ``my_res`` is a Cuda resource, therefore ``a1`` 
and ``a2`` will both be allocated on the GPU. If ``use_gpu`` is ``false``, then only ``a1`` is 
allocated on the GPU, and ``a2`` is allocated on the host.


------
Forall
------

A resource is an optional argument to a ``RAJA::forall`` call. When used, it is passed as the first
argument of the call.::

    RAJA::forall<ExecPol>(my_cuda_res, .... )

When specifying a Cuda/Hip resource the ``RAJA::forall`` is executed aynchronously on a stream.
Currently Cuda/Hip are the only Resources that enable asynchronous threading with a ``RAJA::forall``.
All other calls are defaulted to using the ``Host`` resource until further support is 
added.

The Resource type that is passed to a ``RAJA::forall`` call must be a concrete type. This is to
allow compile time assertion that the resource is compatible with the given execution policy. For
example::
    
    using ExecPol = RAJA::cuda_exec_async<BLOCK_SIZE>;
    RAJA::resources::Cuda my_cuda_res;
    RAJA::resources::Resource my_res{RAJA::resources::Cuda()};
    RAJA::resources::Host my_host_res;

    RAJA::forall<ExecPol>(my_cuda_res, .... ) // Compiles.
    RAJA::forall<ExecPol>(my_res, .... ) // Compilation Error. Not Concrete.
    RAJA::forall<ExecPol>(my_host_res, .... ) // Compilation Error. Mismatched Resource and Exec Policy.

Below is a list of the current concrete resource types and their execution policy suport.

 ======== ==============================
 Resource Policy Type
 ======== ==============================
 Cuda     | cuda_exec
          | cuda_exec_async
 Hip      | hip_exec
          | hip_exec_async
 Omp*     | omp_target_parallel_for_exec
          | omp_target_parallel_for_exec_n
 Host     | loop_exec
          | seq_exec
          | openmp_parallel_exec
          | omp_for_schedule_exec
          | omp_for_nowait_schedule_exec
          | simd_exec
          | tbb_for_dynamic
          | tbb_for_static
 ======== ==============================

.. note:: * The ``RAJA::resources::Omp`` resource is still under development.

IndexSet policies require two execution policies to define them. Currently, users may only pass a
single resource to the forall Indexset call. This resource is used to execute the inner execution 
loop of the indexset policy.::

    using ExecPol = RAJA::ExecPolicy<RAJA::seq_segit, RAJA::cuda_exec<256>>;
    RAJA::forall<ExecPol>(my_cuda_res, iset,  .... );

When a resource is not provided by the user, a *default* resource is assigned from within the RAJA
forall implementation. This default resource can be accessed in a number of ways.

Directly from the concrete resource type::

    RAJA::resources::Cuda my_default_cuda = RAJA::resources::Cuda::get_defualt();

The resource type can be deduced from an execution policy::

    using Res = RAJA::resources::get_resource<ExecPol>::type;
    Res r = Res::get_default();

Deduced from an execution policy and return the default directly::

    auto my_resource = RAJA::resources::get_default_resource<ExecPol>();

.. note:: * For Cuda and Hip the default resource is *NOT* the CUDA or HIP default stream. It is its 
            own stream defined in ``camp/include/resource/``. This is in an attempt to break away
            from some of the issues that arise from the synchronization behaviour of the CUDA/HIP 
            default stream. It is still possible to use the CUDA/HIP defined default stream as the
            default resource. This can be enabled by defining ``CAMP_USE_PLATFORM_DEFAULT_STREAM``
            before including RAJA in a project.

------
Events
------

Event objects are a feature that allow users to wait or query the status of a resource's action. An
event can be returned from a resource with::

    RAJA::resources::Event e = my_res.get_event();

Getting an event like this enqueues an event type object for the given backend. 

Users can call the blocking wait function on the event::

    e.wait();

Preferably, users can enqueue the event to a specific resource, forcing only that resource to wait for
the event::

    my_res.wait_for(&e);

The latter is useful as it allows the user to set up dependencies between resource objects and 
``RAJA::forall`` calls.

.. note:: *An Event object is only created if a user explicitly uses the event returned by the 
          ``RAJA::forall`` call. This stops unnecessary event objects being created when not needed.
          For example::
    
               forall<cuda_exec_async<BLOCK_SIZE>>(my_cuda_res, ...

           will *not* generate a cudaStreamEvent.

                RAJA::resources::Event e = forall<cuda_exec_async<BLOCK_SIZE>>(my_cuda_res, ...

           will generate a cudaStreamEvent.

-------
Example
-------

This example executes three kernels across two cuda streams on the GPU with a requirement that the
first and second kernel finish execution before launching the third. It also demonstrates copying 
memory from the device to host on a resource:
    
First define two concrete CUDA resources and a Host resource:

.. literalinclude:: ../../../../examples/resource-forall.cpp
   :start-after: _raja_res_defres_start
   :end-before: _raja_res_defres_end
   :language: C++

Allocate data on 2 GPU arrays and a host array:

.. literalinclude:: ../../../../examples/resource-forall.cpp
   :start-after: _raja_res_alloc_start
   :end-before: _raja_res_alloc_end
   :language: C++

Execute Cuda stream 1/``res_gpu1``:

.. literalinclude:: ../../../../examples/resource-forall.cpp
   :start-after: _raja_res_k1_start
   :end-before: _raja_res_k1_end
   :language: C++
    
Execute Cuda stream 2/``res_gpu2`` and return an ``Event`` object:

.. literalinclude:: ../../../../examples/resource-forall.cpp
   :start-after: _raja_res_k2_start
   :end-before: _raja_res_k2_end
   :language: C++
    
The next kernel on ``res_gpu1`` requires that the last forall on ``res_gpu2`` finish first. Therefore we 
enqueue a wait to ``res_gpu1`` depending on ``res_gpu2`` finishing:

.. literalinclude:: ../../../../examples/resource-forall.cpp
   :start-after: _raja_res_wait_start
   :end-before: _raja_res_wait_end
   :language: C++
    
Execute the second kernel on ``res_gpu1`` now that work has finished on the previous two kernels:

.. literalinclude:: ../../../../examples/resource-forall.cpp
   :start-after: _raja_res_k3_start
   :end-before: _raja_res_k3_end
   :language: C++
    
We enqueue a memcpy on ``res_gpu1`` to move data from the GPU to the host:

.. literalinclude:: ../../../../examples/resource-forall.cpp
   :start-after: _raja_res_memcpy_start
   :end-before: _raja_res_memcpy_end
   :language: C++
    
Finally use the data on the host side.:

.. literalinclude:: ../../../../examples/resource-forall.cpp
   :start-after: _raja_res_k4_start
   :end-before: _raja_res_k4_end
   :language: C++