#include <boost/program_options.hpp>
#include <curl/curl.h>
#include <mutex>
#include <poll.h>
#include <signal.h>
#include <thread>

typedef struct {
	std::string url;
	volatile bool running;
	std::mutex mutex;
	uint32_t numErrors;
} HttpClientControl;

size_t nullWriter(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	return size * nmemb; /* i.e., pretend we are actually doing something */
}

int httpClientMain(int id, HttpClientControl &control)
{
	fprintf(stderr, "Thread %d executing\n", id);

	CURL *curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_URL, control.url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullWriter);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);

	while (control.running) {
		bool error = (curl_easy_perform(curl) != 0);

		std::lock_guard<std::mutex> lock(control.mutex);
		if (error)
			control.numErrors ++;
	}
	curl_easy_cleanup(curl);

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

	curl_global_init(CURL_GLOBAL_ALL);

	/* Setup thread control structure */
	HttpClientControl control;
	control.running = true;
	control.url = url;
	control.numErrors = 0;

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

	/* Report at regular intervals */
	int signo;
	while (control.running) {
		struct timespec timeout = { 1, 0 };
		signo = sigtimedwait(&sigset, NULL, &timeout);

		if (signo > 0)
			control.running = false;

		struct timeval now;
		gettimeofday(&now, NULL);

		int numErrors;
		{
			std::lock_guard<std::mutex> lock(control.mutex);
			numErrors = control.numErrors; control.numErrors = 0;
		}
		fprintf(stderr, "[%6ld.%06ld] errors=%d\n", now.tv_sec, now.tv_usec, numErrors);
	}
	fprintf(stderr, "Got signal %d, cleaning up ...\n", signo);

	/*
	 * Cleanup
	 */
	for (int i = 0; i < concurrency; i++) {
		httpClientThreads[i].join();
	}
	curl_global_cleanup();

	return 0;
}
