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

/**
 * @file
 * @brief Implementation of KPF activities
 *
 */

#include "track_filter_kpf_activity.h"

#include <vital/logger/logger.h>
static kwiver::vital::logger_handle_t main_logger( kwiver::vital::get_logger( __FILE__ ) );

#include <arrows/kpf/yaml/kpf_reader.h>
#include <arrows/kpf/yaml/kpf_yaml_writer.h>
#include <arrows/kpf/yaml/kpf_yaml_parser.h>
#include <arrows/kpf/yaml/kpf_canonical_io_adapter.h>

#include <track_oracle/utils/logging_map.h>
#include <track_oracle/file_formats/kpf_utils/kpf_utils.h>

#include <yaml-cpp/yaml.h>

#include <iostream>
#include <fstream>
#include <sstream>

using std::map;
using std::make_pair;
using std::string;
using std::ifstream;
using std::ostringstream;
using std::vector;

namespace KPF=::kwiver::vital::kpf;

namespace // anon
{

using namespace kwiver::track_oracle;

bool
build_lookup_map( const track_handle_list_type& ref_tracks,
                  map< unsigned, track_handle_type >& lookup_map )
{
  track_field< dt::tracking::external_id > id_field;
  for (auto i=0; i<ref_tracks.size(); ++i)
  {
    auto probe = id_field.get( ref_tracks[i].row );
    if ( ! probe.first )
    {
      LOG_ERROR( main_logger, "KPF activity reference track without ID1?" );
      return false;
    }
    auto insert_test = lookup_map.insert( make_pair( probe.second, ref_tracks[i] ));
    if ( ! insert_test.second )
    {
      LOG_ERROR( main_logger, "KPF activity: duplicate track IDs " << probe.second );
      return false;
    }
  }
  return true;
}

struct kpf_actor_track_type: public track_base< kpf_actor_track_type >
{
  // empty, just exists to allow frames to be created on it
};

struct kpf_act_exception
{
  explicit kpf_act_exception( const string& msg ): what(msg) {}
  string what;
};


} // anon namespace

namespace kwiver {
namespace track_oracle {

bool
track_filter_kpf_activity
::read( const std::string& fn,
        const track_handle_list_type& ref_tracks,
        int kpf_activity_domain,
        track_handle_list_type& new_tracks )
{
  logging_map_type wmsgs( main_logger, KWIVER_LOGGER_SITE );
  bool all_okay = true;

  try
  {
    ifstream is( fn.c_str() );
    if ( ! is )
    {
      throw kpf_act_exception( "Couldn't open KPF activity file '"+fn+"'");
    }

    LOG_INFO( main_logger, "KPF activity YAML load start");
    KPF::kpf_yaml_parser_t parser( is );
    KPF::kpf_reader_t reader( parser );
    LOG_INFO( main_logger, "KPF activity YAML load end");

    map< unsigned, track_handle_type > lookup_table;
    if ( ! build_lookup_map( ref_tracks, lookup_table ))
    {
      throw kpf_act_exception("id->track handle lookup failure");
    }

    track_field< dt::tracking::frame_number > fn_field;
    kpf_actor_track_type actor_track_instance;
    track_filter_kpf_activity act_schema;

    //
    // process each line
    //

    while ( reader.next() )
    {
      namespace KPFC = ::kwiver::vital::kpf::canonical;

      //
      // Special handling for the actN field
      //

      auto activity_probe = reader.transfer_packet_from_buffer(
        KPF::packet_header_t( KPF::packet_style::ACT, kpf_activity_domain ));

      if ( ! activity_probe.first )
      {
        ostringstream oss;
        oss << "Skipping non-metadata record with no activity in domain " << kpf_activity_domain;
        wmsgs.add_msg( oss.str() );
        continue;
      }

      //
      // for each actor, clone over the track and geometry within its time window
      //

      vector< unsigned > missing;
      track_handle_list_type actor_tracks;
      const KPFC::activity_t& kpf_act = activity_probe.second.activity;

      for (auto const& a: kpf_act.actors)
      {
        unsigned id = a.id.d;
        auto id_probe = lookup_table.find( id );

        //
        // remember and log any missing actors in the ref set
        //
        if (id_probe == lookup_table.end())
        {
          missing.push_back( id );
          continue;
        }

        //
        // get the actor's timespan in frame numbers
        //
        auto tsr_probe = find_if( a.actor_timespan.begin(), a.actor_timespan.end(),
                                  [](const KPFC::activity_t::scoped_tsr_t& t )
                                  { return t.domain == KPFC::timestamp_t::FRAME_NUMBER; } );
        if (tsr_probe == a.actor_timespan.end())
        {
          ostringstream oss;
          oss << "Actor has no timespan in frames? " << activity_probe.second;
          throw kpf_act_exception( oss.str() );
        }

        // clone non-system fields
        track_handle_type new_track( track_oracle_core::get_next_handle() );
        if ( ! track_oracle_core::clone_nonsystem_fields( id_probe->second, new_track ))
        {
          ostringstream oss;
          oss << "Couldn't clone non-system track fields for " << id << "?";
          throw kpf_act_exception( oss.str() );
        }

        // copy over frames in the actor's time window
        wmsgs.add_msg( "Selecting relevant actor frames based on frame_number only" );
        for (const auto& src_frame: track_oracle_core::get_frames( id_probe->second ))
        {
          auto fn_probe = fn_field.get( src_frame.row );
          if (! fn_probe.first)
          {
            ostringstream oss;
            oss << "No frame number for frame in track " << id << "?";
            throw kpf_act_exception( oss.str() );
          }
          auto frame_num = fn_probe.second;

          // is the frame within the time window?
          if ((tsr_probe->tsr.start <= frame_num) && (frame_num <= tsr_probe->tsr.stop))
          {
            //
            // create a frame on the new track and clone the fields
            //
            frame_handle_type new_frame = actor_track_instance( new_track ).create_frame();
            if ( ! track_oracle_core::clone_nonsystem_fields( src_frame, new_frame ))
            {
              ostringstream oss;
              oss << "Couldn't clone non-system fields for track / frame " << id << " / " << frame_num << "?";
              throw kpf_act_exception( oss.str() );
            }
          } // ... within time window
        } // ...for all source frames

        actor_tracks.push_back( new_track );

      } // ...for all actor tracks

      //
      // Did we miss anybody?
      //

      if ( ! missing.empty() )
      {
        ostringstream oss;
        oss << "Activity " << activity_probe.second << " missing the following tracks:";
        for (auto m: missing) oss << " " << m;
        throw kpf_act_exception( oss.str() );
      }

      //
      // okay then-- we can finally create the activity result
      //

      track_handle_type k = act_schema.create();

      act_schema.activity_label() = kpf_act.activity_name;
      act_schema.activity_id() = kpf_act.activity_id.d;
      act_schema.activity_domain() = kpf_activity_domain;
      act_schema.actors() = actor_tracks;

      //
      // set any attributes
      //
      for (auto const& kv: kpf_act.attributes)
      {
        // hmm, need to rebuild the kv_t into a packet to get into kpf_utils -- annoying
        KPF::packet_t p( (KPF::packet_header_t( KPF::packet_style::KV )));
        p.kv = kv;
        kpf_utils::add_to_row( wmsgs, k.row, p );
      }

      //
      // ...and anything else in the row
      //
      for (auto const& p: reader.get_packet_buffer() )
      {
        kpf_utils::add_to_row( wmsgs, k.row, p.second );
      }

      new_tracks.push_back( k );

    } // ...for each record in the file

  } // .. try
  catch (const kpf_act_exception& e )
  {
    LOG_ERROR( main_logger, "Error loading KPF activity file '" << fn << "': " << e.what );
    all_okay = false;
  }

  //
  // anything to report?
  //

  if (! wmsgs.empty() )
  {
    LOG_INFO( main_logger, "KPF act reader: warnings begin" );
    wmsgs.dump_msgs();
    LOG_INFO( main_logger, "KPF act reader: warnings end" );
  }

  return all_okay;

}

} // ...track_oracle
} // ...kwiver
