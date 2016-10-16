FROM ubuntu:latest
RUN cat /dev/null >> /etc/apt/sources.list
RUN echo 'deb http://mirrors.163.com/ubuntu/ precise main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb http://mirrors.163.com/ubuntu/ precise-security main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb http://mirrors.163.com/ubuntu/ precise-updates main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb http://mirrors.163.com/ubuntu/ precise-proposed main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb http://mirrors.163.com/ubuntu/ precise-backports main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb-src http://mirrors.163.com/ubuntu/ precise main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb-src http://mirrors.163.com/ubuntu/ precise-security main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb-src http://mirrors.163.com/ubuntu/ precise-updates main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb-src http://mirrors.163.com/ubuntu/ precise-proposed main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb-src http://mirrors.163.com/ubuntu/ precise-backports main restricted universe multiverse' >> /etc/apt/sources.list

RUN echo 'deb http://tw.archive.ubuntu.com/ubuntu precise main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb http://tw.archive.ubuntu.com/ubuntu precise-security main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb http://tw.archive.ubuntu.com/ubuntu precise-updates main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb http://tw.archive.ubuntu.com/ubuntu precise-backports main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb http://tw.archive.ubuntu.com/ubuntu precise-proposed main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb-src http://tw.archive.ubuntu.com/ubuntu precise main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb-src http://tw.archive.ubuntu.com/ubuntu precise-security main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb-src http://tw.archive.ubuntu.com/ubuntu precise-updates main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb-src http://tw.archive.ubuntu.com/ubuntu precise-backports main restricted universe multiverse' >> /etc/apt/sources.list
RUN echo 'deb-src http://tw.archive.ubuntu.com/ubuntu precise-proposed main restricted universe multiverse' >> /etc/apt/sources.list

RUN apt-get -y update
RUN apt-get install -y vim
RUN apt-get install -y cmake
RUN apt-get install -y clang