//
// This software is copyright by Sourcey <mail@sourcey.com> and is distributed under a dual license:
// Copyright (C) 2002 Sourcey
//
// Non-Commercial Use:
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// Commercial Use:
// Please contact mail@sourcey.com
//


#ifndef SOURCEY_NET_Types_H
#define SOURCEY_NET_Types_H


#include "Sourcey/Stateful.h"


namespace Sourcey {	
namespace Net {
	

const int MAX_TCP_PACKET_SIZE = 64 * 1024;
const int MAX_UDP_PACKET_SIZE = 1500;


enum TransportProtocol 
{
	UDP,
	TCP,
	SSLTCP
};


struct Error 
{
	std::string message;
	int error;
};


} } // namespace Sourcey::Net


#endif


