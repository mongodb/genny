# Sharding Workload Combinator
This directory contains a script `generate.py` that will generate all possible combinations for a set of patch files to be applied to a base workload.

## Regenerating Workloads
To regenerate all workloads, use the following command:

```
python generate.py
```

Resulting workloads will be generated in `src/workloads/sharding` matching the directory structure of `src/workloads/sharding/templates` e.g. `src/workloads/sharding/templates/multi_updates` will generate files in `src/workloads/sharding/multi_updates`.

This should be done prior to committing whenever workloads are added or modified.

## Creating New Workloads
* Create a new subdirectory for your base workload and patch files.
* Create a single `SomeWorkload.yml.base` file in this subdirectory containing the base genny workload.
  * The base workload will be taken as an output workload as-is, so it should be a valid genny workload.
* Create any number of patch files (see below) named `SomePatch.patch` alongside the base workload.
  * Every possible combination of patches will be applied to the base workload during generation, so avoid adding too many patches or else a very large number of output workloads will be created.
  * Patches should not conflict with each other.
* Regenerate all workloads.

## Creating Patch Files
* Copy the base workload `SomeWorkload.yml.base` you'd like to patch to a temporary file `SomeWorkloadCopy.yml.base`.
* Make the desired changes to `SomeWorkloadCopy.yml.base`, e.g. changing a parameter.
* Use `diff` to generate a patch file as follows:
```
diff SomeWorkload.yml.base SomeWorkloadCopy.yml.base > SomePatch.patch
```
* Delete the temporary file `SomeWorkloadCopy.yml.base`.

## Modifying Existing Patch Files
* Apply the patch to the base workload using the `patch` command as follows:
```
patch SomeWorkload.yml.base SomePatch.patch -o SomeWorkloadCopy.yml.base
```
* Make desired changes to `SomeWorkloadCopy.yml.base`.
* Update the existing patch file using the `diff` command as follows:
```
diff SomeWorkload.yml.base SomeWorkloadCopy.yml.base > SomePatch.patch
```
* Delete the temporary file `SomeWorkloadCopy.yml.base`.

## Gotchas
### Modifying Base Workload
When modifying the base workload, it's possible that existing patch files will no longer apply. These conflicts may need to be resolve manually (e.g. by regenerating the patch files).
### Long Workload Names
Workloads are named based on the patch files that are enabled, and as such can become quite long. DSI will add a tag to AWS hosts containing (among other things) the name of the workload, and this tag has a limit of 256 characters. It's possible that this causes a system failure in evergreen with the error message `Tag value exceeds the maximum length of 256 characters`. Reducing the length of the base workload and/or patch filenames can resolve this issue.