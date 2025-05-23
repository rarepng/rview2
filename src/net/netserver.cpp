#include "netserver.hpp"

#include <chrono>
#include <iostream>

static netserver *s_Instance = nullptr;

netserver::netserver(int port) : m_Port(port) {}

netserver::~netserver() {
	if (m_NetworkThread.joinable())
		m_NetworkThread.join();
}

void netserver::Start() {
	if (m_Running)
		return;

	m_NetworkThread = std::thread([this]() {
		NetworkThreadFunc();
	});
}

void netserver::Stop() {
	m_Running = false;
}

void netserver::NetworkThreadFunc() {
	s_Instance = this;
	m_Running = true;

	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
		OnFatalError("GameNetworkingSockets_Init failed");
		return;
	}

	m_Interface = SteamNetworkingSockets();

	SteamNetworkingIPAddr serverLocalAddress;
	serverLocalAddress.Clear();
	serverLocalAddress.m_port = m_Port;

	SteamNetworkingConfigValue_t options;
	options.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
	               (void *)netserver::ConnectionStatusChangedCallback);

	m_ListenSocket = m_Interface->CreateListenSocketIP(serverLocalAddress, 1, &options);

	if (m_ListenSocket == k_HSteamListenSocket_Invalid) {
		OnFatalError("Fatal error: Failed to listen on port {}" + std::to_string(m_Port));
		return;
	}

	m_PollGroup = m_Interface->CreatePollGroup();
	if (m_PollGroup == k_HSteamNetPollGroup_Invalid) {
		OnFatalError("Fatal error: Failed to listen on port {}" + std::to_string(m_Port));
		return;
	}

	std::cout << "Server listening on port " << m_Port << std::endl;

	while (m_Running) {
		PollIncomingMessages();
		PollConnectionStateChanges();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	std::cout << "Closing connections..." << std::endl;
	for (const auto &[clientID, clientInfo] : m_ConnectedClients) {
		m_Interface->CloseConnection(clientID, 0, "Server Shutdown", true);
	}

	m_ConnectedClients.clear();

	m_Interface->CloseListenSocket(m_ListenSocket);
	m_ListenSocket = k_HSteamListenSocket_Invalid;

	m_Interface->DestroyPollGroup(m_PollGroup);
	m_PollGroup = k_HSteamNetPollGroup_Invalid;
}

void netserver::ConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t *info) {
	s_Instance->OnConnectionStatusChanged(info);
}

void netserver::OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *status) {
	switch (status->m_info.m_eState) {
	case k_ESteamNetworkingConnectionState_None:
		break;

	case k_ESteamNetworkingConnectionState_ClosedByPeer:
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
		if (status->m_eOldState == k_ESteamNetworkingConnectionState_Connected) {
			auto itClient = m_ConnectedClients.find(status->m_hConn);

			m_ClientDisconnectedCallback(itClient->second);

			m_ConnectedClients.erase(itClient);
		}

		m_Interface->CloseConnection(status->m_hConn, 0, nullptr, false);
		break;
	}

	case k_ESteamNetworkingConnectionState_Connecting: {
		if (m_Interface->AcceptConnection(status->m_hConn) != k_EResultOK) {
			m_Interface->CloseConnection(status->m_hConn, 0, nullptr, false);
			std::cout << "Couldn't accept connection (it was already closed?)" << std::endl;
			break;
		}

		if (!m_Interface->SetConnectionPollGroup(status->m_hConn, m_PollGroup)) {
			m_Interface->CloseConnection(status->m_hConn, 0, nullptr, false);
			std::cout << "Failed to set poll group" << std::endl;
			break;
		}

		SteamNetConnectionInfo_t connectionInfo;
		m_Interface->GetConnectionInfo(status->m_hConn, &connectionInfo);

		auto &client = m_ConnectedClients[status->m_hConn];
		client.ID = (ClientID)status->m_hConn;
		client.ConnectionDesc = connectionInfo.m_szConnectionDescription;

		m_ClientConnectedCallback(client);

		break;
	}

	case k_ESteamNetworkingConnectionState_Connected:
		break;

	default:
		break;
	}
}

void netserver::PollConnectionStateChanges() {
	m_Interface->RunCallbacks();
}

void netserver::PollIncomingMessages() {
	while (m_Running) {
		ISteamNetworkingMessage *incomingMessage = nullptr;
		int messageCount = m_Interface->ReceiveMessagesOnPollGroup(m_PollGroup, &incomingMessage, 1);
		if (messageCount == 0)
			break;

		if (messageCount < 0) {
			m_Running = false;
			return;
		}

		auto itClient = m_ConnectedClients.find(incomingMessage->m_conn);
		if (itClient == m_ConnectedClients.end()) {
			std::cout << "ERROR: Received data from unregistered client\n";
			continue;
		}

		if (incomingMessage->m_cbSize)
			m_DataReceivedCallback(itClient->second, netbuffer(incomingMessage->m_pData, incomingMessage->m_cbSize));

		incomingMessage->Release();
	}
}

void netserver::SetClientNick(HSteamNetConnection hConn, const char *nick) {
	m_Interface->SetConnectionName(hConn, nick);
}

void netserver::SetDataReceivedCallback(const DataReceivedCallback &function) {
	m_DataReceivedCallback = function;
}

void netserver::SetClientConnectedCallback(const ClientConnectedCallback &function) {
	m_ClientConnectedCallback = function;
}

void netserver::SetClientDisconnectedCallback(const ClientDisconnectedCallback &function) {
	m_ClientDisconnectedCallback = function;
}

void netserver::SendBufferToClient(ClientID clientID, netbuffer buffer, bool reliable) {
	m_Interface->SendMessageToConnection((HSteamNetConnection)clientID, buffer.d, (ClientID)buffer.s,
	                                     reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable,
	                                     nullptr);
}

void netserver::SendBufferToAllClients(netbuffer buffer, ClientID excludeClientID, bool reliable) {
	for (const auto &[clientID, clientInfo] : m_ConnectedClients) {
		if (clientID != excludeClientID)
			SendBufferToClient(clientID, buffer, reliable);
	}
}

void netserver::SendStringToClient(const ClientID clientID, ClientID clientID2, std::string string, bool reliable) {
	string = "10000" + string;

	std::memcpy(&string.at(1), &clientID2, 4);

	SendBufferToClient(clientID, netbuffer(string.data(), string.size()), reliable);
}

void netserver::sendgamestate(const ClientID clientID, ClientID clientID2, int state, bool reliable) {
	std::string s = "200000";
	std::memcpy(&s.at(1), &clientID2, 4);
	std::memcpy(&s.at(5), &state, 1);
	SendBufferToClient(clientID, netbuffer(s.data(), s.size()), reliable);
}

void netserver::sendgamepos(const ClientID clientID, ClientID clientID2, const glm::vec3 &pos, bool reliable) {
	std::string newpos{"40000000000000000"};
	std::memcpy(&newpos.at(1), &clientID2, 4);
	std::memcpy(&newpos.at(5), &pos, 12);
	SendBufferToClient(clientID, netbuffer(newpos.data(), newpos.size()), reliable);
}

void netserver::sendconnections(const ClientID clientID, const std::vector<ClientID> &clientIDs, bool reliable) {
	std::string newpos{"8"};
	for (size_t i{0}; i < clientIDs.size(); i++) {
		newpos.append("0000");
		std::memcpy(&newpos.at(1 + (4 * i)), clientIDs.data() + i, 4);
	}
	SendBufferToClient(clientID, netbuffer(newpos.data(), newpos.size()), reliable);
}

void netserver::SendStringToAllClients(const std::string &string, ClientID excludeClientID, bool reliable) {
	SendBufferToAllClients(netbuffer(string.data(), string.size()), excludeClientID, reliable);
}

void netserver::KickClient(ClientID clientID) {
	m_Interface->CloseConnection(clientID, 0, "Kicked by host", false);
}

void netserver::OnFatalError(const std::string &message) {
	std::cout << message << std::endl;
	m_Running = false;
}
