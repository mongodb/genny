<!---
Thanks for submitting a PR to the Genny repo. Please include the
following fields (if relevant) prior to submitting your PR.
--->

**Jira Ticket:** [FOO-123](https://jira.mongodb.org/browse/FOO-123)

### Whats Changed

<!---
High level explanation of changes
--->

### Patch Testing Results

<!---
If applicable, link a patch test showing code changes running
successfully
--->

<!---
### Workload Submission form

If applicable, only required if there is a new workload being added. The
form is [here].

[here]: https://docs.google.com/forms/d/e/1FAIpQLSf1oh23Ddo1khjLZMqZ9DHbRdObnpeH20PSXTBuXVg-7mxxTA/viewform

### Merging and Linting

We currently have a manual linting stage required if you're
adding/modifying a workload YAML document. Evergreen will verify that
this has been run.

```bash
./run-genny generate-docs
```

Once (1) your PR is approved by a CODEOWNER and (2) all your CI tests
pass, you can initiate a merge by clicking "Add to merge queue".
This kicks your PR into the [Github Merge Queue]. Github will
automatically squash-merge your commit after checking that Evergreen's
CI tasks have returned a successful status.

[Github Merge Queue]: https://docs.devprod.prod.corp.mongodb.com/evergreen/Project-Configuration/Merge-Queue

--->
