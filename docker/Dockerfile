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

RUN apt-get -y -o Dpkg::Progress-Fancy="1" update
RUN apt-get install -y -o Dpkg::Progress-Fancy="1" vim 
RUN apt-get install -y -o Dpkg::Progress-Fancy="1" cmake
RUN apt-get install -y -o Dpkg::Progress-Fancy="1" clang