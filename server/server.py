from datetime import datetime, timedelta
import os

from flask import Flask, request, abort
from dateutil.parser import ParserError, parse as dateparser
import orjson as json

from analysis import Analysis


data_path = os.environ.get("LITTER_BOX_DATA_PATH", "./data/")
port = int(os.environ.get("LITTER_BOX_PORT", 5000))
debug = os.environ.get("LITTER_BOX_DEBUG", "false").lower() == "true"

app = Flask(__name__)
analysis = Analysis(data_path=data_path)


@app.route("/measure")
def measure():
    try:
        value = float(request.args["value"])
    except KeyError:
        return abort(403, "Parameter 'value' required")
    except ValueError:
        return abort(403, "Parameter 'value' not a valid float")
    status = analysis.add(value)
    return status


@app.route("/data")
@app.route("/data/<start_time>")
@app.route("/data/<start_time>/<end_time>")
def data(start_time=None, end_time=None):
    # load start_time
    if start_time is None:
        start_time = datetime.now() - timedelta(days=7)
    else:
        try:
            start_time = dateparser(start_time)
        except ParserError:
            return abort(403, "Invalid start time")
    # load end_time
    if end_time is None:
        end_time = datetime.now()
    else:
        try:
            end_time = dateparser(end_time)
        except ParserError:
            return abort(403, "Invalid end time")
    result = list(analysis.get_data(start_time, end_time))
    return json.dumps(result)


if __name__ == "__main__":
    print("Starting litter-box")
    print("Port:", port)
    print("Data path:", data_path)
    print("Debug:", debug)

    app.run(host="0.0.0.0", port=port, debug=debug)
