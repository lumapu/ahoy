############################
# build executable binary
############################

FROM python:slim-bullseye

COPY . /hoymiles
WORKDIR /hoymiles

RUN python3 -m pip install pyrf24 influxdb_client && \
python3 -m pip list #watch for RF24 module - if its there its installed

RUN pip install crcmod pyyaml paho-mqtt SunTimes

CMD python3 -um hoymiles --log-transactions --verbose --config /etc/ahoy.yml
