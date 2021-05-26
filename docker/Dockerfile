FROM phusion/baseimage:bionic-1.0.0
LABEL maintainer="Bytz Developers <dev@bytz.gg>"
LABEL description="Dockerised Bytz, built from Travis"

ARG USER_ID
ARG GROUP_ID

ENV HOME /bytz

# add user with specified (or default) user/group ids
ENV USER_ID ${USER_ID:-1000}
ENV GROUP_ID ${GROUP_ID:-1000}
RUN groupadd -g ${GROUP_ID} bytz
RUN useradd -u ${USER_ID} -g bytz -s /bin/bash -m -d /bytz bytz
RUN mkdir /bytz/.bytz
RUN chown bytz:bytz -R /bytz

COPY bin/* /usr/local/bin/

RUN chmod a+x /usr/local/bin/*

USER bytz

VOLUME ["/bytz"]

EXPOSE 9998 9999 19998 19999

WORKDIR /bytz
