#include "Server.hpp"

bool	Server::_run = true;

int main(int argc, char **argv){
	if (argc != 3)
	{
		std::cout << "Usage: ./irc <port> <password>\n";
		Server::printPasswordPolicy();
		return (1);
	}
	
	try
	{
		// Server::setSignals();
		Server server(argv[1], argv[2]);
		server.createServer();
	}
	catch(Server::InvalidPasswordException())
	{
		Server::printPasswordPolicy();
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}
	
}