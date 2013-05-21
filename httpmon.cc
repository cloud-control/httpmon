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
	uint32_t numRequests;
	double minLatency, maxLatency, sumLatency;
} HttpClientControl;

double inline now()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)tv.tv_usec / 1000000 + tv.tv_sec;
}

size_t nullWriter(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	return size * nmemb; /* i.e., pretend we are actually doing something */
}

int httpClientMain(int id, HttpClientControl &control)
{
	CURL *curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_URL, control.url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullWriter);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);

	while (control.running) {
		/* Send HTTP request */
		double start = now();
		bool error = (curl_easy_perform(curl) != 0);
		double latency = now() - start;

		/* Add data to report */
		/* XXX: one day, this might be a bottleneck */
		{
			std::lock_guard<std::mutex> lock(control.mutex);
			if (error)
				control.numErrors ++;
			control.numRequests ++;
			control.sumLatency += latency;
			control.minLatency = std::min(control.minLatency, latency);
			control.maxLatency = std::max(control.maxLatency, latency);
		}
	}
	curl_easy_cleanup(curl);

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
		double minLatency, avgLatency, maxLatency, throughput;
		{
			std::lock_guard<std::mutex> lock(control.mutex);
			numErrors = control.numErrors;
			minLatency = control.minLatency;
			maxLatency = control.maxLatency;
			avgLatency = control.sumLatency / control.numRequests;
			throughput = control.numRequests /* XXX: divided by measurement interval which is slightly higher than 1 */;

			control.numRequests = 0;
			control.numErrors = 0;
			control.minLatency = INFINITY;
			control.maxLatency = 0;
			control.sumLatency = 0;
		}
		fprintf(stderr, "[%6ld.%06ld] latency=%d:%d:%dms throughput=%drps errors=%d\n", now.tv_sec, now.tv_usec, int(minLatency*1000), int(avgLatency*1000), int(maxLatency*1000), int(throughput), numErrors);
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
