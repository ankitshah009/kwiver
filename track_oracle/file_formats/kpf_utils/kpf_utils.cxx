/*ckwg +29
 * Copyright 2017 by Kitware, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither name of Kitware, Inc. nor the names of any contributors may be used
 *    to endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// When looking in track_oracle's database for any optional fields
// to emit, we select anything whose field name matches either
//
// XXX_nnn, where 'XXX' can be converted to a KPF style, and nnn is an
// integer (interpreted as the domain), and the type is a double,
//
// OR
//
// key_KKK, where 'key' is the KPF style for a key/value pair; KKK is
// taken as the key, and the track_oracle entry is the value, and the type
// is a string.
//
// Another approach might be to store these packet headers when we read the
// KPF in, but somehow doing it just-in-time here feels better, if more
// convoluted.
//

//
// Whoops, we have to store a whole packet because the KV packet
// doesn't record the key in the header. Drat.
//

#include "kpf_utils.h"

#include <vital/logger/logger.h>
static kwiver::vital::logger_handle_t main_logger( kwiver::vital::get_logger( __FILE__ ) );

#include <track_oracle/core/track_oracle_core.h>
#include <track_oracle/core/track_field.h>
#include <track_oracle/data_terms/data_terms.h>

#include <kwiversys/RegularExpression.hxx>

#include <sstream>

using std::map;
using std::vector;
using std::string;
using std::ostream;
using std::ostringstream;

namespace KPF=::kwiver::vital::kpf;

namespace // anon
{

using namespace kwiver::track_oracle;

map< field_handle_type, KPF::packet_t >
get_optional_fields()
{
  map< field_handle_type, KPF::packet_t > optional_fields;
  string dbltype( typeid( double(0) ).name()), strtype( typeid( string("") ).name());
  string kpf_key_str( KPF::style2str( KPF::packet_style::KV ));
  // Note that these regexs are tied to the synthesized field names in the reader.
  kwiversys::RegularExpression field_dbl("^([a-zA-Z0-9]+)_([0-9]+)$");
  kwiversys::RegularExpression field_kv("^"+kpf_key_str+"_([a-zA-Z0-9]+)$");

  for (auto i: track_oracle_core::get_all_field_handles())
  {
    element_descriptor e = track_oracle_core::get_element_descriptor( i );

    // Is the type double?
    if (e.typeid_str == dbltype)
    {
      // did we find two matches and is the first a KPF style?
      if (field_dbl.find( e.name ))
      {
        auto style = KPF::str2style( field_dbl.match(1) );
        if (style != KPF::packet_style::INVALID )
        {
          // Convert the domain and associate a packet with the field
          optional_fields[i] = KPF::packet_t( KPF::packet_header_t( style, stoi( field_dbl.match(2) )));

        } // ...not a valid KPF domain
      } // ... didn't find two matches
    } // ...field is not a double

    // Is the type string?
    else if (e.typeid_str == strtype)
    {
      if (field_kv.find( e.name ))
      {
        // Create a partial KV packet with the key
        KPF::packet_t p( (KPF::packet_header_t( KPF::packet_style::KV )) );
        p.kv.key = field_kv.match(1);
        optional_fields[i] = p;

      } // ...didn't get exactly one match
    } // ... field is not a string
  } // ...for all fields in track_oracle

  return optional_fields;
}

} // anon

namespace kwiver {
namespace track_oracle {

namespace KPFC = ::kwiver::vital::kpf::canonical;

kpf_utils::optional_field_state
::optional_field_state( kwiver::logging_map_type& lmt )
  : first_pass( true ),
    optional_fields( get_optional_fields() ),
    log_map( lmt )
{
}


void
kpf_utils::add_to_row( kwiver::logging_map_type& log_map,
                       const oracle_entry_handle_type& row,
                       const KPF::packet_t& p )
{
  //
  // Step through the packet types and add them
  //
  switch (p.header.style)
  {
  case KPF::packet_style::ID:
    {
      unsigned id = p.id.d;
      switch (p.header.domain)
      {
      case KPFC::id_t::DETECTION_ID:
        {
          track_field< dt::detection::detection_id > x;
          x(row) = id;
        }
        break;
      case KPFC::id_t::TRACK_ID:
        {
          track_field< dt::tracking::external_id > x;
          x(row) = id;
        }
        break;
      case KPFC::id_t::EVENT_ID:
        {
          track_field< dt::events::event_id > x;
          x(row) = id;
        }
      default:
        {
          ostringstream oss;
          oss << "Ignoring ID domain " << p.header.domain;
          log_map.add_msg( oss.str() );
        }
      }
    }
    break;

  case KPF::packet_style::TS:
    {
      switch (p.header.domain)
      {
      case KPFC::timestamp_t::FRAME_NUMBER:
        {
          track_field< dt::tracking::frame_number > x;
          x(row) = static_cast< dt::tracking::frame_number::Type >( p.timestamp.d );
        }
        break;
      default:
        {
          ostringstream oss;
          oss << "Ignoring TS domain " << p.header.domain;
          log_map.add_msg( oss.str() );
        }
      }
    }
    break;
  case KPF::packet_style::GEOM:
    {
      switch (p.header.domain)
      {
      case KPFC::bbox_t::IMAGE_COORDS:
        {
          track_field< dt::tracking::bounding_box > x;
          x(row) = vgl_box_2d<double>(
            vgl_point_2d<double>( p.bbox.x1, p.bbox.y1 ),
            vgl_point_2d<double>( p.bbox.x2, p.bbox.y2 ));
        }
        break;
      default:
        {
          ostringstream oss;
          oss << "Ignoring TS domain " << p.header.domain;
          log_map.add_msg( oss.str() );
        }
      }
    }
    break;
  case KPF::packet_style::CONF:
    {
      // since KPF's domains for confidence are open-ended, we don't
      // hardwire this file's confidence values to fixed data_terms
      // the way we do with (for example) the IDs or bounding boxes--
      // that's the whole rationale for pre-defined domains, that we
      // can assert their semantic interpretation. Not so with conf packets,
      // so we'll just create their storage on-the-fly.


      // Note that this string needs to be kept in sync with the regex
      // used to pick it up in the writer.

      ostringstream oss;
      oss << KPF::style2str( p.header.style ) << "_" << p.header.domain;
      field_handle_type f = track_oracle_core::lookup_by_name( oss.str() );
      if ( f == INVALID_FIELD_HANDLE )
      {
        element_descriptor e( oss.str(),
                              "KPF ad-hoc",
                              typeid( double(0) ).name(),
                              element_descriptor::ADHOC );
        f = track_oracle_core::create_element< double >( e );
      }
      track_oracle_core::get_field<double>( row, f ) = p.conf.d;
    }
    break;
  case KPF::packet_style::KV:
    {
      // Like the conf values, but here we don't have a domain, and
      // the type is string, not double.

      // Note that this string needs to be kept in sync with the regex
      // used to pick it up in the writer.

      ostringstream oss;
      oss << KPF::style2str( p.header.style ) << "_" << p.kv.key;
      field_handle_type f = track_oracle_core::lookup_by_name( oss.str() );
      if ( f == INVALID_FIELD_HANDLE )
      {
        element_descriptor e( oss.str(),
                              "KPF ad-hoc",
                              typeid( string("") ).name(),
                              element_descriptor::ADHOC );
        f = track_oracle_core::create_element< string >( e );
      }
      track_oracle_core::get_field<string>( row, f ) = p.kv.val;
    }
    break;
  default:
    {
      ostringstream oss;
      oss << "Ignoring packet header " << p.header;
      log_map.add_msg( oss.str() );
    }
  };
}

vector< KPF::packet_t >
kpf_utils::optional_fields_to_packets( kpf_utils::optional_field_state& ofs,
                                       const oracle_entry_handle_type& row )
{
  vector< KPF::packet_t > ret;
  //
  // log what we're doing the first time through
  //

  if ( ofs.first_pass )
  {
    ofs.first_pass = false;
    for (auto i: ofs.optional_fields)
    {
      const KPF::packet_t& p = i.second;
      if (p.header.style == KPF::packet_style::KV)
      {
        LOG_INFO( main_logger, "KPF writer: adding optional KV " << p.kv.key );
      }
      else
      {
        LOG_INFO( main_logger, "KPF writer: adding optional " << p.header );
      }
    }
  }

  //
  // loop through all the fields defined on the row, convert the ones
  // we know how to handle
  //

  for (auto fh: track_oracle_core::fields_at_row( row ) )
  {
    auto probe = ofs.optional_fields.find( fh );
    if (probe == ofs.optional_fields.end())
    {
      continue;
    }

    //
    // hmm, awkward
    //

    KPF::packet_t p = probe->second;
    bool lost_value_flag = true;
    switch (p.header.style)
    {
    case KPF::packet_style::CONF:
      {
        auto v = track_oracle_core::get<double>( row, fh );
        if ( v.first )
        {
          p.conf.d = v.second;
          lost_value_flag = false;
        }
      }
      break;
    case KPF::packet_style::EVAL:
      {
        auto v = track_oracle_core::get<double>( row, fh );
        if ( v.first )
        {
          p.eval.d = v.second;
          lost_value_flag = false;
        }
      }
      break;
    case KPF::packet_style::KV:
      {
        auto v = track_oracle_core::get<string>( row, fh );
        if ( v.first )
        {
          p.kv.val = v.second;
          lost_value_flag = false;
        }
      }
      break;
    default:
      {
        lost_value_flag = false;
        ostringstream oss;
        oss << "No handler for optional packet " << p.header;
        ofs.log_map.add_msg( oss.str() );
      }
    }
    if (lost_value_flag)
    {
      LOG_ERROR( main_logger, "Lost value for " << p.header << "?" );
    }
    else
    {
      ret.push_back( p );
    }
  } // ...for all optional fields

  return ret;
}

void
kpf_utils::write_optional_packets( const vector< KPF::packet_t>& packets,
                                   kwiver::logging_map_type& log_map,
                                   KPF::record_yaml_writer& w )
{
  for (const auto& p: packets )
  {
    switch (p.header.style)
    {
    case KPF::packet_style::CONF:
      w << KPF::writer< KPFC::conf_t >( p.conf.d, p.header.domain );
      break;
    case KPF::packet_style::EVAL:
      w << KPF::writer< KPFC::eval_t >( p.eval.d, p.header.domain );
      break;
    case KPF::packet_style::KV:
      w << KPF::writer< KPFC::kv_t >( p.kv.key, p.kv.val );
      break;
    default:
      {
        ostringstream oss;
        oss << "No handler for optional packet " << p.header;
        log_map.add_msg( oss.str() );
      }
    }
  } // ...for all optional fields
}

} // ...track_oracle
} // ...kwiver
