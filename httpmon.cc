#include <boost/program_options.hpp>
#include <poll.h>
#include <signal.h>
#include <thread>

typedef struct {
	std::string url;
	volatile bool running;
} HttpClientControl;

int httpClientMain(int id, HttpClientControl &control)
{
	fprintf(stderr, "Thread %d executing\n", id);

	while (control.running) {
		sleep(1);
	}
	
	fprintf(stderr, "Thread %d stopping\n", id);

	return 0;
}

int main(int argc, char **argv)
{
	namespace po = boost::program_options;

	/*
	 * Initialize
	 */
	std::string url;
	int concurrency;
	int runfor;

	/*
	 * Parse command-line
	 */
	po::options_description desc("Real-time monitor of a HTTP server's throughput and latency");
	desc.add_options()
		("help", "produce help message")
		("url", po::value<std::string>(&url), "set URL to request")
		("concurrency", po::value<int>(&concurrency)->default_value(1000), "set concurrency (number of HTTP client threads)")
		("runfor", po::value<int>(&runfor)->default_value(-1), "set time to run for in seconds (-1 = infinity)")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << "\n";
		return 1;
	}

	/*
	 * Start HTTP client threads
	 */

	/* Setup thread control structure */
	HttpClientControl control;
	control.running = true;
	control.url = url;

	/* Start client threads */
	std::thread httpClientThreads[concurrency];
	for (int i = 0; i < concurrency; i++) {
		httpClientThreads[i] = std::thread(httpClientMain, i, std::ref(control));
	}

	/*
	 * Let client threads work, until user interrupts us
	 */

	/* Block SIGINT and SIGQUIT */
	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGQUIT);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	int sig;
	sigwait(&sigset, &sig);
	fprintf(stderr, "Got signal %d, cleaning up ...\n", sig);

	/*
	 * Cleanup
	 */
	control.running = false;
	for (int i = 0; i < concurrency; i++) {
		httpClientThreads[i].join();
	}

	return 0;
}
