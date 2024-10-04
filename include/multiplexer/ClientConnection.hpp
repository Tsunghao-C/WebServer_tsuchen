/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ClientConnection.hpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bthomas <bthomas@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/09/26 10:53:42 by bthomas           #+#    #+#             */
/*   Updated: 2024/10/04 15:47:08 by bthomas          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#ifndef CLIENTCONNECTION_HPP
# define CLIENTCONNECTION_HPP

#include "EventHandler.hpp"

class ClientConnection
{
	public:
		enum reqType {
			CHUNKED,
			NONCHUNKED
		};

	public:
		int _clientFd;
		std::string _requestBuffer;
		std::string _responseBuffer;
		std::string _cgiBuffer;
		size_t	_bytesSent;
		bool _responseReady;
		bool _cgiFailed;
		reqType _reqType;

		uint8_t _errorCode;

		const Config &_config;

	public:
		void resetData();
		void closeConnection();

	public:
		ClientConnection (int fd, const Config &config);
		ClientConnection(const ClientConnection&);
		ClientConnection& operator=(const ClientConnection& other);
		~ClientConnection ();
};

#endif /* CLIENTCONNECTION_HPP */
