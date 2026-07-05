# Deployment Builds

This directory contains Docker builds for the parts of the upstream README that
can be reproduced from the checked-in source:

- `Dockerfile.full` builds the shared `evmtrans` library.
- `Dockerfile.evmtrans` builds the standalone conversion CLI.

These images do not build, start, or validate the geth + Hera testnet flow from
the README section "Execute WASM smart contract". The supported scope here is
source build and bytecode conversion only.

## Source Caveat

The source snapshot in this repository does not reproduce byte-identical output
when compared with the prebuilt binaries distributed by the original authors in
`bin/`.

The changes in this branch are intentionally narrow:

- make the checked-in source build in Docker;
- avoid compiling LLVM/LLD from source;
- make `standalone-evmtrans` usable for conversion experiments;
- allow runtime-level evaluation of generated Wasm.

This should be treated as a practical, reproducible rebuild path for evaluation,
not as a reconstruction of the authors' original release build.

In local checks against the synthetic benchmarks, the rebuilt converter and
`bin/tool-full` both produced valid Wasm. Full deployer outputs were not
byte-identical. Runtime-only outputs generated with `--dump --onlyRt` were much
closer structurally: import/export names, table, memory, and globals matched in
the tested cases, while the generated runtime code still differed.

## LLVM And LLD

The upstream build path downloads and builds LLVM/LLD as external dependencies.
That is slow and heavy for Docker.

These Dockerfiles use Ubuntu 20.04 packages instead:

```text
llvm-10-dev
liblld-10-dev
```

The CMake build is configured with:

```sh
-DEVMTRANS_USE_SYSTEM_LLVM=ON
```

## Full Build

`Dockerfile.full` mirrors the README source build:

```sh
cmake -DBUILD_SHARED_LIBS=ON ..
cmake --build . --target evmtrans
```

Build:

```sh
docker build -f deployment/Dockerfile.full -t ethsema-full .
```

The image installs:

```text
/opt/ethsema/lib/libevmtrans.so
/opt/ethsema/include/
```

This image is only for producing the shared library. It is not a runtime image
for the geth + Hera testnet.

## Standalone Converter

`Dockerfile.evmtrans` builds only `standalone-evmtrans`, the minimal entrypoint
for converting EVM creation bytecode to eWASM.

Build:

```sh
docker build -f deployment/Dockerfile.evmtrans -t ethsema-evmtrans .
```

Convert the README-style hex input:

```sh
cat tmp.hex | xxd -r -ps > .tmp.bin
docker run --rm -v "$PWD:/work" -w /work ethsema-evmtrans .tmp.bin -o out.wasm
```

Pass converter flags after the input path:

```sh
docker run --rm -v "$PWD:/work" -w /work ethsema-evmtrans .tmp.bin -o out.wasm --check-reentrancy
docker run --rm -v "$PWD:/work" -w /work ethsema-evmtrans .tmp.bin --dump
```

Runtime-only output can be generated with:

```sh
docker run --rm -v "$PWD:/work" -w /work ethsema-evmtrans .tmp.bin --dump --onlyRt
```

That writes `runtime.wasm` and `rt.ll` in the mounted working directory.

## Optional Compose Build

If Docker Compose is available:

```sh
docker compose -f deployment/compose.yml --profile full build full
docker compose -f deployment/compose.yml --profile evmtrans build evmtrans
```

Compose is only a convenience wrapper around the Dockerfiles.

## Parallelism

Limit build parallelism when memory is tight:

```sh
docker build --build-arg BUILD_PARALLEL_JOBS=2 -f deployment/Dockerfile.full -t ethsema-full .
docker build --build-arg BUILD_PARALLEL_JOBS=2 -f deployment/Dockerfile.evmtrans -t ethsema-evmtrans .
```
