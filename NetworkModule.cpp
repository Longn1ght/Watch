#include "Wacth.h"

bool NetworkModule::SendNetFrameMessage(IDENTITY id, MESSAGE_TYPE mt, LPVOID lpData)
{
	switch (id)
	{
	case HOST:
		//服务器身份
		break;
	case CLIENT:
		//客户端身份
		break;
	}
}
