#include "netclient.hpp"

#include <iostream>

static netclient *s_Instance = nullptr;

netclient::~netclient() {
	if (m_NetworkThread.joinable())
		m_NetworkThread.join();
}

void netclient::ConnectToServer(const std::string &serverAddress) {
	if (m_Running)
		return;

	if (m_NetworkThread.joinable())
		m_NetworkThread.join();

	m_ServerAddress = serverAddress;
	m_NetworkThread = std::thread([this]() {
		NetworkThreadFunc();
	});
}

void netclient::Disconnect() {
	m_Running = false;

	if (m_NetworkThread.joinable())
		m_NetworkThread.join();
}

void netclient::SetDataReceivedCallback(const DataReceivedCallback &function) {
	m_DataReceivedCallback = function;
}

void netclient::SetServerConnectedCallback(const ServerConnectedCallback &function) {
	m_ServerConnectedCallback = function;
}

void netclient::SetServerDisconnectedCallback(const ServerDisconnectedCallback &function) {
	m_ServerDisconnectedCallback = function;
}

void netclient::NetworkThreadFunc() {
	s_Instance = this;

	m_ConnectionStatus = ConnectionStatus::Connecting;

	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
		m_ConnectionDebugMessage = "Could not initialize GameNetworkingSockets";
		m_ConnectionStatus = ConnectionStatus::FailedToConnect;
		return;
	}

	m_Interface = SteamNetworkingSockets();

	SteamNetworkingIPAddr address;
	if (!address.ParseString(m_ServerAddress.c_str())) {
		OnFatalError("Invalid IP address - could not parse {" + m_ServerAddress + "}");
		m_ConnectionDebugMessage = "Invalid IP address";
		m_ConnectionStatus = ConnectionStatus::FailedToConnect;
		return;
	}

	SteamNetworkingConfigValue_t options;
	options.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void *)ConnectionStatusChangedCallback);
	m_Connection = m_Interface->ConnectByIPAddress(address, 1, &options);
	if (m_Connection == k_HSteamNetConnection_Invalid) {
		m_ConnectionDebugMessage = "Failed to create connection";
		m_ConnectionStatus = ConnectionStatus::FailedToConnect;
		return;
	}

	m_Running = true;
	while (m_Running) {
		PollIncomingMessages();
		PollConnectionStateChanges();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	m_Interface->CloseConnection(m_Connection, 0, nullptr, false);
	m_ConnectionStatus = ConnectionStatus::Disconnected;
	GameNetworkingSockets_Kill();
}

void netclient::Shutdown() {
	m_Running = false;
}

void netclient::SendBuffer(netbuffer buffer, bool reliable) {
	EResult result = m_Interface->SendMessageToConnection(
	                     m_Connection, buffer.d, (uint32_t)buffer.s,
	                     reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable, nullptr);
}

void netclient::SendString(std::string string, bool reliable) {
	string = '1' + string;
	SendBuffer(netbuffer(string.data(), string.size()), reliable);
}

void netclient::sendgamestate(int state0, bool reliable) {
	std::string s = "20";
	std::memcpy(&s.at(1), &state0, 1);
	SendBuffer(netbuffer(s.data(), s.size()), reliable);
}

void netclient::sendgamepos(const glm::vec3 &pos, bool reliable) {
	std::string newpos{"4000000000000"};
	std::memcpy(&newpos.at(1), &pos, 12);
	SendBuffer(netbuffer(newpos.data(), newpos.size()), reliable);
}

void netclient::PollIncomingMessages() {
	while (m_Running) {
		ISteamNetworkingMessage *incomingMessage = nullptr;
		int messageCount = m_Interface->ReceiveMessagesOnConnection(m_Connection, &incomingMessage, 1);
		if (messageCount == 0)
			break;

		if (messageCount < 0) {
			m_Running = false;
			return;
		}

		m_DataReceivedCallback(netbuffer(incomingMessage->m_pData, incomingMessage->m_cbSize));

		incomingMessage->Release();
	}
}

void netclient::PollConnectionStateChanges() {
	m_Interface->RunCallbacks();
}

void netclient::ConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t *info) {
	s_Instance->OnConnectionStatusChanged(info);
}

void netclient::OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *info) {

	switch (info->m_info.m_eState) {
	case k_ESteamNetworkingConnectionState_None:
		break;

	case k_ESteamNetworkingConnectionState_ClosedByPeer:
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
		m_Running = false;
		m_ConnectionStatus = ConnectionStatus::FailedToConnect;
		m_ConnectionDebugMessage = info->m_info.m_szEndDebug;

		if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting) {
			std::cout << "Could not connect to remote host. " << info->m_info.m_szEndDebug << std::endl;
		} else if (info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
			std::cout << "Lost connection with remote host. " << info->m_info.m_szEndDebug << std::endl;
		} else {
			std::cout << "Disconnected from host. " << info->m_info.m_szEndDebug << std::endl;
		}

		m_Interface->CloseConnection(info->m_hConn, 0, nullptr, false);
		m_Connection = k_HSteamNetConnection_Invalid;
		m_ConnectionStatus = ConnectionStatus::Disconnected;
		break;
	}

	case k_ESteamNetworkingConnectionState_Connecting:
		break;

	case k_ESteamNetworkingConnectionState_Connected:
		m_ConnectionStatus = ConnectionStatus::Connected;
		m_ServerConnectedCallback();
		break;

	default:
		break;
	}
}

void netclient::OnFatalError(const std::string &message) {
	std::cout << message << std::endl;
	m_Running = false;
}
