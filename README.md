<p align="center">
  <img width="200" src="https://avatars.githubusercontent.com/u/33623778?s=200&v=4" />
</p>

# Bazel Registry for Boost

The bazelboost project is an attempt to build boost modules as bazel modules. This is the [bazel registry](https://bazel.build/external/registry) for bazelboost.

## Usage

To use the bazelboost registry add it to your projects `.bazelrc` file.

```sh
# make sure bzlmod is enabled
common --enable_bzlmod

# add bazelboost registry
build --registry=https://raw.githubusercontent.com/bazelboost/registry/main
# by adding the bazelboost registry the BCR gets removed so you must add it here as well
build --registry=https://bcr.bazel.build
```

Now all the bazelboost modules may be added to your projects `MODULE.bazel` file.

```starlark
module(name = "my_project")

bazel_dep(name = "boost.asio")
```

All bazelboost modules have a target at the root with the same name as the module. For most cases you simple can use the module like so:

```starlark
cc_library(
    name = "my_library",
    # ...
    deps = [
        # ...
        "@boost.asio",
    ],
)
```

Some modules may have multiple targets. Review the `BUILD.bazel` file(s) or use [`bazel query`](https://bazel.build/query/guide) to find out whats available.

```sh
bazel query 'kind(cc_.*, @boost.test//...)'
```

```txt
@boost.test//:boost.test
@boost.test//:cpp_main
@boost.test//:test_main
@boost.test//:unit_test_main
```

If a feature is missing please file an issue in the bazelboost module.

## Bazel Boost Repository Structure

Each repository in the bazelboost org maintains a `bazelboost-*` branch for each version it supports. At this time we're limiting support to `1.83.0` until everything is relatively stable. No source file modifications are permitted, only bazel related files may be added.
