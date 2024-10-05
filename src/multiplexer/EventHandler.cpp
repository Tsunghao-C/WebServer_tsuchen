/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventHandler.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bthomas <bthomas@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/09/24 12:20:55 by bthomas           #+#    #+#             */
/*   Updated: 2024/10/05 13:15:59 by bthomas          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "EventHandler.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include <string>
#include <unistd.h>
#include <vector>

void cgiOut(int clientFd, char **av, char **env);

class EventHandler::epollInitFailure : public std::exception {
	public:
		const char* what() const throw() {
			return "Error: could not initialise epoll\n";
		}
};

class EventHandler::epollWaitFailure : public std::exception {
	public:
		const char* what() const throw() {
			return "Error: epoll_wait failed\n";
		}
};

EventHandler::EventHandler()
{
	_epollFd = epoll_create1(0);
	if (_epollFd == -1) {
		throw epollInitFailure();
	}
}

EventHandler::~EventHandler()
{
	if (_epollFd != -1) {
		close(_epollFd);
	}
	std::map<int, ClientConnection*>::iterator it;
	for (it = _clients.begin(); it != _clients.end(); ++it) {
		delete it->second;
	}
	_clients.clear();
}

void EventHandler::setNonBlock(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		std::cerr << "Error: couldn't set fd to non-blocking\n";
		return ;
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		std::cerr << "Error: couldn't set fd to non-blocking\n";
	}
}

void EventHandler::changeToRead(int clientFd) {
	struct epoll_event ev;
	ev.data.fd = clientFd;
	ev.events = EPOLLIN;
	if (epoll_ctl(_epollFd, EPOLL_CTL_MOD, clientFd, &ev) == -1) {
		std::cerr << "Error: could not mark fd " << clientFd << " as EPOLLIN\n";
	}
}

void EventHandler::changeToWrite(int clientFd) {
	struct epoll_event ev;
	ev.data.fd = clientFd;
	ev.events = EPOLLOUT;
	if (epoll_ctl(_epollFd, EPOLL_CTL_MOD, clientFd, &ev) == -1) {
		std::cerr << "Error: could not mark fd " << clientFd << " as EPOLLOUT\n";
	}
}

bool EventHandler::addToEpoll(int fd) {
	setNonBlock(fd);
	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = EPOLLIN;
	if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &ev) == -1) {
		std::cerr << "Error: could not mark fd " << fd << " as EPOLLIN\n";
		return false;
	}
	return true;
}

bool EventHandler::deleteFromEpoll(int fd) {
	if (epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL) == -1) {
		std::cerr << "Error: could not delete fd " << fd << "\n";
		return false;
	}
	return true;
}

void EventHandler::addClient(int clientFd, const Config &config) {
	ClientConnection* conn = new ClientConnection(clientFd, config);
	_clients[clientFd] = conn;
	_openConns[clientFd] = EP_CLIENT;
}

void EventHandler::addServer(Server & s) {
	int serverFd = s.getSockFd();
	std::map<int, Server*>::iterator it = _servers.find(serverFd);
	if (it == _servers.end()) {
		if (!addToEpoll(serverFd)) {
			return ;
		}
		_servers[serverFd] = &s;
		_openConns[serverFd] = EP_SERVER;
	}
}

void EventHandler::handleNewConnection(Server & s) {
	int serverFd = s.getSockFd();
	std::cout << "New connection from " << serverFd << "\n";

	struct sockaddr_in clientAddr;
	socklen_t clientAddrLen = sizeof(clientAddr);

	int clientFd = accept(serverFd, (struct sockaddr *)&clientAddr, &clientAddrLen);
	if (clientFd == -1) {
		std::cerr << "Error: could not accept new connection from fd " << serverFd << "\n";
		return ;
	}
	if (!addToEpoll(clientFd)) {
		close(clientFd);
		return ;
	}
	addClient(clientFd, s.getConfig());
}

bool EventHandler::isResponseComplete(int clientFd) {
	// check header completion
	std::string buff = _clients[clientFd]->_requestBuffer;
	std::string::size_type pos = getHeaderEndPos(clientFd);
	if (pos == std::string::npos) {
		return false;
	}

	if (isHeaderChunked(clientFd)) {
		_clients.at(clientFd)->_reqType = (ClientConnection::reqType)CHUNKED;
	}
	
	//Check if it's a POST request with a body
	if (_clients.at(clientFd)->_reqType == (ClientConnection::reqType)CHUNKED) {
		return isChunkReqFinished(clientFd);
	}
	size_t content_len_pos = buff.find("Content-Length: ");
	if (content_len_pos != std::string::npos) {
		size_t content_len_end = buff.find("\r\n", content_len_pos);
		std::string content_len_str = buff.substr(content_len_pos + 16, content_len_end - (content_len_pos + 16));
		int content_length = std::atoi(content_len_str.c_str());
		return buff.length() >= (pos + 4 + content_length);
	}
	return true;
}

std::string::size_type EventHandler::getHeaderEndPos(int clientFd) {
	std::string reqBuffer = _clients.at(clientFd)->_requestBuffer;
	std::string::size_type headerEnd = reqBuffer.find("\r\n\r\n");
	if (headerEnd == std::string::npos) {
		headerEnd = reqBuffer.find("\n\n");
		if (headerEnd == std::string::npos) {
			return std::string::npos;
		}
		headerEnd += 2;
	} else {
		headerEnd += 4;
	}
	return headerEnd;
}

bool EventHandler::isBodyTooBig(int clientFd, int bytes_read) {
	std::string::size_type headerEndPos = getHeaderEndPos(clientFd);
	if (headerEndPos == std::string::npos)
		return false;
	uint64_t maxBodySize = _clients.at(clientFd)->_config.get_max_body_size();
	std::string::size_type currentBodySize;
	currentBodySize = _clients.at(clientFd)->_requestBuffer.size() - headerEndPos;
	if (currentBodySize + bytes_read > maxBodySize) {
		return true;
	}
	return false;
}

void EventHandler::sendInvalidResponse(int clientFd) {
	std::cerr << "Error: body size limit reached.\n";
	_clients.at(clientFd)->resetData();
	// change request buffer to cause 504
	_clients.at(clientFd)->_errorCode = 504;
	generateResponse(clientFd);
	changeToWrite(clientFd);
}

// Read all data from the client
void EventHandler::handleClientRequest(int clientFd) {
	char buffer[BUFFER_SIZE];
	int bytes_read;

	std::cout << "Handling client request from " << clientFd << "\n";
	bytes_read = read(clientFd, buffer, sizeof(buffer) - 1);
	if (bytes_read <= 0) {
		std::cerr << "Error: failed to read or client closed connection.\n";
		deleteFromEpoll(clientFd);
		delete _clients.at(clientFd);
		_openConns.erase(clientFd);
		_clients.erase(clientFd);
		return ;
	}
	buffer[bytes_read] = 0;
	if (isBodyTooBig(clientFd, bytes_read)) {
		// send 504.
		sendInvalidResponse(clientFd);
		return ;
	}
	_clients.at(clientFd)->_requestBuffer.append(buffer, _clients.at(clientFd)->_requestBuffer.size(), bytes_read);
	if (isResponseComplete(clientFd))
	{
		std::cout << "Recieved request:\n" << _clients.at(clientFd)->_requestBuffer << "\n";
		if (_clients[clientFd]->_reqType == (ClientConnection::reqType)CHUNKED) {
			cleanChunkedReq(clientFd);
		}

		Request	tmp_request(_clients.at(clientFd)->_requestBuffer);
		const Config::Routes &route = Response::find_match(_clients.at(clientFd)->_config, tmp_request.getUrl());

		std::map<std::string, std::string>::const_iterator cgi_route;
		if ((cgi_route = Response::check_cgi(route, tmp_request.getUrl())) != route.cgi.end())
		{
			std::vector<std::string> arguments;
			arguments.push_back(cgi_route->second);
			std::string file = tmp_request.getUrl().substr(route.path.length());
			arguments.push_back(route.directory + file);
			if (!startCGI(clientFd, arguments))
			{
				_clients.at(clientFd)->_cgiFailed = true;
				generateResponse(clientFd);
				changeToWrite(clientFd);
			}
		}
		else
		{
			generateResponse(clientFd);
			changeToWrite(clientFd);
		}
	}
}

// Write response to the client

void EventHandler::generateResponse(int clientFd) {
	if (_clients.at(clientFd)->_errorCode) {
		Response rsp(_clients.at(clientFd)->_config, _clients.at(clientFd)->_errorCode);
		_clients.at(clientFd)->_responseBuffer = rsp.generateResponse();
		return ;
	}
	Request	rqs(_clients.at(clientFd)->_requestBuffer);
	std::string s;
	if (!_clients.at(clientFd)->_cgiBuffer.empty())
	{
		Response rsp(rqs, _clients.at(clientFd)->_config, _clients.at(clientFd)->_cgiBuffer, !_clients.at(clientFd)->_cgiFailed);
		s = rsp.generateResponse();
	}
	else
	{
		Response rsp(rqs, _clients.at(clientFd)->_config);
		s = rsp.generateResponse();
	}
	// IF REQUEST WAS FOR A CGI -> _responseBuffer contains CGI content
	_clients.at(clientFd)->_responseBuffer.append(s);
}

void EventHandler::handleResponse(int clientFd) {
	std::cout << "Sending response to client " << clientFd << "\n";
	
	ssize_t bytes_remaining = _clients.at(clientFd)->_responseBuffer.size();
	ssize_t bytes_written = write(clientFd, _clients.at(clientFd)->_responseBuffer.c_str(), _clients.at(clientFd)->_responseBuffer.length());
	if (bytes_written < 0) {
		return  ; // could be temporary full socket, but not allowed to check err code to confirm :(
	} else if (bytes_written == bytes_remaining) {
		_clients.at(clientFd)->resetData();
		changeToRead(clientFd);
	} else {
		_clients.at(clientFd)->_responseBuffer.erase(0, bytes_written);
	}
}

void EventHandler::checkCompleteCGIProcesses(void)
{
	std::map<int, CGIInfo*>::iterator it;
	std::vector<int> completed;

	for (it = _cgiManager._cgiProcesses.begin();
			it != _cgiManager._cgiProcesses.end();
			++it) {
		if (it->second->isFinished) {
			CGIInfo *info = it->second;
			int clientFd = info->clientFd;
			_clients.at(clientFd)->_cgiBuffer = info->output;
			deleteFromEpoll(info->pipeFd);
			completed.push_back(info->pipeFd);
			_openConns.erase(info->pipeFd);
			generateResponse(clientFd);
			changeToWrite(clientFd);
		}
	}

	for (std::vector<int>::const_iterator it = completed.begin(); it < completed.end(); it++)
		_cgiManager.deleteFromCGI(*it);
}

void EventHandler::epollLoop(void) {
	struct epoll_event eventQueue[MAX_EVENTS];

	while (1) {
		int numEvents = epoll_wait(_epollFd, eventQueue, MAX_EVENTS, 0);
		if (numEvents == -1) {
			throw epollWaitFailure();
		}
		for (int i = 0; i < numEvents; ++i) {
			int fd = eventQueue[i].data.fd;
			switch (_openConns[fd]) {
				case EP_SERVER:
					handleNewConnection(*_servers[fd]);
					break ;
				case EP_CGI:
					_cgiManager.readCGIOutput(fd);
					break ;
				case EP_CLIENT:
					if (eventQueue[i].events & EPOLLIN)
						handleClientRequest(fd);
					else if (eventQueue[i].events & EPOLLOUT)
						handleResponse(fd);
					break ;
			}
		}
		checkCompleteCGIProcesses();
	}
}
