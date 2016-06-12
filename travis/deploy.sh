#!/bin/bash

echo "script started"
openssl aes-256-cbc -K $encrypted_5d8775c46482_key -iv $encrypted_5d8775c46482_iv -in travis.enc -out /tmp/travis.key -d
chmod 600 /tmp/travis.key
mkdir /tmp/package
cp "/tmp/build/.pioenvs/nodemcu/firmware.bin" "/tmp/package/"
cp -r "examples/ESPmanager-example/data" "/tmp/package/"
ls /tmp/package/

# generate the manifest
python $TRAVIS_BUILD_DIR/travis/buildmanifest.py /tmp/package /tmp/package/manifest.json

#  no host checking
#ssh -v -p 4022 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -i /tmp/travis.key $HOME_USER@$HOME_IP "mkdir -p ~/projects/$TRAVIS_REPO_SLUG/$TRAVIS_BRANCH/latest/"
#scp -v -P 4022 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -i /tmp/travis.key -rp /tmp/package/. "$HOME_USER@$HOME_IP:~/projects/$TRAVIS_REPO_SLUG/$TRAVIS_BRANCH/latest/"  
echo "repo slug = $TRAVIS_REPO_SLUG"
 
ssh -v -p 4022  -i /tmp/travis.key $HOME_USER@$HOME_IP "mkdir -p ~/projects/$TRAVIS_REPO_SLUG/$TRAVIS_BRANCH/$1/"
scp -v -P 4022  -i /tmp/travis.key -rp /tmp/package/. "$HOME_USER@$HOME_IP:~/projects/$TRAVIS_REPO_SLUG/$TRAVIS_BRANCH/$1/"  
