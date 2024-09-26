/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventHandler.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bthomas <bthomas@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/09/24 12:21:12 by bthomas           #+#    #+#             */
/*   Updated: 2024/09/26 14:46:22 by bthomas          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#ifndef EVENTHANDLER_HPP 
# define EVENTHANDLER_HPP 

#include "Server.hpp"
#include "ClientConnection.hpp"
#include <fcntl.h>
#include <map>
#include <utility>
#define BUFFER_SIZE 30720

class ClientConnection;

enum responseType {
	TEXT,
	NONTEXT
};

struct responseContent {
	responseType type;
	void * content;
};

class EventHandler
{
	private:
		int _epollFd;
		std::map<int, ClientConnection> _clients;

	public:
		class epollInitFailure;
		void setNonBlock(int fd);
		void addSocketToEpoll(Server & s);
		void addSocketToEpoll(int fd);
		void addClient(int clientFd);
		void handleNewConnection(Server & s);
		void handleClientRequest(int clientFd);
		void handleResponse(int clientFd);
		void epollLoop(Server & s);
		void changeToWrite(int clientFd);
		void changeToRead(int clientFd);
		bool isResponseComplete(int clientFd);

	public:
		EventHandler (void);
		~EventHandler ();
};

#endif /* EVENTHANDLER_HPP */
