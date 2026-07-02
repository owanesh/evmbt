# Hera local distribution patch

This patch records the local Hera changes found in this distribution of EVMBT.
They are present in the distributed source tree, but they are not committed by
the Hera authors at the submodule commit pinned by this repository:

```text
ethsema/hera -> 1c28c66ac0e2466fe7b4c29571d8c8678f30885d
```

The upstream Hera repository checked for this note is:

```text
https://github.com/Kenun99/hera.git
```

At the time this patch was recorded, the same changes were not present in the
upstream commits available after `1c28c66` on `origin/main` or `origin/master`.

## Contents

- `hera-local-distribution.patch`: patch against the Hera submodule working tree.

## Apply

From the repository root:

```sh
git -C ethsema/hera apply --check ../../patches/hera-local-distribution/hera-local-distribution.patch
git -C ethsema/hera apply ../../patches/hera-local-distribution/hera-local-distribution.patch
```

## Revert

From the repository root:

```sh
git -C ethsema/hera apply -R ../../patches/hera-local-distribution/hera-local-distribution.patch
```

