/**
 * Mandelbulber v2, a 3D fractal generator
 *
 * Netrender class - Networking class for command and payload communication
 * between one server and multiple clients
 *
 * Copyright (C) 2014 Krzysztof Marczak
 *
 * This file is part of Mandelbulber.
 *
 * Mandelbulber is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Mandelbulber is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details. You should have received a copy of the GNU
 * General Public License along with Mandelbulber. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Krzysztof Marczak (buddhi1980@gmail.com), Sebastian Jennen
 */

#include "netrender.hpp"
#include "system.hpp"
#include <QAbstractSocket>

CNetRender::CNetRender(qint32 workerCount)
{
	this->workerCount = workerCount;
	deviceType = UNKNOWN;
	version = 1000 * MANDELBULBER_VERSION;
}

CNetRender::~CNetRender()
{
	DeleteServer();
	DeleteClient();
}

void CNetRender::SetServer(qint32 portNo)
{
	DeleteClient();
	this->portNo = portNo;
	server = new QTcpServer(this);
	if(!server->listen(QHostAddress::LocalHost, portNo))
	{
		qDebug() << "CNetRender - SetServer Error: " << server->errorString();
	}
	else
	{
		connect(server, SIGNAL(newConnection()), this, SLOT(HandleNewConnection()));
		deviceType = SERVER;
		qDebug() << "CNetRender - SetServer setup on localhost, port: " << portNo;
	}
}

void CNetRender::DeleteServer()
{
	if(deviceType != SERVER) return;
	// delete all clients
	// TODO check if memory leaks
	// qDeleteAll(clients);

	deviceType = UNKNOWN;
	qDebug() << "CNetRender - DeleteServer";
}

int CNetRender::getTotalWorkerCount()
{
	int totalCount = 0;
	for(int i = 0; i < clients.count(); i++)
	{
		totalCount += clients[i]->clientWorkerCount;
	}
	return totalCount;
}

void CNetRender::HandleNewConnection()
{
	while (server->hasPendingConnections())
	{
		// push new socket to list
		sClient *client = new sClient;
		client->socket = server->nextPendingConnection();
		client->msg = new sMessage;
		clients.append(client);

		// qDebug() << "CNetRender - HandleNewConnection socket: " << client->socket->peerAddress();

		connect(client->socket, SIGNAL(disconnected()), this, SLOT(ClientDisconnected()));
		connect(client->socket, SIGNAL(readyRead()), this, SLOT(ReceiveFromClient()));

		// tell mandelbulber version to client
		sMessage msg;
		msg.command = VERSION;
		msg.payload = (char*) &version;
		msg.size = sizeof(qint32);
		SendData(client->socket, msg);
		emit ClientsChanged();
	}
}

void CNetRender::ClientDisconnected()
{
	// get client by signal emitter
	QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
	qDebug() << "CNetRender - ClientDisconnected socket: " << socket->peerAddress();
	if(socket)
	{
		int index = GetClientIndexFromSocket(socket);
		if(index > -1){
			clients.removeAt(index);
		}
		socket->close();
		socket->deleteLater();
		emit ClientsChanged();
	}
}

int CNetRender::GetClientIndexFromSocket(const QTcpSocket *socket)
{
	for(int i = 0; i < clients.size(); i++)
	{
		if(clients[i]->socket->socketDescriptor() == socket->socketDescriptor())
		{
			return i;
		}
	}
	return -1;
}

void CNetRender::ReceiveFromClient()
{
	// get client by signal emitter
	QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
	int index = GetClientIndexFromSocket(socket);
	if(index != -1)
	{
		ReceiveData(socket, clients[index]->msg);
	}
}

void CNetRender::SetClient(QString address, int portNo)
{
	DeleteServer();
	deviceType = CLIENT;
	this->address = address;
	this->portNo = portNo;
	ResetMessage(&msgFromServer);
	clientSocket = new QTcpSocket(this);

	reconnectTimer = new QTimer;
	reconnectTimer->setInterval(1000);
	connect(reconnectTimer, SIGNAL(timeout()), this, SLOT(TryServerConnect()));
	connect(clientSocket, SIGNAL(disconnected()), this, SLOT(ServerDisconnected()));
	connect(clientSocket, SIGNAL(readyRead()), this, SLOT(ReceiveFromServer()));

	reconnectTimer->start();
	qDebug() << "CNetRender - SetClient setup, link to server: " << address << ", port: " << portNo;
}

void CNetRender::ServerDisconnected()
{
	reconnectTimer->start();
}

void CNetRender::TryServerConnect()
{
	if(clientSocket->state() == QAbstractSocket::ConnectedState)
	{
		reconnectTimer->stop();
	}
	else
	{
		// qDebug() << "CNetRender - Try (re)connect to server: " << address << ", port: " << portNo;
		clientSocket->close();
		clientSocket->connectToHost(address, portNo);
	}
}

void CNetRender::ReceiveFromServer()
{
	ReceiveData(clientSocket, &msgFromServer);
}

void CNetRender::DeleteClient()
{
	if (deviceType != CLIENT) return;
	deviceType = UNKNOWN;
	qDebug() << "CNetRender - DeleteClient";
}

bool CNetRender::SendData(QTcpSocket *socket, sMessage msg)
{
	//			NetRender Message format         (optional)
	// | qint32		| qint32	| qint32	|( 0 - ???	| quint16  |)
	// | command	| id			| size		|( payload	| checksum |)

	if (!socket) return false;
	if(socket->state() != QAbstractSocket::ConnectedState) return false;

	QByteArray byteArray;
	QDataStream socketWriteStream(&byteArray, QIODevice::ReadWrite);

	// append header
	socketWriteStream << msg.command << msg.id << msg.size;

	// append payload
	if(msg.size > 0)
	{
		socketWriteStream.writeRawData(msg.payload, msg.size);
		// append checksum
		socketWriteStream << qChecksum(msg.payload, msg.size);
	}

	// write to socket
	socket->write(byteArray);
	socket->waitForBytesWritten();

	// qDebug() << "wrote data: " << msg.command << ", id: " << msg.id << ", size: " << msg.size;
	return true;
}

void CNetRender::ResetMessage(sMessage *msg)
{
	if(msg == NULL)
	{
		msg = new sMessage;
	}
	else
	{
		msg->command = -1;
		msg->id = 0;
		msg->size = 0;
	}
}

void CNetRender::ReceiveData(QTcpSocket *socket, sMessage *msg)
{
	QDataStream socketReadStream(socket);

	if (msg->command == -1)
	{
		if (socket->bytesAvailable() < (sizeof(msg->command) + sizeof(msg->id) + sizeof(msg->size)))
		{
			return;
		}
		// meta data available
		socketReadStream >> msg->command;
		socketReadStream >> msg->id;
		socketReadStream >> msg->size;
	}

	if(msg->size > 0)
	{
		if (socket->bytesAvailable() < sizeof(quint16) + msg->size)
		{
			return;
		}
		// full payload available
		msg->payload = new char[msg->size];
		socketReadStream.readRawData(msg->payload, msg->size);
		quint16 crc;
		socketReadStream >> crc;
		if(crc != qChecksum(msg->payload, msg->size))
		{
			qDebug() << "CNetRender - checksum mismatch, will ignore this message(cmd: " << msg->command << "id: " << msg->id << "size: " << msg->size << ")";
			return;
		}
	}
	ProcessData(socket, msg);
}

void CNetRender::ProcessData(QTcpSocket *socket, sMessage *inMsg)
{
	// beware: payload points to char, first cast to target type pointer, then dereference
	// *(qint32*)msg->payload

	if(IsClient())
	{
		switch ((netCommand)inMsg->command)
		{
		case VERSION:
		{
			sMessage outMsg;
			if(*(qint32*)inMsg->payload == version)
			{
				qDebug() << "CNetRender - version matches (" << version << "), connection established";
				// server version matches, send worker count
				outMsg.command = WORKER;
				outMsg.payload = (char*) &workerCount;
				outMsg.size = sizeof(qint32);
			}
			else
			{
				qDebug() << "CNetRender - version mismatch! client version: " << version << ", server: " << *(qint32*)inMsg->payload;
				outMsg.command = BAD;
			}
			SendData(clientSocket, outMsg);
			break;
		}
		case RENDER:
		{
			// emit signal on which client is listening for rendering jobs
			qDebug() << "CNetRender - received render request from server, id: " << inMsg->id;
			emit RenderRequest(inMsg);
			break;
		}
		default:
			break;
		}
	}
	else if(IsServer())
	{
		int index = GetClientIndexFromSocket(socket);
		if(index > -1)
		{
			switch ((netCommand)inMsg->command)
			{
			case BAD:
			{
				qDebug() << "CNetRender - clients[" << index << "] " << socket->peerAddress() << " has wrong version";
				clients.removeAt(index);
				emit ClientsChanged();
				break;
			}
			case WORKER:
			{
				clients[index]->clientWorkerCount = *(qint32*)inMsg->payload;
				if(clients[index]->status == NEW) clients[index]->status = IDLE;
				qDebug() << "CNetRender - clients[" << index << "] " << socket->peerAddress() << " has " << clients[index]->clientWorkerCount << "workers";
				emit ClientsChanged();
				break;
			}
			case DATA:
			{
				qDebug() << "CNetRender - received data from clients[" << index << "] " << socket->peerAddress() << ", id: " << inMsg->id;
				// emit signal on which server is listening for rendered response
				emit RenderResponse(index, inMsg);
				break;
			}
			default:
				break;
			}
		}
		else
		{
			qDebug() << "CNetRender - client " << socket->peerAddress() << " unknown";
		}
	}
	ResetMessage(inMsg);
}