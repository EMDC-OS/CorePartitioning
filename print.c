#include "print.h"

char * type_iton(int type, char * str)
{
	switch(type){
		case TYPE_OPEN:
			memcpy(str, "OPEN", sizeof("OPEN"));	break;
		case TYPE_SOCKET:
			memcpy(str, "SOCKET", sizeof("SOCKET"));	break;
		case TYPE_BIND:
			memcpy(str, "BIND", sizeof("BIND"));	break;
		case TYPE_LISTEN:
			memcpy(str, "LISTEN", sizeof("LISTEN"));	break;
		case TYPE_ACCEPT:
			memcpy(str, "ACCEPT", sizeof("ACCEPT"));	break;
		case TYPE_CONNECT:
			memcpy(str, "CONNECT", sizeof("CONNECT"));	break;
		case TYPE_SEND:
			memcpy(str, "SEND", sizeof("SEND"));	break;
		case TYPE_RECV:
			memcpy(str, "RECV", sizeof("RECV"));	break;
		case TYPE_SETSOCKOPT:
			memcpy(str, "SETSOCKOPT", sizeof("SETSOCKOPT"));	break;
		case TYPE_GETSOCKOPT:
			memcpy(str, "GETSOCKOPT", sizeof("GETSOCKOPT"));	break;
		case TYPE_READ:
			memcpy(str, "READ", sizeof("READ"));	break;
		case TYPE_WRITE:
			memcpy(str, "WRITE", sizeof("WRITE"));	break;
		case TYPE_CLOSE:
			memcpy(str, "CLOSE", sizeof("CLOSE"));	break;
		case TYPE_POLL:
			memcpy(str, "POLL", sizeof("POLL"));	break;
		case TYPE_PPOLL:
			memcpy(str, "PPOLL", sizeof("PPOLL"));	break;
		case TYPE_SELECT:
			memcpy(str, "SELECT", sizeof("SELECT"));	break;
		case TYPE_SENDTO:
			memcpy(str, "SENDTO", sizeof("SENDTO"));	break;
		case TYPE_RECVFROM:
			memcpy(str, "RECVFROM", sizeof("RECVFROM"));	break;	
		case TYPE_SENDMSG:
			memcpy(str, "SENDMSG", sizeof("SENDMSG"));	break;
		case TYPE_RECVMSG:
			memcpy(str, "RECVMSG", sizeof("RECVMSG"));	break;
		case TYPE_SHUTDOWN:
			memcpy(str, "SHUTDOWN", sizeof("SHUTDOWN"));	break;
		case TYPE_GETSOCKNAME:
			memcpy(str, "GETSOCKNAME", sizeof("GETSOCKNAME"));	break;
		case TYPE_OPEN64:
			memcpy(str, "OPEN64", sizeof("OPEN64"));	break;
		case TYPE_OPENAT:
			memcpy(str, "OPENAT", sizeof("OPENAT"));	break;
		case TYPE_OPENAT64:
			memcpy(str, "OPENAT64", sizeof("OPENAT64"));	break;
		case TYPE_STAT:
			memcpy(str, "STAT", sizeof("STAT"));	break;
		case TYPE_STAT64:
			memcpy(str, "STAT64", sizeof("STAT64"));	break;
		case TYPE_LSEEK:
			memcpy(str, "LSEEK", sizeof("LSEEK"));	break;
		case TYPE_LSEEK64:
			memcpy(str, "LSEEK64", sizeof("LSEEK64"));	break;
		case TYPE_FOPEN:
			memcpy(str, "FOPEN", sizeof("FOPEN"));	break;
		default:
			printf("[ITON Default Error]\n");
			break;
	}
	return str;
}

void print_msg_info(args_data data){
	switch(data.request_type){
		case TYPE_OPEN:
			printf("Filename = %s Access = %d Permission = %d\n", data.open.pathname, data.open.flags, data.open.mode);
			break;
		case TYPE_SOCKET:
			printf("Domain = %d Type = %d Protocol = %d\n", data.socket.domain, data.socket.type, data.socket.protocol);
			break;
		case TYPE_BIND:
			printf("Socket = %d Address = 0x%p Address_len = %d\n", data.bind.socket, data.bind.address, data.bind.address_len);
			break;	
		case TYPE_LISTEN:
			printf("SocketFd = %d Backlog = %x \n", data.listen.sockfd, data.listen.backlog);
			break;
		case TYPE_ACCEPT:
			printf("Socket = %d Addr = 0x%p Addr_Len = 0x%p\n", data.accept.socket, data.accept.addr, data.accept.addrlen);
			break;
		case TYPE_CONNECT:
			printf("Socket = %d Address = 0x%p Address_Len = %d\n", data.connect.socket, data.connect.address, data.connect.address_len);
			break;
		case TYPE_SEND:
			printf("Socket = %d Buffer = 0x%p Length = %d Flags = %d\n", data.send.socket, data.send.buffer, (int)data.send.length, data.send.flags);
			break;
		case TYPE_RECV:
			printf("Socket = %d Buffer = 0x%p Length = %d Flags = %d\n", data.recv.socket, data.recv.buf, (int)data.recv.length, data.recv.flags);
			break;
		case TYPE_SETSOCKOPT:
			printf("Socket = %d Level = %d Option_name = %d Option_Val = 0x%p Option_Len = %d\n", data.setsockopt.socket, data.setsockopt.level, data.setsockopt.option_name, data.setsockopt.option_value, data.setsockopt.option_len);
			break;	
		case TYPE_GETSOCKOPT:
			printf("Socket = %d Level = %d Option_name = %d Option_Val = 0x%p Option_Len = %p\n", data.getsockopt.socket, data.getsockopt.level, data.getsockopt.option_name, data.getsockopt.buf, data.getsockopt.addrlen);
			break;	
		case TYPE_READ:
			printf("Fildes = %d Buffer = 0x%p nByte = %d\n", data.read.fildes, data.read.buf, (int)data.read.nbyte);
			break;
		case TYPE_WRITE:
			printf("Fildes = %d Buffer = 0x%p nByte = %d\n", data.write.fildes, data.write.buf, (int)data.write.nbyte);
			break;
		case TYPE_CLOSE:
			printf("Fildes = %d\n", data.fildes);
			break;
		case TYPE_POLL:
			printf("Fd = %d\n", data.poll.ufds->fd);
			break;
		case TYPE_PPOLL:
			printf("Fd = %d\n", data.ppoll.ufds->fd);
			break;
		case TYPE_SELECT:
			printf("N = %d Readfds = 0x%p Writefds = 0x%p Exceptfds = 0x%p Timeout = %lu\n", data.select.n, data.select.readfds, data.select.writefds, data.select.exceptfds, data.select.stimeout->tv_sec);
			break;
		case TYPE_SENDTO:
			printf("Socket = %d Buffer = 0x%p Length = %d Flags = %d sockaddr = 0x%p address_len = %d\n", data.sendto.socket, data.sendto.buffer, (int)data.sendto.length, data.sendto.flags, data.sendto.address, data.sendto.address_len);
			break;
		case TYPE_RECVFROM:
			printf("Socket = %d Buffer = 0x%p Length = %d Flags = %d sockaddr = 0x%p address_len = 0x%p\n", data.recvfrom.socket, data.recvfrom.buf, (int)data.recvfrom.length, data.recvfrom.flags, data.recvfrom.addr, data.recvfrom.addrlen);
			break;		
		case TYPE_SENDMSG:
			printf("Socket = %d MsgHdr = 0x%p Flags = %d\n", data.sendmsg.socket, data.sendmsg.msg, data.sendmsg.flags);
			break;
		case TYPE_RECVMSG:
			printf("Socket = %d MsgHdr = 0x%p Flags = %d\n", data.recvmsg.socket, data.recvmsg.msg_rcv, data.recvmsg.flags);
			break;
		case TYPE_SHUTDOWN:
			printf("Socket = %d Flags = %d\n", data.shutdown.socket, data.shutdown.flags);
			break;
		case TYPE_GETSOCKNAME:
			printf("Socket = %d sockaddr = 0x%p address_len = 0x%p\n", data.getsockname.socket, data.getsockname.addr, data.getsockname.addrlen);
			break;
		case TYPE_GETPEERNAME:
			printf("Socket = %d sockaddr = 0x%p address_len = 0x%p\n", data.getpeername.socket, data.getpeername.addr, data.getpeername.addrlen);				
			break;
		default:
			printf("[MSG INFO Default Error]\n");
			break;
	}
} 

void print_table_info(int caller)
{
	thread_info* tmp ;
	
	switch(caller){
		case CALL_BY_FUNCTION:
			printf("[In Function]---------------------------------------\n");
			break;
		case CALL_BY_THREAD:
			printf("[In Thread]-----------------------------------------\n");
			break;
		case CALL_BY_FUNCRECV:
			printf("[Function Recv]-------------------------------------\n");
			break;
		case CALL_BY_FUNCSEND:
			printf("[Function Send]-------------------------------------\n");
			break;
		case CALL_BY_THRRECV:
			printf("[Thread Recv]---------------------------------------\n");			
			break;
		case CALL_BY_THRSEND:
			printf("[Thread Send]---------------------------------------\n");
			break;
		case CALL_BY_FORKCHILD:
			printf("[Child Process]-------------------------------------\n");
			break;
		case CALL_BY_FORKPARENT:
			printf("[Parent Process]------------------------------------\n");
			break;
		default:
			printf("[TABLE INFO Default Error]\n");
			break;
	}
	tmp = header->next ;
	while(tmp != tail){
		printf("fd : %d message : %x %d\n",tmp->thr_fd, tmp->message);
		tmp = tmp->next;
	}
	
	printf("------------------------------------------------------\n");
}

