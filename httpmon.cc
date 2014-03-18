#include <algorithm>
#include <array>
#include <atomic>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <curl/curl.h>
#include <fcntl.h>
#include <limits>
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
const long MicroSecondsInASecond = 1000000;
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
	int concurrency;
	bool open;
	std::atomic<uint32_t> numOpenQueuing;
	std::atomic<int> numRequestsLeft;
} HttpClientControl;

template<typename T>
struct Statistics {
	T minimum, lowerQuartile, median, upperQuartile, maximum;
	T percentile95, percentile99;
	T average;
};

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

size_t percentileRank(size_t n, double percentile)
{
	/* Nearest rank: http://en.wikipedia.org/wiki/Percentile#Nearest_rank */
	return static_cast<size_t>(percentile / 100.0 * n - 0.5);
}

template<typename T>
double average(const T &a)
{
	double sum = std::accumulate(a.begin(), a.end(), 0.0);
	return sum / a.size();
}

template<typename T>
Statistics<typename T::value_type> computeStatistics(T &a)
{
	Statistics<typename T::value_type> s =
		{ NAN, NAN, NAN, NAN, NAN, NAN };

	size_t n = a.size();
	if (n < 1)
		return s;
	std::sort(a.begin(), a.end());

	s.minimum = a[0]  ; /* minimum */
	s.maximum = a[n-1]; /* maximum */

	s.median = median(a.begin(), a.end());
	s.lowerQuartile = median(a.begin(), a.begin() + n / 2);
	s.upperQuartile = median(a.begin() + n / 2, a.end());

	s.percentile95 = a[percentileRank(n, 95)];
	s.percentile99 = a[percentileRank(n, 99)];

	s.average = average(a);

	return s;
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
	/* Block SIGUSR2 */
	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR2);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);

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
	double lastThinkTime = control.thinkTime;
	std::exponential_distribution<double> waitDistribution(1 / control.thinkTime);

	rng.seed(now() + id);

	double lastArrivalTime = now();
	while (true) {
		/* Check to see if paramaters have changed and update distribution */
		if (lastThinkTime != control.thinkTime) {
			lastThinkTime = control.thinkTime;
			waitDistribution = std::exponential_distribution<double>(1 / control.thinkTime);
		}
		
		/* Simulate think-time */
		/* We make sure that we first wait, then initiate the first connection
		 * to avoid spiky transient effects */ 
		if (control.thinkTime > 0) {
			double interval = waitDistribution(rng);

			/* Behave open if requested */
			/* NOTE: Requests may queue up on the client-side if the server is too slow */
			if (control.open) {
				/* Adjust sleep interval, so that it does not depend on response time */
				double nextArrivalTime = lastArrivalTime + interval;
				interval = std::max(nextArrivalTime - now(), 0.0);
				if (interval == 0.0)
					control.numOpenQueuing++;
				lastArrivalTime = nextArrivalTime;
			}

			struct timespec timeout = { int(interval), int((interval-(int)interval) * NanoSecondsInASecond)};
			int signo = sigtimedwait(&sigset, NULL, &timeout);

			if (signo > 0)
				break; /* master thread asked us to exit */
		}
		else {
			/* Avoid spinning */
			usleep(0);
		}

		/* Check if we are allowed to send this request */
		if (control.numRequestsLeft-- > 0) {
			/* Send HTTP request */
			double start = now();
			optionalStuff = 0;
			bool error = (curl_easy_perform(curl) != 0);
			double lastLatency = now() - start;

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
				control.latencies.push_back(lastLatency);
			}
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
	auto stats = computeStatistics(latencies);
	lastReportTime = reportTime;
	totalRequests += latencies.size();
	auto numOpenQueuing = control.numOpenQueuing.load();

	fprintf(stderr, "[%f] latency=%.0f:%.0f:%.0f:%.0f:%.0f:(%.0f)ms latency95=%.0fms latency99=%.0fms throughput=%.0frps rr=%.2f%% cr=%.2f%% errors=%d total=%d openqueuing=%d\n",
		reportTime,
		stats.minimum * 1000,
		stats.lowerQuartile * 1000,
		stats.median * 1000,
		stats.upperQuartile * 1000,
		stats.maximum * 1000,
		stats.average * 1000,
		stats.percentile95 * 1000,
		stats.percentile99 * 1000,
		throughput,
		recommendationRate * 100,
		commentRate * 100,
		numErrors, totalRequests,
		numOpenQueuing);
}

void processInput(std::string &input, HttpClientControl &control)
{
	/* Store last size */
	size_t prevInputSize = input.size();

	/* Read new input */
	char buf[1024];
	ssize_t len;
	while ((len = read(0, buf, sizeof(buf))) > 0)
		input.append(buf, len);
	if (input.size() == prevInputSize) /* no new data */
		return;

	/* Parse line by line */
	for (;;) {
		size_t newlineFound = input.find_first_of('\n', prevInputSize);
		if (newlineFound == std::string::npos) {
			/* newline not found */
			return;
		}
		std::string line = input.substr(0, newlineFound);
		input.erase(0, newlineFound + 1);

		/* Tokenize input */
		using namespace boost::algorithm;
		std::vector<std::string> tokens;
		split(tokens, line, is_any_of(" \n"), token_compress_on);

		/* Parse key values */
		for (auto it = tokens.begin(); it != tokens.end(); it++) {
			std::vector<std::string> keyvalue;
			split(keyvalue, *it, is_any_of("="), token_compress_on);
			if (keyvalue.size() != 2) {
				fprintf(stderr, "[%f] cannot parse key-value '%s'\n", now(), it->c_str());
				continue; /* next input token */
			}
			std::string key = keyvalue[0];
			std::string value = keyvalue[1];

			if (key == "thinktime") {
				control.thinkTime = atof(value.c_str());
				fprintf(stderr, "[%f] set thinktime=%f\n", now(), control.thinkTime);
			}
			else if (key == "concurrency") {
				control.concurrency = atoi(value.c_str());
				fprintf(stderr, "[%f] set concurrency=%d\n", now(), control.concurrency);
			}
			else if (key == "open") {
				control.open = atoi(value.c_str());
				fprintf(stderr, "[%f] set open=%d\n", now(), control.open);
			}
			else if (key == "count") {
				int numRequestsLeft = atoi(value.c_str()); /* avoid race */
				control.numRequestsLeft = numRequestsLeft;
				fprintf(stderr, "[%f] set count=%d\n", now(), numRequestsLeft);
			}
			else
				fprintf(stderr, "[%f] unknown key '%s'\n", now(), key.c_str());
		}
	}
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
	bool open;
	int numRequestsLeft;

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
		("open", "use the open model with client-side queuing, i.e., arrival times do not depend on the response time of the server")
		("count", po::value<int>(&numRequestsLeft)->default_value(std::numeric_limits<int>::max()), "stop after sending this many requests (default: do not stop)")
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

	open = vm.count("open");

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
	control.concurrency = concurrency;
	control.open = open;
	control.numOpenQueuing = 0;
	control.numRequestsLeft = numRequestsLeft;

	/* Start client threads */
	std::vector<std::thread> httpClientThreads;
	for (int i = 0; i < concurrency; i++) {
		httpClientThreads.emplace_back(httpClientMain, i, std::ref(control));
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

	/* Make stdin non-blocking */
	int flags = fcntl(0, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(0, F_SETFL, flags);

	/* Report at regular intervals */
	int signo;
	double lastReportTime = now();
	int totalRequests = 0;
	std::string prevInput;
	while (control.running) {
		struct timespec timeout = { int(interval), int((interval-(int)interval) * NanoSecondsInASecond)};
		signo = sigtimedwait(&sigset, NULL, &timeout);

		if (signo > 0)
			control.running = false;

		report(control, lastReportTime, totalRequests);
		processInput(prevInput, control);

		/* Check if requested concurrency increased */
		while ((int)httpClientThreads.size() < control.concurrency)
			httpClientThreads.emplace_back(httpClientMain, httpClientThreads.size(), std::ref(control));
		/* Check if requested concurrency decreased */
		while ((int)httpClientThreads.size() > control.concurrency) {
			pthread_kill(httpClientThreads.back().native_handle(), SIGUSR2);
			httpClientThreads.back().detach();
			httpClientThreads.pop_back();
		}
	}
	fprintf(stderr, "Got signal %d, cleaning up ...\n", signo);

	/*
	 * Cleanup
	 */
	for (auto &thread : httpClientThreads) {
		pthread_kill(thread.native_handle(), SIGUSR2);
	}
	for (auto &thread : httpClientThreads) {
		thread.join();
	}
	curl_global_cleanup();

	/* Final stats */
	report(control, lastReportTime, totalRequests);

	return 0;
}
