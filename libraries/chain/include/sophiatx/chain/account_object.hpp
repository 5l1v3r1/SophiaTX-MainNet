#pragma once
#include <fc/fixed_string.hpp>

#include <sophiatx/protocol/authority.hpp>
#include <sophiatx/protocol/sophiatx_operations.hpp>

#include <sophiatx/chain/sophiatx_object_types.hpp>
#include <sophiatx/chain/witness_objects.hpp>
#include <sophiatx/chain/shared_authority.hpp>

#include <boost/multi_index/composite_key.hpp>

#include <numeric>

namespace sophiatx { namespace chain {

   using sophiatx::protocol::authority;

   class account_object : public object< account_object_type, account_object >
   {
      account_object() = delete;

      public:
         template<typename Constructor, typename Allocator>
         account_object( Constructor&& c, allocator< Allocator > a )
            :json_metadata( a )
         {
            c(*this);
         };

         id_type           id;

         account_name_type name;
         public_key_type   memo_key;
         shared_string     json_metadata;
         account_name_type proxy;

         time_point_sec    last_account_update;


         share_type        holdings_considered_for_interests = 0;
         share_type        update_considered_holding(share_type inserted, uint32_t block_no, uint32_t interest_blocks = SOPHIATX_INTEREST_BLOCKS){
            uint32_t my_turn = id._id % interest_blocks;
            uint32_t block = block_no % interest_blocks;
            int64_t to_my_turn;
            if ( my_turn >= block ){
               to_my_turn = my_turn - block;
            }else{
               to_my_turn = my_turn + interest_blocks - block;
            }
            share_type to_add = (inserted * to_my_turn);
            holdings_considered_for_interests += to_add;
            return to_add;
         };

         time_point_sec    created;
         bool              mined = true;
         account_name_type recovery_account;
         account_name_type reset_account = SOPHIATX_NULL_ACCOUNT;
         time_point_sec    last_account_recovery;

         asset             balance = asset( 0, chain::sophiatx_config::get<protocol::asset_symbol_type>("SOPHIATX_SYMBOL") );  ///< total liquid shares held by this account
         asset             vesting_shares = asset( 0, VESTS_SYMBOL ); ///< total vesting shares held by this account, controls its voting power

         asset             vesting_withdraw_rate = asset( 0, VESTS_SYMBOL ); ///< at the time this is updated it can be at most vesting_shares/104
         time_point_sec    next_vesting_withdrawal = fc::time_point_sec::maximum(); ///< after every withdrawal this is incremented by 1 week
         share_type        withdrawn = 0; /// Track how many shares have been withdrawn
         share_type        to_withdraw = 0; /// Might be able to look this up with operation history.

         fc::array<share_type, SOPHIATX_MAX_PROXY_RECURSION_DEPTH> proxied_vsf_votes;// = std::vector<share_type>( SOPHIATX_MAX_PROXY_RECURSION_DEPTH, 0 ); ///< the total VFS votes proxied to this account

         uint16_t          witnesses_voted_for = 0;


         share_type        total_balance() const{ return balance.amount + vesting_shares.amount;}
         /// This function should be used only when the account votes for a witness directly
         share_type        witness_vote_weight()const {
            return proxied_vsf_votes_total() + balance.amount + vesting_shares.amount;
         }

         share_type        proxied_vsf_votes_total()const {
            return std::accumulate( proxied_vsf_votes.begin(),
                                    proxied_vsf_votes.end(),
                                    share_type() );
         }

   };

   class account_authority_object : public object< account_authority_object_type, account_authority_object >
   {
      account_authority_object() = delete;

      public:
         template< typename Constructor, typename Allocator >
         account_authority_object( Constructor&& c, allocator< Allocator > a )
            : owner( a ), active( a )
         {
            c( *this );
         }

         id_type           id;

         account_name_type account;

         shared_authority  owner;   ///< used for backup control, can set owner or active
         shared_authority  active;  ///< used for all monetary operations, can set active or posting

         time_point_sec    last_owner_update;
   };

   class owner_authority_history_object : public object< owner_authority_history_object_type, owner_authority_history_object >
   {
      owner_authority_history_object() = delete;

      public:
         template< typename Constructor, typename Allocator >
         owner_authority_history_object( Constructor&& c, allocator< Allocator > a )
            :previous_owner_authority( shared_authority::allocator_type( a.get_segment_manager() ) )
         {
            c( *this );
         }

         id_type           id;

         account_name_type account;
         shared_authority  previous_owner_authority;
         time_point_sec    last_valid_time;
   };

   class account_recovery_request_object : public object< account_recovery_request_object_type, account_recovery_request_object >
   {
      account_recovery_request_object() = delete;

      public:
         template< typename Constructor, typename Allocator >
         account_recovery_request_object( Constructor&& c, allocator< Allocator > a )
            :new_owner_authority( shared_authority::allocator_type( a.get_segment_manager() ) )
         {
            c( *this );
         }

         id_type           id;

         account_name_type account_to_recover;
         shared_authority  new_owner_authority;
         time_point_sec    expires;
   };

   class change_recovery_account_request_object : public object< change_recovery_account_request_object_type, change_recovery_account_request_object >
   {
      public:
         template< typename Constructor, typename Allocator >
         change_recovery_account_request_object( Constructor&& c, allocator< Allocator > a )
         {
            c( *this );
         }

         id_type           id;

         account_name_type account_to_recover;
         account_name_type recovery_account;
         time_point_sec    effective_on;
   };

   class account_fee_sponsor_object : public object<account_fee_sponsor_object_type, account_fee_sponsor_object>
   {
   public:
      template< typename Constructor, typename Allocator >
      account_fee_sponsor_object( Constructor&& c, allocator< Allocator > a )
      {
         c( *this );
      }

      id_type        id;

      account_name_type sponsor;
      account_name_type sponsored;
   };

   struct by_name;
   struct by_proxy;
   struct by_balance;
   struct by_next_vesting_withdrawal;

   /**
    * @ingroup object_index
    */
   typedef multi_index_container<
      account_object,
      indexed_by<
         ordered_unique< tag< by_id >,
            member< account_object, account_id_type, &account_object::id > >,
         ordered_unique< tag< by_name >,
            member< account_object, account_name_type, &account_object::name > >,
         ordered_unique< tag< by_proxy >,
            composite_key< account_object,
               member< account_object, account_name_type, &account_object::proxy >,
               member< account_object, account_name_type, &account_object::name >
            > /// composite key by proxy
         >,
         ordered_non_unique< tag <by_balance>,
            const_mem_fun< account_object, share_type, &account_object::total_balance> >,
         ordered_unique< tag< by_next_vesting_withdrawal >,
            composite_key< account_object,
               member< account_object, time_point_sec, &account_object::next_vesting_withdrawal >,
               member< account_object, account_name_type, &account_object::name >
            > /// composite key by_next_vesting_withdrawal
         >
      >,
      allocator< account_object >
   > account_index;

   struct by_account;

   typedef multi_index_container <
      owner_authority_history_object,
      indexed_by <
         ordered_unique< tag< by_id >,
            member< owner_authority_history_object, owner_authority_history_id_type, &owner_authority_history_object::id > >,
         ordered_unique< tag< by_account >,
            composite_key< owner_authority_history_object,
               member< owner_authority_history_object, account_name_type, &owner_authority_history_object::account >,
               member< owner_authority_history_object, time_point_sec, &owner_authority_history_object::last_valid_time >,
               member< owner_authority_history_object, owner_authority_history_id_type, &owner_authority_history_object::id >
            >,
            composite_key_compare< std::less< account_name_type >, std::less< time_point_sec >, std::less< owner_authority_history_id_type > >
         >
      >,
      allocator< owner_authority_history_object >
   > owner_authority_history_index;

   struct by_last_owner_update;

   typedef multi_index_container <
      account_authority_object,
      indexed_by <
         ordered_unique< tag< by_id >,
            member< account_authority_object, account_authority_id_type, &account_authority_object::id > >,
         ordered_unique< tag< by_account >,
            composite_key< account_authority_object,
               member< account_authority_object, account_name_type, &account_authority_object::account >,
               member< account_authority_object, account_authority_id_type, &account_authority_object::id >
            >,
            composite_key_compare< std::less< account_name_type >, std::less< account_authority_id_type > >
         >,
         ordered_unique< tag< by_last_owner_update >,
            composite_key< account_authority_object,
               member< account_authority_object, time_point_sec, &account_authority_object::last_owner_update >,
               member< account_authority_object, account_authority_id_type, &account_authority_object::id >
            >,
            composite_key_compare< std::greater< time_point_sec >, std::less< account_authority_id_type > >
         >
      >,
      allocator< account_authority_object >
   > account_authority_index;

   struct by_expiration;

   typedef multi_index_container <
      account_recovery_request_object,
      indexed_by <
         ordered_unique< tag< by_id >,
            member< account_recovery_request_object, account_recovery_request_id_type, &account_recovery_request_object::id > >,
         ordered_unique< tag< by_account >,
            composite_key< account_recovery_request_object,
               member< account_recovery_request_object, account_name_type, &account_recovery_request_object::account_to_recover >
            >,
            composite_key_compare< std::less< account_name_type > >
         >,
         ordered_unique< tag< by_expiration >,
            composite_key< account_recovery_request_object,
               member< account_recovery_request_object, time_point_sec, &account_recovery_request_object::expires >,
               member< account_recovery_request_object, account_name_type, &account_recovery_request_object::account_to_recover >
            >,
            composite_key_compare< std::less< time_point_sec >, std::less< account_name_type > >
         >
      >,
      allocator< account_recovery_request_object >
   > account_recovery_request_index;

   struct by_effective_date;

   typedef multi_index_container <
      change_recovery_account_request_object,
      indexed_by <
         ordered_unique< tag< by_id >,
            member< change_recovery_account_request_object, change_recovery_account_request_id_type, &change_recovery_account_request_object::id > >,
         ordered_unique< tag< by_account >,
            composite_key< change_recovery_account_request_object,
               member< change_recovery_account_request_object, account_name_type, &change_recovery_account_request_object::account_to_recover >
            >,
            composite_key_compare< std::less< account_name_type > >
         >,
         ordered_unique< tag< by_effective_date >,
            composite_key< change_recovery_account_request_object,
               member< change_recovery_account_request_object, time_point_sec, &change_recovery_account_request_object::effective_on >,
               member< change_recovery_account_request_object, account_name_type, &change_recovery_account_request_object::account_to_recover >
            >,
            composite_key_compare< std::less< time_point_sec >, std::less< account_name_type > >
         >
      >,
      allocator< change_recovery_account_request_object >
   > change_recovery_account_request_index;

   struct by_sponsor;
   struct by_sponsored;

   typedef multi_index_container <
      account_fee_sponsor_object,
      indexed_by <
         ordered_unique< tag<by_id>,
            member< account_fee_sponsor_object, account_fee_sponsor_id_type, &account_fee_sponsor_object::id >
         >,
         ordered_unique< tag<by_sponsored>,
            member< account_fee_sponsor_object, account_name_type, &account_fee_sponsor_object::sponsored >
         >,
         ordered_unique< tag<by_sponsor>,
            composite_key< account_fee_sponsor_object,
               member< account_fee_sponsor_object, account_name_type, &account_fee_sponsor_object::sponsor >,
               member< account_fee_sponsor_object, account_name_type, &account_fee_sponsor_object::sponsored >
            >,
            composite_key_compare< std::less< account_name_type >, std::less< account_name_type > >
         >
      >,
      allocator<account_fee_sponsor_object>
   > account_fee_sponsor_index;

} }



FC_REFLECT( sophiatx::chain::account_object,
             (id)(name)(memo_key)(json_metadata)(proxy)(last_account_update)
             (created)(mined)
             (vesting_shares)(vesting_withdraw_rate)(next_vesting_withdrawal)(withdrawn)(to_withdraw)
             (recovery_account)(last_account_recovery)(reset_account)
             (balance)(holdings_considered_for_interests)
             (proxied_vsf_votes)(witnesses_voted_for)
          )
CHAINBASE_SET_INDEX_TYPE( sophiatx::chain::account_object, sophiatx::chain::account_index )

FC_REFLECT( sophiatx::chain::account_authority_object,
             (id)(account)(owner)(active)(last_owner_update)
)
CHAINBASE_SET_INDEX_TYPE( sophiatx::chain::account_authority_object, sophiatx::chain::account_authority_index )

FC_REFLECT( sophiatx::chain::owner_authority_history_object,
             (id)(account)(previous_owner_authority)(last_valid_time)
          )
CHAINBASE_SET_INDEX_TYPE( sophiatx::chain::owner_authority_history_object, sophiatx::chain::owner_authority_history_index )

FC_REFLECT( sophiatx::chain::account_recovery_request_object,
             (id)(account_to_recover)(new_owner_authority)(expires)
          )
CHAINBASE_SET_INDEX_TYPE( sophiatx::chain::account_recovery_request_object, sophiatx::chain::account_recovery_request_index )

FC_REFLECT( sophiatx::chain::change_recovery_account_request_object,
             (id)(account_to_recover)(recovery_account)(effective_on)
          )
CHAINBASE_SET_INDEX_TYPE( sophiatx::chain::change_recovery_account_request_object, sophiatx::chain::change_recovery_account_request_index )

FC_REFLECT( sophiatx::chain::account_fee_sponsor_object, (id)(sponsor)(sponsored))

CHAINBASE_SET_INDEX_TYPE( sophiatx::chain::account_fee_sponsor_object, sophiatx::chain::account_fee_sponsor_index )
