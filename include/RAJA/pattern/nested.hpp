#ifndef RAJA_pattern_nested_HPP
#define RAJA_pattern_nested_HPP


#include "RAJA/config.hpp"
#include "RAJA/policy/cuda.hpp"
#include "RAJA/util/defines.hpp"
#include "RAJA/util/types.hpp"

#include "RAJA/pattern/nested/internal.hpp"

#include "RAJA/util/chai_support.hpp"

#include "camp/camp.hpp"
#include "camp/concepts.hpp"
#include "camp/tuple.hpp"

#include <iostream>
#include <type_traits>

namespace RAJA
{

namespace nested
{

/*!
 * A RAJA::nested::forall execution policy.
 *
 * This is just a list of nested::forall statements.
 */
template <typename... Stmts>
using Policy = internal::StatementList<Stmts...>;





template <typename PolicyType, typename SegmentTuple, typename ... Bodies>
RAJA_INLINE void forall(PolicyType &&policy, SegmentTuple &&segments, Bodies && ... bodies)
{
  detail::setChaiExecutionSpace<PolicyType>();

  // TODO: test that all policy members model the Executor policy concept
  // TODO: add a static_assert for functors which cannot be invoked with
  //       index_tuple
  // TODO: add assert that all Lambda<i> match supplied loop bodies

  // Create the LoopData object, which contains our policy object,
  // our segments, loop bodies, and the tuple of loop indices
  // it is passed through all of the nested::forall mechanics by-referenece,
  // and only copied to provide thread-private instances.
  using policy_t = camp::decay<PolicyType>;
  using segment_t = camp::decay<SegmentTuple>;
  internal::LoopData<policy_t, segment_t, camp::decay<Bodies>...>
    loop_data(
          std::forward<PolicyType>(policy),
          std::forward<SegmentTuple>(segments),
          std::forward<Bodies>(bodies)...);

//  printf("sizeof(loop_data)=%ld bytes\n",(long)sizeof(loop_data));

  // Create a StatmentList wrapper to execute our policy (which is just
  // a StatementList)
  auto wrapper = internal::make_statement_list_wrapper(std::forward<PolicyType>(policy), loop_data);

  // Execute!
  wrapper();


  detail::clearChaiExecutionSpace();
}

}  // end namespace nested


}  // end namespace RAJA



#endif /* RAJA_pattern_nested_HPP */
