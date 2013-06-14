#include <algorithm>
#include <array>
#include <boost/program_options.hpp>
#include <curl/curl.h>
#include <mutex>
#include <poll.h>
#include <random>
#include <signal.h>
#include <thread>
#include <vector>

#define OPTIONAL_STUFF1 0x01
#define OPTIONAL_STUFF2 0x02

const int OptionalStuffMarker1 = 128;
const int OptionalStuffMarker2 = 129;
const long NanoSecondsInASecond = 1000000000;

typedef struct {
	std::string url;
	int timeout;
	double thinkTime;
	volatile bool running;
	std::mutex mutex;
	uint32_t numErrors;
	uint32_t numOptionalStuff1;
	uint32_t numOptionalStuff2;
	std::vector<double> latencies;
} HttpClientControl;

double inline now()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)tv.tv_usec / 1000000 + tv.tv_sec;
}

template<typename T>
typename T::value_type median(T begin, T end)
{
	/* assumes vector is sorted */
	int n = end - begin;

	if ((n-1) % 2 == 0)
		return *(begin+(n-1)/2);
	else
		return (*(begin+(n-1)/2) + *(begin+(n-1)/2+1))/2;
}

template<typename T>
std::array<typename T::value_type, 5> quartiles(T &a)
{
	/* return value: minimum, first quartile, median, third quartile, maximum */
	std::array<typename T::value_type, 5> ret = {{ NAN, NAN, NAN, NAN, NAN }};

	size_t n = a.size();
	if (n < 1)
		return ret;
	std::sort(a.begin(), a.end());

	ret[0] = a[0]  ; /* minimum */
	ret[4] = a[n-1]; /* maximum */

	ret[2] = median(a.begin(), a.end());
	ret[1] = median(a.begin(), a.begin() + n / 2);
	ret[3] = median(a.begin() + n / 2, a.end());

	return ret;
}

template<typename T>
double average(const T &a)
{
	double sum = std::accumulate(a.begin(), a.end(), 0.0);
	return sum / a.size();
}

size_t nullWriter(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	uint32_t *optionalStuff = (uint32_t *)userdata;

	if (memchr(ptr, OptionalStuffMarker1, size*nmemb) != NULL)
		*optionalStuff |= OPTIONAL_STUFF1;
	if (memchr(ptr, OptionalStuffMarker2, size*nmemb) != NULL)
		*optionalStuff |= OPTIONAL_STUFF2;
	return size * nmemb; /* i.e., pretend we are actually doing something */
}

int httpClientMain(int id, HttpClientControl &control)
{
	uint32_t optionalStuff;

	CURL *curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_URL, control.url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullWriter);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullWriter);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, control.timeout);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &optionalStuff);

	std::default_random_engine rng; /* random number generator */
	std::exponential_distribution<double> waitDistribution(1 / control.thinkTime);

	rng.seed(now() + id);

	while (control.running) {
		/* Send HTTP request */
		double start = now();
		optionalStuff = 0;
		bool error = (curl_easy_perform(curl) != 0);
		double latency = now() - start;

		/* Add data to report */
		/* XXX: one day, this might be a bottleneck */
		{
			std::lock_guard<std::mutex> lock(control.mutex);
			if (error)
				control.numErrors ++;
			if (optionalStuff & OPTIONAL_STUFF1)
				control.numOptionalStuff1 ++;
			if (optionalStuff & OPTIONAL_STUFF2)
				control.numOptionalStuff2 ++;
			control.latencies.push_back(latency);
		}

		/* If an error has occured, we might spin and lock "control" */
		if (error)
			usleep(0);

		if (control.thinkTime > 0) {
			double wait = waitDistribution(rng);
			usleep(wait * 1000000);
		}
	}
	curl_easy_cleanup(curl);

	return 0;
}

void report(HttpClientControl &control, double &lastReportTime, int &totalRequests)
{
	/* Atomically retrieve relevant data */
	int numErrors;
	int numOptionalStuff1;
	int numOptionalStuff2;
	std::vector<double> latencies;
	double reportTime;
	{
		std::lock_guard<std::mutex> lock(control.mutex);

		numErrors = control.numErrors;
		numOptionalStuff1 = control.numOptionalStuff1;
		numOptionalStuff2 = control.numOptionalStuff2;
		latencies = control.latencies;

		control.numErrors = 0;
		control.numOptionalStuff1 = 0;
		control.numOptionalStuff2 = 0;
		control.latencies.clear();
		reportTime = now();
	}

	double throughput = (double)latencies.size() / (reportTime - lastReportTime);
	double recommendationRate = (double)numOptionalStuff1 / latencies.size();
	double commentRate = (double)numOptionalStuff2 / latencies.size();
	auto latencyQuartiles = quartiles(latencies);
	lastReportTime = reportTime;
	totalRequests += latencies.size();

	fprintf(stderr, "[%f] latency=%.0f:%.0f:%.0f:%.0f:%.0f:(%.0f)ms throughput=%.0frps rr=%.2f%% cr=%.2f%% errors=%d total=%d\n",
		reportTime,
		latencyQuartiles[0] * 1000,
		latencyQuartiles[1] * 1000,
		latencyQuartiles[2] * 1000,
		latencyQuartiles[3] * 1000,
		latencyQuartiles[4] * 1000,
		average(latencies) * 1000,
		throughput,
		recommendationRate * 100,
		commentRate * 100,
		numErrors, totalRequests);
}

int main(int argc, char **argv)
{
	namespace po = boost::program_options;

	/*
	 * Initialize
	 */
	std::string url;
	int concurrency;
	int timeout;
	double thinkTime;
	double interval;

	/*
	 * Parse command-line
	 */
	po::options_description desc("Real-time monitor of a HTTP server's throughput and latency");
	desc.add_options()
		("help", "produce help message")
		("url", po::value<std::string>(&url), "set URL to request")
		("concurrency", po::value<int>(&concurrency)->default_value(100), "set concurrency (number of HTTP client threads)")
		("timeout", po::value<int>(&timeout)->default_value(0), "set HTTP client timeout in seconds")
		("thinktime", po::value<double>(&thinkTime)->default_value(0), "add a random (à la Poisson) interval between requests in seconds")
		("interval", po::value<double>(&interval)->default_value(1), "set report interval in seconds")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << "\n";
		return 1;
	}
	
	if (url.empty()) {
		std::cerr << "Warning, empty URL given. Expect high CPU usage and many errors." << std::endl;
	}

	/*
	 * Start HTTP client threads
	 */

	curl_global_init(CURL_GLOBAL_ALL);

	/* Setup thread control structure */
	HttpClientControl control;
	control.running = true;
	control.url = url;
	control.timeout = timeout;
	control.thinkTime = thinkTime;
	control.numErrors = 0;
	control.numOptionalStuff1 = 0;
	control.numOptionalStuff2 = 0;

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
	double lastReportTime = now();
	int totalRequests = 0;
	while (control.running) {
		struct timespec timeout = { int(interval), int((interval-(int)interval) * NanoSecondsInASecond)};
		signo = sigtimedwait(&sigset, NULL, &timeout);

		if (signo > 0)
			control.running = false;

		report(control, lastReportTime, totalRequests);
	}
	fprintf(stderr, "Got signal %d, cleaning up ...\n", signo);

	/*
	 * Cleanup
	 */
	for (int i = 0; i < concurrency; i++) {
		httpClientThreads[i].join();
	}
	curl_global_cleanup();

	/* Final stats */
	report(control, lastReportTime, totalRequests);

	return 0;
}
