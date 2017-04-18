Closed and open HTTP traffic generator
======================================

`httpmon` is an HTTP traffic generator used to make experiments related to computing capacity shortage avoidance in cloud computing, a project we call *brownout*. For details about brownout, please refer to [this article](http://www.diva-portal.org/smash/record.jsf?searchId=1&pid=diva2:680477), as this page only focuses on `httpmon` itself.

`httpmon` generates HTTP requests to a single URL using either an open or a closed model. In the open model, the inter-arrival time – the time that two consecutive requests are issued – is exponentially-distributed random, without depending on the system's response, in this case an HTTP server. In the closed model, each client makes a request, waits for the request to be served, waits for an exponentially-distributed random think-time and repeats. As opposed to the open model, inter-arrival time in the closed model depends on the response-time of the system: The average inter-arrival time is equal to the average think-time plus the average response-time. For more details about the open and closed model, and to choose which one, please refer to [this article](http://users.cms.caltech.edu/~adamw/papers/openvsclosed.pdf). In essence, an open model is more appropriate if the system is accessed by many users concurrently featuring short sessions – e.g., below 5 requests per session – whereas the closed model should be used when the system is accessed by few users featuring long sessions – e.g., above 5 requests per session.

Besides generating the traffic, `httpmon` periodically reports certain statistics related to the requests that have been served. Statistics are displayed both for the requests that finished during the last reporting period and for all requests, since `httpmon`'s start. The following metrics are reported:

* response-time, also called user-perceived *latency*, in the format: `minimum:lower-quartile:median:upper-quartile:maximum:(average)`;
* 95 and 99-percentile latency;
* throughput, i.e., requests per second;
* queue length, i.e., number of requests sent which have yet to receive a reply;
* rate and accumulated number of requests containing option 1;
* rate and accumulated number of requests containing option 2.

A requests is considered to contain option 1 and 2, if the byte 128 and 129, respectively, are present somewhere in the body of the HTTP reply.

Compiling
---------

`httpmon` is supposed to work on all Unix-like operating systems, but has only been tested on Ubuntu Linux (12.04.2 LTS and 13.10).
To compile, you need the following prerequisites:

* GNU compiler collection >= 4.6.3
* GNU make >= 3.81
* Boost C++ libraries >= 1.48
* libcurl >= 7.22.0

Installing this software on top of Ubuntu can be achieved using the following commands:

    sudo apt-get install build-essential libboost-all-dev libcurl4-openssl-dev

A primitive `Makefile` is included in the repository:

    make

In future, an automake or Cmake-based configuration may be provided.

Usage
-----

`httpmon`'s command-line is fully documented inside the executable itself:

    ./httpmon --help

Examples:

* Emulate 100 closed clients with think-time 1:

        ./httpmon --url http://example.com/testWebPage --concurrency 100 --thinktime 1

* Emulate open clients with 100 requests/second:

        ./httpmon --url http://example.com/testWebPage --concurrency 100 --thinktime 1 --open

Output
------

This section explains the output of `httpmon`. The tool regularly reports some metrics on standard output. Reporting occurs by default every second, but can be overwritten with the `--reportInterval` command-line option.

Each report is printed on one line, formatted as a list of `metricName=metricValue`. Let us discuss each `metricName`:

* `time=1492213912.126146`: time at which reporting occurred, i.e., the time at which this line was printed, in UNIX timestamp;

The next metrics are related to **requests for which a reply was received during the last report interval**:

* `latency=15:71:89:4994:46723:(9708)ms`:
latency (response time); the format is `minimum:firstQuartile:median:thirdQuartile:maximum:(average)`;

* `latency95=46529ms`, `latency99=46692ms`: 95th and 99th percentile latency;

* `requests=1612`: number of requests;

* `option1=0`, `option2=0`: two metrics relevant for [brownout applications](http://kleinlabs.eu/research.html#brownout-or-how-to-deal-with-capacity-shortage), the number of requests with optional content type 1 and type 2; for simplicity, a reply contains such optional content if it contains the byte 128 or 129, respectively;

* `errors=476`: number of failed requests; causes of failure includes invalid URL, timeout (server overload) or 4xx/5xx HTTP code;

* `throughput=46rps`: `requests` divided by `reportInterval`;

* `ql=0`: queue length, i.e., number of requests sent for which a reply is pending;

* `rr=0.00%`: `option1` divided by `requests`;

* `cr=0.00%`: `option2` divided by `requests`;

The following metrics are related to all requests for which a reply was received **since `httpmon`'s start**:

* `accRequests=277705`: total number of requests

* `accOption1=0 accOption2=0`: total number of requests received with option type 1 and 2 (see above for what this means);

* `accLatency=1:56:80:115:49791:(302)ms`: statistics of latencies (response times), format is `min:firstQuartile:median:lastQuartile:max:(average)`;

* `accLatency95=201ms accLatency99=9592ms`: 95th and 99th percentile latency

* `accOpenQueuing=0`: When using the `--open` parameter, `httpmon` tries to emulate an open-system workload generator, i.e., the performance of the webserver does not slow down the workload generator. When this emulation failed, generally because the webserver was too slow, then this counter is incremented.

* `accErrors=32522`: total number of failed requests.

Note that, if you are interested in latencies (reponse times) of requests individually instead of statistics, then `httpmon` can dump such information if given the `--dump` command-line option.

Contact
-------

For questions or comments, please contact Cristian Klein <firstname.lastname@cs.umu.se>.
