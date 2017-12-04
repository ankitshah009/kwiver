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
 * @brief The track_oracle filter for KPF activities.
 *
 * Like KWE, a KPF activity file:
 *
 * - does not contain any geometry; instead it acts as a "track view" into
 *   the actor tracks. Since we don't yet support track views, we clone
 *   the source tracks.
 *
 * - Does not support the generic read() interface
 *
 * UNLIKE KWE, however, 

 */

#ifndef INCL_TRACK_FILTER_KWE_H
#define INCL_TRACK_FILTER_KWE_H

#include <vital/vital_config.h>
#include <track_oracle/track_filter_kwe/track_filter_kwe_export.h>

#include <track_oracle/event_adapter.h>

/// This schema actually is the event_adapter.h::event_data_schema
/// with a read() method added on to it.
///
/// This is the first example of a "track filter", which is
/// information contained in a file that refers to tracks, adds
/// information to tracks, but does not itself contain track geometry.
/// This gets handled specially because loading this file by itself
/// will NOT result in tracks.  In particular, you need to have
/// already loaded the tracks that the filter refers to.
///
/// This particular filter is the legacy event format for Kitware
/// event detectors.  It is an exceptionally brittle format, mostly
/// because the event type is encoded as an arbitrary enum over in
/// event_types.h .
///
/// Other peculiarities of KWE files and scoring:
///
/// -- KWE files can record vidtk events, but we (at the moment) we
/// only score VIRAT events.  The conversion is handled in
/// aries_interface.
///
/// -- KWEs do not contain geometry but instead references to subsets
/// of other tracks, i.e. they are track views.  Since track_oracle
/// doesn't yet support track views, we clone the subsets of the
/// source tracks into new tracks, based on the track IDs.  When we clone,
/// we clone all the non-system fields.
///
/// -- Since this is a filter, and not a format, it's not handled by
/// the file format manager's generic read() interface.  Instead,
/// supporting the clone operation described above requires passing
/// in the set of tracks which the KWE refers to.  (This could also
/// be handled by domains.)
///

namespace kwiver {
namespace track_oracle {

struct TRACK_FILTER_KWE_EXPORT track_filter_kwe_type: public event_data_schema
{
  static bool read( const std::string& fn,
                    const track_handle_list_type& ref_tracks,
                    const std::string& track_style,
                    track_handle_list_type& new_tracks );
};

} // ...track_oracle
} // ...kwiver


#endif
