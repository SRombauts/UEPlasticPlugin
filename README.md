Unreal Engine 4 Plastic Source Control Plugin
---------------------------------------------

UE4PlasticPlugin is a simple Git Source Control Plugin for Unreal Engine

### Status

Alpha version :
- infrastuctre : connect (cm version & cm status, optionnal configuration of the cli executable path)
- show current branch name in status text
- display status icons to show controled/checked-out/added/deleted/private files
- add, delete a file
- revert modifications of a file

#### What *cannot* be done presently (TODO list for v1.0, ordered by priority):
- show changed/ignored files
- show hidden changes/cloaked files
- rename a file

- show history of a file
- visual diff of a blueprint against depot or between previous versions of a file
- checkin/commit a file (cannot handle atomically more than 20 files)
- migrate an asset between two projects if both are using Git
- solve a merge conflict on a blueprint
- Windows, Mac and Linux
- initialize a new workspace to manage your UE4 Game Project.
- should create an appropriate ignore file as part as initialization
- should also make the initial commit
- tags: implement ISourceControlLabel to manage git tags
- Branch is not in the current Editor workflow (but on Epic Roadmap)
- Pull/Fetch/Push are not in the current Editor workflow
- Amend a commit is not in the current Editor workflow
- configure user name & email ('git config user.name' & git config user.email')

#### Known issues:
- the Editor does not show deleted files (only when deleted externaly?)
- the Editor does not show missing files
- the Editor does not show .uproject file
- missing localisation for git specific messages
- displaying states of 'Engine' assets (also needs management of 'out of tree' files)
- a Move/Rename leaves a redirector file behind
- improve the 'Init' window text, hide it if connection is already done, auto connect
- reverting an asset does not seem to update content in Editor! Issue in Editor?
- renaming a Blueprint in Editor leaves a tracker file, AND modify too much the asset to enable git to track its history through renaming
- file history show Changelist as signed integer instead of hexadecimal SHA1
- standard Editor commit dialog ask if user wants to "Keep Files Checked Out" => no use for Git or Mercurial CanCheckOut()==false
