FROM ubuntu:latest
RUN cat /dev/null >> /etc/apt/sources.list
RUN echo 'deb http://cn.archive.ubuntu.com/ubuntu/ xenial main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb http://cn.archive.ubuntu.com/ubuntu/ xenial-security main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb http://cn.archive.ubuntu.com/ubuntu/ xenial-updates main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb http://cn.archive.ubuntu.com/ubuntu/ xenial-backports main restricted universe multiverse' >> /etc/apt/sources.list

RUN echo 'deb http://cn.archive.ubuntu.com/ubuntu/ xenial-proposed main restricted universe multiverse' >> /etc/apt/sources.list

RUN echo 'deb-src http://cn.archive.ubuntu.com/ubuntu/ xenial main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb-src http://cn.archive.ubuntu.com/ubuntu/ xenial-security main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb-src http://cn.archive.ubuntu.com/ubuntu/ xenial-updates main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb-src http://cn.archive.ubuntu.com/ubuntu/ xenial-backports main restricted universe multiverse' >> /etc/apt/sources.list

RUN echo 'deb-src http://cn.archive.ubuntu.com/ubuntu/ xenial-proposed main restricted universe multiverse' >> /etc/apt/sources.list

RUN apt-get -y update
RUN apt-get install -y vim
RUN apt-get install -y cmake
RUN apt-get install -y clang