/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "historynamecache.h"
#include "../manager.h"

HistoryNameCache::HistoryNameCache() : hNameCache_()
{
    using std::vector;
    using std::map;
    using std::string;

    typedef vector<map<string, string> > HistoryList;
    HistoryList history(Manager::instance().getHistory());
    for (HistoryList::iterator i = history.begin(); i != history.end(); ++i) {
        string name((*i)["display_name"]);
        string account((*i)["accountid"]);
        string number((*i)["peer_number"]);
        if (hNameCache_[account][number].empty() and not name.empty() and not number.empty())
            hNameCache_[account][number] = name;
    }
    getNameFromHistory("", "");
}

HistoryNameCache& HistoryNameCache::getInstance()
{
    // Meyer singleton
    static HistoryNameCache instance_;
    return instance_;
}

void HistoryNameCache::serialize(Conf::YamlEmitter &emitter UNUSED)
{
    //TODO
}

void HistoryNameCache::unserialize(const Conf::MappingNode &map UNUSED)
{
    //TODO
}

std::string HistoryNameCache::getNameFromHistory(const std::string &number, const std::string &accountid)
{
    return hNameCache_[accountid][number];
}