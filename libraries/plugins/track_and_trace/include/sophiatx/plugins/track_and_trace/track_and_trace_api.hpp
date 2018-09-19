#pragma once
#include <sophiatx/plugins/json_rpc/utility.hpp>
#include <sophiatx/plugins/track_and_trace/track_and_trace_objects.hpp>

#include <sophiatx/protocol/types.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/vector.hpp>

namespace sophiatx { namespace plugins { namespace track_and_trace_plugin {

namespace detail
{
   class track_and_trace_api_impl;
}

struct get_current_holder_args
{
   tracked_object_name_type serial;
};

struct get_current_holder_return
{
   account_name_type holder;
};

struct get_holdings_args
{
   account_name_type holder;
};

struct get_holdings_return
{
   vector<tracked_object_name_type> serials;
};

struct get_tracked_object_history_args
{
   tracked_object_name_type serial;
};

struct tracked_object_history_item{
   account_name_type   new_owner;
   fc::time_point_sec    change_date;
   tracked_object_history_item(const transfer_history_object & o): new_owner(o.new_owner), change_date(o.change_date){};
};

struct get_tracked_object_history_return
{
   vector<tracked_object_history_item> history_items;
};

class track_and_trace_api
{
   public:
      track_and_trace_api();
      ~track_and_trace_api();

      DECLARE_API( (get_current_holder) (get_holdings) (get_tracked_object_history) )

   private:
      std::unique_ptr< detail::track_and_trace_api_impl > my;
};

} } } // sophiatx::plugins::track_and_trace_plugin

FC_REFLECT( sophiatx::plugins::track_and_trace_plugin::tracked_object_history_item, (new_owner)(change_date) )

FC_REFLECT( sophiatx::plugins::track_and_trace_plugin::get_current_holder_args,
            (serial) )

FC_REFLECT( sophiatx::plugins::track_and_trace_plugin::get_current_holder_return,
            (holder) )

FC_REFLECT( sophiatx::plugins::track_and_trace_plugin::get_holdings_args,
            (holder) )

FC_REFLECT( sophiatx::plugins::track_and_trace_plugin::get_holdings_return,
            (serials) )

FC_REFLECT( sophiatx::plugins::track_and_trace_plugin::get_tracked_object_history_args,
            (serial) )

FC_REFLECT( sophiatx::plugins::track_and_trace_plugin::get_tracked_object_history_return,
            (history_items) )