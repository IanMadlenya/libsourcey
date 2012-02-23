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


#include "Sourcey/Anionu/APIClient.h"
#include "Sourcey/HTTP/Authenticator.h"
#include "Sourcey/Logger.h"

#include "Poco/Net/SSLManager.h"
#include "Poco/Net/KeyConsoleHandler.h"
#include "Poco/Net/ConsoleCertificateHandler.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/StreamCopier.h"
#include "Poco/Format.h"

#include <assert.h>


using namespace std;
using namespace Poco;
using namespace Poco::Net;


namespace Sourcey { 
namespace Anionu {


// ---------------------------------------------------------------------
//
// APIClient
//
// ---------------------------------------------------------------------
APIClient::APIClient()
{
	Log("debug") << "[APIClient] Creating" << endl;

//#ifndef _DEBUG		
	SharedPtr<InvalidCertificateHandler> ptrCert;
	Context::Ptr ptrContext = new Context(Context::CLIENT_USE, "", "", "", Context::VERIFY_NONE, 9, false, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");		
	SSLManager::instance().initializeClient(0, ptrCert, ptrContext);
//#endif //_DEBUG	

	//loadServices(false);
}


APIClient::~APIClient()
{
	Log("debug") << "[APIClient] Destroying" << endl;

	stopWorkers();
}


bool APIClient::loadServices(bool whiny)
{
	Log("debug") << "[APIClient] Loading Services" << endl;
	FastMutex::ScopedLock lock(_mutex);	
	try 
	{
		_services.update();
	} 
	catch (Exception& exc) 
	{
		if (whiny)
			throw exc;
	}
	
	return _services.valid();
}


bool APIClient::isOK()
{
	FastMutex::ScopedLock lock(_mutex);	
	return _services.valid();
}


APIServices& APIClient::services()
{
	FastMutex::ScopedLock lock(_mutex);
	return _services;
}


void APIClient::setCredentials(const string& username, const string& password) 
{ 
	FastMutex::ScopedLock lock(_mutex);
	_credentials.username = username;
	_credentials.password = password;
}


void APIClient::stopWorkers()
{
	Log("debug") << "[APIClient] Stopping Workers" << endl;
	
	FastMutex::ScopedLock lock(_mutex);

	for (vector<APITransaction*>::const_iterator it = _transactions.begin(); it != _transactions.end(); ++it) {	
		(*it)->TransactionComplete -= delegate(this, &APIClient::onTransactionComplete);
		(*it)->cancel();
	}
	_transactions.clear();
}


void APIClient::onTransactionComplete(void* sender, HTTP::Response&)
{
	Log("debug") << "[APIClient] Transaction Complete" << endl;

	FastMutex::ScopedLock lock(_mutex);
	
	for (vector<APITransaction*>::const_iterator it = _transactions.begin(); it != _transactions.end(); ++it) {	
		if (*it == sender) {
			Log("debug") << "[APIClient] Removing Transaction: " << sender << endl;
			_transactions.erase(it);

			// The transaction is responsible for it's own descruction.
			return;
		}
	}
}


APIRequest* APIClient::createRequest(const APIService& service)
{
	FastMutex::ScopedLock lock(_mutex);
	return new APIRequest(service, _credentials);
}

	
APIRequest* APIClient::createRequest(const std::string& service, 
									 const std::string& format, 
									 const StringMap& params)
{
	return createRequest(services().get(service, format, params));
}


APITransaction* APIClient::call(const std::string& service, 
								const std::string& format, 
								const StringMap& params)
{
	return call(createRequest(service, format, params));
}


APITransaction* APIClient::call(const APIService& service)
{
	return call(createRequest(service));
}


APITransaction* APIClient::call(APIRequest* request)
{
	Log("debug") << "[APIClient] Calling: " << request->service.name << endl;
	APITransaction* transaction = new APITransaction(request);
	transaction->TransactionComplete += delegate(this, &APIClient::onTransactionComplete);
	FastMutex::ScopedLock lock(_mutex);
	_transactions.push_back(transaction);
	return transaction;
}


AsyncTransaction* APIClient::callAsync(const std::string& service, 
									   const std::string& format, 
									   const StringMap& params)
{
	return callAsync(createRequest(service, format, params));
}


AsyncTransaction* APIClient::callAsync(const APIService& service)
{
	return callAsync(createRequest(service));
}


AsyncTransaction* APIClient::callAsync(APIRequest* request)
{
	Log("debug") << "[APIClient] Calling: " << request->service.name << endl;
	AsyncTransaction* transaction = new AsyncTransaction();
	transaction->setRequest(request);
	transaction->TransactionComplete += delegate(this, &APIClient::onTransactionComplete);
	FastMutex::ScopedLock lock(_mutex);
	_transactions.push_back(transaction);
	return transaction;
}


// ---------------------------------------------------------------------
//
// API Request
//
// ---------------------------------------------------------------------
void APIRequest::prepare()
{
	setMethod(service.method);
	setURI(service.uri.toString());
	HTTP::Request::prepare();
	set("User-Agent", "Anionu C++ API");
	if (!form &&
		service.method == "POST" || 
		service.method == "PUT")
		set("Content-Type", "application/xml");

	if (!service.isAnonymous) {
		set("Authorization", 
			HTTP::AnionuAuthenticator::generateAuthHeader(
				credentials.username,
				credentials.password, 
				service.method, 
				service.uri.toString(),
				getContentType(), 
				get("Date")
			)
		);
	}

	Log("debug") << "[APIRequest] Output: " << endl;
	write(cout);
}



// ---------------------------------------------------------------------
//
// API Service Description
//
// ---------------------------------------------------------------------
APIService::APIService() 
{
}


void APIService::interpolate(const StringMap& params) 
{
	string path = uri.getPath();
	for (StringMap::const_iterator it = params.begin(); it != params.end(); ++it) {	
		replaceInPlace(path, (*it).first, (*it).second);
	}
	uri.setPath(path);
}


void APIService::format(const string& format) 
{
	string path = uri.getPath();
	replaceInPlace(path, ":format", format.data());
	uri.setPath(path);
}


// ---------------------------------------------------------------------
//
// API Services
//
// ---------------------------------------------------------------------
APIServices::APIServices()
{
	Log("debug") << "[APIServices] Creating" << endl;
}


APIServices::~APIServices()
{
	Log("debug") << "[APIServices] Destroying" << endl;
}


bool APIServices::valid()
{
	return !empty() && !child("services").empty();
}


void APIServices::update()
{
	Log("debug") << "[APIServices] Updating" << endl;
	FastMutex::ScopedLock lock(_mutex); 	

	try 
	{
		string endpoint(ANIONU_API_ENDPOINT);
		replaceInPlace(endpoint, "https", "http");
		URI uri(endpoint + "/services.xml");
		HTTPClientSession session(uri.getHost(), uri.getPort());
		HTTPRequest req("GET", uri.getPathAndQuery(), HTTPMessage::HTTP_1_1);
		session.setTimeout(Timespan(5,0));
		session.sendRequest(req);
		HTTPResponse res;
		istream& rs = session.receiveResponse(res);
		Log("debug") << "[APIServices] " << res.getStatus() << " " << res.getReason() << endl;
		string xml;
		StreamCopier::copyToString(rs, xml);

		load(xml.data());

		//Log("debug") << "[APIServices] API Services XML:\n" << endl;
		//print(cout);
	} 
	catch (Exception& exc) 
	{
		Log("debug") << exc.displayText() << endl;
		exc.rethrow();
	} 
}


APIService APIServices::get(const string& name, const string& format, const StringMap& params)
{	
	Log("debug") << "[APIServices] Get: " << name << endl;
	FastMutex::ScopedLock lock(_mutex); 	
	APIService svc;

	try
	{
		// Attempt to update if invalid...
		if (!valid())
			update();

		// Throw if we fail...
		if (!valid())
			throw Exception("No service description available.");

		XML::Node node = select_single_node(Poco::format("//name[text()='%s']/..", name).data()).node();
		if (node.empty())
			throw Exception("No service description available for: " + name);

		// Parse the Name value
		svc.name = node.child("name").child_value();

		// Parse the Method value
		svc.method = node.child("method").child_value();

		// Parse the URI value
		svc.uri = URI(node.child("uri").child_value());

		// Set the URI from our config value in debug mode.
#ifdef _DEBUG 
		URI endpoint = URI(ANIONU_API_ENDPOINT);
		svc.uri.setScheme(endpoint.getScheme());
		svc.uri.setHost(endpoint.getHost());
		svc.uri.setPort(endpoint.getPort());
#endif

		// Parse the Anonymous value
		svc.isAnonymous = node.child("anonymous").child_value() == "true" ? true : false;

		// Update the format and interpolate parameters
		svc.format(format);
		svc.interpolate(params);

		Log("debug") << "[APIServices] get()\n"
			 << "Name: " << svc.name << "\n"
			 << "Method: " << svc.method << "\n"
			 << "Uri: " << svc.uri.toString() << "\n"
			 << "Anonymous: " << svc.isAnonymous << "\n"
			 << endl;
	}
	catch (Exception& exc)
	{
		Log("error") << "[APIServices] " << exc.displayText() << endl;
		exc.rethrow();
	}
	
	return svc;
}


// ---------------------------------------------------------------------
//
// API Transaction
//
// ---------------------------------------------------------------------
APITransaction::APITransaction(APIRequest* request) : 
	HTTP::Transaction(request)
{
	Log("debug") << "[APITransaction] Creating" << endl;
}


APITransaction::~APITransaction()
{
	Log("debug") << "[APITransaction] Destroying" << endl;
}


void APITransaction::processCallbacks()
{	
	if (!cancelled()) {
		TransactionComplete.dispatch(this, _response);
		APITransactionComplete.dispatch(this, static_cast<APIRequest*>(_request)->service, _response);
	}
}


} } // namespace Sourcey::Anionu