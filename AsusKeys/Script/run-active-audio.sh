#!/bin/sh
clang++ -std=c++11 active-audio.cpp -framework IOKit -framework CoreFoundation -framework AudioToolbox -framework CoreAudio -o active-audio
./active-audio

# install
# 1. sudo cp ./active-audio /usr/local/bin/
# 2. sudo cp ./com.tommy.active-audio.plist /Library/LaunchDaemons/
# 3. sudo chmod +x /usr/local/bin/active-audio
# 4. sudo launchctl load /Library/LaunchDaemons/com.tommy.active-audio.plist
# 5. test: sudo launchctl start com.tommy.active-audio

# uninstall
# 1. sudo launchctl unload /Library/LaunchDaemons/com.tommy.active-audio.plist
# 2. sudo rm /Library/LaunchDaemons/com.tommy.active-audio.plist
# 3. rm /usr/local/bin/active-audio


# install
# 1. cp ./active-audio /usr/local/bin/
# 2. cp ./com.tommy.active-audio.plist ~/Library/LaunchAgents
# 3. chmod +x /usr/local/bin/active-audio
# 4. launchctl load ~/Library/LaunchAgents/com.tommy.active-audio.plist
# 5. test: launchctl start com.tommy.active-audio

# uninstall
# 1. launchctl unload ~/Library/LaunchAgents/com.tommy.active-audio.plist
# 2. rm ~/Library/LaunchAgents/com.tommy.active-audio.plist
# 3. rm /usr/local/bin/active-audio
