Unreal Engine 4 Plastic Source Control Plugin
---------------------------------------------

UE4PlasticPlugin is a simple Plastic Source Control Plugin for Unreal Engine 4

It is not intended to replace Plastic SCM GUI or command line.
It is a complementary tool improving efficiency in daily workflow.
It automates tracking of assets, bring common SCM tasks inside the Editor, and provide visual diffing of Blueprints.

![History Log window](Resources/UE4PlasticPlugin-History.png) 

### Quick setup

1. Your Unreal Engine 4.11.1 Game Project folder should be initialized
   into a workpace using the standard Plastic SCM GUI or cli.
2. Unzip the content of the plugin ZIP in the root of the Unreal Engine 4.11.1 project folder.
   That should create a "Plugins/" folder in it.
3. Launch Unreal Engine 4.11.1, click on the Source Control icon "Connect to Source", select "Plastic".

### Status

Alpha version 0.2.0 2016/04/23 :
- Windows only
- infrastructure : connect (cm version & cm status, optionnal configuration of the cli executable path)
- show current branch name in status text
- display status icons to show controled/checked-out/added/deleted/private/changed/ignored files
- display locked files
- add, duplicate, move/rename a file
- revert modifications of a file
- checkin a set of files with a multiline UTF-8 comment
- migrate an asset between two projects if both are using Plastic SCM
- delete file (but no way to checkin them, see known issues bellow)
- update workspace to latest head (Sync command)
- show history of a file
- visual diff of a blueprint against depot or between previous versions of a file

#### What *cannot* be done presently (TODO list for v1.0, ordered by priority):
- solve a merge conflict on a blueprint
- initialize a new workspace to manage your UE4 Game Project.
- create an appropriate ignore.conf file as part as initialization
- make the initial commit
- also permit late creation of the ignore.conf file
- tags: manage labels
- annotate: blame?
- Windows, Mac and Linux

#### Known issues:
- the Editor does not show deleted files => no way to checkin them!
- the Editor does not show missing files
- the Editor does not show .uproject file
- a Move/Rename leaves a redirector file behind:
  renaming a Blueprint in Editor leaves a tracker file, AND modify too much the asset to enable Plastic to track its history through renaming
- reverting a Blueprint asset does not update content in Editor!
- Branch is not in the current Editor workflow (but on Epic Roadmap)
- Push are not in the current Editor workflow
