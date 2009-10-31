// Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

// $Id$

#include <stdexcept>
#include <stdlib.h>
#include <string.h>

#include <iostream>

#include <cc/cpp/data.h>
#include <cc/cpp/session.h>

#include "common.h"
#include "ccsession.h"

using namespace std;

using ISC::Data::Element;
using ISC::Data::ElementPtr;

CommandSession::CommandSession() :
    session_(ISC::CC::Session())
{
    try {
        session_.establish();
        session_.subscribe("ParkingLot");
        session_.subscribe("Boss");
        session_.subscribe("statistics");
    } catch (...) {
        throw std::runtime_error("SessionManager: failed to open sessions");
    }
}

int
CommandSession::getSocket()
{
    return (session_.getSocket());
}

std::pair<std::string, std::string>
CommandSession::getCommand(int counter) {
    ElementPtr cmd, routing, data, ep;
    string s;

    session_.group_recvmsg(routing, data, false);
    string channel = routing->get("group")->string_value();

    if (channel == "statistics") {
        cmd = data->get("command");
        if (cmd != NULL && cmd->string_value() == "getstat") {
            struct timeval now;
            ElementPtr resp = Element::create(std::map<std::string,
                                              ElementPtr>());
            gettimeofday(&now, NULL);
            resp->set("sent", Element::create(now.tv_sec +
                                              (double)now.tv_usec /
                                              1000000));
            resp->set("counter", Element::create(counter));
            session_.group_sendmsg(resp, "statistics");
        }
    } else {
        cmd = data->get("command");
        if (cmd != NULL) {
            ep = cmd->get(0);
            if (ep != NULL) {
                s = ep->string_value();
                if (s == "addzone" || s == "delzone") {
                    return std::pair<string, string>(s,
                                                     cmd->get(1)->string_value());
                }
                return std::pair<string, string>(s, "");
            }
        }
    }

    return std::pair<string, string>("unknown", "");
}
