FROM ubuntu:latest

RUN apt-get update

COPY ../stack_0.1-1_amd64.deb /tmp/
RUN dpkg -i /tmp/stack-0.1-1.amd64.deb
RUN rm -f /tmp/stack-0.1-1.amd64.deb

ENTRYPOINT /usr/bin/stack
CMD [ "/usr/bin/stack" "-n" "1000" ] 
