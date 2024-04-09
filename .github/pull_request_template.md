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

Once (1) your PR is approved by a CODEOWNER and (2) all your CI tests
pass, you can initiate a merge by entering a comment on your PR with the
text

    evergreen merge

This kicks your PR into the [evergreen commit queue]. Evergreen will
automatically squash-merge your commit after checking that you have PR
approval and that the CI tests all pass.

[evergreen commit queue]: https://github.com/evergreen-ci/evergreen/wiki/Commit-Queue

We currently have a manual linting stage required if you're
adding/modifying a workload YAML document. Evergreen will verify that
this has been run.

```bash
./run-genny generate-docs
```
--->
