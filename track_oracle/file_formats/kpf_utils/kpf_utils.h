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
 * @brief Utilities for reading and writing KPF in track_oracle.
 *
 * add_to_row() adds the KPF packet to the track_oracle row. Any
 * ad hoc fields are added with a specific naming convention which
 * is recognized by get_optional_fields().
 *
 * get_optional_fields() scans the track_oracle data columns (aka fields)
 * and returns a map of which fields correspond to KPF packets.
 *
 */

#ifndef INCL_KPF_UTILITIES_H
#define INCL_KPF_UTILITIES_H

#include <vital/vital_config.h>
#include <track_oracle/file_formats/kpf_utils/kpf_utils_export.h>

#include <track_oracle/core/track_oracle_api_types.h>
#include <track_oracle/utils/logging_map.h>

#include <arrows/kpf/yaml/kpf_packet.h>

#include <map>

namespace kwiver {
namespace track_oracle {
namespace kpf_utils {

namespace KPF=::kwiver::vital::kpf;

KPF_UTILS_EXPORT
std::map< field_handle_type, KPF::packet_t >
get_optional_fields();

KPF_UTILS_EXPORT
void
add_to_row( kwiver::logging_map_type& log_map,
            const oracle_entry_handle_type& row,
            const KPF::packet_t& p );

} // ...kpf_utils
} // ...track_oracle
} // ...kwiver

#endif
