FROM ubuntu:15.10
MAINTAINER Cristian Klein <cklein@cs.umu.se>

RUN apt-get -qq update \
    && apt-get -qqy install \
        build-essential \
        libboost-all-dev \
        libcurl4-openssl-dev \
        libssl-dev \
    && rm -rf /var/lib/apt/lists/* 
COPY * /httpmon/
RUN \
    make -C /httpmon \
    && cp /httpmon/httpmon /usr/bin/httpmon \
    && rm -r /httpmon

ENTRYPOINT ["/usr/bin/httpmon"]
CMD ["--help"]
