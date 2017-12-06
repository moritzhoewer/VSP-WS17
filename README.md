# vslab-riot
Scaffold for practical exercise with RIOT, further information on lecture
[here](http://www.inet.haw-hamburg.de/teaching/ws-2017-18/verteilte-systeme).

This exercise is based on [RIOT-OS](https://riot-os.org), an open source OS for
the IoT. Its source code is on [Github](https://github.com/RIOT-OS/RIOT), more
Information can be found in the [RIOT-Wiki](https://github.com/RIOT-OS/RIOT/wiki),
there are [Tutorials](https://github.com/RIOT-OS/Tutorials), and the RIOT API
[documentation](https://doc.riot-os.org).

Please refer to the wiki giving a more detailed introduction, see parts
[one](https://github.com/inetrg/vslab-riot/wiki/vslab-riot-part1) and
[two](https://github.com/inetrg/vslab-riot/wiki/vslab-riot-part2).

## Quickstart guide

The following commands should run on most systems with any Linux and macOS.

```
git clone https://github.com/inetrg/vslab-riot.git
cd vslab-riot
git submodule update --init
make -C src clean all
```

To run the application multiple times, setup a TAP bridge with several TAP
devices. RIOT provides a helper script for that called `tapsetup`.

* Create a bridge with 5 TAP devices (`tap0` - `tap4`):
```
./RIOT/dist/tools/tapsetup/tapsetup -c 5
```

* Create single application instance:
```
PORT=tap0 make -C src term
```

* Open a new shell and repeat the previous command with `PORT=tap1` ... `PORT=tap4`

----

The application will run on most boards supported by RIOT, if available it
reads temperature values from a HDC1000 sensor. This sensor is available on
the PhyTec PhyWave (RIOT board name `pba-d-01-kw2x`), also see the
[RIOT-Wiki](https://github.com/RIOT-OS/RIOT/wiki/Board%3A-Phytec-phyWAVE-KW22).

Compile and flash the application on this board (or replace `BOARD=<name>`) as
  follows:
```
BOARD=pba-d-01-kw2x make -C clean all flash
```
