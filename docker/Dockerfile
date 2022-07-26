FROM phusion/baseimage:bionic-1.0.0
LABEL maintainer="Wagerr Developers <dev@wagerr.com>"
LABEL description="Dockerised Wagerr, built from Travis"

ARG USER_ID
ARG GROUP_ID

ENV HOME /wagerr

# add user with specified (or default) user/group ids
ENV USER_ID ${USER_ID:-1000}
ENV GROUP_ID ${GROUP_ID:-1000}
RUN groupadd -g ${GROUP_ID} wagerr
RUN useradd -u ${USER_ID} -g wagerr -s /bin/bash -m -d /wagerr wagerr
RUN mkdir /wagerr/.wagerrcoin
RUN chown wagerr:wagerr -R /wagerr

COPY bin/* /usr/local/bin/

RUN chmod a+x /usr/local/bin/*

USER wagerr

VOLUME ["/wagerr"]

EXPOSE 55003 55002 55005 55004

WORKDIR /wagerr
