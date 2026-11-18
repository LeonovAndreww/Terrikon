// Copyright (C) 2003-2024 VLC authors and VideoLAN
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// chapter_command_script.cpp : Matroska Script Codec for Matroska Chapter Codecs
// Authors: Laurent Aimar <fenrir@via.ecp.fr>
//          Steve Lhomme <steve.lhomme@free.fr>


#include "chapter_command_script.hpp"
#include "virtual_segment.hpp"
#include <stdexcept>

namespace mkv {

#define M_MS_MAX_DEPTH 8
//Matroska Script
const std::string matroska_script_interpretor_c::CMD_MS_GOTO_AND_PLAY = "GotoAndPlay";

// see http://www.matroska.org/technical/specs/chapters/index.html#mscript
//  for a description of existing commands
bool matroska_script_interpretor_c::Interpret( MatroskaChapterProcessTime time, const binary * p_command, size_t i_size )
{
    static thread_local int n_call = 0;
    if( n_call > M_MS_MAX_DEPTH )
      return false;
    /* the depth comes back down on every way out */
    struct depth_guard {
        int &n;
        explicit depth_guard( int &n ) : n( n ) { n++; }
        ~depth_guard() { n--; }
    } guard( n_call );

    bool b_result = false;

    std::string sz_command( reinterpret_cast<const char*> (p_command), i_size );

    vlc_debug( l, "command : %s", sz_command.c_str() );

    if ( sz_command.compare( 0, CMD_MS_GOTO_AND_PLAY.size(), CMD_MS_GOTO_AND_PLAY ) == 0 )
    {
        // GotoAndPlay( uid ): spaces allowed before the (, nothing else
        const size_t open = sz_command.find_first_not_of( ' ', CMD_MS_GOTO_AND_PLAY.size() );
        if ( open == std::string::npos || sz_command[open] != '(' )
            return false;
        const size_t close = sz_command.find( ')', open + 1 );
        if ( close == std::string::npos || close <= open + 1 )
            return false;

        {
            std::string st = sz_command.substr( open + 1, close - open - 1 );
            chapter_uid i_chapter_uid;
            try {
                size_t used = 0;
                i_chapter_uid = std::stoull( st, &used );
                if ( st.find_first_not_of( ' ', used ) != std::string::npos )
                    throw std::invalid_argument( "trailing characters" );
            } catch ( const std::exception & ) {
                // what a file says need not be a number
                vlc_debug( l, "Invalid chapter uid in a script command" );
                return false;
            }

            virtual_segment_c *p_vsegment;
            virtual_chapter_c *p_vchapter = vm.FindVChapter( i_chapter_uid, p_vsegment );

            if ( p_vchapter == NULL )
                vlc_debug( l, "Chapter %" PRId64 " not found", i_chapter_uid);
            else
            {
                auto current_chapter = vm.GetCurrentVSegment()->CurrentChapter();
                if ( p_vchapter == current_chapter) {
                    if ( time == MATROSKA_CHAPPROCESSTIME_BEFORE )
                        // the enter command is enter itself, avoid infinite loop
                        return false;
                    vm.JumpTo( *p_vsegment, *p_vchapter );
                }
                else if ( !p_vchapter->EnterAndLeave( current_chapter, false ) )
                    vm.JumpTo( *p_vsegment, *p_vchapter );
                b_result = true;
            }
        }
    }

    return b_result;
}

} // namespace
