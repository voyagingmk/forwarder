# include "utils.h"

using namespace std;

std::string readFile(const string &fileName)
{
	ifstream ifs(fileName.c_str(), ios::in | ios::binary | ios::ate);

	ifstream::pos_type fileSize = ifs.tellg();
	ifs.seekg(0, ios::beg);

	vector<char> bytes(fileSize);
	ifs.read(&bytes[0], fileSize);

	return string(&bytes[0], fileSize);
}

bool isFileExist(const char *fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}

bool setupLogger() {
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(make_shared<spdlog::sinks::daily_file_sink_st>("daily", "txt", 0, 0));
#ifdef _MSC_VER
	sinks.push_back(make_shared<spdlog::sinks::wincolor_stdout_sink_st>());
#else
	sinks.push_back(make_shared<spdlog::sinks::stdout_sink_st>());
#endif
	auto logger = make_shared<spdlog::logger>("my_logger", begin(sinks), end(sinks));
	spdlog::register_logger(logger);
	logger->flush_on(spdlog::level::err);
	spdlog::set_pattern("[%D %H:%M:%S:%e][%l] %v");
	spdlog::set_level(spdlog::level::info);
	logger->set_level(spdlog::level::debug);
	logger->info("logger created successfully.");
	return true;
}