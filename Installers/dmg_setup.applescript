-- Lays out the Finder window CPack's DragNDrop generator shows when TeleportMacClientInstaller-*.dmg
-- is opened: window size/position, icon size, and where the app and the Applications symlink sit.
-- Run by CPack itself via osascript against a temporary, writable copy of the volume (see
-- CPACK_DMG_DS_STORE_SETUP_SCRIPT in ../CMake/TeleportPackaging.cmake) - CPack passes the mounted
-- volume's name as the first argument, hence "on run argv" rather than a hardcoded disk name.
-- Needs a logged-in GUI session to drive Finder (true both locally and on GitHub's macOS runners).
--
-- "TeleportPCClient.app" and "Applications" must match CPACK_PACKAGE_NAME's BUNDLE install name
-- and the symlink CPack's DragNDrop generator creates automatically, respectively.
on run argv
	set volumeName to item 1 of argv
	tell application "Finder"
		tell disk (volumeName as string)
			open
			set current view of container window to icon view
			set toolbar visible of container window to false
			set statusbar visible of container window to false
			-- {left, top, right, bottom}; keep it just wide enough for two 128px icons with room either side.
			set the bounds of container window to {200, 120, 760, 480}
			set viewOptions to the icon view options of container window
			set arrangement of viewOptions to not arranged
			set icon size of viewOptions to 128
			set position of item "TeleportPCClient.app" of container window to {150, 170}
			set position of item "Applications" of container window to {410, 170}
			close
			open
			update without registering applications
			delay 2
		end tell
	end tell
end run
