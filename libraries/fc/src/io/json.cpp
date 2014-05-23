#include <fc/io/json.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/iostream.hpp>
#include <fc/io/buffered_iostream.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/sstream.hpp>
#include <fc/log/logger.hpp>
//#include <utfcpp/utf8.h>
#include <iostream>
#include <fstream>
#include <sstream>

namespace fc
{
   template<typename T>
   variant variant_from_stream( T& in );
   template<typename T>
   char parseEscape( T& in )
   {
      if( in.peek() == '\\' )
      {
         try {
            in.get();
            switch( in.peek() )
            {
               case 't':
                  in.get();
                  return '\t';
               case 'n':
                  in.get();
                  return '\n';
               case 'r':
                  in.get();
                  return '\r';
               case '\\':
                  in.get();
                  return '\\';
               default:
                  return in.get();
            }
         } FC_RETHROW_EXCEPTIONS( info, "Stream ended with '\\'" );
      }
	    FC_THROW_EXCEPTION( parse_error_exception, "Expected '\\'"  );
   }

   template<typename T>
   void skip_white_space( T& in )
   {
       while( true )
       {
          switch( in.peek() )
          {
             case ' ':
             case '\t':
             case '\n':
             case '\r':
                in.get();
                break;
             default:
                return;
          }
       }
   }

   template<typename T>
   fc::string stringFromStream( T& in )
   {
      fc::stringstream token;
      try
      {
         char c = in.peek();

         if( c != '"' )
            FC_THROW_EXCEPTION( parse_error_exception,
                                            "Expected '\"' but read '${char}'",
                                            ("char", string(&c, (&c) + 1) ) );
         in.get();
         while( true )
         {

            switch( c = in.peek() )
            {
               case '\\':
                  token << parseEscape( in );
                  break;
               case '"':
                  in.get();
                  return token.str();
               default:
                  token << c;
                  in.get();
            }
         }
         FC_THROW_EXCEPTION( parse_error_exception, "EOF before closing '\"' in string '${token}'",
                                          ("token", token.str() ) );
       } FC_RETHROW_EXCEPTIONS( warn, "while parsing token '${token}'",
                                          ("token", token.str() ) );
   }
   template<typename T>
   fc::string stringFromToken( T& in )
   {
      fc::stringstream token;
      try
      {
         char c = in.peek();

         while( true )
         {
            switch( c = in.peek() )
            {
               case '\\':
                  token << parseEscape( in );
                  break;
               case '\t':
               case ' ':
               case '\0':
               case '\n':
                  in.get();
                  return token.str();
               default:
                if( isalnum( c ) || c == '_' || c == '-' || c == '.' || c == ':' || c == '/' )
                {
                  token << c;
                  in.get();
                }
                else return token.str();
            }
         }
         return token.str();
      }
      catch( const fc::eof_exception& eof )
      {
         return token.str();
      }
      FC_RETHROW_EXCEPTIONS( warn, "while parsing token '${token}'",
                                          ("token", token.str() ) );
   }

   template<typename T>
   variant_object objectFromStream( T& in )
   {
      mutable_variant_object obj;
      try
      {
         char c = in.peek();
         if( c != '{' )
            FC_THROW_EXCEPTION( parse_error_exception,
                                     "Expected '{', but read '${char}'",
                                     ("char",string(&c, &c + 1)) );
         in.get();
         skip_white_space(in);
         while( in.peek() != '}' )
         {
            if( in.peek() == ',' )
            {
               in.get();
            }
            skip_white_space(in);
            string key = stringFromStream( in );
            skip_white_space(in);
            if( in.peek() != ':' )
            {
               FC_THROW_EXCEPTION( parse_error_exception, "Expected ':' after key \"${key}\"",
                                        ("key", key) );
            }
            in.get();
            auto val = variant_from_stream( in );

            obj(std::move(key),std::move(val));
            skip_white_space(in);
         }
         if( in.peek() == '}' )
         {
            in.get();
            return obj;
         }
         FC_THROW_EXCEPTION( parse_error_exception, "Expected '}' after ${variant}", ("variant", obj ) );
      }
      catch( const fc::eof_exception& e )
      {
         FC_THROW_EXCEPTION( parse_error_exception, "Unexpected EOF: ${e}", ("e", e.to_detail_string() ) );
      } FC_RETHROW_EXCEPTIONS( warn, "Error parsing object" );
   }

   template<typename T>
   variants arrayFromStream( T& in )
   {
      variants ar;
      try
      {
        if( in.peek() != '[' )
           FC_THROW_EXCEPTION( parse_error_exception, "Expected '['" );
        in.get();
        skip_white_space(in);

        while( in.peek() != ']' )
        {
           while( in.peek() == ',' )
              in.get();
           ar.push_back( variant_from_stream(in) );
           skip_white_space(in);
        }
        if( in.peek() != ']' )
           FC_THROW_EXCEPTION( parse_error_exception, "Expected ']' after parsing ${variant}",
                                    ("variant", ar) );

        in.get();
      } FC_RETHROW_EXCEPTIONS( warn, "Attempting to parse array ${array}",
                                         ("array", ar ) );
      return ar;
   }

   template<typename T>
   variant number_from_stream( T& in )
   {
      fc::stringstream ss;

      bool  dot = false;
      bool  neg = false;
      if( in.peek() == '-')
      {
        neg = true;
        ss.put( in.get() );
      }
      bool done = false;

      try
      {
        char c;
        while((c = in.peek()) && !done)
        {

          switch( c )
          {
              case '.':
                 if (dot)
                    FC_THROW_EXCEPTION(parse_error_exception, "Can't parse a number with two decimal places");
                 dot = true;
              case '0':
              case '1':
              case '2':
              case '3':
              case '4':
              case '5':
              case '6':
              case '7':
              case '8':
              case '9':
                 ss.put( in.get() );
                 break;
              default:
                 if( isalnum( c ) )
                 {
                    return ss.str() + stringFromToken( in );
                 }
                done = true;
                break;
          }
        }
      }
      catch (fc::eof_exception&)
      {
      }
      fc::string str = ss.str();
      if (str == "-." || str == ".") // check the obviously wrong things we could have encountered
        FC_THROW_EXCEPTION(parse_error_exception, "Can't parse token \"${token}\" as a JSON numeric constant", ("token", str));
      if( dot )
        return to_double(str);
      if( neg )
        return to_int64(str);
      return to_uint64(str);
   }
   template<typename T>
   variant token_from_stream( T& in )
   {
      std::stringstream ss;
      ss.exceptions( std::ifstream::badbit );
      bool received_eof = false;
      bool done = false;

      try
      {
        char c;
        while((c = in.peek()) && !done)
        {
           switch( c )
           {
              case 'n':
              case 'u':
              case 'l':
              case 't':
              case 'r':
              case 'e':
              case 'f':
              case 'a':
              case 's':
                 ss.put( in.get() );
                 break;
              default:
                 done = true;
                 break;
           }
        }
      }
      catch (fc::eof_exception&)
      {
        received_eof = true;
      }

      // we can get here either by processing a delimiter as in "null,"
      // an EOF like "null<EOF>", or an invalid token like "nullZ"
      fc::string str = ss.str();
      if( str == "null" )
        return variant();
      if( str == "true" )
        return true;
      if( str == "false" ) 
        return false;
      else
      {
        if (received_eof)
        {
          if (str.empty())
            FC_THROW_EXCEPTION( parse_error_exception, "Unexpected EOF" );
          else
            return str;
        }
        else
        {
          // if we've reached this point, we've either seen a partial
          // token ("tru<EOF>") or something our simple parser couldn't
          // make out ("falfe")
          // A strict JSON parser would signal this as an error, but we
          // will just treat the malformed token as an un-quoted string.
          return str + stringFromToken(in);;
        }
      }
   }


   template<typename T>
   variant variant_from_stream( T& in )
   {
      skip_white_space(in);
      variant var;
      while( char c = in.peek() )
      {
         switch( c )
         {
            case ' ':
            case '\t':
            case '\n':
            case '\r':
              in.get();
              continue;
            case '"':
              return stringFromStream( in );
            case '{':
              return objectFromStream( in );
            case '[':
              return arrayFromStream( in );
            case '-':
            case '.':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
              return number_from_stream( in );
            // null, true, false, or 'warning' / string
            case 'n':
            case 't':
            case 'f':
              return token_from_stream( in );
            case 0x04: // ^D end of transmission
            case EOF:
              FC_THROW_EXCEPTION( eof_exception, "unexpected end of file" );
            default:
            //  ilog( "unhandled char '${c}' int ${int}", ("c", fc::string( &c, 1 ) )("int", int(c)) );
              return stringFromToken(in);
              in.get(); //
              ilog( "unhandled char '${c}' int ${int}", ("c", fc::string( &c, 1 ) )("int", int(c)) );
              return variant();
         }
      }
	  return variant();
   }
   variant json::from_string( const std::string& utf8_str )
   {
      std::stringstream in( utf8_str );
      in.exceptions( std::ifstream::eofbit );
      return variant_from_stream( in );
   }

   /*
   void toUTF8( const char str, ostream& os )
   {
      // validate str == valid utf8
      utf8::replace_invalid( &str, &str + 1, ostream_iterator<char>(os) );
   }

   void toUTF8( const wchar_t c, ostream& os )
   {
      utf8::utf16to8( &c, (&c)+1, ostream_iterator<char>(os) );
   }
   */

   /**
    *  Convert '\t', '\a', '\n', '\\' and '"'  to "\t\a\n\\\""
    *
    *  All other characters are printed as UTF8.
    */
   void escape_string( const string& str, ostream& os )
   {
      os << '"';
      for( auto itr = str.begin(); itr != str.end(); ++itr )
      {
         switch( *itr )
         {
            case '\t':
               os << "\\t";
               break;
            case '\n':
               os << "\\n";
               break;
            case '\\':
               os << "\\\\";
               break;
            case '\r':
               os << "\\r";
               break;
            case '\a':
               os << "\\a";
               break;
            case '\"':
               os << "\\\"";
               break;
            default:
               os << *itr;
               //toUTF8( *itr, os );
         }
      }
      os << '"';
   }
   ostream& json::to_stream( ostream& out, const fc::string& str )
   {
        escape_string( str, out );
        return out;
   }

   template<typename T>
   void to_stream( T& os, const variants& a )
   {
      os << '[';
      auto itr = a.begin();

      while( itr != a.end() )
      {
         to_stream( os, *itr );
         ++itr;
         if( itr != a.end() )
            os << ',';
      }
      os << ']';
   }
   template<typename T>
   void to_stream( T& os, const variant_object& o )
   {
       os << '{';
       auto itr = o.begin();

       while( itr != o.end() )
       {
          escape_string( itr->key(), os );
          os << ':';
          to_stream( os, itr->value() );
          ++itr;
          if( itr != o.end() )
             os << ',';
       }
       os << '}';
   }

   template<typename T>
   void to_stream( T& os, const variant& v )
   {
      switch( v.get_type() )
      {
         case variant::null_type:
              os << "null";
              return;
         case variant::int64_type:
              os << v.as_int64();
              return;
         case variant::uint64_type:
              os << v.as_uint64();
              return;
         case variant::double_type:
              os << v.as_double();
              return;
         case variant::bool_type:
              os << v.as_string();
              return;
         case variant::string_type:
              escape_string( v.get_string(), os );
              return;
         case variant::array_type:
           {
              const variants&  a = v.get_array();
              to_stream( os, a );
              return;
           }
         case variant::object_type:
           {
              const variant_object& o =  v.get_object();
              to_stream(os, o );
              return;
           }
      }
   }

   fc::string   json::to_string( const variant& v )
   {
      fc::stringstream ss;
      fc::to_stream( ss, v );
      return ss.str();
   }


    fc::string pretty_print( const fc::string& v, uint8_t indent ) {
      int level = 0;
      fc::stringstream ss;
      bool first = false;
      bool quote = false;
      bool escape = false;
      for( uint32_t i = 0; i < v.size(); ++i ) {
         switch( v[i] ) {
            case '\\':
              if( !escape ) {
                if( quote )
                  escape = true;
              } else { escape = false; }
              ss<<v[i];
              break;
            case ':':
              if( !quote ) {
                ss<<": ";
              } else {
                ss<<':';
              }
              break;
            case '"':
              if( first ) {
                 ss<<'\n';
                 for( int i = 0; i < level*indent; ++i ) ss<<' ';
                 first = false;
              }
              if( !escape ) {
                quote = !quote;
              }
              escape = false;
              ss<<'"';
              break;
            case '{':
            case '[':
              ss<<v[i];
              if( !quote ) {
                ++level;
                first = true;
              }else {
                escape = false;
              }
              break;
            case '}':
            case ']':
              if( !quote ) {
                if( v[i-1] != '[' && v[i-1] != '{' ) {
                  ss<<'\n';
                }
                --level;
                if( !first ) {
                  for( int i = 0; i < level*indent; ++i ) ss<<' ';
                }
                first = false;
                ss<<v[i];
                break;
              } else {
                escape = false;
                ss<<v[i];
              }
              break;
            case ',':
              if( !quote ) {
                ss<<',';
                first = true;
              } else {
                escape = false;
                ss<<',';
              }
              break;
            default:
              if( first ) {
                 ss<<'\n';
                 for( int i = 0; i < level*indent; ++i ) ss<<' ';
                 first = false;
              }
              ss << v[i];
         }
      }
      return ss.str();
    }



   fc::string   json::to_pretty_string( const variant& v )
   {
	   return pretty_print(to_string(v), 2);
   }

   void          json::save_to_file( const variant& v, const fc::path& fi, bool pretty )
   {
      if( pretty )
      {
        auto str = json::to_pretty_string( v );
        fc::ofstream o(fi);
        o.write( str.c_str(), str.size() );
      }
      else
      {
       fc::ofstream o(fi);
       fc::to_stream( o, v );
      }
   }
   variant json::from_file( const fc::path& p )
   {
      //auto tmp = std::make_shared<fc::ifstream>( p, ifstream::binary );
      //auto tmp = std::make_shared<std::ifstream>( p.generic_string().c_str(), std::ios::binary );
      //buffered_istream bi( tmp );
      std::ifstream bi( p.generic_string().c_str(), std::ios::binary );
      return variant_from_stream( bi  );
   }
   variant json::from_stream( buffered_istream& in )
   {
      return variant_from_stream( in  );
   }

   ostream& json::to_stream( ostream& out, const variant& v )
   {
      fc::to_stream( out, v );
      return out;
   }
   ostream& json::to_stream( ostream& out, const variants& v )
   {
      fc::to_stream( out, v );
      return out;
   }
   ostream& json::to_stream( ostream& out, const variant_object& v )
   {
      fc::to_stream( out, v );
      return out;
   }

   bool     json::is_valid( const std::string& utf8_str )
   {
      if( utf8_str.size() == 0 ) return false;
      fc::stringstream in( utf8_str );
      variant_from_stream( in );
      try { in.peek(); } catch ( const eof_exception& e ) { return true; }
      return false;
   }


} // fc
