#include "Server.hpp"

Server::Server(std::string const & port, std::string const & pswd)
	:	_port(port),
		_password(pswd),
		_listenSockfd(-1),
		_pfdsCount(0),
		_serv(NULL),
		_pfds(NULL)
{
	checkPort();
	checkPassword();
}

Server::~Server()
{
	if (_serv)
		freeaddrinfo(_serv);
	if (_pfds)
		delete [] _pfds;
	//close any fds here
}

/*
we can change pswd policy later 
*/
void	Server::printPasswordPolicy()
{
	std::cout	<< "Password must be 8 - 12 characters in length and may only "
				<< "contain uppercase letters, lowercase letters, numbers, and "
				<< "the following symbols: !, @, $, *"
				<< std::endl;
}

void	Server::checkPassword() const
{
	size_t	len = _password.length();

	if (len < 8 || len > 12)
		throw InvalidPasswordException();
	for (std::string::const_iterator it = _password.begin(); it != _password.end(); it++)
	{
		if (!std::isalnum(*it) && 
			(*it != '!' || *it != '@' || *it != '$' || *it != '*'))
			throw InvalidPasswordException();
	}
}

/* 
1. check port length (so we're not parsing huge numbers or strings)
2. convert string to int with istringstream and check. 65535 is max port number
NOTE: this method does not accept a port that is entered as a decimal (ex 3000.0)
*/
void	Server::checkPort() const
{
	int	port;

	if (_port.length() > 5)
		throw InvalidPortException();
	std::istringstream ss(_port);
	ss >> port;
	if (ss.fail() || ss.get() != EOF ||
		port < 1024 || port > 65535) //ports 0 - 1024 are reserved (system ports)
		throw InvalidPortException();
}

/* 
1. make a single struct addrinfo (hints) with info about host for server like: ip type, socket type, flags
2. generate linked list of struct addrinfo (_serv) using getaddrinfo
3. socket() - create a socket fd with info from _serv
4. setsockopt() - sometimes the socket isn't cleared from port right away after a program run - this allows our listen socket to reuse port immediately
5. bind() - bind our socket fd to port
6. listen() - make our socket fd a listening socket with a queue of 5 - check this number later
7. make listen socket nonblocking
8. initialize _pfdsMap, _pfdsCount, and _pfds array to hold our listen socket fd (addNewPfd and copyNewPfdMapToArray)
*/
void	Server::makeListenSockfd()
{
	struct addrinfo	hints;
	int yes = 1;

	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM; //using stream sockets
	hints.ai_flags = AI_PASSIVE; //assign address of local host to socket structures

	//we have NULL as first parameter bc we used AI_PASSIVE flag. otherwise use specific ip address
	if (getaddrinfo(NULL, _port.c_str(), &hints, &_serv) != 0) 
		throw GetaddrinfoException();
	if ((_listenSockfd = socket(_serv->ai_family, _serv->ai_socktype, _serv->ai_protocol)) == -1)
		throw SocketException();
	if (setsockopt(_listenSockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		throw SetsockoptException();
	if (bind(_listenSockfd, _serv->ai_addr, _serv->ai_addrlen) == -1)
		throw BindException();
	if (listen(_listenSockfd, 5) == -1)
		throw ListenException();
	addNewPfd(LISTENFD);
	copyPfdMapToArray();
}

/*
1. create new client object - different object is created if it's a listening fd
2. fcntl - set fd to nonblocking and check for error
3. create new struct pollfd with inputted fd and set events to POLLIN
4. add fd and Client pair to map
*/
void	Server::addNewPfd(int tag)
{
	Client newClient;
	if (tag == LISTENFD)
	{
		newClient = Client();
		newClient._sockfd = _listenSockfd; //set our fd to listensockfd;
	}
	else if (tag == CLIENTFD)
		newClient = Client(_listenSockfd); //this constructor fills out _sockfd for us
	if (fcntl(newClient._sockfd, F_SETFL, O_NONBLOCK) == -1)
		throw FcntlException();
	
	struct pollfd newPfd = {};
	newPfd.fd = newClient._sockfd;
	newPfd.events = POLLIN;
	newClient._pfd = newPfd;

	_pfdsMap[newClient._sockfd] = newClient;
}

/* 
1. update _pfdsCount to _pfdsMap.length()
2. delete _pfds array if needed and create new array based on Map
*/
void	Server::copyPfdMapToArray()
{
	_pfdsCount = _pfdsMap.size();	

	if (_pfds)
		delete [] _pfds;
	_pfds = new struct pollfd[_pfdsCount]();
	int i = 0;
	for (std::map<int, Client>::iterator it = _pfdsMap.begin(); it != _pfdsMap.end(); it++)
		_pfds[i++] = it->second._pfd;
}

/* 
1. Print error msg
2. close fd
3. erase fd from map
*/
void	Server::deletePfd(int pfd, int err)
{
	if (err == 0)
		std::cout << "connection for client closed!\n";
	else if (err < 0)
		std::cerr << "recv error\n";
	close(pfd);
	_pfdsMap.erase(pfd);
}

/* 
1. create listening socket
In while loop
2. set a switch int change - tracks if any fds have been added to array
3. poll all fds - this function will switch revents (in struct) if fd is ready for some kind of action
4. iterate through every fd and check if there is POLLIN/POLLOUT/POLLHUP in revents
	POLLIN - data can be read from fd without blocking
	POLLOUT - data can be written to fd without blocking
	POLLHUP - fd has been closed or disconnected - delete it from our array
5. if a new client has been added, modify our array accordingly
7. free addrinfo struct (and everything else that needs to be freed)
 */
void	Server::createServer()
{
	//create a listening socket
	makeListenSockfd();
	while (true)
	{
		int change = 0;
		//-1 means that poll will block indefinitely until it gets something from any file descriptors in _pfds
		if (poll(_pfds, _pfdsCount, -1) == -1)
			throw PollException();
		for (int i = 0; i < _pfdsCount; i++)
		{
			if ((_pfds[i].revents & POLLIN) && _pfds[i].fd == _listenSockfd)
			{
				try
				{
					addNewPfd(CLIENTFD);
					change = 1;
				}
				catch(const std::exception& e)
				{
					//disconnect fd bc we couldn't make it unblocking
					std::cerr << e.what() << '\n';
				}
			}
			else if (_pfds[i].revents & POLLIN)
			{
				//client is ready to read any messages
				//USE RCV MSG HERE
				//ADD PARSING HERE
			}
			else if (_pfds[i].revents & POLLOUT)
			{
				//client is ready to send out a message to the server (or another client)
				//USE SEND MSG HERE
			}
			else if (_pfds[i].revents & POLLHUP)
			{
				//client has disconnected or closed
				//delete their fd here
			}
		}
		if (change == 1)
			copyPfdMapToArray();
	}
}

void	Server::printPfdsMap()
{
	for (std::map<int, Client>::iterator it = _pfdsMap.begin(); it != _pfdsMap.end(); it++)
	{
		it->second.printClient();
	}
}

/* 
1. If user already authenticated, return
2. if password matches, authenticate
3. else print error message and delete the connection
*/
// void Server::Pass(Client& client)
// {
// 	if (client._authenticated == TRUE)
// 	{
// 		std::cout << "Account already authenticated\n"; //ERR_ALREADYREGISTRED
// 		return ;
// 	}
// 	if (client._msg./*param*/ == _password)
// 		client._authenticated = true;
// 	else
// 	{
// 		std::cerr << "Invalid password entered\n";
// 		deletePFD(client._fd, 0); //connection for client closed! can adjust msg later
// 	}
// }

// /*
// 1. if client is not authenticated, send error message and delete connection
// -nick must be unique and can be changed
// -user does not have to be unique and cannot be changed
// -user comes with real name and mode as well
// */
// void Server::User(Client &client)
// {
// 	if (client._authenticated == FALSE)
// 	{
// 		std::cerr << "Account not authenticated\n"; //we need to send this message back to the client
// 		deletePFD(client._fd, 0); //connectin for client closed!
// 	}
	
	
// }

/* 
NOTES:
to initialize things to 0 for new, add () after
note sockaddr_storage can be used since it's flexible with ipv4 or ipv6.
since we're working with ipv4, better to use sockaddr_in -> then we don't have to static cast the struct after
in c++11 and later, we are actually able to get a pointer from the container using data()

IN WHILE LOOP
1. we run poll on our collection of _pfds and check poll_count
	if poll_count == - 1, error
2. now we loop through our _pfds (this is an inner loop)
	a. We check if the it's the listener fd and if its revents is set at POLLIN
		if yes, it means listener is ready to read!
		create new variables ready to take a client
		-struct sockaddr_in (use this for ipv4, use sockaddr_in for ipv6, or sockaddr_storage if you don't know and then typcast to the correct one after)
		-socklen_t addrsize = sizeof(struct sockaddr_in)
		-int newClientSockfd = accept();
		-if newClientSockfd == -1
			throw error
		-if accept was successful, add new fd to _pfds and set to POLLIN
			-fd reports it is ready to send data to socket
		-update count
		-add to our _pfds
		NOTE: to update our _pfds, I think we should use a container so we can always check its size
			if we ever need to expand our array, we can just delete the array and then create a new one from the vector so we don't have to keep resizing it
	b. if it's the client
		int nbytes = recv()
		if nbytes <= 0
			error or connection was closed
			close the fd
			make sure to delete this client from _pfds!
		otherwise we've got info! hurray!
			send the data accordingly -> idk how we're going to do this yet 
				but i'm starting to understand why we might need a whole client class
3. check for signals
	ctrl+c will close
	ctrl+d - ??
	ctrl+z - ??

IRC messages are always lines of characters terminated with a CR-LF
(Carriage Return - Line Feed) pair, and these messages SHALL NOT
exceed 512 characters in length, counting all characters including
the trailing CR-LF. Thus, there are 510 characters maximum allowed
for the command and its parameters. - from rfc 2813

TODO:
CHECK FOR EAGAIN OR EWOULDBLOCK
JOIN #channel-name password

3.1 Connection Registration (see rfc 2812)


   The commands described here are used to register a connection with an
   IRC server as a user as well as to correctly disconnect.

   A "PASS" command is not required for a client connection to be
   registered, but it MUST precede the latter of the NICK/USER
   combination (for a user connection) or the SERVICE command (for a
   service connection). The RECOMMENDED order for a client to register
   is as follows:

                           1. Pass message
           2. Nick message                 2. Service message
           3. User message

   Upon success, the client will receive an RPL_WELCOME (for users) or
   RPL_YOURESERVICE (for services) message indicating that the
   connection is now registered and known the to the entire IRC network.
   The reply message MUST contain the full client identifier upon which
   it was registered.

	NOTES:
	check for POLLHUP
	check for POLLOUT

*/

/* 
DRAFTS:

	// hints.ai_family = AF_UNSPEC; //can be ipv4 or 6

	//for science, lets check how many items are in this linked list _serv
	//there will be two if we use AF_UNSPEC - probably an ipv4 and ipv6
	// struct addrinfo *s;
	// s = _serv;
	// int i = 0;
	// for (; s != NULL; s = s->ai_next)
	// 	i++;
	// std::cout << "there are " << i << " structs in _serv\n";


	//510 is max len we can send. see notes
				//I think the parsing goes here -> parse into a message object in client?
				char buf[510] = {}; //can we do this?
				int nbytes = recv(_pfds[i].fd, buf, sizeof(buf), 0); //no flags = 0
				std::cout << "buf: " << buf;
				if (nbytes <= 0)
				{
					std::cout << "recv failed\n";
					deletePfd(_pfds[i].fd, nbytes);
					change = 1;
				}
				else
				{
					std::string buffer = buf;
					std::string ret;
					if (buffer.find("CAP LS") != std::string::npos)
					{
						ret = "CAP * LS :multi-prefix userhost-in-names\r\n";
						// ret = "CAP * LS\r\n";
					}
					else if (buffer.find("CAP REQ") != std::string::npos)
					{
						ret = ": 451   :need to register first\r\nCAP * ACK :multi-prefix\r\n";
					}
					else if (buffer.find("MODE") != std::string::npos)
					{
						ret = ":localhost 403 h hbui-vu :No such channel\r\n";
					}
					else if (buffer.find("PING") != std::string::npos)
					{
						ret = ":localhost PONG localhost :localhost\r\n";
					}
					std::cout << "sending to: " << _pfds[i].fd << "\n";
					if (send(_pfds[i].fd, ret.c_str(), ret.length() + 1, 0) == -1)
						std::cout << "send unsuccessful\n";
					std::cout << "sent " << ret.c_str() << "\n"; 
					// :localhost 001 h :Welcome to the Internet Relay Chat Network user\r\n";
				}
*/
