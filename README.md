Unreal Engine 4 Plastic SCM Source Control Plugin
-------------------------------------------------

[![release](https://img.shields.io/github/release/SRombauts/UE4PlasticPlugin.svg)](https://github.com/SRombauts/UE4PlasticPlugin/releases)
[![Join the chat at https://gitter.im/SRombauts/UE4PlasticPlugin](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/UE4PlasticPlugin)

UE4PlasticPlugin is a simple [Plastic SCM](https://www.plasticscm.com/) Source Control Plugin for Unreal Engine 4 (UE 4.11 to 4.15)

It is not intended to replace [Plastic SCM GUI](https://www.plasticscm.com/documentation/gui/plastic-scm-version-control-gui-guide.shtml) or [command line interface "cm"](https://www.plasticscm.com/documentation/cli/plastic-scm-version-control-cli-guide.shtml).
It is a complementary tool improving efficiency in your daily workflow.

It automates tracking of assets, brings common SCM tasks inside the Editor, and provides visual diffing of Blueprints. It also helps importing an existing UE4 Project into source contorl, with appropriate *ignore.conf* file.

Source Control Login screen to create a new workspace/a new repository :
<img src="Screenshots/UE4PlasticPlugin-CreateWorkspace.png" width="720">

History menu entry to look a the changelog of an asset :
<img src="Screenshots/UE4PlasticPlugin-History.png" width="720">

Visual Diffing of different revision of a Blueprint :
<img src="https://cdn2.unrealengine.com/blog/DiffTool-1009x542-719850393.png" width="720">

### Copyright

Copyright (c) 2016-2017 Codice Software - Sébastien Rombauts (sebastien.rombauts@gmail.com)
<a href="https://www.paypal.me/SRombauts" title="Pay Me a Beer! Donate with PayPal :)"><img src="https://www.paypalobjects.com/webstatic/paypalme/images/pp_logo_small.png" width="118"></a>

### References

- [Source Control user interface](https://docs.unrealengine.com/latest/INT/Engine/UI/SourceControl/)
- [Source Control Inside Unreal Editor](https://docs.unrealengine.com/latest/INT/Engine/Basics/SourceControl/InEditor/)

- [Diffing Unreal Assets](https://www.unrealengine.com/blog/diffing-unreal-assets)
- [Diffing Blueprints (Video)](https://www.unrealengine.com/blog/diffing-blueprints)

### Quick setup
1. Download the [latest UE4PlasticPlugin-x.x.x.zip file](https://github.com/SRombauts/UE4PlasticPlugin/releases) targeting your UE4 version.
2. Either:
  1. Unzip the content of the ZIP in the root of the Unreal Engine 4.x project folder.
     That should create a "Plugins/" folder into your project.
     This is the way to go to use Platic SCM only on a specific projetc.
  2. Unzip the content of the ZIP in the Engine/ directory of UE4.x directly for all your projects
     (for instance "C:\Program Files\Epic Games\4.15\Engine\")
     That should create a "UE4PlasticPlugin" forlder into the "Plugins/" subidrectory.
     This is the way to enable Plastic SCM for all Unreal Engine projects.
3. Then, launch Unreal Engine 4.x, click on the Source Control icon "Connect to Source", select "Plastic SCM".

### Status

Beta version 0.9.9 2017/05/15 for UE4.15 :
- Windows only
- manage connection to the server
- show current branch name and CL in status text
- display status icons to show controled/checked-out/added/deleted/private/changed/ignored files
- display locked files, and by who
- add, duplicate a file
- move/rename a file or a folder
- revert modifications of a file (works best with the "Content Hot-Reload" option since UE4.15)
- checkin a set of files with a multiline UTF-8 comment
- migrate (copy) an asset between two projects if both are using Plastic SCM
- delete file (but no way to checkin them, see known issues bellow)
- update workspace to latest head (Sync command)
- show history of a file
- visual diff of a blueprint against depot or between previous versions of a file
- initialize a new workspace to manage your UE4 Game Project.
- make the initial commit with a custom message
- create an appropriate ignore.conf file as part of initialization
- also permit late creation of the ignore.conf file
- show conflicted files and 3-way visual diff
- solve a merge conflict on a blueprint
- top-menu global "Sync" instead of on folder's context menu
- top-menu global "undo unchanged" and "undo all checkout"
- [Partial Checkin (like Gluon, for artists)](http://blog.plasticscm.com/2015/03/plastic-gluon-is-out-version-control.html)
- Plastic Cloud is fully supported

#### Feature Requests (post v1.0)
- Mac OS X Support
- fire a global "status" command at startup (to populate the cache) to fix wrong context menu on content folders ("Mark for Add")
- add a setting to pass the --update option to "checkin"
- add a setting to tell UE if Plastic SCM is configured to use "read-only flags" like Perforce
- add a "clean directory" or "checkin deleted files"
- add dedicated icon for Changed files
- add dedicated icon for Conflicted files
- add dedicated icon for Replaced/Merged files

### Reserved for internal use by Epic Games with Perforce only
- tags: get labels (used for crash when the full Engine is under Plastic SCM)
- annotate: blame (used for crash when the full Engine is under Plastic SCM)

#### Bugs
- "Changed" assets popup a "Files need check-out!" (UnrealEdSrv.cpp) windows that does nothing when clicked!
- Revert "Unchanged only" does nothing because Plastic SCM cli lacks a "checked-out but unchanged" status.
- Merge conflict from cherry-pick or from range-merge cannot be solved by the plugin: use the Plastic SCM GUI

#### Known issues:
- Error messages with accents are not handled (for instance connection error in French)
- the Editor does not show deleted files: no way to check them in
- the Editor does not show missing files: no way to revert/restore them
- the Editor does not show folder status and is not able to manage them
- the Editor does not refresh status of assets in subdirectories at startup, so "Mark for add" is wrongly displayed in context menu for thoses subdirectories
* the Editor does not handle visual diff for renamed/moved assets
* History does not show which revision is the current/checkout one
- Branch and Merge are not in the current Editor workflow
- Merge Conflict: "Accept Target" crash the UE4.11 Editor (same with Git Plugin)

