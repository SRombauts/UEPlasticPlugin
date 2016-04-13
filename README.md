Unreal Engine 4 Plastic Source Control Plugin
---------------------------------------------

UE4PlasticPlugin is a simple Plastic Source Control Plugin for Unreal Engine

### Status

Alpha version :
- Windows only
- infrastructure : connect (cm version & cm status, optionnal configuration of the cli executable path)
- show current branch name in status text
- display status icons to show controled/checked-out/added/deleted/private/changed/ignored files
- add, delete, duplicate, move/rename a file
- revert modifications of a file
- checkin/commit a file
- migrate an asset between two projects if both are using Plastic SCM

#### What *cannot* be done presently (TODO list for v1.0, ordered by priority):
- show history of a file
- visual diff of a blueprint against depot or between previous versions of a file
- solve a merge conflict on a blueprint
- Windows, Mac and Linux
- initialize a new workspace to manage your UE4 Game Project.
- should create an appropriate ignore.conf file as part as initialization
- should also make the initial commit
- should also permit late creation of the ignore.conf file
- tags: implement ISourceControlLabel to manage Plastic labels
- Branch is not in the current Editor workflow (but on Epic Roadmap)
- Pull/Fetch/Push are not in the current Editor workflow

#### Known issues:
- the Editor does not show deleted files
- the Editor does not show missing files
- the Editor does not show .uproject file
- displaying states of 'Engine' assets (also needs management of 'out of tree' files)
- a Move/Rename leaves a redirector file behind:
  renaming a Blueprint in Editor leaves a tracker file, AND modify too much the asset to enable Plastic to track its history through renaming
- improve the 'Init' window text, hide it if connection is already done, auto connect
- reverting an asset does not seem to update content in Editor!
- file history show Changelist as signed integer instead of hexadecimal SHA1
- standard Editor commit dialog ask if user wants to "Keep Files Checked Out" => no use for 
 or Mercurial CanCheckOut()==false
