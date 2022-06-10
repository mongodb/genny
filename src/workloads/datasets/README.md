Some Value Generators, like `ChooseFromDataset`, can read a dataset from the disk that store the values to choose from. Some example datasets are stored in [./src/workloads/datasets](../src/workloads/datasets). Four are included in the repo:

- **airport_codes.txt:** includes all airport codes
- **names.txt:** includes a list of the top 2000 names in USA. This is a dataset has been taken from [SecLists](https://github.com/danielmiessler/SecLists).
- **familynames.txt:** includes a list of the top 1000 family names in the USA. This is a dataset has been taken from [SecLists](https://github.com/danielmiessler/SecLists).
- **empty_test.txt:** this is an empty file to be used during unit testing.