FROM ubuntu:14.04
MAINTAINER Cristian Klein <cristian.klein@cs.umu.se>

RUN apt-get -qq update
RUN apt-get -qqy install build-essential libboost-all-dev libcurl4-openssl-dev
RUN apt-get -qqy install git
RUN git clone https://github.com/cloud-control/httpmon
RUN cd httpmon; make

ENTRYPOINT ["/httpmon/httpmon"]
CMD ["--help"]
