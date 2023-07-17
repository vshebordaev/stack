FROM ubuntu:latest

RUN apt update && apt upgrade

COPY debian/tmp/stack_0.1-1_amd64.deb /tmp/
RUN dpkg -i /tmp/stack_0.1-1_amd64.deb
RUN rm -f /tmp/stack_0.1-1_amd64.deb

