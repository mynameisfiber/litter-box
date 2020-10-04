# Litter Box

> My cat's litter box was boring, so I added a sensor on it


## Hardware

First, take some weight scales and conect it to an HX711. Connect that to a particle photon and register the device under the name `litter-box`.

To flash the firmware, go into the [firmware](firmware) directory and run `make
flash`. Make sure to modify the static variables in
[litter-box.ino](firmware/litter-box.ino) to match your setup.


## Software

To run the setup, build and run the docker image,

```bash
$ docker build -t $(whoami)/litter-box .
$ docker run -p 8080:80 $(whoami)/litterbox:latest
```

Data files will be written to `/data/` in the docker container.


### API

- **/measure**: Add a measurement to the system. Takes a float `value` parameter
- **/data/<?start_time>/<?end_time>**: Shows all litter-box sessions by your cat. start_time defaults to 7 days ago and end_time defaults to now.
