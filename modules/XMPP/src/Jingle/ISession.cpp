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


#include "Sourcey/XMPP/Jingle/ISession.h"
#include "Sourcey/XMPP/Jingle/SessionManager.h"
#include "Sourcey/CryptoProvider.h"
#include "Sourcey/Logger.h"


using namespace std;


namespace Sourcey {
namespace XMPP { 
namespace Jingle {


// ---------------------------------------------------------------------
//
// Session
//
// ---------------------------------------------------------------------
ISession::ISession(SessionManager& manager,
				   const string& initiator, 
				   const string& responder, 
				   const string& sid) :
	_manager(manager),
	_initiator(initiator),
	_responder(responder),
	_sid(sid),
	_destroySources(true)
{
	Log("debug") << "[Jingle Session] Created: " << this << endl;
	if (_sid.empty())
		_sid = CryptoProvider::generateRandomKey(16);
}


ISession::ISession(SessionManager& manager, const Jingle& j) :
	_manager(manager),
	_initiator(j.initiator()),
	_responder(j.responder()),
	_sid(j.sid()),
	_destroySources(true)
{
	Log("debug") << "[Jingle Session] Created: " << this << endl;
}


ISession::~ISession() 
{
	Log("debug") << "[Jingle Session] Destroying" << endl;

	// Sessions should not be deleted manually as they are managed 
	// by the Session Manager. They must be terminate()'d before 
	// destruction.
	assert(stateEquals(SessionState::Terminating));
	removeMediaSources();
	//Util::ClearVector(_sources);
	//_manager.removeSession(_sid);
	Log("debug") << "[Jingle Session] Destroying: OK" << endl;
}


void ISession::addMediaSource(PacketDispatcher* source) 
{ 
	_sources.push_back(source); 
}


void ISession::removeMediaSources()
{
	Log("debug") << "[Jingle Session:" << this << "] Removing Media Sources: " << _destroySources << endl;
	if (_destroySources)
		Util::ClearVector(_sources);
	else
		_sources.clear();
}


void ISession::print(std::ostream& ost)
{
	ost << "[Jingle Session] " << sid() << ": " << state() << endl;
}


/*
PacketDispatcher* ISession::getMediaSource(const std::string& name) 
{
	for (PacketBroadcasterList::const_iterator it = _sources.begin(); it != _sources.end(); ++it) {
		if ((*it)->name() == name) {
			return *it;
		}
	}

	return NULL;
}
*/


} // namespace Jingle
} // namespace XMPP 
} // namespace Sourcey 