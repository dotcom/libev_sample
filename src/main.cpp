#include <Burst.hpp>

int main(){
	Burst::Server server(INADDR_ANY, 3333);
	server.init();
	server.run();
	return 0;
}
