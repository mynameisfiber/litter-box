FROM python:3.8-slim

WORKDIR /app/litter/

COPY server/requirements.txt .
RUN python -m pip install -r requirements.txt

COPY server/* ./

ENV LITTER_BOX_DATA_PATH=/data/
ENV LITTER_BOX_PORT=80
ENV LITTER_BOX_DEBUG=false

EXPOSE ${LITTER_BOX_PORT}
CMD python server.py
