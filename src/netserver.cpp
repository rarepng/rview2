#include "netserver.hpp"

#include <iostream>
#include <chrono>

	// Can only have one server instance per-process
	static netserver* s_Instance = nullptr;

	netserver::netserver(int port)
		: m_Port(port)
	{
	}

	netserver::~netserver()
	{
		if (m_NetworkThread.joinable())
			m_NetworkThread.join();
	}

	void netserver::Start()
	{
		if (m_Running)
			return;

		m_NetworkThread = std::thread([this]() { NetworkThreadFunc(); });
	}

	void netserver::Stop()
	{
		m_Running = false;
	}

	void netserver::NetworkThreadFunc()
	{
		s_Instance = this;
		m_Running = true;

		SteamDatagramErrMsg errMsg;
		if (!GameNetworkingSockets_Init(nullptr, errMsg))
		{
			OnFatalError("GameNetworkingSockets_Init failed");
			return;
		}

		m_Interface = SteamNetworkingSockets();

		// Start listening
		SteamNetworkingIPAddr serverLocalAddress;
		serverLocalAddress.Clear();
		serverLocalAddress.m_port = m_Port;

		SteamNetworkingConfigValue_t options;
		options.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)netserver::ConnectionStatusChangedCallback);

		// Try to start listen socket on port
		m_ListenSocket = m_Interface->CreateListenSocketIP(serverLocalAddress, 1, &options);

		if (m_ListenSocket == k_HSteamListenSocket_Invalid)
		{
			OnFatalError("Fatal error: Failed to listen on port {}"+std::to_string(m_Port));
			return;
		}

		// Try to create poll group
		// TODO(Yan): should be optional, though good for groups which is probably the most common use case
		m_PollGroup = m_Interface->CreatePollGroup();
		if (m_PollGroup == k_HSteamNetPollGroup_Invalid)
		{
			OnFatalError("Fatal error: Failed to listen on port {}" + std::to_string(m_Port));
			return;
		}

		std::cout << "Server listening on port " << m_Port << std::endl;

		while (m_Running)
		{
			PollIncomingMessages();
			PollConnectionStateChanges();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		// Close all the connections
		std::cout << "Closing connections..." << std::endl;
		for (const auto& [clientID, clientInfo] : m_ConnectedClients)
		{
			m_Interface->CloseConnection(clientID, 0, "Server Shutdown", true);
		}

		m_ConnectedClients.clear();

		m_Interface->CloseListenSocket(m_ListenSocket);
		m_ListenSocket = k_HSteamListenSocket_Invalid;

		m_Interface->DestroyPollGroup(m_PollGroup);
		m_PollGroup = k_HSteamNetPollGroup_Invalid;
	}

	void netserver::ConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* info) { s_Instance->OnConnectionStatusChanged(info); }

	void netserver::OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* status)
	{
		// Handle connection state
		switch (status->m_info.m_eState)
		{
		case k_ESteamNetworkingConnectionState_None:
			// NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
			break;

		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
		{
			// Ignore if they were not previously connected.  (If they disconnected
			// before we accepted the connection.)
			if (status->m_eOldState == k_ESteamNetworkingConnectionState_Connected)
			{
				// Locate the client.  Note that it should have been found, because this
				// is the only codepath where we remove clients (except on shutdown),
				// and connection change callbacks are dispatched in queue order.
				auto itClient = m_ConnectedClients.find(status->m_hConn);
				//assert(itClient != m_mapClients.end());

				// Either ClosedByPeer or ProblemDetectedLocally - should be communicated to user callback
				// User callback
				m_ClientDisconnectedCallback(itClient->second);

				m_ConnectedClients.erase(itClient);
			}
			else
			{
				//assert(info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting);
			}

			// Clean up the connection.  This is important!
			// The connection is "closed" in the network sense, but
			// it has not been destroyed.  We must close it on our end, too
			// to finish up.  The reason information do not matter in this case,
			// and we cannot linger because it's already closed on the other end,
			// so we just pass 0s.
			m_Interface->CloseConnection(status->m_hConn, 0, nullptr, false);
			break;
		}

		case k_ESteamNetworkingConnectionState_Connecting:
		{
			// This must be a new connection
			// assert(m_mapClients.find(info->m_hConn) == m_mapClients.end());

			// Try to accept incoming connection
			if (m_Interface->AcceptConnection(status->m_hConn) != k_EResultOK)
			{
				m_Interface->CloseConnection(status->m_hConn, 0, nullptr, false);
				std::cout << "Couldn't accept connection (it was already closed?)" << std::endl;
				break;
			}

			// Assign the poll group
			if (!m_Interface->SetConnectionPollGroup(status->m_hConn, m_PollGroup))
			{
				m_Interface->CloseConnection(status->m_hConn, 0, nullptr, false);
				std::cout << "Failed to set poll group" << std::endl;
				break;
			}

			// Retrieve connection info
			SteamNetConnectionInfo_t connectionInfo;
			m_Interface->GetConnectionInfo(status->m_hConn, &connectionInfo);

			// Register connected client
			auto& client = m_ConnectedClients[status->m_hConn];
			client.ID = (ClientID)status->m_hConn;
			client.ConnectionDesc = connectionInfo.m_szConnectionDescription;

			// User callback
			m_ClientConnectedCallback(client);

			break;
		}

		case k_ESteamNetworkingConnectionState_Connected:
			// We will get a callback immediately after accepting the connection.
			// Since we are the server, we can ignore this, it's not news to us.
			break;

		default:
			break;
		}
	}

	void netserver::PollConnectionStateChanges()
	{
		m_Interface->RunCallbacks();
	}

	void netserver::PollIncomingMessages()
	{
		// Process all messages
		while (m_Running)
		{
			ISteamNetworkingMessage* incomingMessage = nullptr;
			int messageCount = m_Interface->ReceiveMessagesOnPollGroup(m_PollGroup, &incomingMessage, 1);
			if (messageCount == 0)
				break;

			if (messageCount < 0)
			{
				// messageCount < 0 means critical error?
				m_Running = false;
				return;
			}

			// assert(numMsgs == 1 && pIncomingMsg);

			auto itClient = m_ConnectedClients.find(incomingMessage->m_conn);
			if (itClient == m_ConnectedClients.end())
			{
				std::cout << "ERROR: Received data from unregistered client\n";
				continue;
			}

			if (incomingMessage->m_cbSize)
				m_DataReceivedCallback(itClient->second, netbuffer(incomingMessage->m_pData, incomingMessage->m_cbSize));

			// Release when done
			incomingMessage->Release();
		}
	}

	void netserver::SetClientNick(HSteamNetConnection hConn, const char* nick)
	{
		// Set the connection name, too, which is useful for debugging
		m_Interface->SetConnectionName(hConn, nick);
	}

	void netserver::SetDataReceivedCallback(const DataReceivedCallback& function)
	{
		m_DataReceivedCallback = function;
	}

	void netserver::SetClientConnectedCallback(const ClientConnectedCallback& function)
	{
		m_ClientConnectedCallback = function;
	}

	void netserver::SetClientDisconnectedCallback(const ClientDisconnectedCallback& function)
	{
		m_ClientDisconnectedCallback = function;
	}

	void netserver::SendBufferToClient(ClientID clientID, netbuffer buffer, bool reliable)
	{
		m_Interface->SendMessageToConnection((HSteamNetConnection)clientID, buffer.d, (ClientID)buffer.s, reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable, nullptr);
	}

	void netserver::SendBufferToAllClients(netbuffer buffer, ClientID excludeClientID, bool reliable)
	{
		for (const auto& [clientID, clientInfo] : m_ConnectedClients)
		{
			if (clientID != excludeClientID)
				SendBufferToClient(clientID, buffer, reliable);
		}
	}

	void netserver::SendStringToClient(const ClientID clientID, ClientID clientID2, std::string string, bool reliable){
		string = "10000" + string;
		
		std::memcpy(&string.at(1), &clientID2, 4);

		SendBufferToClient(clientID, netbuffer(string.data(), string.size()), reliable);
	}

	void netserver::sendgamestate(const ClientID clientID, ClientID clientID2, int state, bool reliable){
		std::string s = "200000";
		std::memcpy(&s.at(1), &clientID2, 4);
		std::memcpy(&s.at(5), &state, 1);
		SendBufferToClient(clientID, netbuffer(s.data(), s.size()), reliable);
	}

	void netserver::sendgamepos(const ClientID clientID, ClientID clientID2, const glm::vec3& pos, bool reliable)	{
		std::string newpos{ "40000000000000000" };
		std::memcpy(&newpos.at(1), &clientID2, 4);
		std::memcpy(&newpos.at(5), &pos, 12);
		SendBufferToClient(clientID, netbuffer(newpos.data(), newpos.size()), reliable);
	}

	void netserver::sendconnections(const ClientID clientID, const std::vector<ClientID>& clientIDs, bool reliable)	{
		std::string newpos{ "8" };
		for (size_t i{ 0 }; i < clientIDs.size(); i++) {
			newpos.append("0000");
			std::memcpy(&newpos.at(1+(4*i)), clientIDs.data()+i, 4);
		}
		SendBufferToClient(clientID, netbuffer(newpos.data(), newpos.size()), reliable);

	}

	void netserver::SendStringToAllClients(const std::string& string, ClientID excludeClientID, bool reliable)
	{
		SendBufferToAllClients(netbuffer(string.data(), string.size()), excludeClientID, reliable);
	}

	void netserver::KickClient(ClientID clientID)
	{
		m_Interface->CloseConnection(clientID, 0, "Kicked by host", false);
	}

	void netserver::OnFatalError(const std::string& message)
	{
		std::cout << message << std::endl;
		m_Running = false;
	}
