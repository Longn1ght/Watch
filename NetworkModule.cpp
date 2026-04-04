#include "Watch.h"

NetworkModule::NetworkModule(IDENTITY id)
{
	//初始化网络模块
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		MessageBox(NULL, L"WSAStartup failed", L"Error", MB_OK | MB_ICONERROR);
	}
	if (id == HOST)
	{
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;

		iResult = getaddrinfo(nullptr, "2778", &hints, &result);
		if (iResult != 0)
		{
			MessageBox(NULL, L"getaddrinfo failed", L"Error", MB_OK | MB_ICONERROR);
			WSACleanup();
		}

		Listensock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (Listensock == INVALID_SOCKET)
		{
			MessageBox(NULL, L"listen socket failed", L"Error", MB_OK | MB_ICONERROR);
		}

		iResult = bind(Listensock, result->ai_addr, (int)result->ai_addrlen);
		if (iResult == SOCKET_ERROR)
		{
			MessageBox(NULL, L"bind failed", L"Error", MB_OK | MB_ICONERROR);
			freeaddrinfo(result);
			closesocket(Listensock);
			WSACleanup();
		}
		freeaddrinfo(result);

		if (listen(Listensock, SOMAXCONN) == SOCKET_ERROR)
		{
			MessageBox(NULL, L"listen failed", L"Error", MB_OK | MB_ICONERROR);
			closesocket(Listensock);
			WSACleanup();
		}

		Connectsock = accept(Listensock, nullptr, nullptr);
		if (Connectsock == INVALID_SOCKET)
		{
			MessageBox(NULL, L"accept failed", L"Error", MB_OK | MB_ICONERROR);
			closesocket(Listensock);
			WSACleanup();
		}
	}
	else if (id == CLIENT)
	{
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		
		iResult = getaddrinfo("127.0.0.1", "2778", &hints, &result);
		if (iResult != 0)
		{
			MessageBox(NULL, L"getaddrinfo failed", L"Error", MB_OK | MB_ICONERROR);
			WSACleanup();
		}
		ptr = result;

		Connectsock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (Connectsock == INVALID_SOCKET)
		{
			MessageBox(NULL, L"connect socket failed", L"Error", MB_OK | MB_ICONERROR);
			WSACleanup();
		}

		iResult = connect(Connectsock, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR)
		{
			MessageBox(NULL, L"connect failed", L"Error", MB_OK | MB_ICONERROR);
			closesocket(Connectsock);
			freeaddrinfo(result);
			WSACleanup();
		}
	}
}

bool NetworkModule::SendNetFrameMessage(MESSAGE_TYPE mt, LPVOID lpData)
{
	msg.mt = mt;
	msg.data = static_cast<uint8_t*>(lpData);
	msg.dataSize = sizeof(lpData);
	if (send(Connectsock, (const char*)&msg, sizeof(msg), 0) < 0)
	{
		MessageBox(NULL, L"send failed", L"Error", MB_OK | MB_ICONERROR);
		return FALSE;
	}

	if (send(Connectsock, (const char*)&msg, sizeof(msg), 0) < 0)
	{
		MessageBox(NULL, L"send failed", L"Error", MB_OK | MB_ICONERROR);
		return FALSE;
	}
	return TRUE;
}

bool NetworkModule::RecvNetFrameMessage(MESSAGE_TYPE mt, LPVOID lpData)
{
	int iResult = recv(Connectsock, (char*)&msg, sizeof(msg), 0);
	if (iResult > 0)
	{
		if (msg.mt == mt)
		{
			memcpy(lpData, msg.data, msg.dataSize);
			return TRUE;
		}
		else
		{
			MessageBox(NULL, L"Received message type mismatch", L"Error", MB_OK | MB_ICONERROR);
			return FALSE;
		}
	}
	else if (iResult == 0)
	{
		MessageBox(NULL, L"Connection closed", L"Info", MB_OK | MB_ICONINFORMATION);
		return FALSE;
	}
	else
	{
		MessageBox(NULL, L"recv failed", L"Error", MB_OK | MB_ICONERROR);
		return FALSE;
	}
}
