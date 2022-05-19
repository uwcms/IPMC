
# This repository has been migrated to CERN Gitlab

This repository has been migrated to CERN Gitlab.

You may find it at https://gitlab.cern.ch/WisconsinCMS/IPMI/IPMC

To update your local copy of the repository to point to this project on CERN Gitlab, run the following commands from within your repository folder:

```sh
# Update git to use the new repository URL:
git remote set-url origin ssh://git@gitlab.cern.ch:7999/WisconsinCMS/IPMI/IPMC.git

# Pull any changes from the new repository (for up-to-date .gitmodules):
git pull

# Instruct your local copy to use the new .gitmodules paths:
git submodule sync
```

This repository includes submodules that may have been migrated. The submodule configuration in the new repository may be different. The last two commands above will attempt to correct this by pulling any changes from the new location, which may include changes to the `.gitmodules` file, and then instructing your local copy of the repository to use the new submodule paths.

Be aware that the `.gitmodules` file may not yet have been updated in all branches.  You may need to manually update the `.gitmodules` file in other branches and rerun `git submodule sync`.

If you have not already done so, you will need to set up your SSH key with CERN Gitlab (just as you did with Github) by visiting [this page](https://gitlab.cern.ch/-/profile/keys).

---

If this is a public repository and you do not wish to use SSH, you may instead run the following command to update your local copy:

```sh
git remote set-url origin https://gitlab.cern.ch/WisconsinCMS/IPMI/IPMC.git
```
