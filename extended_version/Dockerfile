FROM ubuntu:16.04

MAINTAINER yaechanKim <burgund32@gmail.com>

RUN apt-get update && \
    apt-get -y install gcc cmake && \
    apt-get clean

WORKDIR /server

EXPOSE 8080

## For Keeping Container From Foreground Empty
CMD ["tail", "-f", "/dev/null"]