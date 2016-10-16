#include <enet/enet.h>
#include <spdlog/spdlog.h>
#include <stdio.h>
#include<stdlib.h>
#include <signal.h>
#include <string.h>
#include <iostream>
#include <memory>

bool isExit = false;
namespace spd = spdlog;
void cs(int n)
{
	if (n == SIGINT)
	{
		isExit = true;
	}
}

void async_example();
void syslog_example();
void user_defined_example();
void err_handler_example();


int main(int argc, char ** argv)
{
	auto console = spd::stdout_color_mt("console");
	console->info("Welcome to spdlog!");
	console->error("Some error message with arg{}..", 1);

	// Formatting examples
	console->warn("Easy padding in numbers like {:08d}", 12);
	console->critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
	console->info("Support for floats {:03.2f}", 1.23456);
	console->info("Positional args are {1} {0}..", "too", "supported");
	console->info("{:<30}", "left aligned");

	spd::get("console")->info("loggers can be retrieved from a global registry using the spdlog::get(logger_name) function");

	// Create basic file logger (not rotated)
	auto my_logger = spd::basic_logger_mt("basic_logger", "./logs/basic.txt");
	my_logger->info("Some log message");

	// Create a file rotating logger with 5mb size max and 3 rotated files
	auto rotating_logger = spd::rotating_logger_mt("some_logger_name", "logs/mylogfile", 1048576 * 5, 3);
	for (int i = 0; i < 10; ++i)
		rotating_logger->info("{} * {} equals {:>10}", i, i, i*i);

	// Create a daily logger - a new file is created every day on 2:30am
	auto daily_logger = spd::daily_logger_mt("daily_logger", "logs/daily", 2, 30);
	// trigger flush if the log severity is error or higher
	daily_logger->flush_on(spd::level::err);
	daily_logger->info(123.44);

	// Customize msg format for all messages
	spd::set_pattern("*** [%H:%M:%S %z] [thread %t] %v ***");
	rotating_logger->info("This is another message with custom format");


	// Runtime log levels
	spd::set_level(spd::level::info); //Set global log level to info
	console->debug("This message shold not be displayed!");
	console->set_level(spd::level::debug); // Set specific logger's log level
	console->debug("This message shold be displayed..");

	// Compile time log levels
	// define SPDLOG_DEBUG_ON or SPDLOG_TRACE_ON
	SPDLOG_TRACE(console, "Enabled only #ifdef SPDLOG_TRACE_ON..{} ,{}", 1, 3.23);
	SPDLOG_DEBUG(console, "Enabled only #ifdef SPDLOG_DEBUG_ON.. {} ,{}", 1, 3.23);

	// Asynchronous logging is very fast..
	// Just call spdlog::set_async_mode(q_size) and all created loggers from now on will be asynchronous..
	async_example();

	// syslog example. linux/osx only
	syslog_example();

	// Log user-defined types example
	user_defined_example();

	// Change default log error handler
	err_handler_example();

	// Apply a function on all registered loggers
	spd::apply_all([&](std::shared_ptr<spdlog::logger> l)
	{
		l->info("End of example.");
	});

	// Release and close all loggers
	spdlog::drop_all();



	signal(SIGINT, cs);
	if (enet_initialize() != 0)
	{
		fprintf(stderr, "An error occurred while initializing ENet.\n");
		return EXIT_FAILURE;
	}
	ENetAddress address;
	ENetHost * server;
	/* Bind the server to the default localhost.     */
	/* A specific host address can be specified by   */
	/* enet_address_set_host (& address, "x.x.x.x"); */
	enet_address_set_host (&address, "0.0.0.0");
	//address.host = ENET_HOST_ANY;
	/* Bind the server to port 1234. */
	address.port = 18888;
	server = enet_host_create(&address /* the address to bind the server host to */,
		32      /* allow up to 32 clients and/or outgoing connections */,
		2      /* allow up to 2 channels to be used, 0 and 1 */,
		0      /* assume any amount of incoming bandwidth */,
		0      /* assume any amount of outgoing bandwidth */);
	if (server == NULL)
	{
		fprintf(stderr,
			"An error occurred while trying to create an ENet server host.\n");
		exit(EXIT_FAILURE);
	}
	ENetHost * client;
	client = enet_host_create(NULL /* create a client host */,
		1 /* only allow 1 outgoing connection */,
		2 /* allow up 2 channels to be used, 0 and 1 */,
		57600 / 8 /* 56K modem with 56 Kbps downstream bandwidth */,
		14400 / 8 /* 56K modem with 14 Kbps upstream bandwidth */);
	if (client == NULL)
	{
		fprintf(stderr,
			"An error occurred while trying to create an ENet client host.\n");
		exit(EXIT_FAILURE);
	}

	ENetEvent event;
	/* Wait up to 1000 milliseconds for an event. */
	while (!isExit) {
		int ret;
		while (ret = enet_host_service(server, &event, 10) > 0)
		{
			printf("event.type = %d\n", event.type);
			switch (event.type)
			{
			case ENET_EVENT_TYPE_CONNECT:{
				printf("A new client connected from %x:%u.\n",
					event.peer->address.host,
					event.peer->address.port);
				/* Store any relevant client information here. */
				event.peer->data = (char*)"Client information";
				break;
			}
			case ENET_EVENT_TYPE_RECEIVE:{
				printf("A packet of length %u containing %s was received from %s on channel %u.\n",
					event.packet->dataLength,
					event.packet->data,
					event.peer->data,
					event.channelID);
				/* Clean up the packet now that we're done using it. */
				enet_packet_destroy(event.packet);
				ENetPacket * packet = enet_packet_create("world", strlen("world")+1, ENET_PACKET_FLAG_RELIABLE);
				enet_peer_send(event.peer, 0, packet);
				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT:
				printf("%s disconnected.\n", event.peer->data);
				/* Reset the peer's client information. */
				event.peer->data = NULL;
			}
		}
		//std::this_thread::sleep_for(std::chrono::milliseconds(20));
		
	}

	enet_host_destroy(client);
	enet_host_destroy(server);
	atexit(enet_deinitialize);
}


void async_example()
{
    size_t q_size = 4096; //queue size must be power of 2
    spdlog::set_async_mode(q_size);
    auto async_file = spd::daily_logger_st("async_file_logger", "logs/async_log.txt");
    for (int i = 0; i < 100; ++i)
        async_file->info("Async message #{}", i);
}

//syslog example
void syslog_example()
{
#ifdef SPDLOG_ENABLE_SYSLOG 
    std::string ident = "spdlog-example";
    auto syslog_logger = spd::syslog_logger("syslog", ident, LOG_PID);
    syslog_logger->warn("This is warning that will end up in syslog..");
#endif
}

// user defined types logging by implementing operator<<
struct my_type
{
    int i;
    template<typename OStream>
    friend OStream& operator<<(OStream& os, const my_type &c)
    {
        return os << "[my_type i="<<c.i << "]";
    }
};

#include <spdlog/fmt/ostr.h> // must be included
void user_defined_example()
{
    spd::get("console")->info("user defined type: {}", my_type { 14 });
}

//
//custom error handler
//
void err_handler_example()
{   
    spdlog::set_error_handler([](const std::string& msg) {
        std::cerr << "my err handler: " << msg << std::endl;
    }); 
    // (or logger->set_error_handler(..) to set for specific logger)
}