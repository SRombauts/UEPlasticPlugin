Unreal Engine 4 Plastic Source Control Plugin
---------------------------------------------

[![release](https://img.shields.io/github/release/SRombauts/UE4PlasticPlugin.svg)](https://github.com/SRombauts/UE4PlasticPlugin/releases)

UE4PlasticPlugin is a simple Plastic Source Control Plugin for Unreal Engine 4

It is not intended to replace Plastic SCM GUI or command line.
It is a complementary tool improving efficiency in daily workflow.
It automates tracking of assets, bring common SCM tasks inside the Editor, and provide visual diffing of Blueprints.

![History Log window](Resources/UE4PlasticPlugin-History.png) 

### References

- [Source Control user interface](https://docs.unrealengine.com/latest/INT/Engine/UI/SourceControl/)
- [Source Control Inside Unreal Editor](https://docs.unrealengine.com/latest/INT/Engine/Basics/SourceControl/InEditor/)

- [Diffing Unreal Assets](https://www.unrealengine.com/blog/diffing-unreal-assets)
- [Diffing Blueprints (Video)](https://www.unrealengine.com/blog/diffing-blueprints)

### Quick setup

1.a. Either: Unzip the content of the ZIP in the root of the Unreal Engine 4.11.2 project folder.
     That should create a "Plugins/" folder into your project.
     This is the way to go to use Platic SCM only on a specific projetc.
1.b. Else: Unzip the content of the ZIP in the Engine/ directory of UE4.11 directly for all your projects
     (typically "C:\Program Files\Epic Games\4.11\Engine\")
     That should create a "UE4PlasticPlugin" forlder into the "Plugins/" subidrectory.
     This is the way to enable Plastic SCM for all Unreal Engine projects.
2. Launch Unreal Engine 4.11.1, click on the Source Control icon "Connect to Source", select "Plastic SCM".

### Status

Beta version 0.7.0 2016/06/26 :
- Windows only
- infrastructure : connect (cm version & cm status, optionnal configuration of the cli executable path)
- show current branch name in status text
- display status icons to show controled/checked-out/added/deleted/private/changed/ignored files
- display locked files
- add, duplicate a file
- move/rename a file or a folder
- revert modifications of a file
- checkin a set of files with a multiline UTF-8 comment
- migrate (copy) an asset between two projects if both are using Plastic SCM
- delete file (but no way to checkin them, see known issues bellow)
- update workspace to latest head (Sync command)
- show history of a file
- visual diff of a blueprint against depot or between previous versions of a file
- initialize a new workspace to manage your UE4 Game Project.
- create an appropriate ignore.conf file as part as initialization
- make the initial commit
- also permit late creation of the ignore.conf file
- show conflicted files and 3-way visual diff
- solve a merge conflict on a blueprint

#### What *cannot* be done presently (TODO list for v1.0):
- Mac and Linux

#### Feature Requests (post v1.0)
- improve "status" efficiency by requesting status of the workspace root instead of file by file
- fire a global "status" command at startup (to populate the cache) to fix wrong context menu on content folders ("Mark for Add")
- add a top-menu "Sync/Update" instead of "Sync" on folder's context menu
- add a top-menu option to "undo all changes" in the project
- add a top-menu option to "undo unchanged only"
- add a "clean directory" or "checkin deleted files"
- add a setting to pass the --update option to "checkin"
- add a setting to tell UE if Plastic SCM is configured to use "read-only flags" like Perforce
- add support for partial checkin (like Gluon, for artists)
- add icon for Changed files
- add icon for Conflicted files
- add icon for Replaced/Merged files

### Abandonned as reserved for internal use by Epic Games with Perforce only
- tags: get labels (used for crash when the full Engine is under Plastic SCM)
- annotate: blame (used for crash when the full Engine is under Plastic SCM)

#### Bugs
- "Changed" assets popup a "Files need check-out!" (UnrealEdSrv.cpp) windows that does nothing when clicked!
- Source Control icon does not show correctly at next launch of the Editor after first connection, or when server is offline
- "NotCurrent" warning is not working because "DepotRevisionChangeset" is not correct in the "cm fileinfo" command
- Revert "Unchanged only" does nothing because Plastic SCM cli lacks a "chacked-out but unchanged" status.
- Merge conflict from cherry-pick or from range-merge cannot be solved by the plugin: use the Plastic SCM GUI

#### Known issues:
- Error messages with accents are not handled (for instance connection error in French)
- the Editor does not show deleted files: no way to check them in
- the Editor does not show missing files: no way to revert/restore them
- the Editor does not show .uproject file: no way to check in modification to the project file
- the Editor does not show folder status and is not able to manage them
- the Editor does not refresh status of assets in subdirectories at startup, so "Mark for add" is wrongly displayed in context menu for thoses subdirectories
- the Editor does not handle visual diff for renamed/moved assets
- reverting a Blueprint asset does not update content in Editor (and popup saying "is in use")!
- Branch is not in the current Editor workflow
- Merge is not in the current Editor workflow
- History does not show which revision is the current/checkout one
- Merge: "Accept Target" crash the UE4.11 Editor (same with Git Plugin)
