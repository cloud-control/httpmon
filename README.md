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

Contact
-------

For questions or comments, please contact Cristian Klein <firstname.lastname@cs.umu.se>.
