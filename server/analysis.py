import orjson as json
from dateutil.parser import parse as dateparser

from pathlib import Path
from datetime import datetime, timedelta
from collections import deque


class Analysis:
    def __init__(self, data_path="./data/", threshold=20, post_session_timeout=15, buffer_length=32, hard_threshold=100):
        self.data_path = data_path
        self.threshold = threshold
        self.hard_threshold = hard_threshold
        self.post_session_timeout = timedelta(seconds=post_session_timeout)
        
        self.start_time = None
        self.end_time = None
        self.pre_session_weight = None
        self.post_session_buffer = []

        self.data = []
        self.buffer = deque(maxlen=buffer_length)
        self.buffer_length = buffer_length
        self.dirty = False
        self.__average = None

    def _buffer_average(self):
        if self.dirty or self.__average is None:
            try:
                self.__average = sum(self.buffer) / len(self.buffer)
            except ZeroDivisionError:
                return 0
            self.dirty = False
        return self.__average

    def add(self, sample):
        if not self.buffer:
            self.dirty = True
            self.buffer.append(sample)
            return "starting"
        average = self._buffer_average()
        now = datetime.now()
        if sample < -self.hard_threshold and sample < average * (1 - self.threshold):
            # Litter box taken off of sensor
            return "cleaning"
        elif sample > self.hard_threshold and sample > average * (1 + self.threshold):
            # Cat is in the litterbox
            self.start_time = now
            self.end_time = None
            self.pre_session_weight = self._buffer_average()
        elif self.start_time and not self.end_time:
            # Cat was on litterbox and has left
            self.end_time = now

        if self.end_time:
            if now - self.end_time >= self.post_session_timeout:
                post_session_weight = sum(self.post_session_buffer) / len(self.post_session_buffer)
                self.record_session(self.data, self.pre_session_weight, post_session_weight, self.start_time, self.end_time)
                self.start_time = None
                self.end_time = None
                self.post_session_buffer.clear()
            else:
                self.post_session_buffer.append(sample)

        if self.start_time and not self.end_time:
            self.data.append(sample)
            return "session"
        else:
            self.dirty = True
            self.buffer.append(sample)
            return "waiting"

    def record_session(self, data, pre_session_weight, post_session_weight, start_time, end_time):
        mean_weight = sum(data) / len(data)
        data.sort()
        median_weight = data[len(data) // 2]
        sample = {
            "pre_session_weight": pre_session_weight,
            "post_session_weight": post_session_weight,
            "mean_weight": mean_weight,
            "median_weight": median_weight,
            "start_time": start_time,
            "end_time": end_time,
            "data": data,
        }
        filepath = self._data_filename(start_time)
        filepath.parent.mkdir(parents=True, exist_ok=True)
        with open(filepath, "ab+") as fd:
            fd.write(json.dumps(sample))
            fd.write(b'\n')

    def _data_filename(self, time):
        return Path(f"{self.data_path}/{time.year:04d}/{time.month:02d}/{time.day:02d}.jsonl")

    def load_date(self, date):
        try:
            with open(self._data_filename(date), 'rb') as fd:
                for line in fd:
                    datum = json.loads(line.strip())
                    datum['start_time'] = dateparser(datum['start_time'])
                    datum['end_time'] = dateparser(datum['end_time'])
                    yield datum
        except FileNotFoundError:
            return

    def get_data(self, start_time, end_time):
        delta = timedelta(days=1)
        cur_date = start_time
        while cur_date <= end_time:
            data = self.load_date(cur_date)
            for d in data:
                if (
                    start_time <= d["start_time"] <= end_time
                    or start_time <= d["end_time"] <= end_time
                ):
                    yield d
            cur_date += delta

