#include <sophiatx/plugins/json_rpc/json_rpc_plugin.hpp>
#include <sophiatx/plugins/json_rpc/utility.hpp>

#include <sophiatx/remote_db/remote_db.hpp>

#include <boost/algorithm/string.hpp>

#include <fc/log/logger_config.hpp>
#include <fc/exception/exception.hpp>
#include <fc/macros.hpp>
#include <fc/io/fstream.hpp>

#include <chainbase/chainbase.hpp>

namespace sophiatx { namespace plugins { namespace json_rpc {

namespace detail
{
   struct json_rpc_error
   {
      json_rpc_error()
         : code( 0 ) {}

      json_rpc_error( int32_t c, std::string m, fc::optional< fc::variant > d = fc::optional< fc::variant >() )
         : code( c ), message( m ), data( d ) {}

      int32_t                          code;
      std::string                      message;
      fc::optional< fc::variant >      data;
   };

   struct json_rpc_response
   {
      std::string                      jsonrpc = "2.0";
      fc::optional< fc::variant >      result;
      fc::optional< json_rpc_error >   error;
      fc::variant                      id;
   };

   typedef void_type             get_methods_args;
   typedef vector< string >      get_methods_return;

   struct get_signature_args
   {
      string method;
   };

   typedef api_method_signature  get_signature_return;

   class json_rpc_logger
   {
   public:
      json_rpc_logger(const string& _dir_name) : dir_name(_dir_name) {}

      ~json_rpc_logger()
      {
         if (counter == 0)
            return; // nothing to flush

         // flush tests.yaml file with all passed responses
         fc::path file(dir_name);
         file /= "tests.yaml";

         const char* head =
         "---\n"
         "- config:\n"
         "  - testset: \"API Tests\"\n"
         "  - generators:\n"
         "    - test_id: {type: 'number_sequence', start: 1}\n"
         "\n"
         "- base_test: &base_test\n"
         "  - generator_binds:\n"
         "    - test_id: test_id\n"
         "  - url: \"/rpc\"\n"
         "  - method: \"POST\"\n"
         "  - validators:\n"
         "    - extract_test: {jsonpath_mini: \"error\", test: \"not_exists\"}\n"
         "    - extract_test: {jsonpath_mini: \"result\", test: \"exists\"}\n";

         fc::ofstream o(file);
         o << head;
         o << "    - json_file_validator: {jsonpath_mini: \"result\", comparator: \"json_compare\", expected: {template: '" << dir_name << "/$test_id'}}\n\n";

         for (uint32_t i = 1; i <= counter; ++i)
         {
            o << "- test:\n";
            o << "  - body: {file: \"" << i << ".json\"}\n";
            o << "  - name: \"test" << i << "\"\n";
            o << "  - <<: *base_test\n";
            o << "\n";
         }

         o.close();
      }

      void log(const fc::variant_object& request, json_rpc_response& response)
      {
         fc::path file(dir_name);
         bool error = response.error.valid();
         std::string counter_str;

         if (error)
            counter_str = std::to_string(++errors) + "_error";
         else
            counter_str = std::to_string(++counter);

         file /= counter_str + ".json";

         fc::json::save_to_file(request, file);

         file.replace_extension("json.pat");

         if (error)
            fc::json::save_to_file(response.error, file);
         else
            fc::json::save_to_file(response.result, file);
      }

   private:
      string   dir_name;
      /** they are used as 1-based, because of problem with start pyresttest number_sequence generator from 0
       *  (it means first test is '1' also as first error)
       */
      uint32_t counter = 0;
      uint32_t errors = 0;
   };

   class json_rpc_plugin_impl
   {
      public:
         json_rpc_plugin_impl(json_rpc_plugin& plugin);
         ~json_rpc_plugin_impl();

         void add_api_method( const string& api_name, const string& method_name, const api_method& api, const api_method_signature& sig );

         void add_api_subscribe_method( const string& api_name, const string& method_name );

         api_method* find_api_method( const std::string& api, const std::string& method );
         void process_params( string method, const fc::variant_object& request, std::string& api_name,
               string& method_name ,fc::variant& func_args, bool& is_subscribe );
         fc::optional< fc::variant > call_api_method(const string& api_name, const string& method_name, const fc::variant& func_args);
         void rpc_id( const fc::variant_object& request, json_rpc_response& response );
         void rpc_jsonrpc( const fc::variant_object& request, json_rpc_response& response, std::function<void(string)> callback );
         json_rpc_response rpc( const fc::variant& message, std::function<void(string)> callback );
         void send_ws_notice( uint64_t id, string);
         void initialize();

         void log(const fc::variant_object& request, json_rpc_response& response, const std::string& api, const std::string& method)
         {
            std::string responseStr = "OK";
            std::string errorMsg;
            if (response.error) {
               responseStr = "ERR";
               errorMsg = response.error->message;
            }
            // Logs request info for monitoring
            ilog("Received request. Api: ${a}, Method: ${m}, Resp: ${r}", ("a", api)("m", method)("r", responseStr));

            if (_logger)
               _logger->log(request, response);
         }

         DECLARE_API(
            (get_methods)
            (get_signature) )

         map< string, api_description >                     _registered_apis;
         vector< string >                                   _methods;
         map< string, map< string, api_method_signature > > _method_sigs;
         std::unique_ptr< json_rpc_logger >                 _logger;
         vector<string>                                     _subscribe_methods;
         map<uint64_t, std::function<void(string)> >        _subscribe_callbacks;
         json_rpc_plugin&                                   _plugin;
   };

   json_rpc_plugin_impl::json_rpc_plugin_impl(json_rpc_plugin& plugin):_plugin(plugin) {}
   json_rpc_plugin_impl::~json_rpc_plugin_impl() {}

   void json_rpc_plugin_impl::add_api_method( const string& api_name, const string& method_name, const api_method& api, const api_method_signature& sig )
   {
      _registered_apis[ api_name ][ method_name ] = api;
      _method_sigs[ api_name ][ method_name ] = sig;

      std::stringstream canonical_name;
      canonical_name << api_name << '.' << method_name;
      _methods.push_back( canonical_name.str() );
   }

   void json_rpc_plugin_impl::add_api_subscribe_method( const string& api_name, const string& method_name )
   {
      std::stringstream canonical_name;
      canonical_name << api_name << '.' << method_name;
      _subscribe_methods.push_back( canonical_name.str() );
   }

   void json_rpc_plugin_impl::initialize()
   {
      JSON_RPC_REGISTER_API( "jsonrpc", _plugin.app() );
   }

   get_methods_return json_rpc_plugin_impl::get_methods( const get_methods_args& args, bool lock )
   {
      FC_UNUSED( lock )
      return _methods;
   }

   get_signature_return json_rpc_plugin_impl::get_signature( const get_signature_args& args, bool lock )
   {
      FC_UNUSED( lock )
      vector< string > v;
      boost::split( v, args.method, boost::is_any_of( "." ) );
      FC_ASSERT( v.size() == 2, "Invalid method name" );

      auto api_itr = _method_sigs.find( v[0] );
      FC_ASSERT( api_itr != _method_sigs.end(), "Method ${api}.${method} does not exist.", ("api", v[0])("method", v[1]) );

      auto method_itr = api_itr->second.find( v[1] );
      FC_ASSERT( method_itr != api_itr->second.end(), "Method ${api}.${method} does not exist", ("api", v[0])("method", v[1]) );

      return method_itr->second;
   }

   api_method* json_rpc_plugin_impl::find_api_method( const std::string& api, const std::string& method )
   {
      auto api_itr = _registered_apis.find( api );
      FC_ASSERT( api_itr != _registered_apis.end(), "Could not find API ${api}", ("api", api) );

      auto method_itr = api_itr->second.find( method );
      FC_ASSERT( method_itr != api_itr->second.end(), "Could not find method ${method}", ("method", method) );

      return &(method_itr->second);
   }

void json_rpc_plugin_impl::process_params( string method, const fc::variant_object& request, std::string& api_name,
                     string& method_name ,fc::variant& func_args, bool& is_subscribe ) {
      if( method == "call" )
      {
         FC_ASSERT( request.contains( "params" ) );

         std::vector< fc::variant > v;

         if( request[ "params" ].is_array() )
            v = request[ "params" ].as< std::vector< fc::variant > >();

         FC_ASSERT( v.size() == 2 || v.size() == 3, "params should be {\"api\", \"method\", \"args\"" );

         api_name = v[0].as_string();
         method_name = v[1].as_string();
         func_args = ( v.size() == 3 ) ? v[2] : fc::json::from_string( "{}" );

         auto it = find (_subscribe_methods.begin(), _subscribe_methods.end(), api_name + "." + method_name);
         if( it!=_subscribe_methods.end() )
            is_subscribe = true;
      }
      else
      {
         vector< std::string > v;
         boost::split( v, method, boost::is_any_of( "." ) );

         FC_ASSERT( v.size() == 2, "method specification invalid. Should be api.method" );

         api_name = v[0];
         method_name = v[1];
         func_args = request.contains( "params" ) ? request[ "params" ] : fc::json::from_string( "{}" );

         auto it = find (_subscribe_methods.begin(), _subscribe_methods.end(), method);
         if( it!=_subscribe_methods.end() )
            is_subscribe = true;
      }
   }

   void json_rpc_plugin_impl::rpc_id( const fc::variant_object& request, json_rpc_response& response )
   {
      if( request.contains( "id" ) )
      {
         const fc::variant& _id = request[ "id" ];
         int _type = _id.get_type();
         switch( _type )
         {
            case fc::variant::int64_type:
            case fc::variant::uint64_type:
            case fc::variant::string_type:
               response.id = request[ "id" ];
            break;

            default:
               response.error = json_rpc_error( JSON_RPC_INVALID_REQUEST, "Only integer value or string is allowed for member \"id\"" );
         }
      }
   }

   fc::optional<fc::variant>
   json_rpc_plugin_impl::call_api_method(const string &api_name, const string &method_name, const fc::variant &func_args) {
      if( _registered_apis.find(api_name) == _registered_apis.end() && remote::remote_db::initialized()) {
         return fc::optional<fc::variant>(remote::remote_db::remote_call(api_name, method_name, func_args));
      } else {
         api_method *call = find_api_method(api_name, method_name);
         return (*call)(func_args);
      }
   }

   void json_rpc_plugin_impl::rpc_jsonrpc( const fc::variant_object& request, json_rpc_response& response, std::function<void(string)> callback )
   {
      string api_name;
      string method_name;

      if( request.contains( "jsonrpc" ) && request[ "jsonrpc" ].is_string() && request[ "jsonrpc" ].as_string() == "2.0" )
      {
         if( request.contains( "method" ) && request[ "method" ].is_string() )
         {
            try
            {
               string method = request[ "method" ].as_string();

               // This is to maintain backwards compatibility with existing call structure.
               if( ( method == "call" && request.contains( "params" ) ) || method != "call" )
               {
                  bool subscribe = false;
                  fc::variant func_args;

                  try
                  {
                     process_params( method, request, api_name, method_name, func_args, subscribe );
                  }
                  catch( fc::assert_exception& e )
                  {
                     response.error = json_rpc_error( JSON_RPC_PARSE_PARAMS_ERROR, e.to_string(), fc::variant( *(e.dynamic_copy_exception()) ) );
                  }

                  try
                  {
                     if(!response.error.valid())
                     {
                        if(subscribe){
                           _subscribe_callbacks[response.result->as_uint64()] = callback;
                           response.result = 1;
                        } else {
                           response.result = call_api_method(api_name, method_name, func_args);
                        }
                     }
                  }
                  catch( chainbase::lock_exception& e )
                  {
                     response.error = json_rpc_error( JSON_RPC_ERROR_DURING_CALL, e.what() );
                  }
                  catch( fc::assert_exception& e )
                  {
                     response.error = json_rpc_error( JSON_RPC_ERROR_DURING_CALL, e.to_string(), fc::variant( *(e.dynamic_copy_exception()) ) );
                  }
               }
               else
               {
                  response.error = json_rpc_error( JSON_RPC_NO_PARAMS, "A member \"params\" does not exist" );
               }
            }
            catch( fc::assert_exception& e )
            {
               response.error = json_rpc_error( JSON_RPC_METHOD_NOT_FOUND, e.to_string(), fc::variant( *(e.dynamic_copy_exception()) ) );
            }
         }
         else
         {
            response.error = json_rpc_error( JSON_RPC_INVALID_REQUEST, "A member \"method\" does not exist" );
         }
      }
      else
      {
         response.error = json_rpc_error( JSON_RPC_INVALID_REQUEST, "jsonrpc value is not \"2.0\"" );
      }

      log(request, response, api_name, method_name);
   }

   json_rpc_response json_rpc_plugin_impl::rpc( const fc::variant& message, std::function<void(string)> callback )
   {
      json_rpc_response response;

      ddump( (message) );

      try
      {
         const auto& request = message.get_object();

         rpc_id( request, response );

         // This second layer try/catch is to isolate errors that occur after parsing the id so that the id is properly returned.
         try
         {
            if( !response.error.valid() )
               rpc_jsonrpc( request, response, callback );
         }
         catch( fc::exception& e )
         {
            response.error = json_rpc_error( JSON_RPC_SERVER_ERROR, e.to_string(), fc::variant( *(e.dynamic_copy_exception()) ) );
         }
         catch( std::exception& e )
         {
            response.error = json_rpc_error( JSON_RPC_SERVER_ERROR, "Unknown error - parsing rpc message failed", fc::variant( e.what() ) );
         }
         catch( ... )
         {
            response.error = json_rpc_error( JSON_RPC_SERVER_ERROR, "Unknown error - parsing rpc message failed" );
         }
      }
      catch( fc::parse_error_exception& e )
      {
         response.error = json_rpc_error( JSON_RPC_PARSE_ERROR, e.to_string(), fc::variant( *(e.dynamic_copy_exception()) ) );
      }
      catch( fc::bad_cast_exception& e )
      {
         response.error = json_rpc_error( JSON_RPC_PARSE_ERROR, e.to_string(), fc::variant( *(e.dynamic_copy_exception()) ) );
      }
      catch( fc::exception& e )
      {
         response.error = json_rpc_error( JSON_RPC_SERVER_ERROR, e.to_string(), fc::variant( *(e.dynamic_copy_exception()) ) );
      }
      catch( std::exception& e )
      {
         response.error = json_rpc_error( JSON_RPC_SERVER_ERROR, "Unknown error - parsing rpc message failed", fc::variant( e.what() ) );
      }
      catch( ... )
      {
         response.error = json_rpc_error( JSON_RPC_SERVER_ERROR, "Unknown error - parsing rpc message failed" );
      }

      return response;
   }

   void json_rpc_plugin_impl::send_ws_notice( uint64_t id, string message){
      try{
         _subscribe_callbacks[id](message);
      }catch(...){
         _subscribe_callbacks.erase(id);
         fc::send_error_exception e;
         throw e;
      }
   }
}

using detail::json_rpc_error;
using detail::json_rpc_response;
using detail::json_rpc_logger;

json_rpc_plugin::json_rpc_plugin() : my( new detail::json_rpc_plugin_impl( *this ) ) {}
json_rpc_plugin::~json_rpc_plugin() {}

void json_rpc_plugin::set_program_options( options_description& , options_description& cfg)
{
   cfg.add_options()
      ("log-json-rpc", bpo::value< string >(), "json-rpc log directory name.")
      ;
}

void json_rpc_plugin::plugin_initialize( const variables_map& options )
{
   my->initialize();

   if( options.count( "log-json-rpc" ) )
   {
      auto dir_name = options.at( "log-json-rpc" ).as< string >();
      FC_ASSERT(dir_name.empty() == false, "Invalid directory name (empty).");

      fc::path p(dir_name);
      if (fc::exists(p))
         fc::remove_all(p);
      fc::create_directories(p);
      my->_logger.reset(new json_rpc_logger(dir_name));
   }
}

void json_rpc_plugin::plugin_startup()
{
   std::sort( my->_methods.begin(), my->_methods.end() );
}

void json_rpc_plugin::plugin_shutdown() {}

void json_rpc_plugin::add_api_method( const string& api_name, const string& method_name, const api_method& api, const api_method_signature& sig )
{
   my->add_api_method( api_name, method_name, api, sig );
}

void json_rpc_plugin::add_api_subscribe_method( const string& api_name, const string& method_name )
{
   my->add_api_subscribe_method( api_name, method_name );
}

string json_rpc_plugin::call( const string& message, bool& is_error)
{
   is_error = false;
   try
   {
      fc::variant v = fc::json::from_string( message );

      if( v.is_array() )
      {
         vector< fc::variant > messages = v.as< vector< fc::variant > >();
         vector< json_rpc_response > responses;

         if( messages.size() )
         {
            responses.reserve( messages.size() );

            for( auto& m : messages ){
               auto response = my->rpc( m, [](string s){} );
               if(response.error) {
                  is_error = true;
               }
               responses.push_back( std::move(response) );
            }


            return fc::json::to_string( responses );
         }
         else
         {
            //For example: message == "[]"
            json_rpc_response response;
            response.error = json_rpc_error( JSON_RPC_SERVER_ERROR, "Array is invalid" );
            is_error = true;
            return fc::json::to_string( response );
         }
      }
      else
      {
         const json_rpc_response response = my->rpc( v, [](string s){} );
         if(response.error) {
            is_error = true;
         }
         return fc::json::to_string( response );
      }
   }
   catch( fc::exception& e )
   {
      json_rpc_response response;
      response.error = json_rpc_error( JSON_RPC_SERVER_ERROR, e.to_string(), fc::variant( *(e.dynamic_copy_exception()) ) );
      is_error = true;
      return fc::json::to_string( response );
   }
   catch( ... )
   {
      json_rpc_response response;
      response.error = json_rpc_error( JSON_RPC_SERVER_ERROR, "Unknown exception", fc::variant(
         fc::unhandled_exception( FC_LOG_MESSAGE( warn, "Unknown Exception" ), std::current_exception() ).to_detail_string() ) );
      is_error = true;
      return fc::json::to_string( response );
   }

}


struct ws_notice{
   string method="notice";
   std::pair<uint64_t, std::vector<fc::variant>> params;
};

void json_rpc_plugin::send_ws_notice( uint64_t registration_id, uint64_t subscription_id, fc::variant& message ){
   ws_notice n;
   std::vector<fc::variant> array_message;
   array_message.push_back(message);
   n.params = std::make_pair(subscription_id, array_message);
   my->send_ws_notice( registration_id, fc::json::to_string(n));
}

string json_rpc_plugin::call( const string& message, std::function<void(const string&)> callback)
{
   try
   {
      fc::variant v = fc::json::from_string( message );

      if( v.is_array() )
      {
         vector< fc::variant > messages = v.as< vector< fc::variant > >();
         vector< json_rpc_response > responses;

         if( messages.size() )
         {
            responses.reserve( messages.size() );

            for( auto& m : messages )
               responses.push_back( my->rpc( m, callback ) );

            return fc::json::to_string( responses );
         }
         else
         {
            //For example: message == "[]"
            json_rpc_response response;
            response.error = json_rpc_error( JSON_RPC_SERVER_ERROR, "Array is invalid" );
            return fc::json::to_string( response );
         }
      }
      else
      {
         return fc::json::to_string( my->rpc( v, callback ) );
      }
   }
   catch( fc::exception& e )
   {
      json_rpc_response response;
      response.error = json_rpc_error( JSON_RPC_SERVER_ERROR, e.to_string(), fc::variant( *(e.dynamic_copy_exception()) ) );
      return fc::json::to_string( response );
   }
   catch( ... )
   {
      json_rpc_response response;
      response.error = json_rpc_error( JSON_RPC_SERVER_ERROR, "Unknown exception", fc::variant(
            fc::unhandled_exception( FC_LOG_MESSAGE( warn, "Unknown Exception" ), std::current_exception() ).to_detail_string() ) );
      return fc::json::to_string( response );
   }

}

fc::optional< fc::variant > json_rpc_plugin::call_api_method(const string& api_name, const string& method_name, const fc::variant& func_args) const {
   return my->call_api_method( api_name, method_name, func_args);
}


} } } // sophiatx::plugins::json_rpc

FC_REFLECT( sophiatx::plugins::json_rpc::detail::json_rpc_error, (code)(message)(data) )
FC_REFLECT( sophiatx::plugins::json_rpc::detail::json_rpc_response, (jsonrpc)(result)(error)(id) )

FC_REFLECT( sophiatx::plugins::json_rpc::detail::get_signature_args, (method) )
FC_REFLECT( sophiatx::plugins::json_rpc::ws_notice, (method)(params))
