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
#include <iostream>
#include <limits>
#include <mutex>
#include <poll.h>
#include <random>
#include <signal.h>
#include <sys/resource.h>
#include <thread>
#include <vector>

#define RESPONSEFLAGS_CONTENT 0x01
#define RESPONSEFLAGS_OPTION1 0x02
#define RESPONSEFLAGS_OPTION2 0x04

const int OptionalStuffMarker1 = 128;
const int OptionalStuffMarker2 = 129;
const long MicroSecondsInASecond = 1000000;
const long NanoSecondsInASecond = 1000000000;

struct ClientControl {
	/* Control */
	volatile bool running;
	std::atomic<int> numRequestsLeft;

	/* Client parameters */
	std::string url;
	int concurrency;
	double thinkTime;
	double timeout;
	bool open;
	bool deterministic;
	bool compressed;
};

struct RequestData {
	double generatedAt; /*< When was the request generated (according to the model) */
	double sentAt; /*< What was the request effectively sent to the server; normally the same as generatedAt, except when client-side queuing happened */
	double repliedAt;
	bool error;
	bool option1;
	bool option2;
};

struct ClientData {
	/* Protect this structure from concurrent access */
	std::mutex mutex;

	/* Data collected by clients */
	std::vector<double> latencies; /* TODO: avoid duplicate with information below */
	std::vector<RequestData> requests;
	uint32_t numRequests;
	uint32_t numOption1;
	uint32_t numOption2;
	uint32_t numOpenQueuing;
	uint32_t numErrors;
	uint32_t queueLength;
};

struct AccumulatedData {
	double reportTime;

	std::vector<double> latencies; /* TODO: avoid duplicate with information below */
	std::vector<RequestData> requests;
	uint32_t numRequests;
	uint32_t numOption1;
	uint32_t numOption2;
	uint32_t numOpenQueuing;
	uint32_t numErrors;
};

double inline now()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)tv.tv_usec / 1000000 + tv.tv_sec;
}

template<typename T>
struct Statistics {
	T minimum, lowerQuartile, median, upperQuartile, maximum;
	T percentile95, percentile99;
	T average;
};

template<typename T>
typename T::value_type median(T begin, T end)
{
	/* assumes vector is sorted */
	int n = end - begin;

	/* median is not defined for fewer than 2 items */
	if (n < 2)
		return NAN;

	/* even number of items: take average of the middle ones */
	if (n % 2 == 0) {
		/* (begin-1) because index computations are 1-based */
		auto left  = (begin-1) + n / 2;
		auto right = (begin-1) + n / 2 + 1;
		return (*left + *right) / 2.0;
	}
	else {
		auto middle = begin + n / 2;
		return *middle;
	}
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
	s.upperQuartile = median(a.begin() + (n+1) / 2, a.end());

	s.percentile95 = a[percentileRank(n, 95)];
	s.percentile99 = a[percentileRank(n, 99)];

	s.average = average(a);

	return s;
}

size_t nullWriter(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	uint32_t *responseFlags = (uint32_t *)userdata;

	if (size * nmemb > 0)
		*responseFlags |= RESPONSEFLAGS_CONTENT;
	if (memchr(ptr, OptionalStuffMarker1, size*nmemb) != NULL)
		*responseFlags |= RESPONSEFLAGS_OPTION1;
	if (memchr(ptr, OptionalStuffMarker2, size*nmemb) != NULL)
		*responseFlags |= RESPONSEFLAGS_OPTION2;
	return size * nmemb; /* i.e., pretend we are actually doing something */
}

int httpClientMain(int id, ClientControl &control, ClientData &data)
{
	/* Block some signals to let master thread deal with them */
	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGQUIT);
	sigaddset(&sigset, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);

	/* Block SIGUSR2 so we can deal with it synchronously */
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR2);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);

	uint32_t responseFlags;

	CURL *curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_URL, control.url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullWriter);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseFlags);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);

	std::default_random_engine rng; /* random number generator */
	double lastThinkTime = control.thinkTime;
	std::exponential_distribution<double> waitDistribution(1.0 / lastThinkTime);

	if (control.deterministic)
		rng.seed(id);
	else
		rng.seed(now() + id);

	double lastArrivalTime = now();
	while (true) {
		RequestData requestData;

		bool didOpenQueuing = false;

		/* Check to see if paramaters have changed and update distribution */
		const double thinkTime = control.thinkTime; /* for atomicity */
		if (lastThinkTime != thinkTime) {
			waitDistribution = std::exponential_distribution<double>(1.0 / thinkTime);
			lastThinkTime = thinkTime;
		}

		/* Simulate think-time */
		/* We make sure that we first wait, then initiate the first connection
		 * to avoid spiky transient effects */
		double interval = 0.0;
		if (thinkTime > 0) {
			interval = waitDistribution(rng);
			requestData.generatedAt = now();

			/* Behave open if requested */
			/* NOTE: Requests may queue up on the client-side if the server is too slow */
			if (control.open) {
				/* Adjust sleep interval, so that it does not depend on response time */
				double nextArrivalTime = lastArrivalTime + interval;
				interval = std::max(nextArrivalTime - now(), 0.0);
				if (interval == 0.0)
					didOpenQueuing++;
				lastArrivalTime = nextArrivalTime;
			}
		}
		/* In worst case, interval == 0, to yield and avoid spinning */

		/* Sleep (or yield) for a while */
		struct timespec timeout = { int(interval), int((interval-(int)interval) * NanoSecondsInASecond)};
		int signo = sigtimedwait(&sigset, NULL, &timeout);

		if (signo > 0)
			break; /* master thread asked us to exit */

		/* Check if we are allowed to send this request */
		if (control.numRequestsLeft-- > 0) {
			/* Set timeout */
			double timeout = control.timeout; /* also for atomicity */
			requestData.generatedAt = now();
			requestData.sentAt = now();
			if (control.open) {
				timeout = std::max(0.0, lastArrivalTime + timeout - now());
				requestData.generatedAt = lastArrivalTime;
			}

			/* Convert to CURL timeout: measure in ms, 0 = infinity */
			/* We use CURL timeout of 1ms instead of 0ms */
			long curlTimeout = 0; /* infinity */
			if (!std::isinf(timeout))
				curlTimeout = std::max(static_cast<long>(timeout * 1000.0), 1L);
			curl_easy_setopt(curl, CURLOPT_URL, control.url.c_str());
			curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, curlTimeout);
			curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, curlTimeout);
			
			struct curl_slist *headers = NULL;
			if (control.compressed) {
				headers = curl_slist_append(headers, "Accept-Encoding: gzip");
			}
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, false);

			/* Send HTTP request */
			{
				std::lock_guard<std::mutex> lock(data.mutex);
				data.queueLength++;
			}
			responseFlags = 0;
			if (timeout > 0) {
				requestData.error = (curl_easy_perform(curl) != 0);
				curl_slist_free_all(headers);
				requestData.repliedAt = now();
			}
			else {
				requestData.error = true; /* user gave up on this request a long time ago */
				requestData.repliedAt = NAN;
			}
			requestData.option1 = responseFlags & RESPONSEFLAGS_OPTION1;
			requestData.option2 = responseFlags & RESPONSEFLAGS_OPTION2;

			/* Add data to report */
			/* XXX: one day, this might be a bottleneck */
			{
				std::lock_guard<std::mutex> lock(data.mutex);

				data.queueLength--;
				data.requests.push_back(requestData);

				data.numRequests++;
				if (requestData.error) {
					data.numErrors++;
				}
				else {
					data.latencies.push_back(requestData.repliedAt - requestData.generatedAt);
					if (requestData.option1)
						data.numOption1 ++;
					if (requestData.option2)
						data.numOption2 ++;
					if (didOpenQueuing)
						data.numOpenQueuing++;
				}
			}
		}
	}

	curl_easy_cleanup(curl);

	return 0;
}

void report(ClientData &_data, AccumulatedData &accData)
{
	/* Atomically retrieve relevant data */
	ClientData data;
	{
		std::lock_guard<std::mutex> lock(_data.mutex);
		data.latencies = std::move(_data.latencies);
		data.requests = std::move(_data.requests);
		data.numRequests = _data.numRequests;
		data.numOption1 = _data.numOption1;
		data.numOption2 = _data.numOption2;
		data.numOpenQueuing = _data.numOpenQueuing;
		data.numErrors = _data.numErrors;
		data.queueLength = _data.queueLength;

		_data.latencies.clear();
		_data.requests.clear();
		_data.numRequests = 0;
		_data.numOption1 = 0;
		_data.numOption2 = 0;
		_data.numOpenQueuing = 0;
		_data.numErrors = 0;
		/* _data.queueLength must not be reset */
	}
	
	/* Compute how much time passed */
	double reportTime = now(); /* for atomicity */
	double dt = reportTime - accData.reportTime;
	accData.reportTime = reportTime;

	/* Compute statistics since last reporting */
	double throughput = (double)data.latencies.size() / dt;
	double recommendationRate = (double)data.numOption1 / data.latencies.size();
	double commentRate = (double)data.numOption2 / data.latencies.size();
	int queueLength = data.queueLength;
	auto stats = computeStatistics(data.latencies);

	/* Compute accumulated statistics */
	accData.numRequests += data.numRequests;
	accData.numOption1 += data.numOption1;
	accData.numOption2 += data.numOption2;
	accData.numOpenQueuing += data.numOpenQueuing;
	accData.numErrors += data.numErrors;
	accData.latencies.insert(accData.latencies.end(), data.latencies.begin(), data.latencies.end());
	accData.requests.insert(accData.requests.end(), data.requests.begin(), data.requests.end());
	auto accStats = computeStatistics(accData.latencies);

	printf("time=%.6f latency=%.0f:%.0f:%.0f:%.0f:%.0f:(%.0f)ms latency95=%.0fms latency99=%.0fms requests=%d option1=%d option2=%d errors=%d throughput=%.0frps ql=%d rr=%.2f%% cr=%.2f%% accRequests=%d accOption1=%d accOption2=%d accLatency=%.0f:%.0f:%.0f:%.0f:%.0f:(%.0f)ms accLatency95=%.0fms accLatency99=%.0fms accOpenQueuing=%d accErrors=%d\n",
		reportTime,
		stats.minimum * 1000,
		stats.lowerQuartile * 1000,
		stats.median * 1000,
		stats.upperQuartile * 1000,
		stats.maximum * 1000,
		stats.average * 1000,
		stats.percentile95 * 1000,
		stats.percentile99 * 1000,
		data.numRequests,
		data.numOption1,
		data.numOption2,
		data.numErrors,
		throughput,
		queueLength,
		recommendationRate * 100,
		commentRate * 100,
		/* Accumulated statistics */
		accData.numRequests,
		accData.numOption1,
		accData.numOption2,
		accStats.minimum * 1000,
		accStats.lowerQuartile * 1000,
		accStats.median * 1000,
		accStats.upperQuartile * 1000,
		accStats.maximum * 1000,
		accStats.average * 1000,
		accStats.percentile95 * 1000,
		accStats.percentile99 * 1000,
		accData.numOpenQueuing,
		accData.numErrors
	);
}

void processInput(std::string &input, ClientControl &control)
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
			
			if (key == "url") {
				control.url = value;
				printf("time=%.6f url=%s\n", now(), control.url.c_str());
			}
			else if (key == "thinktime") {
				control.thinkTime = atof(value.c_str());
				printf("time=%.6f thinktime=%f\n", now(), control.thinkTime);
			}
			else if (key == "concurrency") {
				control.concurrency = atoi(value.c_str());
				printf("time=%.6f concurrency=%d\n", now(), control.concurrency);
			}
			else if (key == "open") {
				control.open = atoi(value.c_str());
				printf("time=%.6f open=%d\n", now(), control.open);
			}
			else if (key == "compressed") {
				control.compressed = atoi(value.c_str());
				printf("time=%.6f compressed=%d\n", now(), control.compressed);
			}
			else if (key == "count") {
				int numRequestsLeft = atoi(value.c_str()); /* avoid race */
				control.numRequestsLeft = numRequestsLeft;
				printf("time=%.6f count=%d\n", now(), numRequestsLeft);
			}
			else if (key == "timeout") {
				control.timeout = atof(value.c_str());
				printf("time=%.6f timeout=%f\n", now(), control.timeout);
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
	double timeout;
	double thinkTime;
	double interval;
	bool open;
	bool compressed;
	bool deterministic;
	bool dump;
	int numRequestsLeft;

	/* Make stdout unbuffered */
	setvbuf(stdout, NULL, _IONBF, 0);

	/*
	 * Parse command-line
	 */
	po::options_description desc("Real-time monitor of a HTTP server's throughput and latency");
	desc.add_options()
		("help", "produce help message")
		("url", po::value<std::string>(&url), "set URL to request")
		("concurrency", po::value<int>(&concurrency)->default_value(100), "set concurrency (number of HTTP client threads)")
		("timeout", po::value<double>(&timeout)->default_value(INFINITY), "set HTTP client timeout in seconds (default: infinity)")
		("thinktime", po::value<double>(&thinkTime)->default_value(0), "add a random (à la Poisson) interval between requests in seconds")
		("interval", po::value<double>(&interval)->default_value(1), "set report interval in seconds")
		("open", "use the open model with client-side queuing, i.e., arrival times do not depend on the response time of the server")
		("compressed", "request the server to GZip compress the response")
		("count", po::value<int>(&numRequestsLeft)->default_value(std::numeric_limits<int>::max()), "stop after sending this many requests (default: do not stop)")
		("deterministic", "do not seed RNG; useful to compare two systems with the exact same requests (default: no)")
		("dump", "dump all data about requests to httpmon-dump.csv")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cerr << desc << "\n";
		return 1;
	}
	
	if (url.empty()) {
		std::cerr << "Warning, empty URL given. Expect high CPU usage and many errors." << std::endl;
	}

	open = vm.count("open");
	compressed = vm.count("compressed");
	deterministic = vm.count("deterministic");
	dump = vm.count("dump");

	/*
	 * Start HTTP client threads
	 */

	curl_global_init(CURL_GLOBAL_ALL);

	/* Set high number of files */
	struct rlimit rlimit_nofile;
	getrlimit(RLIMIT_NOFILE, &rlimit_nofile);
	rlimit_nofile.rlim_cur = rlimit_nofile.rlim_max;
	setrlimit(RLIMIT_NOFILE, &rlimit_nofile);
	if ((int)rlimit_nofile.rlim_cur < concurrency + 10)
		fprintf(stderr, "WARNING: httpmon's file number limit (%d) is lower than concurrency (%d). Expect requests to fail!\n", (int)rlimit_nofile.rlim_max, concurrency);

	/* Setup thread control */
	ClientControl control;
	control.running = true;
	control.numRequestsLeft = numRequestsLeft;
	control.url = url;
	control.concurrency = concurrency;
	control.thinkTime = thinkTime;
	control.timeout = timeout;
	control.open = open;
	control.compressed = compressed;
	control.deterministic = deterministic;

	/* Setup thread data */
	ClientData data;
	data.numRequests = 0;
	data.numOption1 = 0;
	data.numOption2 = 0;
	data.numOpenQueuing = 0;
	data.numErrors = 0;
	data.queueLength = 0;
	
	/* Setup accumulated data */
	AccumulatedData accData;
	accData.numRequests = 0;
	accData.numOption1 = 0;
	accData.numOption2 = 0;
	accData.numOpenQueuing = 0;
	accData.numErrors = 0;

	/* Start client threads */
	std::vector<std::thread> httpClientThreads;
	for (int i = 0; i < concurrency; i++) {
		httpClientThreads.emplace_back(httpClientMain, i,
			std::ref(control), std::ref(data));
	}

	/*
	 * Let client threads work, until user interrupts us
	 */

	/* Block SIGINT, SIGQUIT and SIGTERM */
	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGQUIT);
	sigaddset(&sigset, SIGTERM);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	/* Make stdin non-blocking */
	int flags = fcntl(0, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(0, F_SETFL, flags);

	/* Report at regular intervals */
	int signo;
	accData.reportTime = now();
	std::string prevInput;
	while (control.running) {
		struct timespec timeout = { int(interval), int((interval-(int)interval) * NanoSecondsInASecond)};
		signo = sigtimedwait(&sigset, NULL, &timeout);

		if (signo > 0)
			control.running = false;

		report(data, accData);
		processInput(prevInput, control);

		/* Check if requested concurrency increased */
		while ((int)httpClientThreads.size() < control.concurrency)
			httpClientThreads.emplace_back(httpClientMain,
				httpClientThreads.size(), std::ref(control), std::ref(data));
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
	report(data, accData);

	if (dump) {
		FILE *f = fopen("httpmon-dump.csv", "w");
		if (!f) {
			fprintf(stderr, "Writing to dumpfile failed\n");
			return 1;
		}
	
		fprintf(f, "generatedAt,sentAt,repliedAt,responseTime,option1,option2,error\n");
		for (auto r : accData.requests) {
			fprintf(f, "%f,%f,%f,%f,%d,%d,%d\n",
				r.generatedAt, r.sentAt, r.repliedAt, r.repliedAt - r.generatedAt,
				r.option1, r.option2, r.error);
		}
		fclose(f);
	}

	return 0;
}
