//delete from any channels -> delete this first!
	//get client address (&address)
	//get list of channels client belongs to in client object
	//go into each channel and delete the client from each channel
//delete from nickMap
	//get client nicknam
	//delete from nickMap
//delete from pfdsMap
//set change to 1 somehow?
//make sure there's nothing to free in the client object!
//need to send :sender_id QUIT :goodbye message -> 
	//should we send it to other users in the channel? -> we need to do this if client exits without quit command

Commands::notifyChanMembers(Channel & chan, std::string message)
{
	std::map<Client *, bool>::iterator it = chan._members.begin();
	for (; it != chan._members.end(); it++)
		*(it->first)._messages.push_back(message);
}
Commands::QUIT()
{
	//for now we will send this message immediately without checking for pollout
	std::string error = "ERROR :client " + _sender._nick + " quit\r\n";
	if (send(_senderFd, error.c_str(), error.length(), 0) == -1)
		std::cout << "Unable to send error quit message to client\n";
	//send quit notification to everyone
	//erase client from all channels they belong to
	//is it possible to make the list of channels in clients a pointer to the actual channel?
	std::vector<std::string> chanList = _sender._channels;
	for (std::vector<std::string>::iterator it = chanList.begin(); it != chanList.end(); it++)
	{
		notifyChanMembers(*it, QUIT(_sender._identifier, _sender._nick)) //see below we need to add this codes.hpp
		*it->_members.erase(&client);
	}
	Server::_nickMap.erase(_sender._nick);
	Server::_pfdsMap.erase(_sender);
	Server::_change = 1;
}

#define QUIT(identifier, nick) \
	":" + identifier + " QUIT :" + nick + " has quit\r\n"

void	Commands::MOTD()
{
	//open file
	//append each prefix to the proper message and push back
	_sender._messages.push_back(RPL_MOTDSTART(Server::getServername(), _sender._nick));
	_sender._messages.push_back(RPL_MOTD(Server::getServername(), _sender._nick));
	_sender._messages.push_back(RPL_ENDOFMOTD(Server::getServername(), _sender._nick));
}

/* FOR FILE TRANSFER BONUS

use Client._clientInfo to access port num and ip

struct sockaddr_in {
    short int          sin_family;  // Address family, AF_INET
    unsigned short int sin_port;    // Port number
    struct in_addr     sin_addr;    // Internet address
    unsigned char      sin_zero[8]; // Same size as struct sockaddr
};

struct in_addr {
    uint32_t s_addr; // that's a 32-bit int (4 bytes)
};

I THINK you need to use ntohs function to convert port and ntohl function to conver the address
since they would currently be in network byte order

*/