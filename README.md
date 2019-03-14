RediStreaming
=============

## Summary

RediStreaming is a streaming module that leverages the content-based publish/subscribe system of [Redis](https://redis.io) to stream messages to subscribers. Publishers can be simulated by [YCSB](https://github.com/brianfrankcooper/YCSB) instances running defined workloads. A repository containing the application setup for a minimal working example of a word counting streaming module can be found under https://github.com/ChrisG55/streaming.

This module was employed in an use case to measure the energy consumption of hardware exploiting Intel SGX and AMD SEV trusted execution environments. Further details can be found in the published [paper](https://dx.doi.org/10.1109/SRDS.2018.00024).

## Build

`make` is used as build system for RediStreaming. Simply run

```
make
```

to build the Redis module. All files generated during the build process can be removed with

```
make clean
```

## Installation

The RediStreaming module can be loaded into a Redis instance either using a configuration directive in `redis.conf`

```
loadmodule /<path>/<to>/streaming.so
```

or it can be loaded at runtime with the command

```
MODULE LOAD /<path>/<to>/streaming.so
```

When RediStreaming is no longer needed, it can be unloaded from a Redis
instance using the following command:

```
MODULE UNLOAD streaming
```

## Acknowledgement

This work has been supported by EU H2020 ICT project LEGaTO, contract #780681 .
